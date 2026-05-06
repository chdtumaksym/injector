#include <windows.h>

// Эта функция вызывается автоматически сразу после маппинга
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // Создаем отдельный поток для нашего кода, чтобы не вешать основной поток игры
        // Но для первого теста можно просто вывести MessageBox прямо здесь
        MessageBoxA(NULL, "Injection Successful!\nManual Mapping: OK\nThread Hijacking: OK", "Bypass Test", MB_OK | MB_ICONINFORMATION);
        break;

    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
