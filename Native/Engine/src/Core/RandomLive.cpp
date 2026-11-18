// Hand-maintained (tools/port/owned.csv).
// Live binding for the Random static members (RandomGenerator.cpp): the
// engine's BIOS rand(), interface keystream and shuffle table.
#include "Core/RandomLive.hpp"

#include "Core/Log.hpp"

namespace VS::Core {
namespace {
Random::Table ShuffleTable() noexcept {
  return Random::Table(VS::Core::randomShuffleTable);
}
}  // namespace

u32 NextLiveInterfaceRandomBits(i32 bit_count) {
  const u32 value = Random::NextInterfaceRandomBits(
      VS::Core::interfaceRandomState, bit_count);
  VS::Core::LogTrace("rng", "keystream bits={} value={:#x} state={:#x}",
                     bit_count, value, VS::Core::interfaceRandomState);
  return value;
}

i32 LiveRandomRangeExclusive(i32 bound_a, i32 bound_b) {
  const i32 value = Random::RandomRangeExclusive(BiosRand(), bound_a, bound_b);
  VS::Core::LogTrace("rng", "range [{}, {}) value={} state={}", bound_a,
                     bound_b, value, BiosRand().State());
  return value;
}

void InitializeLiveRandomShuffleTable() {
  Random::InitializeShuffleTable(BiosRand(), ShuffleTable(),
                                 VS::Core::randomShuffleIndex);
  VS::Core::LogTrace("rng", "shuffle table seeded state={}",
                     BiosRand().State());
}

i32 NextLiveRandomBounded(i32 max) {
  const i32 value = Random::NextBounded(BiosRand(), ShuffleTable(),
                                        VS::Core::randomShuffleIndex, max);
  VS::Core::LogTrace("rng", "bounded max={} value={} cursor={} state={}", max,
                     value, VS::Core::randomShuffleIndex, BiosRand().State());
  return value;
}

i32 NextLiveRandomAveraged(i32 max) {
  const i32 value = Random::NextAveraged(BiosRand(), ShuffleTable(),
                                         VS::Core::randomShuffleIndex, max);
  VS::Core::LogTrace("rng", "averaged max={} value={}", max, value);
  return value;
}
}  // namespace VS::Core
