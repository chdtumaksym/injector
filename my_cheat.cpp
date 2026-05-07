#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

struct Vector3 { float x, y, z; };

// Функция поиска паттерна (сигнатуры)
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
    DWORD size = nt->OptionalHeader.SizeOfImage;
    auto patternBytes = b;
    BYTE* scanStart = (BYTE*)base;

    for (DWORD i = 0; i < size - patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != (BYTE)patternBytes[j]) {
                found = false;
                break;
            }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

DWORD WINAPI HunterThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    
    log << "--- AUTO-HUNTER START (Sig Scan) ---\n";
    log.flush();

    // Сигнатура для dwLocalPlayerPawn
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    
    if (lpAddr) {
        // Вычисляем адрес через смещение
        uintptr_t localPlayerPtr = lpAddr + 7 + *reinterpret_cast<int32_t*>(lpAddr + 3);
        log << "[+] Found Player Base Pointer at: " << std::hex << localPlayerPtr << "\n";
        log.flush();

        while (true) {
            uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(localPlayerPtr);
            if (localPlayer > 0x1000) {
                // Оффсет координат m_vOldOrigin
                Vector3 pos = *reinterpret_cast<Vector3*>(localPlayer + 0x127C);
                log << "POS -> X: " << pos.x << " Y: " << pos.y << " Z: " << pos.z << "\n";
            } else {
                log << "[-] Waiting for player pawn...\n";
            }
            log.flush();
            Sleep(2000);
        }
    } else {
        log << "[!] FAILED: Could not find LocalPlayer pattern!\n";
        log.flush();
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)HunterThread, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
