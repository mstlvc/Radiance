// "Profiles": a look for each game, applied when it opens and put back when it closes.
#pragma once

#include "ui/Page.h"

#include <string>

namespace radiance::ui
{
    class ProfilesPage final : public Page
    {
    public:
        explicit ProfilesPage(PageContext& context);

        const char* Label() const override { return "Profiles"; }
        const char* Subtitle() const override { return "A look for each game - saved here, applied when it opens, put back when it closes."; }
        Icon NavIcon() const override { return Icon::Profiles; }
        void Draw(ImVec2 area) override;

    private:
        void DrawGames();
        void DrawProfile();
        void DrawDialogs();
        void SaveCurrentLookFor(const std::string& gameId);

        std::string renaming_;
        char renameBuffer_[64] = {};
    };
}
