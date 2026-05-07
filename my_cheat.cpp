#include <windows.h>
#include <fstream>
#include <stdint.h>

// Оффсеты, которые мы ПРОВЕРЯЕМ (на основе самых свежих дампов)
namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x1830A18; // Базовый адрес игрока
    constexpr uintptr_t m_iIDEntIndex = 0x1544;       // ID в прицеле
    constexpr uintptr_t m_iHealth = 0x334;            // Здоровье
}

DWORD WINAPI DiagnosticThread(LPVOID lpParam) {
    std::ofstream log("CS2_Final_Log.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    log << "--- DIAGNOSTIC START (Safe Mode) ---\n";
    log << "Client.dll base: " << std::hex << client << "\n";
    log.flush();

    while (true) {
        uintptr_t localPlayer = 0;
        SIZE_T br = 0;

        // 1. Проверяем, находим ли мы игрока
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(client + offsets::dwLocalPlayerPawn), &localPlayer, sizeof(localPlayer), &br) && localPlayer) {
            
            int hp = 0;
            int crossId = 0;
            
            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(localPlayer + offsets::m_iHealth), &hp, sizeof(hp), &br);
            ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(localPlayer + offsets::m_iIDEntIndex), &crossId, sizeof(crossId), &br);

            // Пишем в лог только когда данные меняются
            static int lastId = -1;
            if (crossId != lastId) {
                log << "Player Found! HP: " << std::dec << hp << " | Crosshair ID: " << crossId << "\n";
                log.flush();
                lastId = crossId;
            }

            // Если кто-то в прицеле - попробуем сделать ПИК (звук), чтобы ты знал без лога
            if (crossId > 0 && crossId <= 64) {
                Beep(1000, 100); 
                
                // Пробуем выстрел через простой mouse_event раз SendInput мог не сработать
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                Sleep(10);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
        } else {
            // Если даже игрока не нашел - значит dwLocalPlayerPawn неверный
            static bool errorLogged = false;
            if (!errorLogged) {
                log << "ERROR: LocalPlayer NOT FOUND. Wrong dwLocalPlayerPawn offset!\n";
                log.flush();
                errorLogged = true;
            }
        }
        Sleep(100); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, DiagnosticThread, 0, 0, 0));
    return TRUE;
}
