#include <windows.h>
#include <fstream>
#include <stdint.h>

DWORD WINAPI RealHunterThread(LPVOID lpParam) {
    std::ofstream log("CS2_REAL_OFFSET.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    log << "--- STARTING SCAN FOR REAL PLAYER ---\n";

    while (true) {
        // Мы сканируем память в поисках твоего здоровья (100)
        // Диапазон взят из типичных адресов CS2
        for (uintptr_t off = 0x1800000; off <= 0x2500000; off += 4) {
            uintptr_t potentialPawnPtr = *(uintptr_t*)(client + off);
            
            if (potentialPawnPtr > 0x1000000) { // Если это похоже на адрес
                __try {
                    int hp = *(int*)(potentialPawnPtr + 0x334); // m_iHealth
                    if (hp == 100) { // НАШЛИ ТЕБЯ!
                        log << "FOUND PLAYER! Real Offset: 0x" << std::hex << off << "\n";
                        log.flush();
                        
                        // Попробуем выстрелить по этому адресу
                        int crossId = *(int*)(potentialPawnPtr + 0x1544);
                        if (crossId > 0 && crossId <= 64) {
                            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                            Sleep(10);
                            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) { continue; }
            }
        }
        log << "Scan finished. If empty - addresses are protected.\n";
        log.flush();
        Sleep(5000); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, RealHunterThread, 0, 0, 0));
    return TRUE;
}
