#include "ui/Fonts.h"

#include "ui/Theme.h"

#include "misc/freetype/imgui_freetype.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <string>

namespace radiance::ui
{
    namespace
    {
        enum class Face { Regular, Medium, Bold, Mono };

        struct Step
        {
            Face face;
            float px;
        };

        constexpr std::array<Step, static_cast<size_t>(Type::Count)> kSteps{ {
            { Face::Regular, 12 },  // Small
            { Face::Medium, 12 },   // SmallMedium
            { Face::Medium, 14 },   // Body
            { Face::Regular, 14 },  // BodyRegular
            { Face::Medium, 13 },   // Button
            { Face::Medium, 16 },   // Heading
            { Face::Medium, 19 },   // Title
            { Face::Medium, 30 },   // Value
            { Face::Bold, 17 },     // Brand
            { Face::Mono, 13 },     // Mono
        } };

        std::array<ImFont*, static_cast<size_t>(Type::Count)> g_fonts{};

        struct Blob
        {
            void* data = nullptr;
            int size = 0;
        };

        // The Lexend files are RCDATA resources; they live as long as the process.
        Blob Resource(const wchar_t* name)
        {
            HRSRC res = FindResourceW(nullptr, name, MAKEINTRESOURCEW(10) /* RT_RCDATA */);
            if (!res)
                return {};
            HGLOBAL handle = LoadResource(nullptr, res);
            return Blob{ handle ? LockResource(handle) : nullptr, static_cast<int>(SizeofResource(nullptr, res)) };
        }

        // A font Windows ships, read from the Fonts folder at run time (never bundled).
        std::string WindowsFont(const wchar_t* name)
        {
            wchar_t windows[MAX_PATH]{};
            const UINT n = GetWindowsDirectoryW(windows, MAX_PATH);
            const std::filesystem::path file = std::filesystem::path(std::wstring(windows, n)) / L"Fonts" / name;
            std::error_code ec;
            return std::filesystem::exists(file, ec) ? file.string() : std::string();
        }

        std::string MonoFile()
        {
            for (const wchar_t* name : { L"CascadiaMono.ttf", L"consola.ttf", L"cour.ttf" })
            {
                if (std::string file = WindowsFont(name); !file.empty())
                    return file;
            }
            return {};
        }

        const ImWchar* GlyphRanges()
        {
            // Latin, Latin-1, Latin Extended-A, Cyrillic, and the punctuation Radiance uses.
            static const ImWchar ranges[] = {
                0x0020, 0x00FF,
                0x0100, 0x017F,
                0x0400, 0x052F,
                0x2010, 0x2027,
                0x2030, 0x203A,
                0x2190, 0x2193,
                0x2212, 0x2212,
                0x25CF, 0x25CF,
                0,
            };
            return ranges;
        }
    }

    bool BuildFonts(ImFontAtlas& atlas)
    {
        atlas.Clear();
        g_fonts.fill(nullptr);
        atlas.FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
        // The UI source's flags: auto-hinted, light hinting, nominal (em) pixel sizes.
        atlas.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting |
            ImGuiFreeTypeBuilderFlags_LoadColor | ImGuiFreeTypeBuilderFlags_Bitmap;

        const Blob regular = Resource(L"FONT_LEXEND_REGULAR");
        const Blob medium = Resource(L"FONT_LEXEND_MEDIUM");
        const Blob bold = Resource(L"FONT_LEXEND_BOLD");
        const std::string mono = MonoFile();

        // The macOS theme sets its text in Windows' own Segoe UI (Apple's SF fonts may not
        // be shipped outside Apple's platforms); Lexend stands in if a weight is missing.
        std::string systemFaces[3];
        if (Mac())
        {
            systemFaces[0] = WindowsFont(L"segoeui.ttf");
            systemFaces[1] = WindowsFont(L"seguisb.ttf");
            systemFaces[2] = WindowsFont(L"segoeuib.ttf");
        }
        const bool system = !systemFaces[0].empty() && !systemFaces[1].empty() && !systemFaces[2].empty();

        // Body goes first so it is ImGui's default font.
        constexpr std::array<Type, static_cast<size_t>(Type::Count)> kOrder{
            Type::Body, Type::Small, Type::SmallMedium, Type::BodyRegular, Type::Button, Type::Heading, Type::Title, Type::Value, Type::Brand, Type::Mono
        };
        bool any = false;
        for (const Type type : kOrder)
        {
            const Step& step = kSteps[static_cast<size_t>(type)];
            const float px = std::round(step.px * Scale());
            ImFontConfig cfg;
            cfg.OversampleH = cfg.OversampleV = 1;
            ImFont* font = nullptr;
            if (step.face == Face::Mono)
            {
                if (!mono.empty())
                    font = atlas.AddFontFromFileTTF(mono.c_str(), px, &cfg, GlyphRanges());  // the atlas owns what it read
            }
            else if (system)
            {
                const std::string& file = systemFaces[step.face == Face::Regular ? 0 : (step.face == Face::Bold ? 2 : 1)];
                font = atlas.AddFontFromFileTTF(file.c_str(), px, &cfg, GlyphRanges());
            }
            else
            {
                cfg.FontDataOwnedByAtlas = false;  // the resource outlives the atlas
                const Blob& blob = step.face == Face::Regular ? regular : (step.face == Face::Bold ? bold : medium);
                if (blob.data)
                    font = atlas.AddFontFromMemoryTTF(blob.data, blob.size, px, &cfg, GlyphRanges());
            }
            if (!font)
            {
                ImFontConfig fallback;
                fallback.SizePixels = px;
                font = atlas.AddFontDefault(&fallback);
            }
            else
            {
                any = true;
            }
            g_fonts[static_cast<size_t>(type)] = font;
        }
        atlas.Build();

        // Nominal sizing makes FontSize the em, but Lexend's glyphs span 1.25 em
        // (ascender + descender). ImGui uses FontSize as the line height, so wrapped
        // text would overlap; give each font its real line height. Glyphs are baked,
        // so drawing at FontSize still renders them 1:1.
        for (ImFont* font : g_fonts)
        {
            if (font && font->Ascent - font->Descent > font->FontSize)
                font->FontSize = std::ceil(font->Ascent - font->Descent);
        }
        ImGui::GetIO().FontDefault = g_fonts[static_cast<size_t>(Type::Body)];
        return any;
    }

    ImFont* Font(Type type)
    {
        ImFont* f = g_fonts[static_cast<size_t>(type)];
        return f ? f : ImGui::GetFont();
    }

    ImVec2 Measure(Type type, std::string_view text, float wrapWidth)
    {
        ImFont* f = Font(type);
        return f->CalcTextSizeA(f->FontSize, FLT_MAX, wrapWidth, text.data(), text.data() + text.size());
    }

    void PaintText(ImDrawList* dl, Type type, ImVec2 at, ImU32 colour, std::string_view text)
    {
        if (text.empty())
            return;
        ImFont* f = Font(type);
        dl->AddText(f, f->FontSize, ImVec2(std::round(at.x), std::round(at.y)), colour, text.data(), text.data() + text.size());
    }

    float PaintWrapped(ImDrawList* dl, Type type, ImVec2 at, float width, ImU32 colour, std::string_view text)
    {
        if (text.empty())
            return 0.0f;
        ImFont* f = Font(type);
        dl->AddText(f, f->FontSize, ImVec2(std::round(at.x), std::round(at.y)), colour, text.data(), text.data() + text.size(), width);
        return f->CalcTextSizeA(f->FontSize, FLT_MAX, width, text.data(), text.data() + text.size()).y;
    }

    void PaintClipped(ImDrawList* dl, Type type, const ImRect& box, ImU32 colour, std::string_view text, float alignX)
    {
        if (text.empty() || box.GetWidth() <= 0)
            return;
        ImFont* f = Font(type);
        const float available = box.GetWidth();
        std::string shown(text);
        float width = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, shown.c_str()).x;
        if (width > available)
        {
            const char* ellipsis = "\xE2\x80\xA6";
            const float ellipsisWidth = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, ellipsis).x;
            size_t keep = shown.size();
            while (keep > 0)
            {
                --keep;
                while (keep > 0 && (static_cast<unsigned char>(shown[keep]) & 0xC0) == 0x80)
                    --keep;
                if (f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, shown.data(), shown.data() + keep).x + ellipsisWidth <= available)
                    break;
            }
            shown = shown.substr(0, keep) + ellipsis;
            width = f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, shown.c_str()).x;
        }
        const float x = box.Min.x + std::max(0.0f, (available - width) * alignX);
        const float y = box.Min.y + (box.GetHeight() - (f->Ascent - f->Descent)) * 0.5f;
        dl->PushClipRect(box.Min, box.Max, true);
        dl->AddText(f, f->FontSize, ImVec2(std::round(x), std::round(y)), colour, shown.c_str());
        dl->PopClipRect();
    }
}
