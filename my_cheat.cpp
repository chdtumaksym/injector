#include <windows.h>
#include <fstream>
#include <string>

// Структура координат
struct Vector3 {
    float x, y, z;
};

// Сюда мы будем записывать актуальные оффсеты
// ВНИМАНИЕ: Эти цифры меняются после обновлений игры!
namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824A18; // Смещение до локального игрока
    constexpr uintptr_t m_vOldOrigin = 0x127C;        // Смещение до координат внутри игрока
}

DWORD WINAPI CheatThread(LPVOID lpParam) {
    // Получаем адрес модуля client.dll, где лежат все данные игры
    uintptr_t clientModule = (uintptr_t)GetModuleHandleA("client.dll");
    
    // Если модуль не найден (игра еще грузится), ждем
    while (!clientModule) {
        Sleep(1000);
        clientModule = (uintptr_t)GetModuleHandleA("client.dll");
    }

    // Создаем лог-файл в папке с игрой
    std::ofstream log("cs2_coordinates.txt", std::ios::app);
    log << "--- Start Logging ---" << std::endl;

    while (true) {
        // 1. Читаем указатель на нашего игрока (LocalPlayer)
        // Внутри процесса мы читаем память просто по указателям
        uintptr_t localPlayer = *(uintptr_t*)(clientModule + offsets::dwLocalPlayerPawn);

        if (localPlayer) {
            // 2. Читаем координаты из структуры игрока
            Vector3 pos = *(Vector3*)(localPlayer + offsets::m_vOldOrigin);

            // 3. Записываем координаты в файл, чтобы убедиться, что чит видит игру
            log << "X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z << std::endl;
            log.flush();
        }

        Sleep(500); // Читаем 2 раза в секунду, чтобы не грузить проц
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Создаем поток для нашей логики
        HANDLE hThread = CreateThread(NULL, 0, CheatThread, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
