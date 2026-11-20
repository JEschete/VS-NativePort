// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ConditionSnapshotBinding.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>

#include "Battle/Actor.hpp"
#include "Battle/Ai/ConditionSnapshot.hpp"
#include "Battle/BattleTypes.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Legacy/Psx.h"

namespace VS::Battle::Ai {
namespace {
class PlannerConditionRows final : public ActorConditionTable {
 public:
  explicit PlannerConditionRows(
      std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows)
      : rows_(rows) {}

  bool StoreActorCondition(int actor_id,
                           const ActorCondition& condition) override {
    if (actor_id < 0 || actor_id >= kBattleActorSlotCount) return false;
    VS::Battle::Ai::ActionTiming& row =
        rows_[static_cast<std::size_t>(actor_id)];
    row = VS::Battle::Ai::ActionTiming{};
    row.opaque0_bit0 = static_cast<unsigned>(actor_id);
    row.opaque4 = condition.hp;
    row.opaque6 = condition.max_hp;
    row.opaque8 = condition.mp;
    row.opaque_a = condition.max_mp;
    row.opaque_c = condition.statuses;
    row.opaque10 = condition.weapon_dp;
    row.opaque12 = condition.weapon_max_dp;
    row.opaque14 = condition.shield_dp;
    row.opaque16 = condition.shield_max_dp;
    return true;
  }

 private:
  std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows_;
};

std::optional<ActorCondition> ReadCondition(
    const VS::Battle::ActorStats* stats) {
  if (stats == nullptr) return std::nullopt;
  return ActorCondition{
      stats->current_hp,
      stats->max_hp,
      stats->current_mp,
      stats->max_mp,
      static_cast<std::uint32_t>(stats->statuses),
      static_cast<std::int16_t>(stats->weapon.current_dp),
      static_cast<std::int16_t>(stats->weapon.max_dp),
      static_cast<std::int16_t>(stats->shield.current_dp),
      static_cast<std::int16_t>(stats->shield.max_dp),
  };
}
}  // namespace

void CaptureBattleActorConditions(
    std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows) {
  std::array<ActorConditionReading, std::size(VS::Battle::actors)> readings{};
  std::size_t count = 0;
  for (const VS::Battle::Actor* actor = VS::Battle::actors[0]; actor != nullptr;
       actor = actor->next) {
    if (count == readings.size()) {
      VS::Core::Errorf("ai", "Battle actor list holds more than %zu actors",
                       readings.size());
      std::abort();
    }
    readings[count++] = {actor->id, ReadCondition(actor->stats)};
  }
  PlannerConditionRows table(rows);
  CaptureActorConditionTable(std::span(readings).first(count), table);
}
}  // namespace VS::Battle::Ai
