#pragma once

#include <windows.h>

#include <cstdint>

namespace fl
{
    struct ModuleInfo
    {
        uintptr_t base = 0;
        size_t size = 0;
    };

    ModuleInfo GetModule(const char* name);
    ModuleInfo GetSelfModule();
    uintptr_t FindPattern(const ModuleInfo& module, const char* idaPattern);
}
