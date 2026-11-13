#include "Battle/ActionAnimations.hpp"

#include <array>
#include <bit>

#include "Actors/Skeleton.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
using Core::Math::Vec3;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingNegate;
using Core::Math::WrapSigned16;
using Result = ActionAnimationResult;

constexpr int kLowPitch = -228;
constexpr int kHighPitch = 172;
constexpr int kSpecialMeleePitch = 769;
constexpr std::uint16_t kWideRadius = 128;
constexpr std::int16_t kNormalTurnFrames = 10;
constexpr std::int16_t kWideTurnFrames = 20;
constexpr std::int16_t kDefenseTurnFrames = 4;
constexpr int kMeleeBlendFrames = 6;
constexpr int kActionBlendFrames = 8;
constexpr int kWideDefenseBlendFrames = 4;
constexpr int kMeleeAnimation = 0x48;
constexpr int kSpecialMeleeAnimation = 0x4E;
constexpr int kDefenseAnimation = 0x4F;
constexpr std::uint16_t kSpecialMeleeModel = 40;
constexpr int kDrawnAnimationOffset = 100;
constexpr int kChainAnimationBase = 202;
constexpr int kChainAnimationsPerGroup = 6;
constexpr int kSpecialStartAnimation = 0xBF;
constexpr int kSpecialFallbackAnimation = 0xC1;
constexpr int kSpecialAnimation = 0xFF;
constexpr int kBreakArtFollowupAnimation = 0xFE;
constexpr std::uint8_t kDefenseBlockedMovement = 2;
constexpr std::uint8_t kDefensePreservedMovement = 3;

Vec3 Vector(Actors::MotionVector point) { return {point.x, point.y, point.z}; }

std::int32_t FacingDelta(ActionAnimationActor& actor,
                         ActionAnimationWorld& world) {
  const auto point = world.Target();
  if (!point) return 0;
  Actors::ActorMotion motion;
  motion.position = actor.Position();
  motion.facing = actor.Facing();
  return Actors::ActorFacingDeltaToPoint(motion, &point.value());
}

// The original square's low word is negated when signed-negative. A negative
// wrapped sum addresses outside the original root table; it is not sqrt(0).
std::int32_t AbsoluteSquare(std::int32_t delta) {
  const auto square = WrappingMultiply(delta, delta);
  return square < 0 ? WrappingNegate(square) : square;
}

ActionAnimationPitch Pitch(ActionAnimationActor& actor,
                           ActionAnimationWorld& world) {
  auto point = world.Target();
  if (!point) return {};
  const auto origin = actor.Position();
  const auto dx = static_cast<std::int32_t>(point->x) - origin.x;
  const auto dz = static_cast<std::int32_t>(point->z) - origin.z;
  if (WrappingAdd(AbsoluteSquare(dx), AbsoluteSquare(dz)) < 0) {
    return {0, Result::kNegativeSquaredDistance};
  }
  const auto height = world.RootBoneHeight(actor.Slot());
  if (!height) return {0, Result::kMissingRootBone};
  // Horizontal coordinates precede the bone callback; target Y follows it.
  const auto current_point = world.Target();
  if (!current_point) return {0, Result::kMissingTarget};
  point->y = current_point->y;
  return {Actors::PitchToPoint(Vector(origin), Vector(point.value()),
                               height.value())};
}

int PitchBand(std::int32_t pitch) {
  return pitch < kLowPitch ? 0 : pitch < kHighPitch ? 1 : 2;
}

Result Melee(int slot, ActionAnimationActor& actor, int parameter,
             bool allow_special, ActionAnimationWorld& world) {
  actor.SetFacingDelta(WrapSigned16(
      static_cast<std::uint32_t>(world.FacingDeltaToTarget(actor))));
  actor.SetTurnFrames(actor.Radius() > kWideRadius ? kWideTurnFrames
                                                   : kNormalTurnFrames);
  const auto pitch = world.PitchToTarget(actor);
  if (pitch.result != Result::kComplete) return pitch.result;
  int animation = kMeleeAnimation + PitchBand(pitch.value);
  if (allow_special && pitch.value >= kSpecialMeleePitch && parameter == 0 &&
      actor.ModelId() == kSpecialMeleeModel) {
    animation = kSpecialMeleeAnimation;
  }
  world.Play(slot, animation, kMeleeBlendFrames);
  world.ResetHead(actor);
  return Result::kComplete;
}
}  // namespace

// The typed F00821/F00822 math already owns the facing and pitch algorithms.
std::int32_t ActionAnimationWorld::FacingDeltaToTarget(
    ActionAnimationActor& actor) {
  return FacingDelta(actor, *this);
}
ActionAnimationPitch ActionAnimationWorld::PitchToTarget(
    ActionAnimationActor& actor) {
  return Pitch(actor, *this);
}

// F00817: func_8009DD00, src/BATTLE/BATTLE.PRG/30DB0.c:2331.
ActionAnimationResult PlayMeleeAttackAnimation(int slot, int parameter,
                                               ActionAnimationWorld& world) {
  auto* actor = world.Actor(slot);
  if (actor == nullptr) return Result::kComplete;
  return Melee(slot, *actor, parameter, true, world);
}

// F00818: func_8009DDF4, src/BATTLE/BATTLE.PRG/30DB0.c:2365.
ActionAnimationResult PlayDefenseAnimation(int slot,
                                           ActionAnimationWorld& world) {
  auto* actor = world.Actor(slot);
  if (actor == nullptr) return Result::kMissingActor;
  const auto movement = actor->MovementMode();
  if (actor->WeaponDrawn() &&
      (actor->Slot() == 0 ||
       (!actor->MovementBlocked() && movement != kDefenseBlockedMovement &&
        movement != kDefensePreservedMovement))) {
    int blend = kWideDefenseBlendFrames;
    if (actor->Radius() < kWideRadius) {
      actor->SetFacingDelta(WrapSigned16(
          static_cast<std::uint32_t>(world.FacingDeltaToTarget(*actor))));
      actor->SetTurnFrames(kDefenseTurnFrames);
      blend = kActionBlendFrames;
    }
    const auto pitch = world.PitchToTarget(*actor);
    if (pitch.result != Result::kComplete) return pitch.result;
    world.Play(slot, kDefenseAnimation + PitchBand(pitch.value), blend);
    world.ResetHead(*actor);
  }
  if (actor->Slot() == 0) {
    if (actor->MovementMode() != kDefensePreservedMovement) {
      actor->ClearMovementMode();
    }
    actor->ClearJumpCountdown();
  }
  return Result::kComplete;
}

// F00823: func_8009E2E0, src/BATTLE/BATTLE.PRG/30DB0.c:2554.
ActionAnimationResult PlaySpellCastAnimation(int slot, int phase,
                                             ActionAnimationWorld& world) {
  auto* actor = world.Actor(slot);
  if (actor == nullptr) return Result::kComplete;
  if (phase != 0 && phase != 1) return Result::kInvalidCastPhase;
  if (phase == 0) {
    actor->SetFacingDelta(
        WrapSigned16(static_cast<std::uint32_t>(FacingDelta(*actor, world))));
    actor->SetTurnFrames(actor->Radius() > kWideRadius ? kWideTurnFrames
                                                       : kNormalTurnFrames);
  }
  constexpr std::array<std::uint8_t, 2> kInitial{0x55, 0x58};
  constexpr std::array<std::uint8_t, 2> kFallback{0x57, 0x59};
  const auto index = static_cast<std::size_t>(phase);
  int animation =
      kInitial.at(index) + (actor->WeaponDrawn() ? kDrawnAnimationOffset : 0);
  if (!world.HasAnimation(*actor, animation)) {
    animation = kFallback.at(index) +
                (actor->WeaponDrawn() ? kDrawnAnimationOffset : 0);
  }
  world.Play(slot, animation, kMeleeBlendFrames);
  world.ResetHead(*actor);
  return Result::kComplete;
}

// F00831: func_8009EC9C, src/BATTLE/BATTLE.PRG/30DB0.c:2956.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): original order.
ActionAnimationResult PlayChainAbilityAnimation(int slot, int ability,
                                                int sequence,
                                                ActionAnimationWorld& world) {
  auto* actor = world.Actor(slot);
  if (actor == nullptr) return Result::kComplete;
  if (ability < 0 || ability >= kChainAnimationAbilities) {
    return Result::kInvalidChainAbility;
  }
  std::int32_t animation = kChainAnimationBase +
                           kChainAnimationsPerGroup * world.ChainGroup(ability);
  const auto pitch = Pitch(*actor, world);
  if (pitch.result != Result::kComplete) return pitch.result;
  animation += 2 * PitchBand(pitch.value);
  if (sequence == 1) world.SetChainSide(1);
  const auto side = world.ChainSide() ^ 1U;
  world.SetChainSide(side);
  animation = WrappingAdd(animation, std::bit_cast<std::int32_t>(side));
  actor->SetFacingDelta(
      WrapSigned16(static_cast<std::uint32_t>(FacingDelta(*actor, world))));
  actor->SetTurnFrames(kNormalTurnFrames);
  world.Play(slot, animation, kActionBlendFrames);
  return Result::kComplete;
}

// F00833: func_8009EFEC, src/BATTLE/BATTLE.PRG/30DB0.c:3087.
ActionAnimationResult PlaySpecialActionAnimation(int slot, int mode,
                                                 ActionAnimationWorld& world) {
  auto* actor = world.Actor(slot);
  if (mode != 0 && mode != 1) return Result::kComplete;
  if (actor == nullptr) return Result::kMissingActor;
  if (mode == 0) {
    actor->SetFacingDelta(
        WrapSigned16(static_cast<std::uint32_t>(FacingDelta(*actor, world))));
    actor->SetTurnFrames(actor->Radius() >= kWideRadius ? kWideTurnFrames
                                                        : kNormalTurnFrames);
    const int animation = world.HasAnimation(*actor, kSpecialStartAnimation)
                              ? kSpecialStartAnimation
                              : kSpecialFallbackAnimation;
    world.Play(slot, animation, kActionBlendFrames);
    world.ResetHead(*actor);
    return Result::kComplete;
  }
  if (!world.HasAnimation(*actor, kSpecialAnimation)) {
    auto* fallback = world.Actor(slot);
    if (fallback == nullptr) return Result::kComplete;
    return Melee(slot, *fallback, 0, false, world);
  }
  const auto pitch = Pitch(*actor, world);
  if (pitch.result != Result::kComplete) return pitch.result;
  world.SetSpecialPitch(static_cast<std::uint8_t>(PitchBand(pitch.value)));
  world.Play(slot, kSpecialAnimation, kActionBlendFrames);
  return Result::kComplete;
}

// F00834: func_8009F298, src/BATTLE/BATTLE.PRG/30DB0.c:3189. This executes
// the actual F00833 policy before its distinct follow-up, not a forwarding
// shim.
ActionAnimationResult PlayBreakArtAnimation(int slot, int mode,
                                            ActionAnimationWorld& world) {
  const auto result = PlaySpecialActionAnimation(slot, mode, world);
  if (result != Result::kComplete) return result;
  if (mode != 1) return Result::kComplete;
  auto* actor = world.Actor(slot);
  if (actor == nullptr) return Result::kMissingActor;
  if (world.HasAnimation(*actor, kBreakArtFollowupAnimation)) {
    world.Queue(slot, kBreakArtFollowupAnimation, kActionBlendFrames, 0);
  }
  return Result::kComplete;
}
}  // namespace VS::Battle
