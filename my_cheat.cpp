#include <windows.h>
#include <vector>
#include <math.h>
#include <stdint.h>

// Актуальные оффсеты (проверь в дампере, если будет вылетать)
namespace offsets {
    constexpr uintptr_t dwViewAngles = 0x1880990; // Адрес углов обзора
    constexpr uintptr_t m_vOldOrigin = 0x127C;    // Позиция
    constexpr uintptr_t m_iHealth = 0x334;        // ХП
    constexpr uintptr_t m_iTeamNum = 0x3CB;       // Команда
}

struct Vector3 { float x, y, z; };

// Математика аимбота
void VectorAngles(const Vector3& forward, float* angles) {
    float tmp, yaw, pitch;
    if (forward.y == 0 && forward.x == 0) {
        yaw = 0;
        pitch = (forward.z > 0) ? 270.0f : 90.0f;
    } else {
        yaw = (atan2(forward.y, forward.x) * 180 / 3.14159265358979323846f);
        if (yaw < 0) yaw += 360;
        tmp = sqrt(forward.x * forward.x + forward.y * forward.y);
        pitch = (atan2(-forward.z, tmp) * 180 / 3.14159265358979323846f);
        if (pitch < 0) pitch += 360;
    }
    angles[0] = pitch;
    angles[1] = yaw;
    angles[2] = 0;
}

// Поиск паттерна (оставляем старый, он рабочий)
uintptr_t FindPattern(const char* moduleName, const char* pattern); // реализация та же
uintptr_t GetEntity(uintptr_t listPtr, int idx); // реализация та же

DWORD WINAPI AimThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    uintptr_t lpPtr = lpAddr + 7 + *(int32_t*)(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *(int32_t*)(elAddr + 3);
    uintptr_t viewAnglesPtr = client + offsets::dwViewAngles;

    while (true) {
        uintptr_t local = *(uintptr_t*)lpPtr;
        if (!local) { Sleep(10); continue; }

        int myTeam = *(int*)(local + offsets::m_iTeamNum);
        Vector3 myPos = *(Vector3*)(local + offsets::m_vOldOrigin);

        // Ищем ближайшего врага
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

        // Если нашли цель — наводимся
        if (found && GetAsyncKeyState(VK_LBUTTON)) { // Работает при зажатой левой кнопке мыши
            Vector3 delta = { targetPos.x - myPos.x, targetPos.y - myPos.y, targetPos.z - myPos.z + 60.0f }; // +60 это высота головы
            float angles[3];
            VectorAngles(delta, angles);
            
            // Записываем новые углы в память игры
            *(float*)(viewAnglesPtr) = angles[0];
            *(float*)(viewAnglesPtr + 4) = angles[1];
        }
        Sleep(1);
    }
    return 0;
}
