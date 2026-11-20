// The battle program's jump, landing and locomotion animation tables and the
// actors' support state (Movement.hpp).
#include "Actors/Movement.hpp"

namespace VS::Actors {

// Contents: module image (ModuleData.cpp) for every table down to
// landingRecoveryAnimations.
MovementTable<kJumpHangFramesBytes> jumpHangFrames;
MovementTable<kJumpHeightAdjustmentsBytes> jumpHeightAdjustments;
MovementTable<kMotionResultAnimationsBytes> motionResultAnimations;
std::uint8_t motionResultOneAnimationId;
std::uint8_t hangMotionAnimationId;
MovementTable<kSlowQuadrantMoveBytes> slowQuadrantMoveAnimations;
MovementTable<kFastQuadrantMoveBytes> fastQuadrantMoveAnimations;
MovementTable<kAirTimerEndAnimationsBytes> airTimerEndAnimations;
MovementTable<kSoftLandingAnimationsBytes> softLandingAnimations;
MovementTable<kHardLandingAnimationsBytes> hardLandingAnimations;
MovementTable<kLandingRecoveryAnimationsBytes> landingRecoveryAnimations;

std::uint16_t supportingPlatformLayer;
std::uint8_t supportingGimmickIndex;
char isActorUpdateComplete;
std::uint8_t pushedGimmickIndex;

}  // namespace VS::Actors
