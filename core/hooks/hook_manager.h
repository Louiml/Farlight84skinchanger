#pragma once

#include <string>

namespace fl
{
    class HookManager
    {
    public:
        static bool Initialize();
        static bool Create(const std::string& name, void* target, void* detour, void** original);
        static void Remove(void* target);
        static void Shutdown();
    };
}
