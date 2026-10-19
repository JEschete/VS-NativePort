#pragma once

#include <array>

#include "Core/Types.hpp"

namespace VS::Core {

// Reproduces the original game's random sequence exactly (replay parity, plan
// §9.9).
//  - Rand15: the PS1 BIOS rand() (A0h:2Fh) LCG, 15-bit output. srand(1) at
//  boot.
//  - Random::Next(max): vs_main_getRand, a 97-entry shuffle table over Rand15.
//  - Random::NextSmoothed(max): vs_main_getRandSmoothed, mean of 12 draws.
class Rand15 {
 public:
  static constexpr unsigned kValueBits = 15;
  static constexpr i32 kMaximum = (1 << kValueBits) - 1;
  explicit constexpr Rand15(u32 seed = 1) noexcept : m_state_(seed) {}
  void Seed(u32 seed) { m_state_ = seed; }
  i32 Next() {
    m_state_ = m_state_ * kMultiplier + kIncrement;
    return static_cast<i32>((m_state_ >> kDiscardedBits) & kMaximum);
  }
  u32 State() const { return m_state_; }

 private:
  static constexpr u32 kMultiplier = 0x41C64E6Du;
  static constexpr u32 kIncrement = 0x3039u;
  static constexpr unsigned kDiscardedBits = 16;
  u32 m_state_;
};

class Random {
 public:
  static constexpr usize TableSize = 97;

  Random() { Reset(); }

  // vs_main _initRand: srand(seed), fill table, index = 96.
  void Reset(u32 seed = 1);

  // Returns [0, max) for 1 <= max <= 65536. Other inputs retain the original
  // signed 32-bit product wrap rather than invoking host signed overflow.
  i32 Next(i32 max);

  // Average of 12 Next(max) draws (vs_main_getRandSmoothed).
  i32 NextSmoothed(i32 max);

  // Direct access to the underlying BIOS rand() for code that called rand()
  // itself.
  i32 Raw() { return m_rand_.Next(); }

 private:
  Rand15 m_rand_;
  std::array<i32, TableSize> m_table_{};
  i32 m_index_ = 0;
};

}  // namespace VS::Core
