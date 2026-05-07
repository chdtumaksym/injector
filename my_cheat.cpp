#include <windows.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <cstdio>

namespace offsets {
    // Обновленные оффсеты. Если 0x60C не сработает, попробуй 0x5FC (но судя по твоему логу, он не подошел)
    constexpr uintptr_t m_hPawn = 0x60C;        
    constexpr uintptr_t m_vOldOrigin = 0x127C;  
    constexpr uintptr_t m_iIDEntIndex = 0x1544; // Корректный оффсет для Triggerbot (CrosshairID)
}

struct Vector3 { float x, y, z; };

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
            for (SIZE_T j = 0; j <= size - b.size(); ++j) {
                bool f = true;
                for (size_t k = 0; k < b.size(); ++k) {
                    if (b[k] != -1 && region[j + k] != static_cast<BYTE>(b[k])) { f = false; break; }
                }
                if (f) return reinterpret_cast<uintptr_t>(&region[j]);
            }
        }
        curr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return 0;
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    LogMessage("[CS2] --- V104 OFFSET RECOVERY ---\n");
    
    while (!GetModuleHandleA("client.dll")) Sleep(1000);

    uintptr_t lpPattern = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elPattern = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB");

    uintptr_t dwLocalPlayerController = ResolveRIP(lpPattern, 3, 7);
    uintptr_t dwEntityList = ResolveRIP(elPattern, 3, 7);

    if (!dwLocalPlayerController || !dwEntityList) {
        LogMessage("[!] CRITICAL: Pattern scan failed.\n");
        return 1;
    }

    LogMessage("[+] Core Addresses: Controller=%p, EntityList=%p\n", (void*)dwLocalPlayerController, (void*)dwEntityList);

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        uintptr_t controller = ReadMem<uintptr_t>(dwLocalPlayerController);
        if (!controller) { Sleep(500); continue; }

        uint32_t handle = ReadMem<uint32_t>(controller + offsets::m_hPawn);
        // В CS2 хендл игрока не может быть таким огромным как 0x7FFA
        if (handle == 0 || handle == 0xFFFFFFFF || (handle & 0x7FFF) > 16384) {
            static uint32_t lastHandle = 0;
            if (handle != lastHandle) {
                LogMessage("[Chain] Invalid Pawn Handle: 0x%X (Check m_hPawn offset)\n", handle);
                lastHandle = handle;
            }
            Sleep(1000); continue;
        }

        uintptr_t list = ReadMem<uintptr_t>(dwEntityList);
        if (!list) { Sleep(500); continue; }

        // Поиск в EntityList по индексу из хендла
        uint32_t pIdx = handle & 0x7FFF;
        uintptr_t entry = ReadMem<uintptr_t>(list + 0x8 * (pIdx >> 9) + 0x10);
        if (!entry) {
            LogMessage("[Chain] List Entry NULL for index %d\n", pIdx);
            Sleep(1000); continue;
        }

        uintptr_t localPawn = ReadMem<uintptr_t>(entry + 120 * (pIdx & 0x1FF));
        if (!localPawn) {
            LogMessage("[Chain] Pawn pointer NULL at index %d\n", pIdx);
            Sleep(1000); continue;
        }

        // Если все проверки пройдены - выводим данные
        Vector3 pos = ReadMem<Vector3>(localPawn + offsets::m_vOldOrigin);
        if (pos.x != 0.f) {
            LogMessage("POS: %.1f %.1f %.1f\n", pos.x, pos.y, pos.z);
        }

        // Триггербот (CapsLock)
        if (GetAsyncKeyState(VK_CAPITAL) & 0x8000) {
            int crosshairId = ReadMem<int>(localPawn + offsets::m_iIDEntIndex);
            if (crosshairId > 0 && crosshairId <= 100) {
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                Sleep(20);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                Sleep(200);
            }
        }
        Sleep(100); // Опрашиваем чаще для Triggerbot
    }

    LogMessage("[!] Unloading.\n");
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
