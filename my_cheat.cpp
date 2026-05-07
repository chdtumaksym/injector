#include <windows.h>
#include <fstream>
#include <vector>
#include <stdint.h>

struct Vector3 { float x, y, z; };

namespace offsets {
    uintptr_t dwLocalPlayerController = 0;
    uintptr_t dwEntityList = 0;
    constexpr uintptr_t m_hPawn = 0x60C;
    constexpr uintptr_t m_vOldOrigin = 0x127C;
}

template <typename T>
T ReadMem(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return T();
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return *reinterpret_cast<T*>(addr);
    return T();
}

uintptr_t ResolveRIP(uintptr_t inst, uint32_t offset, uint32_t size) {
    return inst ? inst + size + *reinterpret_cast<int32_t*>(inst + offset) : 0;
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
    PIMAGE_SECTION_HEADER sect = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sect) {
        if (sect->Characteristics & 0x40000000) {
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

DWORD WINAPI AutonomousThread(LPVOID lpParam) {
    std::ofstream log("CS2_Main_Log.txt", std::ios::app);
    offsets::dwLocalPlayerController = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E"), 3, 7);
    offsets::dwEntityList = ResolveRIP(FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 40 8B FA C1 EB"), 3, 7);

    log << "--- SECURE SCANNER v87 ---\nLP: " << std::hex << offsets::dwLocalPlayerController << "\nEL: " << offsets::dwEntityList << "\n";
    log.flush();

    while (true) {
        uintptr_t ctrl = ReadMem<uintptr_t>(offsets::dwLocalPlayerController);
        uint32_t h = ReadMem<uint32_t>(ctrl + offsets::m_hPawn);
        if (h != 0 && h != 0xFFFFFFFF) {
            uintptr_t list = ReadMem<uintptr_t>(offsets::dwEntityList);
            uintptr_t entry = ReadMem<uintptr_t>(list + 0x8 * ((h & 0x7FFF) >> 9) + 0x10);
            uintptr_t pawn = ReadMem<uintptr_t>(entry + 120 * (h & 0x1FF));
            if (pawn) {
                Vector3 pos = ReadMem<Vector3>(pawn + offsets::m_vOldOrigin);
                log << "POS: " << std::dec << pos.x << " " << pos.y << " " << pos.z << "\n"; log.flush();
            }
        }
        Sleep(1000);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID p) {
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        if (HANDLE ht = CreateThread(0, 0, AutonomousThread, 0, 0, 0)) CloseHandle(ht);
    }
    return TRUE;
}
