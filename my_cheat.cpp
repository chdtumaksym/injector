#include <windows.h>
#include <stdint.h>

namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t m_iIDEntIndex = 0x13A8; // ТВОЙ ПОБЕДНЫЙ ОФФСЕТ
}

DWORD WINAPI FinalTrigger(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        // Работает только если окно игры активно
        if (GetForegroundWindow() == FindWindowA("SDL_app", "Counter-Strike 2")) {
            uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(client + offsets::dwLocalPlayerPawn);
            
            if (localPlayer > 0x1000) {
                int crosshairId = *reinterpret_cast<int*>(localPlayer + offsets::m_iIDEntIndex);

                // Если ID врага (1-64)
                if (crosshairId > 0 && crosshairId <= 64) {
                    // Одиночный четкий выстрел
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(20);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    
                    // Пауза, чтобы не забанил античит за "автомат"
                    Sleep(200); 
                }
            }
        }
        Sleep(1); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)FinalTrigger, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
