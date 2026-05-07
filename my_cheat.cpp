#include <windows.h>
#include <fstream>
#include <stdint.h>
#include <vector>

DWORD WINAPI TruthThread(LPVOID lpParam) {
    std::ofstream log("TRUTH_LOG.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t localPlayer = 0;

    log << "--- SEARCHING FOR THE TRUTH ---\n";
    log.flush();

    while (true) {
        localPlayer = *(uintptr_t*)(client + 0x2057720); // Твой проверенный адрес
        if (localPlayer > 0x1000) {
            // Если ты зажал ПРАВУЮ кнопку мыши (как триггер для сканера)
            if (GetAsyncKeyState(VK_RBUTTON)) {
                log << "Scanning while RBUTTON is pressed...\n";
                // Сканируем огромный кусок памяти игрока
                for (uintptr_t i = 0x1300; i < 0x1600; i += 4) {
                    int val = *(int*)(localPlayer + i);
                    if (val > 0 && val <= 64) {
                        log << "Potential ID Found! Offset: 0x" << std::hex << i << " Value: " << std::dec << val << "\n";
                        
                        // СРАЗУ СТРЕЛЯЕМ, чтобы ты понял, тот ли это адрес
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        Sleep(10);
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    }
                }
                log.flush();
                Sleep(500);
            }
        }
        Sleep(10);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, TruthThread, 0, 0, 0));
    return TRUE;
}
