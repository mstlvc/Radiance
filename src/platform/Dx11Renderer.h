// The Direct3D 11 device and swap chain behind Radiance's window.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace radiance::platform
{
    class Dx11Renderer
    {
    public:
        bool Create(HWND window);
        void Destroy();

        // Call when the window's client size changes; the buffers follow on the next frame.
        void RequestResize(UINT width, UINT height);

        // Clears to the given colour and binds the back buffer. False when the
        // device was lost and the frame should be skipped.
        bool BeginFrame(const float clear[4]);
        void Present(bool vsync);

        ID3D11Device* Device() const { return device_.Get(); }
        ID3D11DeviceContext* Context() const { return context_.Get(); }

    private:
        void CreateTarget();
        void ReleaseTarget();

        Microsoft::WRL::ComPtr<ID3D11Device> device_;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
        UINT pendingWidth_ = 0;
        UINT pendingHeight_ = 0;
        bool occluded_ = false;
    };
}
