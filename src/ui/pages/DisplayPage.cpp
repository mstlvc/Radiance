#include "ui/pages/DisplayPage.h"

#include "core/Catalog.h"
#include "core/ShareCode.h"
#include "ui/Widgets.h"
#include "ui/pages/Shared.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>

namespace radiance::ui
{
    namespace
    {
        std::string ResourceForArt(const std::string& art)
        {
            std::string name = "LOOK_";
            for (const char ch : art)
                name += ch == '-' ? '_' : static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            return name;
        }

        std::string EyeText(int v)
        {
            return v == 0 ? "Off" : std::to_string(v) + "%";
        }
    }

    DisplayPage::DisplayPage(PageContext& context) : Page(context)
    {
        games_ = catalog::LookGames();
        const auto found = std::find(games_.begin(), games_.end(), ctx_.settings.lookGame);
        gameIndex_ = found == games_.end() ? 0 : static_cast<int>(found - games_.begin());
        RebuildLooks();
    }

    void DisplayPage::OnShow()
    {
        RebuildLooks();
    }

    void DisplayPage::RebuildLooks()
    {
        looks_.clear();
        if (gameIndex_ >= 0 && gameIndex_ < static_cast<int>(games_.size()))
        {
            for (LookPreset& p : catalog::LooksFor(games_[gameIndex_]))
                looks_.push_back(LookEntry{ std::move(p), false, -1 });
        }
        for (size_t i = 0; i < ctx_.settings.customLooks.size(); ++i)
            looks_.push_back(LookEntry{ ctx_.settings.customLooks[i], true, static_cast<int>(i) });
    }

    void DisplayPage::Changed(bool byHand)
    {
        if (byHand)
            ctx_.settings.lookName.clear();
        ctx_.host.ColourChanged();
    }

    void DisplayPage::ApplyLook(const LookPreset& preset)
    {
        AppSettings& s = ctx_.settings;
        const ColourVision keepVision = s.look.vision;
        s.look = preset.look;
        s.look.vision = keepVision;
        s.lookName = preset.name;
        if (gameIndex_ >= 0 && gameIndex_ < static_cast<int>(games_.size()))
            s.lookGame = games_[gameIndex_];
        Changed(false);
    }

    void DisplayPage::ResetAll()
    {
        AppSettings& s = ctx_.settings;
        const ColourVision keepVision = s.look.vision;
        s.look = ColourLook{};
        s.look.vibrance = ctx_.services.colour->DriverDefault();
        s.look.vision = keepVision;
        s.eyeCare = EyeCare{};
        Changed(true);
    }

    void DisplayPage::SaveCustomLook(const std::string& name)
    {
        LookPreset p;
        p.name = name;
        p.look = ctx_.settings.look;
        auto& customs = ctx_.settings.customLooks;
        const auto same = std::find_if(customs.begin(), customs.end(), [&](const LookPreset& l) { return l.name == name; });
        if (same != customs.end())
            *same = p;
        else
            customs.push_back(p);
        ctx_.settings.lookName = name;
        ctx_.host.SettingsChanged();
        RebuildLooks();
    }

    void DisplayPage::Draw(ImVec2 area)
    {
        // Dialogs open from the right panel, so they are drawn in its ID scope too.
        TwoPanels(area, 0.53f, [&] { DrawColour(); }, [&] { DrawLooks(); DrawDialogs(); });
    }

    // ------------------------------------------------------------------ left: the sliders

    void DisplayPage::DrawColour()
    {
        AppSettings& s = ctx_.settings;
        IColourEngine& engine = *ctx_.services.colour;

        // A game's profile is on screen: these sliders are its look until it closes.
        if (const GameProfile* active = s.activeProfile.empty() ? nullptr : s.ProfileFor(s.activeProfile))
        {
            Note((active->displayName + " is open").c_str(),
                "You're editing its profile: changes are saved to it. Your own look comes back when the game closes.", "Profile");
        }

        if (SaturationAndVibrance(ctx_))
            Changed(true);

        const float half = Split(2);
        const bool comparing = engine.Comparing();
        if (Button(comparing ? "Back to my look##compare" : "Compare##compare", ImVec2(half, 34), comparing ? ButtonKind::Accent : ButtonKind::Normal, Icon::Compare))
            engine.SetCompare(!comparing);
        Tip("Hold your look against neutral, so you can see what it is actually doing.");
        ImGui::SameLine();
        if (Button("Reset##colours", ImVec2(half, 34), ButtonKind::Normal, Icon::Reset))
            ResetAll();
        Tip("Put every slider back to neutral, eye care included. Colour vision stays as it is.");

        Separator();
        SectionTitle("Fine tune");
        Caption("Contrast, highlights and shadows also change how dark it looks, not just gamma.");
        bool changed = false;
        changed |= Slider("##brightness", &s.look.brightness, { "Brightness", 50, 170, 100, Percent, "Lifts or lowers the whole picture. Bright areas clip to white if you push it." });
        changed |= Slider("##gamma", &s.look.gamma, { "Gamma", 50, 150, 100, Gamma, "Mid-tones only: lifts or deepens them while black and white stay put." });
        changed |= Slider("##contrast", &s.look.contrast, { "Contrast", 50, 150, 100, Percent, "Distance between the darkest and lightest parts." });
        changed |= Slider("##temperature", &s.look.temperature, { "Temperature", -100, 100, 0, Signed, "Warm pushes toward orange, cool toward blue. Neutral is untouched." });
        changed |= Slider("##highlights", &s.look.highlights, { "Highlights", -100, 100, 0, Signed, "The brightest parts only. Pull it down to recover detail in a blown-out sky." });
        changed |= Slider("##shadows", &s.look.shadows, { "Shadows", -100, 100, 0, Signed, "The darkest parts only. Lift it to see into corners without washing the whole picture out." });
        changed |= Slider("##blacks", &s.look.blacks, { "Blacks", -100, 100, 0, Signed, "Where black sits. Pull it down for punch, lift it for a faded, filmic base." });
        changed |= Slider("##whites", &s.look.whites, { "Whites", -100, 100, 0, Signed, "Where white sits. Push it up so highlights read hot instead of grey." });
        changed |= Slider("##shadowTint", &s.look.shadowTint, { "Shadow tint", -100, 100, 0, Signed, "Warm or cool, in the dark parts only. Left is cool, right is warm." });
        changed |= Slider("##highlightTint", &s.look.highlightTint, { "Highlight tint", -100, 100, 0, Signed, "Warm or cool, in the bright parts only. A cool sky is a left pull here." });

        Separator();
        SectionTitle("Colour balance");
        changed |= Slider("##hue", &s.look.hue, { "Hue", -180, 180, 0, Degrees, "Turns every colour around the wheel. Small moves go a long way." });
        changed |= Slider("##red", &s.look.redGain, { "Red", 50, 150, 100, Percent });
        changed |= Slider("##green", &s.look.greenGain, { "Green", 50, 150, 100, Percent });
        changed |= Slider("##blue", &s.look.blueGain, { "Blue", 50, 150, 100, Percent });
        if (changed)
            Changed(true);

        Separator();
        SectionTitle("Eye care");
        Caption("Warmer and dimmer for long sessions. It sits on top of whatever look you are using.");
        bool eye = false;
        eye |= Slider("##eyeWarmth", &s.eyeCare.warmth, { "Warmth", 0, 100, std::nullopt, EyeText });
        eye |= Slider("##eyeDim", &s.eyeCare.dim, { "Screen dim", 0, 80, std::nullopt, EyeText });
        if (eye)
            ctx_.host.ColourChanged();
    }

    // ------------------------------------------------------------------ right: looks and the rest

    void DisplayPage::DrawLooks()
    {
        AppSettings& s = ctx_.settings;
        SectionTitle("Scene looks", std::to_string(looks_.size()) + (looks_.size() == 1 ? " look" : " looks"));
        if (Combo("##lookGame", &gameIndex_, games_))
        {
            s.lookGame = games_[gameIndex_];
            ctx_.host.SettingsChanged();
            RebuildLooks();
        }

        int removeCustom = -1;
        for (size_t i = 0; i < looks_.size(); ++i)
        {
            const LookEntry& entry = looks_[i];
            CardSpec card;
            card.title = entry.preset.name;
            card.tag = entry.custom ? "yours" : Percent(entry.preset.look.saturation);
            card.info = "Vibrance " + Percent(entry.preset.look.vibrance) + " \xC2\xB7 Gamma " + Gamma(entry.preset.look.gamma);
            card.selected = s.lookName == entry.preset.name && (entry.custom || s.lookGame == entry.preset.game);
            card.removable = entry.custom;
            card.height = 74;
            if (!entry.preset.art.empty())
            {
                const platform::Texture& t = ctx_.textures.FromResource(ResourceForArt(entry.preset.art));
                card.image = t ? t.Id() : nullptr;
            }
            else
            {
                card.icon = Icon::Display;
            }
            char id[32];
            std::snprintf(id, sizeof(id), "##look%zu", i);
            const CardResult r = Card(id, card);
            if (r == CardResult::Clicked)
                ApplyLook(entry.preset);
            else if (r == CardResult::Removed)
                removeCustom = entry.customIndex;
        }
        if (removeCustom >= 0 && removeCustom < static_cast<int>(s.customLooks.size()))
        {
            s.customLooks.erase(s.customLooks.begin() + removeCustom);
            ctx_.host.SettingsChanged();
            RebuildLooks();
        }

        const float half = Split(2);
        if (Button("Save look##save", ImVec2(half, 34), ButtonKind::Accent, Icon::Save))
        {
            std::snprintf(nameBuffer_, sizeof(nameBuffer_), "My look %zu", s.customLooks.size() + 1);
            OpenDialog("Save look");
        }
        Tip("Saves every slider as it is now, as one of your looks.");
        ImGui::SameLine();
        if (Button("Delete saved##deleteAll", ImVec2(half, 34), ButtonKind::Danger, Icon::Trash, !s.customLooks.empty()))
            OpenDialog("Delete all saved looks");

        Separator();
        SectionTitle("Colour vision");
        Caption("Around one man in twelve can't separate some colours. This rotates the ones you can't tell apart into ones you can.");
        const auto modes = catalog::VisionModes();
        std::vector<std::string> labels;
        int current = 0;
        for (size_t i = 0; i < modes.size(); ++i)
        {
            labels.emplace_back(modes[i].label);
            if (modes[i].mode == s.look.vision)
                current = static_cast<int>(i);
        }
        if (ChoiceGrid("##vision", &current, labels, 2, 34))
        {
            s.look.vision = modes[static_cast<size_t>(current)].mode;
            ctx_.host.ColourChanged();
        }

        Separator();
        SectionTitle("Share");
        Caption("Paste a friend's code to get their exact look, or copy yours to send.");
        bool submitted = false;
        TextField("##shareCode", codeBuffer_, sizeof(codeBuffer_), "RD-XXXXXXXXXXXXXXXXXXXXXXXXXXXX", Icon::Paste, ImGuiInputTextFlags_CharsUppercase, &submitted, Type::Mono);
        const float third = Split(3);
        if (Button("Copy mine##share", ImVec2(third, 34), ButtonKind::Normal, Icon::Copy))
        {
            const std::string code = sharecode::Encode(s.look);
            shareStatusGood_ = ctx_.host.CopyText(code);
            shareStatus_ = shareStatusGood_ ? "Copied " + code : "Couldn't reach the clipboard. Try again in a moment.";
        }
        Tip("Puts your exact look on the clipboard as a short code.");
        ImGui::SameLine();
        const bool apply = Button("Apply##share", ImVec2(third, 34), ButtonKind::Accent);
        Tip("Switch to the look in the box. A wrong code changes nothing.");
        ImGui::SameLine();
        const bool keep = Button("Keep it##share", ImVec2(third, 34));
        Tip("Saves the code in the box to your looks, so you still have it later.");
        if (apply || submitted || keep)
        {
            if (const auto look = sharecode::Decode(codeBuffer_))
            {
                if (keep)
                {
                    LookPreset p;
                    p.name = "Shared " + std::to_string(s.customLooks.size() + 1);
                    p.look = *look;
                    s.customLooks.push_back(p);
                    ctx_.host.SettingsChanged();
                    RebuildLooks();
                    shareStatus_ = "Saved to your looks as \"" + p.name + "\".";
                }
                else
                {
                    s.look = *look;
                    s.lookName.clear();
                    ctx_.host.ColourChanged();
                    shareStatus_ = "Look applied.";
                }
                shareStatusGood_ = true;
            }
            else
            {
                shareStatus_ = "That isn't a Radiance look code - nothing changed.";
                shareStatusGood_ = false;
            }
        }
        if (!shareStatus_.empty())
            Caption(shareStatus_, shareStatusGood_ ? c::text::caption : c::orange);

        Separator();
        SectionTitle("Shortcuts");
        bool keys = false;
        Caption("Quick colour popup", c::text::text_active);
        keys |= HotkeyField("##quickPopup", &s.quickPopup);
        Caption("Open main window", c::text::text_active);
        keys |= HotkeyField("##openMain", &s.openMain);
        Caption("Screenshot with colours", c::text::text_active);
        keys |= HotkeyField("##screenshot", &s.screenshot);
        if (keys)
            hotkeyProblem_ = ctx_.host.HotkeysChanged() ? "" : "Windows refused one of these shortcuts - another app may already use it.";
        if (!hotkeyProblem_.empty())
            Caption(hotkeyProblem_, c::orange);
    }

    void DisplayPage::DrawDialogs()
    {
        if (BeginDialog("Save look", 380))
        {
            SectionTitle("Save look");
            Caption("Saves every slider as it is now, under this name.");
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            bool submitted = false;
            TextField("##lookName", nameBuffer_, sizeof(nameBuffer_), "Name", Icon::Edit, 0, &submitted);
            const float half = Split(2);
            const bool cancel = Button("Cancel##lookName", ImVec2(half, 34)) || ImGui::IsKeyPressed(ImGuiKey_Escape);
            ImGui::SameLine();
            const bool save = Button("Save##lookName", ImVec2(half, 34), ButtonKind::Accent, Icon::Save, nameBuffer_[0] != 0);
            if ((save || submitted) && nameBuffer_[0])
            {
                SaveCustomLook(nameBuffer_);
                CloseDialog();
            }
            else if (cancel)
            {
                CloseDialog();
            }
            EndDialog();
        }

        if (BeginDialog("Delete all saved looks", 380))
        {
            SectionTitle("Delete all saved looks?");
            Caption("Every look you saved goes. The shipped scene looks stay.");
            const float half = Split(2);
            const bool cancel = Button("Cancel##looks", ImVec2(half, 34)) || ImGui::IsKeyPressed(ImGuiKey_Escape);
            ImGui::SameLine();
            if (Button("Delete all##looks", ImVec2(half, 34), ButtonKind::Danger, Icon::Trash))
            {
                ctx_.settings.customLooks.clear();
                ctx_.host.SettingsChanged();
                RebuildLooks();
                CloseDialog();
            }
            else if (cancel)
            {
                CloseDialog();
            }
            EndDialog();
        }
    }
}
