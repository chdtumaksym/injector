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

// 1. Поиск сущности по индексу
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

// 3. Поток логирования
DWORD WINAPI CheatThread(LPVOID lpParam) {
    std::ofstream log("CS2_Debug_Log.txt", std::ios::app);
    log << "--- LOG STARTED ---\n";
    log.flush();

    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) {
        log << "Error: Patterns not found!\n"; log.flush();
        return 0;
    }

    uintptr_t lpPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *reinterpret_cast<int32_t*>(elAddr + 3);

    while (true) {
        uintptr_t local = *reinterpret_cast<uintptr_t*>(lpPtr);
        if (local) {
            int myTeam = *reinterpret_cast<int*>(local + offsets::m_iTeamNum);
            int crossId = *reinterpret_cast<int*>(local + offsets::m_iIDEntIndex);

            if (crossId > 0 && crossId <= 64) {
                uintptr_t targetEnt = GetEntity(elPtr, crossId - 1);
                if (targetEnt) {
                    int targetHP = *reinterpret_cast<int*>(targetEnt + offsets::m_iHealth);
                    int targetTeam = *reinterpret_cast<int*>(targetEnt + offsets::m_iTeamNum);
                    
                    log << "Target detected! HP: " << targetHP << " Team: " << targetTeam << "\n";
                    log.flush();

                    if (targetHP > 0 && targetTeam != myTeam) {
                        // Попытка выстрела
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        Sleep(20);
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    }
                }
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
