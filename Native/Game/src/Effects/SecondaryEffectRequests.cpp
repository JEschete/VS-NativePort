// Hand-maintained (tools/port/owned.csv).
#include "Effects/SecondaryEffectRequests.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Effects {
namespace {
constexpr unsigned kByteBits = 8;

// Two hit word bytes read as one halfword of a position, low byte first.
[[nodiscard]] std::int16_t ByteHalfword(std::uint8_t low, std::uint8_t high) {
  return static_cast<std::int16_t>(
      static_cast<std::uint16_t>(low | (unsigned{high} << kByteBits)));
}

// The hit word of a position hit read as the position
// (VS::Battle::HitTargetWord copied over an SVECTOR).
[[nodiscard]] EffectPoint HitWordPosition(const ImpactTarget& target) {
  return {
      .x = ByteHalfword(target.actor, static_cast<std::uint8_t>(target.limb)),
      .y = ByteHalfword(target.threshold, target.impact),
      .z = target.hp,
      .w = target.mp};
}

[[nodiscard]] EffectEndpoint PositionEndpoint(EffectPoint position) {
  return {.kind = EffectEndpointKind::kPosition, .position = position};
}

// The source the spatial requests name: actor 0, part unset.
[[nodiscard]] EffectEndpoint PlayerSource() {
  return {.kind = EffectEndpointKind::kActor, .actor = 0};
}

// Tile coordinate to the centre of the tile in world units.
constexpr std::int32_t kTileSize = 128;
constexpr std::int32_t kTileCentre = kTileSize / 2;
[[nodiscard]] std::int16_t TileCentre(std::uint8_t tile) {
  return static_cast<std::int16_t>(tile * kTileSize + kTileCentre);
}

// The hit effect each actor impact kind of a defined action uses; kinds past
// the table have none.
enum class ImpactKindEffect : std::uint8_t {
  kDefaultHit,
  kKindOne,
  kPositionHit
};
constexpr std::array kImpactKindEffects{
    ImpactKindEffect::kDefaultHit,  ImpactKindEffect::kKindOne,
    ImpactKindEffect::kDefaultHit,  ImpactKindEffect::kPositionHit,
    ImpactKindEffect::kPositionHit, ImpactKindEffect::kDefaultHit,
    ImpactKindEffect::kPositionHit,
};

// The part func_8006F848's actor endpoint names.
constexpr std::uint8_t kAbilityLearnedPart = 0xFD;
}  // namespace

// decomp: func_8006C4C4 (src/BATTLE/BATTLE.PRG/146C.c:1927)
SingleTargetEffectRequest ActionHitEffectRequest(
    std::uint16_t effect_id, const QueuedEffectAction& action,
    const ImpactTarget& target, std::uint8_t limb, std::int32_t bone_tag) {
  SingleTargetEffectRequest request{
      .effect_id = effect_id,
      .source = {.kind = EffectEndpointKind::kActor,
                 .actor = action.source_actor,
                 .part = 0}};
  if (target.at_position) {
    request.target = PositionEndpoint(HitWordPosition(target));
  } else {
    request.target = {.kind = EffectEndpointKind::kActor,
                      .actor = target.actor,
                      .slot = limb,
                      .part = static_cast<std::uint8_t>(bone_tag)};
  }
  request.target.color_preset = action.effect_variant;
  return request;
}

// decomp: func_8006C4C4 (src/BATTLE/BATTLE.PRG/146C.c:1927)
bool QueueActionHitEffect(std::uint16_t effect_id, const ImpactTarget& target,
                          std::uint8_t limb, ActionImpactWorld& world) {
  const std::optional<QueuedEffectAction> action = world.QueuedAction();
  if (!action.has_value()) return false;
  std::int32_t bone_tag = 0;
  if (!target.at_position) {
    bone_tag = world.BoneTag(target.actor, target.limb, kHitEffectBoneTagKind);
  }
  world.Begin(
      ActionHitEffectRequest(effect_id, *action, target, limb, bone_tag),
      kSecondaryRequestParameter);
  return true;
}

// decomp: func_8006C84C (src/BATTLE/BATTLE.PRG/146C.c:2044)
std::optional<ImpactEffectChoice> ChooseImpactEffect(std::uint16_t action,
                                                     const ImpactTarget& target,
                                                     ActionImpactWorld& world) {
  if (target.at_position) return ImpactEffectChoice{kPositionHitEffect, 0};
  const std::uint8_t impact = target.impact;
  if (action >= kImpactDefinedActions ||
      world.HitPrerequisite(action) == kOwnEffectPrerequisite) {
    return ImpactEffectChoice{action, impact};
  }
  // The original's jump table (0x80068840) over actor impact kinds 0-6.
  if (impact >= kImpactKindEffects.size()) return std::nullopt;
  switch (kImpactKindEffects[impact]) {
    case ImpactKindEffect::kDefaultHit:
      return ImpactEffectChoice{
          action < kDefaultHitActions ? kDefaultHitEffect : action, impact};
    case ImpactKindEffect::kKindOne:
      return ImpactEffectChoice{kImpactKindOneEffect, impact};
    case ImpactKindEffect::kPositionHit:
      return ImpactEffectChoice{kPositionHitEffect, impact};
  }
  return std::nullopt;
}

// decomp: func_8006C84C (src/BATTLE/BATTLE.PRG/146C.c:2044)
std::int32_t QueueActionImpactEffects(std::int32_t attacker,
                                      ActionImpactWorld& world) {
  for (std::size_t index = 0;; ++index) {
    const std::optional<QueuedEffectAction> action = world.QueuedAction();
    if (!action.has_value() || index >= action->target_count) break;
    const std::optional<ImpactTarget> target = world.QueuedTarget(index);
    if (!target.has_value()) break;
    const std::optional<ImpactEffectChoice> choice =
        ChooseImpactEffect(action->action_id, *target, world);
    if (choice.has_value()) {
      static_cast<void>(QueueActionHitEffect(choice->effect_id, *target,
                                             choice->limb, world));
    }
    world.PlayWeaponHitSounds(attacker);
  }
  return 1;
}

// decomp: func_8006F450 (src/BATTLE/BATTLE.PRG/146C.c:3171)
SingleTargetEffectRequest GroundPlaneImpactEffectRequest(std::int16_t x,
                                                         std::int16_t z) {
  return {.effect_id = kGroundPlaneImpactEffect,
          .source = PlayerSource(),
          .target = PositionEndpoint({.x = x, .y = 0, .z = z})};
}

// decomp: func_8006F450 (src/BATTLE/BATTLE.PRG/146C.c:3171)
void SpawnGroundPlaneImpactEffect(std::int16_t x, std::int16_t z,
                                  SecondaryEffectSink& sink) {
  sink.Begin(GroundPlaneImpactEffectRequest(x, z), kSecondaryRequestParameter);
}

// decomp: func_8006F4B0 (src/BATTLE/BATTLE.PRG/146C.c:3187)
SingleTargetEffectRequest TrapTileEffectRequest(const TrapTile& tile) {
  return {.effect_id = kTrapTileEffect,
          .source = PlayerSource(),
          .target = PositionEndpoint({.x = TileCentre(tile.tile_x),
                                      .y = tile.height,
                                      .z = TileCentre(tile.tile_z)})};
}

// decomp: func_8006F4B0 (src/BATTLE/BATTLE.PRG/146C.c:3187)
bool SpawnTrapTileEffect(const TrapTile& tile, SecondaryEffectSink& sink) {
  const SingleTargetEffectRequest request = TrapTileEffectRequest(tile);
  if ((sink.BusyFlags() & kTrapEffectBusyBit) != 0) return false;
  sink.Begin(request, kSecondaryRequestParameter);
  return true;
}

// decomp: func_8006F848 (src/BATTLE/BATTLE.PRG/146C.c:3298)
SingleTargetEffectRequest AbilityLearnedEffectRequest() {
  return {.effect_id = kAbilityLearnedEffect,
          .source = PositionEndpoint({}),
          .target = {.kind = EffectEndpointKind::kActor,
                     .actor = 0,
                     .part = kAbilityLearnedPart}};
}

// decomp: func_8006F848 (src/BATTLE/BATTLE.PRG/146C.c:3298)
void SpawnAbilityLearnedEffect(SecondaryEffectSink& sink) {
  sink.Begin(AbilityLearnedEffectRequest(), kSecondaryRequestParameter);
}
}  // namespace VS::Effects
