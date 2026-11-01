// Notices the supported games opening and closing by looking at the running
// programs. The app polls it on a timer and when the foreground window changes.
#include "services/Services.h"

#include "core/Catalog.h"
#include "platform/Processes.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace radiance
{
    namespace
    {
        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        class GameWatcher final : public IGameWatcher
        {
        public:
            std::vector<GameEvent> Poll() override
            {
                const std::vector<std::string> names = platform::RunningProcessNames();
                const std::set<std::string> running(names.begin(), names.end());

                std::vector<const SupportedGame*> now;
                for (const SupportedGame& game : catalog::Games())
                {
                    if (running.contains(Lower(game.processName)))
                        now.push_back(&game);
                }

                std::vector<GameEvent> events;
                for (const SupportedGame* game : running_)
                {
                    if (std::find(now.begin(), now.end(), game) == now.end())
                        events.push_back(GameEvent{ false, game });
                }
                // Keep the order games opened in, so Running() stays the first one.
                std::vector<const SupportedGame*> next;
                for (const SupportedGame* game : running_)
                {
                    if (std::find(now.begin(), now.end(), game) != now.end())
                        next.push_back(game);
                }
                for (const SupportedGame* game : now)
                {
                    if (std::find(running_.begin(), running_.end(), game) == running_.end())
                    {
                        events.push_back(GameEvent{ true, game });
                        next.push_back(game);
                    }
                }
                running_ = std::move(next);
                return events;
            }

            const SupportedGame* Running() const override
            {
                return running_.empty() ? nullptr : running_.front();
            }

        private:
            std::vector<const SupportedGame*> running_;
        };
    }

    std::unique_ptr<IGameWatcher> MakeGameWatcher()
    {
        return std::make_unique<GameWatcher>();
    }
}
