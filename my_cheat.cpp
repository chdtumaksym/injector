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

// --- 1. Сначала объявляем функцию получения сущности ---
uintptr_t GetEntityByIndex(uintptr_t entityListPtr, int index) {
    if (!entityListPtr) return 0;
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(entityListPtr);
    if (!entityList) return 0;

    uintptr_t chunk = *reinterpret_cast<uintptr_t*>(entityList + 0x8 * (static_cast<uintptr_t>(index) >> 9) + 0x10);
    if (!chunk) return 0;

    return *reinterpret_cast<uintptr_t*>(chunk + 0x78 * (static_cast<uintptr_t>(index) & 0x1FF));
}

// --- 2. Затем функцию поиска паттерна ---
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
    BYTE* scanStart = (BYTE*)moduleBase;

    for (DWORD i = 0; i < size - static_cast<DWORD>(patternBytes.size()); ++i) {
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

// --- 3. И только потом основной поток ---
DWORD WINAPI SimpleFarmThread(LPVOID lpParam) {
    uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    if (!client) return 0;

    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) return 0;

    uintptr_t localPlayerPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);
    uintptr_t entityListPtr = elAddr + 7 + *reinterpret_cast<int32_t*>(elAddr + 3);

    while (true) {
        uintptr_t local = *reinterpret_cast<uintptr_t*>(localPlayerPtr);
        if (local) {
            int myTeam = *reinterpret_cast<int*>(local + offsets::m_iTeamNum);
            int crosshairId = *reinterpret_cast<int*>(local + offsets::m_iIDEntIndex);

            if (crosshairId > 0 && crosshairId (target + offsets::m_iTeamNum);
                    int health = *reinterpret_cast<int*>(target + offsets::m_iHealth);
                    
                    if (targetTeam != myTeam && health > 0 && health <= 100) {
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
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)SimpleFarmThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
