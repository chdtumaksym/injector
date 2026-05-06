#include <windows.h>

// Точка входа в DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    // Выполняем действие только в момент присоединения к процессу
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        
        // Самый наглядный тест: приказываем процессу самоуничтожиться.
        // Если Блокнот закроется — инъекция и выполнение кода прошли успешно.
        TerminateProcess(GetCurrentProcess(), 0);
        
    }
    return TRUE;
}
