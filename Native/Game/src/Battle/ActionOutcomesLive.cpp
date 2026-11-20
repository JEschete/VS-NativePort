// Hand-maintained (tools/port/owned.csv).
#include "Battle/ActionOutcomesLive.hpp"

#include <bit>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>

#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/Location.hpp"
#include "Audio/SfxLive.hpp"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.hpp"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionFlow.hpp"
#include "Battle/ActionOutcomes.hpp"
#include "Battle/ActionResultsLive.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/Feedback.hpp"
#include "Battle/HudOrchestrationLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Items/LootLive.hpp"
#include "Save/Snapshot.hpp"
#include "State/KillScoreLive.hpp"
#include "State/Progress.hpp"

namespace VS::Battle {
namespace {
void Require(bool valid) {
  if (!valid) {
    VS::Core::Errorf("battle",
                     "Action outcome refused: missing used state, actor, "
                     "stats, placement, or invalid target count/index");
    std::abort();
  }
}
Battle_ActionState& State() {
  Require(VS::Battle::actionState != nullptr);
  return *VS::Battle::actionState;
}
Battle_Actor* ActorAt(int slot) {
  Require(slot >= 0 &&
          static_cast<std::size_t>(slot) < std::size(VS::Battle::actors));
  return VS::Battle::actors[slot];
}
Battle_Actor& Actor(int slot) {
  auto* actor = ActorAt(slot);
  Require(actor != nullptr);
  return *actor;
}
ActionTargetHit Hit(const VS::Battle::HitResult& hit) {
  return {.targets_position = hit.targets_position != 0,
          .target_actor = hit.target.target_actor,
          .impact_kind = static_cast<std::uint8_t>(hit.target.impact_kind)};
}
class LiveEnemy final : public EnemyDefeatRecord {
 public:
  explicit LiveEnemy(Battle_Actor& actor) : actor_(actor) {}
  std::uint8_t Spawn() const override { return actor_.roomSpawnIndex; }
  bool Defeated() const override { return actor_.defeated != 0; }
  void RecordKill() override { VS::State::RecordLiveEnemyKillScore(&actor_); }
  void UpdateWeaponStreak() override {
    VS::State::UpdateLiveWeaponKillStreak(0);
  }
  void QueueLoot() override { Items::QueueLiveEnemyLoot(&actor_); }
  void MarkDefeated() override { actor_.defeated = 1; }

 private:
  Battle_Actor& actor_;
};
class LiveAction final : public ActionOutcomeAction {
 public:
  explicit LiveAction(Battle_ActionRequest& action) : action_(action) {}
  std::uint16_t Count() const override {
    Require(action_.targetCount <= std::size(action_.targets));
    return action_.targetCount;
  }
  ActionTargetHit Target(std::size_t index) const override {
    Require(index < std::size(action_.targets));
    return Hit(action_.targets[index]);
  }
  ActionTargetHit Primary() const override {
    return Hit(action_.primaryTarget);
  }
  void DamageReaction(int actor) override {
    PlayLiveActorDamageReaction(actor, &action_.focusPosition);
  }
  void ApplyPrimaryResult() override {
    ApplyLiveActorActionResult(&action_.primaryTarget);
  }

 private:
  Battle_ActionRequest& action_;
};
class LiveOutcomeWorld final : public ActionOutcomeWorld {
 public:
  ActionOutcomeAction& Queued() override {
    auto& state = State();
    constexpr std::uint32_t kQueueMask = 3;
    action_.emplace(state.queue[state.queueIndex & kQueueMask]);
    return action_.value();
  }
  ActionPoint Placement(int actor) override {
    VS::Actors::ActorLocationQuery placement{};
    Require(Actors::LiveActorPlacement(actor, &placement) == 0);
    const auto& point = placement.location.position;
    return {point.vx, point.vy, point.vz, 0};
  }
  std::uint8_t WeaponCategory(int actor) override {
    return Actor(actor).equippedWeaponCategory;
  }
  void PlaySound(std::uint8_t sound, ActionPoint point) override {
    constexpr int kHitSoundGroup = 0x7E;
    auto position = std::bit_cast<SVECTOR>(point);
    Audio::LivePlayWorldSfx(kHitSoundGroup, sound, &position);
  }
  void RecoveryPose() override {
    constexpr int kRecoveryPose = 6;
    Actors::PlayLiveActorActionPose(0, nullptr, kRecoveryPose);
  }

 private:
  std::optional<LiveAction> action_;
};
class LiveDefeatWorld final : public ActorDefeatWorld {
 public:
  void RefreshReservations(int actor) override {
    Ai::RefreshLiveActorAiReservations(actor);
  }
  bool HasActor(int actor) override { return ActorAt(actor) != nullptr; }
  void SetCommandState(int actor, std::uint8_t state) override {
    Actor(actor).commandState = state;
  }
  std::int16_t Hp(int actor) override {
    auto* stats = Actor(actor).stats;
    Require(stats != nullptr);
    return stats->currentHp;
  }
  void HideMarker(int actor) override {
    VS::Battle::SetLiveActorMarker(actor, 0);
  }
  bool Retained(int actor) override {
    return Actor(actor).retainAfterDefeat != 0;
  }
  void BeginDeath(int actor) override {
    Actors::BeginLiveActorDeath(actor, &State().rangeOrigin, 1);
  }
  void LockAnimation(int actor) override {
    Actors::SetLiveActorAnimationLocked(actor, 1);
  }
  std::uint8_t Spawn(int actor) override { return Actor(actor).roomSpawnIndex; }
};
class LiveLearnedWorld final : public LearnedActionWorld {
 public:
  std::uint8_t LearnedByte(std::size_t index) const override {
    Require(index < std::size(VS::State::learnedActionFlags));
    return static_cast<std::uint8_t>(VS::State::learnedActionFlags[index]);
  }
  void SetUnlocked(std::size_t index, bool unlocked) override {
    Require(index < std::size(Battle_actionDefinitions));
    Battle_actionDefinitions[index].unlocked = unlocked;
  }
};
static_assert(std::size(VS::State::learnedActionFlags) ==
              Save::kLearnedActionBytes);
static_assert(std::size(Battle_actionDefinitions) == Save::kLearnedActionCount);
static_assert(std::extent_v<decltype(Battle_ActionRequest::targets)> ==
              kActionTargetCapacity);
}  // namespace

void ResolveLiveEnemyDefeat(int source_actor, int target_actor,
                            int count_kill) {
  LiveEnemy enemy(Actor(target_actor));
  ResolveEnemyDefeat(source_actor, count_kill, enemy);
}
int PlayLiveWeaponHitSounds(int source_actor) {
  LiveOutcomeWorld world;
  return PlayWeaponHitSounds(source_actor, world);
}
void ApplyLiveActionSourceEffects(int source_actor, SVECTOR*) {
  LiveOutcomeWorld world;
  ApplyActionSourceEffects(source_actor, world);
}
int BeginLiveActorDefeatAnimation(int actor) {
  LiveDefeatWorld world;
  return BeginActorDefeatAnimation(actor, world);
}
void ApplyLiveLearnedActionUnlocks() {
  LiveLearnedWorld world;
  ApplyLearnedActionUnlocks(world);
}
}  // namespace VS::Battle
