#include <windows.h>
#include <string.h>

// Реализация GetTickCount64 для Windows XP
static ULONGLONG WINAPI GetTickCount64_XP(void) {
    static DWORD last_tick = 0;
    static ULONGLONG tick_offset = 0;
    static CRITICAL_SECTION cs;
    static BOOL initialized = FALSE;

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

// Оригинальный GetProcAddress
static FARPROC (WINAPI *Real_GetProcAddress)(HMODULE hModule, LPCSTR lpProcName) = NULL;

// Перехватчик GetProcAddress
FARPROC WINAPI Hooked_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    // Если запрашивается GetTickCount64, возвращаем нашу реализацию
    if (lpProcName != NULL && !IsBadReadPtr(lpProcName, 1)) {
        if (HIWORD(lpProcName) != 0) { // Проверяем, что это строка, а не ординал
            if (strcmp(lpProcName, "GetTickCount64") == 0) {
                return (FARPROC)GetTickCount64_XP;
            }
        }
    }

    // Для всех остальных функций вызываем оригинальный GetProcAddress
    return Real_GetProcAddress(hModule, lpProcName);
}

// Инициализация перехвата
void InitXPCompat(void) {
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (hKernel32) {
        Real_GetProcAddress = (FARPROC (WINAPI *)(HMODULE, LPCSTR))GetProcAddress(hKernel32, "GetProcAddress");

        // Патчим таблицу импорта нашего процесса
        HMODULE hModule = GetModuleHandle(NULL);
        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
        PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + pDosHeader->e_lfanew);
        PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hModule +
            pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        while (pImportDesc->Name) {
            char* dllName = (char*)((BYTE*)hModule + pImportDesc->Name);

            if (_stricmp(dllName, "kernel32.dll") == 0) {
                PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + pImportDesc->FirstThunk);
                PIMAGE_THUNK_DATA pOrigThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + pImportDesc->OriginalFirstThunk);

                while (pThunk->u1.Function) {
                    if (pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                        // Импорт по ординалу
                    } else {
                        PIMAGE_IMPORT_BY_NAME pImport = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hModule + pOrigThunk->u1.AddressOfData);

                        if (strcmp((char*)pImport->Name, "GetProcAddress") == 0) {
                            DWORD oldProtect;
                            VirtualProtect(&pThunk->u1.Function, sizeof(FARPROC), PAGE_READWRITE, &oldProtect);
                            pThunk->u1.Function = (DWORD_PTR)Hooked_GetProcAddress;
                            VirtualProtect(&pThunk->u1.Function, sizeof(FARPROC), oldProtect, &oldProtect);
                            return;
                        }
                    }
                    pThunk++;
                    pOrigThunk++;
                }
            }
            pImportDesc++;
        }
    }
}
