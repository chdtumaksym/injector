#include <windows.h>
#include <vector>
#include <math.h>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    constexpr uintptr_t dwViewAngles = 0x11F012C; // Проверь этот оффсет, если не будет наводиться
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
}

// Вспомогательные функции (FindPattern и GetEntityByIndex оставляем как были)
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

uintptr_t GetEntityByIndex(uintptr_t listPtr, int index) {
    uintptr_t list = *(uintptr_t*)listPtr;
    if (!list) return 0;
    uintptr_t chunk = *(uintptr_t*)(list + 8 * (index >> 9) + 0x10);
    if (!chunk) return 0;
    return *(uintptr_t*)(chunk + 0x78 * (index & 0x1FF));
}

// Математика для расчета углов
void CalcAngle(Vector3 src, Vector3 dst, float* angles) {
    double delta[3] = { (src.x - dst.x), (src.y - dst.y), (src.z - dst.z) };
    double hyp = sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
    angles[0] = (float)(asinf(delta[2] / hyp) * 57.295779513082f);
    angles[1] = (float)(atanf(delta[1] / delta[0]) * 57.295779513082f);
    angles[2] = 0.0f;
    if (delta[0] >= 0.0) angles[1] += 180.0f;
}

DWORD WINAPI AimThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    uintptr_t lpPtr = lpAddr + 7 + *(int32_t*)(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *(int32_t*)(elAddr + 3);

    while (true) {
        if (GetAsyncKeyState(VK_LBUTTON)) { // Аим на левую кнопку
            uintptr_t local = *(uintptr_t*)lpPtr;
            if (!local) continue;

            Vector3 myPos = *(Vector3*)(local + offsets::m_vOldOrigin);
            int myTeam = *(int*)(local + offsets::m_iTeamNum);
            float* myAngles = (float*)(client + offsets::dwViewAngles);

            for (int i = 1; i < 64; i++) {
                uintptr_t ent = GetEntityByIndex(elPtr, i);
                if (!ent || ent == local) continue;
                if (*(int*)(ent + offsets::m_iHealth) <= 0 || *(int*)(ent + offsets::m_iTeamNum) == myTeam) continue;

                Vector3 targetPos = *(Vector3*)(ent + offsets::m_vOldOrigin);
                targetPos.z += 60.0f; // Наводимся на голову (чуть выше центра)

                float angles[3];
                CalcAngle(myPos, targetPos, angles);

                // Рассчитываем разницу в пикселях (упрощенно)
                float diffYaw = myAngles[1] - angles[1];
                if (diffYaw > 180) diffYaw -= 360;
                if (diffYaw < -180) diffYaw += 360;

                // Если разница больше 1 градуса — двигаем мышь
                if (abs(diffYaw) > 0.5f) {
                    int moveX = (diffYaw > 0) ? -3 : 3; // Скорость доводки
                    mouse_event(MOUSEEVENTF_MOVE, moveX, 0, 0, 0);
                }
                break; 
            }
        }
        Sleep(5);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, AimThread, 0, 0, 0));
    return TRUE;
}
