#include "ui/pages/ProfilesPage.h"

#include "core/Catalog.h"
#include "ui/Widgets.h"
#include "ui/pages/Shared.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace radiance::ui
{
    namespace
    {
        int64_t NowSeconds()
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        std::string WhenText(int64_t seconds)
        {
            if (seconds <= 0)
                return {};
            const std::time_t t = static_cast<std::time_t>(seconds);
            std::tm local{};
            localtime_s(&local, &t);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%d %b %Y, %H:%M", &local);
            return buf;
        }
    }

    ProfilesPage::ProfilesPage(PageContext& context) : Page(context) {}

    void ProfilesPage::SaveCurrentLookFor(const std::string& gameId)
    {
        const SupportedGame* game = catalog::GameById(gameId);
        if (!game)
            return;
        GameProfile p;
        p.gameId = game->id;
        p.displayName = game->displayName;
        if (const GameProfile* existing = ctx_.settings.ProfileFor(gameId))
            p.displayName = existing->displayName;
        p.look = ctx_.settings.look;
        p.lastUpdated = NowSeconds();
        ctx_.settings.SetProfile(std::move(p));
        ctx_.host.SettingsChanged();
    }

    void ProfilesPage::Draw(ImVec2 area)
    {
        TwoPanels(area, 0.46f, [&] { DrawGames(); }, [&] { DrawProfile(); DrawDialogs(); });
    }

    // ------------------------------------------------------------------ left: the games

    void ProfilesPage::DrawGames()
    {
        AppSettings& s = ctx_.settings;
        const SupportedGame* running = ctx_.services.games->Running();
        SectionTitle("Games", std::to_string(s.profiles.size()) + " saved");

        // A game Radiance knows has just been opened: offer its look.
        if (const SupportedGame* offer = catalog::GameById(s.pendingGameOfferId); offer && offer->lookGame)
        {
            Note("Game detected", std::string(offer->displayName) + " is open. Start it on Radiance's " + offer->lookGame + " look, and switch to it every time it opens?");
            const float half = Split(2);
            if (Button("Yes, set it up##offer", ImVec2(half, 34), ButtonKind::Accent))
            {
                // The game's profile starts on its first scene look; the user's own look is
                // left alone (it comes back whenever the game closes).
                const auto looks = catalog::LooksFor(offer->lookGame);
                if (!looks.empty())
                {
                    GameProfile p;
                    p.gameId = offer->id;
                    p.displayName = offer->displayName;
                    p.look = looks.front().look;
                    p.lastUpdated = NowSeconds();
                    s.SetProfile(std::move(p));
                    s.selectedGameId = offer->id;
                }
                s.pendingGameOfferId.clear();
                ctx_.host.SettingsChanged();  // switches to it now if the game is running
            }
            ImGui::SameLine();
            if (Button("No thanks##offer", ImVec2(half, 34)))
            {
                s.declinedGameOffers.push_back(s.pendingGameOfferId);
                s.pendingGameOfferId.clear();
                ctx_.host.SettingsChanged();
            }
        }

        for (const SupportedGame& game : catalog::Games())
        {
            const GameProfile* profile = s.ProfileFor(game.id);
            const bool isRunning = running && running->id == std::string(game.id);
            CardSpec card;
            card.title = profile ? profile->displayName : game.displayName;
            card.tag = isRunning ? (s.activeProfile == game.id ? "profile on" : "running") : "";
            card.info = profile ? DescribeLook(profile->look) : "No profile yet";
            card.selected = s.selectedGameId == game.id;
            card.icon = Icon::Game;
            card.height = 70;
            char id[48];
            std::snprintf(id, sizeof(id), "##game_%s", game.id);
            if (Card(id, card) == CardResult::Clicked && s.selectedGameId != game.id)
            {
                s.selectedGameId = game.id;
                ctx_.host.SettingsChanged();
            }
        }

        Separator();
        if (Toggle("Switch automatically##auto", &s.autoApplyProfiles, "When a game with a profile opens, Radiance switches to its look, and puts yours back when it closes."))
            ctx_.host.SettingsChanged();
    }

    // ------------------------------------------------------------------ right: the chosen game's profile

    void ProfilesPage::DrawProfile()
    {
        AppSettings& s = ctx_.settings;
        // Never open on an empty panel: start with the first game in the list.
        if (!catalog::GameById(s.selectedGameId) && !catalog::Games().empty())
            s.selectedGameId = catalog::Games().front().id;
        const SupportedGame* game = catalog::GameById(s.selectedGameId);
        if (!game)
        {
            SectionTitle("Profile");
            Caption("Pick a game on the left. Set your colours on Display, then save them here and Radiance will switch to them whenever that game opens.");
            return;
        }
        const GameProfile* profile = s.ProfileFor(game->id);
        SectionTitle(profile ? profile->displayName.c_str() : game->displayName, profile ? "saved " + WhenText(profile->lastUpdated) : std::string("no profile"));

        if (profile)
        {
            const ColourLook& l = profile->look;
            InfoRow("Saturation", Percent(l.saturation));
            InfoRow("Vibrance", Percent(l.vibrance));
            InfoRow("Brightness", Percent(l.brightness));
            InfoRow("Gamma", Gamma(l.gamma));
            InfoRow("Contrast", Percent(l.contrast));
            InfoRow("Temperature", Signed(l.temperature));
            const bool tone = l.highlights || l.shadows || l.blacks || l.whites || l.shadowTint || l.highlightTint;
            InfoRow("Tone", tone ? "Adjusted" : "Neutral");
            const bool balance = l.hue || l.redGain != 100 || l.greenGain != 100 || l.blueGain != 100;
            InfoRow("Colour balance", balance ? "Adjusted" : "Neutral");
        }
        else
        {
            Caption(std::string("No profile yet. Set your colours on Display, then save them here and Radiance will switch to them whenever ") + game->displayName + " opens.");
        }

        Separator();
        if (Button(profile ? "Update to my current look##save" : "Use my current look##save", ImVec2(0, 36), ButtonKind::Accent, Icon::Save))
            SaveCurrentLookFor(game->id);
        if (profile)
        {
            const float third = Split(3);
            if (Button("Use it now##profile", ImVec2(third, 34), ButtonKind::Normal, Icon::Display))
            {
                s.look = profile->look;
                s.lookName.clear();
                ctx_.host.ColourChanged();
            }
            Tip("Puts this game's look on screen now, without waiting for the game.");
            ImGui::SameLine();
            if (Button("Rename##profile", ImVec2(third, 34), ButtonKind::Normal, Icon::Edit))
            {
                renaming_ = profile->gameId;
                std::snprintf(renameBuffer_, sizeof(renameBuffer_), "%s", profile->displayName.c_str());
                OpenDialog("Rename profile");
            }
            ImGui::SameLine();
            if (Button("Forget##profile", ImVec2(third, 34), ButtonKind::Danger, Icon::Trash))
            {
                s.RemoveProfile(game->id);
                ctx_.host.SettingsChanged();
            }
        }

        Separator();
        SectionTitle("Launch resolution");
        Caption("Switch to this resolution while the game is open. Set the list on the Resolution page.");
        std::vector<std::string> options{ "Don't change" };
        const MonitorRule* rule = s.RuleFor(game->id);
        int selected = 0;
        if (rule)
        {
            options.push_back(std::to_string(rule->width) + " \xC3\x97 " + std::to_string(rule->height));
            selected = 1;
        }
        if (Combo("##launchResolution", &selected, options) && selected == 0)
        {
            s.SetRule(game->id, 0, 0);
            ctx_.host.SettingsChanged();
        }
        if (Button("Choose on the Resolution page##goto", ImVec2(0, 34), ButtonKind::Normal, Icon::Resolution))
            ctx_.host.ShowPage(PageId::Resolution);
    }

    void ProfilesPage::DrawDialogs()
    {
        if (BeginDialog("Rename profile", 380))
        {
            SectionTitle("Rename profile");
            Caption("The name shown in your saved profiles.");
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            bool submitted = false;
            TextField("##renameProfile", renameBuffer_, sizeof(renameBuffer_), "Name", Icon::Edit, 0, &submitted);
            const float half = Split(2);
            const bool cancel = Button("Cancel##rename", ImVec2(half, 34)) || ImGui::IsKeyPressed(ImGuiKey_Escape);
            ImGui::SameLine();
            const bool save = Button("Save##rename", ImVec2(half, 34), ButtonKind::Accent, Icon::Save, renameBuffer_[0] != 0);
            if ((save || submitted) && renameBuffer_[0])
            {
                for (GameProfile& p : ctx_.settings.profiles)
                {
                    if (p.gameId == renaming_)
                        p.displayName = renameBuffer_;
                }
                ctx_.host.SettingsChanged();
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
