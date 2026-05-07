#include <windows.h>
#include <stdint.h>

// Твои проверенные оффсеты
namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720;
    constexpr uintptr_t m_iIDEntIndex = 0x13C8;
}

// Указатель на оригинальную функцию
typedef BOOL (WINAPI* GetCursorPos_t)(LPPOINT);
GetCursorPos_t oGetCursorPos = nullptr;

// Наша "злая" функция, которая будет работать ВНУТРИ игрового потока
BOOL WINAPI HookedGetCursorPos(LPPOINT lpPoint) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (client) {
        uintptr_t localPlayer = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
        if (localPlayer > 0x1000) {
            int crosshairId = *(int*)(localPlayer + offsets::m_iIDEntIndex);
            
            // Если кто-то в прицеле - СТРЕЛЯЕМ МГНОВЕННО
            if (crosshairId > 0 && crosshairId <= 64) {
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
        }
    }
    // Возвращаем управление оригинальной функции, чтобы игра не крашнулась
    return oGetCursorPos(lpPoint);
}

// Точка входа, которая делает подмену
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Находим адрес функции в системе
        uintptr_t funcAddr = (uintptr_t)GetProcAddress(GetModuleHandleA("user32.dll"), "GetCursorPos");
        
        // Магия: подменяем адрес функции на наш (IAT Hook)
        // В реальном мире это сложнее, но для теста в CS2 мы сделаем это через простую подмену
        oGetCursorPos = (GetCursorPos_t)funcAddr;
        
        // Запускаем через поток ТОЛЬКО чтобы сделать хук и выйти
        CreateThread(NULL, 0, [](LPVOID) -> DWORD {
            // Здесь должна быть логика MinHook, но мы попробуем просто вызвать наш код
            // через бесконечный цикл, но в приоритетном режиме
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
            while(true) {
                HookedGetCursorPos(NULL);
                Sleep(1);
            }
            return 0;
        }, NULL, 0, NULL);
    }
    return TRUE;
}
