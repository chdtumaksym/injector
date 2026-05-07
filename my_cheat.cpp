#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

struct Vector3 { float x, y, z; };

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

DWORD WINAPI RealPawnThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    // Сигнатура для LocalPlayerPawn (физическое тело)
    uintptr_t pawnAddr = FindPattern("client.dll", "48 8D 05 ? ? ? ? 48 8B D9 48 89 05 ? ? ? ? 48 85 C9 74 3F");
    
    if (pawnAddr) {
        // Вычисляем адрес
        uintptr_t localPawnPtr = pawnAddr + 7 + *reinterpret_cast<int32_t*>(pawnAddr + 3);
        log << "[+] Found REAL Pawn Pointer at: " << std::hex << localPawnPtr << "\n";

        while (true) {
            uintptr_t pawn = *reinterpret_cast<uintptr_t*>(localPawnPtr);
            if (pawn > 0x1000) {
                // В Pawn координаты лежат по оффсету 0x127C или 0xAC
                Vector3 pos1 = *reinterpret_cast<Vector3*>(pawn + 0x127C);
                Vector3 pos2 = *reinterpret_cast<Vector3*>(pawn + 0xAC);

                log << "PAWN 0x127C -> X: " << std::dec << pos1.x << " Y: " << pos1.y << "\n";
                log << "PAWN 0xAC   -> X: " << std::dec << pos2.x << " Y: " << pos2.y << "\n";
                log << "----------------------------\n";
            } else {
                log << "[-] Waiting for spawn...\n";
            }
            log.flush();
            Sleep(1500);
        }
    } else {
        log << "[!] FAILED: Pawn Pattern not found!\n";
        log.flush();
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RealPawnThread, NULL, 0, NULL));
    return TRUE;
}
