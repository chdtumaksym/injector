#include <windows.h>
#include <fstream>

// Функция, которая будет работать в отдельном потоке
DWORD WINAPI HackThread(LPVOID lpParam) {
    // Пробуем создать файл на диске C (или смени на D:\success.txt, если С закрыт)
    std::ofstream out("C:\\bypass_test.txt");
    if (out.is_open()) {
        out << "Manual Mapping + Thread Hijacking: SUCCESS!";
        out.close();
    }
    
    // Пытаемся все же показать окно, но уже из отдельного потока
    MessageBoxA(NULL, "DLL is running inside target process!", "Victory", MB_OK);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // Создаем поток, чтобы не вешать игру/блокнот
        CreateThread(NULL, 0, HackThread, NULL, 0, NULL);
    }
    return TRUE;
}
