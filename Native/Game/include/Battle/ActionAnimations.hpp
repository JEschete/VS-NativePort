#pragma once

#include <cstdint>
#include <optional>

#include "Actors/ActorMotion.hpp"

namespace VS::Battle {
// Mapped chain attacks are action IDs 22..39, not the physical extent of the
// legacy animation-group allocation.
inline constexpr int kChainAnimationAbilities = 18;

enum class ActionAnimationResult : std::uint8_t {
  kComplete,
  kMissingActor,
  kInvalidCastPhase,
  kInvalidChainAbility,
  kNegativeSquaredDistance,
  kMissingRootBone,
  kMissingTarget
};

struct ActionAnimationPitch {
  std::int32_t value{};
  ActionAnimationResult result = ActionAnimationResult::kComplete;
};

// Each source retains an actor owner, while its accessors read current fields
// after service callbacks. No whole-record snapshot is committed.
class ActionAnimationActor {
 public:
  virtual ~ActionAnimationActor() = default;
  virtual std::uint8_t Slot() const = 0;
  virtual bool WeaponDrawn() const = 0;
  virtual std::uint8_t MovementMode() const = 0;
  virtual bool MovementBlocked() const = 0;
  virtual std::uint16_t Radius() const = 0;
  virtual std::uint16_t ModelId() const = 0;
  virtual Actors::MotionVector Position() const = 0;
  virtual std::int16_t Facing() const = 0;
  virtual void SetFacingDelta(std::int16_t delta) = 0;
  virtual void SetTurnFrames(std::int16_t frames) = 0;
  virtual void ClearMovementMode() = 0;
  virtual void ClearJumpCountdown() = 0;
};

class ActionAnimationWorld {
 public:
  virtual ~ActionAnimationWorld() = default;
  // Returned sources remain valid across subsequent lookups, including when
  // the table entry is replaced by a callback.
  virtual ActionAnimationActor* Actor(int slot) = 0;
  // The same nullable input point is retained for the policy's lifetime.
  virtual std::optional<Actors::MotionVector> Target() const = 0;
  virtual std::optional<std::int16_t> RootBoneHeight(std::uint8_t slot) = 0;
  virtual std::int32_t FacingDeltaToTarget(ActionAnimationActor& actor);
  virtual ActionAnimationPitch PitchToTarget(ActionAnimationActor& actor);
  virtual bool HasAnimation(ActionAnimationActor& actor, int animation) = 0;
  virtual std::uint8_t ChainGroup(int ability) = 0;
  virtual std::uint32_t ChainSide() = 0;
  virtual void SetChainSide(std::uint32_t side) = 0;
  virtual void SetSpecialPitch(std::uint8_t pitch) = 0;
  virtual void Play(int slot, std::int32_t animation, int blend) = 0;
  virtual void Queue(int slot, int animation, int blend, int start) = 0;
  virtual void ResetHead(ActionAnimationActor& actor) = 0;
};

class ActionReactionActor : public ActionAnimationActor {
 public:
  virtual std::uint8_t AnimationId() const = 0;
  virtual void CopyPoseToBlend() = 0;
  virtual void SetPoseState(std::uint8_t state) = 0;
};

class ActionReactionWorld : public ActionAnimationWorld {
 public:
  ActionReactionActor* Actor(int slot) override = 0;
  virtual void TurnTo(int slot, std::int16_t facing, int rate) = 0;
  virtual void QueueImpactEffects(int slot) = 0;
};

ActionAnimationResult PlayMeleeAttackAnimation(int slot, int parameter,
                                               ActionAnimationWorld& world);
ActionAnimationResult PlayDefenseAnimation(int slot,
                                           ActionAnimationWorld& world);
ActionAnimationResult SetEnemyCombatPose(int slot, int mode,
                                         ActionReactionWorld& world);
ActionAnimationResult PlaySpellCastAnimation(int slot, int phase,
                                             ActionAnimationWorld& world);
ActionAnimationResult PlayChainAbilityAnimation(int slot, int ability,
                                                int sequence,
                                                ActionAnimationWorld& world);
ActionAnimationResult PlayDefenseAbilityAnimation(int slot,
                                                  ActionReactionWorld& world);
ActionAnimationResult PlaySpecialActionAnimation(int slot, int mode,
                                                 ActionAnimationWorld& world);
ActionAnimationResult PlayBreakArtAnimation(int slot, int mode,
                                            ActionAnimationWorld& world);
ActionAnimationResult FinishEnemyCombatPose(int slot, int facing,
                                            ActionReactionWorld& world);
}  // namespace VS::Battle
