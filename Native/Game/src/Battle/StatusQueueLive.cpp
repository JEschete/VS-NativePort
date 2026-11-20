// Hand-maintained (tools/port/owned.csv).
#include "Battle/StatusQueueLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>

#include "Battle/Actor.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Hit.hpp"
#include "Battle/StatusEffects.hpp"
#include "Battle/StatusQueue.hpp"
#include "Core/Diagnostics.hpp"

using VS::Battle::ActionDefinition;

namespace VS::Battle {
namespace {
constexpr unsigned kExemptActorId = 128;
static_assert(std::size(VS::Battle::statusEffectParams) == kStatusRuleCount);
using StatusHandler = std::optional<StatusFeedback> (*)(
    std::span<const StatusRule>, const StatusTarget&, QueuedStatuses&);

void Require(bool valid, const char* message) {
  if (!valid) {
    VS::Core::Errorf("battle", "%s", message);
    std::abort();
  }
}

VS::Battle::HitResult& HitOf(VS::Battle::HitResult* hit) {
  Require(hit != nullptr, "Status queue requires a hit result");
  return *hit;
}

VS::Battle::ActorStats& StatsOf(const VS::Battle::HitResult& hit) {
  const unsigned actor_id = hit.target.target_actor;
  Require(actor_id < std::size(VS::Battle::actors),
          "Status queue target is outside the battle actor table");
  const auto* actor = VS::Battle::actors[actor_id];
  Require(actor != nullptr && actor->stats != nullptr,
          "Status queue requires the target actor's stats");
  return *actor->stats;
}

StatusTarget TargetOf(const VS::Battle::HitResult& hit) {
  const auto& stats = StatsOf(hit);
  return {.active = std::bit_cast<std::uint32_t>(stats.statuses),
          .immunities = std::bit_cast<std::uint32_t>(stats.status_immunities),
          .exempt = stats.actor_id == kExemptActorId};
}

StatusRule RuleOf(const VS::Battle::StatusEffectParams& params) {
  return {.statuses = std::bit_cast<std::uint32_t>(params.status_flags),
          .cancelled = std::bit_cast<std::uint32_t>(params.cancelled_statuses),
          .blocking = std::bit_cast<std::uint32_t>(params.blocking_statuses)};
}

StatusRule RuleAt(int index) {
  Require(index >= 0 && static_cast<std::size_t>(index) <
                            std::size(VS::Battle::statusEffectParams),
          "Status queue rule is outside its table");
  return RuleOf(VS::Battle::statusEffectParams[index]);
}

QueuedStatuses Load(const VS::Battle::HitResult& hit) {
  // The legacy member names are reversed: remove_statuses adds statuses.
  return {.added = std::bit_cast<std::uint32_t>(hit.remove_statuses),
          .removed = std::bit_cast<std::uint32_t>(hit.inflict_statuses),
          .feedback = static_cast<StatusFeedback>(hit.stat_status_effect)};
}

void Store(const QueuedStatuses& queued, VS::Battle::HitResult& hit) {
  hit.remove_statuses = std::bit_cast<std::int32_t>(queued.added);
  hit.inflict_statuses = std::bit_cast<std::int32_t>(queued.removed);
  hit.stat_status_effect = static_cast<std::uint8_t>(queued.feedback);
}

template <typename Handler, typename... Args>
auto ApplyHandler(VS::Battle::HitResult* target, Handler handler,
                  Args... args) {
  auto& hit = HitOf(target);
  const auto actor = TargetOf(hit);
  std::array<StatusRule, kStatusRuleCount> rules{};
  for (std::size_t index = 0; index < rules.size(); ++index) {
    rules.at(index) = RuleOf(VS::Battle::statusEffectParams[index]);
  }
  auto queued = Load(hit);
  const auto result = handler(rules, actor, queued, args...);
  if (!result) {
    VS::Core::Errorf("battle", "Status handler refused its rule table");
    std::abort();
  }
  Store(queued, hit);
  return result.value();
}

void QueueHandler(VS::Battle::HitResult* target, StatusHandler handler) {
  ApplyHandler(target, handler);
}
}  // namespace

int QueueLiveApplicableStatusChanges(int status_id,
                                     VS::Battle::HitResult* target) {
  auto& hit = HitOf(target);
  const auto rule = RuleAt(status_id);
  const auto actor = TargetOf(hit);
  auto queued = Load(hit);
  const auto result = QueueApplicableStatusChanges(rule, actor, queued);
  Store(queued, hit);
  return std::bit_cast<std::int32_t>(result);
}

int QueueLiveStatusRemovalMask(int status_id, VS::Battle::HitResult* target) {
  auto& hit = HitOf(target);
  const auto rule = RuleAt(status_id);
  const auto actor = TargetOf(hit);
  auto queued = Load(hit);
  const auto result = QueueStatusRemovalMask(rule, actor, queued);
  Store(queued, hit);
  return std::bit_cast<std::int32_t>(result);
}

void QueueLiveStrengthDown(ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueStrengthDown);
}
void QueueLiveIntelligenceDown(ActionDefinition*, VS::Battle::HitResult*,
                               VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueIntelligenceDown);
}
void QueueLiveAgilityDown(ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueAgilityDown);
}
void QueueLiveEquipmentDown(ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueEquipmentDown);
}
void QueueLiveHalfMovement(ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueHalfMovement);
}
void QueueLiveStrengthUp(ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueStrengthUp);
}

// decomp: func_80083B78 (src/BATTLE/BATTLE.PRG/146C.c:12327).
void QueueLiveIntelligenceUp(ActionDefinition*, VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueIntelligenceUp);
}
// decomp: func_80083BC8 (src/BATTLE/BATTLE.PRG/146C.c:12336).
void QueueLiveAgilityUp(ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueAgilityUp);
}
// decomp: func_80083C18 (src/BATTLE/BATTLE.PRG/146C.c:12345).
void QueueLiveEquipmentUp(ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueEquipmentUp);
}
// decomp: func_80083C68 (src/BATTLE/BATTLE.PRG/146C.c:12354).
void QueueLiveQuicken(ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueQuicken);
}
// decomp: func_80083CB8 (src/BATTLE/BATTLE.PRG/146C.c:12363).
void QueueLiveSilence(ActionDefinition*, VS::Battle::HitResult*,
                      VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueSilence);
}
// decomp: func_80083D08 (src/BATTLE/BATTLE.PRG/146C.c:12372).
void QueueLiveMagicWard(ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueMagicWard);
}
// decomp: func_80083D2C (src/BATTLE/BATTLE.PRG/146C.c:12380).
void QueueLiveRegeneration(ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueRegeneration);
}
// decomp: func_80083D7C (src/BATTLE/BATTLE.PRG/146C.c:12389).
void QueueLiveBanish(ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int execute_action) {
  const auto outcome = ApplyHandler(target, QueueBanish, execute_action != 0);
  if (outcome.analyze_requested) {
    StatsOf(HitOf(target)).is_analyzed = 1;
  }
}
void QueueLiveParalysis(ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueParalysis);
}
void QueueLivePoison(ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueuePoison);
}
void QueueLiveNumbness(ActionDefinition*, VS::Battle::HitResult*,
                       VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueNumbness);
}
void QueueLiveCurse(ActionDefinition*, VS::Battle::HitResult*,
                    VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueCurse);
}
void QueueLiveAirAttackUp(ActionDefinition*, VS::Battle::HitResult*,
                          VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueAirAttackUp);
}
void QueueLiveFireAttackUp(ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueFireAttackUp);
}
void QueueLiveEarthAttackUp(ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueEarthAttackUp);
}
void QueueLiveWaterAttackUp(ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueWaterAttackUp);
}
// decomp: func_80083FBC (src/BATTLE/BATTLE.PRG/146C.c:12463).
void QueueLiveAirDefenseUp(ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueAirDefenseUp);
}
// decomp: func_80083FE0 (src/BATTLE/BATTLE.PRG/146C.c:12470).
void QueueLiveFireDefenseUp(ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueFireDefenseUp);
}
// decomp: func_80084004 (src/BATTLE/BATTLE.PRG/146C.c:12477).
void QueueLiveEarthDefenseUp(ActionDefinition*, VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueEarthDefenseUp);
}
// decomp: func_80084028 (src/BATTLE/BATTLE.PRG/146C.c:12484).
void QueueLiveWaterDefenseUp(ActionDefinition*, VS::Battle::HitResult*,
                             VS::Battle::HitResult* target, int, int) {
  ApplyHandler(target, QueueWaterDefenseUp);
}
// decomp: func_8008404C (src/BATTLE/BATTLE.PRG/146C.c:12491).
void QueueLiveParalysisCure(ActionDefinition*, VS::Battle::HitResult*,
                            VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueParalysisCure);
}
// decomp: func_8008409C (src/BATTLE/BATTLE.PRG/146C.c:12500).
void QueueLivePoisonCure(ActionDefinition*, VS::Battle::HitResult*,
                         VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueuePoisonCure);
}
// decomp: func_800840EC (src/BATTLE/BATTLE.PRG/146C.c:12509).
void QueueLiveCurseCure(ActionDefinition*, VS::Battle::HitResult*,
                        VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueCurseCure);
}
// decomp: func_8008413C (src/BATTLE/BATTLE.PRG/146C.c:12518).
void QueueLiveNumbnessCure(ActionDefinition*, VS::Battle::HitResult*,
                           VS::Battle::HitResult* target, int, int) {
  QueueHandler(target, QueueNumbnessCure);
}
// decomp: func_8008418C (src/BATTLE/BATTLE.PRG/146C.c:12527).
void QueueLiveDispel(ActionDefinition*, VS::Battle::HitResult*,
                     VS::Battle::HitResult* target, int, int) {
  auto& hit = HitOf(target);
  auto queued = Load(hit);
  QueueDispel(std::bit_cast<std::uint32_t>(StatsOf(hit).statuses), queued);
  Store(queued, hit);
}

// decomp: func_800841C8 (src/BATTLE/BATTLE.PRG/146C.c:12535).
void QueueLiveNegativeStatusCure(ActionDefinition*, VS::Battle::HitResult*,
                                 VS::Battle::HitResult* target, int, int) {
  auto& hit = HitOf(target);
  auto queued = Load(hit);
  QueueNegativeStatusCure(std::bit_cast<std::uint32_t>(StatsOf(hit).statuses),
                          queued);
  Store(queued, hit);
}
}  // namespace VS::Battle
