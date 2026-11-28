#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/Action.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionCategoryCatalogLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/Actor.hpp"
#include "Battle/CombatActionLive.hpp"
#include "Battle/FloatingLabelPlacementLive.hpp"
#include "Battle/FloatingLabelProducersLive.hpp"
#include "Battle/FloatingLabelQueueLive.hpp"
#include "Battle/StatusHelpersLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Log.hpp"
#include "Field/Gimmicks.hpp"

namespace VS::Battle {
namespace {
constexpr int kRecoveryPose = 6;

struct LiveEvents {
  void operator()(const CombatNumberEvent& event) const {
    const auto slot = LiveCombatPresentationSlot(event.actor);
    if (!slot) return;
    if (event.stat_change) {
      QueueLiveActorStatChangeNumber(
          *slot, event.limb, std::bit_cast<int>(event.amount), event.label);
    } else {
      LiveQueueActorValueLabel(*slot, event.limb, event.amount, event.label);
    }
  }
  void operator()(const CombatStatusEvent& event) const {
    const auto slot = LiveCombatPresentationSlot(event.actor);
    if (!slot) return;
    LiveQueueActorStatusLabels(*slot, 0, std::bit_cast<int>(event.statuses),
                               event.removing);
  }
  void operator()(const CombatEquipmentEvent& event) const {
    if (!LiveCombatPresentationSlot(event.actor)) return;
    LiveQueueEquipmentStatChange(event.label, event.stat,
                                 std::bit_cast<int>(event.amount));
  }
  void operator()(const CombatEquipmentFlush&) const {
    LiveCommitEquipmentStatChanges();
  }
  void operator()(const CombatMovementChange&) const {
    // Movement is committed with the actor's vitals, not by presentation.
  }
};

class LiveTargets final : public ActionTargetEffects {
 public:
  LiveTargets(CombatActionId id, CombatActionResult result,
              std::optional<ActionPoint> position)
      : id_(id), result_(std::move(result)), position_(position) {}

  void QueueWorldLabel() override {
    if (!position_) {
      Core::LogError("combat", "World combat impact has no label position");
      return;
    }
    const std::array<std::uint16_t, 3> point{
        static_cast<std::uint16_t>(position_->x),
        static_cast<std::uint16_t>(position_->y),
        static_cast<std::uint16_t>(position_->z)};
    QueueLiveWorldLockedLabel(point);
  }
  std::size_t HitCount() override {
    return FindLiveCombatAction(id_) ? result_.hits.size() : 0;
  }
  ActionTargetHit Hit(std::size_t index) override {
    return LiveCombatTarget(result_.hits.at(index));
  }
  bool IsAreaAction() override {
    return RequireLiveActionCategory(result_.category).area_target;
  }
  void ApplyHit(std::size_t index) override {
    const auto& hit = result_.hits.at(index).changes;
    const auto applied = CommitLiveCombatImpact(id_, index);
    if (!applied) {
      Core::LogWarn("combat", "Target impact refused: {}", applied.error());
      return;
    }
    const auto hp =
        hit.changes.at(static_cast<std::size_t>(ActionResultField::kHp));
    const auto mp =
        hit.changes.at(static_cast<std::size_t>(ActionResultField::kMp));
    Core::Debugf("combat.result",
                 "action=%u target=%d limb=%u impact=%u hp=%d mp=%d "
                 "hp_effect=%u mp_effect=%u",
                 result_.intent.action, hit.target,
                 static_cast<std::uint8_t>(hit.limb), hit.impact, hp.amount,
                 mp.amount, hp.mode, mp.mode);
    PresentLiveCombatImpact(*applied);
  }
  void PlayDamageReaction(int actor, std::size_t visited) override {
    if (!CanPresent(actor)) return;
    PlayLiveTargetDamageReaction(actor, &actionState->range_origin, visited);
  }
  bool HasActorRecord(int actor) override {
    if (!CanPresent(actor)) return false;
    const auto components =
        Actors::LiveActorComponents(Actors::LiveActorIdentityAt(actor));
    return components && components->body != nullptr;
  }
  void PlayDefense(int actor) override {
    if (!CanPresent(actor)) return;
    PlayLiveDefenseAnimation(actor, &actionState->range_origin);
  }
  void PlayRecoveryPose() override {
    if (!CanPresent(0)) return;
    Actors::PlayLiveActorActionPose(0, nullptr, kRecoveryPose);
  }

 private:
  bool CanPresent(int slot) const {
    if (FindLiveCombatAction(id_) == nullptr || actionState == nullptr) {
      Core::LogWarn("combat", "Reaction belongs to an inactive action");
      return false;
    }
    for (const auto& hit : result_.hits) {
      const auto* actor = std::get_if<CombatActorTarget>(&hit.target);
      if (actor && hit.changes.target == slot) {
        return LiveCombatPresentationSlot(actor->actor).has_value();
      }
    }
    Core::LogWarn("combat", "Reaction actor {} is not an action target", slot);
    return false;
  }
  CombatActionId id_;
  CombatActionResult result_;
  std::optional<ActionPoint> position_;
};
}  // namespace

void PresentLiveCombatImpact(const CombatImpactResult& impact) {
  for (const auto& event : impact.events) {
    if (!LiveCombatPresentationSlot(impact.actor)) return;
    std::visit(LiveEvents{}, event);
  }
}

void ApplyLiveCombatTargetEffects(CombatActionId action, int actor,
                                  std::optional<ActionPoint> position) {
  const auto* result = FindLiveCombatAction(action);
  if (result == nullptr) {
    Core::LogWarn("combat", "Target impact belongs to an inactive action");
    return;
  }
  LiveTargets world(action, *result, position);
  ApplyActionTargetEffects(actor, world);
}

void ApplyLiveCombatPrimaryResult(CombatActionId action) {
  const auto applied = CommitLiveCombatImpact(action, std::nullopt);
  if (!applied) {
    Core::LogWarn("combat", "Source impact refused: {}", applied.error());
    return;
  }
  PresentLiveCombatImpact(*applied);
}
}  // namespace VS::Battle
