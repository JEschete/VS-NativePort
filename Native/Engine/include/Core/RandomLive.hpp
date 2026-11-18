#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Core/Random.hpp"
#include "Core/Types.hpp"

// Live binding for the Random static members: the engine's BIOS rand()
// state, the interface keystream and the 97-entry shuffle table that the
// copied callers share.
namespace VS::Core {
// decomp: _keystreamState (_keystreamState@BATTLE/BATTLE.PRG), the interface
// keystream; the BATTLE.PRG module image loads its seed (ModuleData.cpp).
extern u32 interfaceRandomState;
// decomp: randArr and randIndex (SLUS_010.40), the shuffle table of the
// bounded generator and its next entry.
extern i32 randomShuffleTable[Random::TableSize];
extern i32 randomShuffleIndex;

// The BIOS rand() (A0h:2Fh) state: the PS1 rand()/srand() LCG with 15-bit
// output, seeded with 1 at boot.
[[nodiscard]] Rand15& BiosRand() noexcept;

// BIOS rand() and srand() over BiosRand(), with the "rng" trace of each call.
i32 NextBiosRand();
void SeedBiosRand(u32 seed);

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
