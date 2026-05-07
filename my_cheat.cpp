#include <windows.h>
#include <fstream>
#include <stdint.h>

DWORD WINAPI FinderThread(LPVOID lpParam) {
    std::ofstream log("FINAL_OFFSET.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t localPlayerPawn = 0x2057720; // Твой рабочий адрес игрока

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)(client + localPlayerPawn);
        if (localPlayer > 0x1000) {
            // Проверяем 4 самых вероятных кандидата из твоего лога
            uintptr_t targets[] = { 0x13A8, 0x13C8, 0x1410, 0x1544 };
            
            for (uintptr_t offset : targets) {
                int id = *(int*)(localPlayer + offset);
                
                // Если при наведении на врага появилось ID (1-64)
                if (id > 0 && id <= 64) {
                    // Пишем в лог, какой именно оффсет сработал
                    log << "TRIGGERED! Offset: 0x" << std::hex << offset << "\n";
                    log.flush();

                    // СТРЕЛЯЕМ
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(10);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    
                    Sleep(200); // Пауза, чтобы не спамить
                    break;
                }
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinderThread, 0, 0, 0));
    return TRUE;
}
