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
bool DownloadFileWinINet(const std::string& url, const std::string& output_path) {
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

    // 3. Создание локального файла
    std::ofstream outFile(output_path.c_str(), std::ios::binary);
    if (!outFile.is_open()) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    // 4. Потоковое скачивание данных
    char buffer[8192];
    DWORD bytesRead = 0;
    bool success = true;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outFile.write(buffer, bytesRead);
        if (outFile.fail()) {
            success = false;
            break;
        }
    }

    // 5. Очистка ресурсов
    outFile.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return success;
}

// cURL реализация через curl.exe (для Windows XP используется старая версия)
bool DownloadFileCurl(const std::string& url, const std::string& output_path) {
    std::string cmd = "curl.exe -k -L --max-time 300 -o \"" + output_path + "\" \"" + url + "\" 2>nul";
    int result = system(cmd.c_str());

    // Проверяем, что файл создан и не пустой
    std::ifstream test(output_path.c_str(), std::ios::binary | std::ios::ate);
    if (test.good()) {
        std::streamsize size = test.tellg();
        test.close();
        return (result == 0 && size > 0);
    }

    return false;
}

// libcurl.dll реализация для Windows Vista+ (через системный curl)
bool DownloadFileLibCurl(const std::string& url, const std::string& output_path) {
    // На Vista+ используем системный curl с libcurl.dll
    std::string cmd = "curl.exe -k -L --max-time 300 -o \"" + output_path + "\" \"" + url + "\" 2>nul";
    int result = system(cmd.c_str());

    // Проверяем, что файл создан и не пустой
    std::ifstream test(output_path.c_str(), std::ios::binary | std::ios::ate);
    if (test.good()) {
        std::streamsize size = test.tellg();
        test.close();
        return (result == 0 && size > 0);
    }

    return false;
}

// Универсальная функция - автоматически выбирает метод в зависимости от версии Windows
bool DownloadFile(const std::string& url, const std::string& output_path) {
    if (IsWindowsXP()) {
        // Windows XP: используем curl.exe (старая версия без libcurl.dll)
        return DownloadFileCurl(url, output_path);
    } else {
        // Windows Vista+: используем libcurl.dll через системный curl
        // Сначала пробуем libcurl
        if (DownloadFileLibCurl(url, output_path)) {
            return true;
        }
        // Fallback на WinINet если libcurl не доступен
        return DownloadFileWinINet(url, output_path);
    }
}
