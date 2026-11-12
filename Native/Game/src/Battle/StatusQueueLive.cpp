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
constexpr int kStatusFeedbackShift = 16;
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

StatusTarget TargetOf(const Battle_HitResult& hit) {
  const unsigned actor_id = hit.target.targetActor;
  Require(actor_id < std::size(Battle_actors),
          "Status queue target is outside the battle actor table");
  const auto* actor = Battle_actors[actor_id];
  Require(actor != nullptr && actor->stats != nullptr,
          "Status queue requires the target actor's stats");
  const auto& stats = *actor->stats;
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

int QueueHandler(Battle_HitResult* target, StatusHandler handler) {
  auto& hit = HitOf(target);
  const auto actor = TargetOf(hit);
  std::array<StatusRule, kStatusRuleCount> rules{};
  for (std::size_t index = 0; index < rules.size(); ++index) {
    rules.at(index) = RuleOf(Battle_statusEffectParams[index]);
  }
  auto queued = Load(hit);
  const auto result = handler(rules, actor, queued);
  if (!result) {
    Core_Errorf("battle", "Status handler refused its rule table");
    std::abort();
  }
  Store(queued, hit);
  // The original callbacks return this register value, although their C
  // declarations fall through without a return.
  return static_cast<int>(result.value()) << kStatusFeedbackShift;
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

int QueueLiveStrengthDown(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult* target, int, int) {
  return QueueHandler(target, QueueStrengthDown);
}
int QueueLiveIntelligenceDown(Battle_ActionDefinition*, Battle_HitResult*,
                              Battle_HitResult* target, int, int) {
  return QueueHandler(target, QueueIntelligenceDown);
}
int QueueLiveAgilityDown(Battle_ActionDefinition*, Battle_HitResult*,
                         Battle_HitResult* target, int, int) {
  return QueueHandler(target, QueueAgilityDown);
}
int QueueLiveEquipmentDown(Battle_ActionDefinition*, Battle_HitResult*,
                           Battle_HitResult* target, int, int) {
  return QueueHandler(target, QueueEquipmentDown);
}
int QueueLiveHalfMovement(Battle_ActionDefinition*, Battle_HitResult*,
                          Battle_HitResult* target, int, int) {
  return QueueHandler(target, QueueHalfMovement);
}
int QueueLiveStrengthUp(Battle_ActionDefinition*, Battle_HitResult*,
                        Battle_HitResult* target, int, int) {
  return QueueHandler(target, QueueStrengthUp);
}
}  // namespace VS::Battle
