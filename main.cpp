#include <windows.h>
#include <vector>
#include <fstream>
#include <iostream>

// -------------------- MANUAL MAPPING DATA --------------------
struct MANUAL_MAPPING_DATA {
    HMODULE(__stdcall* pLoadLibraryA)(LPCSTR);
    FARPROC(__stdcall* pGetProcAddress)(HMODULE, LPCSTR);
    LPVOID pBase;
};

// -------------------- SHELLCODE --------------------
extern "C" void __stdcall Shellcode(MANUAL_MAPPING_DATA* pData);
extern "C" void ShellcodeEnd(); // Пустая функция как метка конца

__stdcall void Shellcode(MANUAL_MAPPING_DATA* pData) {
    if (!pData || !pData->pBase) return;

    BYTE* base = static_cast<BYTE*>(pData->pBase);
    IMAGE_DOS_HEADER* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    IMAGE_NT_HEADERS* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);

    // ---------------- RELocations ----------------
    ULONGLONG delta = (ULONGLONG)base - ntHeaders->OptionalHeader.ImageBase;
    if (delta && ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size) {
        IMAGE_BASE_RELOCATION* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            base + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
        DWORD maxSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
        DWORD processed = 0;

        while (processed < maxSize) {
            DWORD pageVA = reloc->VirtualAddress;
            DWORD blockSize = reloc->SizeOfBlock;
            WORD* relocData = reinterpret_cast<WORD*>(reloc + 1);
            DWORD count = (blockSize - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);

            for (DWORD i = 0; i < count; ++i) {
                WORD typeOffset = relocData[i];
                if ((typeOffset >> 12) == IMAGE_REL_BASED_DIR64) {
                    ULONGLONG* patchAddr = reinterpret_cast<ULONGLONG*>(base + pageVA + (typeOffset & 0x0FFF));
                    *patchAddr += delta;
                }
            }
            processed += blockSize;
            reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(reloc) + blockSize);
        }
    }

    // ---------------- IMPORTS ----------------
    if (ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size) {
        IMAGE_IMPORT_DESCRIPTOR* importDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            base + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        while (importDesc->Name) {
            const char* dllName = reinterpret_cast<const char*>(base + importDesc->Name);
            HMODULE hModule = pData->pLoadLibraryA(dllName);

            IMAGE_THUNK_DATA64* thunkOrig = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + importDesc->OriginalFirstThunk);
            IMAGE_THUNK_DATA64* thunkIAT  = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + importDesc->FirstThunk);

            while (thunkOrig->u1.AddressOfData) {
                FARPROC func = nullptr;
                if (thunkOrig->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                    func = pData->pGetProcAddress(hModule, (LPCSTR)(thunkOrig->u1.Ordinal & 0xFFFF));
                } else {
                    IMAGE_IMPORT_BY_NAME* importName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + thunkOrig->u1.AddressOfData);
                    func = pData->pGetProcAddress(hModule, importName->Name);
                }
                thunkIAT->u1.Function = (ULONGLONG)func;
                ++thunkOrig;
                ++thunkIAT;
            }
            ++importDesc;
        }
    }

    // ---------------- DLLMAIN ----------------
    using DllMain_t = BOOL(__stdcall*)(HINSTANCE, DWORD, LPVOID);
    DllMain_t DllMainFunc = reinterpret_cast<DllMain_t>(
        base + ntHeaders->OptionalHeader.AddressOfEntryPoint);
    if (DllMainFunc) DllMainFunc((HINSTANCE)base, DLL_PROCESS_ATTACH, nullptr);

    // ---------------- Zero headers ----------------
    ULONGLONG sizeHeaders = ntHeaders->OptionalHeader.SizeOfHeaders;
    for (ULONGLONG i = 0; i < sizeHeaders; ++i) base[i] = 0x00;
}

void __stdcall ShellcodeEnd() {} // Пустая функция как метка конца

// -------------------- SET SECTION PROTECTIONS --------------------
void SetSectionProtections(HANDLE hProcess, LPVOID remoteBase, IMAGE_NT_HEADERS* ntHeaders) {
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeaders);

    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        LPVOID sectionAddr = (BYTE*)remoteBase + section[i].VirtualAddress;
        SIZE_T sectionSize = section[i].Misc.VirtualSize;
        DWORD oldProtect = 0;
        DWORD newProtect = 0;

        if (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            if (section[i].Characteristics & IMAGE_SCN_MEM_READ) {
                newProtect = (section[i].Characteristics & IMAGE_SCN_MEM_WRITE) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
            } else {
                newProtect = PAGE_EXECUTE;
            }
        } else {
            if (section[i].Characteristics & IMAGE_SCN_MEM_READ) {
                newProtect = (section[i].Characteristics & IMAGE_SCN_MEM_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
            } else if (section[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
                newProtect = PAGE_WRITECOPY;
            } else {
                newProtect = PAGE_NOACCESS;
            }
        }

        VirtualProtectEx(hProcess, sectionAddr, sectionSize, newProtect, &oldProtect);
    }
}

// -------------------- MANUAL MAP DLL --------------------
bool ManualMapDLL(HANDLE hProcess, const std::vector<char>& dllBuffer) {
    if (!hProcess) return false;

    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)dllBuffer.data();
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)(dllBuffer.data() + dosHeader->e_lfanew);
    SIZE_T imageSize = ntHeaders->OptionalHeader.SizeOfImage;

    // 1. Выделяем память под DLL (READWRITE)
    LPVOID remoteBase = VirtualAllocEx(hProcess, nullptr, imageSize,
                                       MEM_RESERVE | MEM_COMMIT,
                                       PAGE_READWRITE);
    if (!remoteBase) return false;

    // 2. Копируем заголовки
    if (!WriteProcessMemory(hProcess, remoteBase,
                            dllBuffer.data(),
                            ntHeaders->OptionalHeader.SizeOfHeaders,
                            nullptr)) return false;

    // 3. Копируем секции
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeaders);
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        LPVOID dest = (BYTE*)remoteBase + section[i].VirtualAddress;
        LPVOID src  = (BYTE*)dllBuffer.data() + section[i].PointerToRawData;
        SIZE_T size = section[i].SizeOfRawData;
        if (!WriteProcessMemory(hProcess, dest, src, size, nullptr)) return false;
    }

    // 4. Подготовка структуры MANUAL_MAPPING_DATA
    MANUAL_MAPPING_DATA data{};
    data.pLoadLibraryA  = (HMODULE(__stdcall*)(LPCSTR))GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    data.pGetProcAddress = (FARPROC(__stdcall*)(HMODULE,LPCSTR))GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcAddress");
    data.pBase = remoteBase;

    // 5. Выделяем память под структуру + shellcode
    SIZE_T shellcodeSize = (SIZE_T)((BYTE*)ShellcodeEnd - (BYTE*)Shellcode);
    LPVOID remoteData = VirtualAllocEx(hProcess, nullptr,
                                       sizeof(MANUAL_MAPPING_DATA) + shellcodeSize,
                                       MEM_COMMIT | MEM_RESERVE,
                                       PAGE_READWRITE);
    if (!remoteData) return false;

    LPVOID remoteDataStruct = remoteData;
    LPVOID remoteShellcode = (BYTE*)remoteData + sizeof(MANUAL_MAPPING_DATA);

    // Копируем структуру + shellcode
    WriteProcessMemory(hProcess, remoteDataStruct, &data, sizeof(MANUAL_MAPPING_DATA), nullptr);
    WriteProcessMemory(hProcess, remoteShellcode, (LPVOID)Shellcode, shellcodeSize, nullptr);

    // 6. Запускаем remote thread
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                        (LPTHREAD_START_ROUTINE)remoteShellcode,
                                        remoteDataStruct,
                                        0, nullptr);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    // 7. Применяем права секций (код — EXECUTE_READ, данные — READWRITE)
    SetSectionProtections(hProcess, remoteBase, ntHeaders);

    // 8. Освобождаем память структуры + shellcode
    VirtualFreeEx(hProcess, remoteData, 0, MEM_RELEASE);

    return true;
}

// -------------------- MAIN --------------------
int main() {
    DWORD pid;
    std::cout << "Enter target PID: ";
    std::cin >> pid;

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cerr << "Failed to open process\n";
        return 1;
    }

    // Загружаем DLL в буфер
    std::ifstream file("example.dll", std::ios::binary | std::ios::ate);
    if (!file.is_open()) return 1;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) return 1;

    if (ManualMapDLL(hProcess, buffer))
        std::cout << "DLL successfully manual mapped\n";
    else
        std::cerr << "Manual mapping failed\n";

    CloseHandle(hProcess);
    return 0;
}
