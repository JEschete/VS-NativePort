#include <bit>

#include "Core/BiosRand.h"
#include "Core/Log.hpp"
#include "Core/Random.hpp"  // NOLINT(build/include): native C++ interface.

namespace VS::Core {

// decomp: SLUS_010.40 _initRand (src/SLUS_010.40/main.c:8239)
void Random::Reset(u32 seed) {
  m_rand_.Seed(seed);
  for (auto& v : m_table_) {
    v = m_rand_.Next();
  }
  m_index_ = static_cast<i32>(TableSize) - 1;
}

// decomp: SLUS_010.40 vs_main_getRand (src/SLUS_010.40/main.c:8251)
i32 Random::Next(i32 max) {
  i32 scaled =
      m_table_[static_cast<usize>(m_index_)] * static_cast<i32>(TableSize);
  if (scaled < 0) {
    scaled += Rand15::kMaximum;
  }
  m_index_ = scaled >> Rand15::kValueBits;

  const i32 picked = Rand15::kMaximum - m_table_[static_cast<usize>(m_index_)];
  m_table_[static_cast<usize>(m_index_)] = m_rand_.Next();

  const u32 product = static_cast<u32>(picked) * static_cast<u32>(max);
  i32 result = std::bit_cast<i32>(product);
  if (result < 0) {
    result += Rand15::kMaximum;
  }
  return result >> Rand15::kValueBits;
}

// decomp: SLUS_010.40 vs_main_getRandSmoothed (src/SLUS_010.40/main.c:8277)
i32 Random::NextSmoothed(i32 max) {
  constexpr int kSampleCount = 12;
  i32 sum = 0;
  for (int index = 0; index < kSampleCount; ++index) {
    sum += Next(max);
  }
  return sum / kSampleCount;
}

}  // namespace VS::Core

namespace {
// The BIOS rand() state shared by every copied caller of rand()/srand(); seeded
// with 1 at boot like the original.
constinit VS::Core::Rand15 g_bios_rand;
}  // namespace

extern "C" int Core_Random_Rand(void) {
  const auto before = g_bios_rand.State();
  const int value = g_bios_rand.Next();
  VS_LOG_TRACE("rng", "raw before={} after={} value={}", before,
               g_bios_rand.State(), value);
  return value;
}

extern "C" void Core_Random_Seed(unsigned int seed) {
  g_bios_rand.Seed(seed);
  VS_LOG_TRACE("rng", "seed={}", seed);
}

extern "C" unsigned int Core_Random_State(void) { return g_bios_rand.State(); }
