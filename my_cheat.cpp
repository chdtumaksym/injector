#include <windows.h>
#include <stdint.h>

DWORD WINAPI ProTrigger(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t localPlayerPawn = 0x2057720; 
    uintptr_t foundOffset = 0;

    while (true) {
        uintptr_t local = *(uintptr_t*)(client + localPlayerPawn);
        if (local > 0x1000000) {
            
            if (foundOffset == 0) {
                // ОБУЧЕНИЕ: Нажми ЛКМ ТОЛЬКО когда прицел на враге!
                if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                    for (uintptr_t i = 0x1300; i < 0x1600; i += 4) {
                        int val = *(int*)(local + i);
                        // Ищем адрес, который равен ID врага (1-64)
                        if (val > 0 && val <= 64) {
                            // Проверяем соседний байт, чтобы отсечь мусор
                            if (*(int*)(local + i + 4) == 0) { 
                                foundOffset = i;
                                Beep(1000, 200); // ПИК - НАШЛИ!
                                break;
                            }
                        }
                    }
                }
            } else {
                // РАБОТА: Стреляем только по окну игры
                if (GetForegroundWindow() == FindWindowA("SDL_app", "Counter-Strike 2")) {
                    int id = *(int*)(local + foundOffset);
                    if (id > 0 && id <= 64) {
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        Sleep(20);
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        Sleep(200); // Пауза, чтобы не было спама
                    }
                }
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, ProTrigger, 0, 0, 0));
    return TRUE;
}
