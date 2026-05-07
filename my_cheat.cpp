#include <windows.h>
#include <stdint.h>

// Используем диапазон, который выдал твой успешный лог
DWORD WINAPI UltimateTrigger(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    // Твой проверенный адрес игрока (из последнего удачного лога)
    // ВНИМАНИЕ: Если перезапустил карту, адрес может измениться!
    uintptr_t localPlayerPawn = 0x2057720; 

    while (true) {
        uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(client + localPlayerPawn);
        
        if (localPlayer > 0x1000000) {
            // Сканируем только те 3 оффсета, которые "мигали" в твоем логе
            uintptr_t targetOffsets[] = { 0x13C8, 0x13A8, 0x1458, 0x1544 };
            
            for (uintptr_t off : targetOffsets) {
                int id = *reinterpret_cast<int*>(localPlayer + off);

                // Если ID врага (1-64)
                if (id > 0 && id <= 64) {
                    // Используем самый надежный метод клика
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(20);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    
                    Sleep(200); // Пауза, чтобы не спамить
                    break;
                }
            }
        }
        Sleep(1); // Максимальная скорость
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)UltimateTrigger, nullptr, 0, nullptr);
    }
    return TRUE;
}
