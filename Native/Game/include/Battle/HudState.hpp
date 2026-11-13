#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/StatusLifecycle.hpp"

namespace VS::Battle {
// Enums name canonical policies, not the entire accepted command domain.
// Noncanonical visibility/mode commands retain their byte value.
enum class HudVisibility : std::uint8_t {
  kShown = 0,
  kHidden = 1,
};
enum class HudBattleMode : std::uint8_t {
  kUnarmed = 0,
  kWeaponDrawn = 1,
  kActionMenu = 2,
  kArmedActionMenu = 3,
};
enum class LowerScreenUi : std::uint8_t {
  kIdle = 0,
  kPuzzleTimer = 1,
  kMenuBackground = 2,
  kTimerAndMenuBackground = 3,
};
enum class ShortcutHudRequest : std::uint8_t {
  kIdle = 0,
  kOpen = 1,
};
enum class HudLimbCondition : std::int32_t {
  kCritical = 0,
  kDamaged = 1,
  kWounded = 2,
  kGood = 3,
  kExcellent = 4,
};

struct HudVisibilityCommand {
  explicit constexpr HudVisibilityCommand(std::uint8_t value = 0)
      : value(value) {}
  explicit constexpr HudVisibilityCommand(HudVisibility visibility)
      : HudVisibilityCommand(static_cast<std::uint8_t>(visibility)) {}
  std::uint8_t value;
  friend bool operator==(const HudVisibilityCommand&,
                         const HudVisibilityCommand&) = default;
};

struct HudBattleModeCommand {
  explicit constexpr HudBattleModeCommand(std::uint8_t value = 0)
      : value(value) {}
  explicit constexpr HudBattleModeCommand(HudBattleMode mode)
      : HudBattleModeCommand(static_cast<std::uint8_t>(mode)) {}
  std::uint8_t value;
  friend bool operator==(const HudBattleModeCommand&,
                         const HudBattleModeCommand&) = default;
};

struct HudState {
  HudVisibilityCommand visibility{};
  HudBattleModeCommand battle_mode{};
  std::uint8_t lower_screen{};
  std::uint8_t shortcut{};

  [[nodiscard]] bool IsHidden() const;
  [[nodiscard]] bool ShowsBattleModeIndicator() const;
  [[nodiscard]] bool UsesWeaponLimbDisplay() const;
  [[nodiscard]] bool ShowsPuzzleTimer() const;
  [[nodiscard]] bool SuppressesLimbDisplay() const;
  friend bool operator==(const HudState&, const HudState&) = default;
};

inline constexpr std::size_t kHudMappedStatusCount = 8;

// Small value views reuse status/vitals owners without copying or inspecting
// their equipment, timers or unrelated limbs.
struct HudStatusView {
  explicit constexpr HudStatusView(std::uint32_t value) : statuses(value) {}
  explicit constexpr HudStatusView(const StatusLifecycleState& owner)
      : HudStatusView(owner.statuses) {}
  std::uint32_t statuses;
};

struct HudLimbHealthView {
  constexpr HudLimbHealthView(std::int16_t hp, std::int16_t max_hp)
      : hp(hp), max_hp(max_hp) {}
  explicit constexpr HudLimbHealthView(const StatusLimb& owner)
      : HudLimbHealthView(owner.hp, owner.max_hp) {}
  std::int16_t hp;
  std::int16_t max_hp;
};

// Only set status bits require their corresponding table entries. An empty
// mapping is valid when no mapped status is present; missing used entries
// refuse. decomp: F01356 vs_battle_getStatusFlags
// (src/BATTLE/BATTLE.PRG/5BF94.c:3158).
std::optional<std::uint32_t> StatusIconFlags(
    HudStatusView status, std::span<const std::uint16_t> upper_status_icons);

// Signed halfword HP, strict quarter/three-quarter comparisons; no division,
// clamp, or positive-max-HP precondition. Equipment and other limb data are
// unused. decomp: F01357 vs_battle_getLimbStatus
// (src/BATTLE/BATTLE.PRG/5BF94.c:3174).
HudLimbCondition LimbCondition(HudLimbHealthView limb);

class GameplayHudWorld {
 public:
  virtual ~GameplayHudWorld() = default;
  virtual void SetHudVisibility(HudVisibility visibility) = 0;
  virtual void SetFieldOverlayVisible(bool visible) = 0;
};

// This composed command tests the full word and returns it unchanged, unlike
// SetHudHidden's byte command. The HUD write precedes the field-overlay
// service. decomp: F00235 func_8007BD8C (src/BATTLE/BATTLE.PRG/146C.c:8561).
std::int32_t SetGameplayHudHidden(GameplayHudWorld& world,
                                  std::int32_t command);

// Requests opening without restarting the shortcut animation.
// decomp: F01359 func_800C9F88 (src/BATTLE/BATTLE.PRG/5BF94.c:3239).
void ShowShortcutHud(HudState& state);
// Both timer commands replace the entire lower-screen byte, including the
// menu-background bit; they do not change the scene timer itself.
// decomp: F01382 func_800CB550 (src/BATTLE/BATTLE.PRG/5BF94.c:3838).
void ShowPuzzleTimer(HudState& state);
// decomp: F01383 func_800CB560 (src/BATTLE/BATTLE.PRG/5BF94.c:3840).
void HidePuzzleTimer(HudState& state);
// decomp: F01385 func_800CB654 (src/BATTLE/BATTLE.PRG/5BF94.c:3867).
void SetHudHidden(HudState& state, HudVisibilityCommand visibility);
// decomp: F01386 func_800CB660 (src/BATTLE/BATTLE.PRG/5BF94.c:3869).
void SetHudBattleMode(HudState& state, HudBattleModeCommand mode);
}  // namespace VS::Battle
