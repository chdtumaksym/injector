#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace Offsets {
    // Из offsets.json -> "client.dll"
    constexpr uintptr_t dwLocalPlayerPawn = 33912608; 
    constexpr uintptr_t dwEntityList = 38608368;       
    
    // Из client_dll.json
    constexpr uintptr_t m_hPawn = 1724;        
    constexpr uintptr_t m_iTeamNum = 1003;     
    constexpr uintptr_t m_pGameSceneNode = 816; 
    constexpr uintptr_t m_iHealth = 844;       
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

    // Читаем локального игрока и список энтити
    uintptr_t localPlayerPawn = Read<uintptr_t>(client + Offsets::dwLocalPlayerPawn);
    uintptr_t entityList = Read<uintptr_t>(client + Offsets::dwEntityList);

    if (!localPlayerPawn || !entityList) {
        printf("[!] Ожидание загрузки карты... Local: %llu | EntList: %llu\n", localPlayerPawn, entityList);
        return;
    }

    int myTeam = Read<int>(localPlayerPawn + Offsets::m_iTeamNum);
    
    int valid_controllers = 0;
    int valid_pawns = 0;
    int alive_players = 0;

    printf("\n--- СКАН (Моя команда: %d) ---\n", myTeam);

    for (int i = 1; i <= 64; i++) {
        uintptr_t listEntry = Read<uintptr_t>(entityList + 0x10);
        uintptr_t controller = Read<uintptr_t>(listEntry + i * 0x78);
        if (!controller) continue;
        valid_controllers++;

        uint32_t pawnHandle = Read<uint32_t>(controller + Offsets::m_hPawn);
        if (!pawnHandle) continue;

        uintptr_t listEntry2 = Read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 0x10);
        uintptr_t enemyPawn = Read<uintptr_t>(listEntry2 + 0x78 * (pawnHandle & 0x1FF));
        
        if (!enemyPawn || enemyPawn == localPlayerPawn) continue;
        valid_pawns++;

        int enemyTeam = Read<int>(enemyPawn + Offsets::m_iTeamNum);
        int enemyHealth = Read<int>(enemyPawn + Offsets::m_iHealth);
        
        if (enemyHealth <= 0) continue;
        alive_players++;

        uintptr_t gameSceneNode = Read<uintptr_t>(enemyPawn + Offsets::m_pGameSceneNode);
        
        // Ищем массив костей по разным возможным смещениям (на случай если 0x1F0 отвалился в обнове)
        uintptr_t boneArray = Read<uintptr_t>(gameSceneNode + 0x1F0); 
        if (!boneArray) boneArray = Read<uintptr_t>(gameSceneNode + 0x1E0);
        if (!boneArray) boneArray = Read<uintptr_t>(gameSceneNode + 0x1D0);
        if (!boneArray) boneArray = Read<uintptr_t>(gameSceneNode + 0x160);

        if (boneArray) {
            Vector3 headPos = Read<Vector3>(boneArray + (6 * 32));
            printf("[+] Игрок %d | Команда: %d | HP: %d | HeadZ: %.1f\n", i, enemyTeam, enemyHealth, headPos.z);
        } else {
            printf("[-] Игрок %d | Команда: %d | HP: %d | ОШИБКА: Нет BoneArray!\n", i, enemyTeam, enemyHealth);
        }
    }
    
    printf("Итого: Контроллеров: %d | Пешек: %d | Живых: %d\n", valid_controllers, valid_pawns, alive_players);
}

DWORD WINAPI HackThread(LPVOID lpParam) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    
    printf("--- CS2 DIAGNOSTICS V119 ---\n");

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        AimLogic();
        Sleep(3000); // Скан каждые 3 секунды, чтобы лог был читаемым и не улетал в космос
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
