#include <windows.h>
#include <stdint.h>

namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t m_iIDEntIndex = 0x13C8; // ТОТ САМЫЙ ОФФСЕТ ИЗ ТВОЕГО ЛОГА!
    constexpr uintptr_t m_iTeamNum = 0x3CB;
}

DWORD WINAPI FinalTriggerThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    // Даем игре время прогрузиться
    Sleep(5000);

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
        
        if (localPlayer > 0x1000) {
            int crosshairId = *(int*)(localPlayer + offsets::m_iIDEntIndex);

            // Если в прицеле враг (ID от 1 до 64)
            if (crosshairId > 0 && crosshairId <= 64) {
                
                // ТРИГГЕР: Одиночный четкий выстрел
                INPUT input[2] = { 0 };
                input[0].type = INPUT_MOUSE;
                input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                input[1].type = INPUT_MOUSE;
                input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                
                SendInput(2, input, sizeof(INPUT));
                
                // Пауза, чтобы не стрелять в ту же цель мгновенно
                Sleep(300); 
            }
        }
        Sleep(1); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinalTriggerThread, 0, 0, 0));
    return TRUE;
}
