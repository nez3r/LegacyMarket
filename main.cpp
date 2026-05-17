#include <curl/curl.h>
#include <windows.h>
#include <commctrl.h>

#include <iostream>
#include <cstdio>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>

#define ID_MENU_CHANGE_REPO 5001

HWND hTab, hListBox, hButtonInstall, hButtonUpdate, hStatusText, hInfoBox;
HFONT hCustomFont;

struct AppInfo {
    std::wstring id;
    std::wstring name;
    std::wstring version;
    std::wstring url;
    std::wstring os;
    std::wstring category;
};

std::vector<AppInfo> appList;
std::vector<int> filteredIndices;
std::vector<std::wstring> tabStrings; // Для хранения строк вкладок
const char* ini_filename = "repolast.ini";
const char* config_filename = "config.cfg";
std::string repository_url = "https://raw.githubusercontent.com/nez3r/LegacyMarket/refs/heads/main/apps.ini";
const std::string official_repo = "https://raw.githubusercontent.com/nez3r/LegacyMarket/refs/heads/main/apps.ini";

// Оставляем вектор пустым. Он заполнится сам из apps.ini!
std::vector<std::wstring> categories; 

std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string WstringToAnsi(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(1251, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(1251, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

size_t write_callback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

bool DownloadDatabase() {
    CURL* curl = curl_easy_init();
    if (curl) {
        FILE* fp = fopen(ini_filename, "wb");
        if (!fp) return false;

        curl_easy_setopt(curl, CURLOPT_URL, repository_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Таймаут 10 секунд

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);

        return (res == CURLE_OK);
    }
    return false;
}

bool LoadLocalDatabase() {
    std::ifstream test(ini_filename);
    return test.good();
}

void SaveRepositoryUrl() {
    std::ofstream file(config_filename);
    if (file.is_open()) {
        file << "[Repository]\n";
        file << "url=" << repository_url << "\n";
        file.close();
    }
}

void LoadRepositoryUrl() {
    std::ifstream file(config_filename);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("url=") == 0) {
                repository_url = line.substr(4);
                break;
            }
        }
        file.close();
    } else {
        // Если конфига нет, создаём с дефолтным значением
        SaveRepositoryUrl();
    }
}

bool IsOfficialRepository() {
    return repository_url == official_repo;
}

void ParseDatabase() {
    appList.clear();
    categories.clear();

    // Открываем файл в бинарном режиме для правильного чтения UTF-8
    std::ifstream file(ini_filename, std::ios::binary);
    if (!file.is_open()) return;

    // Проверяем и пропускаем UTF-8 BOM если есть
    char bom[3] = {0};
    file.read(bom, 3);
    if (!(bom[0] == (char)0xEF && bom[1] == (char)0xBB && bom[2] == (char)0xBF)) {
        // BOM нет, возвращаемся в начало файла
        file.seekg(0);
    }

    std::string line;
    AppInfo currentApp;
    bool hasApp = false;
    bool inCategoriesSection = false;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == ';') continue;

        if (line[0] == '[' && line.back() == ']') {
            std::string sectionName = line.substr(1, line.length() - 2);
            
            if (sectionName == "categories") {
                inCategoriesSection = true;
                hasApp = false;
                continue;
            }
            
            inCategoriesSection = false;
            if (hasApp) appList.push_back(currentApp);
            
            currentApp = AppInfo();
            currentApp.id = std::wstring(sectionName.begin(), sectionName.end());
            currentApp.category = Utf8ToWstring("Утилиты"); 
            currentApp.os = Utf8ToWstring("Все");
            hasApp = true;
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string val = line.substr(eq_pos + 1);
            std::wstring wval = Utf8ToWstring(val);

            if (inCategoriesSection) {
                if (key == "list") {
                    std::wstringstream ss(wval);
                    std::wstring item;
                    while (std::getline(ss, item, L',')) {
                        // Удаляем пробелы в начале и конце
                        size_t start = item.find_first_not_of(L" \t\r\n");
                        size_t end = item.find_last_not_of(L" \t\r\n");
                        if (start != std::wstring::npos && end != std::wstring::npos) {
                            item = item.substr(start, end - start + 1);
                            categories.push_back(item);
                        }
                    }
                }
            } else if (hasApp) {
                if (key == "name") currentApp.name = wval;
                else if (key == "version") currentApp.version = wval;
                else if (key == "url") currentApp.url = wval;
                else if (key == "os") currentApp.os = wval;
                else if (key == "category") currentApp.category = wval;
            }
        }
    }
    if (hasApp) appList.push_back(currentApp);
    file.close();

    if (categories.empty()) categories.push_back(L"Apps");
}

void UpdateListBox() {
    SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
    filteredIndices.clear();

    int selTab = TabCtrl_GetCurSel(hTab);
    if (selTab < 0 || (size_t)selTab >= categories.size()) return;

    for (size_t i = 0; i < appList.size(); i++) {
        // selTab == 0 отвечает за первую вкладку "Все"
        if (selTab == 0 || appList[i].category == categories[selTab]) {
            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)appList[i].name.c_str());
            filteredIndices.push_back(i);
        }
    }
    
    SetWindowTextW(hInfoBox, Utf8ToWstring("Выберите программу из списка...").c_str());
    EnableWindow(hButtonInstall, FALSE);
}

DWORD WINAPI DownloadAndInstallThread(LPVOID lpParam) {
    int index = (int)(INT_PTR)lpParam;
    if (index < 0 || (size_t)index >= appList.size()) return 0;

    AppInfo app = appList[index];
    
    EnableWindow(hButtonInstall, FALSE);
    SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Скачивание файла...").c_str());

    std::string url_ansi = WstringToAnsi(app.url);
    std::wstring filename = app.url.substr(app.url.find_last_of(L"/") + 1);
    size_t pos;
    while ((pos = filename.find(L"%20")) != std::wstring::npos) {
        filename.replace(pos, 3, L" ");
    }
    
    wchar_t temp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, temp_path);
    std::wstring local_path = std::wstring(temp_path) + filename;

    CURL* curl = curl_easy_init();
    if (curl) {
        FILE* fp = _wfopen(local_path.c_str(), L"wb");
        if (fp) {
            curl_easy_setopt(curl, CURLOPT_URL, url_ansi.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

            CURLcode res = curl_easy_perform(curl);
            fclose(fp);

            if (res == CURLE_OK) {
                SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Запуск установки...").c_str());
                ShellExecuteW(NULL, L"open", local_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            } else {
                SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Ошибка скачивания!").c_str());
            }
        } else {
            SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Ошибка сохранения!").c_str());
        }
        curl_easy_cleanup(curl);
    }

    EnableWindow(hButtonInstall, TRUE);
    SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Готово").c_str());
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            InitCommonControls();

            // Создаём меню
            HMENU hMenuBar = CreateMenu();
            HMENU hFileMenu = CreateMenu();

            AppendMenuW(hFileMenu, MF_STRING, ID_MENU_CHANGE_REPO, Utf8ToWstring("Сменить репозиторий").c_str());
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, Utf8ToWstring("Настройки").c_str());

            SetMenu(hwnd, hMenuBar);

            hCustomFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                 10, 10, 515, 330, hwnd, (HMENU)3, NULL, NULL);
            SendMessageW(hTab, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            hListBox = CreateWindowW(L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
                                     20, 45, 240, 280, hwnd, (HMENU)1, NULL, NULL);
            SendMessageW(hListBox, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            hInfoBox = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                    275, 45, 240, 200, hwnd, NULL, NULL, NULL);
            SendMessageW(hInfoBox, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            hButtonInstall = CreateWindowW(L"BUTTON", Utf8ToWstring("Установить программу").c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                           275, 255, 240, 40, hwnd, (HMENU)2, NULL, NULL);
            SendMessageW(hButtonInstall, WM_SETFONT, (WPARAM)hCustomFont, TRUE);
            EnableWindow(hButtonInstall, FALSE);

            hButtonUpdate = CreateWindowW(L"BUTTON", Utf8ToWstring("Обновить базу").c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                          275, 300, 240, 30, hwnd, (HMENU)4, NULL, NULL);
            SendMessageW(hButtonUpdate, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            hStatusText = CreateWindowW(L"STATIC", Utf8ToWstring("Статус: Подключение к репозиторию...").c_str(), WS_CHILD | WS_VISIBLE,
                                        10, 355, 515, 20, hwnd, NULL, NULL, NULL);
            SendMessageW(hStatusText, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            // Загружаем URL репозитория из конфига
            LoadRepositoryUrl();

            // Динамическая загрузка данных
            curl_global_init(CURL_GLOBAL_ALL);
            bool downloaded = DownloadDatabase();

            if (downloaded) {
                ParseDatabase();

                // ВАЖНО: Вкладки создаются только ЗДЕСЬ, после парсинга файла!
                TabCtrl_DeleteAllItems(hTab);
                tabStrings.clear();

                TCITEMW tie;
                tie.mask = TCIF_TEXT;

                // Сохраняем строки для вкладок и используем SendMessageW
                for (size_t i = 0; i < categories.size(); i++) {
                    tabStrings.push_back(categories[i]);
                    tie.pszText = (LPWSTR)tabStrings[i].c_str();
                    SendMessageW(hTab, TCM_INSERTITEMW, i, (LPARAM)&tie);
                }

                UpdateListBox();
                std::wstring status_msg = Utf8ToWstring("Статус: База успешно загружена.");
                if (IsOfficialRepository()) {
                    status_msg += Utf8ToWstring(" [Официальный репозиторий]");
                }
                SetWindowTextW(hStatusText, status_msg.c_str());
            } else if (LoadLocalDatabase()) {
                // Если не удалось скачать, используем локальную версию
                ParseDatabase();

                TabCtrl_DeleteAllItems(hTab);
                tabStrings.clear();

                TCITEMW tie;
                tie.mask = TCIF_TEXT;

                for (size_t i = 0; i < categories.size(); i++) {
                    tabStrings.push_back(categories[i]);
                    tie.pszText = (LPWSTR)tabStrings[i].c_str();
                    SendMessageW(hTab, TCM_INSERTITEMW, i, (LPARAM)&tie);
                }

                UpdateListBox();
                std::wstring status_msg = Utf8ToWstring("Статус: Используется локальная база данных.");
                if (IsOfficialRepository()) {
                    status_msg += Utf8ToWstring(" [Официальный репозиторий]");
                }
                SetWindowTextW(hStatusText, status_msg.c_str());
            } else {
                SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Ошибка сети! Локальная база не найдена.").c_str());
            }
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR lpnmhdr = (LPNMHDR)lp;
            if (lpnmhdr->hwndFrom == hTab && lpnmhdr->code == TCN_SELCHANGE) {
                UpdateListBox(); 
            }
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wp) == ID_MENU_CHANGE_REPO) {
                // Диалог для ввода нового URL репозитория
                wchar_t buffer[512] = {0};
                std::wstring current_url = Utf8ToWstring(repository_url);
                wcscpy_s(buffer, 512, current_url.c_str());

                // Создаём диалоговое окно
                HWND hDialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", Utf8ToWstring("Сменить репозиторий").c_str(),
                    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                    (GetSystemMetrics(SM_CXSCREEN) - 500) / 2,
                    (GetSystemMetrics(SM_CYSCREEN) - 150) / 2,
                    500, 150, hwnd, NULL, NULL, NULL);

                CreateWindowW(L"STATIC", Utf8ToWstring("Введите URL репозитория:").c_str(),
                    WS_CHILD | WS_VISIBLE,
                    10, 10, 480, 20, hDialog, NULL, NULL, NULL);

                HWND hEdit = CreateWindowW(L"EDIT", buffer,
                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                    10, 35, 480, 25, hDialog, (HMENU)5002, NULL, NULL);
                SendMessageW(hEdit, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

                HWND hBtnOk = CreateWindowW(L"BUTTON", L"OK",
                    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                    300, 75, 90, 30, hDialog, (HMENU)IDOK, NULL, NULL);
                SendMessageW(hBtnOk, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

                HWND hBtnCancel = CreateWindowW(L"BUTTON", Utf8ToWstring("Отмена").c_str(),
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    400, 75, 90, 30, hDialog, (HMENU)IDCANCEL, NULL, NULL);
                SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

                // Простой цикл сообщений для диалога
                MSG msg;
                bool dialogActive = true;
                while (dialogActive && GetMessageW(&msg, NULL, 0, 0)) {
                    if (msg.message == WM_COMMAND) {
                        if (LOWORD(msg.wParam) == IDOK) {
                            GetWindowTextW(hEdit, buffer, 512);
                            std::wstring new_url_w(buffer);

                            // Преобразуем в UTF-8
                            int size_needed = WideCharToMultiByte(CP_UTF8, 0, &new_url_w[0], (int)new_url_w.size(), NULL, 0, NULL, NULL);
                            repository_url.resize(size_needed);
                            WideCharToMultiByte(CP_UTF8, 0, &new_url_w[0], (int)new_url_w.size(), &repository_url[0], size_needed, NULL, NULL);

                            SaveRepositoryUrl();
                            MessageBoxW(hwnd, Utf8ToWstring("Репозиторий изменён. Нажмите 'Обновить базу' для загрузки.").c_str(),
                                       Utf8ToWstring("Успех").c_str(), MB_OK | MB_ICONINFORMATION);
                            dialogActive = false;
                        } else if (LOWORD(msg.wParam) == IDCANCEL) {
                            dialogActive = false;
                        }
                    }
                    if (msg.message == WM_CLOSE && msg.hwnd == hDialog) {
                        dialogActive = false;
                    }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }

                DestroyWindow(hDialog);
                break;
            }
            if (LOWORD(wp) == 1 && HIWORD(wp) == LBN_SELCHANGE) {
                int index = SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    int realIndex = filteredIndices[index];
                    AppInfo app = appList[realIndex];

                    std::wstring info = Utf8ToWstring(" Название: ") + app.name + L"\n" +
                                        Utf8ToWstring(" Версия: ") + app.version + L"\n" +
                                        Utf8ToWstring(" Платформа: ") + app.os + L"\n" +
                                        Utf8ToWstring(" Категория: ") + app.category + L"\n\n" +
                                        Utf8ToWstring(" Статус: Готов к установке.\n Файл будет очищен после запуска.");
                    SetWindowTextW(hInfoBox, info.c_str());
                    EnableWindow(hButtonInstall, TRUE);
                }
            }
            if (LOWORD(wp) == 2) {
                int index = SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    int realIndex = filteredIndices[index];
                    CreateThread(NULL, 0, DownloadAndInstallThread, (LPVOID)(INT_PTR)realIndex, 0, NULL);
                }
            }
            if (LOWORD(wp) == 4) {
                // Кнопка "Обновить базу"
                SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Обновление базы данных...").c_str());
                EnableWindow(hButtonUpdate, FALSE);

                if (DownloadDatabase()) {
                    ParseDatabase();

                    TabCtrl_DeleteAllItems(hTab);
                    tabStrings.clear();

                    TCITEMW tie;
                    tie.mask = TCIF_TEXT;

                    for (size_t i = 0; i < categories.size(); i++) {
                        tabStrings.push_back(categories[i]);
                        tie.pszText = (LPWSTR)tabStrings[i].c_str();
                        SendMessageW(hTab, TCM_INSERTITEMW, i, (LPARAM)&tie);
                    }

                    UpdateListBox();
                    std::wstring status_msg = Utf8ToWstring("Статус: База успешно обновлена.");
                    if (IsOfficialRepository()) {
                        status_msg += Utf8ToWstring(" [Официальный репозиторий]");
                    }
                    SetWindowTextW(hStatusText, status_msg.c_str());
                } else {
                    SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Ошибка обновления!").c_str());
                }

                EnableWindow(hButtonUpdate, TRUE);
            }
            break;
        }
        case WM_DESTROY:
            DeleteObject(hCustomFont); 
            curl_global_cleanup();
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR args, int ncmdshow) {
    WNDCLASSW wc = {0};
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); 
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = L"LegacyMarketTabsClass";
    wc.lpfnWndProc = WndProc;

    if (!RegisterClassW(&wc)) return -1;

    HWND hwnd = CreateWindowW(L"LegacyMarketTabsClass", L"Legacy Market Client",
                              WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 550, 420, NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, ncmdshow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}