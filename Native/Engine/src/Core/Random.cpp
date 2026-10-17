#include "Core/Random.hpp"
#include "Core/BiosRand.h"

namespace VS::Core {

// decomp: SLUS_010.40 _initRand (src/SLUS_010.40/main.c:8239)
void Random::Reset(u32 seed)
{
    m_rand.Seed(seed);
    for (auto& v : m_table) {
        v = m_rand.Next();
    }
    m_index = static_cast<i32>(TableSize) - 1;
}

// decomp: SLUS_010.40 vs_main_getRand (src/SLUS_010.40/main.c:8251)
i32 Random::Next(i32 max)
{
    i32 scaled = m_table[static_cast<usize>(m_index)] * static_cast<i32>(TableSize);
    if (scaled < 0) {
        scaled += 0x7FFF;
    }
    m_index = scaled >> 15;

    const i32 picked = 0x7FFF - m_table[static_cast<usize>(m_index)];
    m_table[static_cast<usize>(m_index)] = m_rand.Next();

    i32 result = picked * max;
    if (result < 0) {
        result += 0x7FFF;
    }
    return result >> 15;
}

// decomp: SLUS_010.40 vs_main_getRandSmoothed (src/SLUS_010.40/main.c:8277)
i32 Random::NextSmoothed(i32 max)
{
    i32 sum = 0;
    for (int i = 0; i < 12; ++i) {
        sum += Next(max);
    }
    return sum / 12;
}

} // namespace VS::Core

namespace {
// The BIOS rand() state shared by every copied caller of rand()/srand(); seeded with 1 at boot like the original.
VS::Core::Rand15 g_biosRand;
} // namespace

extern "C" int Core_Random_Rand(void)
{
    return g_biosRand.Next();
}

extern "C" void Core_Random_Seed(unsigned int seed)
{
    g_biosRand.Seed(seed);
}
