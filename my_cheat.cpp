#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    // Актуальные смещения для цепочки Source 2
    constexpr uintptr_t dwLocalPlayerController = 0x1912578; 
    constexpr uintptr_t dwEntityList = 0x18C2D58; 
    constexpr uintptr_t m_hPawn = 0x60C;            // Хэндл на тело в контроллере
    constexpr uintptr_t m_vOldOrigin = 0x127C;     // Координаты в теле (Pawn)
}

DWORD WINAPI GlobalChainThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    log  9) + 0x10);
                if (listEntry) {
                    uintptr_t localPawn = *(uintptr_t*)(listEntry + 120 * (playerPawnHandle & 0x1FF));
                    
                    if (localPawn) {
                        // 4. И вот теперь читаем КООРДИНАТЫ из реального ТЕЛА
                        Vector3 pos = *(Vector3*)(localPawn + offsets::m_vOldOrigin);
                        
                        log << "REAL PAWN FOUND! X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z << "\n";
                    }
                }
            }
        } else {
            log << "Searching for LocalController...\n";
        }
        log.flush();
        Sleep(1000);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GlobalChainThread, NULL, 0, NULL));
    return TRUE;
}
