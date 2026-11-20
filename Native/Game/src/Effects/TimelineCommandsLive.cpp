// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelineCommandsLive.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "Actors/AppearanceLive.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/ActionEffectCallbacksLive.hpp"
#include "Effects/ActionRequest.hpp"
#include "Effects/BoneTrailLive.hpp"
#include "Effects/CameraLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/SpawnRequest.hpp"
#include "Effects/Timeline.hpp"
#include "Effects/TimelineCommands.hpp"
#include "Effects/TimelineTracksLive.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Field/ChestInteractionsLive.hpp"
#include "Field/InteractionTargetLive.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Legacy/Psx.h"

namespace VS::Effects {
namespace {
// An action-control key is one word, stored as ActionControlKey lays it out.
static_assert(sizeof(ActionControlKey) == sizeof(std::uint32_t));
static_assert(std::is_trivially_copyable_v<ActionControlKey>);

std::uint64_t refused_instances = 0;
std::uint64_t refused_records = 0;

// Reports a null instance; the first refusal and each power of two after it
// are logged.
[[nodiscard]] bool Refused(const LiveEffectInstance* instance,
                           std::string_view command) {
  if (instance != nullptr) return false;
  if (std::has_single_bit(++refused_instances)) {
    VS::Core::LogWarn("effects",
                      "timeline command {} refused: no effect instance ({} "
                      "refusals)",
                      command, refused_instances);
  }
  return true;
}

// Reports a missing transform, timeline or track key, rate-limited as Refused.
void ReportMissing(std::string_view command, std::string_view missing) {
  if (std::has_single_bit(++refused_records)) {
    VS::Core::LogWarn("effects",
                      "timeline command {} refused: {} ({} refusals)", command,
                      missing, refused_records);
  }
}

// The transform's motion record, or null (refused) past the instance's
// transforms.
[[nodiscard]] LiveTransformSlot* MotionOf(LiveEffectInstance& instance,
                                          std::uint8_t transform_index,
                                          std::string_view command) {
  const auto slot = TransformSlotOf(transform_index);
  if (!slot) {
    ReportMissing(command, "transform past the instance's slots");
    return nullptr;
  }
  return &std::span(instance.transforms)[*slot];
}

[[nodiscard]] CommandEndpoints EndpointsOf(const LiveEffectInstance& instance) {
  const LiveEffectInstanceTargets& context = instance.context;
  return {.source = CommandEndpointOf(context.source),
          .emit_from = CommandEndpointOf(context.emit_from),
          .target = CommandEndpointOf(context.target),
          .emit_to = CommandEndpointOf(context.emit_to)};
}

[[nodiscard]] SVECTOR PointOf(const EndpointPoint& point) {
  return {.vx = point.x, .vy = point.y, .vz = point.z, .pad = point.pad};
}

template <typename Apply>
void ApplyImpact(const ActionImpact& impact, Apply apply) {
  if (!impact.point) {
    apply(impact.actor, nullptr);
    return;
  }
  SVECTOR point = PointOf(*impact.point);
  apply(impact.actor, &point);
}

class LiveTimelineEffectHost final : public TimelineEffectHost {
 public:
  void SaveResourceContext() override { LiveSaveEffectResourceContext(); }
  void RestoreResourceContext() override { LiveRestoreEffectResourceContext(); }
  void SpawnBoneTrail(const WeaponTrail& trail) override {
    LiveSpawnBoneTrail(trail.actor, trail.bone, trail.color, trail.texture);
  }
  void FadeBoneTrails() override { LiveFadeBoneTrails(); }
  void FreezeRoomObjects() override {
    static_cast<void>(Field::FreezeLiveRoomGimmicksTemporarily());
  }
};

// Performs the commands on the instance whose track is ticking.
class LiveActionControlHost final : public ActionControlHost {
 public:
  LiveActionControlHost(LiveEffectInstance& instance, LiveTimelineTrack& track)
      : instance_(&instance), track_(&track) {}

  void Perform(const ActionControl& control) override;
  bool AdvanceKey() override {
    return LiveAdvanceEffectTimelineKey(track_) != 0;
  }

 private:
  LiveEffectInstance* instance_;
  LiveTimelineTrack* track_;
};

// The track's current key, or nullopt (refused) without keys or past them.
[[nodiscard]] std::optional<ActionControlKey> CurrentKeyOf(
    const LiveTimelineTrack& track) {
  const int index = track.key_index;
  if (track.keys == nullptr || index < 0 || index >= track.header.key_count) {
    ReportMissing("action-control track", "no current key");
    return std::nullopt;
  }
  ActionControlKey key;
  std::memcpy(&key, &std::span(track.keys, track.header.key_count)[index],
              sizeof key);
  return key;
}
}  // namespace

CommandEndpoint CommandEndpointOf(const LiveActionEffectTarget& endpoint) {
  const SVECTOR& point = endpoint.position;
  return {
      .type = endpoint.type,
      .actor = static_cast<std::uint8_t>(endpoint.Actor().actor),
      .point = {.x = point.vx, .y = point.vy, .z = point.vz, .pad = point.pad}};
}

void LiveApplyTargetActionImpact(LiveEffectInstance* instance) {
  if (Refused(instance, "target impact")) return;
  ApplyImpact(TargetActionImpact(EndpointsOf(*instance)),
              Battle::ApplyLiveActionTargetEffects);
}

void LiveApplySourceActionImpact(LiveEffectInstance* instance) {
  if (Refused(instance, "source impact")) return;
  ApplyImpact(SourceActionImpact(EndpointsOf(*instance)),
              Battle::ApplyLiveActionSourceEffects);
}

void LiveActivateTargetRoomTile(LiveEffectInstance* instance) {
  if (Refused(instance, "room tile")) return;
  if (const auto tile = TargetRoomTile(EndpointsOf(*instance))) {
    SVECTOR point = PointOf(*tile);
    VS::Field::DisableTrapAtPositionEntry(&point);
  }
}

void LiveSignalSourceActorFromTimeline(LiveEffectInstance* instance) {
  if (Refused(instance, "signal source actor")) return;
  if (const auto actor = TimelineSourceActorToSignal(EndpointsOf(*instance))) {
    VS::Effects::LiveSignalActorEffectAnimation(*actor);
  }
}

void LiveSetSourceWeaponBrightness(LiveEffectInstance* instance,
                                   std::uint8_t brightness) {
  if (Refused(instance, "weapon brightness")) return;
  if (const auto override =
          SourceWeaponBrightness(EndpointsOf(*instance), brightness)) {
    Actors::SetLiveWeaponBrightnessOverride(
        override->actor, override->enabled ? 1 : 0, override->brightness);
  }
}

void LivePlaySourceActorReactionSound(LiveEffectInstance* instance,
                                      std::uint8_t variant) {
  if (Refused(instance, "reaction sound")) return;
  const auto sound =
      SourceReactionSound(EndpointsOf(*instance), variant,
                          lastReactionSoundVariant, VS::Core::NextBiosRand);
  if (!sound) return;
  lastReactionSoundVariant = sound->last_variant;
  Audio::LivePlayActorModelSoundVariant(sound->actor, sound->variant);
}

void LiveActivateTargetGimmick(LiveEffectInstance* instance) {
  if (Refused(instance, "room object")) return;
  if (const auto object = TargetGimmick(EndpointsOf(*instance))) {
    SVECTOR point = PointOf(*object);
    VS::Field::ApplyUnlockEffectToRoomObjectEntry(&point);
  }
}

void LivePrepareTargetDefenseTiming(LiveEffectInstance* instance) {
  if (Refused(instance, "defense timing")) return;
  if (const auto actor = DefenseTimingTarget(EndpointsOf(*instance))) {
    Battle::LiveArmDefenseAbilityTimingWindow(*actor);
  }
}

void LiveSpawnTransformWeaponTrail(LiveEffectInstance* instance,
                                   std::uint8_t transform_index,
                                   std::uint8_t packed_style) {
  if (Refused(instance, "weapon trail")) return;
  const auto* motion = MotionOf(*instance, transform_index, "weapon trail");
  if (motion == nullptr) return;
  LiveTimelineEffectHost host;
  SpawnTransformWeaponTrail({.actor = motion->actor, .bone = motion->bone},
                            packed_style, host);
}

void LiveFadeActionWeaponTrails() {
  LiveTimelineEffectHost host;
  FadeActionWeaponTrails(host);
}

void LiveResumeEffectTransformMotion(LiveEffectInstance* instance,
                                     std::uint8_t transform_index) {
  if (Refused(instance, "resume transform")) return;
  if (auto* motion = MotionOf(*instance, transform_index, "resume transform")) {
    motion->flags = ResumeTransformMotion(motion->flags);
  }
}

void LiveHoldEffectTimelineFrame(LiveEffectInstance* instance) {
  if (Refused(instance, "hold frame")) return;
  LiveEffectTimeline* timeline = instance->context.timeline;
  if (timeline == nullptr) {
    ReportMissing("hold frame", "no timeline");
    return;
  }
  timeline->flags = HoldTimelineFrame(timeline->flags);
}

void LivePauseMovingRoomObjects() {
  LiveTimelineEffectHost host;
  PauseMovingRoomObjects(host);
}

void LiveInvalidateEffectTransformAttachments(LiveEffectInstance* instance,
                                              std::uint8_t transform_index) {
  if (Refused(instance, "invalidate transform")) return;
  if (auto* motion =
          MotionOf(*instance, transform_index, "invalidate transform")) {
    motion->flags = InvalidateTransformAttachments(motion->flags);
  }
}

// NOLINTNEXTLINE(readability-function-size): one call per command.
void LiveActionControlHost::Perform(const ActionControl& control) {
  using enum ActionControlCommand;
  switch (control.command) {
    case kTargetImpact:
      return LiveApplyTargetActionImpact(instance_);
    case kSourceImpact:
      return LiveApplySourceActionImpact(instance_);
    case kTargetRoomTile:
      return LiveActivateTargetRoomTile(instance_);
    case kTransformWeaponTrail:
      return LiveSpawnTransformWeaponTrail(instance_, control.operand0,
                                           control.operand1);
    case kFadeWeaponTrails:
      return LiveFadeActionWeaponTrails();
    case kResumeTransformMotion:
      return LiveResumeEffectTransformMotion(instance_, control.operand0);
    case kHoldTimelineFrame:
      return LiveHoldEffectTimelineFrame(instance_);
    case kPauseRoomObjects:
      return LivePauseMovingRoomObjects();
    case kSignalSourceActor:
      return LiveSignalSourceActorFromTimeline(instance_);
    case kInvalidateTransformAttachments:
      return LiveInvalidateEffectTransformAttachments(instance_,
                                                      control.operand0);
    case kSourceWeaponBrightness:
      return LiveSetSourceWeaponBrightness(instance_, control.operand0);
    case kSourceReactionSound:
      return LivePlaySourceActorReactionSound(instance_, control.operand0);
    case kTargetGimmick:
      return LiveActivateTargetGimmick(instance_);
    case kFocusCamera:
      return LiveFocusCameraOnEffectTransform(instance_, control.operand0,
                                              control.operand1);
    case kRestoreCamera:
      return LiveRestoreActionEffectCamera(instance_, control.operand0);
    case kDefenseTiming:
      return LivePrepareTargetDefenseTiming(instance_);
  }
}

int LiveTickActionControlTrack(LiveEffectInstance* instance,
                               LiveTimelineTrack* track) {
  if (Refused(instance, "action-control track")) return 0;
  if (track == nullptr || instance->context.timeline == nullptr) {
    ReportMissing("action-control track", "no track or timeline");
    return 0;
  }
  const auto key = CurrentKeyOf(*track);
  if (!key) return 0;
  LiveActionControlHost host(*instance, *track);
  return TickActionControlTrack(*key, instance->context.timeline->frame, host)
             ? 1
             : 0;
}
}  // namespace VS::Effects
