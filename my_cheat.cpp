#include <windows.h>
#include <fstream>

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        // Прямая попытка создать файл БЕЗ потоков
        std::ofstream f("INJECTOR_TEST.txt");
        f << "IF YOU SEE THIS - INJECTOR IS OK";
        f.close();
        
        // Попытка выкинуть сообщение
        MessageBoxA(NULL, "DLL INJECTED!", "SUCCESS", MB_OK);
    }
    return TRUE;
}
