// Hand-maintained (tools/port/owned.csv).
// Live binding for the room AI lifecycle (RoomAiMap.cpp).
#include "Battle/Ai/RoomAiMapLive.hpp"

namespace VS::Battle::Ai {
// decomp: BATTLE.PRG func_800E6158 (src/BATTLE/BATTLE.PRG/7CD68.c:814).
void InitializeLiveRoomNavigation() {
  InitializeRoomNavigation(BattleRoomAiRuntime());
}
}  // namespace VS::Battle::Ai
