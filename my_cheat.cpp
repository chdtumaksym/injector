#include <windows.h>
#include <fstream>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t dwEntityList = 0x24D19F0; 
    constexpr uintptr_t m_vOldOrigin = 0x127C;
}

DWORD WINAPI RadarThread(LPVOID lpParam) {
    std::ofstream log("CS2_Radar_Log.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");

    log << "--- RADAR START ---\n";

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
        uintptr_t entityList = *(uintptr_t*)(client + offsets::dwEntityList);

        if (localPlayer && entityList) {
            // Читаем твою позицию
            Vector3 myPos = *(Vector3*)(localPlayer + offsets::m_vOldOrigin);

            // Ищем первого врага в списке (индекс 1)
            // В CS2 список сущностей - это сложная структура, попробуем пробиться к первому чанку
            uintptr_t listEntry = *(uintptr_t*)(entityList + 0x10); 
            if (listEntry) {
                uintptr_t enemyPawn = *(uintptr_t*)(listEntry + 0x78 * 1); // 1-й индекс после тебя
                if (enemyPawn && enemyPawn != localPlayer) {
                    Vector3 enPos = *(Vector3*)(enemyPawn + offsets::m_vOldOrigin);
                    
                    log << "MY POS: " << myPos.x << " " << myPos.y << " " << myPos.z << "\n";
                    log << "EN POS: " << enPos.x << " " << enPos.y << " " << enPos.z << "\n";
                    log << "----------------------------\n";
                    log.flush();
                }
            }
        }
        Sleep(2000); // Раз в 2 секунды, чтобы не забить диск
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, RadarThread, 0, 0, 0));
    return TRUE;
}
