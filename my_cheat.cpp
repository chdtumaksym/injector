#include <windows.h>
#include <stdint.h>

// ЭТО ТВОИ ЦИФРЫ ИЗ JSON. ЕСЛИ НЕ СРАБОТАЮТ - ЗНАЧИТ ДАМПЕР ВРЕТ.
namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824A18; // Проверь это число в offsets.json!
    constexpr uintptr_t m_iIDEntIndex = 0x1544;       // ПРАВИЛЬНЫЙ ИЗ ТВОЕГО JSON
}

DWORD WINAPI FinalAttemptThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        // Читаем адрес игрока
        uintptr_t local = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
        
        if (local > 0x1000) {
            int crosshairId = *(int*)(local + offsets::m_iIDEntIndex);

            // Если кто-то в прицеле
            if (crosshairId > 0 && crosshairId <= 64) {
                // ИСПОЛЬЗУЕМ КЛИК ЧЕРЕЗ KEYBD_EVENT (иногда игра его ест лучше)
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                Sleep(10);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                
                Sleep(200); 
            }
        }
        Sleep(1); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        // Ставим самый высокий приоритет, чтобы античит не успевал заморозить поток
        HANDLE hThread = CreateThread(0, 0, FinalAttemptThread, 0, 0, 0);
        if (hThread) {
            SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);
            CloseHandle(hThread);
        }
    }
    return TRUE;
}
