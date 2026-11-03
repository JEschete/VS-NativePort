// Hand-maintained (tools/port/owned.csv).
// Typed appearance requests (Appearance.hpp); AppearanceLive.cpp binds them
// to the actor and gimmick tables.
#include "Actors/Appearance.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace VS::Actors {
namespace {
// The original stored the doubled fade parameter as a byte.
constexpr int kFadeRateScale = 2;
constexpr std::uint8_t kPulseDirectionFlag = 1;
// The pulse flag bits a restart replaces.
constexpr std::uint8_t kPulseRestartMask = 0xFC;
constexpr int kHighestPulseBit = kPalettePulseSlots - 1;
constexpr int kTransitionBlackKind = 0;
constexpr int kTransitionDelayKind = 1;
constexpr int kTransitionWhiteKind = 2;
// Companion slots 0 and 1 name no companion.
constexpr int kFirstCompanionSlot = 2;

[[nodiscard]] std::uint8_t PulseSlotBit(int pulse_slot) {
  return static_cast<std::uint8_t>(1U << (kHighestPulseBit - pulse_slot));
}

[[nodiscard]] bool IsPulseSlot(int pulse_slot) {
  return static_cast<unsigned>(pulse_slot) <
         static_cast<unsigned>(kPalettePulseSlots);
}
}  // namespace

// decomp: BATTLE.PRG func_8009D7E8 (src/BATTLE/BATTLE.PRG/30DB0.c:2173)
void SetActorPaletteFade(PaletteFade& fade, int fade_parameter) {
  if (fade_parameter == 0) {
    fade.active = false;
    return;
  }
  fade.phase = 0;
  fade.active = true;
  fade.rate = static_cast<std::uint8_t>(fade_parameter * kFadeRateScale);
  fade.reverse = false;
}

// decomp: BATTLE.PRG func_8009D88C (src/BATTLE/BATTLE.PRG/30DB0.c:2195)
void ResetActorWeaponVisibilityBlend(
    std::span<std::optional<WeaponAppearance>> weapons) {
  for (auto& weapon : weapons) {
    if (!weapon.has_value() || !weapon->attached) continue;
    const std::uint8_t level = weapon->shown ? kWeaponBlendShown : 0;
    weapon->blend_level = level;
    weapon->blend_target = level;
    weapon->blend_frames = 0;
  }
}

// decomp: BATTLE.PRG func_8009F898 (src/BATTLE/BATTLE.PRG/30DB0.c:3363)
void SetWeaponBrightnessOverride(WeaponAppearance& weapon, bool enabled,
                                 std::uint8_t brightness) {
  weapon.brightness = brightness;
  weapon.brightness_override = enabled;
}

// decomp: BATTLE.PRG func_8009FB64 (src/BATTLE/BATTLE.PRG/30DB0.c:3467)
std::int16_t SetRoomActorScale(Core::Math::Vec3& stored,
                               Core::Math::Vec3 room_scale) {
  stored = room_scale;
  return Core::Math::WrapSigned16(static_cast<std::uint32_t>(room_scale.x));
}

// decomp: BATTLE.PRG func_8009FD5C (src/BATTLE/BATTLE.PRG/30DB0.c:3548)
PalettePulseChange EnableModelPalettePulse(PalettePulseRequest& request,
                                           int pulse_flags, int pulse_slot) {
  PalettePulseChange change;
  if (request.kind == 0) {
    request.pulse.level = 0;
    request.pulse.flags =
        static_cast<std::uint8_t>((request.pulse.flags & kPulseRestartMask) |
                                  (pulse_flags & kPulseDirectionFlag));
    change.restarted = true;
  }
  if (pulse_slot == kEveryPalettePulseSlot) {
    request.kind = kEveryPalettePulseKind;
  } else if (!IsPulseSlot(pulse_slot)) {
    change.status = AppearanceStatus::kBadPulseSlot;
  } else {
    request.kind |= PulseSlotBit(pulse_slot);
  }
  return change;
}

// decomp: BATTLE.PRG func_8009FE74 (src/BATTLE/BATTLE.PRG/30DB0.c:3586)
AppearanceStatus DisableModelPalettePulse(PalettePulseRequest& request,
                                          int pulse_slot) {
  if (pulse_slot == kEveryPalettePulseSlot) {
    request.kind = 0;
    return AppearanceStatus::kOk;
  }
  if (!IsPulseSlot(pulse_slot)) return AppearanceStatus::kBadPulseSlot;
  request.kind &= static_cast<std::uint8_t>(~PulseSlotBit(pulse_slot));
  return AppearanceStatus::kOk;
}

// decomp: BATTLE.PRG func_800A06B8 (src/BATTLE/BATTLE.PRG/30DB0.c:3877)
void BeginActorShadowBlend(ShadowBlend& shadow, ShadowRates rates,
                           int blend_frames) {
  shadow.radiusFrames = 0;
  if (blend_frames == 0) {
    shadow.radiusExpandRate = rates.expand;
    shadow.radiusShrinkRate = rates.shrink;
    shadow.level = 0;
  } else {
    shadow.radiusExpandDelta =
        static_cast<std::int16_t>(rates.expand - shadow.radiusExpandRate);
    shadow.radiusShrinkDelta =
        static_cast<std::int16_t>(rates.shrink - shadow.radiusShrinkRate);
    shadow.levelDelta = static_cast<std::int16_t>(-shadow.level);
  }
  shadow.levelFrames = static_cast<std::uint8_t>(blend_frames);
  shadow.radiusFrames = static_cast<std::uint8_t>(blend_frames);
}

// decomp: BATTLE.PRG func_800A071C (src/BATTLE/BATTLE.PRG/30DB0.c:3894)
void ActorFadeParameters(const ActorColors& colors,
                         Render::ColorCode& fade_color) {
  fade_color.red = colors.fade[0];
  fade_color.green = colors.fade[1];
  fade_color.blue = colors.fade[2];
}

// decomp: BATTLE.PRG func_800A0768 (src/BATTLE/BATTLE.PRG/30DB0.c:3907)
void SetActorFadeParameters(ActorColors& colors,
                            const Render::ColorCode& fade_color) {
  colors.fade = {fade_color.red, fade_color.green, fade_color.blue};
}

// decomp: BATTLE.PRG func_800A07B4 (src/BATTLE/BATTLE.PRG/30DB0.c:3920)
Render::ColorCode ActorTint(const ActorColors& colors) { return colors.tint; }

// decomp: BATTLE.PRG func_800A07FC (src/BATTLE/BATTLE.PRG/30DB0.c:3930)
void SetActorTint(ActorColors& colors,
                  const std::optional<Render::ColorCode>& tint) {
  colors.custom_tint = tint.has_value();
  if (tint.has_value()) colors.tint = *tint;
}

std::optional<int> AppearanceTransitionState(int transition_kind) {
  switch (transition_kind) {
    case kTransitionBlackKind:
      return kAppearanceBlack;
    case kTransitionDelayKind:
      return kAppearanceDelay;
    case kTransitionWhiteKind:
      return kAppearanceWhite;
    default:
      return std::nullopt;
  }
}

// decomp: BATTLE.PRG func_800AC37C (src/BATTLE/BATTLE.PRG/40564.c:2179)
bool StartActorAppearanceTransition(AppearanceTransitionActors& actors,
                                    int actor_id, int transition_kind) {
  const std::optional<int> state = AppearanceTransitionState(transition_kind);
  for (int reached = 0; reached < kActorTableSize; ++reached) {
    if (!actors.Exists(actor_id)) return false;
    if (state.has_value()) actors.SetTransitionState(actor_id, *state);
    const int companion = actors.CompanionSlot(actor_id);
    if (companion < kFirstCompanionSlot) return true;
    actor_id = companion;
  }
  return false;
}
}  // namespace VS::Actors
