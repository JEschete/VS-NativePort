// Hand-maintained (tools/port/owned.csv).
#include "Effects/ActionEffectCallbacks.hpp"

#include <cstdint>

#include "Battle/HudState.hpp"
#include "Field/GameMode.hpp"
#include "Render/PaletteAnimation.hpp"
#include "State/SceneTimer.hpp"

namespace VS::Effects {
namespace {
// The player's actor, and the part 0 the standalone endpoints name.
constexpr std::uint8_t kPlayerActor = 0;

[[nodiscard]] EffectEndpoint PlayerEndpoint() {
  return {.kind = EffectEndpointKind::kActor,
          .actor = kPlayerActor,
          .slot = 0,
          .part = 0};
}
}  // namespace

// decomp: BATTLE.PRG func_8006CDB8 (F00064,
// src/BATTLE/BATTLE.PRG/146C.c:1851, 0x8006CDB8-0x8006CDD8)
void SignalActorEffectAnimation(std::int32_t actor,
                                ActionEffectCallbackHost& host) {
  host.RequestActorAnimationAdvance(actor);
}

ActorColorTransform ActionTint(std::int32_t actor) {
  return {.mode = Render::ColorTransform::kFadeThenRestore,
          .duration = kActionTintDuration,
          .actor = actor,
          .red_offset = kActionTintRedOffset};
}

ActorColorTransform ActionTintRestore(std::int32_t actor) {
  return {.mode = Render::ColorTransform::kRestoreBase,
          .duration = kActionTintRestoreDuration,
          .actor = actor};
}

// decomp: BATTLE.PRG func_8006D028 (F00068,
// src/BATTLE/BATTLE.PRG/146C.c:1913, 0x8006D028-0x8006D064)
void TintActionActorRed(std::int32_t actor, ActionEffectCallbackHost& host) {
  host.SetTintedActor(actor);
  host.TransformActorColors(ActionTint(actor));
}

// decomp: BATTLE.PRG func_8006D064 (F00069,
// src/BATTLE/BATTLE.PRG/146C.c:1919, 0x8006D064-0x8006D0A4)
void RestoreActionActorTint(ActionEffectCallbackHost& host) {
  host.TransformActorColors(ActionTintRestore(host.TintedActor()));
  host.SetTintedActor(kNoTintedActor);
}

PrimaryEffectRequest StandaloneEffectRequest(std::uint16_t effect_id) {
  return {.effect_id = effect_id,
          .source = PlayerEndpoint(),
          .target = PlayerEndpoint(),
          .other_count = 0,
          .position = {}};
}

// decomp: BATTLE.PRG func_800733F8 (F00119,
// src/BATTLE/BATTLE.PRG/146C.c:4340, 0x800733F8-0x80073484)
void BeginAshleyStandaloneEffect(std::uint16_t effect_id,
                                 ActionEffectCallbackHost& host) {
  host.SetFieldControlMode(Field::FieldControlMode::kStandaloneEffect);
  host.BeginActionEffect(StandaloneEffectRequest(effect_id),
                         kPrimaryRequestVariant, kPrimaryRequestArgument);
  host.SetTickSpeed(State::kSlowedTickSpeed);
  host.SetHudVisibility(Battle::HudVisibility::kHidden);
  host.SetMinimapVisible(false);
}

// decomp: BATTLE.PRG func_80073484 (F00120,
// src/BATTLE/BATTLE.PRG/146C.c:4365, 0x80073484-0x800734C0)
void EndAshleyStandaloneEffect(ActionEffectCallbackHost& host) {
  host.EnterFieldExplorationMode();
  host.SetTickSpeed(State::kNormalTickSpeed);
  host.SetHudVisibility(Battle::HudVisibility::kShown);
  host.SetMinimapVisible(true);
}

// decomp: BATTLE.PRG func_80074294 (F00143,
// src/BATTLE/BATTLE.PRG/146C.c:4794, 0x80074294-0x800742A4)
std::uint32_t ActionEffectBusyFlags(ActionEffectCallbackHost& host) {
  return host.EffectBusyFlags() & kActionEffectBusyMask;
}
}  // namespace VS::Effects
