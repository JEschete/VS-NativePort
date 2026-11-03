// Hand-maintained (tools/port/owned.csv).
// Live binding for the Random static members (RandomGenerator.cpp): the
// engine's BIOS rand(), interface keystream and shuffle table.
#include "Core/RandomLive.hpp"

#include "Core/Log.hpp"

extern "C" {
#include "Core/Random.h"
}

namespace VS::Core {
namespace {
Random::Table ShuffleTable() noexcept {
  return Random::Table(Core_randomShuffleTable);
}
}  // namespace

u32 NextLiveInterfaceRandomBits(i32 bit_count) {
  const u32 value =
      Random::NextInterfaceRandomBits(Core_interfaceRandomState, bit_count);
  VS_LOG_TRACE("rng", "keystream bits={} value={:#x} state={:#x}", bit_count,
               value, Core_interfaceRandomState);
  return value;
}

i32 LiveRandomRangeExclusive(i32 bound_a, i32 bound_b) {
  const i32 value = Random::RandomRangeExclusive(BiosRand(), bound_a, bound_b);
  VS_LOG_TRACE("rng", "range [{}, {}) value={} state={}", bound_a, bound_b,
               value, BiosRand().State());
  return value;
}

void InitializeLiveRandomShuffleTable() {
  Random::InitializeShuffleTable(BiosRand(), ShuffleTable(),
                                 Core_randomShuffleIndex);
  VS_LOG_TRACE("rng", "shuffle table seeded state={}", BiosRand().State());
}

i32 NextLiveRandomBounded(i32 max) {
  const i32 value = Random::NextBounded(BiosRand(), ShuffleTable(),
                                        Core_randomShuffleIndex, max);
  VS_LOG_TRACE("rng", "bounded max={} value={} cursor={} state={}", max, value,
               Core_randomShuffleIndex, BiosRand().State());
  return value;
}

i32 NextLiveRandomAveraged(i32 max) {
  const i32 value = Random::NextAveraged(BiosRand(), ShuffleTable(),
                                         Core_randomShuffleIndex, max);
  VS_LOG_TRACE("rng", "averaged max={} value={}", max, value);
  return value;
}
}  // namespace VS::Core
