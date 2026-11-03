#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Core/Random.hpp"
#include "Core/Types.hpp"

// Live binding for the Random static members: the engine's BIOS rand()
// state, the interface keystream and the 97-entry shuffle table that the
// copied callers share.
namespace VS::Core {
// The BIOS rand() (A0h:2Fh) state behind Core_Random_Rand.
[[nodiscard]] Rand15& BiosRand() noexcept;

// Random::NextInterfaceRandomBits over the interface keystream.
u32 NextLiveInterfaceRandomBits(i32 bit_count);

// Random::RandomRangeExclusive over BiosRand().
i32 LiveRandomRangeExclusive(i32 bound_a, i32 bound_b);

// Random::InitializeShuffleTable, NextBounded and NextAveraged over
// BiosRand() and the engine's shuffle table.
void InitializeLiveRandomShuffleTable();
i32 NextLiveRandomBounded(i32 max);
i32 NextLiveRandomAveraged(i32 max);
}  // namespace VS::Core
