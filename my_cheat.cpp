#include <windows.h>
#include <vector>
#include <math.h>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    constexpr uintptr_t dwViewAngles = 0x11F012C; // Исправлено под твой скрин
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
}

// Поиск паттерна
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)moduleBase + dos->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
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
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = (BYTE*)moduleBase;
    for (DWORD i = 0; i < size - (DWORD)patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != (BYTE)patternBytes[j]) { found = false; break; }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

uintptr_t GetEntity(uintptr_t listPtr, int idx) {
    uintptr_t list = *(uintptr_t*)listPtr;
    if (!list) return 0;
    uintptr_t chunk = *(uintptr_t*)(list + 8 * (idx >> 9) + 0x10);
    if (!chunk) return 0;
    return *(uintptr_t*)(chunk + 0x78 * (idx & 0x1FF));
}

DWORD WINAPI AimThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) return 0;

    uintptr_t lpPtr = lpAddr + 7 + *(int32_t*)(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *(int32_t*)(elAddr + 3);
    uintptr_t viewAnglesAddr = client + offsets::dwViewAngles;

    while (true) {
        uintptr_t local = *(uintptr_t*)lpPtr;
        if (!local) { Sleep(10); continue; }

        int myTeam = *(int*)(local + offsets::m_iTeamNum);
        Vector3 myPos = *(Vector3*)(local + offsets::m_vOldOrigin);

        float bestDist = 999999.0f;
        Vector3 targetPos = {0,0,0};
        bool found = false;

        for (int i = 1; i < 64; i++) {
            uintptr_t ent = GetEntity(elPtr, i);
            if (!ent || ent == local) continue;
            if (*(int*)(ent + offsets::m_iHealth) <= 0 || *(int*)(ent + offsets::m_iTeamNum) == myTeam) continue;

            Vector3 entPos = *(Vector3*)(ent + offsets::m_vOldOrigin);
            float dist = sqrt(pow(entPos.x - myPos.x, 2) + pow(entPos.y - myPos.y, 2));
            
            if (dist < bestDist) {
                bestDist = dist;
                targetPos = entPos;
                found = true;
            }
        }

        if (found && GetAsyncKeyState(VK_LBUTTON)) {
            Vector3 delta = { targetPos.x - myPos.x, targetPos.y - myPos.y, targetPos.z - myPos.z + 50.0f };
            float hyp = sqrt(delta.x * delta.x + delta.y * delta.y);
            float pitch = -atan2(delta.z, hyp) * (180.0f / 3.14159265f);
            float yaw = atan2(delta.y, delta.x) * (180.0f / 3.14159265f);

            *(float*)(viewAnglesAddr) = pitch;
            *(float*)(viewAnglesAddr + 4) = yaw;
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, AimThread, 0, 0, 0));
    return TRUE;
}
