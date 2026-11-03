// Hand-maintained (tools/port/owned.csv).
#include <bit>

#include "Core/BiosRand.h"
#include "Core/Log.hpp"
#include "Core/Random.hpp"  // NOLINT(build/include): native C++ interface.
#include "Core/RandomLive.hpp"

namespace VS::Core {
namespace {
// The BIOS rand() state shared by every copied caller of rand()/srand(); seeded
// with 1 at boot like the original.
constinit Rand15 g_bios_rand;

constexpr i32 kTableLast = static_cast<i32>(Random::TableSize) - 1;
constexpr unsigned kWordBits = 32;
constexpr unsigned kShiftMask = kWordBits - 1;
constexpr u32 kKeystreamMultiplier = 0x19660Du;
constexpr int kAveragedDraws = 12;

bool InTable(i32 index) { return index >= 0 && index <= kTableLast; }
}  // namespace

Rand15& BiosRand() noexcept { return g_bios_rand; }

// decomp: SLUS_010.40 _initRand (src/SLUS_010.40/main.c:8239)
void Random::FillShuffleTable(Rand15& source, Table table, i32& index) {
  for (auto& entry : table) {
    entry = source.Next();
  }
  index = kTableLast;
}

void Random::Reset(u32 seed) {
  m_rand_.Seed(seed);
  FillShuffleTable(m_rand_, m_table_, m_index_);
}

i32 Random::Next(i32 max) {
  return NextBounded(m_rand_, m_table_, m_index_, max);
}

i32 Random::NextSmoothed(i32 max) {
  return NextAveraged(m_rand_, m_table_, m_index_, max);
}

// decomp: BATTLE.PRG vs_battle_keystreamBits
// (src/BATTLE/BATTLE.PRG/5BF94.c:4496)
u32 Random::NextInterfaceRandomBits(u32& state, i32 bit_count) {
  const u32 previous = state;
  state = previous * kKeystreamMultiplier;
  return previous >> ((kWordBits - static_cast<u32>(bit_count)) & kShiftMask);
}

// decomp: BATTLE.PRG vs_battle_randUniformInt
// (src/BATTLE/BATTLE.PRG/5BF94.c:5810) The span and the sum wrap at 32 bits as
// the original's subu/addu do.
i32 Random::RandomRangeExclusive(Rand15& source, i32 bound_a, i32 bound_b) {
  if (bound_a == bound_b) {
    return bound_a;
  }
  const i32 low = bound_b < bound_a ? bound_b : bound_a;
  const i32 high = bound_b < bound_a ? bound_a : bound_b;
  const i32 span =
      std::bit_cast<i32>(static_cast<u32>(high) - static_cast<u32>(low));
  const i32 offset = source.Next() % span;
  return std::bit_cast<i32>(static_cast<u32>(offset) + static_cast<u32>(low));
}

// decomp: SLUS_010.40 _initRand (src/SLUS_010.40/main.c:8239)
void Random::InitializeShuffleTable(Rand15& source, Table table, i32& index) {
  source.Seed(1);
  FillShuffleTable(source, table, index);
}

// decomp: SLUS_010.40 vs_main_getRand (src/SLUS_010.40/main.c:8251)
i32 Random::NextBounded(Rand15& source, Table table, i32& index, i32 max) {
  if (!InTable(index)) {
    VS_LOG_WARN("rng", "shuffle cursor {} is outside the table; using {}",
                index, kTableLast);
    index = kTableLast;
  }
  const u32 product = static_cast<u32>(table[static_cast<usize>(index)]) *
                      static_cast<u32>(TableSize);
  i32 scaled = std::bit_cast<i32>(product);
  if (scaled < 0) {
    scaled += Rand15::kMaximum;
  }
  index = scaled >> Rand15::kValueBits;
  if (!InTable(index)) {
    VS_LOG_WARN("rng", "shuffle entry selects {}; using {}", index, kTableLast);
    index = kTableLast;
  }

  const i32 picked = Rand15::kMaximum - table[static_cast<usize>(index)];
  table[static_cast<usize>(index)] = source.Next();

  i32 result =
      std::bit_cast<i32>(static_cast<u32>(picked) * static_cast<u32>(max));
  if (result < 0) {
    result += Rand15::kMaximum;
  }
  return result >> Rand15::kValueBits;
}

// decomp: SLUS_010.40 vs_main_getRandSmoothed (src/SLUS_010.40/main.c:8277)
i32 Random::NextAveraged(Rand15& source, Table table, i32& index, i32 max) {
  i32 sum = 0;
  for (int draw = 0; draw < kAveragedDraws; ++draw) {
    sum += NextBounded(source, table, index, max);
  }
  return sum / kAveragedDraws;
}

}  // namespace VS::Core

extern "C" int Core_Random_Rand(void) {
  auto& bios = VS::Core::BiosRand();
  const auto before = bios.State();
  const int value = bios.Next();
  VS_LOG_TRACE("rng", "raw before={} after={} value={}", before, bios.State(),
               value);
  return value;
}

extern "C" void Core_Random_Seed(unsigned int seed) {
  VS::Core::BiosRand().Seed(seed);
  VS_LOG_TRACE("rng", "seed={}", seed);
}

extern "C" unsigned int Core_Random_State(void) {
  return VS::Core::BiosRand().State();
}
