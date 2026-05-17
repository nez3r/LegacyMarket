#include <curl/curl.h>  // Сначала CURL
#include <windows.h>    // Затем Windows
#include <commctrl.h>   // Поддержка вкладок (Tab Control)

#include <iostream>
#include <cstdio>
#include <vector>
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
std::vector<std::wstring> categories = { L"Все", L"Браузеры", L"Мультимедиа", L"Утилиты", L"Офис" };

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

        if (res == CURLE_OK) {
            fp = fopen(ini_filename, "rb");
            if (fp) {
                fseek(fp, 0, SEEK_END);
                long size = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                std::string utf8_content(size, 0);
                fread(&utf8_content[0], 1, size, fp);
                fclose(fp);

                std::wstring wcontent = Utf8ToWstring(utf8_content);
                std::string ansi_content = WstringToAnsi(wcontent);

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

void ParseDatabase() {
    appList.clear();
    char abs_path[MAX_PATH];
    GetFullPathNameA(ini_filename, MAX_PATH, abs_path, NULL);

    // Буфер для названий всех секций (выделяем с запасом на будущее)
    char section_buffer[8192] = {0};
    GetPrivateProfileSectionNamesA(section_buffer, sizeof(section_buffer), abs_path);

    // Перебираем секции (они разделены символом \0, в конце списка \0\0)
    char* pSection = section_buffer;
    while (*pSection != '\0') {
        std::string id = pSection;

        char name[256] = {0};
        char version[50] = {0};
        char url[512] = {0};
        char os[256] = {0};
        char category[100] = {0};

        GetPrivateProfileStringA(id.c_str(), "name", "", name, sizeof(name), abs_path);
        
        if (strlen(name) > 0) {
            GetPrivateProfileStringA(id.c_str(), "version", "0.0", version, sizeof(version), abs_path);
            GetPrivateProfileStringA(id.c_str(), "url", "", url, sizeof(url), abs_path);
            GetPrivateProfileStringA(id.c_str(), "os", "Все", os, sizeof(os), abs_path);
            // Читаем категорию, если её нет в .ini — по умолчанию отправляем в "Утилиты"
            GetPrivateProfileStringA(id.c_str(), "category", "Утилиты", category, sizeof(category), abs_path);

            AppInfo app;
            
            // Для ID и URL используем чистый перевод (без привязки к локали), так как там всегда латиница
            app.id = std::wstring(id.begin(), id.end());
            app.url = std::wstring(url, url + strlen(url));

            // Для русского текста (name, os, category) используем конвертер из Windows-1251
            auto AnsiToWstr = [](const char* ansiStr) {
                int nw = MultiByteToWideChar(1251, 0, ansiStr, -1, NULL, 0);
                std::wstring ws(nw - 1, 0);
                MultiByteToWideChar(1251, 0, ansiStr, -1, &ws[0], nw);
                return ws;
            };

            app.name = AnsiToWstr(name);
            app.version = AnsiToWstr(version);
            app.os = AnsiToWstr(os);
            app.category = AnsiToWstr(category);

            appList.push_back(app);
        }
        pSection += id.length() + 1; // Переходим к следующей секции в буфере
    }
}

// Функция обновления списка в зависимости от выбранной вкладки
void UpdateListBox() {
    SendMessageW(hListBox, LB_RESETCONTENT, 0, 0);
    filteredIndices.clear();

    int selTab = TabCtrl_GetCurSel(hTab);
    std::wstring selCategory = categories[selTab];

    for (size_t i = 0; i < appList.size(); i++) {
        if (selCategory == L"Все" || appList[i].category == selCategory) {
            SendMessageW(hListBox, LB_ADDSTRING, 0, (LPARAM)appList[i].name.c_str());
            filteredIndices.push_back(i); // Запоминаем реальный индекс в общем векторе
        }
    }
    
    // Сбрасываем инфо-бокс и кнопку установки
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

            // Заполняем вкладки названиями категорий
            TCITEMW tie;
            tie.mask = TCIF_TEXT;
            for (size_t i = 0; i < categories.size(); i++) {
                tie.pszText = (LPWSTR)categories[i].c_str();
                TabCtrl_InsertItemW(hTab, i, &tie);
            }

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

            // Загрузка данных
            curl_global_init(CURL_GLOBAL_ALL);
            if (DownloadDatabase()) {
                ParseDatabase();
                UpdateListBox(); // Отобразит приложения для первой вкладки "Все"
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