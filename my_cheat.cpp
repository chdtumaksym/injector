#include <windows.h>
#include <fstream>
#include <string>

// Функция нашего чита, которая работает в отдельном потоке
DWORD WINAPI HackThread(LPVOID lpParam) {
    // 1. Пытаемся создать файл-подтверждение на диске C или в папке пользователя
    // Используем относительный путь, чтобы точно были права на запись
    std::ofstream out("Bypass_Success.txt");; 
    if (!out.is_open()) {
        // Если диск C закрыт, пробуем создать на рабочем столе (примерный путь)
        out.open("Bypass_Success_Alt.txt");
    }

    if (out.is_open()) {
        out << "========= INJECTION REPORT =========" << std::endl;
        out << "Status: SUCCESS" << std::endl;
        out << "Method: Manual Mapping + Thread Hijacking" << std::endl;
        out << "Result: Code executed inside target process." << std::endl;
        out.close();
    }

    // 2. Выводим окно. В отдельном потоке это не вызовет зависания (Deadlock)
    MessageBoxA(NULL, "Victory! DLL is fully functional inside the process.\nCheck C:\\Bypass_Success.txt", "Manual Map Success", MB_OK | MB_ICONINFORMATION);
    
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Создаем поток, чтобы основной поток игры/блокнота сразу вернулся к работе
        // Это обходит проблему краша стека и "мертвой петли", о которой ныл Гемини
        HANDLE hThread = CreateThread(NULL, 0, HackThread, NULL, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        }
    }
    return TRUE;
}
