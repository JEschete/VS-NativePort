// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelineTracksLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "Actors/ActorFlags.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Runtime.h"
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
using KeyRecord = Effects_ColorTrackKey;
using Request = Effects_ActionEffectRequest;
constexpr std::size_t kColourKeyWords = sizeof(KeyRecord) / sizeof(u_long);
static_assert(kColourKeyWords * sizeof(u_long) == sizeof(KeyRecord));
constexpr auto kActionTargets = std::extent_v<decltype(Request::targets)>;
constexpr auto kActionOthers = std::extent_v<decltype(Request::others)>;

class LiveColourTrackHost final : public ColourTrackHost {
 public:
  explicit LiveColourTrackHost(Effects_TimelineTrack& track) : track_(&track) {}

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
  Effects_TimelineTrack* track_;
};

// The current colour key and the timeline frame a colour track compares, or
// nullopt (refused).
struct ColourTrackState {
  ColourKey key;
  std::int16_t frame{};
};

[[nodiscard]] std::optional<ColourTrackState> ColourTrackStateOf(
    const Effects_EffectInstance* instance, const Effects_TimelineTrack* track,
    std::string_view call) {
  if (instance == nullptr || instance->context.timeline == nullptr) {
    RefuseTimelineCall(call, "no effect instance or timeline");
    return std::nullopt;
  }
  const auto words = TimelineKeyUnits<u_long>(track, kColourKeyWords, call);
  if (words.empty()) return std::nullopt;
  const auto& key =
      *reinterpret_cast<const Effects_ColorTrackKey*>(words.data());
  return ColourTrackState{.key = {.frame = key.frame,
                                  .duration = key.duration,
                                  .red = key.red,
                                  .green = key.green,
                                  .blue = key.blue,
                                  .mode = key.mode},
                          .frame = instance->context.timeline->frame};
}

// Runs a colour track's tick on its current key: 1 while it runs, 0 once it
// has no more keys or when refused.
template <typename Tick>
int RunColourTrack(Effects_EffectInstance* instance,
                   Effects_TimelineTrack* track, std::string_view call,
                   Tick tick) {
  const auto state = ColourTrackStateOf(instance, track, call);
  if (!state) return 0;
  LiveColourTrackHost host(*track);
  return tick(state->key, state->frame, host) ? 1 : 0;
}
}  // namespace

void LiveInitializeParticleTimelineKey(Effects_TimelineTrack* track) {
  const auto words =
      TimelineKeyUnits<u_long>(track, kParticleKeyWords, "particle key");
  if (words.empty()) return;
  const ParticleKeyTiming timing =
      ParticleKeyTimingOf(static_cast<std::uint32_t>(words.front()));
  track->keyParameter = timing.parameter;
  track->keyTimer = timing.timer;
}

void LiveInitializeShakeTimelineKey(Effects_TimelineTrack* track) {
  const auto words =
      TimelineKeyUnits<u_long>(track, kShakeKeyWords, "shake key");
  if (words.empty()) return;
  track->keyTimer =
      ShakeKeyTimer(static_cast<std::uint32_t>(words[kShakeParametersWord]),
                    [](std::uint8_t curve) {
                      return LiveSegmentedEffectCurveDuration(curve);
                    });
}

int LiveAdvanceEffectTimelineKey(Effects_TimelineTrack* track) {
  if (track == nullptr) {
    RefuseTimelineCall("key advance", "no track");
    return 0;
  }
  const auto next = AdvanceTimelineKey(
      track->keyIndex, static_cast<std::uint8_t>(track->header.keyCount),
      static_cast<std::uint8_t>(track->header.kind));
  if (!next) return 0;
  track->keyIndex = next->key_index;
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
                                  Effects_TimelineTrack* track) {
  return RunColourTrack(
      instance, track, "source actor colour track",
      [instance](const ColourKey& key, std::int16_t frame,
                 ColourTrackHost& host) {
        return TickSourceActorColourTrack(
            key, frame, CommandEndpointOf(instance->context.emitFrom), host);
      });
}

int LiveTickTargetActorColorTrack(Effects_EffectInstance* instance,
                                  Effects_TimelineTrack* track,
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
                                 Effects_TimelineTrack* track) {
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
                            Effects_TimelineTrack* track) {
  return RunColourTrack(instance, track, "scene colour track",
                        TickSceneColourTrack);
}

int LiveTickBackgroundColorTrack(Effects_EffectInstance* instance,
                                 Effects_TimelineTrack* track) {
  return RunColourTrack(instance, track, "background colour track",
                        TickBackgroundColourTrack);
}
}  // namespace VS::Effects
