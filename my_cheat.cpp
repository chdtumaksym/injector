#include <windows.h>
#include <vector>
#include <math.h>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    constexpr uintptr_t dwViewAngles = 0x11F012C; 
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
}

// Поиск паттерна (универсальный)
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<BYTE*>(moduleBase) + dos->e_lfanew);
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

uintptr_t GetEntityByIndex(uintptr_t listPtr, int index) {
    uintptr_t list = *reinterpret_cast<uintptr_t*>(listPtr);
    if (!list) return 0;
    uintptr_t chunk = *reinterpret_cast<uintptr_t*>(list + 8 * (index >> 9) + 0x10);
    if (!chunk) return 0;
    return *reinterpret_cast<uintptr_t*>(chunk + 0x78 * (index & 0x1FF));
}

// Математика углов
void CalcAngle(Vector3 src, Vector3 dst, float* aimAngles) {
    float delta[3] = { (dst.x - src.x), (dst.y - src.y), (dst.z - src.z) };
    float hyp = sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
    aimAngles[0] = (float)(atan2(-delta[2], hyp) * 57.2957795f);
    aimAngles[1] = (float)(atan2(delta[1], delta[0]) * 57.2957795f);
}

DWORD WINAPI FullAutoFarmThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) return 0;

    uintptr_t lpPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *reinterpret_cast<int32_t*>(elAddr + 3);

    while (true) {
        // УБРАЛИ GetAsyncKeyState — теперь бот работает САМ ВСЕГДА
        uintptr_t local = *reinterpret_cast<uintptr_t*>(lpPtr);
        if (!local) { Sleep(100); continue; }

        Vector3 myPos = *reinterpret_cast<Vector3*>(local + offsets::m_vOldOrigin);
        int myTeam = *reinterpret_cast<int*>(local + offsets::m_iTeamNum);
        float* myAngles = reinterpret_cast<float*>(client + offsets::dwViewAngles);

        for (int i = 1; i < 64; i++) {
            uintptr_t ent = GetEntityByIndex(elPtr, i);
            if (!ent || ent == local) continue;
            if (*reinterpret_cast<int*>(ent + offsets::m_iHealth) <= 0 || *reinterpret_cast<int*>(ent + offsets::m_iTeamNum) == myTeam) continue;

            Vector3 targetPos = *reinterpret_cast<Vector3*>(ent + offsets::m_vOldOrigin);
            targetPos.z += 55.0f; // Целимся в голову

            float aimAngles[2];
            CalcAngle(myPos, targetPos, aimAngles);

            float diffYaw = myAngles[1] - aimAngles[1];
            if (diffYaw > 180) diffYaw -= 360;
            if (diffYaw < -180) diffYaw += 360;

            // 1. Быстрая доводка
            if (abs(diffYaw) > 0.5f) {
                int moveX = (diffYaw > 0) ? -6 : 6; // Увеличил скорость для фарма
                mouse_event(MOUSEEVENTF_MOVE, moveX, 0, 0, 0);
            } 
            // 2. Моментальный выстрел
            else {
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                Sleep(10);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
            break; 
        }
        Sleep(1); // Максимальная скорость цикла
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FullAutoFarmThread, 0, 0, 0));
    return TRUE;
}
