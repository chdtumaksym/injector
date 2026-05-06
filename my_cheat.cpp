#include <windows.h>
#include <fstream>
#include <string>

// Структура координат
struct Vector3 { float x, y, z; };

namespace offsets {
    // ВНИМАНИЕ: Проверь эти значения в своем JSON файле (раздел "offsets")
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824A18; 
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
}

DWORD WINAPI AnalyzeThread(LPVOID lpParam) {
    uintptr_t clientModule = (uintptr_t)GetModuleHandleA("client.dll");
    while (!clientModule) {
        Sleep(500);
        clientModule = (uintptr_t)GetModuleHandleA("client.dll");
    }

    // Создаем файл на диске C, чтобы точно его найти
    std::ofstream log("C:\\cs2_test_log.txt", std::ios::app);
    log << "Started\n";

    while (true) {
        // Безопасная проверка: не читаем, если указатель на модуль битый
        if (clientModule) {
            uintptr_t localPlayerPawn = *(uintptr_t*)(clientModule + offsets::dwLocalPlayerPawn);
            
            // Если игрок заспавнился и указатель не нулевой
            if (localPlayerPawn != 0) {
                // Используем SEH (__try/__except) или простые проверки, чтобы не вылетать
                __try {
                    Vector3 pos = *(Vector3*)(localPlayerPawn + offsets::m_vOldOrigin);
                    log << "X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z << std::endl;
                    log.flush();
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    // Если адрес оказался битым, мы просто игнорируем ошибку и не вылетаем
                }
            }
        }
        Sleep(1000);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        CreateThread(NULL, 0, AnalyzeThread, NULL, 0, NULL);
    }
    return TRUE;
}
