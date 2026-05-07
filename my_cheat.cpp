#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

struct Vector3 { float x, y, z; };

// Функция поиска паттерна (сигнатуры)
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

DWORD WINAPI HunterThread(LPVOID lpParam) {
    std::ofstream log("CS2_Final_Debug.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    log << "--- AUTO-HUNTER START ---\n";

    // 1. Ищем ЛОКАЛЬНОГО ИГРОКА по сигнатуре
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    if (lpAddr) {
        // Извлекаем адрес из инструкции (RIP-relative)
        uintptr_t localPlayerPtr = lpAddr + 7 + *(int32_t*)(lpAddr + 3);
        log << "Found LocalPlayerPtr at: " << std::hex << localPlayerPtr << "\n";

        while (true) {
            uintptr_t localPlayer = *(uintptr_t*)localPlayerPtr;
            if (localPlayer > 0x1000) {
                // Оффсет координат m_vOldOrigin обычно 0x127C
                Vector3 pos = *(Vector3*)(localPlayer + 0x127C);
                log << "LIVE POS -> X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z << "\n";
            } else {
                log << "Waiting for spawn...\n";
            }
            log.flush();
            Sleep(1000);
        }
    } else {
        log << "ERROR: Pattern for LocalPlayer not found!\n";
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, HunterThread, 0, 0, 0));
    return TRUE;
}
