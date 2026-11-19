#pragma once
// The snapshot of Ashley's stats kept while he is not loaded (between rooms
// and across saves).
#include <cstddef>
#include <cstdint>

namespace VS::State {
inline constexpr std::size_t kAshleyLimbTypes = 4;
inline constexpr std::size_t kAshleyLimbAffinities = 8;
inline constexpr std::size_t kAshleyStatusTimers = 8;
inline constexpr std::size_t kAshleyLimbs = 6;
inline constexpr std::size_t kAshleySnapshotReservedWords = 9;
inline constexpr std::size_t kAshleyLimbSnapshotBytes = 0x1C;
inline constexpr std::size_t kAshleyStatsSnapshotBytes = 0xDC;
inline constexpr std::size_t kAshleySnapshotBytes = 0x100;

// decomp: D_80060068_t2 (src/SLUS_010.40/main.h:209)
struct AshleyLimbSnapshot {
  /* 0x00 */ std::uint16_t hp;
  /* 0x02 */ std::uint16_t max_hp;
  /* 0x04 */ std::uint16_t types[kAshleyLimbTypes];
  /* 0x0C */ std::uint16_t affinities[kAshleyLimbAffinities];
};
static_assert(sizeof(AshleyLimbSnapshot) == kAshleyLimbSnapshotBytes);

// decomp: D_80060068_t3 (src/SLUS_010.40/main.h:231)
struct AshleyStatsSnapshot {
  /* 0x00 */ std::int32_t is_pending;
  /* 0x04 */ std::int16_t current_hp;
  /* 0x06 */ std::int16_t max_hp;
  /* 0x08 */ std::int16_t current_mp;
  /* 0x0A */ std::int16_t max_mp;
  /* 0x0C */ std::int16_t strength;
  /* 0x0E */ std::int16_t total_strength;
  /* 0x10 */ std::int16_t intelligence;
  /* 0x12 */ std::int16_t total_intelligence;
  /* 0x14 */ std::int16_t agility;
  /* 0x16 */ std::int16_t total_agility;
  /* 0x18 */ std::int16_t risk;
  /* 0x1A */ std::uint16_t action_recovery_timer;
  /* 0x1C */ std::int32_t statuses;
  /* 0x20 */ std::uint8_t status_timers[kAshleyStatusTimers];
  /* 0x28 */ AshleyLimbSnapshot limbs[kAshleyLimbs];
  /* 0xD0 */ std::int32_t weapon_drawn;
  /* 0xD4 */ std::int32_t camera_pitch;
  /* 0xD8 */ std::int32_t camera_distance;
};
static_assert(sizeof(AshleyStatsSnapshot) == kAshleyStatsSnapshotBytes);

// decomp: D_80060068_t (src/SLUS_010.40/main.h:236)
struct AshleySnapshot {
  /* 0x00 */ AshleyStatsSnapshot stats;
  /* 0xDC */ std::int32_t reserved_dc[kAshleySnapshotReservedWords];
};
static_assert(sizeof(AshleySnapshot) == kAshleySnapshotBytes);

// decomp: D_80060068 (D_80060068@SLUS_010.40)
extern AshleySnapshot ashleySnapshot;
}  // namespace VS::State
