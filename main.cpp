#include <winsock2.h>
#include <windows.h>
#include <commctrl.h>

#include <iostream>
#include <cstdio>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>

#include "download.h"

#define ID_MENU_CHANGE_REPO 5001
#define ID_EDIT_REPO 5002
#define ID_BTN_OK 5003
#define ID_BTN_CANCEL 5004

HWND hTab, hListBox, hButtonInstall, hButtonUpdate, hStatusText, hInfoBox, hProgressBar;
HFONT hCustomFont, hTitleFont;
HBRUSH hBackgroundBrush, hButtonBrush;
HWND hDialogRepo = NULL;
HWND hMainWindow = NULL;

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
long last_http_code = 0; // Для хранения последнего HTTP кода

// Оставляем вектор пустым. Он заполнится сам из apps.ini!
std::vector<std::wstring> categories;

// Флаги для отслеживания выполнения операций
bool isUpdatingDatabase = false;

// Функция для форматирования размера файла
std::wstring FormatSize(size_t bytes) {
    wchar_t buffer[64];
    if (bytes < 1024) {
        swprintf(buffer, 64, L"%zu Б", bytes);
    } else if (bytes < 1024 * 1024) {
        swprintf(buffer, 64, L"%.2f КБ", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        swprintf(buffer, 64, L"%.2f МБ", bytes / (1024.0 * 1024.0));
    } else {
        swprintf(buffer, 64, L"%.2f ГБ", bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return std::wstring(buffer);
}

// Callback функция для обновления прогресса загрузки
void UpdateDownloadProgress(size_t downloaded, size_t total, HWND hwnd) {
    if (!hwnd) return;

    // Отправляем сообщение в главный поток для обновления UI
    // wParam = downloaded, lParam = total
    PostMessageW(hwnd, WM_USER + 3, (WPARAM)downloaded, (LPARAM)total);
} 

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

bool DownloadDatabase() {
    // Показываем прогресс-бар
    if (hProgressBar) {
        ShowWindow(hProgressBar, SW_SHOW);
        SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);
    }

    // Используем универсальную функцию загрузки с callback
    bool result = DownloadFile(repository_url, ini_filename, UpdateDownloadProgress, hMainWindow);

    // Скрываем прогресс-бар
    if (hProgressBar) {
        ShowWindow(hProgressBar, SW_HIDE);
    }

    last_http_code = result ? 200 : 0;
    return result;
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

LRESULT CALLBACK RepoDialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HWND hEdit = NULL;

    switch (msg) {
        case WM_CREATE: {
            CreateWindowW(L"STATIC", Utf8ToWstring("Введите URL репозитория:").c_str(),
                WS_CHILD | WS_VISIBLE,
                10, 10, 480, 20, hwnd, NULL, NULL, NULL);

            std::wstring current_url = Utf8ToWstring(repository_url);
            hEdit = CreateWindowW(L"EDIT", current_url.c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                10, 35, 480, 25, hwnd, (HMENU)ID_EDIT_REPO, NULL, NULL);
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            HWND hBtnOk = CreateWindowW(L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                300, 75, 90, 30, hwnd, (HMENU)ID_BTN_OK, NULL, NULL);
            SendMessageW(hBtnOk, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            HWND hBtnCancel = CreateWindowW(L"BUTTON", Utf8ToWstring("Отмена").c_str(),
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                400, 75, 90, 30, hwnd, (HMENU)ID_BTN_CANCEL, NULL, NULL);
            SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hCustomFont, TRUE);
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wp) == ID_BTN_OK) {
                wchar_t buffer[512] = {0};
                GetWindowTextW(hEdit, buffer, 512);
                std::wstring new_url_w(buffer);

                // Преобразуем в UTF-8
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, &new_url_w[0], (int)new_url_w.size(), NULL, 0, NULL, NULL);
                repository_url.resize(size_needed);
                WideCharToMultiByte(CP_UTF8, 0, &new_url_w[0], (int)new_url_w.size(), &repository_url[0], size_needed, NULL, NULL);

                SaveRepositoryUrl();
                DestroyWindow(hwnd);

                // Автоматически обновляем базу данных
                if (hMainWindow) {
                    PostMessageW(hMainWindow, WM_COMMAND, MAKEWPARAM(4, 0), 0);
                }
            } else if (LOWORD(wp) == ID_BTN_CANCEL) {
                DestroyWindow(hwnd);
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            hDialogRepo = NULL;
            break;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
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

// Поток для начальной загрузки базы данных
DWORD WINAPI InitialDatabaseLoadThread(LPVOID lpParam) {
    HWND hwnd = (HWND)lpParam;

    bool downloaded = DownloadDatabase();

    if (downloaded) {
        ParseDatabase();

        // Обновляем UI в главном потоке через PostMessage
        PostMessageW(hwnd, WM_USER + 1, 1, 0); // 1 = успешная загрузка
    } else if (LoadLocalDatabase()) {
        ParseDatabase();
        PostMessageW(hwnd, WM_USER + 1, 2, 0); // 2 = локальная база
    } else {
        PostMessageW(hwnd, WM_USER + 1, 0, 0); // 0 = ошибка
    }

    return 0;
}

// Поток для обновления базы данных
DWORD WINAPI UpdateDatabaseThread(LPVOID lpParam) {
    HWND hwnd = (HWND)lpParam;

    if (DownloadDatabase()) {
        ParseDatabase();
        PostMessageW(hwnd, WM_USER + 2, 1, 0); // 1 = успешное обновление
    } else if (LoadLocalDatabase()) {
        ParseDatabase();
        PostMessageW(hwnd, WM_USER + 2, 2, 0); // 2 = используется локальная база
    } else {
        PostMessageW(hwnd, WM_USER + 2, 0, 0); // 0 = ошибка
    }

    isUpdatingDatabase = false; // Сбрасываем флаг
    return 0;
}

DWORD WINAPI DownloadAndInstallThread(LPVOID lpParam) {
    int index = (int)(INT_PTR)lpParam;
    if (index < 0 || (size_t)index >= appList.size()) return 0;

    AppInfo app = appList[index];

    EnableWindow(hButtonInstall, FALSE);
    SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Скачивание файла...").c_str());

    // Показываем прогресс-бар для скачивания
    if (hProgressBar) {
        ShowWindow(hProgressBar, SW_SHOW);
        SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);
    }

    std::string url_ansi = WstringToAnsi(app.url);
    std::wstring filename = app.url.substr(app.url.find_last_of(L"/") + 1);
    size_t pos;
    while ((pos = filename.find(L"%20")) != std::wstring::npos) {
        filename.replace(pos, 3, L" ");
    }

    wchar_t temp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, temp_path);
    std::wstring local_path = std::wstring(temp_path) + filename;

    // Конвертируем путь в ANSI для универсальной функции загрузки
    std::string local_path_ansi = WstringToAnsi(local_path);

    // Используем универсальную функцию загрузки с callback
    bool result = DownloadFile(url_ansi, local_path_ansi, UpdateDownloadProgress, hMainWindow);

    // Скрываем прогресс-бар
    if (hProgressBar) {
        ShowWindow(hProgressBar, SW_HIDE);
    }

    if (result) {
        SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Запуск установки...").c_str());

        // Запускаем установщик и получаем информацию о процессе
        SHELLEXECUTEINFOW sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = local_path.c_str();
        sei.nShow = SW_SHOWNORMAL;

        if (ShellExecuteExW(&sei)) {
            // Создаём окно "Установка запущена"
            HWND hInstallDialog = CreateWindowExW(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
                L"STATIC", Utf8ToWstring("Установка запущена").c_str(),
                WS_POPUP | WS_CAPTION | WS_VISIBLE,
                (GetSystemMetrics(SM_CXSCREEN) - 300) / 2,
                (GetSystemMetrics(SM_CYSCREEN) - 100) / 2,
                300, 100, hMainWindow, NULL, GetModuleHandle(NULL), NULL);

            HWND hLabel = CreateWindowW(L"STATIC", Utf8ToWstring("Пожалуйста, дождитесь завершения установки...").c_str(),
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                10, 30, 280, 40, hInstallDialog, NULL, NULL, NULL);
            SendMessageW(hLabel, WM_SETFONT, (WPARAM)hCustomFont, TRUE);

            // Ждём завершения процесса установки
            if (sei.hProcess) {
                WaitForSingleObject(sei.hProcess, INFINITE);
                CloseHandle(sei.hProcess);
            }

            // Закрываем окно "Установка запущена"
            DestroyWindow(hInstallDialog);
        }
    } else {
        SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Ошибка скачивания!").c_str());
    }

    EnableWindow(hButtonInstall, TRUE);
    SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Готово").c_str());
    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            hMainWindow = hwnd;
            InitCommonControls();

            // Создаём кисти для фона
            hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 245));
            hButtonBrush = CreateSolidBrush(RGB(0, 120, 215));

            // Создаём меню
            HMENU hMenuBar = CreateMenu();
            HMENU hFileMenu = CreateMenu();

            AppendMenuW(hFileMenu, MF_STRING, ID_MENU_CHANGE_REPO, Utf8ToWstring("Сменить репозиторий").c_str());
            AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, Utf8ToWstring("Настройки").c_str());

            SetMenu(hwnd, hMenuBar);

            // Создаём шрифты
            hCustomFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            hTitleFont = CreateFontW(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

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

            // Создаём прогресс-бар (скрытый по умолчанию)
            hProgressBar = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | PBS_SMOOTH,
                                         10, 380, 515, 20, hwnd, NULL, NULL, NULL);
            SendMessageW(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessageW(hProgressBar, PBM_SETPOS, 0, 0);

            // Загружаем URL репозитория из конфига
            LoadRepositoryUrl();

            // Запускаем загрузку базы данных в отдельном потоке
            SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Подключение к репозиторию...").c_str());
            CreateThread(NULL, 0, InitialDatabaseLoadThread, (LPVOID)hwnd, 0, NULL);
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
                if (hDialogRepo == NULL) {
                    // Регистрируем класс окна для диалога
                    WNDCLASSW wc = {0};
                    wc.lpfnWndProc = RepoDialogProc;
                    wc.hInstance = GetModuleHandle(NULL);
                    wc.lpszClassName = L"RepoDialogClass";
                    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
                    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
                    RegisterClassW(&wc);

                    // Создаём диалоговое окно
                    hDialogRepo = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RepoDialogClass",
                        Utf8ToWstring("Сменить репозиторий").c_str(),
                        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                        (GetSystemMetrics(SM_CXSCREEN) - 500) / 2,
                        (GetSystemMetrics(SM_CYSCREEN) - 150) / 2,
                        500, 150, hwnd, NULL, GetModuleHandle(NULL), NULL);
                }
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
                // Проверяем, не выполняется ли уже обновление
                if (isUpdatingDatabase) {
                    break; // Игнорируем повторные нажатия
                }

                isUpdatingDatabase = true;
                SetWindowTextW(hStatusText, Utf8ToWstring("Статус: Обновление базы данных...").c_str());
                EnableWindow(hButtonUpdate, FALSE);

                // Запускаем обновление в отдельном потоке
                CreateThread(NULL, 0, UpdateDatabaseThread, (LPVOID)hwnd, 0, NULL);
            }
            break;
        }
        case WM_USER + 1: {
            // Сообщение от InitialDatabaseLoadThread
            int result = (int)wp;

            if (result == 1 || result == 2) {
                // Успешная загрузка или локальная база
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

                std::wstring status_msg;
                if (result == 1) {
                    status_msg = Utf8ToWstring("Статус: База успешно загружена.");
                } else {
                    status_msg = Utf8ToWstring("Статус: Используется локальная база данных.");
                }

                if (IsOfficialRepository()) {
                    status_msg += Utf8ToWstring(" [Официальный репозиторий]");
                }
                SetWindowTextW(hStatusText, status_msg.c_str());
            } else {
                // Ошибка загрузки
                std::string error_msg = "Статус: Ошибка сети";
                if (last_http_code > 0) {
                    error_msg += ": HTTP " + std::to_string(last_http_code);
                } else {
                    error_msg += ": Ошибка подключения";
                }
                error_msg += ". Локальная база не найдена.";
                SetWindowTextW(hStatusText, Utf8ToWstring(error_msg).c_str());
            }
            break;
        }
        case WM_USER + 2: {
            // Сообщение от UpdateDatabaseThread
            int result = (int)wp;

            if (result == 1 || result == 2) {
                // Успешное обновление или локальная база
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

                // Пересоздаём информационное окно для сохранения стиля
                DestroyWindow(hInfoBox);
                hInfoBox = CreateWindowW(L"STATIC", Utf8ToWstring("Выберите программу из списка...").c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER,
                                        275, 45, 240, 200, hwnd, NULL, NULL, NULL);
                SendMessageW(hInfoBox, WM_SETFONT, (WPARAM)hCustomFont, TRUE);
                EnableWindow(hButtonInstall, FALSE);

                // Принудительно обновляем видимость кнопок
                ShowWindow(hButtonInstall, SW_SHOW);
                ShowWindow(hButtonUpdate, SW_SHOW);
                InvalidateRect(hwnd, NULL, TRUE);

                std::wstring status_msg;
                if (result == 1) {
                    status_msg = Utf8ToWstring("Статус: База успешно обновлена.");
                } else {
                    status_msg = Utf8ToWstring("Статус: Не удалось обновить. Используется локальная база.");
                }

                if (IsOfficialRepository()) {
                    status_msg += Utf8ToWstring(" [Официальный репозиторий]");
                }
                SetWindowTextW(hStatusText, status_msg.c_str());
            } else {
                // Ошибка обновления
                std::string error_msg = "Статус: Ошибка обновления базы";
                if (last_http_code > 0) {
                    error_msg += ": HTTP " + std::to_string(last_http_code);
                } else {
                    error_msg += ": Ошибка подключения";
                }
                error_msg += ". Локальная база не найдена.";
                SetWindowTextW(hStatusText, Utf8ToWstring(error_msg).c_str());
            }

            EnableWindow(hButtonUpdate, TRUE);
            break;
        }
        case WM_USER + 3: {
            // Сообщение для обновления прогресса загрузки
            size_t downloaded = (size_t)wp;
            size_t total = (size_t)lp;

            if (!hProgressBar || !hStatusText) break;

            // Вычисляем процент
            int percent = 0;
            if (total > 0) {
                percent = (int)((downloaded * 100) / total);
            }

            // Обновляем прогресс-бар
            SendMessageW(hProgressBar, PBM_SETPOS, percent, 0);

            // Обновляем текст статуса
            std::wstring downloadedStr = FormatSize(downloaded);
            std::wstring totalStr = FormatSize(total);

            wchar_t statusBuffer[256];
            if (total > 0) {
                swprintf(statusBuffer, 256, L"Статус: Загрузка... %d%% (%s / %s)",
                         percent, downloadedStr.c_str(), totalStr.c_str());
            } else {
                swprintf(statusBuffer, 256, L"Статус: Загрузка... (%s)", downloadedStr.c_str());
            }

            SetWindowTextW(hStatusText, statusBuffer);
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wp;
            SetTextColor(hdcStatic, RGB(50, 50, 50));
            SetBkColor(hdcStatic, RGB(240, 240, 245));
            return (INT_PTR)hBackgroundBrush;
        }
        case WM_DESTROY:
            DeleteObject(hCustomFont);
            DeleteObject(hTitleFont);
            DeleteObject(hBackgroundBrush);
            DeleteObject(hButtonBrush);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR args, int ncmdshow) {
    WNDCLASSW wc = {0};
    wc.hbrBackground = CreateSolidBrush(RGB(240, 240, 245));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = L"LegacyMarketTabsClass";
    wc.lpfnWndProc = WndProc;

    if (!RegisterClassW(&wc)) return -1;

    HWND hwnd = CreateWindowW(L"LegacyMarketTabsClass", L"Legacy Market Client",
                              WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 550, 450, NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, ncmdshow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}