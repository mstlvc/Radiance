#include "ui/pages/SettingsPage.h"

#include "platform/Strings.h"
#include "ui/Widgets.h"
#include "ui/pages/Shared.h"

#include <algorithm>

namespace radiance::ui
{
    namespace
    {
        constexpr const char* kReleasesUrl = "https://github.com/mstlvc/Radiance/releases";

        std::string GroupedHash(const std::string& hash)
        {
            if (hash.size() < 16)
                return hash;
            return hash.substr(0, 4) + " " + hash.substr(4, 4) + " \xE2\x80\xA6 " + hash.substr(hash.size() - 8, 4) + " " + hash.substr(hash.size() - 4);
        }
    }

    SettingsPage::SettingsPage(PageContext& context) : Page(context) {}

    void SettingsPage::OnShow()
    {
        launchWithWindows_ = ctx_.host.LaunchWithWindows();
        if (hash_.empty())
            hash_ = platform::Sha256OfFile(platform::Widen(ctx_.host.ExecutablePath()));
        hashCopied_ = false;
    }

    std::string SettingsPage::DiagnosticsReport() const
    {
        const platform::MachineFacts& m = ctx_.host.Machine();
        std::string r = "Radiance " + ctx_.host.Version() + "\n";
        r += m.windows + "\n";
        r += "Graphics: " + m.gpu + (m.nvidia ? " (NVIDIA present)" : "") + "\n";
        r += "Monitors: " + std::to_string(m.monitors) + "\n";
        r += std::string("Running as administrator: ") + (m.elevated ? "yes" : "no") + "\n";
        r += "Colour engine: " + ctx_.services.colour->StatusLine() + "\n";
        for (const DisplayInfo& d : ctx_.services.displays->Displays())
        {
            r += d.deviceName + ": " + d.friendlyName + (d.primary ? " (primary)" : "");
            if (const auto mode = ctx_.services.displays->Current(d.deviceName))
                r += " - " + std::to_string(mode->width) + "x" + std::to_string(mode->height) + " @ " + std::to_string(mode->refreshHz) + " Hz";
            r += "\n";
        }
        return r;
    }

    void SettingsPage::Draw(ImVec2 area)
    {
        TwoPanels(area, 0.5f, [&] { DrawBehaviour(); }, [&] { DrawAbout(); });
    }

    void SettingsPage::DrawBehaviour()
    {
        AppSettings& s = ctx_.settings;
        SectionTitle("General");
        if (Toggle("Launch with Windows##startup", &launchWithWindows_, "Starts in the tray when Windows starts and puts your look back."))
        {
            if (ctx_.host.SetLaunchWithWindows(launchWithWindows_))
            {
                s.startWithWindows = launchWithWindows_;
                ctx_.host.SettingsChanged();
                startupNote_.clear();
            }
            else
            {
                launchWithWindows_ = !launchWithWindows_;
                startupNote_ = "Windows didn't let Radiance change its startup entry - another copy of Radiance may own it.";
            }
        }
        if (!startupNote_.empty())
            Caption(startupNote_, c::orange);
        if (Toggle("Keep desktop composited##composited", &s.keepDesktopComposited,
                s.keepDesktopComposited ? "Smoothest with the colour overlay. Turn it off if a game feels a frame behind."
                                        : "Lowest latency. Windows may briefly flash when a game takes over the screen."))
            ctx_.host.SettingsChanged();
        if (Toggle("In-game performance##inGame", &s.inGamePerformance, "While anything is full-screen, your colour is cut back or off."))
            ctx_.host.SettingsChanged();
        Separator();
        SectionTitle("Appearance");
        Caption("Radiance's own look or one after macOS, each in dark or light. Your colours on screen don't change.");
        int theme = ThemeFromName(s.theme) == ThemeId::MacOS ? 1 : 0;
        if (ChoiceGrid("##theme", &theme, { "Radiance", "macOS" }, 2))
        {
            const std::string chosen = ThemeName(theme == 1 ? ThemeId::MacOS : ThemeId::Radiance);
            if (chosen != s.theme)
            {
                s.theme = chosen;
                ctx_.host.ThemeChanged();
            }
        }
        int mode = s.lightMode ? 1 : 0;
        if (ChoiceGrid("##themeMode", &mode, { "Dark", "Light" }, 2) && (mode == 1) != s.lightMode)
        {
            s.lightMode = mode == 1;
            ctx_.host.ThemeChanged();
        }
        if (Toggle("Background particles##particles", &s.particles, "The slow network of dots drifting behind the window."))
            ctx_.host.SettingsChanged();

        Separator();
        InfoRow("Quick vibrance shortcut", s.quickPopup.Describe());
        InfoRow("Display engine", ctx_.host.Machine().nvidia ? "NVIDIA + DX11" : "DX11");

        Separator();
        SectionTitle("Updates");
        InfoRow("Installed", "Radiance " + ctx_.host.Version());
        Caption("New versions are published on GitHub.");
        if (Button("Check for updates##updates", ImVec2(0, 34), ButtonKind::Normal, Icon::ExternalLink))
            ctx_.host.OpenUrl(kReleasesUrl);

        Separator();
        SectionTitle("Diagnostics");
        Caption(diagnosticsNote_.empty() ? "What Radiance measured about this PC when it started. No personal details." : diagnosticsNote_);
        const float half = Split(2);
        if (Button("Copy diagnostics##diag", ImVec2(half, 34), ButtonKind::Normal, Icon::Copy))
            diagnosticsNote_ = ctx_.host.CopyText(DiagnosticsReport()) ? "Copied. It has no personal details in it." : "Couldn't reach the clipboard. Try again in a moment.";
        ImGui::SameLine();
        if (Button("Settings folder##folder", ImVec2(half, 34), ButtonKind::Normal, Icon::ExternalLink))
            ctx_.host.OpenSettingsFolder();
    }

    void SettingsPage::DrawAbout()
    {
        // The Radiance block: mark, name, what it does.
        {
            const float w = ImGui::GetContentRegionAvail().x;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(w, S(56)));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float tile = S(56);
            dl->AddRectFilled(at, at + ImVec2(tile, tile), Col(c::element::filling), S(c::element::rounding));
            dl->AddRect(at, at + ImVec2(tile, tile), Col(c::accent, 0.6f), S(c::element::rounding));
            DrawMark(dl, at + ImVec2(tile, tile) * 0.5f, S(30), Col(c::accent), Col(c::accent, 0.08f));
            const float textX = at.x + tile + S(16);
            PaintText(dl, Type::Brand, ImVec2(textX, at.y + S(2)), Col(c::text::text_active), "Radiance");
            const float nameW = Measure(Type::Brand, "Radiance").x;
            PaintText(dl, Type::SmallMedium, ImVec2(textX + nameW + S(8), at.y + S(7)), Col(c::accent), "v" + ctx_.host.Version());
            PaintClipped(dl, Type::Small, ImRect(ImVec2(textX, at.y + S(28)), ImVec2(at.x + w, at.y + S(46))), Col(c::text::caption),
                "Changes your monitor, never the game.");
        }

        Separator();
        SectionTitle("This PC");
        const platform::MachineFacts& m = ctx_.host.Machine();
        bool anyHdr = false;
        for (const DisplayInfo& d : ctx_.services.displays->Displays())
            anyHdr |= d.hdrActive;
        InfoRow("Windows", m.windows.empty() ? "Unknown" : m.windows);
        InfoRow("Graphics", m.gpu);
        switch (ctx_.services.colour->Driver())
        {
        case DriverState::Available: InfoRow("Driver vibrance", "NVIDIA, in use", c::green); break;
        case DriverState::DisplayNotOnNvidia: InfoRow("Driver vibrance", "No display on the NVIDIA GPU", c::orange); break;
        case DriverState::NoNvidiaCard: InfoRow("Driver vibrance", m.nvidia ? "NVIDIA driver not answering" : "Software only", c::orange); break;
        default: InfoRow("Driver vibrance", "Unknown", c::orange); break;
        }
        InfoRow("Monitors", std::to_string(m.monitors));
        InfoRow("HDR", anyHdr ? "On - Windows ignores colour changes" : "Off", anyHdr ? c::orange : c::text::text);
        InfoRow("Administrator", m.elevated ? "Yes" : "No");

        Separator();
        SectionTitle("Verify this copy", "SHA-256");
        if (hash_.empty())
        {
            Caption("Not available right now. This does not mean anything is wrong - try again after a restart.");
            return;
        }
        Caption("The fingerprint of the Radiance now running. It should match the one published with the release.");
        InfoRow("Fingerprint", GroupedHash(hash_), c::accent);
        Tip(hash_.c_str());
        const float half = Split(2);
        if (Button(hashCopied_ ? "Copied##hash" : "Copy##hash", ImVec2(half, 34), ButtonKind::Normal, hashCopied_ ? Icon::Check : Icon::Copy))
            hashCopied_ = ctx_.host.CopyText(hash_);
        ImGui::SameLine();
        if (Button("VirusTotal##hash", ImVec2(half, 34), ButtonKind::Normal, Icon::ExternalLink))
            ctx_.host.OpenUrl("https://www.virustotal.com/gui/file/" + hash_);
    }
}
