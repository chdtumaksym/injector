#include <windows.h>
#include <stdint.h>

namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t m_iIDEntIndex = 0x13C8;
    // Оффсет для прямой стрельбы (проверь в дампере dwForceAttack)
    constexpr uintptr_t dwForceAttack = 0x181D120; // Примерный адрес для выстрела
}

DWORD WINAPI FinalForceThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
        if (localPlayer > 0x1000) {
            int crosshairId = *(int*)(localPlayer + offsets::m_iIDEntIndex);

            // Если в прицеле враг
            if (crosshairId > 0 && crosshairId <= 64) {
                // Вместо мышки, пишем команду "ВЫСТРЕЛ" прямо в память
                *(int*)(client + offsets::dwForceAttack) = 65537; // Нажать
                Sleep(20);
                *(int*)(client + offsets::dwForceAttack) = 256;   // Отпустить
                Sleep(150); 
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinalForceThread, 0, 0, 0));
    return TRUE;
}
