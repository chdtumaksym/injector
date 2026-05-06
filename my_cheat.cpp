#include <windows.h>
#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
    constexpr uintptr_t m_iIDEntIndex = 0x1544; 
}

// 1. Поиск игрока в списке
uintptr_t GetEntity(uintptr_t listPtr, int idx) {
    if (!listPtr) return 0;
    uintptr_t list = *reinterpret_cast<uintptr_t*>(listPtr);
    if (!list) return 0;
    uintptr_t chunk = *reinterpret_cast<uintptr_t*>(list + 8 * (static_cast<uintptr_t>(idx) >> 9) + 0x10);
    if (!chunk) return 0;
    return *reinterpret_cast<uintptr_t*>(chunk + 0x78 * (static_cast<uintptr_t>(idx) & 0x1FF));
}

// 2. Поиск паттерна
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;
    
    auto PatternToBytes = [](const char* p) {
        std::vector<int> bytes;
        char* start = const_cast<char*>(p);
        char* end = const_cast<char*>(p) + strlen(p);
        for (char* curr = start; curr < end; ++curr) {
            if (*curr == '?') { curr++; if (*curr == '?') curr++; bytes.push_back(-1); }
            else { bytes.push_back((int)strtoul(curr, &curr, 16)); }
        }
        return bytes;
    };

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(moduleBase) + dos->e_lfanew);
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = (BYTE*)moduleBase;
    for (DWORD i = 0; i < nt->OptionalHeader.SizeOfImage - static_cast<DWORD>(patternBytes.size()); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != static_cast<BYTE>(patternBytes[j])) { found = false; break; }
        }
        if (found) return reinterpret_cast<uintptr_t>(scanStart + i);
    }
    return 0;
}

// 3. Основной поток (Дебаг-логгер)
DWORD WINAPI CheatThread(LPVOID lpParam) {
    std::ofstream log("CS2_Debug_Log.txt", std::ios::app);
    log << "--- NEW SESSION STARTED ---\n";
    log.flush();

    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) {
        log << "[!] ERROR: client.dll not found!\n"; log.flush();
        return 0;
    }
    log << "[+] Found client.dll at: " << std::hex << client << "\n";

    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) {
        log << "[!] ERROR: Patterns not found (Update required)!\n"; log.flush();
        return 0;
    }
    log << "[+] Patterns found successfully!\n";

    uintptr_t lpPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *reinterpret_cast<int32_t*>(elAddr + 3);

    log << "[+] Addresses resolved. Starting main loop...\n";
    log.flush();

    while (true) {
        uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(lpPtr);

        if (localPlayer) {
            int myTeam = *reinterpret_cast<int*>(localPlayer + offsets::m_iTeamNum);
            int crossId = *reinterpret_cast<int*>(localPlayer + offsets::m_iIDEntIndex);

            // Если кто-то попал в прицел
            if (crossId > 0 && crossId (target + offsets::m_iHealth);
                    int team = *reinterpret_cast<int*>(target + offsets::m_iTeamNum);
                    log << "    - HP: " << hp << " | Team: " << team << " | My Team: " << myTeam << "\n";
                    
                    if (hp > 0 && team != myTeam) {
                        log << "    - ACTION: TRIGGER SHOT!\n";
                        INPUT input = { 0 };
                        input.type = INPUT_MOUSE;
                        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                        SendInput(1, &input, sizeof(INPUT));
                        Sleep(20);
                        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                        SendInput(1, &input, sizeof(INPUT));
                    }
                }
                log.flush();
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(CheatThread), nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
