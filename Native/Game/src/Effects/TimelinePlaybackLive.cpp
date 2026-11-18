// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelinePlaybackLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "Audio/SfxLive.hpp"
#include "Audio/StreamLive.hpp"
#include "Content/PFile.hpp"
#include "Content/PFileLive.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Effects/CameraLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/EffectInstancesLive.hpp"
#include "Effects/EffectResources.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/ParticleEmitterLive.hpp"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelineCommandsLive.hpp"
#include "Effects/TimelinePlayback.hpp"
#include "Effects/TimelineTrackKeys.hpp"
#include "Effects/TimelineTracksLive.hpp"
#include "Input/ControllerLive.hpp"

namespace VS::Effects {
namespace {
using Request = Effects_ActionEffectRequest;
using Instance = Effects_EffectInstance;
constexpr auto kActionTargets = std::extent_v<decltype(Request::targets)>;
constexpr auto kTransforms = std::extent_v<decltype(Instance::transforms)>;
constexpr auto kRenderers = std::extent_v<decltype(Effects_effectRenderers)>;
// Particle and sound keys are one word, renderer and shake keys two and
// vibration keys one halfword.
constexpr std::size_t kOneWordKey = 1;
constexpr std::size_t kTwoWordKey = 2;
constexpr std::size_t kVibrationKeyHalfwords = 1;
// A renderer key's second word holds its parameter's high half; the shake
// track starts its first key's channels, the key's second word.
constexpr std::size_t kSecondKeyWord = 1;
constexpr std::size_t kChildActorRegister = 1;
constexpr std::uint32_t kAllMotors = 0xFF;
constexpr int kEffectVibrationOwner = 1;
// A child track's key parameter holds its delay and next action target.
constexpr int kChildIndexShift = 16;
constexpr std::uint32_t kChildDelayMask = 0xFFFF;
constexpr std::string_view kVibrationCall = "vibration track";
static_assert(offsetof(Effects_ChildTrackView, delay) ==
              offsetof(Effects_TimelineTrack, keyParameter));
static_assert(offsetof(Effects_ChildTrackView, index) ==
              offsetof(Effects_TimelineTrack, keyParameter) +
                  sizeof(Effects_ChildTrackView::delay));
static_assert(sizeof(Effects_TimelineTrackHeader) == sizeof(std::uint16_t));

[[nodiscard]] Effects_EffectTimeline* TimelineOf(
    Effects_EffectInstance* instance, std::string_view call) {
  if (instance == nullptr || instance->context.timeline == nullptr) {
    RefuseTimelineCall(call, "no effect instance or timeline");
    return nullptr;
  }
  return instance->context.timeline;
}

[[nodiscard]] Request* RequestOf(std::string_view call) {
  if (Effects_package == nullptr || Effects_package->request == nullptr) {
    RefuseTimelineCall(call, "no effect package or action request");
    return nullptr;
  }
  return Effects_package->request;
}

// The tick's result for its step: 1 to keep the key, the advance's result
// for the next key and 0 once the track has finished.
int Finish(TrackStep step, Effects_TimelineTrack* track) {
  switch (step) {
    case TrackStep::kContinue:
      return 1;
    case TrackStep::kNextKey:
      return LiveAdvanceEffectTimelineKey(track);
    case TrackStep::kEnd:
      return 0;
  }
  return 0;
}

class LivePlaybackHost final : public TimelinePlaybackHost {
 public:
  LivePlaybackHost(Effects_EffectInstance& instance,
                   Effects_EffectTimeline& timeline, Request* request = nullptr,
                   int child_script_offset = 0)
      : instance_(&instance),
        timeline_(&timeline),
        request_(request),
        child_script_offset_(child_script_offset) {}

  bool FrameHeld() override {
    return (static_cast<std::uint8_t>(timeline_->flags) & kTimelineFrameHeld) !=
           0;
  }
  void SampleCurves(std::int16_t curve_frame) override {
    LiveSampleAllEffectCurves(curve_frame);
  }
  void EmitParticles(const ParticleEmission& emission) override {
    EmitLiveParticleEvent(instance_, emission.event, emission.transparency,
                          emission.variant, nullptr);
  }
  void PlayStream(std::int32_t handle) override {
    Audio::LivePlayCutsceneStreamSlotCentered(handle);
  }
  void PlaySfx(std::int32_t group, std::uint16_t effect) override {
    Audio::LivePlaySfxCentered(group, effect);
  }
  std::optional<Translation> TransformTranslation(
      std::uint8_t transform) override {
    if (transform >= kTransforms) {
      RefuseTimelineCall("sound track",
                         "a transform past the instance's sixteen");
      return std::nullopt;
    }
    const auto& world = std::span(instance_->transforms)[transform].world;
    return Translation{.x = world.t[0], .y = world.t[1], .z = world.t[2]};
  }
  void PlayWorldSfx(std::int32_t group, std::uint16_t effect,
                    const SoundPoint& point) override {
    SVECTOR at{point.x, point.y, point.z, 0};
    Audio::LivePlayWorldSfx(group, effect, &at);
  }
  void AttachRenderer(std::uint8_t renderer, std::int32_t parameters) override {
    if (renderer >= kRenderers) {
      RefuseTimelineCall("renderer track", "a renderer past the table");
      return;
    }
    static_cast<void>(LiveAttachEffectRenderer(
        instance_, std::span(Effects_effectRenderers)[renderer], parameters));
  }
  void Vibrate(std::uint8_t pattern) override {
    const std::span<const std::byte> block =
        LiveBoundPackageBlock(PackageBlockType::kSoundSequences);
    const auto offset = VibrationPatternOffset(block, pattern);
    if (!offset) {
      RefuseTimelineCall("vibration track",
                         "a pattern outside the package's block 11");
      return;
    }
    // The package may be released while the pattern plays: the slot keeps a
    // copy.
    static_cast<void>(Input::StartLiveOwnedVibrationPattern(
        block.subspan(*offset), kAllMotors, kEffectVibrationOwner));
  }
  void ShakeCamera(const ShakeChannels& channels) override {
    LiveStartCameraShakeChannels(
        channels.mask, channels.shape, channels.amplitude,
        channels.step_uses_curve ? 1 : 0, channels.step,
        channels.random_uses_curve ? 1 : 0, channels.random);
  }
  bool SpawnChild(std::size_t target,
                  std::optional<std::uint8_t> actor) override;

 private:
  Effects_EffectInstance* instance_;
  Effects_EffectTimeline* timeline_;
  Request* request_;
  int child_script_offset_;
};

bool LivePlaybackHost::SpawnChild(std::size_t target,
                                  std::optional<std::uint8_t> actor) {
  if (request_ == nullptr || target >= kActionTargets) {
    RefuseTimelineCall("child effect track", "no action target");
    return false;
  }
  Effects_ActionEffectTarget& endpoint = std::span(request_->targets)[target];
  Effects_EffectSpawnRequest spawn{};
  spawn.source = &request_->source;
  spawn.emitFrom = &request_->source;
  spawn.target = &endpoint;
  spawn.emitTo = &endpoint;
  spawn.script = Effects_package->block8Data;
  spawn.scriptOffset = child_script_offset_;
  spawn.parent = instance_;
  spawn.token = static_cast<u_char>(instance_->token);
  Effects_EffectInstance* child = LiveCreatePrimaryEffectInstance(&spawn);
  if (child == nullptr) {
    RefuseTimelineCall("child effect track", "no free effect instance");
    return false;
  }
  if (actor) {
    std::span(child->registers)[kChildActorRegister] =
        static_cast<char>(*actor);
  }
  return true;
}

// Runs a track's tick on its current key of `units_per_key` units.
template <typename Unit, typename Tick>
int RunTrack(Effects_EffectInstance* instance, Effects_TimelineTrack* track,
             std::string_view call, std::size_t units_per_key, Tick tick) {
  Effects_EffectTimeline* timeline = TimelineOf(instance, call);
  if (timeline == nullptr) return 0;
  const auto key = TimelineKeyUnits<Unit>(track, units_per_key, call);
  if (key.empty()) return 0;
  LivePlaybackHost host(*instance, *timeline);
  return Finish(tick(key, timeline->frame, host), track);
}

[[nodiscard]] ChildTrackKey ChildKeyOf(const Effects_TimelineTrack& track) {
  const auto& raw = *reinterpret_cast<const Effects_ChildTrackKey*>(track.keys);
  ChildTrackKey key{
      .final_actor = raw.final, .frame = raw.frame, .delay = raw.delay};
  std::ranges::copy(raw.actors, key.actors.begin());
  return key;
}

[[nodiscard]] ChildTarget ChildTargetOf(const Effects_ActionEffectTarget& at) {
  return {.type = at.type,
          .event_actor_slot =
              static_cast<std::uint8_t>(at.value.actor.eventActorSlot)};
}

[[nodiscard]] ChildTrackState ChildStateOf(
    const Effects_EffectInstance& instance,
    const Effects_TimelineTrack& track) {
  const auto parameter = static_cast<std::uint32_t>(track.keyParameter);
  return {.delay = static_cast<std::int16_t>(parameter & kChildDelayMask),
          .index = static_cast<std::int16_t>(parameter >> kChildIndexShift),
          .child_count = static_cast<std::uint8_t>(instance.childCount)};
}

void StoreChildState(const ChildTrackState& state,
                     Effects_EffectInstance& instance,
                     Effects_TimelineTrack& track) {
  track.keyParameter = static_cast<int>(
      static_cast<std::uint16_t>(state.delay) |
      (static_cast<std::uint32_t>(static_cast<std::uint16_t>(state.index))
       << kChildIndexShift));
  instance.childCount = state.child_count;
}

class LiveTimelineLoadHost final : public TimelineLoadHost {
 public:
  LiveTimelineLoadHost(Effects_EffectTimeline& timeline,
                       std::uint8_t track_count, const std::byte* section,
                       int timeline_index)
      : tracks_(timeline.tracks, track_count),
        section_(section),
        timeline_index_(timeline_index) {}

  // Binds the track's keys and reads its entry, which the key lookup has
  // found inside the timeline block.
  std::optional<std::uint16_t> TrackHeader(std::size_t track) override {
    auto* keys = Content::PFileLiveTimelineTrackData(
        Effects_package->block3Data, timeline_index_, static_cast<int>(track));
    if (keys == nullptr) return std::nullopt;
    tracks_[track].keys = reinterpret_cast<u_long*>(keys);
    std::uint16_t header{};
    std::memcpy(&header,
                section_ + Content::kPFileTimelineHeaderBytes +
                    (track * Content::kPFileTimelineTrackEntryBytes),
                sizeof header);
    return header;
  }
  void StartTrack(std::size_t track, const TimelineTrackStart& start) override {
    Effects_TimelineTrack& slot = tracks_[track];
    slot.header = std::bit_cast<Effects_TimelineTrackHeader>(start.header);
    slot.keyIndex = start.key_index;
    slot.curveFrame = start.timers.curve_frame;
    slot.keyTimer = start.timers.key_timer;
    slot.keyParameter = start.key_parameter;
    LiveAdvanceEffectTimelineKey(&slot);
  }

 private:
  std::span<Effects_TimelineTrack> tracks_;
  const std::byte* section_;
  int timeline_index_;
};

int TickTrack(Effects_EffectInstance* instance, Effects_TimelineTrack* track,
              TimelineTrackKind kind, bool all_targets,
              int child_script_offset) {
  switch (kind) {
    case TimelineTrackKind::kParticleEmission:
      return LiveTickParticleEmissionTrack(instance, track);
    case TimelineTrackKind::kSound:
      return LiveTickEffectSoundTrack(instance, track);
    case TimelineTrackKind::kChildEffects:
      return LiveTickChildEffectTrack(instance, track, child_script_offset);
    case TimelineTrackKind::kRenderer:
      return LiveTickEffectRendererTrack(instance, track);
    case TimelineTrackKind::kVibration:
      return LiveTickControllerVibrationTrack(instance, track);
    case TimelineTrackKind::kSourceActorColour:
      return LiveTickSourceActorColorTrack(instance, track);
    case TimelineTrackKind::kTargetActorColour:
      return LiveTickTargetActorColorTrack(instance, track,
                                           all_targets ? 1 : 0);
    case TimelineTrackKind::kOtherActorColour:
      return LiveTickOtherActorColorTrack(instance, track);
    case TimelineTrackKind::kSceneColour:
      return LiveTickSceneColorTrack(instance, track);
    case TimelineTrackKind::kBackgroundColour:
      return LiveTickBackgroundColorTrack(instance, track);
    case TimelineTrackKind::kActionControl:
      return LiveTickActionControlTrack(instance, track);
    case TimelineTrackKind::kCameraShake:
      return LiveTickCameraShakeTrack(instance, track);
  }
  return 0;
}

class LiveTimelineRunner final : public TimelineRunner {
 public:
  LiveTimelineRunner(Effects_EffectInstance& instance,
                     Effects_EffectTimeline& timeline, int child_script_offset)
      : instance_(&instance),
        tracks_(timeline.tracks,
                static_cast<std::uint8_t>(timeline.trackCount)),
        flags_(&timeline.flags),
        child_script_offset_(child_script_offset) {}

  std::uint8_t Flags() override { return static_cast<std::uint8_t>(*flags_); }
  std::uint8_t KindOf(std::size_t track) override {
    return static_cast<std::uint8_t>(tracks_[track].header.kind);
  }
  bool Tick(std::size_t track, TimelineTrackKind kind,
            bool all_targets) override {
    return TickTrack(instance_, &tracks_[track], kind, all_targets,
                     child_script_offset_) != 0;
  }
  void UnknownKind(std::size_t /*track*/, std::uint8_t /*kind*/) override {
    RefuseTimelineCall("tick", "a track kind without a tick");
  }

 private:
  Effects_EffectInstance* instance_;
  std::span<Effects_TimelineTrack> tracks_;
  const char* flags_;
  int child_script_offset_;
};
}  // namespace

// decomp: func_800D55A4 (src/BATTLE/BATTLE.PRG/5BF94.c:7885)
void LiveLoadEffectTimeline(Effects_EffectInstance* instance,
                            int timeline_index, int /*unused*/) {
  if (instance == nullptr || Effects_package == nullptr) {
    RefuseTimelineCall("load", "no effect instance or package");
    return;
  }
  const std::byte* section = Content::PFileLiveTimelineSection(
      Effects_package->block3Data, timeline_index);
  if (section == nullptr) return;
  // The original reads the count word's low byte; the section lookup bounds
  // only the section's first byte.
  const auto track_count = std::to_integer<std::uint8_t>(*section);
  if (instance->context.timeline != nullptr) {
    Core::Memory::FreeLiveFromHighEnd(instance->context.timeline);
  }
  instance->context.timeline = static_cast<Effects_EffectTimeline*>(
      Core::Memory::AllocateLiveFromHighEnd(
          sizeof(Effects_EffectTimeline) +
          (std::size_t{track_count} * sizeof(Effects_TimelineTrack))));
  Effects_EffectTimeline* timeline = instance->context.timeline;
  if (timeline == nullptr) {
    RefuseTimelineCall("load", "no heap for the timeline");
    return;
  }
  timeline->trackCount = static_cast<char>(track_count);
  timeline->frame = 0;
  timeline->activeTracks = 0;
  LiveTimelineLoadHost host(*timeline, track_count, section, timeline_index);
  const LoadedTimeline loaded = LoadEffectTimeline(track_count, host);
  timeline->trackCount = static_cast<char>(loaded.track_count);
  timeline->flags = static_cast<char>(loaded.flags);
  timeline->activeTracks = static_cast<int>(loaded.active_tracks);
  VS::Core::LogDebug("effects", "timeline={} tracks={} active={:#x}",
                     timeline_index, loaded.track_count, loaded.active_tracks);
}

// decomp: func_800D57FC (src/BATTLE/BATTLE.PRG/5BF94.c:7950)
int LiveTickParticleEmissionTrack(Effects_EffectInstance* instance,
                                  Effects_TimelineTrack* track) {
  return RunTrack<u_long>(
      instance, track, "particle emission track", kOneWordKey,
      [track](std::span<const u_long> key, std::int16_t frame,
              TimelinePlaybackHost& host) {
        TrackTimers timers{.curve_frame = track->curveFrame,
                           .key_timer = track->keyTimer};
        const TrackStep step = TickParticleEmissionTrack(
            static_cast<std::uint32_t>(key.front()), frame, timers, host);
        track->curveFrame = timers.curve_frame;
        track->keyTimer = timers.key_timer;
        return step;
      });
}

// decomp: func_800D5904 (src/BATTLE/BATTLE.PRG/5BF94.c:7974)
int LiveTickEffectSoundTrack(Effects_EffectInstance* instance,
                             Effects_TimelineTrack* track) {
  return RunTrack<u_long>(
      instance, track, "sound track", kOneWordKey,
      [](std::span<const u_long> key, std::int16_t frame,
         TimelinePlaybackHost& host) {
        // Without a package there is no stream to play.
        const std::int32_t stream = Effects_package != nullptr
                                        ? Effects_package->auxiliarySoundHandle
                                        : 0;
        return TickSoundTrack(static_cast<std::uint32_t>(key.front()), frame,
                              stream, host);
      });
}

// decomp: func_800D5A98 (src/BATTLE/BATTLE.PRG/5BF94.c:8005)
int LiveTickChildEffectTrack(Effects_EffectInstance* instance,
                             Effects_TimelineTrack* track,
                             int child_script_offset) {
  constexpr std::string_view kCall = "child effect track";
  Effects_EffectTimeline* timeline = TimelineOf(instance, kCall);
  Request* request = timeline != nullptr ? RequestOf(kCall) : nullptr;
  if (request == nullptr) return 0;
  // The track reads its first key whichever key is current.
  if (track == nullptr || track->keys == nullptr) {
    RefuseTimelineCall(kCall, "no track keys");
    return 0;
  }
  std::array<ChildTarget, kActionTargets> targets{};
  std::ranges::transform(request->targets, targets.begin(), ChildTargetOf);
  ChildTrackState state = ChildStateOf(*instance, *track);
  LivePlaybackHost host(*instance, *timeline, request, child_script_offset);
  const TrackStep step = TickChildEffectTrack(
      ChildKeyOf(*track), timeline->frame,
      FirstActionEntries(targets, request->count, kCall), state, host);
  StoreChildState(state, *instance, *track);
  return Finish(step, track);
}

// decomp: func_800D5D74 (src/BATTLE/BATTLE.PRG/5BF94.c:8069)
int LiveTickEffectRendererTrack(Effects_EffectInstance* instance,
                                Effects_TimelineTrack* track) {
  return RunTrack<u_long>(
      instance, track, "renderer track", kTwoWordKey,
      [](std::span<const u_long> key, std::int16_t frame,
         TimelinePlaybackHost& host) {
        return TickRendererTrack(
            {.timing = static_cast<std::uint32_t>(key.front()),
             .parameter_high = static_cast<std::uint16_t>(key[kSecondKeyWord])},
            frame, host);
      });
}

// decomp: func_800D5E00 (src/BATTLE/BATTLE.PRG/5BF94.c:8081)
int LiveTickControllerVibrationTrack(Effects_EffectInstance* instance,
                                     Effects_TimelineTrack* track) {
  const Request* request = RequestOf(kVibrationCall);
  if (request == nullptr) return 0;
  return RunTrack<u_short>(
      instance, track, kVibrationCall, kVibrationKeyHalfwords,
      [instance, request](std::span<const u_short> key, std::int16_t frame,
                          TimelinePlaybackHost& host) {
        std::array<CommandEndpoint, kActionTargets> targets{};
        std::ranges::transform(request->targets, targets.begin(),
                               CommandEndpointOf);
        const VibrationEndpoints endpoints{
            .source = CommandEndpointOf(instance->context.source),
            .target = CommandEndpointOf(instance->context.target),
            .action_targets =
                FirstActionEntries(targets, request->count, kVibrationCall)};
        return TickVibrationTrack(key.front(), frame, endpoints, host);
      });
}

// decomp: func_800D6A18 (src/BATTLE/BATTLE.PRG/5BF94.c:8422)
int LiveTickCameraShakeTrack(Effects_EffectInstance* instance,
                             Effects_TimelineTrack* track) {
  return RunTrack<u_long>(
      instance, track, "camera shake track", kTwoWordKey,
      [track](std::span<const u_long> key, std::int16_t frame,
              TimelinePlaybackHost& host) {
        // The original starts the first key's channels, whichever key is
        // current.
        const auto channels = static_cast<std::uint32_t>(
            std::span(track->keys, kTwoWordKey)[kSecondKeyWord]);
        return TickCameraShakeTrack(static_cast<std::uint16_t>(key.front()),
                                    channels, frame, track->keyTimer, host);
      });
}

// decomp: func_800D6AEC (src/BATTLE/BATTLE.PRG/5BF94.c:8442)
void LiveTickEffectTimeline(Effects_EffectInstance* instance,
                            int child_script_offset) {
  Effects_EffectTimeline* timeline = TimelineOf(instance, "tick");
  if (timeline == nullptr) return;
  TimelineProgress progress{
      .track_count = static_cast<std::uint8_t>(timeline->trackCount),
      .frame = timeline->frame,
      .active_tracks = static_cast<std::uint32_t>(timeline->activeTracks)};
  LiveTimelineRunner runner(*instance, *timeline, child_script_offset);
  TickEffectTimeline(progress, runner);
  timeline->activeTracks = static_cast<int>(progress.active_tracks);
  timeline->frame = progress.frame;
}
}  // namespace VS::Effects
