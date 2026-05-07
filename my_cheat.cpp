#include <windows.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <cstdio>

namespace constants {
    constexpr int EntityListEntrySize = 0x78; 
    constexpr int SchemaSystemIndex = 13; 
    constexpr int FindClassIndex = 2;     
}

struct Vector3 { float x, y, z; };

typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

namespace offsets {
    uintptr_t dwLocalPlayerController = 0;
    uintptr_t dwEntityList = 0;
    uintptr_t m_hPawn = 0;
    uintptr_t m_vOldOrigin = 0;
}

void LogMessage(const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buffer, fmt, args);
    va_end(args);

    OutputDebugStringA(buffer);

    FILE* f = nullptr;
    if (fopen_s(&f, "CS2_Final_Log.txt", "a+") == 0 && f) {
        fprintf(f, "%s", buffer);
        fflush(f);
        fclose(f);
    }
}

template <typename T>
T ReadMem(uintptr_t addr) {
    if (!addr || addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T();
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT) {
        if (!(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            T val;
            memcpy(&val, reinterpret_cast<void*>(addr), sizeof(T));
            return val;
        }
    }
    return T();
}

bool SafeReadString(uintptr_t addr, char* out, size_t maxLen) {
    if (!addr) return false;
    for (size_t i = 0; i < maxLen; ++i) {
        char c = ReadMem<char>(addr + i);
        out[i] = c;
        if (c == '\0') return i > 0;
    }
    out[maxLen - 1] = '\0';
    return true;
}

uintptr_t ResolveRIP(uintptr_t inst, uint32_t offset, uint32_t size) {
    if (!inst) return 0;
    return inst + size + ReadMem<int32_t>(inst + offset);
}

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
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
            BYTE* region = reinterpret_cast<BYTE*>(mbi.BaseAddress);
            SIZE_T size = mbi.RegionSize;
            if (reinterpret_cast<uintptr_t>(region) < curr) {
                size -= (curr - reinterpret_cast<uintptr_t>(region));
                region = reinterpret_cast<BYTE*>(curr);
            }
            if (reinterpret_cast<uintptr_t>(region) + size > end) size = end - reinterpret_cast<uintptr_t>(region);
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
    uintptr_t GetOffset(const char* moduleName, const char* className, const char* fieldName) {
        HMODULE hSch = GetModuleHandleA("schemasystem.dll");
        if (!hSch) return 0;
        
        auto CreateInterface = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(hSch, "CreateInterface"));
        if (!CreateInterface) return 0;

        void* schemaSystem = CreateInterface("SchemaSystem_001", nullptr);
        if (!schemaSystem) return 0;

        // ПАРАНОИДАЛЬНЫЙ ВЫЗОВ VFUNC
        void** vtable = *reinterpret_cast<void***>(schemaSystem);
        if (!vtable) return 0;
        
        auto getScope = reinterpret_cast<void*(__fastcall*)(void*, const char*)>(vtable[constants::SchemaSystemIndex]);
        if (!getScope) return 0;

        void* typeScope = getScope(schemaSystem, moduleName);
        if (!typeScope) return 0;

        void** scopeVtable = *reinterpret_cast<void***>(typeScope);
        if (!scopeVtable) return 0;

        auto findClass = reinterpret_cast<void*(__fastcall*)(void*, const char*)>(scopeVtable[constants::FindClassIndex]);
        if (!findClass) return 0;

        void* classInfo = findClass(typeScope, className);
        if (!classInfo) return 0;

        short fieldsCount = ReadMem<short>(reinterpret_cast<uintptr_t>(classInfo) + 0x1C);
        uintptr_t fieldsPtr = ReadMem<uintptr_t>(reinterpret_cast<uintptr_t>(classInfo) + 0x28);
        
        if (fieldsCount > 0 && fieldsPtr) {
            for (int i = 0; i < fieldsCount; i++) {
                uintptr_t fieldAddr = fieldsPtr + (i * 0x20); 
                uintptr_t namePtr = ReadMem<uintptr_t>(fieldAddr);
                int offset = ReadMem<int>(fieldAddr + 0x10);
                
                char nameBuf[128] = {0};
                if (SafeReadString(namePtr, nameBuf, sizeof(nameBuf))) {
                    if (strcmp(nameBuf, fieldName) == 0) return static_cast<uintptr_t>(offset);
                }
            }
        }
        return 0;
    }
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    while (!GetModuleHandleA("client.dll") || !GetModuleHandleA("schemasystem.dll")) Sleep(500);

    LogMessage("[CS2] --- V100 PARANOID START ---\n");

    offsets::dwLocalPlayerController = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E"), 3, 7);
    offsets::dwEntityList = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB"), 3, 7);
    
    LogMessage("[Scan] LP: %p | EL: %p\n", (void*)offsets::dwLocalPlayerController, (void*)offsets::dwEntityList);

    // Heartbeat логи для точной отладки
    LogMessage("[Schema] Fetching m_hPawn...\n");
    offsets::m_hPawn = schema::GetOffset("client.dll", "CBasePlayerController", "m_hPawn");
    LogMessage("[Schema] m_hPawn: 0x%x\n", (uint32_t)offsets::m_hPawn);

    LogMessage("[Schema] Fetching m_vOldOrigin...\n");
    offsets::m_vOldOrigin = schema::GetOffset("client.dll", "C_BasePlayerPawn", "m_vOldOrigin");
    LogMessage("[Schema] m_vOldOrigin: 0x%x\n", (uint32_t)offsets::m_vOldOrigin);

    if (!offsets::dwLocalPlayerController || !offsets::dwEntityList || !offsets::m_hPawn || !offsets::m_vOldOrigin) {
        LogMessage("[!] FAILURE. One of critical offsets is NULL. Thread hibernating.\n");
        while(true) Sleep(1000); 
    }

    LogMessage("[+] SUCCESS. System active. Tracking coordinates...\n");

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        uintptr_t ctrl = ReadMem<uintptr_t>(offsets::dwLocalPlayerController);
        if (ctrl) {
            uint32_t handle = ReadMem<uint32_t>(ctrl + offsets::m_hPawn);
            if (handle != 0 && handle != 0xFFFFFFFF) {
                uintptr_t list = ReadMem<uintptr_t>(offsets::dwEntityList);
                uintptr_t entry = ReadMem<uintptr_t>(list + 0x8 * ((handle & 0x7FFF) >> 9) + 0x10);
                uintptr_t pawn = ReadMem<uintptr_t>(entry + constants::EntityListEntrySize * (handle & 0x1FF));
                if (pawn) {
                    Vector3 pos = ReadMem<Vector3>(pawn + offsets::m_vOldOrigin);
                    LogMessage("POS: %.2f %.2f %.2f\n", pos.x, pos.y, pos.z);
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
        if (HANDLE ht = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, 0, 0, 0)) 
            CloseHandle(ht);
    }
    return TRUE;
}
