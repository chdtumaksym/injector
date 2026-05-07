#include <windows.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cmath>

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
    LogMessage("\n[CS2] --- V110 ULTIMATE BRUTEFORCER ---\n");
    
    while (!GetModuleHandleA("client.dll")) Sleep(1000);

    // Базовые якоря (работают железно)
    uintptr_t lpPattern = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 4E");
    uintptr_t elPattern = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB");

    uintptr_t dwLocalPlayerController = ResolveRIP(lpPattern, 3, 7);
    uintptr_t dwEntityList = ResolveRIP(elPattern, 3, 7);

    if (!dwLocalPlayerController || !dwEntityList) {
        LogMessage("[!] CRITICAL FAILURE: Pattern scan failed.\n");
        return 1;
    }

    LogMessage("[+] Core Locked! LP: %p | EL: %p\n", (void*)dwLocalPlayerController, (void*)dwEntityList);

    int dyn_m_hPawn = 0; // Сюда мы запишем правильный оффсет

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        uintptr_t controller = ReadMem<uintptr_t>(dwLocalPlayerController);
        uintptr_t list = ReadMem<uintptr_t>(dwEntityList);

        if (!controller || !list) {
            Sleep(500);
            continue;
        }

        // БРУТФОРС ОФФСЕТА ПЕШКИ
        if (dyn_m_hPawn == 0) {
            // Ищем в диапазоне актуальных смещений CS2 (0x700 - 0x850)
            for (int off = 0x700; off < 0x850; off += 4) {
                uint32_t handle = ReadMem<uint32_t>(controller + off);
                if (handle == 0 || handle == 0xFFFFFFFF) continue;

                uint32_t idx = handle & 0x7FFF;
                // ИНДЕКС ПЕШКИ ВСЕГДА БОЛЬШЕ 64! Отсекаем мусор и контроллеры.
                if (idx <= 64 || idx > 2048) continue;

                uintptr_t entry = ReadMem<uintptr_t>(list + 0x8 * (idx >> 9) + 0x10);
                if (!entry) continue;

                uintptr_t pawn = ReadMem<uintptr_t>(entry + 120 * (idx & 0x1FF));
                if (!pawn) continue;

                // Магия: проверяем, живой ли это игрок, читая m_iHealth (0x334)
                int health = ReadMem<int>(pawn + 0x334);
                if (health >= 1 && health <= 100) {
                    dyn_m_hPawn = off;
                    LogMessage("[+] BINGO! Bruteforced m_hPawn offset: 0x%X (Health: %d)\n", off, health);
                    break;
                }
            }

            if (dyn_m_hPawn == 0) {
                static bool waitLog = false;
                if (!waitLog) { LogMessage("[Scan] Bruteforcing memory... (Make sure you are spawned and alive!)\n"); waitLog = true; }
                Sleep(1000);
                continue;
            }
        }

        // РАБОТАЕМ С НАЙДЕННЫМ ОФФСЕТОМ
        uint32_t handle = ReadMem<uint32_t>(controller + dyn_m_hPawn);
        if (handle != 0 && handle != 0xFFFFFFFF) {
            uint32_t idx = handle & 0x7FFF;
            uintptr_t entry = ReadMem<uintptr_t>(list + 0x8 * (idx >> 9) + 0x10);
            if (entry) {
                uintptr_t localPawn = ReadMem<uintptr_t>(entry + 120 * (idx & 0x1FF));
                if (localPawn) {
                    
                    // Читаем координаты для лога
                    Vector3 pos = ReadMem<Vector3>(localPawn + 0x127C); // Самый частый оффсет
                    if (pos.x == 0.0f) pos = ReadMem<Vector3>(localPawn + 0x1324); // Запасной

                    static int tick = 0;
                    if (tick++ % 20 == 0 && std::isfinite(pos.x) && pos.x != 0.0f) {
                        LogMessage("[Target Locked] POS: X=%.1f Y=%.1f\n", pos.x, pos.y);
                    }

                    // ТРИГГЕРБОТ
                    if (GetAsyncKeyState(VK_CAPITAL) & 0x8000) { // Зажат CapsLock
                        int crossId = ReadMem<int>(localPawn + 0x1544);
                        if (crossId > 0 && crossId <= 64) {
                            LogMessage("[Triggerbot] Firing! Enemy ID: %d\n", crossId);
                            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                            Sleep(15);
                            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                            Sleep(150);
                        }
                    }
                } else {
                    dyn_m_hPawn = 0; // Игрок умер или вышел — сброс брутфорсера
                }
            } else {
                dyn_m_hPawn = 0;
            }
        } else {
            dyn_m_hPawn = 0;
        }

        Sleep(20);
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
