#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

// Вспомогательные структуры
struct Vector3 { float x, y, z; };

namespace offsets {
    // Используем твои цифры, но с жесткой проверкой
    constexpr uintptr_t dwLocalPlayerPawn = 0x1824E18; 
    constexpr uintptr_t m_iIDEntIndex = 0x1544; 
    constexpr uintptr_t m_iHealth = 0x334;
    constexpr uintptr_t m_iTeamNum = 0x3CB;
}

// Функция безопасного чтения (защита от вылета)
template <typename T>
T SafeRead(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFEFFFF) return T(); // Базовая проверка диапазона x64
    __try {
        return *reinterpret_cast<T*>(addr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return T();
    }
}

DWORD WINAPI UltraFinalThread(LPVOID lpParam) {
    std::ofstream log("CS2_Debug_Log.txt", std::ios::app);
    log << "--- SAFE SESSION STARTED ---\n";
    log.flush();

    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        // Читаем локального игрока через безопасную функцию
        uintptr_t localPlayer = SafeRead<uintptr_t>(client + offsets::dwLocalPlayerPawn);
        
        if (localPlayer) {
            // Читаем ID в прицеле
            int crossId = SafeRead<int>(localPlayer + offsets::m_iIDEntIndex);

            // Если кто-то в прицеле (ID от 1 до 64)
            if (crossId > 0 && crossId <= 64) {
                log << "Target detected! ID: " << crossId << "\n";
                log.flush();

                // ТРИГГЕРБОТ (Эмуляция клика)
                INPUT input[2] = { 0 };
                input[0].type = INPUT_MOUSE;
                input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                input[1].type = INPUT_MOUSE;
                input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                
                SendInput(2, input, sizeof(INPUT));
                Sleep(100); // Задержка между выстрелами
            }
        }
        Sleep(1); // Максимальная скорость
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        // Используем стандартный поток, так как мы добавили защиту __try/__except
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)UltraFinalThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
