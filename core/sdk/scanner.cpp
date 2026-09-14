#include "scanner.h"

#include <tlhelp32.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    struct PatternByte
    {
        uint8_t value = 0;
        bool wildcard = false;
    };

    std::vector<PatternByte> ParsePattern(const char* idaPattern)
    {
        std::vector<PatternByte> bytes;
        std::string s(idaPattern);
        s.erase(std::remove(s.begin(), s.end(), ','), s.end());

        size_t i = 0;
        while (i < s.size())
        {
            if (s[i] == ' ')
            {
                i++;
                continue;
            }
            if (s[i] == '?')
            {
                bytes.push_back({0, true});
                while (i < s.size() && s[i] == '?')
                {
                    i++;
                }
                continue;
            }
            if (i + 1 >= s.size())
            {
                break;
            }
            const std::string hexByte = s.substr(i, 2);
            bytes.push_back({static_cast<uint8_t>(std::stoul(hexByte, nullptr, 16)), false});
            i += 2;
        }
        return bytes;
    }

    bool IsReadableProtection(DWORD protect)
    {
        return (protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0
            && (protect & PAGE_GUARD) == 0;
    }
}

namespace fl
{
    ModuleInfo GetModule(const char* name)
    {
        ModuleInfo info{};
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snap == INVALID_HANDLE_VALUE)
        {
            return info;
        }

        MODULEENTRY32 me{};
        me.dwSize = sizeof(me);
        if (Module32First(snap, &me))
        {
            do
            {
                if (name == nullptr || _stricmp(me.szModule, name) == 0)
                {
                    info.base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    info.size = me.modBaseSize;
                    break;
                }
            } while (Module32Next(snap, &me));
        }
        CloseHandle(snap);
        return info;
    }

    ModuleInfo GetSelfModule()
    {
        HMODULE self = nullptr;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&GetSelfModule), &self);

        char path[MAX_PATH]{};
        GetModuleFileNameA(self, path, MAX_PATH);
        const char* lastSlash = std::strrchr(path, '\\');
        return GetModule(lastSlash ? lastSlash + 1 : path);
    }

    uintptr_t FindPattern(const ModuleInfo& module, const char* idaPattern)
    {
        if (module.base == 0 || module.size == 0)
        {
            return 0;
        }

        const auto pattern = ParsePattern(idaPattern);
        if (pattern.empty())
        {
            return 0;
        }

        uintptr_t address = module.base;
        MEMORY_BASIC_INFORMATION mbi{};
        while (address < module.base + module.size)
        {
            if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) == 0)
            {
                break;
            }
            if (mbi.State == MEM_COMMIT && IsReadableProtection(mbi.Protect))
            {
                const auto regionStart = reinterpret_cast<const uint8_t*>(mbi.BaseAddress);
                const size_t regionSize = mbi.RegionSize;
                for (size_t i = 0; i + pattern.size() <= regionSize; i++)
                {
                    bool found = true;
                    for (size_t j = 0; j < pattern.size(); j++)
                    {
                        if (!pattern[j].wildcard && regionStart[i + j] != pattern[j].value)
                        {
                            found = false;
                            break;
                        }
                    }
                    if (found)
                    {
                        return reinterpret_cast<uintptr_t>(regionStart + i);
                    }
                }
            }
            address = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        }
        return 0;
    }
}
