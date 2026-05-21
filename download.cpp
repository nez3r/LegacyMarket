#include "download.h"
#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <iostream>
#include <vector>  // <-- ДОБАВЬТЕ ЭТУ СТРОКУ
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

// cURL реализация через curl.exe — БЕЗ отображения окна консоли
bool DownloadFileCurl(const std::string& url, const std::string& output_path, ProgressCallback callback, HWND hwnd) {
    // Формируем аргументы командной строки (без вызова командного интерпретатора напрямую)
    // -k (игнорировать сертификаты), -L (следовать редиректам), -s (silent режим, чтобы не слал мусор в никуда)
    std::string cmd = "curl.exe -k -L -s --max-time 300 -o \"" + output_path + "\" \"" + url + "\"";
    
    // CreateProcess требует модифицируемую строку, поэтому копируем её в буфер
    std::vector<char> cmd_buffer(cmd.begin(), cmd.end());
    cmd_buffer.push_back('\0');

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Настройка скрытого запуска:
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Явно просим скрыть окно, если оно создастся

    // Запускаем процесс curl.exe с важным флагом CREATE_NO_WINDOW
    if (CreateProcessA(
        NULL,               // Имя модуля (используем командную строку)
        &cmd_buffer[0],     // Командная строка
        NULL,               // Атрибуты безопасности процесса
        NULL,               // Атрибуты безопасности потока
        FALSE,              // Наследование дескрипторов
        CREATE_NO_WINDOW,   // Мheader ФЛАГ: запускает консольное приложение без создания окна!
        NULL,               // Окружение
        NULL,               // Текущий каталог
        &si,                // Предопределенные настройки стартового окна
        &pi                 // Информация о созданном процессе
    )) {
        // Ждем, пока curl закончит скачивание (максимум 5 минут, как в вашем таймауте)
        WaitForSingleObject(pi.hProcess, 300000);

        // Получаем код возврата процесса
        DWORD exit_code = 1;
        GetExitCodeProcess(pi.hProcess, &exit_code);

        // Закрываем дескрипторы процесса и потока
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // Проверяем, что файл создан и не пустой
        std::ifstream test(output_path.c_str(), std::ios::binary | std::ios::ate);
        if (test.good()) {
            std::streamsize size = test.tellg();
            test.close();

            // Вызываем callback с финальным размером для UI
            if (callback && hwnd && size > 0) {
                callback(size, size, hwnd);
            }

            return (exit_code == 0 && size > 0);
        }
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
