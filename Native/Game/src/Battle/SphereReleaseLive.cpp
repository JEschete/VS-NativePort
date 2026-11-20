#include "Battle/SphereReleaseLive.hpp"

#include "Battle/Sphere.hpp"
#include "Battle/TargetPages.hpp"
#include "Core/Memory/HeapLive.hpp"

namespace VS::Battle {
// decomp: BATTLE.PRG func_800C05B4 (src/BATTLE/BATTLE.PRG/573B8.c:436).
void FreeSphereEntry() {
  ResetTargetPages();
  if (VS::Battle::targetSphere == nullptr) return;
  VS::Core::Memory::FreeLiveFromLowEnd(VS::Battle::targetSphere);
  VS::Battle::targetSphere = nullptr;
}
}  // namespace VS::Battle
