// Screenshots of the whole desktop. Windows' own capture leaves out the gamma ramps,
// the colour filter and NVIDIA vibrance, so the app applies the look to the pixels
// itself (colour::ApplyToPixels) before saving.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace radiance::platform
{
    struct Screenshot
    {
        int left = 0;  // virtual-screen position of pixel (0, 0)
        int top = 0;
        int width = 0;
        int height = 0;
        std::vector<uint32_t> pixels;  // BGRA, top row first, alpha opaque
    };

    // Where each display sits on the virtual screen, for applying its own ramp.
    struct DisplayArea
    {
        std::string device;
        int left = 0, top = 0, right = 0, bottom = 0;
    };

    std::optional<Screenshot> CaptureDesktop();
    std::vector<DisplayArea> DisplayAreas();

    bool SavePng(const std::filesystem::path& file, const Screenshot& shot);
    bool CopyImage(void* ownerWindow, const Screenshot& shot);

    // Pictures\Radiance, created if needed; empty when Windows has no Pictures folder.
    std::filesystem::path ScreenshotFolder();
}
