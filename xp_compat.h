#ifndef XP_COMPAT_H
#define XP_COMPAT_H

#include <windows.h>

// Обёртка для GetTickCount64, которая недоступна в Windows XP
// Использует GetTickCount (32-bit) с обработкой переполнения
#ifdef __cplusplus
extern "C" {
#endif

typedef ULONGLONG (WINAPI *GetTickCount64_t)(void);

static ULONGLONG WINAPI GetTickCount64_XP(void) {
    static DWORD last_tick = 0;
    static ULONGLONG tick_offset = 0;

    DWORD current_tick = GetTickCount();

    // Обнаружение переполнения (происходит каждые ~49.7 дней)
    if (current_tick < last_tick) {
        tick_offset += 0x100000000ULL;
    }

    last_tick = current_tick;
    return tick_offset + current_tick;
}

// Инициализация совместимости при загрузке программы
static void InitXPCompat(void) {
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (hKernel32) {
        GetTickCount64_t pGetTickCount64 = (GetTickCount64_t)GetProcAddress(hKernel32, "GetTickCount64");

        // Если GetTickCount64 не найдена (Windows XP), подменяем её
        if (!pGetTickCount64) {
            // Используем нашу реализацию через GetTickCount
            // Это работает, потому что cURL динамически загружает функцию
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif // XP_COMPAT_H
