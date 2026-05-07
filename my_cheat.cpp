#include <windows.h>
#include <stdint.h>

DWORD WINAPI FinalBossThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    // Пробуем ТРИ самых вероятных адреса одновременно
    uintptr_t testOffsets[] = { 0x1544, 0x13A8, 0x13C8 };

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)(client + 0x2057720);
        if (localPlayer > 0x1000) {
            
            for (uintptr_t off : testOffsets) {
                int id = *(int*)(localPlayer + off);
                
                // Если хоть один из них увидел врага
                if (id > 0 && id <= 64) {
                    // ГРОМКИЙ ПИСК (чтобы ты точно услышал, что контакт есть)
                    Beep(1200, 100); 

                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(20);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    
                    Sleep(300);
                    break;
                }
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinalBossThread, 0, 0, 0));
    return TRUE;
}
