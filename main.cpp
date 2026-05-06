#include <windows.h>
#include <iostream>
#include <vector>
#include <tlhelp32.h>
#include <fstream>

// Структура данных для передачи в игру
struct MANUAL_MAPPING_DATA {
    HMODULE(__stdcall* pLoadLibraryA)(LPCSTR);
    FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR);
    BYTE* pBase;
    ULONG_PTR pOldRip; // Оригинальный адрес для возврата потока
    bool bDone;        // Флаг завершения работы шелл-кода
};

// -------------------- SHELLCODE (x64) --------------------
// Важно: компилировать без /GS и в Release x64
void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData) {
    if (!pData || !pData->pBase) return;

    BYTE* base = pData->pBase;
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);

    // 1. Релокации (Base Relocations)
    auto delta = reinterpret_cast<ULONG_PTR>(base) - ntHeaders->OptionalHeader.ImageBase;
    if (delta) {
        auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(base + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
        while (reloc->VirtualAddress) {
            DWORD count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            WORD* data = reinterpret_cast<WORD*>(reloc + 1);
            for (DWORD i = 0; i < count; ++i) {
                if ((data[i] >> 12) == IMAGE_REL_BASED_DIR64) {
                    auto* patch = reinterpret_cast<ULONG_PTR*>(base + reloc->VirtualAddress + (data[i] & 0xFFF));
                    *patch += delta;
                }
            }
            reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(reloc) + reloc->SizeOfBlock);
        }
    }

    // 2. Импорты (Imports)
    if (ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
        auto* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        while (importDesc->Name) {
            HMODULE hModule = pData->pLoadLibraryA(reinterpret_cast<const char*>(base + importDesc->Name));
            auto* thunkOrig = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + importDesc->OriginalFirstThunk);
            auto* thunkIAT = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + importDesc->FirstThunk);
            while (thunkOrig->u1.AddressOfData) {
                if (thunkOrig->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                    thunkIAT->u1.Function = reinterpret_cast<ULONG_PTR>(pData->pGetProcAddress(hModule, reinterpret_cast<LPCSTR>(thunkOrig->u1.Ordinal & 0xFFFF)));
                } else {
                    auto* importName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + thunkOrig->u1.AddressOfData);
                    thunkIAT->u1.Function = reinterpret_cast<ULONG_PTR>(pData->pGetProcAddress(hModule, importName->Name));
                }
                thunkOrig++; thunkIAT++;
            }
            importDesc++;
        }
    }

    // 3. Вызов DllMain
    if (ntHeaders->OptionalHeader.AddressOfEntryPoint) {
        auto DllMain = reinterpret_cast<BOOL(APIENTRY*)(HMODULE, DWORD, LPVOID)>(base + ntHeaders->OptionalHeader.AddressOfEntryPoint);
        DllMain(reinterpret_cast<HMODULE>(base), DLL_PROCESS_ATTACH, nullptr);
    }

    // 4. Скрытие (Затираем PE заголовки)
    for (DWORD i = 0; i < ntHeaders->OptionalHeader.SizeOfHeaders; ++i) base[i] = 0;

    // 5. Сигнал инжектору и возврат
    pData->bDone = true;
    auto pReturn = reinterpret_cast<void(*)()>(pData->pOldRip);
    pReturn(); 
}

// Пустышка для вычисления размера Shellcode
void __stdcall ShellcodeEnd() {}

// -------------------- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ --------------------

DWORD GetTargetThreadID(DWORD pid) {
    THREADENTRY32 te32 = { sizeof(te32) };
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (Thread32First(hSnap, &te32)) {
        do {
            if (te32.th32OwnerProcessID == pid) {
                CloseHandle(hSnap);
                return te32.th32ThreadID;
            }
        } while (Thread32Next(hSnap, &te32));
    }
    CloseHandle(hSnap);
    return 0;
}

void SetSectionProtections(HANDLE hProcess, LPVOID remoteBase, IMAGE_NT_HEADERS* ntHeaders) {
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        DWORD oldProtect;
        VirtualProtectEx(hProcess, (BYTE*)remoteBase + section[i].VirtualAddress, section[i].Misc.VirtualSize, PAGE_EXECUTE_READ, &oldProtect);
    }
}

// -------------------- ОСНОВНОЙ ЛОАДЕР --------------------

bool ManualMapDLL(HANDLE hProcess, const std::vector<char>& buffer) {
    auto* dosHeader = (IMAGE_DOS_HEADER*)buffer.data();
    auto* ntHeaders = (IMAGE_NT_HEADERS*)(buffer.data() + dosHeader->e_lfanew);

    // Выделяем память под DLL
    LPVOID remoteBase = VirtualAllocEx(hProcess, nullptr, ntHeaders->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    
    // Копируем секции
    WriteProcessMemory(hProcess, remoteBase, buffer.data(), ntHeaders->OptionalHeader.SizeOfHeaders, nullptr);
    auto* section = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        WriteProcessMemory(hProcess, (BYTE*)remoteBase + section[i].VirtualAddress, buffer.data() + section[i].PointerToRawData, section[i].SizeOfRawData, nullptr);
    }

    // Готовим данные
    MANUAL_MAPPING_DATA data = { 0 };
    data.pLoadLibraryA = LoadLibraryA;
    data.pGetProcAddress = GetProcAddress;
    data.pBase = (BYTE*)remoteBase;
    data.bDone = false;

    // Выделяем память под Shellcode
    SIZE_T shellSize = (BYTE*)ShellcodeEnd - (BYTE*)Shellcode;
    LPVOID remoteShell = VirtualAllocEx(hProcess, nullptr, shellSize + sizeof(data), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    LPVOID remoteStruct = (BYTE*)remoteShell + shellSize;

    // Угон потока (Thread Hijacking)
    DWORD tid = GetTargetThreadID(GetProcessId(hProcess));
    HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
    SuspendThread(hThread);

    CONTEXT ctx = { CONTEXT_CONTROL };
    GetThreadContext(hThread, &ctx);
    data.pOldRip = ctx.Rip; // Сохраняем оригинал

    WriteProcessMemory(hProcess, remoteStruct, &data, sizeof(data), nullptr);
    WriteProcessMemory(hProcess, remoteShell, Shellcode, shellSize, nullptr);

    ctx.Rip = (ULONG_PTR)remoteShell;
    SetThreadContext(hThread, &ctx);
    ResumeThread(hThread);

    // Профессиональная синхронизация (Ждем флаг bDone)
    for (int i = 0; i < 2000; i++) {
        MANUAL_MAPPING_DATA check;
        ReadProcessMemory(hProcess, remoteStruct, &check, sizeof(check), nullptr);
        if (check.bDone) break;
        Sleep(10);
    }

    SetSectionProtections(hProcess, remoteBase, ntHeaders);
    // Чистим шелл-код
    VirtualFreeEx(hProcess, remoteShell, 0, MEM_RELEASE);
    CloseHandle(hThread);
    return true;
}

int main() {
    DWORD pid;
    std::cout << "Target PID: "; std::cin >> pid;
    
    std::ifstream file("my_cheat.dll", std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (ManualMapDLL(hProcess, buffer)) std::cout << "DONE!";
    
    CloseHandle(hProcess);
    return 0;
}
