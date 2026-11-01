#include "platform/Dx11Renderer.h"

#include <dxgi1_2.h>

namespace radiance::platform
{
    using Microsoft::WRL::ComPtr;

    bool Dx11Renderer::Create(HWND window)
    {
        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL got{};
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &device_, &got, &context_);
        if (FAILED(hr))
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &device_, &got, &context_);
        if (FAILED(hr))
            return false;

        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory;
        if (FAILED(device_.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) || FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
            return false;

        // Flip model: cheaper composition and no tearing with the desktop compositor.
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        hr = factory->CreateSwapChainForHwnd(device_.Get(), window, &desc, nullptr, nullptr, &swapChain_);
        if (FAILED(hr))
        {
            desc.Scaling = DXGI_SCALING_STRETCH;  // some older drivers refuse SCALING_NONE
            hr = factory->CreateSwapChainForHwnd(device_.Get(), window, &desc, nullptr, nullptr, &swapChain_);
        }
        if (FAILED(hr))
            return false;
        factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
        CreateTarget();
        return true;
    }

    void Dx11Renderer::Destroy()
    {
        ReleaseTarget();
        swapChain_.Reset();
        context_.Reset();
        device_.Reset();
    }

    void Dx11Renderer::RequestResize(UINT width, UINT height)
    {
        pendingWidth_ = width;
        pendingHeight_ = height;
    }

    void Dx11Renderer::CreateTarget()
    {
        ComPtr<ID3D11Texture2D> backBuffer;
        if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
            device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &target_);
    }

    void Dx11Renderer::ReleaseTarget()
    {
        target_.Reset();
    }

    bool Dx11Renderer::BeginFrame(const float clear[4])
    {
        if (!swapChain_)
            return false;
        if (pendingWidth_ && pendingHeight_)
        {
            ReleaseTarget();
            swapChain_->ResizeBuffers(0, pendingWidth_, pendingHeight_, DXGI_FORMAT_UNKNOWN, 0);
            pendingWidth_ = pendingHeight_ = 0;
            CreateTarget();
        }
        if (!target_)
            return false;
        ID3D11RenderTargetView* views[] = { target_.Get() };
        context_->OMSetRenderTargets(1, views, nullptr);
        context_->ClearRenderTargetView(target_.Get(), clear);
        return true;
    }

    void Dx11Renderer::Present(bool vsync)
    {
        if (!swapChain_)
            return;
        const HRESULT hr = swapChain_->Present(vsync ? 1 : 0, 0);
        occluded_ = (hr == DXGI_STATUS_OCCLUDED);
    }
}
