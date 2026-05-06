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

// --- Функция поиска паттерна ---
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;

    auto PatternToBytes = [](const char* pattern) {
        std::vector<int> bytes;
        char* start = const_cast<char*>(pattern);
        char* end = const_cast<char*>(pattern) + (int)strlen(pattern);
        for (char* current = start; current < end; ++current) {
            if (*current == '?') {
                current++; if (*current == '?') current++;
                bytes.push_back(-1);
            } else {
                bytes.push_back((int)strtoul(current, &current, 16));
            }
        }
        return bytes;
    };

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(moduleBase) + dosHeader->e_lfanew);
    DWORD sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = (BYTE*)moduleBase;

    for (DWORD i = 0; i (entityListPtr);
    if (!entityList) return 0;

    uintptr_t chunk = *reinterpret_cast<uintptr_t*>(entityList + 0x8 * (index >> 9) + 0x10);
    if (!chunk) return 0;

    return *reinterpret_cast<uintptr_t*>(chunk + 0x78 * (index & 0x1FF));
}

// --- Основной поток чита ---
DWORD WINAPI CheatThread(LPVOID lpParam) {
    std::ofstream log("CS2_Entity_Log.txt", std::ios::app);
    log << "--- Start Scanning ---\n";
    log.flush();

    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    // Ищем сигнатуры
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

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(0, 0, CheatThread, 0, 0, 0);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
