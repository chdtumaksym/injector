#include <windows.h>
#include <stdint.h>

DWORD WINAPI SmartTrigger(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t localPlayerPawn = 0x2057720; 
    uintptr_t foundOffset = 0;

    while (true) {
        uintptr_t local = *(uintptr_t*)(client + localPlayerPawn);
        if (local > 0x1000000) {
            
            if (foundOffset == 0) {
                // ОБУЧЕНИЕ: Зажми ПКМ, когда прицел ТОЧНО на враге
                if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
                    for (uintptr_t i = 0x1300; i < 0x1600; i += 4) {
                        int val = *(int*)(local + i);
                        // Если значение похоже на ID игрока и это НЕ мусор
                        if (val > 0 && val <= 64) {
                            // Проверка на стабильность: ID прицела не должен меняться каждую мс
                            Sleep(10);
                            if (*(int*)(local + i) == val) {
                                foundOffset = i;
                                Beep(1000, 300); // ПОБЕДНЫЙ ПИК
                                break;
                            }
                        }
                    }
                }
            } else {
                // РАБОТА: Стреляем только при наведении
                int id = *(int*)(local + foundOffset);
                if (id > 0 && id <= 64) {
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(20);
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    Sleep(250); 
                }
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, SmartTrigger, 0, 0, 0));
    return TRUE;
}
