#include "download.h"
#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <iostream>

// Проверка версии Windows
bool IsWindowsXP() {
    OSVERSIONINFOA osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

    if (GetVersionExA(&osvi)) {
        // Windows XP = 5.1, Windows Server 2003 = 5.2
        // Возвращаем true только для Windows 5.x (XP/2003)
        return (osvi.dwMajorVersion == 5);
    }
    return false;
}

// WinINet реализация для Windows XP
bool DownloadFileWinINet(const std::string& url, const std::string& output_path, ProgressCallback callback, HWND hwnd) {
    // 1. Инициализация сессии
    HINTERNET hInternet = InternetOpenA(
        "LegacyMarket/1.0",
        INTERNET_OPEN_TYPE_DIRECT,
        NULL, NULL, 0
    );

    if (!hInternet) {
        return false;
    }

    // 2. Открытие URL (с поддержкой HTTPS для XP)
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;

    // Для HTTPS добавляем флаги игнорирования сертификатов (как -k в curl)
    if (url.find("https://") == 0) {
        flags |= INTERNET_FLAG_SECURE |
                 INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
                 INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);

    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    // Получаем размер файла
    DWORD totalSize = 0;
    DWORD bufferSize = sizeof(totalSize);
    DWORD index = 0;
    HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &totalSize, &bufferSize, &index);

    // 3. Создание локального файла
    std::ofstream outFile(output_path.c_str(), std::ios::binary);
    if (!outFile.is_open()) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    // 4. Потоковое скачивание данных с отслеживанием прогресса
    char buffer[8192];
    DWORD bytesRead = 0;
    size_t totalDownloaded = 0;
    bool success = true;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
        if (outFile.fail()) {
            success = false;
            break;
        }

        totalDownloaded += bytesRead;

        // Вызываем callback для обновления прогресса
        if (callback && hwnd) {
            callback(totalDownloaded, totalSize, hwnd);
        }
    }

    // 5. Очистка ресурсов
    outFile.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return success;
}

// cURL реализация через curl.exe (для Windows XP используется старая версия)
bool DownloadFileCurl(const std::string& url, const std::string& output_path, ProgressCallback callback, HWND hwnd) {
    // Примечание: curl.exe не поддерживает callback прогресса напрямую
    // Для XP просто выполняем загрузку без детального прогресса
    std::string cmd = "curl.exe -k -L --max-time 300 -o \"" + output_path + "\" \"" + url + "\" 2>nul";
    int result = system(cmd.c_str());

    // Проверяем, что файл создан и не пустой
    std::ifstream test(output_path.c_str(), std::ios::binary | std::ios::ate);
    if (test.good()) {
        std::streamsize size = test.tellg();
        test.close();

        // Вызываем callback с финальным размером
        if (callback && hwnd && size > 0) {
            callback(size, size, hwnd);
        }

        return (result == 0 && size > 0);
    }

    return false;
}

// Универсальная функция - автоматически выбирает метод в зависимости от версии Windows
bool DownloadFile(const std::string& url, const std::string& output_path, ProgressCallback callback, HWND hwnd) {
    if (IsWindowsXP()) {
        // Windows XP: используем curl.exe (старая версия без libcurl.dll)
        return DownloadFileCurl(url, output_path, callback, hwnd);
    } else {
        // Windows Vista+: используем WinINet API (встроенный, надежный)
        return DownloadFileWinINet(url, output_path, callback, hwnd);
    }
}
