#include <windows.h>
#include <fstream>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    // Актуальные смещения (проверь, чтобы dwEntityList был свежим!)
    constexpr uintptr_t dwLocalPlayerController = 0x1912578; 
    constexpr uintptr_t dwEntityList = 0x18C2D58; 
    constexpr uintptr_t m_hPawn = 0x60C;            
    constexpr uintptr_t m_vOldOrigin = 0x127C;     
}

// Функция для безопасного чтения (защита от краша)
template <typename T>
T ReadMem(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFEFFFF) return T();
    return *reinterpret_cast<T*>(addr);
}

DWORD WINAPI Source2ChainThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    
    log << "--- SOURCE 2 PRO-CHAIN STARTED ---\n";
    log.flush();

    while (true) {
        if (client) {
            // 1. Получаем LocalPlayerController
            uintptr_t localController = ReadMem<uintptr_t>(client + offsets::dwLocalPlayerController);
            
            if (localController) {
                // 2. Получаем хэндл пешки
                uint32_t pHandle = ReadMem<uint32_t>(localController + offsets::m_hPawn);
                
                if (pHandle != 0 && pHandle != 0xFFFFFFFF) {
                    // 3. Достаем EntityList
                    uintptr_t entityList = ReadMem<uintptr_t>(client + offsets::dwEntityList);
                    
                    if (entityList) {
                        // 4. Математика Чанка (по заветам Гемини)
                        uintptr_t listEntry = ReadMem<uintptr_t>(entityList + 0x8 * ((pHandle & 0x7FFF) >> 9) + 0x10);
                        
                        if (listEntry) {
                            // 5. Получаем саму Пешку (Pawn)
                            uintptr_t localPawn = ReadMem<uintptr_t>(listEntry + 120 * (pHandle & 0x1FF));
                            
                            if (localPawn) {
                                // 6. И только теперь — координаты!
                                Vector3 pos = ReadMem<Vector3>(localPawn + offsets::m_vOldOrigin);
                                
                                log << "SUCCESS! Pawn: " << std::hex << localPawn 
                                    << " | X: " << std::dec << pos.x << " Y: " << pos.y << "\n";
                                log.flush();
                            }
                        }
                    }
                }
            }
        }
        Sleep(1000);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(Source2ChainThread), nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
