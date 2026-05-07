#include <windows.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <cstdio>

namespace offsets {
    // Самые стабильные оффсеты для прямого чтения из пешки (Pawn)
    constexpr uintptr_t m_vOldOrigin = 0x127C;  
    constexpr uintptr_t m_iIDEntIndex = 0x1544; // CrosshairID
}

struct Vector3 { float x, y, z; };

// Защищенное чтение памяти
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
    LogMessage("[CS2] --- V105 THE NUCLEAR FIX ---\n");
    
    while (!GetModuleHandleA("client.dll")) Sleep(1000);

    // ПРЯМОЙ ПОИСК LocalPlayerPawn
    // Мы пропускаем контроллеры и листы сущностей, чтобы цепочка не ломалась
    uintptr_t pawnPattern = FindPattern("client.dll", "48 8B 05 ? ? ? ? 48 85 C0 74 4F");
    uintptr_t dwLocalPlayerPawn = ResolveRIP(pawnPattern, 3, 7);

    if (!dwLocalPlayerPawn) {
        LogMessage("[!] Pattern scan failed. Trying secondary signature...\n");
        pawnPattern = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 31");
        dwLocalPlayerPawn = ResolveRIP(pawnPattern, 3, 7);
    }

    if (!dwLocalPlayerPawn) {
        LogMessage("[!] CRITICAL FAILURE: Could not find LocalPlayerPawn address.\n");
        return 1;
    }

    LogMessage("[+] Pawn Address Resolved: %p\n", (void*)dwLocalPlayerPawn);

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        // Читаем адрес нашей пешки (Pawn) напрямую
        uintptr_t localPawn = ReadMem<uintptr_t>(dwLocalPlayerPawn);
        
        if (localPawn) {
            Vector3 pos = ReadMem<Vector3>(localPawn + offsets::m_vOldOrigin);
            
            // Если мы получили вменяемые координаты
            if (pos.x != 0.f || pos.y != 0.f) {
                static int tick = 0;
                if (tick++ % 10 == 0) { // Не спамим лог каждую секунду
                    LogMessage("POS: X=%.1f Y=%.1f Z=%.1f\n", pos.x, pos.y, pos.z);
                }

                // Триггербот на CapsLock
                if (GetAsyncKeyState(VK_CAPITAL) & 0x8000) {
                    int crosshairId = ReadMem<int>(localPawn + offsets::m_iIDEntIndex);
                    // Если в прицеле игрок (ID 1-64)
                    if (crosshairId > 0 && crosshairId <= 64) {
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        Sleep(10);
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        Sleep(150);
                    }
                }
            }
        } else {
            static bool waitMsg = false;
            if (!waitMsg) { LogMessage("[State] Waiting for player to spawn (Enter match)...\n"); waitMsg = true; }
        }
        
        Sleep(50); // Частота опроса 20 раз в секунду
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
