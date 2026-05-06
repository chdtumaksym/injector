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
}

// 1. Поиск паттерна
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

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)moduleBase + dos->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = (BYTE*)moduleBase;

    for (DWORD i = 0; i < size - (DWORD)patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != (BYTE)patternBytes[j]) {
                found = false; break;
            }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

// 2. Получение игрока
uintptr_t GetEntity(uintptr_t listPtr, int idx) {
    uintptr_t list = *(uintptr_t*)listPtr;
    if (!list) return 0;
    uintptr_t chunk = *(uintptr_t*)(list + 8 * (idx >> 9) + 0x10);
    if (!chunk) return 0;
    return *(uintptr_t*)(chunk + 0x78 * (idx & 0x1FF));
}

// 3. Основной цикл
DWORD WINAPI CheatMain(LPVOID lpParam) {
    std::ofstream log("CS2_Entity_Log.txt", std::ios::app);
    log << "--- Start ---\n"; log.flush();

    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");

    if (!lpAddr || !elAddr) return 0;

    uintptr_t lpPtr = lpAddr + 7 + *(int32_t*)(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *(int32_t*)(elAddr + 3);

    while (true) {
        uintptr_t local = *(uintptr_t*)lpPtr;
        if (local) {
            int myTeam = *(int*)(local + offsets::m_iTeamNum);
            for (int i = 1; i < 64; i++) {
                uintptr_t ent = GetEntity(elPtr, i);
                if (!ent || ent == local) continue;

                int hp = *(int*)(ent + offsets::m_iHealth);
                int team = *(int*)(ent + offsets::m_iTeamNum);

                if (hp > 0 && hp <= 100 && team != myTeam) {
                    Vector3 pos = *(Vector3*)(ent + offsets::m_vOldOrigin);
                    log << "Enemy HP: " << hp << " | X: " << pos.x << "\n";
                    log.flush();
                }
            }
        }
        Sleep(1000);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, CheatMain, 0, 0, 0));
    return TRUE;
}
