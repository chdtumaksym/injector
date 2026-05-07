#include <windows.h>
#include <stdint.h>

namespace offsets {
    // ВЗЯТО НАПРЯМУЮ ИЗ ТВОЕГО JSON:
    constexpr uintptr_t dwLocalPlayerPawn = 0x2057720; 
    constexpr uintptr_t m_iIDEntIndex = 0x1544; // ВОТ ОН, ПРАВИЛЬНЫЙ!
}

DWORD WINAPI RealTriggerThread(LPVOID lpParam) {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return 0;

    // Ждем 5 секунд, пока всё прогрузится
    Sleep(5000);

    while (true) {
        uintptr_t localPlayer = *reinterpret_cast<uintptr_t*>(client + offsets::dwLocalPlayerPawn);
        
        if (localPlayer > 0x1000) {
            int crosshairId = *reinterpret_cast<int*>(localPlayer + offsets::m_iIDEntIndex);

            // Если ID врага (1-64)
            if (crosshairId > 0 && crosshairId <= 64) {
                // Вместо "форс атаки" делаем классический клик, 
                // но максимально четко для Windows
                INPUT input = { 0 };
                input.type = INPUT_MOUSE;
                input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                SendInput(1, &input, sizeof(INPUT));
                
                Sleep(10); // Микро-задержка для Sub-Tick
                
                input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                SendInput(1, &input, sizeof(INPUT));
                
                // Даем игре время переварить выстрел
                Sleep(200); 
            }
        }
        // Не спим слишком долго, чтобы не пропустить врага
        Sleep(1); 
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        // Запускаем поток, но минимизируем его присутствие
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)RealTriggerThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
