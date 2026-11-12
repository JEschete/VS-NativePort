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

#include "Battle/StatusQueue.hpp"
extern "C" {
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle {
namespace {
constexpr unsigned kExemptActorId = 128;
static_assert(std::size(Battle_statusEffectParams) == kStatusRuleCount);
using StatusHandler = std::optional<StatusFeedback> (*)(
    std::span<const StatusRule>, const StatusTarget&, QueuedStatuses&);

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("battle", "%s", message);
    std::abort();
  }
}

Battle_HitResult& HitOf(Battle_HitResult* hit) {
  Require(hit != nullptr, "Status queue requires a hit result");
  return *hit;
}

Battle_ActorStats& StatsOf(const Battle_HitResult& hit) {
  const unsigned actor_id = hit.target.targetActor;
  Require(actor_id < std::size(Battle_actors),
          "Status queue target is outside the battle actor table");
  const auto* actor = Battle_actors[actor_id];
  Require(actor != nullptr && actor->stats != nullptr,
          "Status queue requires the target actor's stats");
  return *actor->stats;
}

StatusTarget TargetOf(const Battle_HitResult& hit) {
  const auto& stats = StatsOf(hit);
  return {.active = std::bit_cast<std::uint32_t>(stats.statuses),
          .immunities = std::bit_cast<std::uint32_t>(stats.statusImmunities),
          .exempt = stats.actorId == kExemptActorId};
}

StatusRule RuleOf(const Battle_StatusEffectParams& params) {
  return {.statuses = std::bit_cast<std::uint32_t>(params.statusFlags),
          .cancelled = std::bit_cast<std::uint32_t>(params.cancelledStatuses),
          .blocking = std::bit_cast<std::uint32_t>(params.blockingStatuses)};
}

StatusRule RuleAt(int index) {
  Require(index >= 0 && static_cast<std::size_t>(index) <
                            std::size(Battle_statusEffectParams),
          "Status queue rule is outside its table");
  return RuleOf(Battle_statusEffectParams[index]);
}

QueuedStatuses Load(const Battle_HitResult& hit) {
  // The legacy member names are reversed: removeStatuses adds statuses.
  return {.added = std::bit_cast<std::uint32_t>(hit.removeStatuses),
          .removed = std::bit_cast<std::uint32_t>(hit.inflictStatuses),
          .feedback = static_cast<StatusFeedback>(hit.statStatusEffect)};
}

void Store(const QueuedStatuses& queued, Battle_HitResult& hit) {
  hit.removeStatuses = std::bit_cast<std::int32_t>(queued.added);
  hit.inflictStatuses = std::bit_cast<std::int32_t>(queued.removed);
  hit.statStatusEffect = static_cast<std::uint8_t>(queued.feedback);
}

template <typename Handler, typename... Args>
auto ApplyHandler(Battle_HitResult* target, Handler handler, Args... args) {
  auto& hit = HitOf(target);
  const auto actor = TargetOf(hit);
  std::array<StatusRule, kStatusRuleCount> rules{};
  for (std::size_t index = 0; index < rules.size(); ++index) {
    rules.at(index) = RuleOf(Battle_statusEffectParams[index]);
  }
  auto queued = Load(hit);
  const auto result = handler(rules, actor, queued, args...);
  if (!result) {
    Core_Errorf("battle", "Status handler refused its rule table");
    std::abort();
  }
  Store(queued, hit);
  return result.value();
}

void QueueHandler(Battle_HitResult* target, StatusHandler handler) {
  ApplyHandler(target, handler);
}
}  // namespace

int QueueLiveApplicableStatusChanges(int status_id, Battle_HitResult* target) {
  auto& hit = HitOf(target);
  const auto rule = RuleAt(status_id);
  const auto actor = TargetOf(hit);
  auto queued = Load(hit);
  const auto result = QueueApplicableStatusChanges(rule, actor, queued);
  Store(queued, hit);
  return std::bit_cast<std::int32_t>(result);
}

int QueueLiveStatusRemovalMask(int status_id, Battle_HitResult* target) {
  auto& hit = HitOf(target);
  const auto rule = RuleAt(status_id);
  const auto actor = TargetOf(hit);
  auto queued = Load(hit);
  const auto result = QueueStatusRemovalMask(rule, actor, queued);
  Store(queued, hit);
  return std::bit_cast<std::int32_t>(result);
}

void QueueLiveStrengthDown(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueStrengthDown);
}
void QueueLiveIntelligenceDown(Battle_ActionDefinition*, Battle_HitResult*,
                               Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueIntelligenceDown);
}
void QueueLiveAgilityDown(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueAgilityDown);
}
void QueueLiveEquipmentDown(Battle_ActionDefinition*, Battle_HitResult*,
                            Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueEquipmentDown);
}
void QueueLiveHalfMovement(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueHalfMovement);
}
void QueueLiveStrengthUp(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueStrengthUp);
}

// decomp: func_80083B78 (src/BATTLE/BATTLE.PRG/146C.c:12327).
void QueueLiveIntelligenceUp(Battle_ActionDefinition*, Battle_HitResult*,
                             Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueIntelligenceUp);
}
// decomp: func_80083BC8 (src/BATTLE/BATTLE.PRG/146C.c:12336).
void QueueLiveAgilityUp(Battle_ActionDefinition*, Battle_HitResult*,
                        Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueAgilityUp);
}
// decomp: func_80083C18 (src/BATTLE/BATTLE.PRG/146C.c:12345).
void QueueLiveEquipmentUp(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueEquipmentUp);
}
// decomp: func_80083C68 (src/BATTLE/BATTLE.PRG/146C.c:12354).
void QueueLiveQuicken(Battle_ActionDefinition*, Battle_HitResult*,
                      Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueQuicken);
}
// decomp: func_80083CB8 (src/BATTLE/BATTLE.PRG/146C.c:12363).
void QueueLiveSilence(Battle_ActionDefinition*, Battle_HitResult*,
                      Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueSilence);
}
// decomp: func_80083D08 (src/BATTLE/BATTLE.PRG/146C.c:12372).
void QueueLiveMagicWard(Battle_ActionDefinition*, Battle_HitResult*,
                        Battle_HitResult* target, int, int) {
  ApplyHandler(target, QueueMagicWard);
}
// decomp: func_80083D2C (src/BATTLE/BATTLE.PRG/146C.c:12380).
void QueueLiveRegeneration(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int) {
  QueueHandler(target, QueueRegeneration);
}
// decomp: func_80083D7C (src/BATTLE/BATTLE.PRG/146C.c:12389).
void QueueLiveBanish(Battle_ActionDefinition*, Battle_HitResult*,
                     Battle_HitResult* target, int, int execute_action) {
  const auto outcome = ApplyHandler(target, QueueBanish, execute_action != 0);
  if (outcome.analyze_requested) {
    StatsOf(HitOf(target)).isAnalyzed = 1;
  }
}
}  // namespace VS::Battle
