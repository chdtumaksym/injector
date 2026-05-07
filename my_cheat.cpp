#include <windows.h>
#include <stdint.h>

// Твои последние данные из дампов
namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824A18; // Пробуем этот (из offsets.json)
    constexpr uintptr_t m_iIDEntIndex = 0x1544;       // Из твоего client_dll.json
    constexpr uintptr_t m_iTeamNum = 0x3CB;
    constexpr uintptr_t m_iHealth = 0x334;
}

// Функция "тихого" выстрела через прямое сообщение окну
void DoShot(HWND hwnd) {
    PostMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(0, 0));
    Sleep(10);
    PostMessage(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(0, 0));
}

DWORD WINAPI GodModeThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    HWND gameWnd = FindWindowA("SDL_app", "Counter-Strike 2");
    
    if (!client || !gameWnd) return 0;

    while (true) {
        // ОБОЛОЧКА ОТ ВЫЛЕТОВ: если адрес битый, программа просто пропустит цикл
        __try {
            uintptr_t localPlayer = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
            
            if (localPlayer > 0x1000000) { // Проверка на валидность адреса
                int crosshairId = *(int*)(localPlayer + offsets::m_iIDEntIndex);

                if (crosshairId > 0 && crosshairId <= 64) {
                    // Простейшая проверка на ХП (чтобы не стрелять в никуда)
                    int health = *(int*)(localPlayer + offsets::m_iHealth);
                    
                    if (health > 0) {
                        DoShot(gameWnd); // Стреляем через сообщения окна (не мышь!)
                        Sleep(250); // Задержка между выстрелами
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // Если случилась ошибка чтения - просто спим и пробуем снова
            Sleep(100);
        }
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(0, 0, GodModeThread, 0, 0, 0);
        if (hThread) {
            SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
            CloseHandle(hThread);
        }
    }
    return TRUE;
}
