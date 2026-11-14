#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelinePlayback.hpp"

namespace {
namespace fx = VS::Effects;

// The modes timeline_playback_differential.py compares.
enum Mode : int {
    kLoad,
    kParticle,
    kSound,
    kChild,
    kRenderer,
    kVibration,
    kShake,
    kDispatch,
};

// The call kinds the harness records.
enum Call : int {
    kCallSample = 1,
    kCallEmit,
    kCallStream,
    kCallSfx,
    kCallWorldSfx,
    kCallSpawn,
    kCallAttach,
    kCallVibrate,
    kCallShake,
    kCallAdvance,
    kCallTrackData,
    kCallStart,
    kCallTick,
};

constexpr std::size_t kTransforms = 64;
constexpr std::size_t kVibrationPatterns = 32;
constexpr std::size_t kBlock11Bytes = 0x100;
constexpr std::size_t kVibrationOffsetsAt = 4;
constexpr int kBitsPerByte = 8;
constexpr unsigned kByteMask = 0xFF;
constexpr int kChildCountShift = 11;
constexpr std::uint32_t kChildCountBits = 0xF800;

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

// Every call a track tick makes, with the frame flag, translations and vibration block it reads.
class RecordingHost final : public fx::TimelinePlaybackHost {
public:
    bool FrameHeld() override { return held; }
    void SampleCurves(std::int16_t curve_frame) override { calls.push_back({ kCallSample, curve_frame }); }
    void EmitParticles(const fx::ParticleEmission& emission) override
    {
        calls.push_back({ kCallEmit, emission.event, emission.transparency, emission.variant });
    }
    void PlayStream(std::int32_t handle) override { calls.push_back({ kCallStream, handle }); }
    void PlaySfx(std::int32_t group, std::uint16_t effect) override { calls.push_back({ kCallSfx, group, effect }); }
    std::optional<fx::Translation> TransformTranslation(std::uint8_t transform) override
    {
        if (transform >= translations.size())
            return std::nullopt;
        return translations.at(transform);
    }
    void PlayWorldSfx(std::int32_t group, std::uint16_t effect, const fx::SoundPoint& point) override
    {
        calls.push_back({ kCallWorldSfx, group, effect, point.x, point.y, point.z });
    }
    void AttachRenderer(std::uint8_t renderer, std::int32_t parameters) override
    {
        calls.push_back({ kCallAttach, renderer, parameters });
    }
    void Vibrate(std::uint8_t pattern) override
    {
        const auto offset = fx::VibrationPatternOffset(block, pattern);
        calls.push_back({ kCallVibrate, offset ? static_cast<long long>(*offset) : -1 });
    }
    void ShakeCamera(const fx::ShakeChannels& channels) override
    {
        calls.push_back({ kCallShake, channels.mask, channels.shape, channels.amplitude, channels.step_uses_curve ? 1 : 0,
            channels.step, channels.random_uses_curve ? 1 : 0, channels.random });
    }
    bool SpawnChild(std::size_t target, std::optional<std::uint8_t> actor) override
    {
        calls.push_back({ kCallSpawn, static_cast<long long>(target), actor ? *actor : -1 });
        return true;
    }

    bool held = false;
    std::vector<fx::Translation> translations;
    std::array<std::byte, kBlock11Bytes> block {};
    Calls calls;
};

// The tick's result as the original returns it: 1 to keep the key, the advance's result for the
// next key and 0 once the track ends.
long long Result(fx::TrackStep step, long long more, Calls& calls)
{
    switch (step) {
    case fx::TrackStep::kContinue:
        return 1;
    case fx::TrackStep::kNextKey:
        calls.push_back({ kCallAdvance });
        return more;
    case fx::TrackStep::kEnd:
        return 0;
    }
    return -1;
}

bool Particle()
{
    std::uint32_t key = 0;
    std::int16_t frame = 0;
    fx::TrackTimers timers;
    int held = 0;
    long long more = 0;
    if (!Read(key) || !Read(frame) || !Read(timers.curve_frame) || !Read(timers.key_timer) || !Read(held)
        || !Read(more))
        return false;
    RecordingHost host;
    host.held = held != 0;
    const long long result = Result(fx::TickParticleEmissionTrack(key, frame, timers, host), more, host.calls);
    std::cout << result << ' ' << timers.curve_frame << ' ' << timers.key_timer << ' ';
    Print(host.calls);
    return true;
}

bool Sound()
{
    std::uint32_t key = 0;
    std::int16_t frame = 0;
    std::int32_t handle = 0;
    long long more = 0;
    if (!Read(key) || !Read(frame) || !Read(handle) || !Read(more))
        return false;
    RecordingHost host;
    host.translations.resize(kTransforms);
    for (auto& translation : host.translations) {
        if (!Read(translation.x) || !Read(translation.y) || !Read(translation.z))
            return false;
    }
    std::cout << Result(fx::TickSoundTrack(key, frame, handle, host), more, host.calls) << ' ';
    Print(host.calls);
    return true;
}

bool Child()
{
    fx::ChildTrackKey key;
    for (auto& actor : key.actors) {
        if (!Read(actor))
            return false;
    }
    std::int16_t frame = 0;
    std::size_t count = 0;
    if (!Read(key.final_actor) || !Read(key.frame) || !Read(key.delay) || !Read(frame) || !Read(count))
        return false;
    std::vector<fx::ChildTarget> targets(count);
    for (auto& target : targets) {
        if (!Read(target.type) || !Read(target.event_actor_slot))
            return false;
    }
    fx::ChildTrackState state;
    std::uint32_t child_word = 0;
    if (!Read(state.delay) || !Read(state.index) || !Read(child_word))
        return false;
    state.child_count = static_cast<std::uint8_t>((child_word & kChildCountBits) >> kChildCountShift);
    RecordingHost host;
    const fx::TrackStep step = fx::TickChildEffectTrack(key, frame, targets, state, host);
    child_word = (child_word & ~kChildCountBits) | (static_cast<std::uint32_t>(state.child_count) << kChildCountShift);
    std::cout << (step == fx::TrackStep::kContinue ? 1 : step == fx::TrackStep::kEnd ? 0 : -1) << ' ' << state.delay
              << ' ' << state.index << ' ' << static_cast<std::int32_t>(child_word) << ' ';
    Print(host.calls);
    return true;
}

bool Renderer()
{
    fx::RendererKey key;
    std::int16_t frame = 0;
    long long more = 0;
    if (!Read(key.timing) || !Read(key.parameter_high) || !Read(frame) || !Read(more))
        return false;
    RecordingHost host;
    std::cout << Result(fx::TickRendererTrack(key, frame, host), more, host.calls) << ' ';
    Print(host.calls);
    return true;
}

bool Read(fx::CommandEndpoint& endpoint) { return Read(endpoint.type) && Read(endpoint.actor); }

bool Vibration()
{
    std::uint16_t key = 0;
    std::int16_t frame = 0;
    fx::CommandEndpoint source;
    fx::CommandEndpoint target;
    std::size_t count = 0;
    if (!Read(key) || !Read(frame) || !Read(source) || !Read(target) || !Read(count))
        return false;
    std::vector<fx::CommandEndpoint> targets(count);
    for (auto& action_target : targets) {
        if (!Read(action_target))
            return false;
    }
    RecordingHost host;
    for (std::size_t pattern = 0; pattern < kVibrationPatterns; ++pattern) {
        unsigned offset = 0;
        if (!Read(offset))
            return false;
        const std::size_t entry = kVibrationOffsetsAt + (pattern * 2);
        host.block.at(entry) = static_cast<std::byte>(offset & kByteMask);
        host.block.at(entry + 1) = static_cast<std::byte>((offset >> kBitsPerByte) & kByteMask);
    }
    long long more = 0;
    if (!Read(more))
        return false;
    const fx::VibrationEndpoints endpoints { .source = source, .target = target, .action_targets = targets };
    std::cout << Result(fx::TickVibrationTrack(key, frame, endpoints, host), more, host.calls) << ' ';
    Print(host.calls);
    return true;
}

bool Shake()
{
    std::uint16_t timing = 0;
    std::uint32_t channels = 0;
    std::int16_t frame = 0;
    std::int16_t timer = 0;
    long long more = 0;
    if (!Read(timing) || !Read(channels) || !Read(frame) || !Read(timer) || !Read(more))
        return false;
    RecordingHost host;
    const long long result = Result(fx::TickCameraShakeTrack(timing, channels, frame, timer, host), more, host.calls);
    std::cout << result << ' ' << timer << ' ';
    Print(host.calls);
    return true;
}

// The section's track headers, recording each track the load reads and starts.
class RecordingLoadHost final : public fx::TimelineLoadHost {
public:
    std::optional<std::uint16_t> TrackHeader(std::size_t track) override
    {
        calls.push_back({ kCallTrackData, static_cast<long long>(track) });
        if (track >= headers.size())
            return std::nullopt;
        return headers.at(track);
    }
    void StartTrack(std::size_t track, const fx::TimelineTrackStart& start) override
    {
        calls.push_back({ kCallStart, static_cast<long long>(track), start.header, start.key_index,
            start.timers.curve_frame, start.timers.key_timer, start.key_parameter });
    }

    std::vector<std::uint16_t> headers;
    Calls calls;
};

bool Load()
{
    std::uint8_t count = 0;
    if (!Read(count))
        return false;
    RecordingLoadHost host;
    host.headers.resize(count);
    for (auto& header : host.headers) {
        if (!Read(header))
            return false;
    }
    const fx::LoadedTimeline loaded = fx::LoadEffectTimeline(count, host);
    std::cout << int { loaded.track_count } << ' ' << int { loaded.flags } << ' '
              << static_cast<std::int32_t>(loaded.active_tracks) << ' ';
    Print(host.calls);
    return true;
}

// The timeline's track kinds and each tick's drawn result and flags.
class RecordingRunner final : public fx::TimelineRunner {
public:
    std::uint8_t Flags() override { return flags; }
    std::uint8_t KindOf(std::size_t track) override { return kinds.at(track); }
    bool Tick(std::size_t track, fx::TimelineTrackKind kind, bool all_targets) override
    {
        calls.push_back({ kCallTick, static_cast<long long>(kind), static_cast<long long>(track), all_targets ? 1 : 0 });
        if (next >= ticks.size())
            return false;
        const auto [result, after] = ticks.at(next++);
        if (after >= 0)
            flags = static_cast<std::uint8_t>(after);
        return result != 0;
    }
    void UnknownKind(std::size_t track, std::uint8_t kind) override
    {
        calls.push_back({ -1, kind, static_cast<long long>(track) });
    }

    std::uint8_t flags {};
    std::vector<std::uint8_t> kinds;
    std::vector<std::pair<long long, long long>> ticks;
    std::size_t next {};
    Calls calls;
};

bool Dispatch()
{
    fx::TimelineProgress progress;
    RecordingRunner runner;
    if (!Read(progress.track_count) || !Read(progress.frame) || !Read(progress.active_tracks) || !Read(runner.flags))
        return false;
    runner.kinds.resize(progress.track_count);
    for (auto& kind : runner.kinds) {
        if (!Read(kind))
            return false;
    }
    runner.ticks.resize(progress.track_count);
    for (auto& [result, after] : runner.ticks) {
        if (!Read(result) || !Read(after))
            return false;
    }
    fx::TickEffectTimeline(progress, runner);
    std::cout << static_cast<std::int32_t>(progress.active_tracks) << ' ' << progress.frame << ' ';
    Print(runner.calls);
    return true;
}
} // namespace

int main()
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int mode = 0;
    while (Read(mode)) {
        bool read = false;
        switch (mode) {
        case kLoad:
            read = Load();
            break;
        case kParticle:
            read = Particle();
            break;
        case kSound:
            read = Sound();
            break;
        case kChild:
            read = Child();
            break;
        case kRenderer:
            read = Renderer();
            break;
        case kVibration:
            read = Vibration();
            break;
        case kShake:
            read = Shake();
            break;
        case kDispatch:
            read = Dispatch();
            break;
        default:
            return 1;
        }
        if (!read)
            return 1;
    }
    return 0;
}
