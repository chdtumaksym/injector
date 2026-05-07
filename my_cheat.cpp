#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <vector>

// Теперь здесь ЧИСТЫЕ десятичные цифры (Decimal) прямо из твоего JSON! 
// Больше никаких ошибок с кривым переводом в HEX.
namespace Offsets {
    // Из offsets.json -> "client.dll"
    constexpr uintptr_t dwLocalPlayerPawn = 33912608; 
    constexpr uintptr_t dwEntityList = 38608368;       
    
    // Из client_dll.json
    constexpr uintptr_t m_hPawn = 1724;        // CBasePlayerController -> m_hPawn
    constexpr uintptr_t m_iTeamNum = 1003;     // C_BaseEntity -> m_iTeamNum
    constexpr uintptr_t m_pGameSceneNode = 816; // C_BaseEntity -> m_pGameSceneNode
    constexpr uintptr_t m_iHealth = 844;       // C_BaseEntity -> m_iHealth
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
    if (!client) return;

    // Читаем локального игрока
    uintptr_t localPlayerPawn = Read<uintptr_t>(client + Offsets::dwLocalPlayerPawn);
    if (!localPlayerPawn) return; 

    // Читаем список сущностей
    uintptr_t entityList = Read<uintptr_t>(client + Offsets::dwEntityList);
    if (!entityList) {
        // Если тут NULL, значит оффсет всё еще кривой, но теперь это точная цифра из файла
        static bool error_logged = false;
        if (!error_logged) { printf("[!] EntityList is NULL. Check decimal offset: %llu\n", Offsets::dwEntityList); error_logged = true; }
        return;
    }

    int myTeam = Read<int>(localPlayerPawn + Offsets::m_iTeamNum);
    bool found_at_least_one = false;

    // Проходим по списку контроллеров игроков
    for (int i = 1; i <= 64; i++) {
        uintptr_t listEntry = Read<uintptr_t>(entityList + 0x10);
        uintptr_t controller = Read<uintptr_t>(listEntry + i * 0x78);
        if (!controller) continue;

        // Получаем хэндл пешки
        uint32_t pawnHandle = Read<uint32_t>(controller + Offsets::m_hPawn);
        if (!pawnHandle) continue;

        // Находим саму пешку (Pawn) через хэндл
        uintptr_t listEntry2 = Read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 0x10);
        uintptr_t enemyPawn = Read<uintptr_t>(listEntry2 + 0x78 * (pawnHandle & 0x1FF));
        
        if (!enemyPawn || enemyPawn == localPlayerPawn) continue;

        // Проверка команды и здоровья
        int enemyTeam = Read<int>(enemyPawn + Offsets::m_iTeamNum);
        int enemyHealth = Read<int>(enemyPawn + Offsets::m_iHealth);
        
        if (enemyTeam == myTeam || enemyHealth <= 0 || enemyHealth > 100) continue;

        // Получаем узел сцены для доступа к костям
        uintptr_t gameSceneNode = Read<uintptr_t>(enemyPawn + Offsets::m_pGameSceneNode);
        if (!gameSceneNode) continue;

        // BoneArray (0x1F0) — это стандарт для текущего билда, обычно он не меняется
        uintptr_t boneArray = Read<uintptr_t>(gameSceneNode + 0x1F0); 
        if (!boneArray) continue;

        // Читаем координаты головы (кость №6)
        Vector3 headPos = Read<Vector3>(boneArray + (6 * 32));

        if (headPos.x != 0) {
            found_at_least_one = true;
            printf("[BINGO] Враг %d | HP: %d | Head: X:%.1f, Y:%.1f, Z:%.1f\n", i, enemyHealth, headPos.x, headPos.y, headPos.z);
        }
    }
    
    if (!found_at_least_one) {
        // Если тишина — либо врагов нет, либо оффсеты всё еще корявые
    }
}

DWORD WINAPI HackThread(LPVOID lpParam) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    
    printf("--- CS2 DEBUG V118 (DECIMAL OFFSETS) ---\n");
    printf("Локальный Pawn: %llu\n", Offsets::dwLocalPlayerPawn);
    printf("Список сущностей: %llu\n", Offsets::dwEntityList);

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        AimLogic();
        Sleep(1000); 
    }

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
