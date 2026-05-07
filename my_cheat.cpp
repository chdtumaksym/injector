#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>

// Быстрое и действительно безопасное чтение памяти через SEH (Structured Exception Handling)
// Не насилует ядро системными вызовами VirtualQuery при каждом чихе.
template <typename T>
T ReadMem(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFEFFFFFFFF) return T();
    T val = T();
    __try {
        val = *reinterpret_cast<T*>(addr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return T(); // Если память нечитаема, ловим исключение и отдаем пустоту
    }
    return val;
}

// Оптимизированный логгер. Открываем файл один раз на сессию.
void LogMessage(const char* fmt, ...) {
    static FILE* f = nullptr;
    if (!f) {
        fopen_s(&f, "CS2_Final_Log.txt", "w");
    }
    
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsprintf_s(buffer, fmt, args);
    va_end(args);
    
    OutputDebugStringA(buffer);
    if (f) {
        fprintf(f, "%s", buffer);
        fflush(f);
    }
}

uintptr_t ResolveRIP(uintptr_t inst, uint32_t offset, uint32_t size) {
    if (!inst) return 0;
    return inst + size + ReadMem<int32_t>(inst + offset);
}

// Твой сканнер паттернов пойдет, хотя и не самый быстрый из-за strtoul в цикле
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
        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READONLY | PAGE_READWRITE))) {
            BYTE* region = reinterpret_cast<BYTE*>(mbi.BaseAddress);
            SIZE_T size = mbi.RegionSize;
            
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
    LogMessage("\n[CS2] --- V114 OPTIMIZED SCANNER ---\n");
    
    while (!GetModuleHandleA("client.dll")) Sleep(1000);

    uintptr_t pawnPattern = FindPattern("client.dll", "48 8B 05 ? ? ? ? 48 85 C0 74 4F");
    uintptr_t dwLocalPlayerPawn = ResolveRIP(pawnPattern, 3, 7);

    // Паттерн для списка сущностей, без него аимбот сделать физически невозможно
    uintptr_t entityListPattern = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA C1 EB");
    uintptr_t dwEntityList = ResolveRIP(entityListPattern, 3, 7);

    if (!dwLocalPlayerPawn || ReadMem<uintptr_t>(dwLocalPlayerPawn) == 0) {
        LogMessage("[!] Primary pattern failed. Trying fallback...\n");
        pawnPattern = FindPattern("client.dll", "48 8B 0D ? ? ? ? 48 85 C9 74 31");
        dwLocalPlayerPawn = ResolveRIP(pawnPattern, 3, 7);
    }

    if (!dwLocalPlayerPawn) {
        LogMessage("[!] CRITICAL: All pawn patterns failed.\n");
        return 1;
    }

    if (!dwEntityList) {
        LogMessage("[!] CRITICAL: EntityList not found! You are blind.\n");
        return 1;
    }

    LogMessage("[+] Deep Scanner Hooked: LocalPawn: %p | EntityList: %p\n", (void*)dwLocalPlayerPawn, (void*)dwEntityList);

    int calibratedTriggerOffset = 0;
    struct ScanResult { int offset; int value; };
    std::vector<ScanResult> wallValues;
    bool waitingForWall = true;

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        uintptr_t localPawn = ReadMem<uintptr_t>(dwLocalPlayerPawn);
        uintptr_t entList = ReadMem<uintptr_t>(dwEntityList);
        
        if (!localPawn || !entList) {
            Sleep(500);
            continue;
        }

        // Обход энтити листа для поиска вражеских моделек
        if (GetAsyncKeyState(VK_F1) & 0x8000) {
            LogMessage("\n--- DUMPING ENTITIES ---\n");
            // В CS2 список разбит на блоки. Берем первый базовый чанк для игроков
            uintptr_t listEntry = ReadMem<uintptr_t>(entList + 0x10); 
            for (int i = 1; i <= 64; i++) {
                uintptr_t currentController = ReadMem<uintptr_t>(listEntry + i * 0x78);
                if (!currentController) continue;
                
                // Достаем m_hPawn (ссылка на физическую модель контроллера)
                uint32_t pawnHandle = ReadMem<uint32_t>(currentController + 0x60C); // Оффсет может отличаться от версии к версии
                if (!pawnHandle) continue;

                // Магия вычисления адреса пешки из хэндла в CS2
                uintptr_t listEntry2 = ReadMem<uintptr_t>(entList + 0x8 * ((pawnHandle & 0x7FFF) >> 9) + 0x10);
                uintptr_t currentPawn = ReadMem<uintptr_t>(listEntry2 + 0x78 * (pawnHandle & 0x1FF));
                
                if (currentPawn && currentPawn != localPawn) {
                    // Читаем m_iHealth, чтобы убедиться, что это живая модель
                    int health = ReadMem<int>(currentPawn + 0x334); 
                    if (health > 0 && health <= 100) {
                        LogMessage("[Enemy %d] PawnPtr: %p | HP: %d\n", i, (void*)currentPawn, health);
                        // Именно отсюда (currentPawn) потом достаются кости (m_pGameSceneNode) для аима
                    }
                }
            }
            Sleep(1000); // Защита от спама в лог
        }

        if (calibratedTriggerOffset == 0) {
            if (waitingForWall) {
                if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
                    wallValues.clear();
                    wallValues.reserve(1500); // Резервируем память, чтобы не реаллоцировать вектор в цикле
                    for (int off = 0x1000; off < 0x2500; off += 4) {
                        wallValues.push_back({off, ReadMem<int>(localPawn + off)});
                    }
                    LogMessage("[DeepScan] Recorded offsets. AIM AT ENEMY and press DELETE.\n");
                    waitingForWall = false;
                    Sleep(500);
                }
            } else {
                if (GetAsyncKeyState(VK_DELETE) & 0x8000) {
                    int matches = 0;
                    for (auto& res : wallValues) {
                        int newVal = ReadMem<int>(localPawn + res.offset);
                        if ((res.value <= 0 || res.value > 100) && (newVal > 0 && newVal <= 64)) {
                            calibratedTriggerOffset = res.offset;
                            matches++;
                            LogMessage("[BINGO] Candidate: 0x%X, Old: %d, New: %d\n", res.offset, res.value, newVal);
                        }
                    }

                    if (matches == 1) {
                        LogMessage("[+] Calibration LOCKED on 0x%X.\n", calibratedTriggerOffset);
                    } else {
                        LogMessage(matches > 1 ? "[!] Too many matches (%d). Try again.\n" : "[-] NO CHANGES DETECTED.\n", matches);
                        calibratedTriggerOffset = 0;
                        waitingForWall = true;
                    }
                    Sleep(500);
                }
            }
        } else {
            if (GetAsyncKeyState(VK_CAPITAL) & 0x8000) {
                int crossId = ReadMem<int>(localPawn + calibratedTriggerOffset);
                if (crossId > 0 && crossId <= 64) {
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    Sleep(10); // В CS2 часто нужно чуть больше времени между нажатиями, проверь это
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    Sleep(150);
                }
            }
        }
        Sleep(1); // Не 10, иначе мышь будет реагировать с задержкой, но и не 0, чтобы не жрать CPU
    }

    LogMessage("[!] Thread terminating.\n");
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
