#pragma once
// The manual screen's display state. Manual.cpp defines it.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Field {
inline constexpr std::size_t kManualScrollPositionCount = 14;
inline constexpr std::size_t kManualDisplayStateBytes = 0x24;

// The open manual and the scroll position kept for each manual; the room
// load clears it.
// decomp: ManualDisplayState (src/BATTLE/BATTLE.PRG/menuState.h:7)
struct ManualDisplayState {
  std::uint16_t current_manual;
  std::uint16_t reserved02;
  std::array<std::uint16_t, kManualScrollPositionCount> scroll_positions;
  std::uint16_t reserved20;
  std::uint8_t reserved22;
  std::uint8_t reserved23;
};
static_assert(sizeof(ManualDisplayState) == kManualDisplayStateBytes);

// decomp: vs_battle_manualDisplayState (menuState.h:38,
// vs_battle_manualDisplayState@BATTLE/BATTLE.PRG)
extern ManualDisplayState manualDisplayState;
}  // namespace VS::Field
