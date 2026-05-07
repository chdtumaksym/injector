#include <windows.h>
#include <vector>
#include <stdint.h>

namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t dwForceAttack = 0x181D120; // Оффсет стрельбы (проверь в дампере)
    constexpr uintptr_t m_iIDEntIndex = 0x13C8;    // Твой найденный оффсет прицела
}

DWORD WINAPI ForceAimThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        uintptr_t local = *reinterpret_cast<uintptr_t*>(client + offsets::dwLocalPlayerPawn);
        
        if (local > 0x1000) {
            int crossId = *reinterpret_cast<int*>(local + offsets::m_iIDEntIndex);

            // Если кто-то в прицеле
            if (crossId > 0 && crossId <= 64) {
                // ПИШЕМ ПРЯМО В ПАМЯТЬ: ВЫСТРЕЛ!
                *reinterpret_cast<int*>(client + offsets::dwForceAttack) = 65537; // Нажать
                Sleep(20);
                *reinterpret_cast<int*>(client + offsets::dwForceAttack) = 256;   // Отпустить
                
                Sleep(200); 
            }
        }
        Sleep(1); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        CloseHandle(CreateThread(0, 0, ForceAimThread, 0, 0, 0));
    }
    return TRUE;
}
