#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <vector>

// Твои новые оффсеты из JSON (статические адреса)
namespace Offsets {
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720;
    constexpr uintptr_t dwEntityList = 0x24D1990;
    
    // Оффсеты внутри классов (Schema)
    constexpr uintptr_t m_hPawn = 0x6BC;
    constexpr uintptr_t m_iTeamNum = 0x3EB;
    constexpr uintptr_t m_pGameSceneNode = 0x330;
    constexpr uintptr_t m_iHealth = 0x32C; // Обычно тут, если нет в твоем списке
}

// Упрощенное чтение
template <typename T>
T Read(uintptr_t addr) {
    T val = T();
    if (addr < 0x10000) return val;
    __try { val = *(T*)addr; } __except (1) {}
    return val;
}

// Структура вектора для координат
struct Vector3 { float x, y, z; };

void AimLogic() {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return;

    // 1. Находим себя
    uintptr_t localPlayerPawn = Read<uintptr_t>(client + Offsets::dwLocalPlayerPawn);
    if (!localPlayerPawn) return;

    int myTeam = Read<int>(localPlayerPawn + Offsets::m_iTeamNum);

    // 2. Идем в список сущностей
    uintptr_t entityList = Read<uintptr_t>(client + Offsets::dwEntityList);
    if (!entityList) return;

    // 3. Цикл по игрокам (первые 64 слота)
    for (int i = 1; i <= 64; i++) {
        // Получаем контроллер (логическая сущность игрока)
        uintptr_t listEntry = Read<uintptr_t>(entityList + 0x10);
        uintptr_t controller = Read<uintptr_t>(listEntry + i * 0x78);
        if (!controller) continue;

        // Берем хэндл на пешку (физическое тело)
        uint32_t pawnHandle = Read<uint32_t>(controller + Offsets::m_hPawn);
        if (!pawnHandle) continue;

        // Вычисляем адрес самой пешки в памяти (магия Source 2)
        uintptr_t listEntry2 = Read<uintptr_t>(entityList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 0x10);
        uintptr_t enemyPawn = Read<uintptr_t>(listEntry2 + 0x78 * (pawnHandle & 0x1FF));
        
        if (!enemyPawn || enemyPawn == localPlayerPawn) continue;

        // Проверка на команду и здоровье
        int enemyTeam = Read<int>(enemyPawn + Offsets::m_iTeamNum);
        int enemyHealth = Read<int>(enemyPawn + Offsets::m_iHealth);
        
        if (enemyTeam == myTeam || enemyHealth <= 0) continue;

        // 4. ДОСТАЕМ КООРДИНАТЫ ДЛЯ АИМА (Bone Matrix)
        uintptr_t gameSceneNode = Read<uintptr_t>(enemyPawn + Offsets::m_pGameSceneNode);
        
        // У костей в CS2 есть свой указатель внутри GameSceneNode
        // 0x1F0 - это стандартный оффсет для BoneArray внутри CGameSceneNode
        uintptr_t boneArray = Read<uintptr_t>(gameSceneNode + 0x1F0); 
        
        // 6 - это индекс кости головы (Head) для большинства моделей
        // Каждая кость занимает 32 байта (структура трансформы)
        Vector3 headPos = Read<Vector3>(boneArray + (6 * 32));

        // Теперь в headPos лежат X, Y, Z координаты головы врага в 3D мире.
        // Дальше тебе нужно просто вычислить углы и записать их в dwViewAngles.
        printf("Target %d Head Pos: X:%.2f Y:%.2f Z:%.2f\n", i, headPos.x, headPos.y, headPos.z);
    }
}
