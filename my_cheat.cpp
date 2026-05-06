#include <windows.h>
#include <fstream>

struct Vector3 { float x, y, z; };

namespace offsets {
    // Взято из твоего скриншота offsets.json (25315864)
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824E18; 
    // Взято из client_dll.json (m_vOldOrigin)
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
}

DWORD WINAPI AnalyzeThread(LPVOID lpParam) {
    uintptr_t clientModule = (uintptr_t)GetModuleHandleA("client.dll");
    while (!clientModule) {
        Sleep(500);
        clientModule = (uintptr_t)GetModuleHandleA("client.dll");
    }

    // Сохраняем файл в папку с игрой (game/bin/win64)
    std::ofstream log("Bypass_Log.txt", std::ios::app);
    log << "--- Start session ---\n";

    while (true) {
        if (clientModule) {
            // Безопасное чтение указателя
            uintptr_t localPlayerPawn = 0;
            __try {
                localPlayerPawn = *(uintptr_t*)(clientModule + offsets::dwLocalPlayerPawn);
            } __except (EXCEPTION_EXECUTE_HANDLER) { localPlayerPawn = 0; }

            if (localPlayerPawn) {
                __try {
                    Vector3 pos = *(Vector3*)(localPlayerPawn + offsets::m_vOldOrigin);
                    log << "Pos: " << pos.x << " " << pos.y << " " << pos.z << std::endl;
                    log.flush();
                } __except (EXCEPTION_EXECUTE_HANDLER) {}
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
