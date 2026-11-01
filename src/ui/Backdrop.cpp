#include "ui/Backdrop.h"

#include "ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace radiance::ui
{
    namespace
    {
        constexpr const char* kGradient = "BACKDROP_GRADIENT";
        constexpr float kDensity = 7.0e-5f;    // particles per logical px^2, what the source showed on screen
        constexpr float kSpeed = 34.0f;        // logical px per second along each axis
        constexpr float kLinkNear = 20.0f;     // fully opaque link inside this distance
        constexpr float kLinkFar = 45.0f;      // no link beyond this

        struct Glow
        {
            float x, y;      // centre, as a fraction of width / height
            float radius;    // as a fraction of the width
            float r, g, b;   // added to the ground at the centre
        };

        // Fitted to the UI source's background.png: a charcoal ground, broad glows low on
        // either side and softer ones in the top corners.
        constexpr Glow kGlows[] = {
            { 0.20f, 0.67f, 0.50f, 11, 13, 19 },
            { 0.80f, 0.67f, 0.50f, 10, 12, 18 },
            { 0.00f, 0.00f, 0.30f, 10, 11, 17 },
            { 1.00f, 0.00f, 0.30f, 8, 9, 14 },
        };
        constexpr float kGround[3] = { 17, 18, 21 };

        // The light version: a pale blue-grey ground, the glows turned into soft cool shade.
        constexpr Glow kLightGlows[] = {
            { 0.20f, 0.67f, 0.50f, -12, -10, -2 },
            { 0.80f, 0.67f, 0.50f, -11, -9, -2 },
            { 0.00f, 0.00f, 0.30f, -8, -7, -1 },
            { 1.00f, 0.00f, 0.30f, -7, -6, -1 },
        };
        constexpr float kLightGround[3] = { 240, 242, 248 };
    }

    void Backdrop::BuildGradient(ImVec2 size, platform::TextureCache& textures)
    {
        // Built at a quarter of the window's size; bilinear sampling smooths it out.
        const int w = std::max(8, static_cast<int>(size.x / 4));
        const int h = std::max(8, static_cast<int>(size.y / 4));
        std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
        const float aspect = static_cast<float>(h) / w;
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const float fx = (x + 0.5f) / w;
                const float fy = (y + 0.5f) / h;
                const float* ground = Light() ? kLightGround : kGround;
                float rgb[3] = { ground[0], ground[1], ground[2] };
                for (const Glow& glow : Light() ? std::span<const Glow>(kLightGlows) : std::span<const Glow>(kGlows))
                {
                    const float dx = fx - glow.x;
                    const float dy = (fy - glow.y) * aspect;
                    const float d = std::sqrt(dx * dx + dy * dy) / glow.radius;
                    if (d >= 1.0f)
                        continue;
                    const float k = (1.0f - d) * (1.0f - d);
                    rgb[0] += glow.r * k;
                    rgb[1] += glow.g * k;
                    rgb[2] += glow.b * k;
                }
                uint8_t* p = &pixels[(static_cast<size_t>(y) * w + x) * 4];
                for (int i = 0; i < 3; ++i)
                    p[i] = static_cast<uint8_t>(std::clamp(rgb[i], 0.0f, 255.0f));
                p[3] = 255;
            }
        }
        textures.FromPixels(kGradient, w, h, pixels.data());
        gradientSize_ = size;
        gradientLight_ = Light();
    }

    void Backdrop::Seed(ImVec2 size)
    {
        const int count = static_cast<int>(std::round((size.x / Scale()) * (size.y / Scale()) * kDensity));
        std::uniform_real_distribution<float> px(0.0f, size.x);
        std::uniform_real_distribution<float> py(0.0f, size.y);
        std::uniform_int_distribution<int> coin(0, 1);
        particles_.clear();
        for (int i = 0; i < count; ++i)
            particles_.push_back(Particle{ ImVec2(px(random_), py(random_)), ImVec2(coin(random_) ? 1.0f : -1.0f, coin(random_) ? 1.0f : -1.0f) });
        seededSize_ = size;
    }

    void Backdrop::Draw(ImDrawList* dl, ImVec2 size, platform::TextureCache& textures, bool animate, bool particles)
    {
        if (size.x < 2 || size.y < 2)
            return;
        if (Mac())
        {
            dl->AddRectFilled(ImVec2(0, 0), size, Col(c::background::filling));
        }
        else
        {
            if (gradientSize_.x != size.x || gradientSize_.y != size.y || gradientLight_ != Light())
                BuildGradient(size, textures);
            const platform::Texture& gradient = textures.FromResource(kGradient);
            if (gradient)
                dl->AddImage(gradient.Id(), ImVec2(0, 0), size);
            else
                dl->AddRectFilled(ImVec2(0, 0), size, Col(c::background::filling));
        }
        if (!particles)
            return;
        if (seededSize_.x != size.x || seededSize_.y != size.y)
            Seed(size);

        // Particles drift diagonally and bounce off the edges, picking a new slant.
        const float radius = S(3.0f);
        if (animate)
        {
            const float step = S(kSpeed) * std::min(ImGui::GetIO().DeltaTime, 0.1f);
            std::uniform_int_distribution<int> coin(0, 1);
            for (Particle& p : particles_)
            {
                p.pos.x += p.dir.x * step;
                p.pos.y += p.dir.y * step;
                if (p.pos.x - radius < 0 || p.pos.x + radius > size.x)
                {
                    p.dir.x = p.pos.x - radius < 0 ? 1.0f : -1.0f;
                    p.dir.y = coin(random_) ? -1.0f : 1.0f;
                }
                if (p.pos.y - radius < 0 || p.pos.y + radius > size.y)
                {
                    p.dir.y = p.pos.y - radius < 0 ? 1.0f : -1.0f;
                    p.dir.x = coin(random_) ? -1.0f : 1.0f;
                }
            }
        }

        // In the accent on the Radiance theme; quiet white or black on macOS's grey.
        const ImVec4 a = c::particle;
        const float strength = Mac() ? 0.45f : (Light() ? 0.8f : 1.0f);
        const float linkNear = S(kLinkNear);
        const float linkFar = S(kLinkFar);
        for (size_t i = 0; i < particles_.size(); ++i)
        {
            const ImVec2 pi = particles_[i].pos;
            dl->AddCircleFilled(pi, radius - 1.0f, Col(ImVec4(a.x, a.y, a.z, 75 / 255.f * strength)), 12);
            for (size_t j = i + 1; j < particles_.size(); ++j)
            {
                const ImVec2 pj = particles_[j].pos;
                const float dx = pj.x - pi.x;
                const float dy = pj.y - pi.y;
                const float d2 = dx * dx + dy * dy;
                if (d2 > linkFar * linkFar)
                    continue;
                const float d = std::sqrt(d2);
                const float alpha = d <= linkNear ? 1.0f : std::max(0.0f, 1.0f - (d - linkNear) / (linkFar - linkNear));
                dl->AddLine(pi, pj, Col(ImVec4(a.x, a.y, a.z, alpha * strength)), 1.0f);
                const ImU32 halo = Col(ImVec4(a.x, a.y, a.z, 30 / 255.f * strength));
                dl->AddCircleFilled(pi, radius, halo, 12);
                dl->AddCircleFilled(pj, radius, halo, 12);
            }
        }
    }
}
