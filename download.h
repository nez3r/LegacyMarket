#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <string>

// Определяем версию Windows
bool IsWindowsXP();

// Универсальная функция загрузки (автоматически выбирает метод)
bool DownloadFile(const std::string& url, const std::string& output_path);

// WinINet реализация (для Windows Vista+)
bool DownloadFileWinINet(const std::string& url, const std::string& output_path);

// curl.exe реализация (для Windows XP - старая версия без libcurl.dll)
bool DownloadFileCurl(const std::string& url, const std::string& output_path);

#endif // DOWNLOAD_H
