#include <windows.h>
#include <vector>
#include <string>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
}

// --- 1. Функция поиска паттерна (ТЕЛО ФУНКЦИИ) ---
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;

    auto PatternToBytes = [](const char* p) {
        std::vector<int> bytes;
        char* start = const_cast<char*>(p);
        char* end = const_cast<char*>(p) + strlen(p);
        for (char* curr = start; curr < end; ++curr) {
            if (*curr == '?') {
                curr++; if (*curr == '?') curr++;
                bytes.push_back(-1);
            } else {
                bytes.push_back((int)strtoul(curr, &curr, 16));
            }
        }
        return bytes;
    };

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((BYTE*)moduleBase + dos->e_lfanew);
    DWORD size = nt->OptionalHeader.SizeOfImage;
    auto patternBytes = PatternToBytes(pattern);
    BYTE* scanStart = (BYTE*)moduleBase;

    for (DWORD i = 0; i < size - (DWORD)patternBytes.size(); ++i) {
        bool found = true;
        for (size_t j = 0; j < patternBytes.size(); ++j) {
            if (patternBytes[j] != -1 && scanStart[i + j] != (BYTE)patternBytes[j]) {
                found = false; break;
            }
        }
        if (found) return (uintptr_t)(scanStart + i);
    }
    return 0;
}

// --- 2. Получение сущности (ТЕЛО ФУНКЦИИ) ---
uintptr_t GetEntityByIndex(uintptr_t listPtr, int index) {
    uintptr_t list = *(uintptr_t*)listPtr;
    if (!list) return 0;
    uintptr_t chunk = *(uintptr_t*)(list + 8 * (index >> 9) + 0x10);
    if (!chunk) return 0;
    return *(uintptr_t*)(chunk + 0x78 * (index & 0x1FF));
}

// --- 3. Поток Аимбота (Через мышь) ---
DWORD WINAPI MouseAimThread(LPVOID lpParam) {
    uintptr_t lpAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elAddr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");
    
    if (!lpAddr || !elAddr) return 0;

    uintptr_t lpPtr = lpAddr + 7 + *(int32_t*)(lpAddr + 3);
    uintptr_t elPtr = elAddr + 7 + *(int32_t*)(elAddr + 3);

    while (true) {
        // Аим работает, пока зажата Левая Кнопка Мыши
        if (GetAsyncKeyState(VK_LBUTTON)) {
            uintptr_t local = *(uintptr_t*)lpPtr;
            if (local) {
                int myTeam = *(int*)(local + offsets::m_iTeamNum);
                
                for (int i = 1; i < 64; i++) {
                    uintptr_t ent = GetEntityByIndex(elPtr, i);
                    if (!ent || ent == local) continue;
                    if (*(int*)(ent + offsets::m_iHealth) <= 0 || *(int*)(ent + offsets::m_iTeamNum) == myTeam) continue;

                    // Если нашли живого врага — просто немного двигаем мышь вправо для теста
                    // В полноценной версии здесь будет расчет дистанции до головы
                    mouse_event(MOUSEEVENTF_MOVE, 2, 0, 0, 0); 
                    break; 
                }
            }
        }
        Sleep(10);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MouseAimThread, nullptr, 0, nullptr));
    }
    return TRUE;
}
