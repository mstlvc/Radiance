#include "ui/Shell.h"

#include "ui/Widgets.h"
#include "ui/pages/DisplayPage.h"
#include "ui/pages/Shared.h"
#include "ui/pages/MonitorPage.h"
#include "ui/pages/ProfilesPage.h"
#include "ui/pages/ResolutionPage.h"
#include "ui/pages/SettingsPage.h"
#include "ui/pages/UtilitiesPage.h"

#include <cmath>
#include <cstdio>

namespace radiance::ui
{
    using namespace ShellMetrics;

    namespace
    {
        constexpr float kFadeSpeed = 7.0f;  // the UI source fades tabs out and in; per second

        // Sidebar order; Settings sits at the foot on its own.
        constexpr std::array<PageId, 5> kTopTiles{ PageId::Display, PageId::Resolution, PageId::Monitor, PageId::Profiles, PageId::Utilities };
        constexpr std::array<PageId, 1> kFootTiles{ PageId::Settings };

        // The coloured plate behind each page's icon in the macOS sidebar, as System
        // Settings does it.
        ImVec4 PlateColour(PageId id)
        {
            switch (id)
            {
            case PageId::Display: return ImVec4(10 / 255.f, 132 / 255.f, 255 / 255.f, 1.0f);
            case PageId::Resolution: return ImVec4(94 / 255.f, 92 / 255.f, 230 / 255.f, 1.0f);
            case PageId::Monitor: return ImVec4(50 / 255.f, 173 / 255.f, 230 / 255.f, 1.0f);
            case PageId::Profiles: return ImVec4(255 / 255.f, 159 / 255.f, 10 / 255.f, 1.0f);
            default: return ImVec4(142 / 255.f, 142 / 255.f, 147 / 255.f, 1.0f);
            }
        }

        // The red / yellow / green buttons; the centres along the top of the sidebar.
        constexpr float kTrafficY = 22.0f;
        constexpr float kTrafficX[3] = { 20.0f, 40.0f, 60.0f };
        constexpr float kTrafficRadius = 6.0f;
    }

    void TwoPanels(ImVec2 area, float leftShare, const std::function<void()>& left, const std::function<void()>& right)
    {
        const float gap = S(20);
        const float leftW = std::floor((area.x - gap) * leftShare);
        const float rightW = area.x - gap - leftW;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        if (BeginPanel("##left", ImVec2(leftW, area.y)))
            left();
        EndPanel();
        ImGui::SetCursorScreenPos(ImVec2(origin.x + leftW + gap, origin.y));
        if (BeginPanel("##right", ImVec2(rightW, area.y)))
            right();
        EndPanel();
    }

    Shell::Shell(PageContext& context) : ctx_(context), quickPanel_(context)
    {
        pages_[static_cast<size_t>(PageId::Display)] = std::make_unique<DisplayPage>(context);
        pages_[static_cast<size_t>(PageId::Resolution)] = std::make_unique<ResolutionPage>(context);
        pages_[static_cast<size_t>(PageId::Monitor)] = std::make_unique<MonitorPage>(context);
        pages_[static_cast<size_t>(PageId::Profiles)] = std::make_unique<ProfilesPage>(context);
        pages_[static_cast<size_t>(PageId::Settings)] = std::make_unique<SettingsPage>(context);
        pages_[static_cast<size_t>(PageId::Utilities)] = std::make_unique<UtilitiesPage>(context);
        const int saved = context.settings.lastPage;
        if (saved >= 0 && saved < static_cast<int>(PageId::Count))
            current_ = target_ = static_cast<PageId>(saved);
    }

    void Shell::Select(PageId page)
    {
        if (page == target_)
            return;
        target_ = page;
        ctx_.settings.lastPage = static_cast<int>(page);
        ctx_.host.SettingsChanged();
    }

    bool Shell::IsCaption(ImVec2 point, ImVec2 windowSize) const
    {
        if (quick_)
            return quickPanel_.IsCaption(point, windowSize);
        if (Mac())
        {
            // The strip along the top, bar the traffic lights.
            if (point.y < 0 || point.y >= S(MacTitleBar))
                return false;
            const bool onLights = point.x < S(kTrafficX[2] + 14) && std::abs(point.y - S(kTrafficY)) < S(12);
            return !onLights;
        }
        if (point.y < 0 || point.y >= S(TopBar))
            return false;
        // The window buttons at the right end stay clickable.
        return point.x < windowSize.x - S(WindowButton * 2);
    }

    void Shell::Draw(ImVec2 size, bool drawEdge, bool animate)
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(size);
        ImGui::Begin("##radiance", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        backdrop_.Draw(dl, size, ctx_.textures, animate, ctx_.settings.particles);
        if (quick_)
        {
            if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                ctx_.host.HideWindow();
            quickPanel_.Draw(size);
            if (drawEdge)
                ImGui::GetForegroundDrawList()->AddRect(ImVec2(0, 0), size, Col(c::background::stroke), 0, 0, 1.0f);
            ImGui::End();
            return;
        }
        HandleKeys();
        if (Mac())
        {
            DrawMacSidebar(dl, size);
            DrawMacTitle(dl, size);
        }
        else
        {
            DrawChrome(dl, size);
            DrawSidebar(size);
        }
        DrawPage(size);
        DrawKeepDialog();

        if (drawEdge)
            ImGui::GetForegroundDrawList()->AddRect(ImVec2(0, 0), size, Col(c::background::stroke), 0, 0, 1.0f);
        ImGui::End();
    }

    // A resolution or scaling change waits here for a yes; when the countdown runs
    // out (the screen may be black) the display service puts it back.
    void Shell::DrawKeepDialog()
    {
        IDisplayService& displays = *ctx_.services.displays;
        displays.Tick();
        const std::optional<PendingChange> pending = displays.Pending();
        constexpr const char* kId = "Keep display";
        if (pending && !ImGui::IsPopupOpen(kId))
            OpenDialog(kId);
        if (!BeginDialog(kId, 400))
            return;
        if (!pending)
        {
            CloseDialog();
            EndDialog();
            return;
        }
        SectionTitle("Keep this display setting?");
        const int left = pending->secondsLeft;
        Caption("Now showing " + pending->description + ". Going back in " + std::to_string(left) + (left == 1 ? " second" : " seconds") +
            " unless you keep it.");
        const float half = Split(2);
        const bool goBack = Button("Go back##keep", ImVec2(half, 34), ButtonKind::Normal, Icon::Reset) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        ImGui::SameLine();
        const bool keep = Button("Keep##keep", ImVec2(half, 34), ButtonKind::Accent, Icon::Check);
        if (keep)
        {
            displays.Keep();
            CloseDialog();
        }
        else if (goBack)
        {
            displays.Revert();
            CloseDialog();
        }
        EndDialog();
    }

    void Shell::HandleKeys()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput || IsCapturingHotkey() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
            return;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            ctx_.host.HideWindow();
            return;
        }
        const int count = static_cast<int>(PageId::Count);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Tab))
        {
            Select(static_cast<PageId>((static_cast<int>(target_) + (io.KeyShift ? count - 1 : 1)) % count));
            return;
        }
        if (io.KeyCtrl)
        {
            for (int i = 0; i < count; ++i)
            {
                if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i), false))
                    Select(static_cast<PageId>(i));
            }
        }
    }

    void Shell::DrawChrome(ImDrawList* dl, ImVec2 size)
    {
        // The two translucent bars of the UI source: across the top and down the left.
        const float rounding = S(c::background::rounding);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(size.x, S(TopBar)), Col(c::element::panel), rounding, ImDrawFlags_RoundCornersTop);
        dl->AddRectFilled(ImVec2(0, S(TopBar)), ImVec2(S(Sidebar), size.y), Col(c::element::panel), rounding, ImDrawFlags_RoundCornersBottomLeft);

        // The Radiance mark in the corner square.
        DrawMark(dl, ImVec2(S(Sidebar) * 0.5f, S(TopBar) * 0.5f), S(26), Col(c::accent), Col(c::accent, 0.08f));

        // Page title and its one-line description.
        const Page& page = *pages_[static_cast<size_t>(current_)];
        const float x = S(ContentLeft);
        const float titleH = TextHeight(Type::Title);
        const float subH = TextHeight(Type::Small);
        const float top = std::round((S(TopBar) - titleH - subH) * 0.5f);
        const float alpha = pageAlpha_;
        PaintText(dl, Type::Title, ImVec2(x, top), Col(c::text::text_active, alpha), page.Label());
        PaintClipped(dl, Type::Small, ImRect(ImVec2(x, top + titleH), ImVec2(size.x - S(WindowButton * 2 + 20), top + titleH + subH)), Col(c::text::text, alpha), page.Subtitle());

        // The window buttons.
        const ImVec2 button(S(WindowButton), S(TopBar));
        ImGui::SetCursorScreenPos(ImVec2(size.x - button.x * 2, 0));
        if (IconButton("##minimise", Icon::Minimise, button))
            ctx_.host.MinimiseWindow();
        ImGui::SetCursorScreenPos(ImVec2(size.x - button.x, 0));
        if (IconButton("##close", Icon::Close, button))
            ctx_.host.HideWindow();
        Tip("Close to the tray - Radiance keeps your colours on");
    }

    void Shell::DrawSidebar(ImVec2 size)
    {
        const ImVec2 tile(S(Tile), S(Tile));
        const float x = std::round((S(Sidebar) - tile.x) * 0.5f);
        float y = S(ContentTop);
        const auto place = [&](PageId id, float top) {
            Page& page = *pages_[static_cast<size_t>(id)];
            char label[48];
            std::snprintf(label, sizeof(label), "##tile%d", static_cast<int>(id));
            ImGui::SetCursorScreenPos(ImVec2(x, std::round(top)));
            if (PageTile(label, page.NavIcon(), target_ == id, tile))
                Select(id);
            Tip(page.Label());
        };
        for (PageId id : kTopTiles)
        {
            place(id, y);
            y += S(TilePitch);
        }
        float foot = std::max(y, size.y - S(ContentInset) - tile.y - S(TilePitch) * (kFootTiles.size() - 1));
        for (PageId id : kFootTiles)
        {
            place(id, foot);
            foot += S(TilePitch);
        }
    }

    // ------------------------------------------------------------------ the macOS layout

    ImVec2 Shell::ContentOrigin() const
    {
        return Mac() ? ImVec2(S(MacSidebar + 20), S(MacTitleBar + 14)) : ImVec2(S(ContentLeft), S(ContentTop));
    }

    bool Shell::MacRowButton(PageId id, float y, ImVec2 size)
    {
        Page& page = *pages_[static_cast<size_t>(id)];
        const ImRect bb(ImVec2(S(10), y), ImVec2(S(MacSidebar - 10), y + S(MacRow)));
        char label[48];
        std::snprintf(label, sizeof(label), "##row%d", static_cast<int>(id));
        ImGui::SetCursorScreenPos(bb.Min);
        const bool pressed = ImGui::InvisibleButton(label, bb.GetSize());
        const bool hovered = ImGui::IsItemHovered();
        const bool selected = target_ == id;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (selected)
            dl->AddRectFilled(bb.Min, bb.Max, Col(c::sidebar::selection), S(6));
        else if (hovered)
            dl->AddRectFilled(bb.Min, bb.Max, Col(c::sidebar::hover), S(6));

        const float plate = S(22);
        const ImVec2 plateMin(bb.Min.x + S(6), std::round(bb.GetCenter().y - plate * 0.5f));
        dl->AddRectFilled(plateMin, plateMin + ImVec2(plate, plate), Col(PlateColour(id)), S(5));
        const float g = S(15);
        const ImVec2 pc = plateMin + ImVec2(plate, plate) * 0.5f;
        DrawIcon(dl, page.NavIcon(), ImRect(pc - ImVec2(g, g) * 0.5f, pc + ImVec2(g, g) * 0.5f), Col(ImVec4(1, 1, 1, 1)));

        const float textX = plateMin.x + plate + S(10);
        PaintClipped(dl, Type::BodyRegular, ImRect(ImVec2(textX, bb.Min.y), ImVec2(bb.Max.x - S(6), bb.Max.y)),
            Col(selected ? c::sidebar::selectedText : c::sidebar::text), page.Label());
        (void)size;
        return pressed;
    }

    void Shell::DrawMacSidebar(ImDrawList* dl, ImVec2 size)
    {
        const float w = S(MacSidebar);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(w, size.y), Col(c::sidebar::filling), S(c::background::rounding), ImDrawFlags_RoundCornersLeft);
        dl->AddRectFilled(ImVec2(w - 1, 0), ImVec2(w, size.y), Col(c::sidebar::divider));

        // Traffic lights: close sends Radiance to the tray, minimise minimises; the window
        // can't be resized, so the green one is greyed out as macOS does.
        const ImVec4 lights[3] = {
            ImVec4(255 / 255.f, 95 / 255.f, 87 / 255.f, 1.0f),
            ImVec4(254 / 255.f, 188 / 255.f, 46 / 255.f, 1.0f),
            ImVec4(105 / 255.f, 105 / 255.f, 105 / 255.f, 1.0f),
        };
        const ImVec2 group(S(kTrafficX[0] - kTrafficRadius - 4), S(kTrafficY - kTrafficRadius - 4));
        const ImVec2 groupEnd(S(kTrafficX[2] + kTrafficRadius + 4), S(kTrafficY + kTrafficRadius + 4));
        const bool overGroup = ImGui::IsMouseHoveringRect(group, groupEnd);
        for (int i = 0; i < 3; ++i)
        {
            const ImVec2 centre(S(kTrafficX[i]), S(kTrafficY));
            const float r = S(kTrafficRadius);
            ImGui::SetCursorScreenPos(centre - ImVec2(r, r));
            char id[16];
            std::snprintf(id, sizeof(id), "##light%d", i);
            const bool pressed = ImGui::InvisibleButton(id, ImVec2(r * 2, r * 2)) && i < 2;
            dl->AddCircleFilled(centre, r, Col(lights[i]), 0);
            dl->AddCircle(centre, r - 0.5f, Col(ImVec4(0, 0, 0, 0.2f)), 0, 1.0f);
            if (overGroup && i < 2)
            {
                // The glyphs macOS shows while the pointer is over the group.
                const ImU32 glyph = Col(ImVec4(0, 0, 0, 0.55f));
                const float k = r * 0.45f;
                if (i == 0)
                {
                    dl->AddLine(centre - ImVec2(k, k), centre + ImVec2(k, k), glyph, S(1.2f));
                    dl->AddLine(centre + ImVec2(-k, k), centre + ImVec2(k, -k), glyph, S(1.2f));
                }
                else
                {
                    dl->AddLine(centre - ImVec2(k * 1.2f, 0), centre + ImVec2(k * 1.2f, 0), glyph, S(1.2f));
                }
            }
            if (pressed)
            {
                if (i == 0)
                    ctx_.host.HideWindow();
                else
                    ctx_.host.MinimiseWindow();
            }
        }

        // The Radiance mark on its own, as the Radiance theme shows it.
        const float markTop = S(MacTitleBar);
        DrawMark(dl, ImVec2(S(34), markTop + S(16)), S(28), Col(c::accent), Col(c::accent, 0.08f));

        float y = markTop + S(32) + S(20);
        for (PageId id : kTopTiles)
        {
            if (MacRowButton(id, y, size))
                Select(id);
            y += S(MacRowPitch);
        }
        float foot = std::max(y + S(12), size.y - S(14) - S(MacRowPitch) * kFootTiles.size());
        for (PageId id : kFootTiles)
        {
            if (MacRowButton(id, foot, size))
                Select(id);
            foot += S(MacRowPitch);
        }
    }

    void Shell::DrawMacTitle(ImDrawList* dl, ImVec2 size)
    {
        // The page title in the toolbar, its one-line description beside it.
        const Page& page = *pages_[static_cast<size_t>(current_)];
        const float x = S(MacSidebar + 22);
        const float titleH = TextHeight(Type::Title);
        const float top = std::round((S(MacTitleBar) - titleH) * 0.5f) + S(4);
        const float alpha = pageAlpha_;
        PaintText(dl, Type::Title, ImVec2(x, top), Col(c::text::text_active, alpha), page.Label());
        const float titleW = Measure(Type::Title, page.Label()).x;
        const float subH = TextHeight(Type::Small);
        const float subTop = top + titleH - subH - S(2);
        PaintClipped(dl, Type::Small, ImRect(ImVec2(x + titleW + S(12), subTop), ImVec2(size.x - S(20), subTop + subH)), Col(c::text::text, alpha),
            page.Subtitle());
    }

    void Shell::DrawPage(ImVec2 size)
    {
        // Fade out the page being left, swap, fade the new one in.
        const float dt = ImGui::GetIO().DeltaTime;
        if (target_ != current_)
        {
            pageAlpha_ = std::max(0.0f, pageAlpha_ - dt * kFadeSpeed);
            if (pageAlpha_ <= 0.0f)
            {
                current_ = target_;
                showPending_ = true;
            }
        }
        else
        {
            pageAlpha_ = std::min(1.0f, pageAlpha_ + dt * kFadeSpeed);
        }

        Page& page = *pages_[static_cast<size_t>(current_)];
        if (showPending_)
        {
            ctx_.services.colour->SetCompare(false);  // comparing ends when you leave the page
            page.OnShow();
            showPending_ = false;
        }

        const ImVec2 origin = ContentOrigin();
        const ImVec2 area(size.x - origin.x - S(ContentInset), size.y - origin.y - S(ContentInset));
        ImGui::SetCursorScreenPos(origin);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pageAlpha_);
        ImGui::PushID(static_cast<int>(current_));
        page.Draw(area);
        ImGui::PopID();
        ImGui::PopStyleVar();
    }
}
