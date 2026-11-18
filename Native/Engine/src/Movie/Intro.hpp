#pragma once
// The PS1 CD streaming library's interrupt flag (decomp: StCdIntrFlag), which
// the executable image tables (ModulePointers.cpp) still name. The native
// movie player streams from files and never sets it.
#include <cstdint>

namespace VS::Movie {
// decomp: StCdIntrFlag (StCdIntrFlag@SLUS_010.40)
extern std::int32_t legacyStreamInterruptFlag;
}  // namespace VS::Movie
