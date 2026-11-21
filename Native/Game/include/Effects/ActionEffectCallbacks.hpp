#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/HudState.hpp"
#include "Effects/SecondaryEffectRequests.hpp"
#include "Field/GameMode.hpp"
#include "Render/PaletteAnimation.hpp"

// The standalone presentation and actor callbacks around the action effect
// lifecycle (BATTLE.PRG 146C.c:2149-5064): the effect script's actor
// animation signal, the red action tint and its paired restore, the
// Ashley-only standalone effect's start and end, and the busy mask the trap
// and standalone waits poll. The rules pick the values; the host performs
// every effect on actors, palettes, the effect runtime, the HUD, the minimap
// and the game clock.
namespace VS::Effects {
// LiveActionEffectRequest.others: the actors a primary request includes
// besides its target.
inline constexpr std::size_t kPrimaryEffectOthers = 16;

// A primary (action) effect request as func_800CF0E8 takes it: one target,
// the other actors and a position. A request defines only the fields its
// producer writes; the others stay zero (the original left them as stack
// bytes).
struct PrimaryEffectRequest {
  std::uint16_t effect_id{};
  EffectEndpoint source{}, target{};
  std::uint8_t other_count{};
  std::array<std::uint8_t, kPrimaryEffectOthers> others{};
  EffectPoint position{};
  friend bool operator==(const PrimaryEffectRequest&,
                         const PrimaryEffectRequest&) = default;
};

// The action variant and its argument every request here begins with
// (func_800CF0E8's second and third arguments).
inline constexpr std::int32_t kPrimaryRequestVariant = 0;
inline constexpr std::int32_t kPrimaryRequestArgument = 0;

// Where primary requests go.
class PrimaryEffectSink {
 public:
  virtual ~PrimaryEffectSink() = default;
  // func_800CF0E8: begins the request's primary effect.
  virtual void BeginActionEffect(const PrimaryEffectRequest& request,
                                 std::int32_t action_variant,
                                 std::int32_t variant_argument) = 0;

 protected:
  PrimaryEffectSink() = default;
  PrimaryEffectSink(const PrimaryEffectSink&) = default;
  PrimaryEffectSink& operator=(const PrimaryEffectSink&) = default;
  PrimaryEffectSink(PrimaryEffectSink&&) = default;
  PrimaryEffectSink& operator=(PrimaryEffectSink&&) = default;
};

// An actor palette transform (func_8007B29C's arguments).
struct ActorColorTransform {
  Render::ColorTransform mode{};
  std::int32_t duration{};
  std::int32_t actor{};
  std::int16_t red_offset{}, green_offset{}, blue_offset{};
  friend bool operator==(const ActorColorTransform&,
                         const ActorColorTransform&) = default;
};

// The red action tint: fade to +16 red over 8 frames, then the restore to
// the base palette over 4 frames.
inline constexpr std::int32_t kActionTintDuration = 8;
inline constexpr std::int16_t kActionTintRedOffset = 0x10;
inline constexpr std::int32_t kActionTintRestoreDuration = 4;
// tintedActor once the tint is restored.
inline constexpr std::int32_t kNoTintedActor = 0;

// effectBusyFlags bits the trap and standalone waits observe: the
// primary (1) and secondary (2) effects' busy bits.
inline constexpr std::uint32_t kActionEffectBusyMask = 3;

// The services the callbacks use.
class ActionEffectCallbackHost : public PrimaryEffectSink {
 public:
  // func_8009D854: sets the actor's effect-synchronized animation flag.
  virtual void RequestActorAnimationAdvance(std::int32_t actor) = 0;
  // func_8007B29C.
  virtual void TransformActorColors(const ActorColorTransform& transform) = 0;
  // tintedActor (D_800F185C).
  [[nodiscard]] virtual std::int32_t TintedActor() = 0;
  virtual void SetTintedActor(std::int32_t actor) = 0;
  // VS::Field::controlMode (D_800F196C).
  virtual void SetFieldControlMode(Field::FieldControlMode mode) = 0;
  // VS::State::tickSpeed (vs_gametime_tickspeed).
  virtual void SetTickSpeed(std::int32_t tick_speed) = 0;
  // func_800CB654.
  virtual void SetHudVisibility(Battle::HudVisibility visibility) = 0;
  // func_8008B4C8.
  virtual void SetMinimapVisible(bool visible) = 0;
  // func_8006F5CC.
  virtual void EnterFieldExplorationMode() = 0;
  // effectBusyFlags (D_800F1868).
  [[nodiscard]] virtual std::uint32_t EffectBusyFlags() = 0;
};

// decomp: BATTLE.PRG func_8006CDB8 (F00064,
// src/BATTLE/BATTLE.PRG/146C.c:1851, 0x8006CDB8-0x8006CDD8)
void SignalActorEffectAnimation(std::int32_t actor,
                                ActionEffectCallbackHost& host);

// The transform the red action tint applies to `actor`.
[[nodiscard]] ActorColorTransform ActionTint(std::int32_t actor);
// The transform that restores `actor` after it.
[[nodiscard]] ActorColorTransform ActionTintRestore(std::int32_t actor);

// decomp: BATTLE.PRG func_8006D028 (F00068,
// src/BATTLE/BATTLE.PRG/146C.c:1913, 0x8006D028-0x8006D064)
// Remembers `actor` as the tinted actor, then tints it red.
void TintActionActorRed(std::int32_t actor, ActionEffectCallbackHost& host);

// decomp: BATTLE.PRG func_8006D064 (F00069,
// src/BATTLE/BATTLE.PRG/146C.c:1919, 0x8006D064-0x8006D0A4)
// Restores the remembered actor's palette, then forgets it (actor 0).
void RestoreActionActorTint(ActionEffectCallbackHost& host);

// The request the Ashley-only standalone effect begins: `effect_id` from
// actor 0 to actor 0 at the origin, with no other actors.
[[nodiscard]] PrimaryEffectRequest StandaloneEffectRequest(
    std::uint16_t effect_id);

// decomp: BATTLE.PRG func_800733F8 (F00119,
// src/BATTLE/BATTLE.PRG/146C.c:4340, 0x800733F8-0x80073484)
// Enters the standalone effect mode, begins the effect, slows the clock and
// hides the HUD and the minimap.
void BeginAshleyStandaloneEffect(std::uint16_t effect_id,
                                 ActionEffectCallbackHost& host);

// decomp: BATTLE.PRG func_80073484 (F00120,
// src/BATTLE/BATTLE.PRG/146C.c:4365, 0x80073484-0x800734C0)
// Returns to exploration at the normal clock and shows the HUD and the
// minimap.
void EndAshleyStandaloneEffect(ActionEffectCallbackHost& host);

// decomp: BATTLE.PRG func_80074294 (F00143,
// src/BATTLE/BATTLE.PRG/146C.c:4794, 0x80074294-0x800742A4)
// The primary and secondary busy bits.
[[nodiscard]] std::uint32_t ActionEffectBusyFlags(
    ActionEffectCallbackHost& host);
}  // namespace VS::Effects
