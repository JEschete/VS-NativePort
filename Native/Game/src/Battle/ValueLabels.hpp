#pragma once
// A floating battle label: the value, status, chain or timing caption the
// HUD draws over an actor. Feedback.hpp holds the live label queue.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Gte.hpp"

namespace VS::Battle {

inline constexpr std::size_t kFloatingLabelKeyBytes = 2;

// The label's key: its kind and argument bytes, or both as one halfword.
// decomp: D_800F1BAC_t.unk8 (src/BATTLE/BATTLE.PRG/146C.h:24-27)
union ValueLabelKey {
  std::array<std::int8_t, kFloatingLabelKeyBytes> bytes;
  std::uint16_t halfword;
};
inline constexpr std::size_t kValueLabelKeyBytes = 0x2;
static_assert(sizeof(ValueLabelKey) == kValueLabelKeyBytes,
              "ValueLabelKey must keep its PS1 layout");

inline constexpr std::size_t kFloatingLabelTextBytes = 4;

// decomp: D_800F1BAC_t (src/BATTLE/BATTLE.PRG/146C.h:22-37)
struct FloatingLabel {
  VS::Core::Gte::SVECTOR position;
  ValueLabelKey key;
  std::uint8_t limb;
  std::uint8_t frame;
  std::uint16_t value;
  std::uint8_t color_flags;
  std::uint8_t text_length;
  std::int16_t scale_x;
  std::int16_t scale_y;
  std::array<std::int8_t, kFloatingLabelTextBytes> text;
};
inline constexpr std::size_t kFloatingLabelBytes = 0x18;
static_assert(sizeof(FloatingLabel) == kFloatingLabelBytes,
              "FloatingLabel must keep its PS1 layout");

// The label queue's records (D_800F1BAC's 32-entry block).
inline constexpr std::size_t kFloatingLabelSlotCount = 32;
using FloatingLabelSlots = std::array<FloatingLabel, kFloatingLabelSlotCount>;

}  // namespace VS::Battle
