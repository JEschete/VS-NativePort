#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/AppearanceTransition.hpp"
#include "Actors/SequencePlayback.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/DepthCue.hpp"

// Appearance requests scripts and the battle flow make of an actor, its
// weapons or a field gimmick: the root spin, weapon visibility and brightness,
// the room's model scale, the targeting palette pulse, the shadow blend, the
// fade color and tint and the appearance transitions. Each works on the typed
// state below; AppearanceLive.hpp binds them to the actor and gimmick tables.
namespace VS::Actors {
// Actor records the battle tables hold (Actors_actorRecords).
inline constexpr int kActorTableSize = 17;
// Weapon models each actor slot owns (Actors_weaponModels).
inline constexpr std::size_t kWeaponsPerActor = 2;
// The fully shown weapon blend level.
inline constexpr std::uint8_t kWeaponBlendShown = 64;
// Palette pulse slots; slot s is bit 7 - s of the pulse kind.
inline constexpr int kPalettePulseSlots = 8;
// The pulse slot that names every slot at once.
inline constexpr int kEveryPalettePulseSlot = -1;
inline constexpr std::uint8_t kEveryPalettePulseKind = 0xFF;

// The original's return codes for an appearance request.
enum class AppearanceStatus : std::int8_t {
  kOk = 0,
  // The id names neither an actor nor (for the pulse) a gimmick.
  kNoModel = -1,
  // The pulse slot is past kPalettePulseSlots.
  kBadPulseSlot = -2,
};

// The actor's root spin (paletteFadeActive, paletteFadeReverse and
// paletteFadeState): while active the model drawer turns the root by half the
// phase.
struct PaletteFade {
  bool active = false;
  bool reverse = false;
  std::uint8_t phase = 0;
  std::uint8_t rate = 0;

  friend bool operator==(const PaletteFade&, const PaletteFade&) = default;
};

// func_8009D7E8: 0 stops the spin; any other parameter restarts it forward
// from phase 0 at twice that rate, truncated to a byte.
void SetActorPaletteFade(PaletteFade& fade, int fade_parameter);

// What the appearance requests read and write of a weapon model.
struct WeaponAppearance {
  // The weapon is attached to a bone (attachmentBone != 0).
  bool attached = false;
  bool shown = false;
  bool brightness_override = false;
  std::uint8_t brightness = 0;
  // The visibility blend level, its target and the frames left.
  std::uint8_t blend_level = 0;
  std::uint8_t blend_target = 0;
  std::uint8_t blend_frames = 0;

  friend bool operator==(const WeaponAppearance&,
                         const WeaponAppearance&) = default;
};

// func_8009D88C: settles the blend of every attached weapon at its shown
// level (kWeaponBlendShown or 0); weapons not attached are left alone.
void ResetActorWeaponVisibilityBlend(
    std::span<std::optional<WeaponAppearance>> weapons);

// func_8009F898: sets the brightness the weapon is drawn with and whether it
// replaces the weapon's base color.
void SetWeaponBrightnessOverride(WeaponAppearance& weapon, bool enabled,
                                 std::uint8_t brightness);

// func_8009FB64: stores the room's model scale and returns the scale every
// actor takes from it. The original gave each actor the halfword its render
// context kept at the stored x (D_800F4438), so the actor scale is x
// truncated to a signed halfword.
[[nodiscard]] std::int16_t SetRoomActorScale(Core::Math::Vec3& stored,
                                             Core::Math::Vec3 room_scale);

// A model's palette pulse: `kind` holds one bit per pulse slot.
struct PalettePulseRequest {
  std::uint8_t kind = 0;
  PalettePulse pulse;

  friend bool operator==(const PalettePulseRequest&,
                         const PalettePulseRequest&) = default;
};

struct PalettePulseChange {
  AppearanceStatus status = AppearanceStatus::kOk;
  // The pulse was idle and restarted, so the model's base palettes are
  // uploaded again before it pulses.
  bool restarted = false;

  friend bool operator==(const PalettePulseChange&,
                         const PalettePulseChange&) = default;
};

// func_8009FD5C: an idle pulse restarts at level 0 with the direction flag
// `pulse_flags & 1`; then pulse slot `pulse_slot` is armed, or every slot for
// kEveryPalettePulseSlot. A slot past the table is refused after the restart,
// as the original did.
[[nodiscard]] PalettePulseChange EnableModelPalettePulse(
    PalettePulseRequest& request, int pulse_flags, int pulse_slot);

// func_8009FE74: disarms pulse slot `pulse_slot`, or every slot for
// kEveryPalettePulseSlot.
[[nodiscard]] AppearanceStatus DisableModelPalettePulse(
    PalettePulseRequest& request, int pulse_slot);

// The actor's own shadow radius rates, which the blend moves toward.
struct ShadowRates {
  std::uint16_t expand = 0;
  std::uint16_t shrink = 0;

  friend bool operator==(const ShadowRates&, const ShadowRates&) = default;
};

// func_800A06B8: 0 frames snaps the radius rates to the actor's and the level
// to 0; otherwise the blend steps from the current rates and level to the
// actor's rates and level 0 over `blend_frames` frames (a byte).
void BeginActorShadowBlend(ShadowBlend& shadow, ShadowRates rates,
                           int blend_frames);

// An actor's fade color and the tint it is drawn with.
struct ActorColors {
  std::array<std::uint8_t, kRgbChannels> fade{};
  bool custom_tint = false;
  Render::ColorCode tint;

  friend bool operator==(const ActorColors&, const ActorColors&) = default;
};

// func_800A071C: the fade color in red, green and blue; `fade_color` keeps
// its code.
void ActorFadeParameters(const ActorColors& colors,
                         Render::ColorCode& fade_color);
// func_800A0768: sets the fade color from red, green and blue.
void SetActorFadeParameters(ActorColors& colors,
                            const Render::ColorCode& fade_color);
// func_800A07B4.
[[nodiscard]] Render::ColorCode ActorTint(const ActorColors& colors);
// func_800A07FC: a tint draws the actor with it; none returns to the model's
// own color and keeps the last tint.
void SetActorTint(ActorColors& colors,
                  const std::optional<Render::ColorCode>& tint);

// The actors an appearance transition reaches through their companion slots.
class AppearanceTransitionActors {
 public:
  virtual ~AppearanceTransitionActors() = default;
  [[nodiscard]] virtual bool Exists(int actor_id) const = 0;
  virtual void SetTransitionState(int actor_id, int state) = 0;
  [[nodiscard]] virtual int CompanionSlot(int actor_id) const = 0;
};

// The transition state transition kinds 0, 1 and 2 start (kAppearanceBlack,
// kAppearanceDelay and kAppearanceWhite); other kinds start none.
[[nodiscard]] std::optional<int> AppearanceTransitionState(int transition_kind);

// func_800AC37C: starts the transition on the actor and on each companion
// past slot 1 it names, in turn. Returns false when the chain names a missing
// actor or is longer than the actor table (the original recursed without
// either check); the actors before that point keep their new state.
bool StartActorAppearanceTransition(AppearanceTransitionActors& actors,
                                    int actor_id, int transition_kind);
}  // namespace VS::Actors
