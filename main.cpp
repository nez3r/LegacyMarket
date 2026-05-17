#include <curl/curl.h>  // Сначала CURL
#include <windows.h>    // Затем Windows
#include <commctrl.h>

#include <iostream>
#include <cstdio>
#include <vector>
#include <string>

// Элементы интерфейса (Перевели на HWND для широких символов)
HWND hListBox, hButtonInstall, hStatusText, hInfoBox;

// Структура для хранения данных о приложении (используем std::wstring для Unicode)
struct AppInfo {
    std::wstring id;
    std::wstring name;
    std::wstring version;
    std::wstring url;
    std::wstring os;
};

std::vector<AppInfo> appList;
const char* ini_filename = "apps.ini";

// Функция конвертации строки из UTF-8 в Unicode (wstring)
std::wstring Utf8ToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Функция конвертации wstring в ANSI (Windows-1251) для функции GetPrivateProfileString
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

// Функция загрузки базы данных приложений с GitHub
bool DownloadDatabase() {
    CURL* curl;
    CURLcode res;
    FILE* fp;
    const char* url = "https://raw.githubusercontent.com/nez3r/LegacyMarket/main/apps.ini";

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(ini_filename, "wb");
        if (!fp) return false;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);

        if (res == CURLE_OK) {
            // ФОКУС: Конвертируем скачанный файл из UTF-8 в Windows-1251
            fp = fopen(ini_filename, "rb");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                
                std::string utf8_content(size, 0);
                fread(&utf8_content[0], 1, size, fp);
                fclose(fp);

                // Переводим через наш конвертер
                std::wstring wcontent = Utf8ToWstring(utf8_content);
                std::string ansi_content = WstringToAnsi(wcontent);

                // Перезаписываем файл в кодировке Windows-1251
                fp = fopen(ini_filename, "wb");
                if (fp) {
                    fwrite(ansi_content.c_str(), 1, ansi_content.size(), fp);
                    fclose(fp);
                }
            }
            return true;
        }
    }
    return false;
}

// Функция парсинга скачанного INI-файла
void ParseDatabase() {
    appList.clear();
    char abs_path[MAX_PATH];
    GetFullPathNameA(ini_filename, MAX_PATH, abs_path, NULL);

    // Список твоих ID из репозитория
    std::vector<std::string> ids = {
        "7z920", "7z1604", "opera1218", "opera36", "firefox45", 
        "firefox52", "chrome49", "kmeleon75", "vlc208", "vlc228", 
        "winamp5666", "ccleaner564", "openoffice411_ru", "winscp556"
    };

    for (const auto& id : ids) {
        char name[256] = {0};
        char version[50] = {0};
        char url[512] = {0};
        char os[256] = {0};

        GetPrivateProfileStringA(id.c_str(), "name", "", name, sizeof(name), abs_path);
        
        if (strlen(name) > 0) {
            GetPrivateProfileStringA(id.c_str(), "version", "0.0", version, sizeof(version), abs_path);
            GetPrivateProfileStringA(id.c_str(), "url", "", url, sizeof(url), abs_path);
            GetPrivateProfileStringA(id.c_str(), "os", "Unknown", os, sizeof(os), abs_path);

            // Переводим строки из ANSI (1251) в широкие строки (wstring) для GUI
            AppInfo app;
            
            int nw = MultiByteToWideChar(1251, 0, id.c_str(), -1, NULL, 0);
            app.id.resize(nw - 1);
            MultiByteToWideChar(1251, 0, id.c_str(), -1, &app.id[0], nw);

            nw = MultiByteToWideChar(1251, 0, name, -1, NULL, 0);
            app.name.resize(nw - 1);
            MultiByteToWideChar(1251, 0, name, -1, &app.name[0], nw);

            nw = MultiByteToWideChar(1251, 0, version, -1, NULL, 0);
            app.version.resize(nw - 1);
            MultiByteToWideChar(1251, 0, version, -1, &app.version[0], nw);

            nw = MultiByteToWideChar(1251, 0, url, -1, NULL, 0);
            app.url.resize(nw - 1);
            MultiByteToWideChar(1251, 0, url, -1, &app.url[0], nw);

            nw = MultiByteToWideChar(1251, 0, os, -1, NULL, 0);
            app.os.resize(nw - 1);
            MultiByteToWideChar(1251, 0, os, -1, &app.os[0], nw);

            appList.push_back(app);
        }
    }
}

// Поток скачивания файлов
DWORD WINAPI DownloadAndInstallThread(LPVOID lpParam) {
    int index = (int)(INT_PTR)lpParam;
    if (index < 0 || (size_t)index >= appList.size()) return 0;

    AppInfo app = appList[index];
    
    EnableWindow(hButtonInstall, FALSE);
    SetWindowTextW(hStatusText, L"Статус: Скачивание файла...");

    CreateDirectoryA("storage", NULL);

    // Переводим URL в обычную строку для Curl
    std::string url_ansi = WstringToAnsi(app.url);

    std::wstring filename = app.url.substr(app.url.find_last_of(L"/") + 1);
    size_t pos;
    while ((pos = filename.find(L"%20")) != std::wstring::npos) {
        filename.replace(pos, 3, L" ");
    }
    
    std::wstring local_path = L"storage\\" + filename;

    CURL* curl = curl_easy_init();
    if (curl) {
        // Открываем файл, используя широкие символы (поддержка русских букв в путях)
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
            SetWindowTextW(hStatusText, L"Статус: Ошибка создания файла!");
        }
        curl_easy_cleanup(curl);
    }

    EnableWindow(hButtonInstall, TRUE);
    SetWindowTextW(hStatusText, L"Статус: Готово");
    return 0;
}

// Обработчик событий окна (Переведен на Unicode)
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            // Элементы управления теперь создаются через W-функции (Unicode)
            hListBox = CreateWindowW(L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
                                     10, 10, 250, 280, hwnd, (HMENU)1, NULL, NULL);

            hInfoBox = CreateWindowW(L"STATIC", L"Выберите программу из списка...", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                    270, 10, 250, 200, hwnd, NULL, NULL, NULL);

            hButtonInstall = CreateWindowW(L"BUTTON", L"Установить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                           270, 220, 250, 40, hwnd, (HMENU)2, NULL, NULL);
            EnableWindow(hButtonInstall, FALSE);

            hStatusText = CreateWindowW(L"STATIC", L"Статус: Обновление базы данных...", WS_CHILD | WS_VISIBLE,
                                        10, 300, 510, 20, hwnd, NULL, NULL, NULL);

            curl_global_init(CURL_GLOBAL_ALL);
            if (DownloadDatabase()) {
                ParseDatabase();
                for (const auto& app : appList) {
                    SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)app.name.c_str());
                }
                SetWindowTextW(hStatusText, L"Статус: Репозиторий успешно загружен.");
            } else {
                SetWindowTextW(hStatusText, L"Статус: Ошибка подключения!");
            }
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wp) == 1 && HIWORD(wp) == LBN_SELCHANGE) {
                int index = SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    AppInfo app = appList[index];
                    std::wstring info = L"Название: " + app.name + L"\n" +
                                        L"Версия: " + app.version + L"\n" +
                                        L"Поддержка ОС: " + app.os + L"\n\n" +
                                        L"Сохранение в storage\\";
                    SetWindowTextW(hInfoBox, info.c_str());
                    EnableWindow(hButtonInstall, TRUE);
                }
            }
            if (LOWORD(wp) == 2) {
                int index = SendMessageW(hListBox, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    CreateThread(NULL, 0, DownloadAndInstallThread, (LPVOID)(INT_PTR)index, 0, NULL);
                }
            }
            break;
        }
        case WM_DESTROY:
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
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = L"LegacyMarketClass";
    wc.lpfnWndProc = WndProc;

    if (!RegisterClassW(&wc)) return -1;

    HWND hwnd = CreateWindowW(L"LegacyMarketClass", L"Legacy Market Client v1.0",
                              WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 550, 370, NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, ncmdshow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}