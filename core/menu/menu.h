#pragma once

#include <string>

struct IDXGISwapChain;
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace fl
{
    struct Config;

    namespace menu
    {
        void SetConfig(Config* config, const std::string& configPath);
        bool InitD3D11(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context);
        bool IsReady();
        bool IsVisible();
        void RenderFrame();
        void Shutdown();
    }
}
