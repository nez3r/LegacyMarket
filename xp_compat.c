#include <windows.h>

// Реализация GetTickCount64 для Windows XP
// Эта функция будет использоваться вместо отсутствующей в kernel32.dll
ULONGLONG WINAPI GetTickCount64(void) {
    static DWORD last_tick = 0;
    static ULONGLONG tick_offset = 0;
    static CRITICAL_SECTION cs;
    static BOOL initialized = FALSE;

    // Инициализация критической секции для потокобезопасности
    if (!initialized) {
        InitializeCriticalSection(&cs);
        initialized = TRUE;
    }

    EnterCriticalSection(&cs);

    DWORD current_tick = GetTickCount();

    // Обнаружение переполнения (происходит каждые ~49.7 дней)
    if (current_tick < last_tick) {
        tick_offset += 0x100000000ULL;
    }

    last_tick = current_tick;
    ULONGLONG result = tick_offset + current_tick;

    LeaveCriticalSection(&cs);

    return result;
}
