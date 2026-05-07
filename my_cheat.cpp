#include <windows.h>
#include <stdint.h>

DWORD WINAPI TruthHunter(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    uintptr_t localPlayerPawn = 0x2057720; 
    uintptr_t foundOffset = 0;

    while (true) {
        uintptr_t local = *(uintptr_t*)(client + localPlayerPawn);
        if (local > 0x1000000) {
            
            if (foundOffset == 0) {
                // ШАГ 1: ОБУЧЕНИЕ (Наведи на бота и зажми ПКМ)
                if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
                    for (uintptr_t i = 0x1300; i < 0x1600; i += 4) {
                        int val = *(int*)(local + i);
                        if (val > 0 && val <= 64) {
                            // ШАГ 2: ПРОВЕРКА НА МУСОР
                            // Ждем 100мс и смотрим: если ты убрал прицел, значение ДОЛЖНО стать 0
                            Sleep(100); 
                            if (*(int*)(local + i) == 0) { 
                                // МЫ НАШЛИ ЕГО! Это адрес, который реагирует на наведение
                                foundOffset = i;
                                Beep(1200, 500); // Громкий длинный писк
                                break;
                            }
                        }
                    }
                }
            } else {
                // РАБОТА: Чистый триггер
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
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, TruthHunter, 0, 0, 0));
    return TRUE;
}
