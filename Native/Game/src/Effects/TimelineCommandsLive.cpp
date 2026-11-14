// Hand-maintained (tools/port/owned.csv).
#include "Effects/TimelineCommandsLive.hpp"

#include <bit>
#include <cstdint>
#include <optional>
#include <string_view>

#include "Actors/AppearanceLive.hpp"
#include "Audio/ActorSoundLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Core/Log.hpp"
#include "Effects/TimelineCommands.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Core/BiosRand.h"
#include "Core/Gte.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Field/ChestInteractionsLive.h"
#include "Field/InteractionTargetLive.h"
}

namespace VS::Effects {
namespace {
std::uint64_t refused_instances = 0;

// Reports a null instance; the first refusal and each power of two after it
// are logged.
[[nodiscard]] bool Refused(const Effects_EffectInstance* instance,
                           std::string_view command) {
  if (instance != nullptr) return false;
  if (std::has_single_bit(++refused_instances)) {
    VS_LOG_WARN("effects",
                "timeline command {} refused: no effect instance ({} "
                "refusals)",
                command, refused_instances);
  }
  return true;
}

[[nodiscard]] CommandEndpoint EndpointOf(
    const Effects_ActionEffectTarget& endpoint) {
  const SVECTOR& point = endpoint.value.position;
  return {
      .type = endpoint.type,
      .actor = static_cast<std::uint8_t>(endpoint.value.actor.actor),
      .point = {.x = point.vx, .y = point.vy, .z = point.vz, .pad = point.pad}};
}

[[nodiscard]] CommandEndpoints EndpointsOf(
    const Effects_EffectInstance& instance) {
  const Effects_EffectInstanceTargets& context = instance.context;
  return {.source = EndpointOf(context.source),
          .emit_from = EndpointOf(context.emitFrom),
          .target = EndpointOf(context.target),
          .emit_to = EndpointOf(context.emitTo)};
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
}  // namespace

void LiveApplyTargetActionImpact(Effects_EffectInstance* instance) {
  if (Refused(instance, "target impact")) return;
  ApplyImpact(TargetActionImpact(EndpointsOf(*instance)),
              Battle::ApplyLiveActionTargetEffects);
}

void LiveApplySourceActionImpact(Effects_EffectInstance* instance) {
  if (Refused(instance, "source impact")) return;
  ApplyImpact(SourceActionImpact(EndpointsOf(*instance)),
              Battle::ApplyLiveActionSourceEffects);
}

void LiveActivateTargetRoomTile(Effects_EffectInstance* instance) {
  if (Refused(instance, "room tile")) return;
  if (const auto tile = TargetRoomTile(EndpointsOf(*instance))) {
    SVECTOR point = PointOf(*tile);
    Field_DisableTrapAtPositionEntry(&point);
  }
}

void LiveSignalSourceActorFromTimeline(Effects_EffectInstance* instance) {
  if (Refused(instance, "signal source actor")) return;
  if (const auto actor = TimelineSourceActorToSignal(EndpointsOf(*instance))) {
    Effects_SignalActorEffectAnimation(*actor);
  }
}

void LiveSetSourceWeaponBrightness(Effects_EffectInstance* instance,
                                   std::uint8_t brightness) {
  if (Refused(instance, "weapon brightness")) return;
  if (const auto override =
          SourceWeaponBrightness(EndpointsOf(*instance), brightness)) {
    Actors::SetLiveWeaponBrightnessOverride(
        override->actor, override->enabled ? 1 : 0, override->brightness);
  }
}

void LivePlaySourceActorReactionSound(Effects_EffectInstance* instance,
                                      std::uint8_t variant) {
  if (Refused(instance, "reaction sound")) return;
  const auto sound =
      SourceReactionSound(EndpointsOf(*instance), variant,
                          Effects_lastReactionSoundVariant, Core_Random_Rand);
  if (!sound) return;
  Effects_lastReactionSoundVariant = sound->last_variant;
  Audio::LivePlayActorModelSoundVariant(sound->actor, sound->variant);
}

void LiveActivateTargetGimmick(Effects_EffectInstance* instance) {
  if (Refused(instance, "room object")) return;
  if (const auto object = TargetGimmick(EndpointsOf(*instance))) {
    SVECTOR point = PointOf(*object);
    Field_ApplyUnlockEffectToRoomObjectEntry(&point);
  }
}

void LivePrepareTargetDefenseTiming(Effects_EffectInstance* instance) {
  if (Refused(instance, "defense timing")) return;
  if (const auto actor = DefenseTimingTarget(EndpointsOf(*instance))) {
    Battle::LiveArmDefenseAbilityTimingWindow(*actor);
  }
}
}  // namespace VS::Effects
