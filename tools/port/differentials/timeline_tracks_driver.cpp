#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelineTracks.hpp"

namespace {
namespace fx = VS::Effects;

// The modes timeline_tracks_differential.py compares.
enum Mode : int {
    kParticle,
    kShake,
    kAdvance,
    kSource,
    kTarget,
    kOther,
    kScene,
    kBackground,
};

// The call kinds the harness records.
enum Call : int {
    kCallVisible = 1,
    kCallActor,
    kCallScene,
    kCallBackground,
    kCallCurve,
    kCallAdvance,
};

constexpr std::size_t kActors = 256;
constexpr std::size_t kMaskBits = 32;

template <typename Value> bool Read(Value& value)
{
    long long read = 0;
    if (!(std::cin >> read))
        return false;
    value = static_cast<Value>(read);
    return true;
}

using Calls = std::vector<std::vector<long long>>;

void Print(const Calls& calls)
{
    std::cout << calls.size();
    for (const auto& call : calls) {
        for (const long long word : call)
            std::cout << ' ' << word;
    }
    std::cout << '\n';
}

std::vector<long long> ShiftCall(Call kind, const fx::ColourShift& shift)
{
    return { kind, shift.mode, shift.duration, shift.red, shift.green, shift.blue };
}

// The visibility queries, colour shifts and the track advance.
class RecordingColourHost final : public fx::ColourTrackHost {
public:
    RecordingColourHost(const std::array<bool, kActors>& visible, bool more_keys)
        : visible_(visible)
        , more_keys_(more_keys)
    {
    }

    bool ActorVisible(std::uint8_t actor) override
    {
        calls.push_back({ kCallVisible, actor });
        return visible_.at(actor);
    }
    void ShiftActorColours(std::uint8_t actor, const fx::ColourShift& shift) override
    {
        calls.push_back({ kCallActor, shift.mode, shift.duration, actor, shift.red, shift.green, shift.blue });
    }
    void ShiftSceneColours(const fx::ColourShift& shift) override { calls.push_back(ShiftCall(kCallScene, shift)); }
    void ShiftBackgroundColours(const fx::ColourShift& shift) override
    {
        calls.push_back(ShiftCall(kCallBackground, shift));
    }
    bool AdvanceKey() override
    {
        calls.push_back({ kCallAdvance, 1 });
        return more_keys_;
    }

    Calls calls;

private:
    std::array<bool, kActors> visible_;
    bool more_keys_;
};

struct TrackInputs {
    std::int16_t key_index {};
    std::uint8_t key_count {};
    std::uint8_t kind {};
    std::int16_t timer {};
    std::int32_t parameter {};
    std::uint32_t particle_key {};
    std::uint32_t shake_parameters {};
    std::int32_t duration {};
};

bool Read(TrackInputs& track)
{
    return Read(track.key_index) && Read(track.key_count) && Read(track.kind) && Read(track.timer)
        && Read(track.parameter) && Read(track.particle_key) && Read(track.shake_parameters)
        && Read(track.duration);
}

// The curve duration the harness returns, recording each curve read.
fx::CurveDuration RecordingDuration(Calls& calls, std::int32_t duration)
{
    return [&calls, duration](std::uint8_t curve) {
        calls.push_back({ kCallCurve, curve });
        return duration;
    };
}

bool Track(int mode)
{
    TrackInputs track;
    if (!Read(track))
        return false;
    Calls calls;
    const auto duration = RecordingDuration(calls, track.duration);
    if (mode == kParticle) {
        const fx::ParticleKeyTiming timing = fx::ParticleKeyTimingOf(track.particle_key);
        std::cout << timing.timer << ' ' << timing.parameter << ' ';
    } else if (mode == kShake) {
        std::cout << fx::ShakeKeyTimer(track.shake_parameters, duration) << ' ';
    } else {
        const auto next = fx::AdvanceTimelineKey(track.key_index, track.key_count, track.kind);
        if (next) {
            track.key_index = next->key_index;
            if (next->setup == fx::TimelineKeySetup::kParticleEmission) {
                const fx::ParticleKeyTiming timing = fx::ParticleKeyTimingOf(track.particle_key);
                track.timer = timing.timer;
                track.parameter = timing.parameter;
            } else if (next->setup == fx::TimelineKeySetup::kCameraShake) {
                track.timer = fx::ShakeKeyTimer(track.shake_parameters, duration);
            }
        }
        std::cout << (next ? 1 : 0) << ' ' << track.key_index << ' ' << track.timer << ' ' << track.parameter << ' ';
    }
    Print(calls);
    return true;
}

bool Read(fx::CommandEndpoint& endpoint) { return Read(endpoint.type) && Read(endpoint.actor); }

bool ReadVisible(std::array<bool, kActors>& visible)
{
    for (std::size_t base = 0; base < kActors; base += kMaskBits) {
        std::uint32_t mask = 0;
        if (!Read(mask))
            return false;
        for (std::size_t bit = 0; bit < kMaskBits; ++bit)
            visible.at(base + bit) = ((mask >> bit) & 1U) != 0;
    }
    return true;
}

// A colour track's mode-specific inputs and tick.
bool Tick(int mode, const fx::ColourKey& key, std::int16_t frame, bool more, bool& running, Calls& calls)
{
    std::array<bool, kActors> visible { };
    const auto finish = [&](auto tick) {
        if (mode != kScene && mode != kBackground && !ReadVisible(visible))
            return false;
        RecordingColourHost host(visible, more);
        running = tick(host);
        calls = host.calls;
        return true;
    };
    switch (mode) {
    case kSource: {
        fx::CommandEndpoint emit_from;
        return Read(emit_from) && finish([&](fx::ColourTrackHost& host) {
            return fx::TickSourceActorColourTrack(key, frame, emit_from, host);
        });
    }
    case kTarget: {
        int all_targets = 0;
        std::size_t count = 0;
        if (!Read(all_targets) || !Read(count))
            return false;
        std::vector<fx::CommandEndpoint> targets(count);
        for (auto& target : targets) {
            if (!Read(target))
                return false;
        }
        fx::CommandEndpoint emit_to;
        return Read(emit_to) && finish([&](fx::ColourTrackHost& host) {
            return fx::TickTargetActorColourTrack(key, frame, targets, emit_to, all_targets != 0, host);
        });
    }
    case kOther: {
        std::size_t count = 0;
        if (!Read(count))
            return false;
        std::vector<std::uint8_t> others(count);
        for (auto& other : others) {
            if (!Read(other))
                return false;
        }
        return finish([&](fx::ColourTrackHost& host) { return fx::TickOtherActorColourTrack(key, frame, others, host); });
    }
    case kScene:
        return finish([&](fx::ColourTrackHost& host) { return fx::TickSceneColourTrack(key, frame, host); });
    default:
        return finish([&](fx::ColourTrackHost& host) { return fx::TickBackgroundColourTrack(key, frame, host); });
    }
}

bool Colour(int mode)
{
    fx::ColourKey key;
    std::int16_t frame = 0;
    int more = 0;
    if (!Read(key.frame) || !Read(key.duration) || !Read(key.red) || !Read(key.green) || !Read(key.blue)
        || !Read(key.mode) || !Read(frame) || !Read(more))
        return false;
    bool running = false;
    Calls calls;
    if (!Tick(mode, key, frame, more != 0, running, calls))
        return false;
    std::cout << (running ? 1 : 0) << ' ';
    Print(calls);
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        if (mode < kParticle || mode > kBackground)
            return 1;
        const bool read = mode <= kAdvance ? Track(mode) : Colour(mode);
        if (!read)
            return 1;
    }
    return 0;
}
