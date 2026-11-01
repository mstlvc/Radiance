// The window's backdrop, after the UI source: a dark gradient with blue-grey glows
// in the corners, and a slow network of drifting particles that link up when close.
// The macOS theme lays the flat window colour instead of the gradient; the particles
// can be switched off in Settings.
#pragma once

#include "platform/Textures.h"

#include "imgui.h"

#include <random>
#include <vector>

namespace radiance::ui
{
    class Backdrop
    {
    public:
        void Draw(ImDrawList* dl, ImVec2 size, platform::TextureCache& textures, bool animate, bool particles);

    private:
        struct Particle
        {
            ImVec2 pos;
            ImVec2 dir;  // each axis is -1 or +1, as in the source
        };

        void BuildGradient(ImVec2 size, platform::TextureCache& textures);
        void Seed(ImVec2 size);

        ImVec2 gradientSize_{};
        bool gradientLight_ = false;
        ImVec2 seededSize_{};
        std::vector<Particle> particles_;
        std::mt19937 random_{ 1337u };
    };
}
