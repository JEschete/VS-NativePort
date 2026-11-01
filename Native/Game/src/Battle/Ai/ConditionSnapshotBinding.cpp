// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ConditionSnapshotBinding.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>

#include "Battle/Ai/ConditionSnapshot.hpp"
#include "Core/Diagnostics.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
}

namespace VS::Battle::Ai {
namespace {
class PlannerConditionRows final : public ActorConditionTable {
 public:
  explicit PlannerConditionRows(
      std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows)
      : rows_(rows) {}

  bool StoreActorCondition(int actor_id,
                           const ActorCondition& condition) override {
    if (actor_id < 0 || actor_id >= kBattleActorSlotCount) return false;
    Battle_Ai_ActionTiming& row = rows_[static_cast<std::size_t>(actor_id)];
    row = Battle_Ai_ActionTiming{};
    row.opaque0Bit0 = static_cast<unsigned>(actor_id);
    row.opaque4 = condition.hp;
    row.opaque6 = condition.max_hp;
    row.opaque8 = condition.mp;
    row.opaqueA = condition.max_mp;
    row.opaqueC = condition.statuses;
    row.opaque10 = condition.weapon_dp;
    row.opaque12 = condition.weapon_max_dp;
    row.opaque14 = condition.shield_dp;
    row.opaque16 = condition.shield_max_dp;
    return true;
  }

 private:
  std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows_;
};

std::optional<ActorCondition> ReadCondition(const Battle_ActorStats* stats) {
  if (stats == nullptr) return std::nullopt;
  return ActorCondition{
      stats->currentHp,
      stats->maxHp,
      stats->currentMp,
      stats->maxMp,
      static_cast<std::uint32_t>(stats->statuses),
      static_cast<std::int16_t>(stats->weapon.currentDp),
      static_cast<std::int16_t>(stats->weapon.maxDp),
      static_cast<std::int16_t>(stats->shield.currentDp),
      static_cast<std::int16_t>(stats->shield.maxDp),
  };
}
}  // namespace

void CaptureBattleActorConditions(
    std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows) {
  std::array<ActorConditionReading, std::size(Battle_actors)> readings{};
  std::size_t count = 0;
  for (const Battle_Actor* actor = Battle_actors[0]; actor != nullptr;
       actor = actor->next) {
    if (count == readings.size()) {
      Core_Errorf("ai", "Battle actor list holds more than %zu actors",
                  readings.size());
      std::abort();
    }
    readings[count++] = {actor->id, ReadCondition(actor->stats)};
  }
  PlannerConditionRows table(rows);
  CaptureActorConditionTable(std::span(readings).first(count), table);
}
}  // namespace VS::Battle::Ai
