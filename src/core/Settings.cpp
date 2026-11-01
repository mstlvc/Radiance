#include "core/Settings.h"

#include "core/Json.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace radiance
{
    namespace
    {
        using json::Value;

        // ------------------------------------------------------------ colour

        Value LookToJson(const ColourLook& l)
        {
            Value v = Value::MakeObject();
            v.Set("saturation", l.saturation);
            v.Set("vibrance", l.vibrance);
            v.Set("brightness", l.brightness);
            v.Set("gamma", l.gamma);
            v.Set("contrast", l.contrast);
            v.Set("temperature", l.temperature);
            v.Set("highlights", l.highlights);
            v.Set("shadows", l.shadows);
            v.Set("blacks", l.blacks);
            v.Set("whites", l.whites);
            v.Set("shadowTint", l.shadowTint);
            v.Set("highlightTint", l.highlightTint);
            v.Set("hue", l.hue);
            v.Set("redGain", l.redGain);
            v.Set("greenGain", l.greenGain);
            v.Set("blueGain", l.blueGain);
            v.Set("vision", static_cast<int>(l.vision));
            return v;
        }

        ColourLook LookFromJson(const Value& v)
        {
            const ColourLook d;
            ColourLook l;
            l.saturation = std::clamp(v["saturation"].AsInt(d.saturation), 0, 300);
            l.vibrance = std::clamp(v["vibrance"].AsInt(d.vibrance), 0, 350);
            l.brightness = std::clamp(v["brightness"].AsInt(d.brightness), 50, 170);
            l.gamma = std::clamp(v["gamma"].AsInt(d.gamma), 50, 150);
            l.contrast = std::clamp(v["contrast"].AsInt(d.contrast), 50, 150);
            l.temperature = std::clamp(v["temperature"].AsInt(d.temperature), -100, 100);
            l.highlights = std::clamp(v["highlights"].AsInt(0), -100, 100);
            l.shadows = std::clamp(v["shadows"].AsInt(0), -100, 100);
            l.blacks = std::clamp(v["blacks"].AsInt(0), -100, 100);
            l.whites = std::clamp(v["whites"].AsInt(0), -100, 100);
            l.shadowTint = std::clamp(v["shadowTint"].AsInt(0), -100, 100);
            l.highlightTint = std::clamp(v["highlightTint"].AsInt(0), -100, 100);
            l.hue = std::clamp(v["hue"].AsInt(0), -180, 180);
            l.redGain = std::clamp(v["redGain"].AsInt(100), 50, 150);
            l.greenGain = std::clamp(v["greenGain"].AsInt(100), 50, 150);
            l.blueGain = std::clamp(v["blueGain"].AsInt(100), 50, 150);
            l.vision = static_cast<ColourVision>(std::clamp(v["vision"].AsInt(0), 0, 3));
            return l;
        }

        Value HotkeyToJson(const Hotkey& h)
        {
            Value v = Value::MakeObject();
            v.Set("modifiers", h.modifiers);
            v.Set("key", h.virtualKey);
            v.Set("enabled", h.enabled);
            return v;
        }

        Hotkey HotkeyFromJson(const Value& v, const Hotkey& fallback)
        {
            if (!v.IsObject())
                return fallback;
            Hotkey h;
            h.modifiers = static_cast<uint32_t>(v["modifiers"].AsInt(static_cast<int>(fallback.modifiers)) & 0xF);
            h.virtualKey = static_cast<uint32_t>(std::clamp(v["key"].AsInt(static_cast<int>(fallback.virtualKey)), 0, 0xFE));
            h.enabled = v["enabled"].AsBool(fallback.enabled);
            return h;
        }

        std::string ReadFile(const std::filesystem::path& file)
        {
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return {};
            std::ostringstream buffer;
            buffer << in.rdbuf();
            return buffer.str();
        }
    }

    // ---------------------------------------------------------------- AppSettings

    PanelSettings& AppSettings::PanelFor(int monitorIndex)
    {
        for (PanelSettings& p : panels)
        {
            if (p.monitorIndex == monitorIndex)
                return p;
        }
        PanelSettings fresh;
        fresh.monitorIndex = monitorIndex;
        panels.push_back(fresh);
        return panels.back();
    }

    MonitorColour& AppSettings::ColourFor(const std::string& deviceName)
    {
        for (MonitorColour& c : monitorColours)
        {
            if (c.deviceName == deviceName)
                return c;
        }
        MonitorColour fresh;
        fresh.deviceName = deviceName;
        monitorColours.push_back(fresh);
        return monitorColours.back();
    }

    const GameProfile* AppSettings::ProfileFor(const std::string& gameId) const
    {
        for (const GameProfile& p : profiles)
        {
            if (p.gameId == gameId)
                return &p;
        }
        return nullptr;
    }

    void AppSettings::SetProfile(GameProfile profile)
    {
        for (GameProfile& p : profiles)
        {
            if (p.gameId == profile.gameId)
            {
                p = std::move(profile);
                return;
            }
        }
        profiles.push_back(std::move(profile));
    }

    void AppSettings::RemoveProfile(const std::string& gameId)
    {
        std::erase_if(profiles, [&](const GameProfile& p) { return p.gameId == gameId; });
    }

    const MonitorRule* AppSettings::RuleFor(const std::string& gameId) const
    {
        for (const MonitorRule& r : monitorRules)
        {
            if (r.gameId == gameId)
                return &r;
        }
        return nullptr;
    }

    void AppSettings::SetRule(const std::string& gameId, int width, int height)
    {
        std::erase_if(monitorRules, [&](const MonitorRule& r) { return r.gameId == gameId; });
        if (width > 0 && height > 0)
            monitorRules.push_back(MonitorRule{ gameId, width, height });
    }

    // ---------------------------------------------------------------- SettingsStore

    SettingsStore::SettingsStore(std::filesystem::path file) : file_(std::move(file)) {}

    std::filesystem::path SettingsStore::DefaultFolder()
    {
        // RADIANCE_DATA_DIR points Radiance at another folder (portable copies, testing).
        wchar_t custom[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"RADIANCE_DATA_DIR", custom, MAX_PATH) > 0)
            return std::filesystem::path(custom);

        PWSTR roaming = nullptr;
        std::filesystem::path folder;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &roaming)) && roaming)
            folder = std::filesystem::path(roaming) / L"Radiance";
        else
            folder = std::filesystem::temp_directory_path() / L"Radiance";
        CoTaskMemFree(roaming);
        return folder;
    }

    uint64_t SettingsStore::Now()
    {
        return GetTickCount64();
    }

    bool SettingsStore::Load(AppSettings& s) const
    {
        const std::string text = ReadFile(file_);
        if (text.empty())
            return false;
        const std::optional<Value> parsed = json::Parse(text);
        if (!parsed || !parsed->IsObject())
            return false;
        const Value& v = *parsed;
        const AppSettings d;

        const Value& window = v["window"];
        s.windowX = window["x"].AsInt(d.windowX);
        s.windowY = window["y"].AsInt(d.windowY);
        s.lastPage = std::max(0, window["page"].AsInt(0));
        s.quickX = window["quickX"].AsInt(d.quickX);
        s.quickY = window["quickY"].AsInt(d.quickY);

        s.firstRunDone = v["firstRunDone"].AsBool(true);
        s.startWithWindows = v["startWithWindows"].AsBool(d.startWithWindows);
        s.keepDesktopComposited = v["keepDesktopComposited"].AsBool(d.keepDesktopComposited);
        s.inGamePerformance = v["inGamePerformance"].AsBool(d.inGamePerformance);
        s.autoApplyProfiles = v["autoApplyProfiles"].AsBool(d.autoApplyProfiles);
        s.theme = v["theme"].AsString() == "macos" ? "macos" : "radiance";
        s.lightMode = v["themeMode"].AsString() == "light";
        // Stored under a new name: particles became opt-in, and earlier files all said true.
        s.particles = v["backgroundParticles"].AsBool(d.particles);

        s.look = LookFromJson(v["look"]);
        s.vibranceChosen = v["vibranceChosen"].AsBool(false);
        s.eyeCare.warmth = std::clamp(v["eyeCare"]["warmth"].AsInt(0), 0, 100);
        s.eyeCare.dim = std::clamp(v["eyeCare"]["dim"].AsInt(0), 0, 80);
        s.lookGame = v["lookGame"].AsString();
        s.lookName = v["lookName"].AsString();
        s.customLooks.clear();
        for (const Value& c : v["customLooks"].Elements())
        {
            LookPreset p;
            p.name = c["name"].AsString();
            p.look = LookFromJson(c["look"]);
            if (!p.name.empty())
                s.customLooks.push_back(std::move(p));
        }

        const Value& keys = v["hotkeys"];
        s.quickPopup = HotkeyFromJson(keys["quickPopup"], d.quickPopup);
        s.openMain = HotkeyFromJson(keys["openMain"], d.openMain);
        s.screenshot = HotkeyFromJson(keys["screenshot"], d.screenshot);

        const int scaling = v["scaling"].AsInt(-1);
        s.scaling = scaling >= 0 && scaling <= static_cast<int>(ScalingMode::Integer) ? std::optional<ScalingMode>(static_cast<ScalingMode>(scaling)) : std::nullopt;
        s.monitorRulesEnabled = v["monitorRulesEnabled"].AsBool(true);
        s.monitorRules.clear();
        for (const Value& r : v["monitorRules"].Elements())
        {
            MonitorRule rule{ r["gameId"].AsString(), r["width"].AsInt(), r["height"].AsInt() };
            if (!rule.gameId.empty() && rule.width > 0 && rule.height > 0)
                s.monitorRules.push_back(std::move(rule));
        }
        s.panels.clear();
        for (const Value& p : v["panels"].Elements())
        {
            PanelSettings panel;
            panel.monitorIndex = p["monitor"].AsInt();
            panel.brightness = p["brightness"].AsInt(-1);
            panel.contrast = p["contrast"].AsInt(-1);
            panel.lowBlue = p["lowBlue"].AsInt(0);
            panel.redGain = p["redGain"].AsInt(-1);
            panel.greenGain = p["greenGain"].AsInt(-1);
            panel.originalBrightness = p["originalBrightness"].AsInt(-1);
            panel.originalContrast = p["originalContrast"].AsInt(-1);
            panel.originalRed = p["originalRed"].AsInt(-1);
            panel.originalGreen = p["originalGreen"].AsInt(-1);
            panel.originalBlue = p["originalBlue"].AsInt(-1);
            s.panels.push_back(panel);
        }
        s.monitorColours.clear();
        for (const Value& c : v["monitorColours"].Elements())
        {
            MonitorColour colour;
            colour.deviceName = c["device"].AsString();
            colour.gammaPercent = std::clamp(c["gamma"].AsInt(100), 50, 150);
            colour.warmthPercent = std::clamp(c["warmth"].AsInt(0), 0, 100);
            if (!colour.deviceName.empty())
                s.monitorColours.push_back(std::move(colour));
        }

        s.selectedGameId = v["selectedGame"].AsString();
        s.profiles.clear();
        for (const Value& p : v["profiles"].Elements())
        {
            GameProfile profile;
            profile.gameId = p["gameId"].AsString();
            profile.displayName = p["displayName"].AsString();
            profile.look = LookFromJson(p["look"]);
            profile.lastUpdated = p["lastUpdated"].AsInt64();
            if (!profile.gameId.empty())
                s.profiles.push_back(std::move(profile));
        }
        s.pendingGameOfferId = v["pendingGameOffer"].AsString();
        s.declinedGameOffers.clear();
        for (const Value& id : v["declinedGameOffers"].Elements())
            s.declinedGameOffers.push_back(id.AsString());
        s.activeProfile = v["activeProfile"].AsString();
        s.lookBeforeGame = v["lookBeforeGame"].IsObject() ? std::optional<ColourLook>(LookFromJson(v["lookBeforeGame"])) : std::nullopt;
        s.gameDisplayDevice = v["gameDisplayDevice"].AsString();
        return true;
    }

    bool SettingsStore::Save(const AppSettings& s)
    {
        Value v = Value::MakeObject();
        Value window = Value::MakeObject();
        window.Set("x", s.windowX);
        window.Set("y", s.windowY);
        window.Set("page", s.lastPage);
        window.Set("quickX", s.quickX);
        window.Set("quickY", s.quickY);
        v.Set("window", std::move(window));

        v.Set("firstRunDone", s.firstRunDone);
        v.Set("startWithWindows", s.startWithWindows);
        v.Set("keepDesktopComposited", s.keepDesktopComposited);
        v.Set("inGamePerformance", s.inGamePerformance);
        v.Set("autoApplyProfiles", s.autoApplyProfiles);
        v.Set("theme", s.theme);
        v.Set("themeMode", s.lightMode ? "light" : "dark");
        v.Set("backgroundParticles", s.particles);

        v.Set("look", LookToJson(s.look));
        v.Set("vibranceChosen", s.vibranceChosen);
        Value eye = Value::MakeObject();
        eye.Set("warmth", s.eyeCare.warmth);
        eye.Set("dim", s.eyeCare.dim);
        v.Set("eyeCare", std::move(eye));
        v.Set("lookGame", s.lookGame);
        v.Set("lookName", s.lookName);
        Value custom = Value::MakeArray();
        for (const LookPreset& p : s.customLooks)
        {
            Value c = Value::MakeObject();
            c.Set("name", p.name);
            c.Set("look", LookToJson(p.look));
            custom.Push(std::move(c));
        }
        v.Set("customLooks", std::move(custom));

        Value keys = Value::MakeObject();
        keys.Set("quickPopup", HotkeyToJson(s.quickPopup));
        keys.Set("openMain", HotkeyToJson(s.openMain));
        keys.Set("screenshot", HotkeyToJson(s.screenshot));
        v.Set("hotkeys", std::move(keys));

        v.Set("scaling", s.scaling ? static_cast<int>(*s.scaling) : -1);
        v.Set("monitorRulesEnabled", s.monitorRulesEnabled);
        Value rules = Value::MakeArray();
        for (const MonitorRule& r : s.monitorRules)
        {
            Value rule = Value::MakeObject();
            rule.Set("gameId", r.gameId);
            rule.Set("width", r.width);
            rule.Set("height", r.height);
            rules.Push(std::move(rule));
        }
        v.Set("monitorRules", std::move(rules));
        Value panels = Value::MakeArray();
        for (const PanelSettings& p : s.panels)
        {
            Value panel = Value::MakeObject();
            panel.Set("monitor", p.monitorIndex);
            panel.Set("brightness", p.brightness);
            panel.Set("contrast", p.contrast);
            panel.Set("lowBlue", p.lowBlue);
            panel.Set("redGain", p.redGain);
            panel.Set("greenGain", p.greenGain);
            panel.Set("originalBrightness", p.originalBrightness);
            panel.Set("originalContrast", p.originalContrast);
            panel.Set("originalRed", p.originalRed);
            panel.Set("originalGreen", p.originalGreen);
            panel.Set("originalBlue", p.originalBlue);
            panels.Push(std::move(panel));
        }
        v.Set("panels", std::move(panels));
        Value colours = Value::MakeArray();
        for (const MonitorColour& c : s.monitorColours)
        {
            Value colour = Value::MakeObject();
            colour.Set("device", c.deviceName);
            colour.Set("gamma", c.gammaPercent);
            colour.Set("warmth", c.warmthPercent);
            colours.Push(std::move(colour));
        }
        v.Set("monitorColours", std::move(colours));

        v.Set("selectedGame", s.selectedGameId);
        Value profiles = Value::MakeArray();
        for (const GameProfile& p : s.profiles)
        {
            Value profile = Value::MakeObject();
            profile.Set("gameId", p.gameId);
            profile.Set("displayName", p.displayName);
            profile.Set("look", LookToJson(p.look));
            profile.Set("lastUpdated", p.lastUpdated);
            profiles.Push(std::move(profile));
        }
        v.Set("profiles", std::move(profiles));
        v.Set("pendingGameOffer", s.pendingGameOfferId);
        Value declined = Value::MakeArray();
        for (const std::string& id : s.declinedGameOffers)
            declined.Push(id);
        v.Set("declinedGameOffers", std::move(declined));
        v.Set("activeProfile", s.activeProfile);
        if (s.lookBeforeGame)
            v.Set("lookBeforeGame", LookToJson(*s.lookBeforeGame));
        v.Set("gameDisplayDevice", s.gameDisplayDevice);

        // Write beside the real file, then swap it in, so a crash mid-write never
        // leaves a half-written settings file behind.
        std::error_code ec;
        std::filesystem::create_directories(file_.parent_path(), ec);
        const std::filesystem::path temp = file_.wstring() + L".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out)
                return false;
            const std::string text = json::Dump(v);
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            if (!out)
                return false;
        }
        dirtySince_ = 0;
        return MoveFileExW(temp.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }

    void SettingsStore::FlushIfDue(const AppSettings& settings, bool force)
    {
        if (dirtySince_ == 0)
            return;
        if (force || Now() - dirtySince_ >= 500)
            Save(settings);
    }
}
