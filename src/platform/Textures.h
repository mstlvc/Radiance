// PNG resources (embedded as RCDATA) decoded with WIC and uploaded as D3D11
// textures for ImGui. Loaded on first use and kept for the life of the window.
#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <string>
#include <unordered_map>

typedef void* ImTextureID;

namespace radiance::platform
{
    struct Texture
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        int width = 0;
        int height = 0;

        ImTextureID Id() const { return reinterpret_cast<ImTextureID>(view.Get()); }
        explicit operator bool() const { return view != nullptr; }
    };

    class TextureCache
    {
    public:
        void Attach(ID3D11Device* device) { device_ = device; }
        void Clear() { textures_.clear(); }

        // Resource name as declared in radiance.rc (e.g. "LOOK_RUST_BALANCED").
        // Returns an empty texture when the resource is missing or unreadable.
        const Texture& FromResource(const std::string& name);

        // Uploads (or replaces) a texture built in memory, 8-bit RGBA rows.
        const Texture& FromPixels(const std::string& name, int width, int height, const void* rgba);

    private:
        ID3D11Device* device_ = nullptr;
        std::unordered_map<std::string, Texture> textures_;
    };
}
