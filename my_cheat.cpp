#include <windows.h>
#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>

namespace constants {
    constexpr int EntityListEntrySize = 0x78; 
    constexpr int SchemaSystemIndex = 13;     
    constexpr int FindClassIndex = 2;         
}

struct SchemaClassFieldData_t {
    const char* m_name;
    void* m_type;
    short m_offset;
    char pad_0012[14];
};

struct SchemaClassInfo_t {
    void* m_vtable;
    const char* m_name;
    const char* m_module_name;
    int m_size;
    short m_fields_count;
    char pad_1E[10];
    SchemaClassFieldData_t* m_fields;
};

struct Vector3 { float x, y, z; };

typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

namespace offsets {
    uintptr_t dwLocalPlayerController = 0;
    uintptr_t dwEntityList = 0;
    uintptr_t m_hPawn = 0;
    uintptr_t m_vOldOrigin = 0;
}

template <typename T>
T ReadMem(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T();
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT) {
        if (!(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return *reinterpret_cast<T*>(addr);
    }
    return T();
}

uintptr_t ResolveRIP(uintptr_t inst, uint32_t offset, uint32_t size) {
    if (!inst) return 0;
    return inst + size + *reinterpret_cast<int32_t*>(inst + offset);
}

// Переписанный и абсолютно безопасный сканнер через VirtualQuery
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(moduleName));
    if (!base) return 0;

    std::vector<int> b;
    char* start_p = const_cast<char*>(pattern);
    for (char* curr = start_p; curr < start_p + strlen(pattern); ++curr) {
        if (*curr == '?') { curr++; if (*curr == '?') curr++; b.push_back(-1); }
        else b.push_back(static_cast<int>(strtoul(curr, &curr, 16)));
    }

    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + reinterpret_cast<PIMAGE_DOS_HEADER>(base)->e_lfanew);
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    uintptr_t curr = base;

    while (curr < end - b.size()) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(curr), &mbi, sizeof(mbi))) break;

        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            BYTE* region = reinterpret_cast<BYTE*>(mbi.BaseAddress);
            SIZE_T size = mbi.RegionSize;

            if (reinterpret_cast<uintptr_t>(region) < curr) {
                size -= (curr - reinterpret_cast<uintptr_t>(region));
                region = reinterpret_cast<BYTE*>(curr);
            }
            if (reinterpret_cast<uintptr_t>(region) + size > end) {
                size = end - reinterpret_cast<uintptr_t>(region);
            }

            if (size >= b.size()) {
                for (SIZE_T j = 0; j <= size - b.size(); ++j) {
                    bool f = true;
                    for (size_t k = 0; k < b.size(); ++k) {
                        if (b[k] != -1 && region[j + k] != static_cast<BYTE>(b[k])) { f = false; break; }
                    }
                    if (f) return reinterpret_cast<uintptr_t>(&region[j]);
                }
            }
        }
        curr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return 0;
}

namespace schema {
    uintptr_t GetOffset(std::ofstream& log, const char* moduleName, const char* className, const char* fieldName) {
        HMODULE hSch = GetModuleHandleA("schemasystem.dll");
        if (!hSch) return 0;
        
        auto CreateInterface = (CreateInterfaceFn)GetProcAddress(hSch, "CreateInterface");
        if (!CreateInterface) return 0;

        uintptr_t schemaSystem = (uintptr_t)CreateInterface("SchemaSystem_001", nullptr);
        if (!schemaSystem) return 0;

        using GetTypeScope_t = uintptr_t(__fastcall*)(uintptr_t, const char*, void*);
        uintptr_t typeScope = (*(GetTypeScope_t**)schemaSystem)[constants::SchemaSystemIndex](schemaSystem, moduleName, nullptr);
        if (!typeScope) return 0;

        using FindDeclaredClass_t = SchemaClassInfo_t*(__fastcall*)(uintptr_t, const char*);
        SchemaClassInfo_t* classInfo = (*(FindDeclaredClass_t**)typeScope)[constants::FindClassIndex](typeScope, className);
        
        if (classInfo && classInfo->m_fields) {
            for (int i = 0; i < classInfo->m_fields_count; i++) {
                SchemaClassFieldData_t& field = classInfo->m_fields[i];
                if (field.m_name && strcmp(field.m_name, fieldName) == 0) {
                    return field.m_offset;
                }
            }
        }
        return 0;
    }
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    log << "[+] Session started. Waiting for modules...\n"; log.flush();

    while (!GetModuleHandleA("client.dll") || !GetModuleHandleA("schemasystem.dll")) Sleep(500);

    log << "[+] Modules found. Scanning dwLocalPlayerController...\n"; log.flush();
    offsets::dwLocalPlayerController = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E"), 3, 7);
    log << "    -> Found: " << std::hex << offsets::dwLocalPlayerController << "\n"; log.flush();

    log << "[+] Scanning dwEntityList...\n"; log.flush();
    offsets::dwEntityList = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB"), 3, 7);
    log << "    -> Found: " << std::hex << offsets::dwEntityList << "\n"; log.flush();
    
    log << "[+] Parsing Schema System for m_hPawn...\n"; log.flush();
    offsets::m_hPawn = schema::GetOffset(log, "client.dll", "CBasePlayerController", "m_hPawn");
    log << "    -> Found: " << std::hex << offsets::m_hPawn << "\n"; log.flush();

    log << "[+] Parsing Schema System for m_vOldOrigin...\n"; log.flush();
    offsets::m_vOldOrigin = schema::GetOffset(log, "client.dll", "C_BasePlayerPawn", "m_vOldOrigin");
    log << "    -> Found: " << std::hex << offsets::m_vOldOrigin << "\n"; log.flush();

    if (!offsets::dwLocalPlayerController || !offsets::dwEntityList || !offsets::m_hPawn || !offsets::m_vOldOrigin) {
        log << "[!] Critical error: Missing offsets. Shutting down.\n"; log.flush();
        FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 1);
    }

    log << "[+] ALL SYSTEMS GO. Tracking coordinates...\n"; log.flush();

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        uintptr_t ctrl = ReadMem<uintptr_t>(offsets::dwLocalPlayerController);
        if (ctrl) {
            uint32_t handle = ReadMem<uint32_t>(ctrl + offsets::m_hPawn);
            if (handle != 0 && handle != 0xFFFFFFFF) {
                uintptr_t list = ReadMem<uintptr_t>(offsets::dwEntityList);
                if (list) {
                    uintptr_t entry = ReadMem<uintptr_t>(list + 0x8 * ((handle & 0x7FFF) >> 9) + 0x10);
                    if (entry) {
                        uintptr_t pawn = ReadMem<uintptr_t>(entry + constants::EntityListEntrySize * (handle & 0x1FF));
                        if (pawn) {
                            Vector3 pos = ReadMem<Vector3>(pawn + offsets::m_vOldOrigin);
                            if (pos.x != 0.0f || pos.y != 0.0f) {
                                log << "POS: " << std::dec << pos.x << " " << pos.y << " " << pos.z << "\n";
                                log.flush();
                            }
                        }
                    }
                }
            }
        }
        Sleep(1000);
    }

    log << "[!] Unloading...\n"; log.close();
    FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 0);
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        HANDLE ht = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(MainThread), h, 0, nullptr);
        if (ht) CloseHandle(ht);
    }
    return TRUE;
}
