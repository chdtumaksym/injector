#include <windows.h>
#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>

// --- Оффсеты ---
namespace offsets {
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
}

struct Vector3 { float x, y, z; };

// --- 1. Функция поиска паттерна ---
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
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(moduleBase) + dos->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = reinterpret_cast<BYTE*>(moduleBase);

    for (DWORD i = 0; i < size - patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != static_cast<BYTE>(patternBytes[j])) {
                found = false; break;
            }
        }
        if (found) return reinterpret_cast<uintptr_t>(scanStart + i);
    }
    return 0;
}

// --- 2. Функция получения сущности (перенесена ВЫШЕ для видимости) ---
uintptr_t GetEntityByIndex(uintptr_t entityListPtr, int index) {
    if (!entityListPtr) return 0;
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(entityListPtr);
    if (!entityList) return 0;

    uintptr_t chunk = *reinterpret_cast<uintptr_t*>(entityList + 0x8 * (static_cast<uintptr_t>(index) >> 9) + 0x10);
    if (!chunk) return 0;

    return *reinterpret_cast<uintptr_t*>(chunk + 0x78 * (static_cast<uintptr_t>(index) & 0x1FF));
}

// --- 3. Основной поток чита ---
DWORD WINAPI CheatThread(LPVOID lpParam) {
    std::ofstream log("CS2_Entity_Log.txt", std::ios::app);
    log << "--- Start Scanning ---\n";
    log.flush();

    uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    if (!client) return 0;

    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");

    if (!lpAddr || !elAddr) {
        log << "Error: Patterns not found!\n"; log.flush();
        return 0;
    }

    uintptr_t localPlayerPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);
    uintptr_t entityListPtr = elAddr + 7 + *reinterpret_cast<int32_t*>(elAddr + 3);

    while (true) {
        uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(localPlayerPtr);
        if (localPlayer) {
            int myTeam = *reinterpret_cast<int*>(localPlayer + offsets::m_iTeamNum);

            for (int i = 1; i (entity + offsets::m_iHealth);
                int team = *reinterpret_cast<int*>(entity + offsets::m_iTeamNum);

                if (health > 0 && health <= 100 && team != myTeam) {
                    Vector3 pos = *reinterpret_cast<Vector3*>(entity + offsets::m_vOldOrigin);
                    log << "Enemy [" << i << "] HP: " << health << " Pos: " << pos.x << " " << pos.y << std::endl;
                    log.flush();
                }
            }
        }
        Sleep(1000); 
    }
    return 0;
}

// --- 4. Точка входа ---
BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(CheatThread), nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
