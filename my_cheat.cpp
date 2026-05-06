#include <windows.h>
#include <fstream>

struct Vector3 { float x, y, z; };

namespace offsets {
    // ВНИМАНИЕ: Если опять вылетит, значит эти цифры всё же не те!
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824E18; 
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
}

DWORD WINAPI AnalyzeThread(LPVOID lpParam) {
    // Получаем путь к рабочему столу
    char desktopPath[MAX_PATH];
    ExpandEnvironmentStringsA("%USERPROFILE%\\Desktop\\Bypass_Log.txt", desktopPath, MAX_PATH);
    
    std::ofstream log(desktopPath, std::ios::app);
    log << "DLL STARTED" << std::endl; // std::endl принудительно пишет в файл

    uintptr_t clientModule = (uintptr_t)GetModuleHandleA("client.dll");
    
    // Даем игре 5 секунд "продышаться" после инъекции
    Sleep(5000);

    while (true) {
        if (clientModule) {
            // Читаем очень аккуратно через указатель
            uintptr_t* pLocalPlayer = (uintptr_t*)(clientModule + offsets::dwLocalPlayerPawn);
            
            // Проверяем, не указывает ли адрес в пустоту (0)
            if (pLocalPlayer && *pLocalPlayer > 0x1000) {
                uintptr_t localPlayerPawn = *pLocalPlayer;
                Vector3* pPos = (Vector3*)(localPlayerPawn + offsets::m_vOldOrigin);
                
                if (pPos) {
                    Vector3 pos = *pPos;
                    log << "X: " << pos.x << " Y: " << pos.y << std::endl;
                }
            }
        }
        log.flush(); // Гарантируем запись каждой строки
        Sleep(2000); // Читаем редко, чтобы не злить античит
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        CreateThread(NULL, 0, AnalyzeThread, NULL, 0, NULL);
    }
    return TRUE;
}
