#include <windows.h>
#include <vector>
#include <string>
#include <stdint.h>

struct Vector3 { float x, y, z; };

// --- 1. Функция поиска паттерна (исправлена для MSVC) ---
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;

    auto PatternToBytes = [](const char* p) {
        std::vector<int> bytes;
        char* start = const_cast<char*>(p);
        char* end = const_cast<char*>(p) + strlen(p);
        for (char* curr = start; curr < end; ++curr) {
            if (*curr == '?') {
                curr++; if (*curr == '?') curr++;
                bytes.push_back(-1);
            } else {
                bytes.push_back((int)strtoul(curr, &curr, 16));
            }
        }
        return bytes;
    };

    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBase);
    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(moduleBase) + dosHeader->e_lfanew);
    
    DWORD sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = reinterpret_cast<BYTE*>(moduleBase);

    for (DWORD i = 0; i < sizeOfImage - static_cast<DWORD>(patternBytes.size()); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != static_cast<BYTE>(patternBytes[j])) {
                found = false;
                break;
            }
        }
        if (found) return reinterpret_cast<uintptr_t>(scanStart + i);
    }
    return 0;
}

// --- 2. Основной поток логики ---
DWORD WINAPI GlobalHunter(LPVOID lpParam) {
    uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    if (!client) return 0;

    // Ищем игрока по "отпечатку" (это надежнее оффсетов)
    uintptr_t addr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    if (!addr) return 0;
    
    uintptr_t localPlayerPtr = addr + 7 + *reinterpret_cast<int32_t*>(addr + 3);

    while (true) {
        HWND activeWnd = GetForegroundWindow();
        if (activeWnd) {
            char title[256];
            GetWindowTextA(activeWnd, title, sizeof(title));

            // Работаем только если окно CS2 активно
            if (strstr(title, "Counter-Strike 2")) {
                uintptr_t local = *reinterpret_cast<uintptr_t*>(localPlayerPtr);
                if (local > 0x1000) {
                    // Проверяем оффсеты прицела (один из них точно сработает)
                    uintptr_t crossOffsets[] = { 0x13A8, 0x1544, 0x13C8 };
                    for (uintptr_t off : crossOffsets) {
                        int id = *reinterpret_cast<int*>(local + off);
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
        }
        Sleep(1);
    }
    return 0;
}

// --- 3. Точка входа ---
BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(GlobalHunter), nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
