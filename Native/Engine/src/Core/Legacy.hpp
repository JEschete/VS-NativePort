#pragma once
// The PS1 SDK runtime's link-time configuration (decomp: 2MBYTE.OBJ), which
// the executable image tables (ModulePointers.cpp) still name. Native code
// never reads them: the build and the process own the stamp, RAM and stack.
#include <cstdint>

namespace VS::Core {
// decomp: _buildTimestamp (_buildTimestamp@SLUS_010.40)
extern char legacyBuildTimestamp[];
// decomp: _ramsize (_ramsize@SLUS_010.40)
extern std::uint32_t legacyRamSize;
// decomp: _stacksize (_stacksize@SLUS_010.40)
extern std::uint32_t legacyStackSize;
}  // namespace VS::Core
