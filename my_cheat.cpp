#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

DWORD WINAPI LearningTrigger(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t localPlayerPawn = 0x2057720; // Твой базовый оффсет
    uintptr_t foundOffset = 0;

    while (true) {
        uintptr_t local = *(uintptr_t*)(client + localPlayerPawn);
        if (local > 0x1000000) {
            
            // ЕСЛИ МЫ ЕЩЕ НЕ НАШЛИ ОФФСЕТ
            if (foundOffset == 0) {
                // Если ТЫ САМ нажал ЛКМ на враге
                if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                    for (uintptr_t i = 0x1300; i < 0x1600; i += 4) {
                        int val = *(int*)(local + i);
                        // Если в этот момент по адресу лежит ID врага (1-64)
                        if (val > 0 && val <= 64) {
                            foundOffset = i; // МЫ НАШЛИ ЕГО!
                            Beep(1000, 200); // Писк - оффсет найден!
                            break;
                        }
                    }
                }
            } 
            // ЕСЛИ ОФФСЕТ УЖЕ НАЙДЕН - РАБОТАЕМ КАК ТРИГГЕР
            else {
                int id = *(int*)(local + foundOffset);
                if (id > 0 && id <= 64) {
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(10);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    Sleep(200);
                }
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, LearningTrigger, 0, 0, 0));
    return TRUE;
}
