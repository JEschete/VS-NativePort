#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/SoundBanks.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/Runtime.h"
#include "Effects/Timeline.hpp"
#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelinePlayback.hpp"
#include "Effects/TimelinePlaybackLive.hpp"
#include "Effects/Transforms.hpp"

namespace {
namespace fx = VS::Effects;
namespace Memory = VS::Core::Memory;
using fx::TimelineTrackKind;
using fx::TrackStep;

fx::CommandEndpoint Actor(std::uint8_t actor) {
  return {.type = fx::kActorPartEndpoint, .actor = actor, .point = {}};
}

fx::CommandEndpoint RoomPoint(std::uint8_t actor) {
  return {.type = fx::kRoomPointEndpoint, .actor = actor, .point = {}};
}

struct WorldSfx {
  std::int32_t group{};
  std::uint16_t effect{};
  fx::SoundPoint point{};

  friend bool operator==(const WorldSfx&, const WorldSfx&) = default;
};

// Records everything the tracks play.
class RecordingPlaybackHost final : public fx::TimelinePlaybackHost {
 public:
  bool FrameHeld() override { return held; }
  void SampleCurves(std::int16_t curve_frame) override {
    sampled.push_back(curve_frame);
  }
  void EmitParticles(const fx::ParticleEmission& emission) override {
    emitted.push_back(emission);
  }
  void PlayStream(std::int32_t handle) override { streams.push_back(handle); }
  void PlaySfx(std::int32_t group, std::uint16_t effect) override {
    sfx.emplace_back(group, effect);
  }
  std::optional<fx::Translation> TransformTranslation(
      std::uint8_t transform) override {
    queried_transforms.push_back(transform);
    return translation;
  }
  void PlayWorldSfx(std::int32_t group, std::uint16_t effect,
                    const fx::SoundPoint& point) override {
    world_sfx.push_back({group, effect, point});
  }
  void AttachRenderer(std::uint8_t renderer, std::int32_t parameters) override {
    renderers.emplace_back(renderer, parameters);
  }
  void Vibrate(std::uint8_t pattern) override { patterns.push_back(pattern); }
  void ShakeCamera(const fx::ShakeChannels& channels) override {
    shakes.push_back(channels);
  }
  bool SpawnChild(std::size_t target,
                  std::optional<std::uint8_t> actor) override {
    children.emplace_back(target, actor);
    return spawns;
  }

  bool held = false;
  bool spawns = true;
  std::optional<fx::Translation> translation;
  std::vector<std::int16_t> sampled;
  std::vector<fx::ParticleEmission> emitted;
  std::vector<std::int32_t> streams;
  std::vector<std::pair<std::int32_t, std::uint16_t>> sfx;
  std::vector<std::uint8_t> queried_transforms;
  std::vector<WorldSfx> world_sfx;
  std::vector<std::pair<std::uint8_t, std::int32_t>> renderers;
  std::vector<std::uint8_t> patterns;
  std::vector<fx::ShakeChannels> shakes;
  std::vector<std::pair<std::size_t, std::optional<std::uint8_t>>> children;
};

// A timeline of `kinds`, each tick keeping its track unless `finished`.
class RecordingRunner final : public fx::TimelineRunner {
 public:
  RecordingRunner(std::vector<std::uint8_t> kinds, std::uint8_t flags)
      : kinds_(std::move(kinds)), flags_(flags) {}

  std::uint8_t Flags() override { return flags_; }
  std::uint8_t KindOf(std::size_t track) override { return kinds_.at(track); }
  bool Tick(std::size_t track, TimelineTrackKind kind,
            bool all_targets) override {
    ticked.emplace_back(track, kind, all_targets);
    return std::ranges::find(finished, track) == finished.end();
  }
  void UnknownKind(std::size_t track, std::uint8_t kind) override {
    unknown.emplace_back(track, kind);
  }

  std::vector<std::size_t> finished;
  std::vector<std::tuple<std::size_t, TimelineTrackKind, bool>> ticked;
  std::vector<std::pair<std::size_t, std::uint8_t>> unknown;

 private:
  std::vector<std::uint8_t> kinds_;
  std::uint8_t flags_;
};

// A timeline section whose track headers are `headers` (nullopt: missing).
class RecordingLoadHost final : public fx::TimelineLoadHost {
 public:
  explicit RecordingLoadHost(std::vector<std::optional<std::uint16_t>> headers)
      : headers_(std::move(headers)) {}

  std::optional<std::uint16_t> TrackHeader(std::size_t track) override {
    return headers_.at(track);
  }
  void StartTrack(std::size_t track,
                  const fx::TimelineTrackStart& start) override {
    started.emplace_back(track, start.header);
    starts.push_back(start);
  }

  std::vector<std::pair<std::size_t, std::uint16_t>> started;
  std::vector<fx::TimelineTrackStart> starts;

 private:
  std::vector<std::optional<std::uint16_t>> headers_;
};

constexpr fx::ChildTrackKey ChildKey(std::uint8_t delay) {
  return {.actors = {10, 11, 12, 13, 14, 15},
          .final_actor = 20,
          .frame = 2,
          .delay = delay};
}

// Restores the effect package pointer after a test.
class PackageScope {
 public:
  explicit PackageScope(fx::LiveEffectPackage* package)
      : saved_(Effects_package) {
    Effects_package = package;
  }
  ~PackageScope() { Effects_package = saved_; }
  PackageScope(const PackageScope&) = delete;
  PackageScope& operator=(const PackageScope&) = delete;
  PackageScope(PackageScope&&) = delete;
  PackageScope& operator=(PackageScope&&) = delete;

 private:
  fx::LiveEffectPackage* saved_;
};

void PutU16(std::span<std::byte> bytes, std::size_t at, std::uint16_t value) {
  bytes[at] = static_cast<std::byte>(value & 0xFFU);
  bytes[at + 1] = static_cast<std::byte>(value >> 8U);
}

// One timeline section at 0x08 of three tracks: a child track (kind 3, one
// key, data 0x20), a sound track (kind 2, two keys, data 0x30) and a
// particle track whose data lies far past the block.
constexpr std::size_t kTimelineBlockBytes = 0x40;
std::array<std::byte, kTimelineBlockBytes> TimelineBlock() {
  std::array<std::byte, kTimelineBlockBytes> block{};
  PutU16(block, 0, 1);
  PutU16(block, 4, 0x08);
  PutU16(block, 0x08, 3);
  PutU16(block, 0x0C, 0x0103);
  PutU16(block, 0x0E, 0x20);
  PutU16(block, 0x10, 0x0202);
  PutU16(block, 0x12, 0x30);
  PutU16(block, 0x14, 0x0101);
  PutU16(block, 0x16, 0xFFF0);
  // The child key starts on frame 0xFF.
  block[0x20 + 7] = std::byte{0xFF};
  // The sound keys are due on frames 5 and 6.
  PutU16(block, 0x30, 5);
  PutU16(block, 0x34, 6);
  return block;
}

// A track over `keys` on its first key.
fx::LiveTimelineTrack TrackOver(std::uint8_t kind,
                                std::span<const std::uint32_t> keys,
                                std::uint8_t key_count) {
  fx::LiveTimelineTrack track{};
  track.keys = keys.data();
  track.header.kind = kind;
  track.header.key_count = key_count;
  return track;
}

// A live heap whose current package's block 3 is TimelineBlock().
class LiveTimelinePackage {
 public:
  LiveTimelinePackage()
      : payload_(Memory::AllocateLiveFromHighEnd(kTimelineBlockBytes)) {
    REQUIRE(payload_ != nullptr);
    const auto block = TimelineBlock();
    std::memcpy(payload_, block.data(), block.size());
    package_.timelines = static_cast<fx::LiveTimelineTable*>(payload_);
    package_.request = &request_;
  }
  ~LiveTimelinePackage() {
    if (instance.context.timeline != nullptr) {
      Memory::FreeLiveFromHighEnd(instance.context.timeline);
    }
    Memory::FreeLiveFromHighEnd(payload_);
  }
  LiveTimelinePackage(const LiveTimelinePackage&) = delete;
  LiveTimelinePackage& operator=(const LiveTimelinePackage&) = delete;
  LiveTimelinePackage(LiveTimelinePackage&&) = delete;
  LiveTimelinePackage& operator=(LiveTimelinePackage&&) = delete;

  // Loads section 0's timeline into `instance`.
  fx::LiveEffectTimeline& Load() {
    fx::LiveLoadEffectTimeline(&instance, 0, 0);
    REQUIRE(instance.context.timeline != nullptr);
    return *instance.context.timeline;
  }
  [[nodiscard]] std::byte* Block() const {
    return static_cast<std::byte*>(payload_);
  }

  fx::LiveEffectInstance instance{};

 private:
  alignas(Memory::kHeapUnitBytes)
      std::array<std::byte, Memory::kHeapUnitBytes * 128> arena_{};
  Memory::ScopedLiveGameHeap heap_{arena_};
  VS::Effects::LiveActionEffectRequest request_{};
  fx::LiveEffectPackage package_{};
  PackageScope scope_{&package_};
  void* payload_;
};

// Due on frame 5 for two frames: event 0x15, transparency 0x2A, variant 3.
constexpr std::uint32_t kParticleLook = (0x15U << 20) | (0x2AU << 26);
constexpr std::uint32_t kParticleKey = 5U | (3U << 16) | kParticleLook;
constexpr std::uint32_t kSoundEffect = 0x123U << 9;
// Due on frame 3: pattern 0x1A, for the audience in bits 14-15.
constexpr std::uint16_t kVibrationKey = 3 | (0x1A << 9);

// Whether the vibration key for `audience` vibrates its pattern on its due
// frame.
bool Vibrates(std::uint16_t audience, const fx::VibrationEndpoints& endpoints) {
  RecordingPlaybackHost host;
  const auto key = static_cast<std::uint16_t>(kVibrationKey | (audience << 14));
  CHECK(fx::TickVibrationTrack(key, 3, endpoints, host) == TrackStep::kNextKey);
  return host.patterns == std::vector<std::uint8_t>{0x1A};
}
}  // namespace

TEST_CASE("Timeline track kinds exclude the kinds without a tick",
          "[timeline-playback]") {
  CHECK(fx::TimelineTrackKindOf(1) == TimelineTrackKind::kParticleEmission);
  CHECK(fx::TimelineTrackKindOf(10) == TimelineTrackKind::kBackgroundColour);
  CHECK(fx::TimelineTrackKindOf(12) == TimelineTrackKind::kActionControl);
  CHECK(fx::TimelineTrackKindOf(13) == TimelineTrackKind::kCameraShake);
  for (const std::uint8_t kind : std::array<std::uint8_t, 4>{0, 11, 14, 255}) {
    CHECK_FALSE(fx::TimelineTrackKindOf(kind).has_value());
  }
}

TEST_CASE("Particle tracks emit from their due frame", "[timeline-playback]") {
  CHECK(
      fx::ParticleEmissionOf(kParticleKey) ==
      fx::ParticleEmission{.event = 0x15, .transparency = 0x2A, .variant = 3});
  RecordingPlaybackHost host;
  fx::TrackTimers timers{.curve_frame = 0, .key_timer = 2};

  CHECK(fx::TickParticleEmissionTrack(kParticleKey, 4, timers, host) ==
        TrackStep::kContinue);
  CHECK(host.sampled.empty());
  CHECK(fx::TickParticleEmissionTrack(kParticleKey, 5, timers, host) ==
        TrackStep::kContinue);
  CHECK(timers == fx::TrackTimers{.curve_frame = 1, .key_timer = 1});
  CHECK(host.sampled == std::vector<std::int16_t>{0});
  CHECK(host.emitted.size() == 1);
}

TEST_CASE("Particle tracks keep their key on held frames until it ends",
          "[timeline-playback]") {
  RecordingPlaybackHost host;
  fx::TrackTimers timers{.curve_frame = 1, .key_timer = 1};
  host.held = true;
  CHECK(fx::TickParticleEmissionTrack(kParticleKey, 9, timers, host) ==
        TrackStep::kContinue);
  CHECK(timers == fx::TrackTimers{.curve_frame = 2, .key_timer = 1});
  host.held = false;
  CHECK(fx::TickParticleEmissionTrack(kParticleKey, 9, timers, host) ==
        TrackStep::kNextKey);
  CHECK(timers == fx::TrackTimers{.curve_frame = 0, .key_timer = 0});
  CHECK(host.sampled == std::vector<std::int16_t>{1, 2});
  CHECK(host.emitted.size() == 2);
}

TEST_CASE("Sound tracks play bank and stream sounds when due",
          "[timeline-playback]") {
  RecordingPlaybackHost host;
  CHECK(fx::TickSoundTrack(7 | kSoundEffect, 6, 0, host) ==
        TrackStep::kContinue);
  CHECK(fx::TickSoundTrack(7 | kSoundEffect, 7, 0, host) ==
        TrackStep::kNextKey);
  // Any source but the shared bank and the stream plays from the room bank.
  CHECK(fx::TickSoundTrack(7 | kSoundEffect | (3U << 18), 7, 0, host) ==
        TrackStep::kNextKey);
  CHECK(host.sfx == std::vector<std::pair<std::int32_t, std::uint16_t>>{
                        {VS::Audio::kSharedEffectTags, 0x123},
                        {VS::Audio::kRoomSfxBankEffectTags, 0x123}});

  constexpr std::uint32_t kStream = 7 | (2U << 18);
  CHECK(fx::TickSoundTrack(kStream, 7, 0, host) == TrackStep::kNextKey);
  CHECK(fx::TickSoundTrack(kStream, 7, 9, host) == TrackStep::kNextKey);
  CHECK(host.streams == std::vector<std::int32_t>{9});
}

TEST_CASE("Sound tracks play positioned sounds at their transform",
          "[timeline-playback]") {
  constexpr std::uint32_t kPositioned =
      7 | kSoundEffect | (1U << 18) | (1U << 20) | (3U << 22);
  RecordingPlaybackHost host;
  host.translation = fx::Translation{.x = 0x12345, .y = -2, .z = 0x7FFF0001};
  CHECK(fx::TickSoundTrack(kPositioned, 7, 0, host) == TrackStep::kNextKey);
  CHECK(host.world_sfx ==
        std::vector<WorldSfx>{{VS::Audio::kRoomSfxBankEffectTags,
                               0x123,
                               {.x = 0x2345, .y = -2, .z = 1}}});
  // A refused transform plays nothing but still moves to the next key.
  host.translation.reset();
  CHECK(fx::TickSoundTrack(kPositioned, 7, 0, host) == TrackStep::kNextKey);
  CHECK(host.world_sfx.size() == 1);
  CHECK(host.queried_transforms == std::vector<std::uint8_t>{3, 3});
}

TEST_CASE("Child tracks spawn a child per action target from their frame",
          "[timeline-playback]") {
  constexpr fx::ChildTrackKey kKey = ChildKey(0);
  const std::array<fx::ChildTarget, 3> targets{
      fx::ChildTarget{.type = fx::kActorPartEndpoint, .event_actor_slot = 2},
      fx::ChildTarget{.type = fx::kRoomPointEndpoint, .event_actor_slot = 5},
      fx::ChildTarget{.type = fx::kActorPartEndpoint, .event_actor_slot = 9}};
  RecordingPlaybackHost host;
  fx::ChildTrackState state{};

  CHECK(fx::TickChildEffectTrack(kKey, 1, targets, state, host) ==
        TrackStep::kContinue);
  CHECK(host.children.empty());
  CHECK(fx::TickChildEffectTrack(kKey, 2, targets, state, host) ==
        TrackStep::kContinue);
  // Room points take the first actor; a slot past the key leaves the
  // register unset.
  using Spawned = std::pair<std::size_t, std::optional<std::uint8_t>>;
  CHECK(host.children == std::vector<Spawned>{{0, std::uint8_t{12}},
                                              {1, std::uint8_t{10}},
                                              {2, std::nullopt}});
  CHECK(state == fx::ChildTrackState{.delay = 0, .index = 3, .child_count = 3});
}

TEST_CASE("Child tracks start the final child once the others finish",
          "[timeline-playback]") {
  constexpr fx::ChildTrackKey kKey = ChildKey(0);
  const std::array<fx::ChildTarget, 3> targets{};
  RecordingPlaybackHost host;
  fx::ChildTrackState state{.delay = 0, .index = 3, .child_count = 3};

  CHECK(fx::TickChildEffectTrack(kKey, 3, targets, state, host) ==
        TrackStep::kContinue);
  CHECK(host.children.empty());
  state.child_count = 0;
  CHECK(fx::TickChildEffectTrack(kKey, 3, targets, state, host) ==
        TrackStep::kContinue);
  using Spawned = std::pair<std::size_t, std::optional<std::uint8_t>>;
  CHECK(host.children == std::vector<Spawned>{{0, std::uint8_t{20}}});
  CHECK(state == fx::ChildTrackState{.delay = 0, .index = 4, .child_count = 1});
  CHECK(fx::TickChildEffectTrack(kKey, 3, targets, state, host) ==
        TrackStep::kEnd);
  state.index = -1;
  CHECK(fx::TickChildEffectTrack(kKey, 3, targets, state, host) ==
        TrackStep::kEnd);
}

TEST_CASE("Child tracks space their children and count only spawned ones",
          "[timeline-playback]") {
  constexpr fx::ChildTrackKey kKey = ChildKey(2);
  const std::array<fx::ChildTarget, 2> targets{};
  RecordingPlaybackHost host;
  host.spawns = false;
  fx::ChildTrackState state{};

  CHECK(fx::TickChildEffectTrack(kKey, 2, targets, state, host) ==
        TrackStep::kContinue);
  CHECK(state == fx::ChildTrackState{.delay = 1, .index = 1, .child_count = 0});
  CHECK(fx::TickChildEffectTrack(kKey, 2, targets, state, host) ==
        TrackStep::kContinue);
  CHECK(host.children.size() == 1);
  CHECK(fx::TickChildEffectTrack(kKey, 2, targets, state, host) ==
        TrackStep::kContinue);
  CHECK(state == fx::ChildTrackState{.delay = 0, .index = 2, .child_count = 0});
  // Without targets the final child starts at once.
  fx::ChildTrackState alone{};
  host.spawns = true;
  CHECK(fx::TickChildEffectTrack(kKey, 2, {}, alone, host) ==
        TrackStep::kContinue);
  CHECK(alone == fx::ChildTrackState{.delay = 0, .index = 1, .child_count = 1});
}

TEST_CASE("Child keys name event actors only inside the key",
          "[timeline-playback]") {
  constexpr fx::ChildTrackKey kKey = ChildKey(3);
  CHECK(fx::EventActorOf(kKey, 0) == 10);
  CHECK(fx::EventActorOf(kKey, 5) == 15);
  CHECK(fx::EventActorOf(kKey, 6) == 20);
  CHECK(fx::EventActorOf(kKey, 7) == 2);
  CHECK(fx::EventActorOf(kKey, 8) == 3);
  CHECK_FALSE(fx::EventActorOf(kKey, 9).has_value());
  CHECK_FALSE(fx::EventActorOf(kKey, 255).has_value());
}

TEST_CASE("Renderer tracks add their parameter halves when due",
          "[timeline-playback]") {
  constexpr std::uint32_t kTiming =
      4U | (0x1FFU << 9) | (5U << 18) | (0x3FU << 21);
  RecordingPlaybackHost host;
  CHECK(fx::TickRendererTrack({.timing = kTiming, .parameter_high = 0xBEEF}, 3,
                              host) == TrackStep::kContinue);
  CHECK(fx::TickRendererTrack({.timing = kTiming, .parameter_high = 0xBEEF}, 4,
                              host) == TrackStep::kNextKey);
  // 0x1FF + 0x3F00 carries into bit 14.
  CHECK(host.renderers == std::vector<std::pair<std::uint8_t, std::int32_t>>{
                              {5, static_cast<std::int32_t>(0xBEEF40FFU)}});
}

TEST_CASE("Vibration tracks vibrate for everyone or a player endpoint",
          "[timeline-playback]") {
  RecordingPlaybackHost host;
  CHECK(fx::TickVibrationTrack(kVibrationKey, 2, {}, host) ==
        TrackStep::kContinue);
  CHECK(host.patterns.empty());
  CHECK(Vibrates(0, {}));
  CHECK(Vibrates(1, {.source = Actor(0)}));
  CHECK_FALSE(Vibrates(1, {.source = Actor(1)}));
  CHECK_FALSE(Vibrates(1, {.source = RoomPoint(0)}));
  CHECK(Vibrates(2, {.target = Actor(0)}));
  CHECK_FALSE(Vibrates(2, {.source = Actor(0), .target = Actor(2)}));
}

TEST_CASE("Vibration tracks vibrate when any action target is the player",
          "[timeline-playback]") {
  const std::array<fx::CommandEndpoint, 3> with_player{Actor(4), RoomPoint(0),
                                                       Actor(0)};
  const std::array<fx::CommandEndpoint, 2> without_player{Actor(4),
                                                          RoomPoint(0)};
  CHECK(Vibrates(3, {.action_targets = with_player}));
  CHECK_FALSE(
      Vibrates(3, {.source = Actor(0), .action_targets = without_player}));
}

TEST_CASE("Vibration patterns lie inside the package's block 11",
          "[timeline-playback]") {
  std::array<std::byte, 12> block{};
  PutU16(block, 4, 8);
  PutU16(block, 6, 0xFFFF);
  PutU16(block, 8, 12);
  PutU16(block, 10, 11);
  CHECK(fx::VibrationPatternOffset(block, 0) == std::size_t{8});
  CHECK_FALSE(fx::VibrationPatternOffset(block, 1).has_value());
  CHECK_FALSE(fx::VibrationPatternOffset(block, 2).has_value());
  CHECK(fx::VibrationPatternOffset(block, 3) == std::size_t{11});
  CHECK_FALSE(fx::VibrationPatternOffset(block, 4).has_value());
  CHECK_FALSE(
      fx::VibrationPatternOffset(std::span(block).first(5), 0).has_value());
  CHECK_FALSE(fx::VibrationPatternOffset({}, 0).has_value());
}

TEST_CASE("Camera shake tracks start the first key's channels once",
          "[timeline-playback]") {
  constexpr std::uint32_t kChannels = 0x5U | (0x11U << 4) | (0x22U << 9) |
                                      (1U << 16) | (0x33U << 17) |
                                      (0x44U << 25);
  constexpr fx::ShakeChannels kShake{.mask = 5,
                                     .shape = 0x11,
                                     .amplitude = 0x22,
                                     .step_uses_curve = true,
                                     .step = 0x33,
                                     .random_uses_curve = false,
                                     .random = 0x44};
  CHECK(fx::ShakeChannelsOf(kChannels) == kShake);
  CHECK(fx::ShakeChannelsOf(0xFFFFFFFF) ==
        fx::ShakeChannels{.mask = 0xF,
                          .shape = 0x1F,
                          .amplitude = 0x7F,
                          .step_uses_curve = true,
                          .step = 0x7F,
                          .random_uses_curve = true,
                          .random = 0x7F});

  RecordingPlaybackHost host;
  std::int16_t timer = 2;
  CHECK(fx::TickCameraShakeTrack(6, kChannels, 5, timer, host) ==
        TrackStep::kContinue);
  CHECK(timer == 2);
  CHECK(fx::TickCameraShakeTrack(6, kChannels, 6, timer, host) ==
        TrackStep::kContinue);
  CHECK(timer == 1);
  CHECK(fx::TickCameraShakeTrack(6, kChannels, 7, timer, host) ==
        TrackStep::kNextKey);
  CHECK(timer == 0);
  CHECK(host.shakes == std::vector<fx::ShakeChannels>{kShake});
}

TEST_CASE("The dispatcher ticks active tracks and retires finished ones",
          "[timeline-playback]") {
  RecordingRunner runner({1, 7, 3, 11}, fx::kTimelineHasChildTracks);
  runner.finished = {1};
  fx::TimelineProgress progress{
      .track_count = 4, .frame = 7, .active_tracks = 0b1011};
  fx::TickEffectTimeline(progress, runner);
  CHECK(progress == fx::TimelineProgress{
                        .track_count = 4, .frame = 8, .active_tracks = 0b0001});
  using Ticked = std::tuple<std::size_t, TimelineTrackKind, bool>;
  CHECK(runner.ticked ==
        std::vector<Ticked>{{0, TimelineTrackKind::kParticleEmission, false},
                            {1, TimelineTrackKind::kTargetActorColour, true}});
  CHECK(runner.unknown ==
        std::vector<std::pair<std::size_t, std::uint8_t>>{{3, 11}});

  RecordingRunner held({7}, fx::kTimelineFrameHeld);
  fx::TimelineProgress paused{
      .track_count = 1, .frame = -1, .active_tracks = 1};
  fx::TickEffectTimeline(paused, held);
  CHECK(paused.frame == -1);
  CHECK(held.ticked ==
        std::vector<Ticked>{{0, TimelineTrackKind::kTargetActorColour, false}});
}

TEST_CASE("Tracks from the 33rd share the first tracks' activity bits",
          "[timeline-playback]") {
  RecordingRunner runner(std::vector<std::uint8_t>(33, 1), 0);
  fx::TimelineProgress progress{
      .track_count = 33, .frame = 0x7FFF, .active_tracks = 1};
  fx::TickEffectTimeline(progress, runner);
  CHECK(runner.ticked.size() == 2);
  CHECK(std::get<0>(runner.ticked.back()) == 32);
  CHECK(progress.frame == -0x8000);

  RecordingRunner retiring(std::vector<std::uint8_t>(33, 1), 0);
  retiring.finished = {0};
  progress.active_tracks = 1;
  fx::TickEffectTimeline(progress, retiring);
  CHECK(retiring.ticked.size() == 1);
  CHECK(progress.active_tracks == 0);
}

TEST_CASE("Timeline loads start each track until one is missing",
          "[timeline-playback]") {
  RecordingLoadHost host({0x0101, 0x0203, std::nullopt, 0x0102});
  CHECK(
      fx::LoadEffectTimeline(4, host) ==
      fx::LoadedTimeline{.track_count = 2, .flags = 2, .active_tracks = 0b11});
  CHECK(host.started == std::vector<std::pair<std::size_t, std::uint16_t>>{
                            {0, 0x0101}, {1, 0x0203}});
  for (const fx::TimelineTrackStart& start : host.starts) {
    CHECK(start.key_index == -1);
    CHECK(start.timers == fx::TrackTimers{});
    CHECK(start.key_parameter == 0);
  }

  RecordingLoadHost plain({0x0301, 0x0001});
  CHECK(
      fx::LoadEffectTimeline(2, plain) ==
      fx::LoadedTimeline{.track_count = 2, .flags = 0, .active_tracks = 0b11});
  RecordingLoadHost empty({});
  CHECK(fx::LoadEffectTimeline(0, empty) == fx::LoadedTimeline{});
}

TEST_CASE("Live timelines load the package's timeline section",
          "[timeline-playback]") {
  LiveTimelinePackage live;
  const fx::LiveEffectTimeline& timeline = live.Load();
  CHECK(timeline.track_count == 2);
  CHECK(timeline.flags == fx::kTimelineHasChildTracks);
  CHECK(timeline.active_tracks == 0b11);
  CHECK(timeline.frame == 0);
  const auto tracks = timeline.Tracks();
  CHECK(reinterpret_cast<const std::byte*>(tracks[0].keys) ==
        live.Block() + 0x20);
  CHECK(reinterpret_cast<const std::byte*>(tracks[1].keys) ==
        live.Block() + 0x30);
}

TEST_CASE("Live timelines start their tracks and keep them on a bad section",
          "[timeline-playback]") {
  LiveTimelinePackage live;
  const fx::LiveEffectTimeline& timeline = live.Load();
  const auto tracks = timeline.Tracks();
  CHECK(tracks[0].header.kind == 3);
  CHECK(tracks[1].header.key_count == 2);
  CHECK(tracks[1].key_index == 0);
  // A section outside the block keeps the current timeline.
  fx::LiveLoadEffectTimeline(&live.instance, 1000, 0);
  CHECK(live.instance.context.timeline == &timeline);
}

TEST_CASE("Live timelines tick their tracks", "[timeline-playback]") {
  LiveTimelinePackage live;
  fx::LiveEffectTimeline& timeline = live.Load();
  const auto tracks = timeline.Tracks();
  for (int frame = 0; frame < 5; ++frame) {
    fx::LiveTickEffectTimeline(&live.instance, 0);
  }
  CHECK(timeline.frame == 5);
  // The sound track's due key plays the (absent) stream and moves on.
  std::uint32_t sound_key = 0;
  std::memcpy(&sound_key, live.Block() + 0x30, sizeof sound_key);
  sound_key |= 2U << 18;
  std::memcpy(live.Block() + 0x30, &sound_key, sizeof sound_key);
  fx::LiveTickEffectTimeline(&live.instance, 0);
  CHECK(tracks[1].key_index == 1);
  CHECK(timeline.active_tracks == 0b11);
  // Kinds without a tick are retired; a held frame stays.
  tracks[1].header.kind = 11;
  timeline.flags = fx::kTimelineFrameHeld;
  fx::LiveTickEffectTimeline(&live.instance, 0);
  CHECK(timeline.active_tracks == 0b01);
  CHECK(timeline.frame == 6);
}

TEST_CASE("Live timeline tracks refuse missing records",
          "[timeline-playback]") {
  const PackageScope scope(nullptr);
  static fx::LiveEffectTimeline timeline;
  static fx::LiveEffectInstance instance{};
  std::array<std::uint32_t, 2> keys{9, 0};
  fx::LiveTimelineTrack track = TrackOver(4, keys, 1);

  fx::LiveTickEffectTimeline(nullptr, 0);
  fx::LiveTickEffectTimeline(&instance, 0);
  fx::LiveLoadEffectTimeline(nullptr, 0, 0);
  fx::LiveLoadEffectTimeline(&instance, 0, 0);
  CHECK(instance.context.timeline == nullptr);
  CHECK(fx::LiveTickEffectRendererTrack(&instance, &track) == 0);
  instance.context.timeline = &timeline;
  CHECK(fx::LiveTickParticleEmissionTrack(nullptr, &track) == 0);
  CHECK(fx::LiveTickEffectRendererTrack(&instance, nullptr) == 0);
  CHECK(fx::LiveTickControllerVibrationTrack(&instance, &track) == 0);
  CHECK(fx::LiveTickChildEffectTrack(&instance, &track, 0) == 0);
  track.key_index = 1;
  CHECK(fx::LiveTickEffectSoundTrack(&instance, &track) == 0);
  track.key_index = 0;
  timeline.frame = 8;
  CHECK(fx::LiveTickEffectRendererTrack(&instance, &track) == 1);
  instance.context.timeline = nullptr;
}

TEST_CASE("Live shake tracks count down even while the frame is held",
          "[timeline-playback]") {
  static fx::LiveEffectTimeline timeline;
  static fx::LiveEffectInstance instance{};
  instance.context.timeline = &timeline;
  std::array<std::uint32_t, 4> keys{6, 0, 7, 0};
  fx::LiveTimelineTrack track = TrackOver(13, keys, 2);
  track.key_timer = 3;
  timeline.frame = 5;
  CHECK(fx::LiveTickCameraShakeTrack(&instance, &track) == 1);
  CHECK(track.key_timer == 3);
  timeline.frame = 9;
  timeline.flags = fx::kTimelineFrameHeld;
  CHECK(fx::LiveTickCameraShakeTrack(&instance, &track) == 1);
  CHECK(track.key_timer == 2);
  timeline.flags = 0;
  instance.context.timeline = nullptr;
}
