// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ConditionSnapshot.hpp"

#include <cstdlib>
#include <span>

#include "Core/Diagnostics.h"

namespace VS::Battle::Ai {
// decomp: BATTLE.PRG func_800DC424 (src/BATTLE/BATTLE.PRG/73C24.s:8). The
// original walks vs_battle_actors and stores each actor's ID, HP/MP pairs,
// statuses and weapon/shield DP in its 0x18-byte row; it reads stats without a
// check, so a reading without stats is rejected instead.
void CaptureActorConditionTable(std::span<const ActorConditionReading> readings,
                                ActorConditionTable& table) {
  for (const ActorConditionReading& reading : readings) {
    if (reading.actor_id < 0 || reading.actor_id >= kBattleActorSlotCount ||
        !reading.condition.has_value() ||
        !table.StoreActorCondition(reading.actor_id, *reading.condition)) {
      Core_Errorf("ai", "Invalid condition snapshot actor=%d stats=%d",
                  reading.actor_id,
                  static_cast<int>(reading.condition.has_value()));
      std::abort();
    }
  }
}
}  // namespace VS::Battle::Ai
