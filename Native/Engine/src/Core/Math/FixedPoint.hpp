#pragma once
// The arctangent table (decomp: D_80040A14) and the 16-bit square-root
// entries that follow its first 516 bytes, loaded from the executable image
// (ModuleData.cpp). The radial screen warp reads the square roots
// (Effects/ScreenDistortion/RadialWarpLive.cpp). The size is the address gap
// to the next symbol.
#include <cstddef>
#include <cstdint>

namespace VS::Core::Math {
inline constexpr std::size_t kArcTangentTableBytes = 1296;

// decomp: D_80040A14 (D_80040A14@SLUS_010.40)
extern std::uint8_t kArcTangentTable[kArcTangentTableBytes];
}  // namespace VS::Core::Math
