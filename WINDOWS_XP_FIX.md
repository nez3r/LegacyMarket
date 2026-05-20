# Legacy Market - Windows XP Compatibility

## Решение проблемы GetTickCount64

Проблема с ошибкой "Точка входа GetTickCount64 не найдена в kernel32.dll" была решена путём **замены libcurl на curl.exe**.

## Что было сделано

### Попытка 1: API Hooking (не сработало)
- Создали `xp_compat.c` с реализацией GetTickCount64
- Пытались перехватить GetProcAddress
- Проблема: библиотеки уже скомпилированы с импортом GetTickCount64

### Попытка 2: Сборка cURL 7.53.1 (не удалось)
- Скачали исходники cURL 7.53.1
- Столкнулись с проблемами компиляции и зависимостями

### ✅ Решение: Использование curl.exe
Вместо линковки с libcurl, программа теперь вызывает `curl.exe` через `system()`.

## Преимущества решения

1. **Полная совместимость с Windows XP** - нет зависимости от GetTickCount64
2. **Меньший размер** - 2.5 MB вместо 7.9 MB
3. **Простая сборка** - не нужны библиотеки cURL
4. **Легко обновлять** - просто заменить curl.exe на новую версию

## Требования

Для работы программы необходим **curl.exe**:
- Должен быть в PATH
- Или в той же папке, что и market.exe

### Где взять curl.exe для Windows XP

1. **curl 7.88.1** (последняя версия с поддержкой XP):
   - https://curl.se/windows/
   - Скачать MinGW 32-bit версию

2. **Альтернатива**: использовать curl.exe из MinGW

## Сборка проекта

```bash
mingw32-make clean
mingw32-make
```

Результат: `out/market.exe`

## Структура проекта

```
legacymarket/
├── main.cpp          # Основной код (без libcurl)
├── makefile          # Упрощённый makefile
├── out/
│   └── market.exe    # Готовая программа (2.5 MB)
└── curl.exe          # Требуется для работы программы
```

## Технические детали

### Функции загрузки

**До (libcurl):**
```cpp
CURL* curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, url);
curl_easy_perform(curl);
```

**После (curl.exe):**
```cpp
std::string cmd = "curl.exe -k -L --max-time 10 -o \"file.ini\" \"" + url + "\"";
system(cmd.c_str());
```

### Флаги компиляции

- `-D_WIN32_WINNT=0x0501` - таргет Windows XP
- `-Wl,--subsystem,windows:5.01` - Windows XP subsystem
- Статическая линковка только с системными библиотеками

## Проверка совместимости

```bash
file out/market.exe
# Вывод: PE32 executable for MS Windows 5.01 (GUI), Intel i386
```

`5.01` = Windows XP

## Известные ограничения

1. **Прогресс-бар** - показывает только 50% во время загрузки (нет реального прогресса)
2. **curl.exe обязателен** - программа не запустится без него
3. **Консоль может мелькнуть** - при вызове system() на мгновение появляется консоль

## История изменений

- **2026-05-20**: Переход на curl.exe, полная совместимость с XP
- **2026-05-20**: Попытки API hooking и сборки старого cURL
- **2026-05-20**: Первоначальная проблема с GetTickCount64

---

**Итог**: Программа теперь полностью совместима с Windows XP! 🎉
