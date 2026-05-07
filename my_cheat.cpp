#include <windows.h>
#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>

// Structures for Source 2 Schema System
struct SchemaClassFieldData_t {
    const char* m_name;
    void* m_type;
    short m_offset;
    char pad_0012[14];
};

struct SchemaClassInfo_t {
    char pad_0000[8];
    const char* m_name;
    char pad_0010[24];
    short m_fields_count;
    char pad_002A[6];
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

// Memory Safety via VirtualQuery
template <typename T>
T ReadMem(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T();
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return *reinterpret_cast<T*>(addr);
    return T();
}

// RIP-Relative Resolver for x64
uintptr_t ResolveRIP(uintptr_t inst, uint32_t offset, uint32_t size) {
    return inst ? inst + size + *reinterpret_cast<int32_t*>(inst + offset) : 0;
}

// Secure Pattern Scanner
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
    PIMAGE_SECTION_HEADER sect = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sect) {
        if (sect->Characteristics & 0x40000000) { // IMAGE_SCN_MEM_READ
            BYTE* s = reinterpret_cast<BYTE*>(base + sect->VirtualAddress);
            DWORD sz = sect->Misc.VirtualSize;
            if (sz < b.size()) continue;

            for (DWORD j = 0; j < sz - b.size(); ++j) {
                bool f = true;
                for (size_t k = 0; k < b.size(); ++k) {
                    if (b[k] != -1 && s[j + k] != static_cast<BYTE>(b[k])) { f = false; break; }
                }
                if (f) return reinterpret_cast<uintptr_t>(&s[j]);
            }
        }
    }
    return 0;
}

namespace schema {
    uintptr_t GetOffset(const char* moduleName, const char* className, const char* fieldName) {
        HMODULE hSch = GetModuleHandleA("schemasystem.dll");
        if (!hSch) return 0;
        
        auto CreateInterface = (CreateInterfaceFn)GetProcAddress(hSch, "CreateInterface");
        uintptr_t schemaSystem = (uintptr_t)CreateInterface("SchemaSystem_001", nullptr);
        if (!schemaSystem) return 0;

        // GlobalTypeScope index is 13
        uintptr_t typeScope = (*(uintptr_t(__fastcall***)(uintptr_t, const char*))schemaSystem)[13](schemaSystem, moduleName);
        if (!typeScope) return 0;

        // FindDeclaredClass index is 2
        SchemaClassInfo_t* classInfo = nullptr;
        (*(void(__fastcall***)(uintptr_t, SchemaClassInfo_t**, const char*))typeScope)[2](typeScope, &classInfo, className);
        
        if (classInfo && classInfo->m_fields) {
            for (int i = 0; i < classInfo->m_fields_count; i++) {
                if (std::string(classInfo->m_fields[i].m_name) == fieldName) {
                    return classInfo->m_fields[i].m_offset;
                }
            }
        }
        return 0;
    }
}

DWORD WINAPI GlobalAutonomousThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    log << "--- INITIALIZING GLOBAL AUTONOMOUS SYSTEM ---\n";

    // 1. Dynamic Pattern Search (Base Addresses)
    offsets::dwLocalPlayerController = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E"), 3, 7);
    offsets::dwEntityList = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB"), 3, 7);

    // 2. Dynamic Schema Parsing (Class Offsets)
    offsets::m_hPawn = schema::GetOffset("client.dll", "CBasePlayerController", "m_hPawn");
    offsets::m_vOldOrigin = schema::GetOffset("client.dll", "C_BasePlayerPawn", "m_vOldOrigin");

    log << "Base LP: " << std::hex << offsets::dwLocalPlayerController << "\n";
    log << "Base EL: " << offsets::dwEntityList << "\n";
    log << "Schema m_hPawn: 0x" << offsets::m_hPawn << "\n";
    log << "Schema m_vOldOrigin: 0x" << offsets::m_vOldOrigin << "\n";
    log.flush();

    while (true) {
        uintptr_t ctrl = ReadMem<uintptr_t>(offsets::dwLocalPlayerController);
        if (ctrl) {
            uint32_t h = ReadMem<uint32_t>(ctrl + offsets::m_hPawn);
            if (h != 0 && h != 0xFFFFFFFF) {
                uintptr_t list = ReadMem<uintptr_t>(offsets::dwEntityList);
                if (list) {
                    uintptr_t entry = ReadMem<uintptr_t>(list + 0x8 * ((h & 0x7FFF) >> 9) + 0x10);
                    if (entry) {
                        uintptr_t pawn = ReadMem<uintptr_t>(entry + 120 * (h & 0x1FF));
                        if (pawn) {
                            Vector3 pos = ReadMem<Vector3>(pawn + offsets::m_vOldOrigin);
                            log << "COORDS: " << std::dec << pos.x << " " << pos.y << " " << pos.z << "\n";
                            log.flush();
                        }
                    }
                }
            }
        }
        Sleep(1000);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        if (HANDLE ht = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(GlobalAutonomousThread), nullptr, 0, nullptr)) 
            CloseHandle(ht);
    }
    return TRUE;
}
