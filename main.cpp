#include <windows.h>
#include <iostream>
#include <vector>
#include <fstream>

struct MANUAL_MAPPING_DATA {
    HMODULE(__stdcall* pLoadLibraryA)(LPCSTR);
    FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR);
    BYTE* pBase;
};

// -------------------- SHELLCODE (x64) --------------------
void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData) {
    if (!pData || !pData->pBase) return;

    BYTE* base = pData->pBase;
    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);

    // 1. Релокации
    auto delta = reinterpret_cast<ULONG_PTR>(base) - ntHeaders->OptionalHeader.ImageBase;
    if (delta && ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
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

    // 2. Импорты
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

    // 3. Точка входа
    if (ntHeaders->OptionalHeader.AddressOfEntryPoint) {
        auto DllMain = reinterpret_cast<BOOL(APIENTRY*)(HMODULE, DWORD, LPVOID)>(base + ntHeaders->OptionalHeader.AddressOfEntryPoint);
        DllMain(reinterpret_cast<HMODULE>(base), DLL_PROCESS_ATTACH, nullptr);
    }

    // 4. Зачистка
    for (DWORD i = 0; i < ntHeaders->OptionalHeader.SizeOfHeaders; ++i) base[i] = 0;
}

// -------------------- ЛОАДЕР --------------------

bool ManualMapDLL(HANDLE hProcess, const std::vector<char>& buffer) {
    auto* dosHeader = (IMAGE_DOS_HEADER*)buffer.data();
    auto* ntHeaders = (IMAGE_NT_HEADERS*)(buffer.data() + dosHeader->e_lfanew);

    // Выделяем память под DLL (RWX - для теста самое надежное)
    LPVOID remoteBase = VirtualAllocEx(hProcess, nullptr, ntHeaders->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) return false;

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

    SIZE_T shellSize = 4096; 
    LPVOID remoteShell = VirtualAllocEx(hProcess, nullptr, shellSize + sizeof(data), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    LPVOID remoteStruct = (BYTE*)remoteShell + shellSize;

    WriteProcessMemory(hProcess, remoteStruct, &data, sizeof(data), nullptr);
    WriteProcessMemory(hProcess, remoteShell, Shellcode, shellSize, nullptr);

    // Запуск через прямой поток (Самый надежный способ)
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)remoteShell, remoteStruct, 0, nullptr);
    
    if (hThread) {
        std::cout << "[+] SUCCESS! Thread created.\n";
        WaitForSingleObject(hThread, 5000);
        CloseHandle(hThread);
        return true;
    }

    return false;
}

int main() {
    DWORD pid;
    std::cout << "--- FINAL TEST ---\nTarget PID: "; std::cin >> pid;
    
    std::ifstream file("my_cheat.dll", std::ios::binary | std::ios::ate);
    if (!file.is_open()) { std::cout << "DLL NOT FOUND!"; Sleep(3000); return 1; }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);

    // Просим права на запись и создание потока
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    
    if (hProcess && ManualMapDLL(hProcess, buffer)) {
        std::cout << "[+] INJECTION FINISHED!\n";
    } else {
        std::cout << "[!] FATAL ERROR: Check Admin rights or PID.\n";
    }
    
    if (hProcess) CloseHandle(hProcess);
    Sleep(5000);
    return 0;
}
