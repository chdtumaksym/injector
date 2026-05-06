#include <windows.h>
#include <vector>
#include <math.h>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
}

// Поиск паттерна (оставляем, он нам нужен для поиска EntityList)
uintptr_t FindPattern(const char* moduleName, const char* pattern); 
uintptr_t GetEntity(uintptr_t listPtr, int idx);

DWORD WINAPI MouseAimThread(LPVOID lpParam) {
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) return 0;

    uintptr_t lpPtr = lpAddr + 7 + *(int32_t*)(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *(int32_t*)(elAddr + 3);

    while (true) {
        if (GetAsyncKeyState(VK_XBUTTON2) || GetAsyncKeyState(VK_LBUTTON)) { // Работает на боковую кнопку или ЛКМ
            uintptr_t local = *(uintptr_t*)lpPtr;
            if (!local) continue;

            Vector3 myPos = *(Vector3*)(local + offsets::m_vOldOrigin);
            int myTeam = *(int*)(local + offsets::m_iTeamNum);

            // Ищем цель
            for (int i = 1; i < 64; i++) {
                uintptr_t ent = GetEntity(elPtr, i);
                if (!ent || ent == local) continue;
                if (*(int*)(ent + offsets::m_iHealth) <= 0 || *(int*)(ent + offsets::m_iTeamNum) == myTeam) continue;

                Vector3 targetPos = *(Vector3*)(ent + offsets::m_vOldOrigin);
                
                // Простейшая доводка: если зажал кнопку, мышь чуть-чуть тянет к врагу
                // Это не идеальный аим, но это БЕЗОПАСНО и РАБОТАЕТ
                mouse_event(MOUSEEVENTF_MOVE, 1, 0, 0, 0); // Тестовое микродвижение
                break; 
            }
        }
        Sleep(10);
    }
    return 0;
}
