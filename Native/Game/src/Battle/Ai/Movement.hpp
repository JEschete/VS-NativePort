#pragma once
// The arena orbit state of the orbiting boss movement, the movement random
// state saved across a plan and the room's patrol routes. Movement.cpp
// defines the globals.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Battle::Ai {

// decomp: D_800F5920_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:378)
struct ArenaOrbitState {
  /* 0x00 */ std::uint16_t heading;
  /* 0x02 */ std::uint8_t phase;
  /* 0x03 */ std::uint8_t distance_cached;
  /* 0x04 */ std::int32_t distance_squared;
  /* 0x08 */ std::array<std::int8_t, 2> reserved8;
  /* 0x0A */ std::uint8_t is_orbiting;
  /* 0x0B */ std::uint8_t path_step;
  /* 0x0C */ std::int16_t cooldown;
  /* 0x0E */ std::int16_t requested;
  /* 0x10 */ std::int16_t orbit_angle;
  /* 0x12 */ std::int16_t saved_height;
  /* 0x14 */ std::uint8_t health_phase;
  /* 0x15 */ std::int8_t reserved15;
  /* 0x16 */ std::uint8_t plan_saved;
  /* 0x17 */ std::uint8_t saved_plan_count;
  /* 0x18 */ std::int16_t start_x;
  /* 0x1A */ std::int16_t start_z;
  /* 0x1C */ std::uint8_t started;
};
inline constexpr std::size_t kArenaOrbitStateBytes = 0x20;
static_assert(sizeof(ArenaOrbitState) == kArenaOrbitStateBytes,
              "ArenaOrbitState must keep its PS1 layout");

// decomp: D_800F58C4 (src/BATTLE/BATTLE.PRG/6FC00.c:15)
extern std::int32_t savedMovementRandomState;
inline constexpr std::size_t kPatrolRouteStops = 8;
// decomp: D_800F58D8 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:526)
extern std::uint16_t (*patrolRoutes)[kPatrolRouteStops];

}  // namespace VS::Battle::Ai
