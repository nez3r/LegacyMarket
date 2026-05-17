#include <curl/curl.h>  // Сначала CURL
#include <windows.h>    // Затем Windows
#include <commctrl.h>   // Поддержка вкладок (Tab Control)

#include <iostream>
#include <cstdio>
#include <vector>
#include <fstream>
#include <string>

// Элементы управления
HWND hTab, hListBox, hButtonInstall, hStatusText, hInfoBox;
HFONT hCustomFont; // Хэндл для красивого шрифта

// Структура приложения
struct AppInfo {
    std::wstring id;
    std::wstring name;
    std::wstring version;
    std::wstring url;
    std::wstring os;
    std::wstring category; // Новое поле для вкладок
};

std::vector<AppInfo> appList;
std::vector<int> filteredIndices; // Индексы приложений, видимых в текущей вкладке
const char* ini_filename = "apps.ini";

// Список категорий (вкладок)
std::vector<std::wstring> categories;

// Конвертеры кодировок
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

// Загрузка базы данных
bool DownloadDatabase() {
    CURL* curl = curl_easy_init();
    if (curl) {
        FILE* fp = fopen(ini_filename, "wb");
        if (!fp) return false;

        curl_easy_setopt(curl, CURLOPT_URL, "https://raw.githubusercontent.com/nez3r/LegacyMarket/main/apps.ini");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);

        return (res == CURLE_OK);
    }
    return false;
}

void ParseDatabase() {
    appList.clear();
    categories.clear();
    
    std::ifstream file(ini_filename, std::ios::binary);
    if (!file.is_open()) return;

    std::string line;
    AppInfo currentApp;
    bool hasApp = false;
    bool inCategoriesSection = false;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == ';') continue;

        // Фиксируем секции
        if (line[0] == '[' && line.back() == ']') {
            std::string sectionName = line.substr(1, line.length() - 2);
            
            if (sectionName == "categories") {
                inCategoriesSection = true;
                hasApp = false;
                continue;
            }
            
            inCategoriesSection = false;
            if (hasApp) {
                appList.push_back(currentApp);
            }
            
            currentApp = AppInfo();
            currentApp.id = std::wstring(sectionName.begin(), sectionName.end());
            currentApp.category = L"Утилиты"; // Значение по умолчанию
            currentApp.os = L"Все";
            hasApp = true;
            continue;
        }

        // Парсим ключ=значение
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string val = line.substr(eq_pos + 1);
            std::wstring wval = Utf8ToWstring(val);

            if (inCategoriesSection) {
                if (key == "list") {
                    // Разбиваем строку категорий через запятую
                    std::wstringstream ss(wval);
                    std::wstring item;
                    while (std::getline(ss, item, L',')) {
                        if (!item.empty()) categories.push_back(item);
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
    if (hasApp) {
        appList.push_back(currentApp);
    }
    file.close();

    // Если секции в файле вдруг не оказалось, создаем дефолтную вкладку на английском
    if (categories.empty()) {
        categories.push_back(L"Apps");
    }
}

// Функция обновления списка в зависимости от выбранной вкладки
void UpdateListBox() {
    SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
    filteredIndices.clear();

    int selTab = TabCtrl_GetCurSel(hTab);
    if (selTab < 0 || (size_t)selTab >= categories.size()) return;

    for (size_t i = 0; i < appList.size(); i++) {
        // selTab == 0 означает, что выбрана самая первая вкладка ("Все")
        if (selTab == 0 || appList[i].category == categories[selTab]) {
            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)appList[i].name.c_str());
            filteredIndices.push_back(i);
        }
    }
    
    SetWindowTextW(hInfoBox, L"Выберите программу из списка...");
    EnableWindow(hButtonInstall, FALSE);
}

// Улучшенная функция: Качает во временную папку Temp и запускает
DWORD WINAPI DownloadAndInstallThread(LPVOID lpParam) {
    int index = (int)(INT_PTR)lpParam;
    if (index < 0 || (size_t)index >= appList.size()) return 0;

    AppInfo app = appList[index];
    
    EnableWindow(hButtonInstall, FALSE);
    SetWindowTextW(hStatusText, L"Статус: Скачивание файла во временную папку...");

    std::string url_ansi = WstringToAnsi(app.url);
    std::wstring filename = app.url.substr(app.url.find_last_of(L"/") + 1);
    size_t pos;
    while ((pos = filename.find(L"%20")) != std::wstring::npos) {
        filename.replace(pos, 3, L" ");
    }
    
    // Получаем путь к системной папке Temp
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
                SetWindowTextW(hStatusText, L"Статус: Запуск установки...");
                ShellExecuteW(NULL, L"open", local_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            } else {
                SetWindowTextW(hStatusText, L"Статус: Ошибка скачивания!");
            }
        } else {
            SetWindowTextW(hStatusText, L"Статус: Ошибка создания временного файла!");
        }
        curl_easy_cleanup(curl);
    }

    EnableWindow(hButtonInstall, TRUE);
    SetWindowTextW(hStatusText, L"Статус: Готово");
    return 0;
}

// Обработчик событий окна
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            // Инициализация общих элементов управления (нужно для вкладок)
            InitCommonControls();

            // Создаем красивый системный шрифт (Tahoma) вместо стандартного топорного
            hCustomFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                                      RUSSIAN_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");

            // 1. Создаем Вкладки (Tab Control)
            hTab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                 10, 10, 515, 330, hwnd, (HMENU)3, NULL, NULL);
            SendMessageW(hTab, WM_SETFONT, (WPARAM)hCustomFont, TRUE);


            // 2. Создаем ListBox внутри области вкладок
            hListBox = CreateWindowW(L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
                                     20, 45, 240, 280, hwnd, (HMENU)1, NULL, NULL);
            SendMessageW(hListBox, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            // 3. Создаем поле описания (InfoBox)
            hInfoBox = CreateWindowW(L"STATIC", L"Выберите программу из списка...", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                    275, 45, 240, 200, hwnd, NULL, NULL, NULL);
            SendMessageW(hInfoBox, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            // 4. Создаем кнопку Установить
            hButtonInstall = CreateWindowW(L"BUTTON", L"Установить программу", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                           275, 255, 240, 40, hwnd, (HMENU)2, NULL, NULL);
            SendMessageW(hButtonInstall, WM_SETFONT, (WPARAM)hCustomFont, TRUE);
            EnableWindow(hButtonInstall, FALSE);

            // 5. Строка статуса
            hStatusText = CreateWindowW(L"STATIC", L"Статус: Подключение к репозиторию...", WS_CHILD | WS_VISIBLE,
                                        10, 355, 515, 20, hwnd, NULL, NULL, NULL);
            SendMessageW(hStatusText, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            curl_global_init(CURL_GLOBAL_ALL);
            if (DownloadDatabase()) {
                ParseDatabase();
                
                // Динамически создаем вкладки на основе загруженного и декодированного UTF-8
                TabCtrl_DeleteAllItems(hTab);
                TCITEMW tie;
                tie.mask = TCIF_TEXT;
                for (size_t i = 0; i < categories.size(); i++) {
                    tie.pszText = (LPWSTR)categories[i].c_str();
                    TabCtrl_InsertItem(hTab, i, &tie);
                }

                UpdateListBox(); // Отобразит приложения для первой вкладки
                SetWindowTextW(hStatusText, L"Статус: Репозиторий успешно подключен.");
            } else {
                SetWindowTextW(hStatusText, L"Статус: Ошибка сети при чтении репозитория!");
            }
            break;
        }
        case WM_NOTIFY: {
            // Переключение вкладок
            LPNMHDR lpnmhdr = (LPNMHDR)lp;
            if (lpnmhdr->hwndFrom == hTab && lpnmhdr->code == TCN_SELCHANGE) {
                UpdateListBox(); // Перерисовываем список под выбранную категорию
            }
            break;
        }
        case WM_COMMAND: {
            // Клик по элементу списка
            if (LOWORD(wp) == 1 && HIWORD(wp) == LBN_SELCHANGE) {
                int index = SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    int realIndex = filteredIndices[index]; // Достаем реальный ID
                    AppInfo app = appList[realIndex];
                    
                    std::wstring info = L" Название: " + app.name + L"\n" +
                                        L" Версия: " + app.version + L"\n" +
                                        L" Платформа: " + app.os + L"\n" +
                                        L" Категория: " + app.category + L"\n\n" +
                                        L" Статус: Готов к установке.\n" +
                                        L" Файл будет очищен после запуска.";
                    SetWindowTextW(hInfoBox, info.c_str());
                    EnableWindow(hButtonInstall, TRUE);
                }
            }
            // Нажатие кнопки Установить
            if (LOWORD(wp) == 2) {
                int index = SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    int realIndex = filteredIndices[index];
                    CreateThread(NULL, 0, DownloadAndInstallThread, (LPVOID)(INT_PTR)realIndex, 0, NULL);
                }
            }
            break;
        }
        case WM_DESTROY:
            DeleteObject(hCustomFont); // Удаляем шрифт из памяти
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
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); // Цвет фона как у стандартных окон Windows
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = L"LegacyMarketTabsClass";
    wc.lpfnWndProc = WndProc;

    if (!RegisterClassW(&wc)) return -1;

    // Слегка увеличили высоту окна под новые элементы
    HWND hwnd = CreateWindowW(L"LegacyMarketTabsClass", L"Legacy Market Client v1.1",
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