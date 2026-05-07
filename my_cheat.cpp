#include <windows.h>
#include <stdint.h>

// Оффсеты прицела, которые дает GPT и другие дамперы
uintptr_t crossOffsets[] = { 0x1458, 0x1544, 0x13C8, 0x13A8 };

DWORD WINAPI FinalBattleThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    // Мы не используем статичный оффсет, мы будем искать игрока "на лету"
    uintptr_t localPlayer = 0;

    while (true) {
        // Если адрес потерялся или не найден - ищем его (твоим методом 100 HP)
        if (localPlayer == 0) {
            for (uintptr_t off = 0x1800000; off <= 0x2500000; off += 8) {
                uintptr_t* potential = (uintptr_t*)(client + off);
                if (!IsBadReadPtr(potential, sizeof(uintptr_t))) {
                    uintptr_t pawn = *potential;
                    if (pawn > 0x1000000 && !IsBadReadPtr((void*)(pawn + 0x334), 4)) {
                        if (*(int*)(pawn + 0x334) == 100) {
                            localPlayer = pawn; // НАШЛИ!
                            break;
                        }
                    }
                }
            }
        } else {
            // Если игрок найден - работаем по прицелу
            for (uintptr_t crossOff : crossOffsets) {
                if (!IsBadReadPtr((void*)(localPlayer + crossOff), 4)) {
                    int id = *(int*)(localPlayer + crossOff);
                    if (id > 0 && id <= 64) {
                        // КЛИК!
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        Sleep(10);
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        Sleep(200);
                        break;
                    }
                }
            }
            
            // Проверка: жив ли еще этот адрес? (Если ХП не 100 или адрес битый - сброс)
            if (IsBadReadPtr((void*)(localPlayer + 0x334), 4) || *(int*)(localPlayer + 0x334) != 100) {
                localPlayer = 0;
            }
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinalBattleThread, 0, 0, 0));
    return TRUE;
}
