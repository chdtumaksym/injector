#include <windows.h>
#include <fstream>
#include <stdint.h>

// Функция для безопасной проверки: можно ли читать адрес?
bool IsAddrSafe(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFEFFFF) return false;
    return !IsBadReadPtr((void*)addr, sizeof(uintptr_t));
}

DWORD WINAPI RealHunterThread(LPVOID lpParam) {
    std::ofstream log("CS2_REAL_OFFSET.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    log << "--- STARTING SCAN FOR REAL PLAYER ---\n";
    log.flush();

    while (true) {
        // Сканируем память в поисках твоего здоровья (100)
        for (uintptr_t off = 0x1800000; off <= 0x2500000; off += 8) {
            uintptr_t ptrAddr = client + off;
            
            if (IsAddrSafe(ptrAddr)) {
                uintptr_t potentialPawnPtr = *(uintptr_t*)ptrAddr;
                
                if (IsAddrSafe(potentialPawnPtr)) {
                    int hp = *(int*)(potentialPawnPtr + 0x334); // m_iHealth
                    
                    if (hp == 100) { // НАШЛИ ТЕБЯ!
                        log << "FOUND PLAYER! Real Offset: 0x" << std::hex << off << "\n";
                        log.flush();
                        
                        // Попробуем выстрелить (ID прицела 0x1544)
                        if (IsAddrSafe(potentialPawnPtr + 0x1544)) {
                            int crossId = *(int*)(potentialPawnPtr + 0x1544);
                            if (crossId > 0 && crossId <= 64) {
                                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                                Sleep(10);
                                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                                Sleep(200);
                            }
                        }
                    }
                }
            }
        }
        log << "Scan iteration finished.\n";
        log.flush();
        Sleep(5000); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)RealHunterThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
