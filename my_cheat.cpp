#include <windows.h>
#include <fstream>

struct Vector3 { float x, y, z; };

namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824E18; 
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
}

// Вспомогательная функция для безопасного чтения
template <typename T>
T ReadMemory(uintptr_t address) {
    if (address < 0x1000) return T{}; // Базовая проверка на мусорный адрес
    return *reinterpret_cast<T*>(address);
}

DWORD WINAPI AnalyzeThread(LPVOID lpParam) {
    uintptr_t clientModule = 0;
    while (!clientModule) {
        clientModule = (uintptr_t)GetModuleHandleA("client.dll");
        Sleep(500);
    }

    std::ofstream log("Bypass_Log.txt", std::ios::app);
    log << "--- Start session ---\n";

    while (true) {
        // Читаем указатель на игрока
        uintptr_t localPlayerPawn = ReadMemory<uintptr_t>(clientModule + offsets::dwLocalPlayerPawn);

        if (localPlayerPawn) {
            // Читаем координаты
            Vector3 pos = ReadMemory<Vector3>(localPlayerPawn + offsets::m_vOldOrigin);
            
            // Если координаты не нулевые (значит мы заспавнились)
            if (pos.x != 0 || pos.y != 0) {
                log << "Pos: " << pos.x << " " << pos.y << " " << pos.z << std::endl;
                log.flush();
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
