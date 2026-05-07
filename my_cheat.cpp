#include <windows.h>
#include <vector>
#include <stdint.h>

// 1. Умный поиск по байтам
uintptr_t FindPattern(const char* module, const char* pattern) {
    uintptr_t base = (uintptr_t)GetModuleHandleA(module);
    if (!base) return 0;
    
    std::vector<int> b;
    char* start = const_cast<char*>(pattern);
    char* end = start + strlen(pattern);
    for (char* curr = start; curr < end; ++curr) {
        if (*curr == '?') { curr++; if (*curr == '?') curr++; b.push_back(-1); }
        else b.push_back((int)strtoul(curr, &curr, 16));
    }

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    BYTE* scanStart = (BYTE*)base;
    for (DWORD i = 0; i < nt->OptionalHeader.SizeOfImage - b.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < b.size(); ++j) {
            if (b[j] != -1 && scanStart[i + j] != (BYTE)b[j]) { found = false; break; }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

DWORD WINAPI GlobalHunter(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    // Ищем игрока по "отпечатку"
    uintptr_t addr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    if (!addr) return 0;
    
    uintptr_t localPlayerPtr = addr + 7 + *(int32_t*)(addr + 3);

    while (true) {
        // Проверка: активно ли окно CS2
        HWND activeWnd = GetForegroundWindow();
        char title[256];
        GetWindowTextA(activeWnd, title, 256);

        if (strstr(title, "Counter-Strike 2")) {
            uintptr_t local = *(uintptr_t*)localPlayerPtr;
            if (local > 0x1000) {
                // Проверяем три самых частых оффсета прицела
                uintptr_t crossOffsets[] = { 0x13A8, 0x1544, 0x13C8 };
                for (uintptr_t off : crossOffsets) {
                    int id = *(int*)(local + off);
                    if (id > 0 && id <= 64) {
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        Sleep(10);
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        Sleep(200);
                        break;
                    }
                }
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, GlobalHunter, 0, 0, 0));
    return TRUE;
}
