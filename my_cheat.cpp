#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

struct Vector3 { float x, y, z; };

// Функция поиска паттерна
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
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)base + dos->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
    BYTE* scanStart = (BYTE*)base;
    for (DWORD i = 0; i < size - b.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < b.size(); ++j) {
            if (b[j] != -1 && scanStart[i + j] != (BYTE)b[j]) { found = false; break; }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

DWORD WINAPI LiveHunterThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    if (!lpAddr) return 0;

    uintptr_t localPlayerPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);

    while (true) {
        uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(localPlayerPtr);
        if (localPlayer > 0x1000) {
            // Пробуем два самых частых оффсета координат в CS2
            Vector3 pos1 = *reinterpret_cast<Vector3*>(localPlayer + 0x1274); // m_vAbsOrigin
            Vector3 pos2 = *reinterpret_cast<Vector3*>(localPlayer + 0x80);   // m_vecOrigin

            log << "POS A (0x1274) -> X: " << pos1.x << " Y: " << pos1.y << "\n";
            log << "POS B (0x80)   -> X: " << pos2.x << " Y: " << pos2.y << "\n";
            log << "-----------------------------------\n";
        }
        log.flush();
        Sleep(1000);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LiveHunterThread, NULL, 0, NULL));
    return TRUE;
}
