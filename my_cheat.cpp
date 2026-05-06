#include <windows.h>
#include <fstream>
#include <stdint.h>

DWORD WINAPI OffsetScannerThread(LPVOID lpParam) {
    std::ofstream log("CS2_Scanner_Log.txt", std::ios::app);
    log << "--- STARTING AUTO-SCAN ---\n";

    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    // Используем твой новый рабочий адрес (0x2057720)
    uintptr_t localPlayerPawn = 0x2057720;

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)(client + localPlayerPawn);
        if (localPlayer > 0x1000) {
            // Сканируем диапазон, где обычно лежит ID прицела (от 0x1300 до 0x1600)
            for (uintptr_t offset = 0x1300; offset <= 0x1600; offset += 4) {
                int value = *(int*)(localPlayer + offset);
                
                // Если мы видим ID врага (от 1 до 64)
                if (value > 0 && value <= 64) {
                    log << "[FOUND CANDIDATE] Offset: 0x" << std::hex << offset << " Value: " << std::dec << value << "\n";
                    log.flush();
                    
                    // ЕСЛИ ЭТО ТОТ САМЫЙ ОФФСЕТ - СТРЕЛЯЕМ!
                    INPUT input = { 0 };
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                    SendInput(1, &input, sizeof(INPUT));
                    Sleep(10);
                    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                    SendInput(1, &input, sizeof(INPUT));
                }
            }
        }
        Sleep(100); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, OffsetScannerThread, 0, 0, 0));
    return TRUE;
}
