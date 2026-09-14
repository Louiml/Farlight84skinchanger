#include "logging.h"

#include "config/config.h"
#include "hooks/hook_manager.h"
#include "hooks/present_hook.h"
#include "menu/menu.h"
#include "sdk/scanner.h"

#include <windows.h>

#include <string>

namespace
{
    fl::Config g_config;
    std::string g_configPath;

    std::string PathNextTo(HMODULE module, const char* fileName)
    {
        char path[MAX_PATH]{};
        GetModuleFileNameA(module, path, MAX_PATH);
        const std::string s(path);
        const auto slash = s.find_last_of("\\/");
        return s.substr(0, slash + 1) + fileName;
    }

    DWORD WINAPI MainThread(LPVOID param)
    {
        const auto module = static_cast<HMODULE>(param);

        fl::InitLog(PathNextTo(module, "FarlightCore.log"));
        fl::Log("FarlightCore loaded - " __DATE__ " " __TIME__);
        fl::Log("PID=%lu module=%p", GetCurrentProcessId(), static_cast<void*>(module));

        g_configPath = PathNextTo(module, "config.json");
        fl::LoadConfig(g_configPath, g_config);
        fl::menu::SetConfig(&g_config, g_configPath);

        if (fl::HookManager::Initialize())
        {
            const auto exe = fl::GetModule(nullptr);
            const auto self = fl::GetSelfModule();
            fl::Log("exe module: base=0x%llX size=0x%llX",
                    static_cast<unsigned long long>(exe.base),
                    static_cast<unsigned long long>(exe.size));
            fl::Log("self module: base=0x%llX size=0x%llX",
                    static_cast<unsigned long long>(self.base),
                    static_cast<unsigned long long>(self.size));
            fl::Log("framework ready - installing present hook");

            if (fl::present_hook::Install())
            {
                fl::Log("menu attaches on the game's first frame - PAGE UP toggles it");
            }
        }

        return 0;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        if (CreateThread(nullptr, 0, MainThread, module, 0, nullptr) == nullptr)
        {
            return FALSE;
        }
    }
    else if (reason == DLL_PROCESS_DETACH && reserved == nullptr)
    {
        fl::menu::Shutdown();
        fl::present_hook::Uninstall();
        fl::HookManager::Shutdown();
    }
    return TRUE;
}
