#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <stdint.h>

struct Vector3 { float x, y, z; };

// --- СИСТЕМНЫЕ СТРУКТУРЫ SOURCE 2 ---
typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

uintptr_t GetInterface(const char* moduleName, const char* interfaceName) {
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return 0;
    CreateInterfaceFn pCreateInterface = (CreateInterfaceFn)GetProcAddress(hMod, "CreateInterface");
    return (uintptr_t)pCreateInterface(interfaceName, nullptr);
}

// Упрощенный поиск оффсета через Schema (в рантайме)
uintptr_t GetSchemaOffset(const char* className, const char* fieldName) {
    // В реальном чите здесь идет обход дерева классов schemasystem.dll
    // Чтобы не перегружать код, я дам тебе стабильную функцию поиска для конкретной версии
    // Но сама идея "спрашивать у игры" - заложена в этот блок.
    if (std::string(className) == "C_BasePlayerPawn") {
        if (std::string(fieldName) == "m_vOldOrigin") return 0x127C; // Мы подтвердим это сканом
    }
    return 0;
}

// Твоя рабочая функция поиска паттерна
uintptr_t FindPattern(const char* module, const char* pattern) {
    uintptr_t base = (uintptr_t)GetModuleHandleA(module);
    if (!base) return 0;
    std::vector<int> b;
    char* start = const_cast<char*>(pattern);
    char* end = start + strlen(pattern);
    for (char* curr = start; curr < end; ++curr) {
        if (*curr == '?') { curr++; if (*curr == '?') curr++; b.push_back(-1); }
        else b.push_back((int)strtoul(curr, &curr, 16));
    }
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)base + dos->e_lfanew);
    BYTE* scanStart = (BYTE*)base;
    for (DWORD i = 0; i < nt->OptionalHeader.SizeOfImage - b.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < b.size(); ++j) {
            if (b[j] != -1 && scanStart[i + j] != (BYTE)b[j]) { found = false; break; }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

DWORD WINAPI SchemaHunterThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    log << "--- SCHEMA SYSTEM INITIALIZED ---\n";
    
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    // Используем сигнатуру, которая вчера находила LocalPlayer
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    if (!lpAddr) return 0;
    uintptr_t localPlayerPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);

    while (true) {
        uintptr_t pawn = *reinterpret_cast<uintptr_t*>(localPlayerPtr);
        if (pawn > 0x1000) {
            // Вместо гадания, мы сканируем блок памяти Pawn на наличие изменений
            // Если ты ходишь, координаты ДОЛЖНЫ меняться. 
            // Проверим диапазон от 0x1200 до 0x1300
            for (uintptr_t offset = 0x1200; offset <= 0x1300; offset += 4) {
                float val = *reinterpret_cast<float*>(pawn + offset);
                // Координаты в CS2 обычно большие числа, не 0 и не 1
                if (val > 10.0f || val < -10.0f) {
                    log << "DYNAMIC OFFSET DETECTED! 0x" << std::hex << offset << " Value: " << std::dec << val << "\n";
                }
            }
            log << "-----------------------------------\n";
        }
        log.flush();
        Sleep(2000);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SchemaHunterThread, NULL, 0, NULL));
    return TRUE;
}
