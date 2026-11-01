// DDC/CI: the monitor's own brightness, contrast, RGB gains and input source.
// Probing and writing both go over the cable and are slow, so the probe runs on the
// caller's worker thread and writes go through a queue drained by a thread of our own.
// One mutex keeps the two from talking to a monitor at the same time.
#include "services/Services.h"

#include "core/Mccs.h"
#include "platform/Ddc.h"
#include "platform/Displays.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace radiance
{
    namespace
    {
        using namespace std::chrono_literals;

        // DDC/CI wants a pause between commands. Without one, about half of them are
        // dropped (measured on a Gigabyte G27F); with 50 ms they all land.
        constexpr auto kGap = 50ms;
        constexpr int kReadAttempts = 3;
        constexpr int kWriteAttempts = 2;
        constexpr auto kCapabilitiesRetry = 500ms;

        bool Generic(const std::string& description)
        {
            return description.empty() || description.find("Generic") != std::string::npos || description.find("PnP") != std::string::npos;
        }

        class MonitorControl final : public IMonitorControl
        {
        public:
            MonitorControl() : worker_([this] { Run(); }) {}

            ~MonitorControl() override
            {
                {
                    std::lock_guard lock(queueMutex_);
                    stopping_ = true;
                }
                wake_.notify_all();
                worker_.join();  // sends whatever is still queued first ("Put it back" on the way out)
            }

            std::vector<MonitorCapability> Probe() override
            {
                std::map<std::string, std::string> names;
                for (const DisplayInfo& d : platform::AttachedDisplays())
                    names[d.deviceName] = d.friendlyName;

                std::lock_guard bus(bus_);
                platform::PhysicalMonitors monitors;
                std::vector<MonitorCapability> caps;
                for (const platform::PhysicalMonitors::Monitor& m : monitors.List())
                {
                    MonitorCapability cap = Interrogate(m.handle);
                    cap.index = static_cast<int>(caps.size());
                    cap.deviceName = m.device;
                    const auto named = names.find(m.device);
                    cap.description = (Generic(m.description) && named != names.end() && !named->second.empty()) ? named->second
                                      : (m.description.empty() ? "Monitor " + std::to_string(cap.index + 1) : m.description);
                    caps.push_back(std::move(cap));
                }
                return caps;
            }

            void Write(int monitorIndex, uint8_t vcpCode, int value) override
            {
                {
                    std::lock_guard lock(queueMutex_);
                    std::erase_if(queue_, [&](const Pending& p) { return p.monitor == monitorIndex && p.vcp == vcpCode; });
                    queue_.push_back(Pending{ monitorIndex, vcpCode, value });
                }
                wake_.notify_one();
            }

            std::string LastProblem() const override
            {
                std::lock_guard lock(queueMutex_);
                return problem_;
            }

        private:
            struct Pending
            {
                int monitor;
                uint8_t vcp;
                int value;
            };

            // Everything below talks to a monitor: call with bus_ held.
            void Pace()
            {
                std::this_thread::sleep_until(lastCommand_ + kGap);
            }

            std::optional<platform::VcpReading> Read(void* monitor, uint8_t code)
            {
                for (int attempt = 0; attempt < kReadAttempts; ++attempt)
                {
                    Pace();
                    const std::optional<platform::VcpReading> r = platform::ReadVcp(monitor, code);
                    lastCommand_ = std::chrono::steady_clock::now();
                    if (r)
                        return r;
                }
                return std::nullopt;
            }

            bool Send(void* monitor, uint8_t code, int value)
            {
                for (int attempt = 0; attempt < kWriteAttempts; ++attempt)
                {
                    Pace();
                    const bool ok = platform::WriteVcp(monitor, code, value);
                    lastCommand_ = std::chrono::steady_clock::now();
                    if (ok)
                        return true;
                }
                return false;
            }

            // The capabilities reply takes about a second and now and then comes back
            // empty; try again after giving the monitor a longer moment each time.
            std::string Capabilities(void* monitor)
            {
                for (int attempt = 0; attempt < 3; ++attempt)
                {
                    if (attempt)
                        std::this_thread::sleep_for(kCapabilitiesRetry * attempt);
                    Pace();
                    std::string text = platform::CapabilitiesOf(monitor);
                    lastCommand_ = std::chrono::steady_clock::now();
                    if (!text.empty())
                        return text;
                }
                return {};
            }

            MonitorCapability Interrogate(void* monitor)
            {
                MonitorCapability cap;
                const std::map<uint8_t, std::vector<uint8_t>> listed = mccs::ParseVcp(Capabilities(monitor));
                // Monitors that don't give a capabilities string can still answer reads.
                const auto offered = [&](uint8_t code) { return listed.empty() || listed.contains(code); };
                bool answered = false;
                const auto read = [&](uint8_t code, VcpRange& range) {
                    if (!offered(code))
                        return false;
                    const std::optional<platform::VcpReading> r = Read(monitor, code);
                    answered |= r.has_value();
                    if (!r || r->maximum <= 0 || r->current < 0 || r->current > r->maximum)
                        return false;
                    range = VcpRange{ 0, r->maximum, r->current };
                    return true;
                };
                cap.supportsBrightness = read(mccs::kBrightness, cap.brightness);
                cap.supportsContrast = read(mccs::kContrast, cap.contrast);
                cap.supportsRedGain = read(mccs::kRedGain, cap.redGain);
                cap.supportsGreenGain = read(mccs::kGreenGain, cap.greenGain);
                cap.supportsRgbGain = read(mccs::kBlueGain, cap.blueGain);

                const auto inputs = listed.find(mccs::kInputSource);
                if (inputs != listed.end() && !inputs->second.empty())
                {
                    cap.inputSources = inputs->second;
                    cap.supportsInputSource = true;
                    if (const auto r = Read(monitor, mccs::kInputSource))
                        cap.currentInputSource = static_cast<uint8_t>(r->current & 0xFF);
                }

                if (!cap.AnyControl())
                {
                    cap.refusal = (answered || !listed.empty())
                        ? "This monitor answers over DDC/CI but offers none of the controls Radiance uses."
                        : "This monitor didn't answer over DDC/CI. Look for \"DDC/CI\" in its on-screen menu and turn it on.";
                }
                return cap;
            }

            void Run()
            {
                for (;;)
                {
                    std::vector<Pending> batch;
                    {
                        std::unique_lock lock(queueMutex_);
                        wake_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
                        if (queue_.empty())
                            return;  // stopping, nothing left to send
                        batch.swap(queue_);
                    }

                    int refused = 0;
                    {
                        std::lock_guard bus(bus_);
                        platform::PhysicalMonitors monitors;
                        const auto& list = monitors.List();
                        for (const Pending& p : batch)
                        {
                            if (p.monitor < 0 || p.monitor >= static_cast<int>(list.size()))
                            {
                                ++refused;
                                continue;
                            }
                            if (!Send(list[static_cast<size_t>(p.monitor)].handle, p.vcp, p.value))
                                ++refused;
                        }
                    }
                    {
                        std::lock_guard lock(queueMutex_);
                        problem_ = refused == 0 ? std::string()
                                                : "The monitor didn't take " + std::string(refused == 1 ? "a change" : "some changes") +
                                                      ". Check that DDC/CI is on in its menu, then try again.";
                    }
                }
            }

            std::mutex bus_;  // one conversation with the monitors at a time
            std::chrono::steady_clock::time_point lastCommand_{};  // guarded by bus_

            mutable std::mutex queueMutex_;
            std::condition_variable wake_;
            std::vector<Pending> queue_;
            std::string problem_;
            bool stopping_ = false;

            std::thread worker_;  // last: starts once everything above exists
        };
    }

    std::unique_ptr<IMonitorControl> MakeMonitorControl()
    {
        return std::make_unique<MonitorControl>();
    }
}
