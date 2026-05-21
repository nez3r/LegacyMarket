#include <windows.h>

// Динамическая загрузка GetTickCount64 с fallback на GetTickCount для XP
typedef ULONGLONG (WINAPI *GetTickCount64Func)(void);

static GetTickCount64Func pGetTickCount64 = NULL;
static bool initialized = false;

extern "C" {
    // Настоящая GetTickCount64 из kernel32 (если есть)
    ULONGLONG WINAPI __real_GetTickCount64(void);

    // Обёртка для GetTickCount64 с поддержкой XP
    ULONGLONG WINAPI __wrap_GetTickCount64(void) {
        if (!initialized) {
            HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
            if (hKernel32) {
                pGetTickCount64 = (GetTickCount64Func)GetProcAddress(hKernel32, "GetTickCount64");
            }
            initialized = true;
        }

        if (pGetTickCount64) {
            // Windows Vista+ - используем настоящую GetTickCount64
            return pGetTickCount64();
        } else {
            // Windows XP - используем GetTickCount (32-bit)
            return (ULONGLONG)GetTickCount();
        }
    }
}
