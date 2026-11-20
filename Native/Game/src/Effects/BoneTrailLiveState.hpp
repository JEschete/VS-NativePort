#pragma once
// The bone trail palettes in the module image. ModuleData.cpp loads them;
// BoneTrailLive.cpp defines them and reads them into TrailPalettes.

#include <array>

#include "Effects/BoneTrail.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Effects {
// decomp: D_800EC2E8 (src/BATTLE/BATTLE.PRG/5BF94.c:398): the colours a
// textured trail adds or subtracts, by colour number.
extern std::array<VS::Core::Gte::CVECTOR, kAdditiveTrailColors>
    trailAdditiveColors;  // NOLINT(whitespace/indent_namespace)
// decomp: D_800EC308 (src/BATTLE/BATTLE.PRG/5BF94.c:399): the colours of a
// distortion trail, by colour number.
extern std::array<VS::Core::Gte::CVECTOR, kDistortionTrailColors> trailColors;
}  // namespace VS::Effects
