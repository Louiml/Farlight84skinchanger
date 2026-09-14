#include "hook_manager.h"

#include "logging.h"

#include <MinHook.h>

namespace fl
{
    bool HookManager::Initialize()
    {
        const MH_STATUS status = MH_Initialize();
        if (status != MH_OK)
        {
            Log("MinHook: initialize failed (%d)", static_cast<int>(status));
            return false;
        }
        Log("MinHook: initialized");
        return true;
    }

    bool HookManager::Create(const std::string& name, void* target, void* detour, void** original)
    {
        if (target == nullptr)
        {
            Log("hook '%s': target is null", name.c_str());
            return false;
        }
        const MH_STATUS created = MH_CreateHook(target, detour, original);
        if (created != MH_OK)
        {
            Log("hook '%s': create failed (%d)", name.c_str(), static_cast<int>(created));
            return false;
        }
        const MH_STATUS enabled = MH_EnableHook(target);
        if (enabled != MH_OK)
        {
            Log("hook '%s': enable failed (%d)", name.c_str(), static_cast<int>(enabled));
            return false;
        }
        Log("hook '%s': installed at %p", name.c_str(), target);
        return true;
    }

    void HookManager::Remove(void* target)
    {
        if (target != nullptr)
        {
            MH_DisableHook(target);
            MH_RemoveHook(target);
        }
    }

    void HookManager::Shutdown()
    {
        MH_Uninitialize();
        Log("MinHook: shut down");
    }
}
