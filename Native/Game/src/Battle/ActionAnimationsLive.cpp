#include "Battle/ActionAnimationsLive.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/ActionAnimations.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionFlow.h"
#include "Core/Diagnostics.h"
#include "Effects/ActionEffects.h"
}

namespace VS::Battle {
namespace {
constexpr std::size_t kRetainedOwners = 3;

const char* Reason(ActionAnimationResult result) {
  switch (result) {
    case ActionAnimationResult::kComplete:
      return "complete";
    case ActionAnimationResult::kMissingActor:
      return "missing used actor";
    case ActionAnimationResult::kInvalidCastPhase:
      return "cast phase outside 0..1";
    case ActionAnimationResult::kInvalidChainAbility:
      return "chain animation ability outside 0..17";
    case ActionAnimationResult::kNegativeSquaredDistance:
      return "negative wrapped square-root argument";
    case ActionAnimationResult::kMissingRootBone:
      return "root bone query produced no position";
    case ActionAnimationResult::kMissingTarget:
      return "used target owner disappeared";
  }
  return "unknown result";
}

void Require(bool valid) {
  if (!valid) {
    Core_Errorf("battle", "Action animation refused: invalid used state");
    std::abort();
  }
}
void Require(ActionAnimationResult result) {
  if (result != ActionAnimationResult::kComplete) {
    Core_Errorf("battle", "Action animation refused: %s", Reason(result));
    std::abort();
  }
}

class LiveActor final : public ActionReactionActor {
 public:
  explicit LiveActor(Actors_ActorRecord& record) : record_(record) {}
  Actors_ActorRecord& Record() { return record_; }
  std::uint8_t Slot() const override { return record_.instance.slot; }
  bool WeaponDrawn() const override {
    return record_.instance.weaponDrawn != 0;
  }
  std::uint8_t MovementMode() const override {
    return record_.instance.movementMode;
  }
  bool MovementBlocked() const override {
    return record_.instance.motionLocked != 0 ||
           record_.instance.isJumping != 0;
  }
  std::uint16_t Radius() const override { return record_.collisionRadius; }
  std::uint16_t ModelId() const override { return record_.modelId; }
  Actors::MotionVector Position() const override {
    const auto& point = record_.instance.position;
    return {point.vx, point.vy, point.vz};
  }
  std::int16_t Facing() const override { return record_.instance.facing; }
  void SetFacingDelta(std::int16_t delta) override {
    record_.instance.facingDelta = delta;
  }
  void SetTurnFrames(std::int16_t frames) override {
    record_.instance.turnFrames = static_cast<u_short>(frames);
  }
  void ClearMovementMode() override { record_.instance.movementMode = 0; }
  void ClearJumpCountdown() override { record_.instance.jumpCountdown = 0; }
  std::uint8_t AnimationId() const override { return record_.animationId; }
  void CopyPoseToBlend() override { record_.blendPose = record_.pose; }
  void SetPoseState(std::uint8_t state) override {
    record_.instance.poseState = state;
  }

 private:
  Actors_ActorRecord& record_;
};

class LiveWorld final : public ActionReactionWorld {
 public:
  explicit LiveWorld(SVECTOR* target) : target_(target) {}
  ActionReactionActor* Actor(int slot) override {
    Require(slot >= 0 &&
            static_cast<std::size_t>(slot) < std::size(Actors_actorRecords));
    auto* record = Actors_actorRecords[slot];
    if (record == nullptr) return nullptr;
    for (auto& owner : owners_) {
      if (owner && &owner->Record() == record) return &owner.value();
    }
    for (auto& owner : owners_) {
      if (!owner) return &owner.emplace(*record);
    }
    Require(false);
    return nullptr;
  }
  std::optional<Actors::MotionVector> Target() const override {
    if (target_ == nullptr) return std::nullopt;
    return Actors::MotionVector{target_->vx, target_->vy, target_->vz};
  }
  std::optional<std::int16_t> RootBoneHeight(std::uint8_t slot) override {
    SVECTOR root;
    if (Actors::LiveActorBonePosition(slot, 0, &root, 0) != 0) {
      return std::nullopt;
    }
    return root.vy;
  }
  bool HasAnimation(ActionAnimationActor& actor, int animation) override {
    u_short* header = nullptr;
    auto& record = static_cast<LiveActor&>(actor).Record();
    static_cast<void>(
        Actors::FindLiveActorAnimationHeader(&record, animation, &header));
    return header != nullptr;
  }
  std::uint8_t ChainGroup(int ability) override {
    Require(ability >= 0 && ability < kChainAnimationAbilities);
    return Battle_chainAbilityAnimationGroups[ability];
  }
  std::uint32_t ChainSide() override {
    return std::bit_cast<std::uint32_t>(Battle_chainAbilityAnimationSide);
  }
  void SetChainSide(std::uint32_t side) override {
    Battle_chainAbilityAnimationSide = std::bit_cast<int>(side);
  }
  void SetSpecialPitch(std::uint8_t pitch) override {
    Battle_specialActionPitch = static_cast<char>(pitch);
  }
  void Play(int slot, std::int32_t animation, int blend) override {
    Actors::PlayLiveActorAnimation(slot, animation, 0, blend);
  }
  void Queue(int slot, int animation, int blend, int start) override {
    Actors::QueueLiveActorAnimation(slot, animation, blend, start);
  }
  void ResetHead(ActionAnimationActor& actor) override {
    auto& record = static_cast<LiveActor&>(actor).Record();
    Actors::ResetLiveActorHeadTrackingSpeed(&record);
  }
  void TurnTo(int slot, std::int16_t facing, int rate) override {
    Actors::TurnLiveActorToAtRate(slot, facing, rate);
  }
  void QueueImpactEffects(int slot) override {
    Require(Battle_actionState != nullptr);
    static_cast<void>(Effects_QueueActionImpactEffects(slot));
  }

 private:
  SVECTOR* target_;
  // Initial owner, optional F00833 fallback, and F00834 follow-up owner.
  std::array<std::optional<LiveActor>, kRetainedOwners> owners_;
};
}  // namespace

// F00817: func_8009DD00, src/BATTLE/BATTLE.PRG/30DB0.c:2331.
void PlayLiveMeleeAttackAnimation(int slot, SVECTOR* target, int parameter) {
  LiveWorld world(target);
  Require(PlayMeleeAttackAnimation(slot, parameter, world));
}
// F00818: func_8009DDF4, src/BATTLE/BATTLE.PRG/30DB0.c:2365.
void PlayLiveDefenseAnimation(int slot, SVECTOR* target) {
  LiveWorld world(target);
  Require(PlayDefenseAnimation(slot, world));
}
// decomp: func_8009DF3C (src/BATTLE/BATTLE.PRG/30DB0.c:2408), F00819.
void SetLiveEnemyCombatPose(int slot, int mode) {
  LiveWorld world(nullptr);
  Require(SetEnemyCombatPose(slot, mode, world));
}
// F00823: func_8009E2E0, src/BATTLE/BATTLE.PRG/30DB0.c:2554.
void PlayLiveSpellCastAnimation(int slot, SVECTOR* target, int phase) {
  LiveWorld world(target);
  Require(PlaySpellCastAnimation(slot, phase, world));
}
// F00831: func_8009EC9C, src/BATTLE/BATTLE.PRG/30DB0.c:2956.
void PlayLiveChainAbilityAnimation(int slot, SVECTOR* target, int ability,
                                   int sequence) {
  LiveWorld world(target);
  Require(PlayChainAbilityAnimation(slot, ability, sequence, world));
}
// decomp: func_8009EE9C (src/BATTLE/BATTLE.PRG/30DB0.c:3040), F00832.
void PlayLiveDefenseAbilityAnimation(int slot, SVECTOR* target,
                                     int /*unused*/) {
  LiveWorld world(target);
  Require(PlayDefenseAbilityAnimation(slot, world));
}
// F00833: func_8009EFEC, src/BATTLE/BATTLE.PRG/30DB0.c:3087.
void PlayLiveSpecialActionAnimation(int slot, SVECTOR* target, int mode) {
  LiveWorld world(target);
  Require(PlaySpecialActionAnimation(slot, mode, world));
}
// F00834: func_8009F298, src/BATTLE/BATTLE.PRG/30DB0.c:3189.
void PlayLiveBreakArtAnimation(int slot, SVECTOR* target, int mode) {
  LiveWorld world(target);
  Require(PlayBreakArtAnimation(slot, mode, world));
}
// decomp: func_800A2574 (src/BATTLE/BATTLE.PRG/38C1C.c:625), F00891.
void FinishLiveEnemyCombatPose(int slot, std::int16_t facing) {
  LiveWorld world(nullptr);
  Require(FinishEnemyCombatPose(slot, facing, world));
}
}  // namespace VS::Battle
