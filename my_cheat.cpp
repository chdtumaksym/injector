#include <windows.h>
#include <stdint.h>

namespace offsets {
    // ВСТАВИЛ ТВОИ НОВЫЕ ЦИФРЫ (HEX)
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t dwViewAngles = 0x2341888;
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
    constexpr uintptr_t m_iIDEntIndex = 0x1544; 
}

DWORD WINAPI AutoFarmThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        // 1. Безопасно получаем игрока
        uintptr_t localPlayer = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
        
        if (localPlayer > 0x1000) {
            // 2. Проверяем, кто в прицеле
            int crossId = *(int*)(localPlayer + offsets::m_iIDEntIndex);

            // Если ID врага (от 1 до 64)
            if (crossId > 0 && crossId <= 64) {
                // 3. СТРЕЛЬБА (TriggerBot)
                INPUT input[2] = {};
                input[0].type = INPUT_MOUSE;
                input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                input[1].type = INPUT_MOUSE;
                input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                
                SendInput(2, input, sizeof(INPUT));
                Sleep(150); // Задержка, чтобы не забанил VAC Net за слишком быструю стрельбу
            }
        }
        Sleep(1); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, AutoFarmThread, 0, 0, 0));
    return TRUE;
}
