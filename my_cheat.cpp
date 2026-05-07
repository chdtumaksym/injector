#include <windows.h>
#include <stdint.h>

namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t m_iIDEntIndex = 0x13C8; // Твой найденный оффсет
    constexpr uintptr_t m_iTeamNum = 0x3CB;    // Команда
}

DWORD WINAPI FinalFarmThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        // Проверяем, что окно CS2 активно, чтобы не кликать на рабочем столе
        if (GetForegroundWindow() == FindWindowA("SDL_app", "Counter-Strike 2")) {
            
            uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(client + offsets::dwLocalPlayerPawn);
            
            if (localPlayer > 0x1000) {
                int crossId = *reinterpret_cast<int*>(localPlayer + offsets::m_iIDEntIndex);

                // Если в прицеле КТО-ТО есть
                if (crossId > 0 && crossId <= 64) {
                    // Здесь можно добавить проверку на команду, если будут оффсеты,
                    // но для фарма в режиме "каждый сам за себя" этого уже хватит!
                    
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(20);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    
                    Sleep(150); // Пауза, чтобы не палиться и не лагать
                }
            }
        }
        Sleep(1); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)FinalFarmThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
