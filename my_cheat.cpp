#include <windows.h>
#include <fstream>
#include <iostream>

// --- АКТУАЛЬНЫЕ ОФФСЕТЫ ИЗ ВАШЕГО ДАМПА ---
namespace offsets {
    // Из раздела "client.dll" -> "classes"
    // Мы берем смещения для поиска игрока и его координат
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824A18; // Смещение до локального игрока в client.dll
    constexpr uintptr_t m_vOldOrigin = 0x127C;        // Смещение координат внутри C_BasePlayerPawn
    constexpr uintptr_t m_iHealth = 0x334;            // Смещение здоровья (для проверки)
}

struct Vector3 { float x, y, z; };

DWORD WINAPI AnalyzeThread(LPVOID lpParam) {
    // Ждем, пока игра загрузится полностью
    uintptr_t clientModule = 0;
    while (!clientModule) {
        clientModule = (uintptr_t)GetModuleHandleA("client.dll");
        Sleep(500);
    }

    std::ofstream log("cs2_coordinates_log.txt", std::ios::app);
    log << "--- Start Logging Session ---\n";
    
    while (true) {
        // 1. Безопасное чтение указателя на локального игрока
        uintptr_t localPlayer = 0;
        if (clientModule + offsets::dwLocalPlayerPawn) {
            localPlayer = *(uintptr_t*)(clientModule + offsets::dwLocalPlayerPawn);
        }
        
        if (localPlayer) {
            // 2. Читаем координаты и здоровье
            // Мы используем прямой доступ к памяти, так как мы внутри процесса (Internal)
            Vector3 coords = *(Vector3*)(localPlayer + offsets::m_vOldOrigin);
            int health = *(int*)(localPlayer + offsets::m_iHealth);
            
            // 3. Записываем данные в файл
            if (health > 0) { // Логируем только если игрок живой
                log << "Health: " << health << " | Pos: " 
                    << coords.x << ", " << coords.y << ", " << coords.z << std::endl;
                log.flush();
            }
        }

        Sleep(1000); // Читаем данные раз в секунду
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Запускаем наш поток анализа отдельно от основного потока игры
        HANDLE hThread = CreateThread(NULL, 0, AnalyzeThread, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
