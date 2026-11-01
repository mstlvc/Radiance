#include "platform/Textures.h"

#include "platform/Strings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>

#include <vector>

namespace radiance::platform
{
    using Microsoft::WRL::ComPtr;

    namespace
    {
        bool Decode(const void* data, DWORD size, std::vector<uint8_t>& pixels, UINT& width, UINT& height)
        {
            ComPtr<IWICImagingFactory> factory;
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
                return false;
            ComPtr<IWICStream> stream;
            if (FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(static_cast<BYTE*>(const_cast<void*>(data)), size)))
                return false;
            ComPtr<IWICBitmapDecoder> decoder;
            if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder)))
                return false;
            ComPtr<IWICBitmapFrameDecode> frame;
            if (FAILED(decoder->GetFrame(0, &frame)))
                return false;
            ComPtr<IWICFormatConverter> converter;
            if (FAILED(factory->CreateFormatConverter(&converter)) ||
                FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
                return false;
            if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0)
                return false;
            pixels.resize(static_cast<size_t>(width) * height * 4);
            return SUCCEEDED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data()));
        }
    }

    const Texture& TextureCache::FromResource(const std::string& name)
    {
        if (const auto found = textures_.find(name); found != textures_.end())
            return found->second;

        Texture& texture = textures_[name];
        if (!device_)
            return texture;

        const std::wstring wide = Widen(name);
        HRSRC resource = FindResourceW(nullptr, wide.c_str(), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
        if (!resource)
            return texture;
        HGLOBAL handle = LoadResource(nullptr, resource);
        const void* data = handle ? LockResource(handle) : nullptr;
        const DWORD size = SizeofResource(nullptr, resource);
        std::vector<uint8_t> pixels;
        UINT width = 0, height = 0;
        if (!data || !Decode(data, size, pixels, width, height))
            return texture;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{ pixels.data(), width * 4, 0 };
        ComPtr<ID3D11Texture2D> tex;
        if (FAILED(device_->CreateTexture2D(&desc, &init, &tex)))
            return texture;
        if (FAILED(device_->CreateShaderResourceView(tex.Get(), nullptr, &texture.view)))
            return texture;
        texture.width = static_cast<int>(width);
        texture.height = static_cast<int>(height);
        return texture;
    }

    const Texture& TextureCache::FromPixels(const std::string& name, int width, int height, const void* rgba)
    {
        Texture& texture = textures_[name];
        texture = Texture{};
        if (!device_ || width <= 0 || height <= 0 || !rgba)
            return texture;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{ rgba, static_cast<UINT>(width * 4), 0 };
        ComPtr<ID3D11Texture2D> tex;
        if (FAILED(device_->CreateTexture2D(&desc, &init, &tex)) || FAILED(device_->CreateShaderResourceView(tex.Get(), nullptr, &texture.view)))
            return texture;
        texture.width = width;
        texture.height = height;
        return texture;
    }
}
