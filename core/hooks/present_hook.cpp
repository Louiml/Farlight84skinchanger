#include "present_hook.h"

#include "logging.h"
#include "hooks/hook_manager.h"
#include "menu/menu.h"

#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>

namespace
{
    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);

    constexpr int kPresentVtableIndex = 8;

    PresentFn g_originalPresent = nullptr;
    void* g_presentTarget = nullptr;
    HWND g_dummyWindow = nullptr;
    ID3D11Device* g_dummyDevice = nullptr;
    IDXGISwapChain* g_dummySwapChain = nullptr;
    bool g_loggedNonD3D11 = false;

    HWND CreateDummyWindow()
    {
        WNDCLASSA wc{};
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "FarlightDummy";
        RegisterClassA(&wc);
        return CreateWindowExA(0, "FarlightDummy", "", WS_POPUP, 0, 0, 100, 100,
                               nullptr, nullptr, wc.hInstance, nullptr);
    }

    bool CreateDummySwapChain()
    {
        g_dummyWindow = CreateDummyWindow();
        if (g_dummyWindow == nullptr)
        {
            fl::Log("present: dummy window failed (%lu)", GetLastError());
            return false;
        }

        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width = 100;
        desc.BufferDesc.Height = 100;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.RefreshRate.Numerator = 0;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 1;
        desc.OutputWindow = g_dummyWindow;
        desc.Windowed = TRUE;
        desc.SampleDesc.Count = 1;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &desc, &g_dummySwapChain, &g_dummyDevice, nullptr, nullptr);

        if (FAILED(hr))
        {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                D3D11_SDK_VERSION, &desc, &g_dummySwapChain, &g_dummyDevice, nullptr, nullptr);
        }

        if (FAILED(hr))
        {
            fl::Log("present: dummy swap chain failed (hr=0x%lX)", static_cast<unsigned long>(hr));
            return false;
        }
        return true;
    }

    void ReleaseDummy()
    {
        if (g_dummySwapChain != nullptr)
        {
            g_dummySwapChain->Release();
            g_dummySwapChain = nullptr;
        }
        if (g_dummyDevice != nullptr)
        {
            g_dummyDevice->Release();
            g_dummyDevice = nullptr;
        }
        if (g_dummyWindow != nullptr)
        {
            DestroyWindow(g_dummyWindow);
            UnregisterClassA("FarlightDummy", GetModuleHandleA(nullptr));
            g_dummyWindow = nullptr;
        }
    }

    HRESULT STDMETHODCALLTYPE PresentDetour(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
    {
        if (swapChain != nullptr && !fl::menu::IsReady())
        {
            ID3D11Device* device = nullptr;
            if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device),
                                               reinterpret_cast<void**>(&device))))
            {
                ID3D11DeviceContext* context = nullptr;
                device->GetImmediateContext(&context);
                fl::Log("present: game swap chain captured (device %p)",
                        static_cast<void*>(device));
                fl::menu::InitD3D11(swapChain, device, context);
            }
            else if (!g_loggedNonD3D11)
            {
                g_loggedNonD3D11 = true;
                fl::Log("present: swap chain is not D3D11 (DX12/Vulkan RHI) - DX12 wiring pending");
            }
        }

        fl::menu::RenderFrame();
        return g_originalPresent(swapChain, syncInterval, flags);
    }
}

namespace fl::present_hook
{
    bool Install()
    {
        if (!CreateDummySwapChain())
        {
            ReleaseDummy();
            return false;
        }

        void* const* vtable = *reinterpret_cast<void* const**>(g_dummySwapChain);
        void* present = vtable[kPresentVtableIndex];

        ReleaseDummy();

        if (present == nullptr)
        {
            fl::Log("present: vtable theft failed");
            return false;
        }
        fl::Log("present: Present located at %p", present);

        if (!HookManager::Create("IDXGISwapChain::Present", present,
                                 reinterpret_cast<void*>(&PresentDetour),
                                 reinterpret_cast<void**>(&g_originalPresent)))
        {
            return false;
        }
        g_presentTarget = present;
        return true;
    }

    void Uninstall()
    {
        if (g_presentTarget != nullptr)
        {
            HookManager::Remove(g_presentTarget);
            g_presentTarget = nullptr;
            g_originalPresent = nullptr;
        }
    }
}
