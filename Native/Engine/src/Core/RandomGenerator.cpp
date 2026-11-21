// Hand-maintained (tools/port/owned.csv).
#include <bit>

#include "Core/Log.hpp"
#include "Core/Random.hpp"  // NOLINT(build/include): native C++ interface.
#include "Core/RandomLive.hpp"

namespace VS::Core {
namespace {
// The BIOS rand() state shared by every copied caller of rand()/srand(); seeded
// with 1 at boot like the original.
constinit Rand15 biosRand;

constexpr i32 kTableLast = static_cast<i32>(Random::TableSize) - 1;
constexpr unsigned kWordBits = 32;
constexpr unsigned kShiftMask = kWordBits - 1;
constexpr u32 kKeystreamMultiplier = 0x19660Du;
constexpr int kAveragedDraws = 12;

bool InTable(i32 index) { return index >= 0 && index <= kTableLast; }
}  // namespace

Rand15& BiosRand() noexcept { return biosRand; }

i32 NextBiosRand() {
  const u32 before = biosRand.State();
  const i32 value = biosRand.Next();
  LogTrace("rng", "raw before={} after={} value={}", before, biosRand.State(),
           value);
  return value;
}

void SeedBiosRand(u32 seed) {
  biosRand.Seed(seed);
  LogTrace("rng", "seed={}", seed);
}

// decomp: SLUS_010.40 initRand (src/SLUS_010.40/main.c:1103; was _initRand)
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

// decomp: BATTLE.PRG vs_battle_getRandBits
// (src/BATTLE/BATTLE.PRG/5BF94.c:4062; was vs_battle_keystreamBits)
u32 Random::NextInterfaceRandomBits(u32& state, i32 bit_count) {
  const u32 previous = state;
  state = previous * kKeystreamMultiplier;
  return previous >> ((kWordBits - static_cast<u32>(bit_count)) & kShiftMask);
}

// decomp: BATTLE.PRG vs_battle_randUniformInt
// (src/BATTLE/BATTLE.PRG/5BF94.c:5375) The span and the sum wrap at 32 bits as
// the original's subu/addu do.
i32 Random::RandomRangeExclusive(Rand15& source, i32 bound_a, i32 bound_b) {
  const i32 low = bound_b < bound_a ? bound_b : bound_a;
  const i32 high = bound_b < bound_a ? bound_a : bound_b;
  const i32 span =
      std::bit_cast<i32>(static_cast<u32>(high) - static_cast<u32>(low));
  // The span is zero exactly when the bounds are equal: no draw is taken.
  if (span == 0) {
    return bound_a;
  }
  const i32 offset = source.Next() % span;
  return std::bit_cast<i32>(static_cast<u32>(offset) + static_cast<u32>(low));
}

// decomp: SLUS_010.40 initRand (src/SLUS_010.40/main.c:1103; was _initRand)
void Random::InitializeShuffleTable(Rand15& source, Table table, i32& index) {
  source.Seed(1);
  FillShuffleTable(source, table, index);
}

// decomp: SLUS_010.40 vs_main_getRand (src/SLUS_010.40/main.c:1116)
i32 Random::NextBounded(Rand15& source, Table table, i32& index, i32 max) {
  if (!InTable(index)) {
    LogWarn("rng", "shuffle cursor {} is outside the table; using {}", index,
            kTableLast);
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
    LogWarn("rng", "shuffle entry selects {}; using {}", index, kTableLast);
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

// decomp: SLUS_010.40 vs_main_getRandSmoothed (src/SLUS_010.40/main.c:1127)
i32 Random::NextAveraged(Rand15& source, Table table, i32& index, i32 max) {
  i32 sum = 0;
  for (int draw = 0; draw < kAveragedDraws; ++draw) {
    sum += NextBounded(source, table, index, max);
  }
  return sum / kAveragedDraws;
}

}  // namespace VS::Core
