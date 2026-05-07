#include <windows.h>
#include <string>
#include <vector>
#include <stdint.h>
#include <cstdio>

namespace constants {
    constexpr int EntityListEntrySize = 0x78; 
    constexpr int SchemaSystemIndex = 13;     
    constexpr int FindClassIndex = 2;         
}

struct SchemaClassFieldData_t {
    const char* m_name;
    void* m_type;
    int m_offset;
    char pad_0014[12];
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
    if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T();
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) return T();
    if (mbi.State != MEM_COMMIT) return T();
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return T();
    __try {
        return *reinterpret_cast<T*>(addr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return T();
    }
}

uintptr_t ResolveRIP(uintptr_t inst, uint32_t offset, uint32_t size) {
    if (!inst) return 0;
    int32_t rel = 0;
    __try {
        rel = *reinterpret_cast<int32_t*>(inst + offset);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return inst + size + rel;
}

uintptr_t FindPattern(const char* moduleName, const char* pattern) {
    uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(moduleName));
    if (!base) return 0;

    std::vector<int> bytes;
    const char* p = pattern;
    while (*p) {
        if (*p == '?') { bytes.push_back(-1); p++; if (*p == '?') p++; }
        else bytes.push_back(static_cast<int>(strtoul(p, const_cast<char**>(&p), 16)));
    }

    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + reinterpret_cast<PIMAGE_DOS_HEADER>(base)->e_lfanew);
    uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    uintptr_t curr = base;

    while (curr < end - bytes.size()) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(curr), &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            BYTE* region = reinterpret_cast<BYTE*>(mbi.BaseAddress);
            SIZE_T size = mbi.RegionSize;
            if (reinterpret_cast<uintptr_t>(region) < curr) {
                size -= (curr - reinterpret_cast<uintptr_t>(region));
                region = reinterpret_cast<BYTE*>(curr);
            }
            if (reinterpret_cast<uintptr_t>(region) + size > end) size = end - reinterpret_cast<uintptr_t>(region);
            for (SIZE_T j = 0; j <= size - bytes.size(); ++j) {
                bool match = true;
                for (size_t k = 0; k < bytes.size(); k++) {
                    if (bytes[k] != -1 && region[j + k] != bytes[k]) { match = false; break; }
                }
                if (match) return reinterpret_cast<uintptr_t>(&region[j]);
            }
        }
        curr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return 0;
}

namespace schema {
    template <typename T>
    inline T GetVFunc(void* inst, int index) {
        if (!inst) return nullptr;
        return reinterpret_cast<T>((*reinterpret_cast<void***>(inst))[index]);
    }

    uintptr_t GetOffset(const char* moduleName, const char* className, const char* fieldName) {
        HMODULE hSch = GetModuleHandleA("schemasystem.dll");
        if (!hSch) return 0;

        auto CreateInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hSch, "CreateInterface"));
        if (!CreateInterface) return 0;

        void* schemaSystem = CreateInterface("SchemaSystem_001", nullptr);
        if (!schemaSystem) return 0;

        using GetTypeScope_t = void*(__fastcall*)(void*, const char*, void*);
        auto getScope = GetVFunc<GetTypeScope_t>(schemaSystem, constants::SchemaSystemIndex);
        if (!getScope) return 0;

        void* typeScope = getScope(schemaSystem, moduleName, nullptr);
        if (!typeScope) return 0;

        using FindDeclaredClass_t = void(__fastcall*)(void*, SchemaClassInfo_t**, const char*);
        auto findClass = GetVFunc<FindDeclaredClass_t>(typeScope, constants::FindClassIndex);
        if (!findClass) return 0;

        SchemaClassInfo_t* classInfo = nullptr;
        findClass(typeScope, &classInfo, className);
        if (!classInfo || !classInfo->m_fields) return 0;

        for (int i = 0; i < classInfo->m_fields_count; i++) {
            SchemaClassFieldData_t& field = classInfo->m_fields[i];
            if (field.m_name && strcmp(field.m_name, fieldName) == 0) return static_cast<uintptr_t>(field.m_offset);
        }
        return 0;
    }
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    while (!GetModuleHandleA("client.dll") || !GetModuleHandleA("schemasystem.dll")) Sleep(500);

    offsets::dwLocalPlayerController = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E"), 3, 7);
    offsets::dwEntityList = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB"), 3, 7);
    offsets::m_hPawn = schema::GetOffset("client.dll", "CBasePlayerController", "m_hPawn");
    offsets::m_vOldOrigin = schema::GetOffset("client.dll", "C_BasePlayerPawn", "m_vOldOrigin");

    if (!offsets::dwLocalPlayerController || !offsets::dwEntityList || !offsets::m_hPawn || !offsets::m_vOldOrigin) {
        OutputDebugStringA("[!] Critical offsets missing. Exiting.\n");
        FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 1);
    }

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        uintptr_t ctrl = ReadMem<uintptr_t>(offsets::dwLocalPlayerController);
        if (!ctrl) { Sleep(100); continue; }

        uint32_t handle = ReadMem<uint32_t>(ctrl + offsets::m_hPawn);
        if (handle == 0 || handle == 0xFFFFFFFF) { Sleep(100); continue; }

        uintptr_t list = ReadMem<uintptr_t>(offsets::dwEntityList);
        if (!list) { Sleep(100); continue; }

        uintptr_t entryBase = ReadMem<uintptr_t>(list + 0x8 * ((handle & 0x7FFF) >> 9) + 0x10);
        if (!entryBase) { Sleep(100); continue; }

        uintptr_t pawn = ReadMem<uintptr_t>(entryBase + constants::EntityListEntrySize * (handle & 0x1FF));
        if (!pawn) { Sleep(100); continue; }

        Vector3 pos = ReadMem<Vector3>(pawn + offsets::m_vOldOrigin);
        if (pos.x != 0.f || pos.y != 0.f || pos.z != 0.f) {
            char buffer[128];
            sprintf_s(buffer, "POS: %.2f %.2f %.2f\n", pos.x, pos.y, pos.z);
            OutputDebugStringA(buffer);
        }

        Sleep(1000);
    }

    FreeLibraryAndExitThread(static_cast<HMODULE>(lpParam), 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibrary
