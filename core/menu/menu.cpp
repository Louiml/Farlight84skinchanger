#include "menu.h"

#include "config/config.h"
#include "logging.h"

#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include <string>
#include <unordered_map>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    IDXGISwapChain* g_swapChain = nullptr;
    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;
    HWND g_hwnd = nullptr;
    WNDPROC g_originalWndProc = nullptr;
    fl::Config* g_config = nullptr;
    std::string g_configPath;
    bool g_ready = false;
    bool g_visible = false;

    char g_heroId[64]{};
    char g_heroSkin[64]{};
    char g_weaponId[64]{};
    char g_weaponSkin[64]{};
    char g_vehicleId[64]{};
    char g_vehicleSkin[64]{};

    LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (g_ready && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam) != 0)
        {
            return 1;
        }
        return CallWindowProcW(g_originalWndProc, hwnd, msg, wParam, lParam);
    }

    void DrawIdSkinEditor(const char* label, char* idBuf, char* skinBuf,
                          std::unordered_map<std::string, std::string>& target)
    {
        ImGui::InputText("Item ID", idBuf, IM_ARRAYSIZE(idBuf));
        ImGui::InputText("Skin ID", skinBuf, IM_ARRAYSIZE(skinBuf));
        ImGui::SameLine();
        if (ImGui::Button("Add") && idBuf[0] != '\0' && skinBuf[0] != '\0')
        {
            target[idBuf] = skinBuf;
            idBuf[0] = '\0';
            skinBuf[0] = '\0';
        }

        if (ImGui::BeginTable(label, 3, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Item ID");
            ImGui::TableSetupColumn("Skin ID");
            ImGui::TableSetupColumn("");
            ImGui::TableHeadersRow();
            for (auto it = target.begin(); it != target.end();)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(it->first.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(it->second.c_str());
                ImGui::TableNextColumn();
                ImGui::PushID(it->first.c_str());
                if (ImGui::SmallButton("Remove"))
                {
                    it = target.erase(it);
                }
                else
                {
                    ++it;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void DrawMenu()
    {
        if (!ImGui::Begin("Farlight Skin Changer", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginTabBar("tabs"))
        {
            if (ImGui::BeginTabItem("Heroes"))
            {
                ImGui::Checkbox("Enabled", &g_config->heroEnabled);
                DrawIdSkinEditor("heroes", g_heroId, g_heroSkin, g_config->heroSkins);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Weapons"))
            {
                ImGui::Checkbox("Enabled", &g_config->weaponEnabled);
                DrawIdSkinEditor("weapons", g_weaponId, g_weaponSkin, g_config->weaponSkins);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Vehicles"))
            {
                ImGui::Checkbox("Enabled", &g_config->vehicleEnabled);
                DrawIdSkinEditor("vehicles", g_vehicleId, g_vehicleSkin, g_config->vehicleSkins);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings"))
            {
                ImGui::Text("Menu key: PAGE UP (change in config.json)");
                ImGui::Spacing();
                if (ImGui::Button("Save config"))
                {
                    fl::SaveConfig(g_configPath, *g_config);
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("framework ready - skin swap activates once offsets are wired (phase 4)");
        ImGui::End();
    }
}

namespace fl::menu
{
    void SetConfig(Config* config, const std::string& configPath)
    {
        g_config = config;
        g_configPath = configPath;
    }

    bool InitD3D11(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        g_swapChain = swapChain;
        g_device = device;
        g_context = context;

        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(swapChain->GetDesc(&desc)) || desc.OutputWindow == nullptr)
        {
            Log("menu: swap chain has no output window");
            return false;
        }
        g_hwnd = desc.OutputWindow;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(device, context);

        g_originalWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubclassProc)));

        g_ready = true;
        Log("menu: initialized (D3D11, hwnd %p)", g_hwnd);
        return true;
    }

    bool IsReady()
    {
        return g_ready;
    }

    bool IsVisible()
    {
        return g_visible;
    }

    void RenderFrame()
    {
        if (!g_ready || g_config == nullptr)
        {
            return;
        }

        if (GetAsyncKeyState(static_cast<int>(g_config->menuKey)) & 1)
        {
            g_visible = !g_visible;
            if (!g_visible)
            {
                SaveConfig(g_configPath, *g_config);
            }
        }

        ImGui_ImplWin32_NewFrame();
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();

        if (g_visible)
        {
            DrawMenu();
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void Shutdown()
    {
        if (!g_ready)
        {
            return;
        }
        SaveConfig(g_configPath, *g_config);
        if (g_hwnd != nullptr && g_originalWndProc != nullptr)
        {
            SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        }
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_ready = false;
        Log("menu: shut down");
    }
}
