#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <vector>

// Твои оффсеты из JSON (сверил, в HEX перевел верно)
namespace Offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720;
    constexpr uintptr_t dwEntityList = 0x24D1990;
    
    // Смещения из твоего списка (Schema)
    constexpr uintptr_t m_hPawn = 0x6BC;
    constexpr uintptr_t m_iTeamNum = 0x3EB;
    constexpr uintptr_t m_pGameSceneNode = 0x330;
    constexpr uintptr_t m_iHealth = 0x32C; 
}

template <typename T>
T Read(uintptr_t addr) {
    T val = T();
    if (addr < 0x10000 || addr > 0x7FFEFFFFFFFF) return val;
    __try { val = *(T*)addr; } __except (EXCEPTION_EXECUTE_HANDLER) { return T(); }
    return val;
}

struct Vector3 { float x, y, z; };

void AimLogic() {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) {
        printf("[!] client.dll NOT FOUND\n");
        return;
    }

    uintptr_t localPlayerPawn = Read<uintptr_t>(client + Offsets::dwLocalPlayerPawn);
    if (!localPlayerPawn) {
        printf("[!] LocalPlayerPawn is NULL. (Bad offset: 0x%X)\n", Offsets::dwLocalPlayerPawn);
        return; 
    }

    int myTeam = Read<int>(localPlayerPawn + Offsets::m_iTeamNum);
    uintptr_t entityList = Read<uintptr_t>(client + Offsets::dwEntityList);
    if (!entityList) {
        printf("[!] EntityList is NULL. (Bad offset: 0x%X)\n", Offsets::dwEntityList);
        return;
    }

    // Проверку на кнопку вырезал. Теперь сканирует всегда.
    bool found_anyone = false;

    for (int i = 1; i <= 64; i++) {
        uintptr_t listEntry = Read<uintptr_t>(entityList + 0x10);
        uintptr_t controller = Read<uintptr_t>(listEntry + i * 0x78);
        if (!controller) continue;

        uint32_t pawnHandle = Read<uint32_t>(controller + Offsets::m_hPawn);
        if (!pawnHandle) continue;

        // Поиск пешки по хэндлу в EntityList
        uintptr_t listEntry2 = Read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 0x10);
        uintptr_t enemyPawn = Read<uintptr_t>(listEntry2 + 0x78 * (pawnHandle & 0x1FF));
        
        if (!enemyPawn || enemyPawn == localPlayerPawn) continue;

        int enemyTeam = Read<int>(enemyPawn + Offsets::m_iTeamNum);
        int enemyHealth = Read<int>(enemyPawn + Offsets::m_iHealth);
        
        // Фильтр: только живые враги
        if (enemyTeam == myTeam || enemyHealth <= 0 || enemyHealth > 100) continue;

        uintptr_t gameSceneNode = Read<uintptr_t>(enemyPawn + Offsets::m_pGameSceneNode);
        if (!gameSceneNode) continue;

        // BoneArray - это место, где чаще всего ломается аим после патчей
        uintptr_t boneArray = Read<uintptr_t>(gameSceneNode + 0x1F0); 
        if (!boneArray) continue;

        // Кость головы (индекс 6)
        Vector3 headPos = Read<Vector3>(boneArray + (6 * 32));

        if (headPos.x != 0) {
            found_anyone = true;
            printf("[MATCH] Enemy ID: %d | HP: %d | HeadPos: %.1f, %.1f, %.1f\n", i, enemyHealth, headPos.x, headPos.y, headPos.z);
        }
    }
    
    if (!found_anyone) {
        printf("[-] No valid enemies found in this tick.\n");
    }
}

DWORD WINAPI HackThread(LPVOID lpParam) {
    // Создаем консоль, чтобы видеть printf!
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    
    printf("--- CS2 INTERNAL DEBUG STARTED ---\n");
    printf("Press END to unload\n\n");

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        AimLogic();
        Sleep(1000); // Задержка 1 секунда, чтобы не спамило миллион строк в миллисекунду
    }

    printf("Unloading...\n");
    if (f) fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread((HMODULE)lpParam, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        CloseHandle(CreateThread(0, 0, HackThread, h, 0, 0));
    }
    return TRUE;
}
