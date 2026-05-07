#include <windows.h>
#include <stdint.h>

namespace offsets {
    // Взял один из твоих найденных оффсетов, где точно был твой HP
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t m_iIDEntIndex = 0x13C8; // Тот самый, что "пищал" раньше
}

DWORD WINAPI InstantTrigger(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        // Читаем напрямую по адресу, который нашел сканер
        uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(client + offsets::dwLocalPlayerPawn);
        
        if (localPlayer > 0x1000) {
            int crossId = *reinterpret_cast<int*>(localPlayer + offsets::m_iIDEntIndex);

            // Если враг в прицеле - СТРЕЛЬБА БЕЗ ЗАДЕРЖЕК
            if (crossId > 0 && crossId <= 64) {
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                Sleep(10);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                
                Sleep(150); // Пауза между выстрелами
            }
        }
        // Спим 1 мс, чтобы не грузить проц, но стрелять быстро
        Sleep(1); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)InstantTrigger, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
