#include <windows.h>
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

// --- 1. ТЕЛО ФУНКЦИИ FindPattern ---
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
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)moduleBase + dos->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = (BYTE*)moduleBase;
    for (DWORD i = 0; i (entityListPtr);
    if (!entityList) return 0;
    uintptr_t chunk = *reinterpret_cast<uintptr_t*>(entityList + 0x8 * (index >> 9) + 0x10);
    if (!chunk) return 0;
    return *reinterpret_cast<uintptr_t*>(chunk + 0x78 * (index & 0x1FF));
}

// --- 3. ПОТОК ТРИГГЕРБОТА И АИМА ---
DWORD WINAPI SimpleFarmThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) return 0;

    uintptr_t lpPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *reinterpret_cast<int32_t*>(elAddr + 3);

    while (true) {
        uintptr_t local = *reinterpret_cast<uintptr_t*>(lpPtr);
        if (local) {
            int myTeam = *reinterpret_cast<int*>(local + offsets::m_iTeamNum);
            
            // ТРИГГЕРБОТ: Авто-выстрел при наведении
            int crosshairId = *reinterpret_cast<int*>(local + offsets::m_iIDEntIndex);
            if (crosshairId > 0 && crosshairId <= 64) {
                uintptr_t target = GetEntityByIndex(elPtr, crosshairId - 1);
                if (target) {
                    int targetTeam = *reinterpret_cast<int*>(target + offsets::m_iTeamNum);
                    int health = *reinterpret_cast<int*>(target + offsets::m_iHealth);
                    if (targetTeam != myTeam && health > 0) {
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
        CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)SimpleFarmThread, nullptr, 0, nullptr));
    }
    return TRUE;
}
