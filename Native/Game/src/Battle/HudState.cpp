// Hand-maintained (tools/port/owned.csv).
#include "Battle/HudState.hpp"

namespace VS::Battle {
namespace {
constexpr std::uint32_t kFirstDirectStatusBit = 5;
constexpr std::uint32_t kFirstMappedStatusBit = 21;
constexpr std::uint32_t kLowIconMask = 0xFFFF;
constexpr std::uint32_t kUpperIconShift = 16;
constexpr std::uint8_t kWeaponLimbDisplayBit = 1;
constexpr std::uint8_t kMenuBackgroundBit = 2;
constexpr std::int32_t kCriticalHpEnd = 2;
constexpr std::int32_t kHpQuarterScale = 4;
constexpr std::int32_t kWoundedMaxHpScale = 3;
}  // namespace

bool HudState::IsHidden() const {
  return visibility.value != static_cast<std::uint8_t>(HudVisibility::kShown);
}

bool HudState::ShowsBattleModeIndicator() const {
  return battle_mode.value ==
         static_cast<std::uint8_t>(HudBattleMode::kWeaponDrawn);
}

bool HudState::UsesWeaponLimbDisplay() const {
  return (battle_mode.value & kWeaponLimbDisplayBit) != 0;
}

bool HudState::ShowsPuzzleTimer() const {
  return lower_screen == static_cast<std::uint8_t>(LowerScreenUi::kPuzzleTimer);
}

bool HudState::SuppressesLimbDisplay() const {
  return (lower_screen & kMenuBackgroundBit) != 0;
}

// decomp: F01356 vs_battle_getStatusFlags (src/BATTLE/BATTLE.PRG/5BF94.c:3158).
std::optional<std::uint32_t> StatusIconFlags(
    HudStatusView status, std::span<const std::uint16_t> upper_status_icons) {
  std::uint32_t icons =
      (status.statuses >> kFirstDirectStatusBit) & kLowIconMask;
  const std::uint32_t mapped = status.statuses >> kFirstMappedStatusBit;
  for (std::size_t index = 0; index < kHudMappedStatusCount; ++index) {
    if ((mapped & (std::uint32_t{1} << index)) == 0) continue;
    if (index >= upper_status_icons.size()) return std::nullopt;
    icons |= std::uint32_t{upper_status_icons[index]} << kUpperIconShift;
  }
  return icons;
}

// decomp: F01357 vs_battle_getLimbStatus (src/BATTLE/BATTLE.PRG/5BF94.c:3174).
HudLimbCondition LimbCondition(HudLimbHealthView limb) {
  const std::int32_t hp = limb.hp;
  const std::int32_t max_hp = limb.max_hp;
  if (hp < kCriticalHpEnd) return HudLimbCondition::kCritical;
  const std::int32_t scaled_hp = hp * kHpQuarterScale;
  if (scaled_hp < max_hp) return HudLimbCondition::kDamaged;
  if (scaled_hp < max_hp * kWoundedMaxHpScale) {
    return HudLimbCondition::kWounded;
  }
  if (hp < max_hp) return HudLimbCondition::kGood;
  return HudLimbCondition::kExcellent;
}

// decomp: F00235 func_8007BD8C (src/BATTLE/BATTLE.PRG/146C.c:8561).
std::int32_t SetGameplayHudHidden(GameplayHudWorld& world,
                                  std::int32_t command) {
  if (command != 0) {
    world.SetHudVisibility(HudVisibility::kHidden);
    world.SetFieldOverlayVisible(false);
  } else {
    world.SetHudVisibility(HudVisibility::kShown);
    world.SetFieldOverlayVisible(true);
  }
  return command;
}

// decomp: F01359 func_800C9F88 (src/BATTLE/BATTLE.PRG/5BF94.c:3239).
void ShowShortcutHud(HudState& state) {
  state.shortcut = static_cast<std::uint8_t>(ShortcutHudRequest::kOpen);
}

// decomp: F01382 func_800CB550 (src/BATTLE/BATTLE.PRG/5BF94.c:3838).
void ShowPuzzleTimer(HudState& state) {
  state.lower_screen = static_cast<std::uint8_t>(LowerScreenUi::kPuzzleTimer);
}

// decomp: F01383 func_800CB560 (src/BATTLE/BATTLE.PRG/5BF94.c:3840).
void HidePuzzleTimer(HudState& state) {
  state.lower_screen = static_cast<std::uint8_t>(LowerScreenUi::kIdle);
}

// decomp: F01385 func_800CB654 (src/BATTLE/BATTLE.PRG/5BF94.c:3867).
void SetHudHidden(HudState& state, HudVisibilityCommand visibility) {
  state.visibility = visibility;
}

// decomp: F01386 func_800CB660 (src/BATTLE/BATTLE.PRG/5BF94.c:3869).
void SetHudBattleMode(HudState& state, HudBattleModeCommand mode) {
  state.battle_mode = mode;
}
}  // namespace VS::Battle
