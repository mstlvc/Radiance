#include "ui/pages/MonitorPage.h"

#include "core/Mccs.h"
#include "ui/Widgets.h"
#include "ui/pages/Shared.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace radiance::ui
{
    namespace
    {
        constexpr const char* kInputDialog = "Switch input";

        std::string Supports(const MonitorCapability& cap)
        {
            std::string out;
            const auto add = [&](const char* part) {
                if (!out.empty())
                    out += " \xC2\xB7 ";
                out += part;
            };
            if (cap.supportsBrightness) add("Brightness");
            if (cap.supportsContrast) add("Contrast");
            if (cap.supportsRgbGain || cap.supportsRedGain || cap.supportsGreenGain) add("Colour");
            if (cap.supportsInputSource) add("Input");
            return out.empty() ? (cap.refusal.empty() ? "No DDC/CI answer" : cap.refusal) : out;
        }
    }

    MonitorPage::MonitorPage(PageContext& context) : Page(context) {}

    MonitorPage::~MonitorPage()
    {
        if (probe_.valid())
            probe_.wait();
    }

    void MonitorPage::OnShow()
    {
        if (!probed_ && !probe_.valid())
            BeginProbe();
    }

    void MonitorPage::BeginProbe()
    {
        if (probe_.valid())
            return;  // already asking
        IMonitorControl* monitors = ctx_.services.monitors.get();
        probe_ = std::async(std::launch::async, [monitors] { return monitors->Probe(); });
    }

    void MonitorPage::Revert(const MonitorCapability& cap)
    {
        IMonitorControl& monitors = *ctx_.services.monitors;
        PanelSettings& panel = ctx_.settings.PanelFor(cap.index);
        if (panel.originalBrightness >= 0) monitors.Write(cap.index, mccs::kBrightness, panel.originalBrightness);
        if (panel.originalContrast >= 0) monitors.Write(cap.index, mccs::kContrast, panel.originalContrast);
        if (panel.originalRed >= 0) monitors.Write(cap.index, mccs::kRedGain, panel.originalRed);
        if (panel.originalGreen >= 0) monitors.Write(cap.index, mccs::kGreenGain, panel.originalGreen);
        if (panel.originalBlue >= 0) monitors.Write(cap.index, mccs::kBlueGain, panel.originalBlue);
        // The sliders fall back to what the probe read; make that the restored values.
        for (MonitorCapability& c : caps_)
        {
            if (c.index != cap.index)
                continue;
            if (panel.originalBrightness >= 0) c.brightness.current = panel.originalBrightness;
            if (panel.originalContrast >= 0) c.contrast.current = panel.originalContrast;
            if (panel.originalRed >= 0) c.redGain.current = panel.originalRed;
            if (panel.originalGreen >= 0) c.greenGain.current = panel.originalGreen;
            if (panel.originalBlue >= 0) c.blueGain.current = panel.originalBlue;
        }
        const int index = panel.monitorIndex;
        panel = PanelSettings{};
        panel.monitorIndex = index;
        ctx_.host.SettingsChanged();
    }

    void MonitorPage::Draw(ImVec2 area)
    {
        if (probe_.valid() && probe_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            caps_ = probe_.get();
            probed_ = true;
            selected_ = std::clamp(selected_, 0, std::max(0, static_cast<int>(caps_.size()) - 1));
        }
        TwoPanels(area, 0.44f, [&] { DrawMonitors(); }, [&] { DrawControls(); });
        if (openInputDialog_)
        {
            OpenDialog(kInputDialog);
            openInputDialog_ = false;
        }
        DrawInputDialog();
    }

    // ------------------------------------------------------------------ left: the monitors

    void MonitorPage::DrawMonitors()
    {
        SectionTitle("Monitors", "DDC/CI");
        if (!probed_)
        {
            // The UI source's progress ring, turning while the monitors answer.
            const float w = ImGui::GetContentRegionAvail().x;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(w, S(110)));
            const float t = static_cast<float>(std::fmod(ImGui::GetTime(), 1.2) / 1.2);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 centre(at.x + w * 0.5f, at.y + S(44));
            ProgressRing(dl, centre, S(26), 0.25f + 0.5f * t);
            const char* text = "Asking your monitors what they support\xE2\x80\xA6";
            const float tw = Measure(Type::Small, text).x;
            PaintText(dl, Type::Small, ImVec2(at.x + (w - tw) * 0.5f, at.y + S(86)), Col(c::text::caption), text);
            return;
        }
        if (caps_.empty())
            Note("No answer", "No monitors answered - your panel may not support DDC/CI, or the cable doesn't carry it.");

        for (size_t i = 0; i < caps_.size(); ++i)
        {
            const MonitorCapability& cap = caps_[i];
            CardSpec card;
            card.title = cap.description;
            card.info = Supports(cap);
            card.selected = static_cast<int>(i) == selected_;
            card.icon = Icon::Monitor;
            card.height = 74;
            char id[32];
            std::snprintf(id, sizeof(id), "##monitor%zu", i);
            if (Card(id, card) == CardResult::Clicked)
                selected_ = static_cast<int>(i);
        }
        if (Button("Ask again##probe", ImVec2(0, 34), ButtonKind::Normal, Icon::Refresh))
        {
            probed_ = false;
            BeginProbe();
        }

        if (selected_ >= 0 && selected_ < static_cast<int>(caps_.size()) && !caps_[static_cast<size_t>(selected_)].deviceName.empty())
        {
            Separator();
            SectionTitle("This monitor's colour", "software");
            Caption("This panel only - works on every monitor, DDC/CI or not.");
            MonitorColour& colour = ctx_.settings.ColourFor(caps_[static_cast<size_t>(selected_)].deviceName);
            bool changed = false;
            changed |= Slider("##panelGamma", &colour.gammaPercent, { "Gamma", 50, 150, 100, Percent, nullptr, false });
            changed |= Slider("##panelWarmth", &colour.warmthPercent, { "Warmth", 0, 100, std::nullopt, Percent, nullptr, false });
            if (changed)
                ctx_.host.ColourChanged();
        }
    }

    // ------------------------------------------------------------------ right: the panel's own controls

    void MonitorPage::DrawControls()
    {
        if (!probed_ || caps_.empty() || selected_ < 0 || selected_ >= static_cast<int>(caps_.size()))
        {
            SectionTitle("Panel controls");
            Caption(probed_ ? "Nothing to control here - no monitor answered over DDC/CI." : "Waiting for your monitors\xE2\x80\xA6");
            return;
        }
        IMonitorControl& monitors = *ctx_.services.monitors;
        const MonitorCapability& cap = caps_[static_cast<size_t>(selected_)];
        PanelSettings& panel = ctx_.settings.PanelFor(cap.index);
        ImGui::PushID(cap.index);
        SectionTitle(cap.description.c_str(), "on the panel");
        if (!cap.AnyControl())
        {
            Caption(cap.refusal.empty() ? "This monitor didn't answer over DDC/CI." : cap.refusal);
            ImGui::PopID();
            return;
        }
        Caption("These change the monitor itself, so they stay as you leave them, even with Radiance closed.");

        // The first change to a control remembers what the monitor had, for "Put it back".
        const auto remember = [](int& original, int current) {
            if (original < 0)
                original = current;
        };
        if (cap.supportsBrightness)
        {
            int v = panel.brightness >= 0 ? panel.brightness : cap.brightness.ToPercent(cap.brightness.current);
            if (Slider("##brightness", &v, { "Brightness", 0, 100, std::nullopt, Percent, nullptr, false }))
            {
                remember(panel.originalBrightness, cap.brightness.current);
                panel.brightness = v;
                monitors.Write(cap.index, mccs::kBrightness, cap.brightness.FromPercent(v));
                ctx_.host.SettingsChanged();
            }
        }
        if (cap.supportsContrast)
        {
            int v = panel.contrast >= 0 ? panel.contrast : cap.contrast.ToPercent(cap.contrast.current);
            if (Slider("##contrast", &v, { "Contrast", 0, 100, std::nullopt, Percent, nullptr, false }))
            {
                remember(panel.originalContrast, cap.contrast.current);
                panel.contrast = v;
                monitors.Write(cap.index, mccs::kContrast, cap.contrast.FromPercent(v));
                ctx_.host.SettingsChanged();
            }
        }
        if (cap.supportsRgbGain)
        {
            int v = std::max(0, panel.lowBlue);
            if (Slider("##lowBlue", &v, { "Low blue light", 0, 100, std::nullopt, Percent,
                    "Warmer for late sessions. Lowers the panel's blue gain from wherever it already was, which every monitor understands.", false }))
            {
                remember(panel.originalBlue, cap.blueGain.current);
                panel.lowBlue = v;
                const int from = cap.blueGain.ToPercent(panel.originalBlue);
                monitors.Write(cap.index, mccs::kBlueGain, cap.blueGain.FromPercent(from * (100 - v * 7 / 10) / 100));
                ctx_.host.SettingsChanged();
            }
        }
        if (cap.supportsRedGain)
        {
            int v = panel.redGain >= 0 ? panel.redGain : cap.redGain.ToPercent(cap.redGain.current);
            if (Slider("##redGain", &v, { "Red gain", 0, 100, std::nullopt, Percent, nullptr, false }))
            {
                remember(panel.originalRed, cap.redGain.current);
                panel.redGain = v;
                monitors.Write(cap.index, mccs::kRedGain, cap.redGain.FromPercent(v));
                ctx_.host.SettingsChanged();
            }
        }
        if (cap.supportsGreenGain)
        {
            int v = panel.greenGain >= 0 ? panel.greenGain : cap.greenGain.ToPercent(cap.greenGain.current);
            if (Slider("##greenGain", &v, { "Green gain", 0, 100, std::nullopt, Percent, nullptr, false }))
            {
                remember(panel.originalGreen, cap.greenGain.current);
                panel.greenGain = v;
                monitors.Write(cap.index, mccs::kGreenGain, cap.greenGain.FromPercent(v));
                ctx_.host.SettingsChanged();
            }
        }
        if (cap.supportsInputSource)
        {
            Separator();
            SectionTitle("Input source");
            std::vector<std::string> names;
            int index = -1;
            for (size_t i = 0; i < cap.inputSources.size(); ++i)
            {
                names.push_back(mccs::InputSourceName(cap.inputSources[i]));
                if (cap.currentInputSource && *cap.currentInputSource == cap.inputSources[i])
                    index = static_cast<int>(i);
            }
            const int was = index;
            if (Combo("##inputSource", &index, names) && index >= 0 && index != was)
            {
                inputMonitor_ = cap.index;
                inputValue_ = cap.inputSources[static_cast<size_t>(index)];
                openInputDialog_ = true;
            }
            Caption("Names follow the DDC/CI standard; a few monitors number their inputs their own way.");
        }

        const std::string problem = monitors.LastProblem();
        if (!problem.empty())
            Note("Not taken", problem);

        Separator();
        if (Button("Put it back##revert", ImVec2(0, 34), ButtonKind::Normal, Icon::Reset, panel.HasOriginals()))
            Revert(cap);
        Tip("Puts back what the monitor had before Radiance touched it.");
        ImGui::PopID();
    }

    void MonitorPage::DrawInputDialog()
    {
        if (!BeginDialog(kInputDialog, 400))
            return;
        const std::string name = mccs::InputSourceName(inputValue_);
        SectionTitle(("Switch to " + name + "?").c_str());
        Caption("This monitor will show whatever is on " + name + ". To come back, use the monitor's own buttons, or switch back from Radiance on another screen.");
        const float half = Split(2);
        const bool cancel = Button("Cancel##input", ImVec2(half, 34)) || ImGui::IsKeyPressed(ImGuiKey_Escape);
        ImGui::SameLine();
        const bool go = Button("Switch##input", ImVec2(half, 34), ButtonKind::Accent, Icon::Check);
        if (go)
        {
            ctx_.services.monitors->Write(inputMonitor_, mccs::kInputSource, inputValue_);
            for (MonitorCapability& cap : caps_)
            {
                if (cap.index == inputMonitor_)
                    cap.currentInputSource = inputValue_;
            }
            CloseDialog();
        }
        else if (cancel)
        {
            CloseDialog();
        }
        EndDialog();
    }
}
