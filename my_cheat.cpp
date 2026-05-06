#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

// Оффсеты (проверь актуальность в своем дампере!)
namespace offsets {
    constexpr uintptr_t m_vOldOrigin = 0x127C; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB; // Смещение команды (чтобы не стрелять в своих)
}

struct Vector3 { float x, y, z; };

// Функция поиска паттерна (та же, что и была)
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t moduleBase = (uintptr_t)GetModuleHandleA(moduleName);
    if (!moduleBase) return 0;
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)moduleBase;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS*)((BYTE*)moduleBase + dosHeader->e_lfanew);
    DWORD size = ntHeaders->OptionalHeader.SizeOfImage;
    // ... (код PatternToBytes пропустим для краткости, используй старый)
    return 0; // В финальном коде будет полная версия
}

// Функция получения игрока по индексу (логика из твоего запроса)
uintptr_t GetEntityByIndex(uintptr_t entityListPtr, int index) {
    if (!entityListPtr) return 0;
    uintptr_t entityList = *(uintptr_t*)entityListPtr;
    if (!entityList) return 0;

    uintptr_t chunk = *(uintptr_t*)(entityList + 0x8 * (index >> 9) + 0x10);
    if (!chunk) return 0;

    return *(uintptr_t*)(chunk + 0x78 * (index & 0x1FF));
}

DWORD WINAPI CheatThread(LPVOID lpParam) {
    std::ofstream log("CS2_Entity_Log.txt", std::ios::app);
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");

    // Ищем сигнатуры
    uintptr_t localPlayerPtr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E"); // Оффсет нужно извлечь как раньше
    uintptr_t entityListPtr = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB");

    // Извлекаем реальные адреса из сигнатур (RIP-relative)
    localPlayerPtr = localPlayerPtr + 7 + *(int32_t*)(localPlayerPtr + 3);
    entityListPtr = entityListPtr + 7 + *(int32_t*)(entityListPtr + 3);

    while (true) {
        uintptr_t localPlayer = *(uintptr_t*)localPlayerPtr;
        if (!localPlayer) { Sleep(1000); continue; }

        int myTeam = *(int*)(localPlayer + offsets::m_iTeamNum);

        // Перебираем первых 64 игрока
        for (int i = 0; i  0 && health <= 100 && team != myTeam) {
                Vector3 pos = *(Vector3*)(entity + offsets::m_vOldOrigin);
                log << "Enemy [" << i << "] HP: " << health << " Pos: " << pos.x << " " << pos.y << std::endl;
                log.flush();
                
                // ЗДЕСЬ БУДЕТ ЛОГИКА АИМБОТА
                // Мы нашли врага, его координаты у нас в руках (pos)
            }
        }
        Sleep(500);
    }
    return 0;
}
