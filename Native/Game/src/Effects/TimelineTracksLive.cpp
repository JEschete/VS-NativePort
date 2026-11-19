// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelineTracksLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "Actors/ActorFlags.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Runtime.h"
#include "Effects/Timeline.hpp"
#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelineCommandsLive.hpp"
#include "Effects/TimelineTrackKeys.hpp"
#include "Effects/TimelineTracks.hpp"
#include "Render/PaletteAnimationLive.hpp"

namespace VS::Effects {
namespace {
// A particle key is one word; a shake key's second word holds its curve.
constexpr std::size_t kParticleKeyWords = 1;
constexpr std::size_t kShakeKeyWords = 2;
constexpr std::size_t kShakeParametersWord = 1;
using Request = Effects_ActionEffectRequest;
// A colour key is two words, stored as ColourKey lays them out.
static_assert(sizeof(ColourKey) == 2 * sizeof(std::uint32_t));
static_assert(std::is_trivially_copyable_v<ColourKey>);
constexpr auto kActionTargets = std::extent_v<decltype(Request::targets)>;
constexpr auto kActionOthers = std::extent_v<decltype(Request::others)>;

class LiveColourTrackHost final : public ColourTrackHost {
 public:
  explicit LiveColourTrackHost(LiveTimelineTrack& track) : track_(&track) {}

  bool ActorVisible(std::uint8_t actor) override {
    return (Actors::LiveActorStateFlags(actor) & Actors::StateFlag::kVisible) !=
           0;
  }
  void ShiftActorColours(std::uint8_t actor,
                         const ColourShift& shift) override {
    Render::TransformLiveActorColors(shift.mode, shift.duration, actor,
                                     shift.red, shift.green, shift.blue);
  }
  void ShiftSceneColours(const ColourShift& shift) override {
    Render::TransformLiveSceneColors(shift.mode, shift.duration, shift.red,
                                     shift.green, shift.blue);
  }
  void ShiftBackgroundColours(const ColourShift& shift) override {
    Render::TransformLiveBackgroundColors(shift.mode, shift.duration, shift.red,
                                          shift.green, shift.blue);
  }
  bool AdvanceKey() override {
    return LiveAdvanceEffectTimelineKey(track_) != 0;
  }

 private:
  LiveTimelineTrack* track_;
};

// The current colour key and the timeline frame a colour track compares, or
// nullopt (refused).
struct ColourTrackState {
  ColourKey key;
  std::int16_t frame{};
};

[[nodiscard]] std::optional<ColourTrackState> ColourTrackStateOf(
    const Effects_EffectInstance* instance, const LiveTimelineTrack* track,
    std::string_view call) {
  if (instance == nullptr || instance->context.timeline == nullptr) {
    RefuseTimelineCall(call, "no effect instance or timeline");
    return std::nullopt;
  }
  const auto bytes =
      TimelineKeyUnits<std::byte>(track, sizeof(ColourKey), call);
  if (bytes.empty()) return std::nullopt;
  ColourKey key;
  std::memcpy(&key, bytes.data(), sizeof key);
  return ColourTrackState{.key = key,
                          .frame = instance->context.timeline->frame};
}

// Runs a colour track's tick on its current key: 1 while it runs, 0 once it
// has no more keys or when refused.
template <typename Tick>
int RunColourTrack(Effects_EffectInstance* instance, LiveTimelineTrack* track,
                   std::string_view call, Tick tick) {
  const auto state = ColourTrackStateOf(instance, track, call);
  if (!state) return 0;
  LiveColourTrackHost host(*track);
  return tick(state->key, state->frame, host) ? 1 : 0;
}
}  // namespace

void LiveInitializeParticleTimelineKey(LiveTimelineTrack* track) {
  const auto words =
      TimelineKeyUnits<std::uint32_t>(track, kParticleKeyWords, "particle key");
  if (words.empty()) return;
  const ParticleKeyTiming timing =
      ParticleKeyTimingOf(static_cast<std::uint32_t>(words.front()));
  track->key_parameter = timing.parameter;
  track->key_timer = timing.timer;
}

void LiveInitializeShakeTimelineKey(LiveTimelineTrack* track) {
  const auto words =
      TimelineKeyUnits<std::uint32_t>(track, kShakeKeyWords, "shake key");
  if (words.empty()) return;
  track->key_timer =
      ShakeKeyTimer(static_cast<std::uint32_t>(words[kShakeParametersWord]),
                    [](std::uint8_t curve) {
                      return LiveSegmentedEffectCurveDuration(curve);
                    });
}

int LiveAdvanceEffectTimelineKey(LiveTimelineTrack* track) {
  if (track == nullptr) {
    RefuseTimelineCall("key advance", "no track");
    return 0;
  }
  const auto next = AdvanceTimelineKey(
      track->key_index, static_cast<std::uint8_t>(track->header.key_count),
      static_cast<std::uint8_t>(track->header.kind));
  if (!next) return 0;
  track->key_index = next->key_index;
  switch (next->setup) {
    case TimelineKeySetup::kParticleEmission:
      LiveInitializeParticleTimelineKey(track);
      break;
    case TimelineKeySetup::kCameraShake:
      LiveInitializeShakeTimelineKey(track);
      break;
    case TimelineKeySetup::kNone:
      break;
  }
  return 1;
}

int LiveTickSourceActorColorTrack(Effects_EffectInstance* instance,
                                  LiveTimelineTrack* track) {
  return RunColourTrack(
      instance, track, "source actor colour track",
      [instance](const ColourKey& key, std::int16_t frame,
                 ColourTrackHost& host) {
        return TickSourceActorColourTrack(
            key, frame, CommandEndpointOf(instance->context.emitFrom), host);
      });
}

int LiveTickTargetActorColorTrack(Effects_EffectInstance* instance,
                                  LiveTimelineTrack* track,
                                  int apply_to_all_targets) {
  return RunColourTrack(
      instance, track, "target actor colour track",
      [instance, apply_to_all_targets](const ColourKey& key, std::int16_t frame,
                                       ColourTrackHost& host) {
        const bool all_targets = apply_to_all_targets != 0;
        std::array<CommandEndpoint, kActionTargets> targets{};
        std::span<const CommandEndpoint> action_targets;
        if (all_targets) {
          std::ranges::transform(Effects_actionRequest.targets, targets.begin(),
                                 CommandEndpointOf);
          action_targets =
              FirstActionEntries(targets, Effects_actionRequest.count,
                                 "target actor colour track");
        }
        return TickTargetActorColourTrack(
            key, frame, action_targets,
            CommandEndpointOf(instance->context.emitTo), all_targets, host);
      });
}

int LiveTickOtherActorColorTrack(Effects_EffectInstance* instance,
                                 LiveTimelineTrack* track) {
  return RunColourTrack(
      instance, track, "other actor colour track",
      [](const ColourKey& key, std::int16_t frame, ColourTrackHost& host) {
        std::array<std::uint8_t, kActionOthers> others{};
        std::ranges::copy(Effects_actionRequest.others, others.begin());
        return TickOtherActorColourTrack(
            key, frame,
            FirstActionEntries(others, Effects_actionRequest.otherCount,
                               "other actor colour track"),
            host);
      });
}

int LiveTickSceneColorTrack(Effects_EffectInstance* instance,
                            LiveTimelineTrack* track) {
  return RunColourTrack(instance, track, "scene colour track",
                        TickSceneColourTrack);
}

int LiveTickBackgroundColorTrack(Effects_EffectInstance* instance,
                                 LiveTimelineTrack* track) {
  return RunColourTrack(instance, track, "background colour track",
                        TickBackgroundColourTrack);
}
}  // namespace VS::Effects
