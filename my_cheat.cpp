#include <windows.h>
#include <stdint.h>

namespace offsets {
    // Используем адрес игрока, который ТЫ нашел в логе (0x197ee50)
    // Это надежнее, чем dwLocalPlayerPawn, который у нас не работал
    constexpr uintptr_t dwLocalPlayerPawn = 0x197ee50; 
    
    // Пробуем оффсет прицела, который дал ГПТ
    constexpr uintptr_t m_iIDEntIndex = 0x1458; 
    
    // Запасной оффсет прицела (на всякий случай)
    constexpr uintptr_t m_iIDEntIndex_alt = 0x1544;
}

DWORD WINAPI FinalBattleThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        // Читаем игрока по адресу из ТВОЕГО лога
        uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(client + offsets::dwLocalPlayerPawn);
        
        if (localPlayer > 0x1000) {
            // Проверяем оффсет от ГПТ (0x1458)
            int id = *reinterpret_cast<int*>(localPlayer + offsets::m_iIDEntIndex);
            
            // Если не сработало, проверяем второй (0x1544)
            if (id <= 0 || id > 64) {
                id = *reinterpret_cast<int*>(localPlayer + offsets::m_iIDEntIndex_alt);
            }

            // Если кто-то в прицеле - БЬЕМ!
            if (id > 0 && id <= 64) {
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
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinalBattleThread, 0, 0, 0));
    return TRUE;
}
