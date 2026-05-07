#include <windows.h>
#include <fstream>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    // Используем твой вчерашний рабочий адрес (измени, если лог будет пустым)
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t m_vOldOrigin = 0x127C; // Оффсет координат (Source 2 стандарт)
}

DWORD WINAPI CoordThread(LPVOID lpParam) {
    std::ofstream log("CS2_Coords_Log.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");

    log << "--- COORDINATE CHECK START ---\n";

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)(client + offsets::dwLocalPlayerPawn);
        if (localPlayer > 0x1000000) {
            Vector3 pos = *(Vector3*)(localPlayer + offsets::m_vOldOrigin);
            
            log << "X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z << "\n";
            log.flush();
        }
        Sleep(500); // Раз в полсекунды
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, CoordThread, 0, 0, 0));
    return TRUE;
}
