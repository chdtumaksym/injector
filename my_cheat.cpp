#include <windows.h>
#include <fstream>
#include <vector>
#include <string>

// --- Продвинутый поиск сигнатур ---
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;

    auto PatternToBytes = [](const char* pattern) {
        std::vector<int> bytes;
        char* start = const_cast<char*>(pattern);
        char* end = const_cast<char*>(pattern) + strlen(pattern);
        for (char* current = start; current < end; ++current) {
            if (*current == '?') {
                current++;
                if (*current == '?') current++;
                bytes.push_back(-1);
            } else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
    };

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS*)(moduleBase + dosHeader->e_lfanew);
    DWORD sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = (BYTE*)moduleBase;

    for (DWORD i = 0; i < sizeOfImage - patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != patternBytes[j]) {
                found = false;
                break;
            }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

DWORD WINAPI CheatThread(LPVOID lpParam) {
    std::ofstream log("CS2_Pattern_Log.txt", std::ios::app);
    log << "--- Starting Signature Scan ---\n";

    // Ищем сигнатуру для LocalPlayerPawn
    // Это стандартный паттерн: 48 8B 0D ? ? ? ? 48 85 C9 74 ? 8B 01
    uintptr_t address = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 ? 8B 01");

    if (address) {
        // Извлекаем адрес из инструкции MOV RCX, [RIP + offset]
        int32_t offset = *(int32_t*)(address + 3);
        uintptr_t localPlayerPawn = address + 7 + offset;
        
        log << "Found LocalPlayerPawn at: " << std::hex << localPlayerPawn << "\n";
        
        while (true) {
            uintptr_t player = *(uintptr_t*)localPlayerPawn;
            if (player) {
                // Оффсет координат m_vOldOrigin обычно стабилен (0x127C)
                float* coords = (float*)(player + 0x127C);
                log << "X: " << coords[0] << " Y: " << coords[1] << " Z: " << coords[2] << "\n";
                log.flush();
            }
            Sleep(1000);
        }
    } else {
        log << "ERROR: Pattern not found!\n";
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CreateThread(0, 0, CheatThread, 0, 0, 0);
    return TRUE;
}
