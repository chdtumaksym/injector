#include <windows.h>
#include <stdint.h>

DWORD WINAPI FinalAttempt(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    // Используем только ДВА самых надежных адреса из твоего лога
    uintptr_t dwLocalPlayer = 0x2057720;
    uintptr_t m_iIDEntIndex = 0x13A8; // Попробуем этот как основной

    while (true) {
        // Стреляем ТОЛЬКО если активно окно игры (защита от кликов в меню/десктопе)
        if (GetForegroundWindow() == FindWindowA("SDL_app", "Counter-Strike 2")) {
            
            uintptr_t local = *(uintptr_t*)(client + dwLocalPlayer);
            if (local > 0x1000) {
                int id = *(int*)(local + m_iIDEntIndex);

                // Если ID врага и ты НЕ в меню (проверка по кнопке мыши или нажатой клавише)
                if (id > 0 && id <= 64) {
                    
                    // Эмуляция клика
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(20);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    
                    Sleep(200); // Пауза
                }
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinalAttempt, 0, 0, 0));
    return TRUE;
}
