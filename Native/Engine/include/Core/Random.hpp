#pragma once

#include "Core/Types.hpp"

#include <array>

namespace VS::Core {

// Reproduces the original game's random sequence exactly (replay parity, plan §9.9).
//  - Rand15: the PS1 BIOS rand() (A0h:2Fh) LCG, 15-bit output. srand(1) at boot.
//  - Random::Next(max): vs_main_getRand, a 97-entry shuffle table over Rand15.
//  - Random::NextSmoothed(max): vs_main_getRandSmoothed, mean of 12 draws.
class Rand15 {
public:
    explicit Rand15(u32 seed = 1) : m_state(seed) {}
    void Seed(u32 seed) { m_state = seed; }
    i32 Next()
    {
        m_state = m_state * 0x41C64E6Du + 0x3039u;
        return static_cast<i32>((m_state >> 16) & 0x7FFF);
    }
    u32 State() const { return m_state; }

private:
    u32 m_state;
};

class Random {
public:
    static constexpr usize TableSize = 97;

    Random() { Reset(); }

    // vs_main _initRand: srand(seed), fill table, index = 96.
    void Reset(u32 seed = 1);

    // Returns [0, max) for max > 0, matching vs_main_getRand bit-for-bit.
    i32 Next(i32 max);

    // Average of 12 Next(max) draws (vs_main_getRandSmoothed).
    i32 NextSmoothed(i32 max);

    // Direct access to the underlying BIOS rand() for code that called rand() itself.
    i32 Raw() { return m_rand.Next(); }

private:
    Rand15 m_rand;
    std::array<i32, TableSize> m_table{};
    i32 m_index = 0;
};

} // namespace VS::Core
