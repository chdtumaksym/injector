#include <windows.h>
#include <vector>
#include <math.h>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    // Тот самый адрес из твоего скриншота (в HEX)
    constexpr uintptr_t dwViewAngles = 0x11F012C; 
}

// Функция поиска паттерна (необходима для работы структуры лоадера)
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)moduleBase + dos->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
    auto PatternToBytes = [](const char* p) {
        std::vector<int> bytes;
        char* start = const_cast<char*>(p);
        char* end = const_cast<char*>(p) + strlen(p);
        for (char* curr = start; curr < end; ++curr) {
            if (*curr == '?') { curr++; if (*curr == '?') curr++; bytes.push_back(-1); }
            else { bytes.push_back((int)strtoul(curr, &curr, 16)); }
        }
        return bytes;
    };
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = (BYTE*)moduleBase;
    for (DWORD i = 0; i < size - (DWORD)patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != (BYTE)patternBytes[j]) { found = false; break; }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

DWORD WINAPI TestThread(LPVOID lpParam) {
    // В CS2 углы обзора обычно лежат в client.dll
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    uintptr_t viewAnglesAddr = client + offsets::dwViewAngles;

    while (true) {
        // Если зажата левая кнопка мыши
        if (GetAsyncKeyState(VK_LBUTTON)) {
            static float testYaw = 0;
            testYaw += 2.0f; // Скорость вращения
            if (testYaw > 180.0f) testYaw = -180.0f;

            // Пробуем записать значение Yaw (горизонтальный угол)
            // В CS2 углы это: Pitch (0), Yaw (4), Roll (8)
            float* pYaw = (float*)(viewAnglesAddr + 4);
            *pYaw = testYaw;
        }
        Sleep(10); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(0, 0, TestThread, 0, 0, 0);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
