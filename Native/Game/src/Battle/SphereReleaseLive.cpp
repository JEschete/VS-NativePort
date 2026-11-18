#include "Battle/SphereReleaseLive.hpp"

#include "Battle/TargetPages.hpp"
#include "Core/Memory/HeapLive.hpp"

struct Battle_TargetSphereSetupView;
extern Battle_TargetSphereSetupView* Battle_targetSphere;

namespace VS::Battle {
// decomp: BATTLE.PRG func_800C05B4 (src/BATTLE/BATTLE.PRG/573B8.c:436).
void FreeSphereEntry() {
  ResetTargetPages();
  if (Battle_targetSphere == nullptr) return;
  VS::Core::Memory::FreeLiveFromLowEnd(Battle_targetSphere);
  Battle_targetSphere = nullptr;
}
}  // namespace VS::Battle
