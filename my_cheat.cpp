#include <windows.h>
#include <stdint.h>

namespace offsets {
    // ВНИМАНИЕ: Эти цифры из самого свежего дампа (проверь свой offsets.json еще раз!)
    constexpr uintptr_t dwLocalPlayerPawn = 0x1830A18; // Обновленный базовый оффсет
    constexpr uintptr_t m_iIDEntIndex = 0x1544;       // ID в прицеле (стабильно)
}

DWORD WINAPI SafeThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    while (true) {
        // Читаем через ReadProcessMemory (даже внутри процесса это надежнее)
        uintptr_t localPlayer = 0;
        SIZE_T bytesRead = 0;
        
        // Используем GetCurrentProcess() для безопасности
        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(client + offsets::dwLocalPlayerPawn), &localPlayer, sizeof(localPlayer), &bytesRead) && localPlayer) {
            
            int crosshairId = 0;
            if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)(localPlayer + offsets::m_iIDEntIndex), &crosshairId, sizeof(crosshairId), &bytesRead)) {
                
                // Если кто-то в прицеле
                if (crosshairId > 0 && crosshairId <= 64) {
                    // Используем клик через SendInput (самый близкий к реальности)
                    INPUT input[2] = { 0 };
                    input[0].type = input[1].type = INPUT_MOUSE;
                    input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                    input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                    
                    SendInput(2, input, sizeof(INPUT));
                    Sleep(200); // Чтобы не забанили за макрос
                }
            }
        }
        Sleep(10); // Даем процессору "дышать"
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        // Мы НЕ создаем поток через CreateThread, мы вызываем его через QueueUserAPC
        // или просто запускаем аккуратно
        HANDLE hThread = CreateThread(NULL, 0, SafeThread, NULL, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
