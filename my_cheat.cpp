#include <windows.h>
#include <fstream>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    // Актуальные смещения для текущего билда CS2
    constexpr uintptr_t dwLocalPlayerController = 0x1912578; 
    constexpr uintptr_t dwEntityList = 0x18C2D58; 
    constexpr uintptr_t m_hPawn = 0x60C;            
    constexpr uintptr_t m_vOldOrigin = 0x127C;     
}

// Безопасное и быстрое чтение памяти через проверку состояния страниц
template <typename T>
T ReadMem(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T();
    
    MEMORY_BASIC_INFORMATION mbi;
    // Проверяем статус страницы памяти перед разыменованием
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_NOACCESS) && !(mbi.Protect & PAGE_GUARD)) {
            return *reinterpret_cast<T*>(addr);
        }
    }
    return T();
}

DWORD WINAPI OptimizedChainThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    uintptr_t client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
    
    log << "--- VIRTUALQUERY CHAIN STARTED ---\n";
    log.flush();

    while (true) {
        if (client) {
            // 1. Контроллер
            uintptr_t localController = ReadMem<uintptr_t>(client + offsets::dwLocalPlayerController);
            if (localController) {
                // 2. Хэндл Пешки
                uint32_t pHandle = ReadMem<uint32_t>(localController + offsets::m_hPawn);
                if (pHandle != 0 && pHandle != 0xFFFFFFFF) {
                    // 3. Список сущностей
                    uintptr_t entityList = ReadMem<uintptr_t>(client + offsets::dwEntityList);
                    if (entityList) {
                        // 4. Правильный чанк EntityList (с битовыми сдвигами)
                        uintptr_t listEntry = ReadMem<uintptr_t>(entityList + 0x8 * ((pHandle & 0x7FFF) >> 9) + 0x10);
                        if (listEntry) {
                            // 5. Указатель на Пешку (Pawn)
                            uintptr_t localPawn = ReadMem<uintptr_t>(listEntry + 120 * (pHandle & 0x1FF));
                            if (localPawn) {
                                // 6. Координаты
                                Vector3 pos = ReadMem<Vector3>(localPawn + offsets::m_vOldOrigin);
                                
                                // Логируем всё: и адрес тела, и живые координаты
                                log << "SUCCESS! Pawn: " << std::hex << localPawn 
                                    << " | X: " << std::dec << pos.x << " Y: " << pos.y << " Z: " << pos.z << "\n";
                                log.flush();
                            }
                        }
                    }
                }
            }
        }
        Sleep(1000); // Опрос раз в секунду
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        if (HANDLE hThread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(OptimizedChainThread), nullptr, 0, nullptr)) {
            CloseHandle(hThread);
        }
    }
    return TRUE;
}
