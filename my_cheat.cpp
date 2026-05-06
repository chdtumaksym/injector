#include <windows.h>
#include <fstream>

DWORD WINAPI FinalDebugThread(LPVOID lpParam) {
    std::ofstream log("CS2_Debug_Log.txt", std::ios::app);
    log << "[!] Searching for game data...\n";
    log.flush();

    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) {
        log << "[ERROR] client.dll not found!\n";
        return 0;
    }

    // Вместо оффсета, попробуем проверить сам базовый адрес
    log << "[INFO] client.dll base: " << std::hex << client << "\n";
    
    // Проверим, доступен ли наш оффсет для чтения вообще
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)(client + 0x1824E18), &mbi, sizeof(mbi))) {
        log << "[INFO] Memory protection at offset: " << std::hex << mbi.Protect << "\n";
        if (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) {
            log << "[INFO] Offset is readable! Waiting for target...\n";
        } else {
            log << "[WARN] Offset is NOT readable or protected!\n";
        }
    }
    log.flush();

    while (true) {
        // Мы просто будем ждать, пока что-то изменится в памяти
        // Если лог не растет, значит данные игрока лежат СОВСЕМ в другом месте
        Sleep(5000);
        log << "Scanning...\n";
        log.flush();
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(0, 0, FinalDebugThread, 0, 0, 0));
    return TRUE;
}
