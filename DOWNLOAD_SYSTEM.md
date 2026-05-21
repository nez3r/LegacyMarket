# Система загрузки файлов

## Описание

Проект использует разные методы загрузки файлов в зависимости от версии Windows:

### Windows XP / Server 2003
- Используется **curl.exe** (старая версия без зависимости от libcurl.dll)
- Не требует libcurl.dll, работает автономно
- Избегает проблемы с отсутствующей точкой входа GetTickCount64 в kernel32.dll

### Windows Vista и выше
- Используется **WinINet API** (встроенный в Windows)
- Надёжный и быстрый метод без внешних зависимостей
- Не требует curl.exe или libcurl.dll

## Структура кода

### download.h
Заголовочный файл с объявлениями функций:
- `IsWindowsXP()` - определяет версию Windows
- `DownloadFile()` - универсальная функция, автоматически выбирает метод
- `DownloadFileCurl()` - для Windows XP (старый curl.exe)
- `DownloadFileWinINet()` - для Windows Vista+ (WinINet API)

### download.cpp
Реализация функций загрузки

## Логика работы

```
DownloadFile()
    |
    ├─> IsWindowsXP() == true?
    |   └─> DownloadFileCurl() (старый curl.exe без libcurl.dll)
    |
    └─> IsWindowsXP() == false?
        └─> DownloadFileWinINet() (WinINet API)
```

## Требования

### Для Windows XP
- Файл `curl.exe` (старая версия 7.x без libcurl.dll) должен быть в папке с программой

### Для Windows Vista+
- Встроенный WinINet API (всегда доступен, не требует внешних файлов)

## Компиляция

```bash
c:/mingw32/bin/mingw32-make.exe clean
c:/mingw32/bin/mingw32-make.exe
```

Готовый файл: `out/market.exe`
