#include <windows.h>
#include <vector>
#include <math.h>
#include <stdint.h>

struct Vector3 { float x, y, z; };

// Оффсеты (проверь, чтобы m_iIDEntIndex был актуален)
namespace offsets {
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
    constexpr uintptr_t m_iIDEntIndex = 0x1544; // Для триггербота
}

// Поиск паттерна (оставляем, он работает)
uintptr_t FindPattern(const char* moduleName, const char* pattern); 
uintptr_t GetEntityByIndex(uintptr_t listPtr, int index);

DWORD WINAPI SimpleFarmThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) return 0;

    uintptr_t lpPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *reinterpret_cast<int32_t*>(elAddr + 3);

    while (true) {
        uintptr_t local = *reinterpret_cast<uintptr_t*>(lpPtr);
        if (!local) { Sleep(100); continue; }

        int myTeam = *reinterpret_cast<int*>(local + offsets::m_iTeamNum);
        
        // --- ПРОСТЕЙШИЙ ТРИГГЕРБОТ ---
        // Если прицел наведен на врага, стреляем
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

        // --- ПРОСТЕЙШИЙ АИМ (ДОВОДКА) ---
        // Просто проверяем наличие врагов рядом
        for (int i = 1; i < 64; i++) {
            uintptr_t ent = GetEntityByIndex(elPtr, i);
            if (!ent || ent == local) continue;
            if (*reinterpret_cast<int*>(ent + offsets::m_iHealth) <= 0 || *reinterpret_cast<int*>(ent + offsets::m_iTeamNum) == myTeam) continue;

            // Если есть живой враг, просто делаем микро-движения мышкой
            // Это заставит твой прицел "дрожать" рядом с врагом
            if (GetAsyncKeyState(VK_LBUTTON)) { // Тестируем с зажатой ЛКМ
                mouse_event(MOUSEEVENTF_MOVE, 2, 0, 0, 0); 
            }
            break;
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CreateThread(0, 0, SimpleFarmThread, 0, 0, 0);
    return TRUE;
}
