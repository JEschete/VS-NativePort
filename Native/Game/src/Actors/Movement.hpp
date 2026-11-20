#pragma once
// An actor's movement request and fall direction, and the battle program's
// jump, landing and locomotion animation tables and support state.
// Movement.cpp defines the globals; the module image fills the tables at
// start-up.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Gte.hpp"

namespace VS::Actors {

// decomp: D_800F4538_unk1848 (src/BATTLE/BATTLE.PRG/entity.h:106)
struct MovementRequest {
  /* 0x00 */ std::uint8_t mode;
  /* 0x01 */ std::uint8_t reserved1;
  /* 0x02 */ std::uint8_t reserved2;
  /* 0x03 */ std::uint8_t reserved3;
  /* 0x04 */ std::int16_t direction;
  /* 0x06 */ std::int16_t facing;
  /* 0x08 */ std::uint32_t distance;
  /* 0x0C */ std::int32_t reserved0c;
  /* 0x10 */ VS::Core::Gte::SVECTOR step;
};
namespace layout {
inline constexpr std::size_t kMovementRequestBytes = 0x18;
}  // namespace layout
static_assert(sizeof(MovementRequest) == layout::kMovementRequestBytes,
              "MovementRequest must keep its PS1 layout");

// decomp: func_800A3054_t (src/BATTLE/BATTLE.PRG/3A1A0.c:15)
struct FallDirection {
  std::int16_t direction_x;
  std::int16_t reserved2;
  std::int16_t direction_z;
};
namespace layout {
inline constexpr std::size_t kFallDirectionBytes = 0x6;
}  // namespace layout
static_assert(sizeof(FallDirection) == layout::kFallDirectionBytes,
              "FallDirection must keep its PS1 layout");

// The byte tables' sizes are the address gaps to the next symbol in the
// module image, not capacities.
inline constexpr std::size_t kJumpHangFramesBytes = 3204;
inline constexpr std::size_t kJumpHeightAdjustmentsBytes = 3196;
inline constexpr std::size_t kMotionResultAnimationsBytes = 3181;
inline constexpr std::size_t kSlowQuadrantMoveBytes = 3172;
inline constexpr std::size_t kFastQuadrantMoveBytes = 3168;
inline constexpr std::size_t kAirTimerEndAnimationsBytes = 2632;
inline constexpr std::size_t kSoftLandingAnimationsBytes = 2624;
inline constexpr std::size_t kHardLandingAnimationsBytes = 2616;
inline constexpr std::size_t kLandingRecoveryAnimationsBytes = 2608;
template <std::size_t Size>
using MovementTable = std::array<std::uint8_t, Size>;

// decomp: D_800E90AC (src/BATTLE/BATTLE.PRG/38C1C.c:27)
extern MovementTable<kJumpHangFramesBytes> jumpHangFrames;
// decomp: D_800E90B4 (src/BATTLE/BATTLE.PRG/38C1C.c:28)
extern MovementTable<kJumpHeightAdjustmentsBytes> jumpHeightAdjustments;
// decomp: D_800E90C3 (D_800E90C3@BATTLE/BATTLE.PRG)
extern MovementTable<kMotionResultAnimationsBytes> motionResultAnimations;
// decomp: D_800E90C4 (D_800E90C4@BATTLE/BATTLE.PRG)
extern std::uint8_t motionResultOneAnimationId;
// decomp: D_800E90C6 (D_800E90C6@BATTLE/BATTLE.PRG)
extern std::uint8_t hangMotionAnimationId;
// decomp: D_800E90CC (src/BATTLE/BATTLE.PRG/3A1A0.c:45)
extern MovementTable<kSlowQuadrantMoveBytes> slowQuadrantMoveAnimations;
// decomp: D_800E90D0 (src/BATTLE/BATTLE.PRG/3A1A0.c:46)
extern MovementTable<kFastQuadrantMoveBytes> fastQuadrantMoveAnimations;
// decomp: D_800E92E8 (D_800E92E8@BATTLE/BATTLE.PRG)
extern MovementTable<kAirTimerEndAnimationsBytes> airTimerEndAnimations;
// decomp: D_800E92F0 (D_800E92F0@BATTLE/BATTLE.PRG)
extern MovementTable<kSoftLandingAnimationsBytes> softLandingAnimations;
// decomp: D_800E92F8 (D_800E92F8@BATTLE/BATTLE.PRG)
extern MovementTable<kHardLandingAnimationsBytes> hardLandingAnimations;
// decomp: D_800E9300 (D_800E9300@BATTLE/BATTLE.PRG)
extern MovementTable<kLandingRecoveryAnimationsBytes> landingRecoveryAnimations;
// decomp: D_800F49F4 (src/BATTLE/BATTLE.PRG/3A1A0.h:62)
extern std::uint16_t supportingPlatformLayer;
// decomp: D_800F49F8 (src/BATTLE/BATTLE.PRG/3A1A0.h:63)
extern std::uint8_t supportingGimmickIndex;
// decomp: D_800F4B18 (src/BATTLE/BATTLE.PRG/3A1A0.h:65)
extern char isActorUpdateComplete;
// decomp: D_800F4B19 (src/BATTLE/BATTLE.PRG/44F14.c:61)
extern std::uint8_t pushedGimmickIndex;

}  // namespace VS::Actors
