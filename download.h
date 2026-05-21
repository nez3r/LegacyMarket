#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <string>
#include <windows.h>

// Callback функция для отслеживания прогресса загрузки
// Параметры: downloaded (байты), total (байты), hwnd (окно для обновления)
typedef void (*ProgressCallback)(size_t downloaded, size_t total, HWND hwnd);

// Определяем версию Windows
bool IsWindowsXP();

// Универсальная функция загрузки (автоматически выбирает метод)
bool DownloadFile(const std::string& url, const std::string& output_path, ProgressCallback callback = NULL, HWND hwnd = NULL);

// WinINet реализация (для Windows Vista+)
bool DownloadFileWinINet(const std::string& url, const std::string& output_path, ProgressCallback callback = NULL, HWND hwnd = NULL);

// curl.exe реализация (для Windows XP - старая версия без libcurl.dll)
bool DownloadFileCurl(const std::string& url, const std::string& output_path, ProgressCallback callback = NULL, HWND hwnd = NULL);

#endif // DOWNLOAD_H
