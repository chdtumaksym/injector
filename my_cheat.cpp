#include <windows.h>
#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>

// Source 2 Constants
namespace constants {
    constexpr int EntityListEntrySize = 0x78; // 120 in decimal
    constexpr int SchemaSystemIndex = 13;     // vtable index for GlobalTypeScope
    constexpr int FindClassIndex = 2;         // vtable index for FindDeclaredClass
}

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

// Memory Safety: Fast and relatively safe
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

// Optimized Scanner
uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(moduleName));
    if (!base) return 0;

    std::vector<int> patternBytes;
    char* start_p = const_cast<char*>(pattern);
    for (char* curr = start_p; curr < start_p + strlen(pattern); ++curr) {
        if (*curr == '?') { 
            curr++; if (*curr == '?') curr++; 
            patternBytes.push_back(-1); 
        } else {
            patternBytes.push_back(static_cast<int>(strtoul(curr, &curr, 16)));
        }
    }

    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + reinterpret_cast<PIMAGE_DOS_HEADER>(base)->e_lfanew);
    PIMAGE_SECTION_HEADER sect = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sect) {
        if (sect->Characteristics & (IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE)) {
            BYTE* s = reinterpret_cast<BYTE*>(base + sect->VirtualAddress);
            DWORD sz = sect->Misc.VirtualSize;
            if (sz < patternBytes.size()) continue;

            for (DWORD j = 0; j < sz - patternBytes.size(); ++j) {
                bool found = true;
                for (size_t k = 0; k < patternBytes.size(); ++k) {
                    if (patternBytes[k] != -1 && s[j + k] != static_cast<BYTE>(patternBytes[k])) {
                        found = false;
                        break;
                    }
                }
                if (found) return reinterpret_cast<uintptr_t>(&s[j]);
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
        if (!CreateInterface) return 0;

        uintptr_t schemaSystem = (uintptr_t)CreateInterface("SchemaSystem_001", nullptr);
        if (!schemaSystem) return 0;

        // Use virtual function calls with caution
        auto GetTypeScope = (*(uintptr_t(__fastcall***)(uintptr_t, const char*))schemaSystem)[constants::SchemaSystemIndex];
        uintptr_t typeScope = GetTypeScope(schemaSystem, moduleName);
        if (!typeScope) return 0;

        SchemaClassInfo_t* classInfo = nullptr;
        auto FindDeclaredClass = (*(void(__fastcall***)(uintptr_t, SchemaClassInfo_t**, const char*))typeScope)[constants::FindClassIndex];
        FindDeclaredClass(typeScope, &classInfo, className);
        
        if (classInfo && classInfo->m_fields) {
            for (int i = 0; i < classInfo->m_fields_count; i++) {
                auto& field = classInfo->m_fields[i];
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
    log << "[+] Session started. Waiting for modules...\n";
    log.flush();

    // Wait for client.dll to be fully loaded
    while (!GetModuleHandleA("client.dll") || !GetModuleHandleA("schemasystem.dll")) {
        Sleep(500);
    }

    // Dynamic Initialization with safety checks
    offsets::dwLocalPlayerController = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E"), 3, 7);
    offsets::dwEntityList = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB"), 3, 7);
    
    offsets::m_hPawn = schema::GetOffset("client.dll", "CBasePlayerController", "m_hPawn");
    offsets::m_vOldOrigin = schema::GetOffset("client.dll", "C_BasePlayerPawn", "m_vOldOrigin");

    if (!offsets::dwLocalPlayerController || !offsets::dwEntityList || !offsets::m_hPawn || !offsets::m_vOldOrigin) {
        log << "[!] Critical error: Some offsets not found!\n";
        log << "LP: " << std::hex << offsets::dwLocalPlayerController << " EL: " << offsets::dwEntityList << "\n";
        log << "m_hPawn: 0x" << offsets::m_hPawn << " Origin: 0x" << offsets::m_vOldOrigin << "\n";
        log.flush();
        FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 1);
    }

    log << "[+] Initialization successful. Polling coordinates...\n";
    log.flush();

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        uintptr_t ctrl = ReadMem<uintptr_t>(offsets::dwLocalPlayerController);
        if (ctrl) {
            uint32_t h = ReadMem<uint32_t>(ctrl + offsets::m_hPawn);
            if (h != 0 && h != 0xFFFFFFFF) {
                uintptr_t list = ReadMem<uintptr_t>(offsets::dwEntityList);
                if (list) {
                    uintptr_t entry = ReadMem<uintptr_t>(list + 0x8 * ((h & 0x7FFF) >> 9) + 0x10);
                    if (entry) {
                        uintptr_t pawn = ReadMem<uintptr_t>(entry + constants::EntityListEntrySize * (h & 0x1FF));
                        if (pawn) {
                            Vector3 pos = ReadMem<Vector3>(pawn + offsets::m_vOldOrigin);
                            if (pos.x != 0.0f || pos.y != 0.0f) {
                                log << "POS: " << pos.x << " " << pos.y << " " << pos.z << "\n";
                                log.flush();
                            }
                        }
                    }
                }
            }
        }
        Sleep(500);
    }

    log << "[!] Unloading...\n";
    log.close();
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
