#include <windows.h>
#include <fstream>
#include <stdint.h>

namespace offsets {
    // Вставляем твои ПРОВЕРЕННЫЕ цифры из offsets.json
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824E18; 
    constexpr uintptr_t dwEntityList = 0x19BDD58; // Проверь эту цифру в своем json!
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
    constexpr uintptr_t m_iIDEntIndex = 0x1544; 
}

DWORD WINAPI FinalThread(LPVOID lpParam) {
    std::ofstream log("CS2_Debug_Log.txt", std::ios::app);
    log << "--- DIRECT ADDRESS SESSION ---\n";

    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
        if (localPlayer) {
            int crossId = *(int*)(localPlayer + offsets::m_iIDEntIndex);
            if (crossId > 0 && crossId <= 64) {
                log << "Target in sight! ID: " << crossId << "\n";
                log.flush();
                
                // Простейший триггер: стреляем сразу
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                Sleep(20);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinalThread, 0, 0, 0));
    return TRUE;
}
