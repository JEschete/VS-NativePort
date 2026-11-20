#pragma once
// The room's AI state and each actor's AI state block with the action
// candidates and slot metadata it embeds, in their PS1 layouts.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Core/Gte.hpp"

namespace VS::Battle {
struct Actor;
struct ActorStats;
}  // namespace VS::Battle

namespace VS::Battle::Ai {

struct ActorAiState;
struct EngagementData;
struct ModelStatePrefix;

inline constexpr std::size_t kRoomAiStateReserved4Bytes = 4;
inline constexpr std::size_t kRoomAiStateReservedBBytes = 9;
inline constexpr std::size_t kRoomAiStateSpecialActorsCount = 4;

// decomp: D_800F58BC_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:353)
struct RoomAiState {
  /* 0x00 */ std::uint8_t max_tile_x;
  /* 0x01 */ std::uint8_t max_tile_z;
  /* 0x02 */ std::uint8_t min_tile_x;
  /* 0x03 */ std::uint8_t min_tile_z;
  /* 0x04 */ std::array<std::uint8_t, kRoomAiStateReserved4Bytes> reserved4;
  /* 0x08 */ std::uint8_t default_action_delay;
  /* 0x09 */ union {
    std::uint8_t reserved9;
    struct {
      std::uint8_t reserved9_prefix;
      std::uint8_t allows_non_spell_actions;
      union {
        std::array<std::uint8_t, kRoomAiStateReservedBBytes> reserved_b;
        struct {
          std::array<std::uint8_t, 2> reserved_b_prefix;
          std::uint8_t engagement_limit;
          std::array<std::uint8_t, 2> reserved_e_prefix;
          std::array<std::uint8_t, 2> engaged_mask_bytes;
          std::uint8_t engaged_count;
          std::uint8_t reserved13;
        };
      };
    };
  };
  /* 0x14 */ std::int32_t flight_ceiling;
  /* 0x18 */ std::uint16_t readiness_level;
  /* 0x1A */ std::array<std::uint8_t, 2> reserved_1a;
  /* 0x1C */ ActorAiState* engaged_head;
  /* 0x20 */ ActorAiState* engaged_tail;
  /* 0x24 */ std::int32_t approach_moves;
  /* 0x28 */ std::array<VS::Battle::Actor*, kRoomAiStateSpecialActorsCount>
      special_actors;
};

// decomp: func_800DCAA0_t2 (src/BATTLE/BATTLE.PRG/6F014.c:96)
struct CandidateFlags {
  std::uint32_t opaque0_bit0 : 1;
  std::uint32_t opaque0_bit1 : 1;
  std::uint32_t opaque0_bit2 : 1;
  std::uint32_t opaque0_bit3 : 1;
  std::uint32_t opaque0_bit4 : 1;
  std::uint32_t opaque0_bit5 : 1;
  std::uint32_t opaque0_bit6 : 16;
  std::uint32_t reserved2_bit6 : 10;
};
inline constexpr std::size_t kCandidateFlagsBytes = 0x4;
static_assert(sizeof(CandidateFlags) == kCandidateFlagsBytes,
              "CandidateFlags must keep its PS1 layout");

// decomp: func_800DCAA0_t (src/BATTLE/BATTLE.PRG/6F014.c:112)
struct ActionCandidateRecord {
  std::int32_t opaque0;
  std::uint32_t row : 3;
  std::uint32_t column : 2;
  std::uint32_t action : 8;
  std::uint32_t threshold : 16;
  std::uint32_t reserved7_bit5 : 3;
  std::int32_t distance_squared;
  std::uint32_t target_id : 4;
  std::uint32_t target_limb : 3;
  std::uint32_t opaque_c_bit7 : 12;
  std::uint32_t weight : 8;
  std::uint32_t reserved_f_bit3 : 5;
  CandidateFlags opaque10;
};
inline constexpr std::size_t kActionCandidateRecordBytes = 0x14;
static_assert(sizeof(ActionCandidateRecord) == kActionCandidateRecordBytes,
              "ActionCandidateRecord must keep its PS1 layout");

// decomp: func_800DD604_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:106)
struct ActionSlotMetadataRecord {
  /* 0x00 */ std::uint32_t reserved0_bit0 : 1;
  /* 0x00 */ std::uint32_t opaque0_bit1 : 1;
  /* 0x00 */ std::uint32_t opaque0_bit2 : 1;
  /* 0x00 */ std::uint32_t kind : 2;
  /* 0x00 */ std::uint32_t long_range : 1;
  /* 0x00 */ std::uint32_t special : 1;
  /* 0x00 */ std::uint32_t opaque0_bit7 : 1;
  /* 0x01 */ std::uint32_t radius : 16;
  /* 0x03 */ std::uint32_t reserved3_bit0 : 8;
  /* 0x04 */ std::uint32_t range_squared;
};
inline constexpr std::size_t kActionSlotMetadataRecordBytes = 0x8;
static_assert(sizeof(ActionSlotMetadataRecord) ==
                  kActionSlotMetadataRecordBytes,
              "ActionSlotMetadataRecord must keep its PS1 layout");

inline constexpr std::size_t kActorAiStateReservedABytes = 3;
inline constexpr std::size_t kActorAiStateReserved10Bytes = 3;
inline constexpr std::size_t kActorAiStateReserved1dBytes = 3;
inline constexpr std::size_t kActorAiStateReserved22Bytes = 4;
inline constexpr std::size_t kActorAiStateReserved2dBytes = 3;
inline constexpr std::size_t kActorAiStateReserved31Bytes = 3;
inline constexpr std::size_t kActorAiStateReserved3cBytes = 8;
inline constexpr std::size_t kActorAiStateReserved45Bytes = 3;
inline constexpr std::size_t kActorAiStateReserved606cBytes = 22;
inline constexpr std::size_t kActorAiStateReserved84Bytes = 4;
inline constexpr std::size_t kActorAiStateReserved89Bytes = 3;
inline constexpr std::size_t kActorAiStateReserved94Bytes = 4;
inline constexpr std::size_t kActorAiStateReserved9cBytes = 23;
inline constexpr std::size_t kActorAiStateLastSlideTileBytes = 4;
inline constexpr std::size_t kActorAiStateReservedA0Bytes = 16;
inline constexpr std::size_t kActorAiStateReservedC8Bytes = 8;
inline constexpr std::size_t kActorAiStateReservedD9Bytes = 14;
inline constexpr std::size_t kActorAiStateReservedEaBytes = 16;
inline constexpr std::size_t kActorAiStateReservedFcBytes = 12;
inline constexpr std::size_t kActorAiStateReservedFcPrefixBytes = 4;
inline constexpr std::size_t kActorAiStateReserved105Bytes = 3;
inline constexpr std::size_t kActorAiStateReserved10dBytes = 3;
inline constexpr std::size_t kActorAiStateDetourDirectionsBytes = 4;
inline constexpr std::size_t kActorAiStateReserved120Bytes = 10;
inline constexpr std::size_t kActorAiStateReserved126Bytes = 4;
inline constexpr std::size_t kActorAiStateReserved154Bytes = 4;
inline constexpr std::size_t kActorAiStateReserved160Bytes = 8;
inline constexpr std::size_t kActorAiStateReserved16cBytes = 4;
inline constexpr std::size_t kActorAiStateReserved174Bytes = 8;
inline constexpr std::size_t kActorAiStateReserved194Bytes = 6;
inline constexpr std::size_t kActorAiStateDetourTileBytes = 4;
inline constexpr std::size_t kActorAiStateReserved19bBytes = 5;
inline constexpr std::size_t kActorAiStateRouteExitBytes = 4;
inline constexpr std::size_t kActorAiStateRelationsCount = 16;
inline constexpr std::size_t kActorAiStateDistancesSquaredCount = 16;
inline constexpr std::size_t kActorAiStateReserved224Bytes = 4;
inline constexpr std::size_t kActorAiStateActionSlotsRows = 6;
inline constexpr std::size_t kActorAiStateActionSlotsColumns = 4;
inline constexpr std::size_t kActorAiStateCandidatesCount = 16;
inline constexpr std::size_t kActorAiStateCandidateOrderCount = 16;
inline constexpr std::size_t kActorAiStateReserved474Bytes = 13;
inline constexpr std::size_t kActorAiStateMovementModeCountersBytes = 13;

// decomp: func_800E78F4_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:149)
struct ActorAiState {
  /* 0x00 */ std::uint8_t reserved0;
  /* 0x01 */ std::uint8_t opaque1;
  /* 0x02 */ std::uint8_t is_stuck;
  /* 0x03 */ std::uint8_t opaque3;
  /* 0x04 */ std::uint8_t opaque4;
  /* 0x05 */ std::uint8_t opaque5;
  /* 0x06 */ union {
    std::uint8_t reserved6;
    std::uint8_t ignore_floor_mask;
  };
  /* 0x07 */ std::uint8_t movement_disabled;
  /* 0x08 */ std::uint8_t opaque8;
  /* 0x09 */ std::uint8_t opaque9;
  /* 0x0A */ std::array<std::uint8_t, kActorAiStateReservedABytes> reserved_a;
  /* 0x0D */ std::uint8_t opaque_d;
  /* 0x0E */ std::uint8_t opaque_e;
  /* 0x0F */ std::uint8_t opaque_f;
  /* 0x10 */ union {
    std::array<std::uint8_t, kActorAiStateReserved10Bytes> reserved10;
    struct {
      std::uint8_t resource_status_restricted;
      std::uint8_t low_mp;
      std::uint8_t low_hp;
    };
  };
  /* 0x13 */ std::uint8_t uses_room_action_delay;
  /* 0x14 */ std::array<std::uint8_t, 2> reserved14;
  /* 0x16 */ std::int8_t linked_actor;
  /* 0x17 */ std::uint8_t action_cost_enabled;
  /* 0x18 */ std::uint8_t opaque18;
  /* 0x19 */ std::uint8_t settings_constant19;
  /* 0x1A */ std::uint8_t opaque_1a;
  /* 0x1B */ std::uint8_t is_dead;
  /* 0x1C */ std::uint8_t opaque_1c;
  /* 0x1D */ std::array<std::uint8_t, kActorAiStateReserved1dBytes> reserved_1d;
  /* 0x20 */ std::uint8_t settings_bit26;
  /* 0x21 */ std::uint8_t settings_byte3_bit0;
  /* 0x22 */ union {
    std::array<std::uint8_t, kActorAiStateReserved22Bytes> reserved22;
    struct {
      std::uint8_t reserved22_prefix;
      std::int8_t movement_enabled;
      std::array<std::uint8_t, 2> reserved24;
    };
  };
  /* 0x26 */ std::uint8_t allow_fallback_action;
  /* 0x27 */ std::uint8_t settings_bit27;
  /* 0x28 */ std::uint8_t reserved28;
  /* 0x29 */ std::uint8_t settings_bit14;
  /* 0x2A */ std::uint8_t has_tagged_bone3;
  /* 0x2B */ std::uint8_t settings_mode_low;
  /* 0x2C */ std::uint8_t settings_bits21;
  /* 0x2D */ std::array<std::uint8_t, kActorAiStateReserved2dBytes> reserved_2d;
  /* 0x30 */ std::uint8_t settings_bit25;
  /* 0x31 */ union {
    std::array<std::uint8_t, kActorAiStateReserved31Bytes> reserved31;
    struct {
      std::uint8_t reserved31_prefix;
      std::uint8_t terrain_reject_mask;
      std::uint8_t reserved33;
    };
  };
  /* 0x34 */ TilePosition tile;
  /* 0x38 */ std::int32_t previous_tile;
  /* 0x3C */ union {
    std::array<std::uint8_t, kActorAiStateReserved3cBytes> reserved_3c;
    struct {
      std::uint32_t remembered_target_tile;
      std::uint16_t target_memory_frames;
      std::int16_t saved_relation_options;
    };
  };
  /* 0x44 */ std::uint8_t opaque44;
  /* 0x45 */ std::array<std::uint8_t, kActorAiStateReserved45Bytes> reserved45;
  /* 0x48 */ TilePosition saved_destination;
  /* 0x4C */ VS::Core::Gte::SVECTOR position;
  /* 0x54 */ EngagementData* actor;
  /* 0x58 */ ModelStatePrefix* model;
  /* 0x5C */ VS::Battle::ActorStats* stats;
  void* native_pointer60;  // PS1 pointer slot 0x60
  void* native_pointer64;  // PS1 pointer slot 0x64
  void* native_pointer68;  // PS1 pointer slot 0x68
  std::array<std::uint8_t, kActorAiStateReserved606cBytes> reserved_60_6c;
  /* 0x82 */ std::int16_t point_height;
  /* 0x84 */ std::array<std::uint8_t, kActorAiStateReserved84Bytes> reserved84;
  /* 0x88 */ std::uint8_t actor_id;
  /* 0x89 */ union {
    std::array<std::uint8_t, kActorAiStateReserved89Bytes> reserved89;
    struct {
      std::uint8_t requested_movement_mode;
      std::uint8_t current_movement_mode;
      std::uint8_t reserved_8b;
    };
  };
  /* 0x8C */ std::uint16_t move_attempts;
  /* 0x8E */ std::uint16_t engagement_frames;
  /* 0x90 */ union {
    std::array<std::uint8_t, 2> reserved90;
    std::uint16_t action_delay;
  };
  /* 0x92 */ std::uint16_t radius;
  /* 0x94 */ union {
    std::array<std::uint8_t, kActorAiStateReserved94Bytes> reserved94;
    struct {
      std::uint16_t collision_radius;
      std::array<std::uint8_t, 2> reserved96;
    };
  };
  /* 0x98 */ std::uint16_t engagement_cooldown_frames;
  /* 0x9A */ std::uint16_t reengagement_grace_frames;
  /* 0x9C */ union {
    std::array<std::uint8_t, kActorAiStateReserved9cBytes> reserved_9c;
    struct {
      std::array<std::uint8_t, kActorAiStateLastSlideTileBytes>
          last_slide_tile_bytes;
      std::array<std::uint8_t, kActorAiStateReservedA0Bytes> reserved_a0;
      std::array<std::uint8_t, 2> step_height_bytes;
      std::uint8_t reserved_b2;
    };
  };
  /* 0xB3 */ std::uint8_t max_range;
  /* 0xB4 */ std::int32_t move_distance;
  /* 0xB8 */ std::int16_t body_height;
  /* 0xBA */ std::uint16_t facing_delay;
  /* 0xBC */ std::array<std::uint8_t, 2> reserved_bc;
  /* 0xBE */ std::int16_t flight_height;
  /* 0xC0 */ std::uint16_t wander_phase;
  /* 0xC2 */ std::uint16_t wander_timer;
  /* 0xC4 */ std::int32_t wander_range_squared;
  /* 0xC8 */ union {
    std::array<std::uint8_t, kActorAiStateReservedC8Bytes> reserved_c8;
    struct {
      std::int32_t wander_center_radius;
      TilePosition wander_center;
    };
  };
  /* 0xD0 */ std::int16_t wander_heading;
  /* 0xD2 */ std::uint16_t wander_cooldown;
  /* 0xD4 */ std::uint16_t rest_period;
  /* 0xD6 */ std::uint16_t move_period;
  /* 0xD8 */ std::uint8_t wander_rate;
  /* 0xD9 */ std::array<std::uint8_t, kActorAiStateReservedD9Bytes> reserved_d9;
  /* 0xE7 */ std::uint8_t opaque_e7;
  /* 0xE8 */ std::uint8_t opaque_e8;
  /* 0xE9 */ std::uint8_t opaque_e9;
  /* 0xEA */ std::array<std::uint8_t, kActorAiStateReservedEaBytes> reserved_ea;
  /* 0xFA */ std::uint8_t detour_step;
  /* 0xFB */ std::uint8_t opaque_fb;
  /* 0xFC */ union {
    std::array<std::uint8_t, kActorAiStateReservedFcBytes> reserved_fc;
    struct {
      std::array<std::uint8_t, kActorAiStateReservedFcPrefixBytes>
          reserved_fc_prefix;
      std::uint8_t patrol_index;
      std::uint8_t patrol_row;
      std::uint8_t patrol_rate;
      std::int8_t patrol_direction;
      std::uint8_t patrol_cooldown;
      std::array<std::uint8_t, kActorAiStateReserved105Bytes> reserved105;
    };
  };
  /* 0x108 */ TilePosition previous_facing_tile;
  /* 0x10C */ std::uint8_t facing_pending;
  /* 0x10D */ std::array<std::uint8_t, kActorAiStateReserved10dBytes>
      reserved_10d;
  TilePosition flee_tile;
  std::uint16_t flee_timer;
  std::array<std::uint8_t, 2> reserved116;
  /* 0x118 */ std::array<std::uint8_t, kActorAiStateDetourDirectionsBytes>
      detour_directions;
  /* 0x11C */ TilePosition goal;
  /* 0x120 */ union {
    std::array<std::uint8_t, kActorAiStateReserved120Bytes> reserved120;
    struct {
      std::uint8_t fine_x;
      std::uint8_t fine_layer;
      std::uint8_t fine_z;
      std::uint8_t fine_status;
      std::uint8_t reserved124;
      std::uint8_t locomotion_mode;
      std::array<std::uint8_t, kActorAiStateReserved126Bytes> reserved126;
    };
  };
  /* 0x12A */ std::uint8_t awareness_frames_remaining;
  /* 0x12B */ union {
    std::array<std::uint8_t, 2> reserved_12b;
    struct {
      std::uint8_t reserved_12b_prefix;
      std::uint8_t engagement_request;
    };
  };
  /* 0x12D */ std::uint8_t behavior_mode;
  /* 0x12E */ std::uint8_t plan_mode;
  /* 0x12F */ std::int8_t opaque_12f;
  /* 0x130 */ std::int8_t opaque130;
  /* 0x131 */ std::uint8_t action_status;
  /* 0x132 */ std::uint8_t ground_area;
  /* 0x133 */ std::uint8_t reserved133;
  /* 0x134 */ TilePosition planned_tile;
  /* 0x138 */ std::uint16_t awareness_marker;
  /* 0x13A */ std::uint16_t opaque_13a;
  /* 0x13C */ std::uint16_t opaque_13c;
  /* 0x13E */ std::uint8_t reserved_13e;
  /* 0x13F */ std::uint8_t min_action_delay;
  /* 0x140 */ ActionCandidateRecord current_choice;
  /* 0x154 */ union {
    std::array<std::uint8_t, kActorAiStateReserved154Bytes> reserved154;
    struct {
      std::uint8_t reserved154_area;
      std::uint8_t decision_delay;
      union {
        std::array<std::uint8_t, 2> reserved156;
        std::uint16_t movement_wait;
      };
    };
  };
  /* 0x158 */ std::int16_t target_x;
  /* 0x15A */ std::int16_t target_z;
  /* 0x15C */ std::uint8_t movement_target;
  /* 0x15D */ std::uint8_t opaque_15d;
  /* 0x15E */ std::uint16_t movement_flags;
  /* 0x160 */ union {
    std::array<std::uint8_t, kActorAiStateReserved160Bytes> reserved160;
    std::uint32_t scripted_target_xz;
  };
  /* 0x168 */ TilePosition destination;
  /* 0x16C */ std::array<std::uint8_t, kActorAiStateReserved16cBytes>
      reserved_16c;
  /* 0x170 */ TilePosition next_tile;
  /* 0x174 */ union {
    std::array<std::uint8_t, kActorAiStateReserved174Bytes> reserved174;
    VS::Core::Gte::SVECTOR next_position;
  };
  /* 0x17C */ std::int32_t move_x;
  /* 0x180 */ std::int32_t move_z;
  /* 0x184 */ std::int32_t move_y;
  /* 0x188 */ std::int16_t heading;
  /* 0x18A */ std::int16_t angle;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::uint32_t navigation_flags;
  union {
    std::array<std::uint8_t, kActorAiStateReserved194Bytes> reserved194;
    struct {
      std::array<std::uint8_t, kActorAiStateDetourTileBytes> detour_tile_bytes;
      std::uint8_t detour_repeats;
      std::uint8_t detour_run;
    };
  };
  /* 0x19A */ std::uint8_t opaque_19a;
  /* 0x19B */ union {
    std::array<std::uint8_t, kActorAiStateReserved19bBytes> reserved_19b;
    struct {
      std::uint8_t route_gate;
      std::array<std::uint8_t, kActorAiStateRouteExitBytes> route_exit_bytes;
    };
  };
  /* 0x1A0 */ ActorAiState* next_engaged;
  /* 0x1A4 */ std::array<RelationWord, kActorAiStateRelationsCount> relations;
  /* 0x1E4 */ std::array<std::uint32_t, kActorAiStateDistancesSquaredCount>
      distances_squared;
  /* 0x224 */ union {
    std::array<std::uint8_t, kActorAiStateReserved224Bytes> reserved224;
    struct {
      std::uint16_t locomotion_flags;
      std::uint8_t walk_step;
      std::uint8_t run_step;
    };
  };
  /* 0x228 */ std::int32_t stall_counter;
  /* 0x22C */ ActionCandidateRecord* current_candidate;
  /* 0x230 */ std::array<
      std::array<ActionSlotMetadataRecord, kActorAiStateActionSlotsColumns>,
      kActorAiStateActionSlotsRows>
      action_slots;
  /* 0x2F0 */ std::array<ActionCandidateRecord, kActorAiStateCandidatesCount>
      candidates;
  /* 0x430 */ std::array<ActionCandidateRecord*,
                         kActorAiStateCandidateOrderCount>
      candidate_order;
  /* 0x470 */ std::int32_t candidate_count;
  /* 0x474 */ union {
    std::array<std::uint8_t, kActorAiStateReserved474Bytes> reserved474;
    std::array<std::uint8_t, kActorAiStateMovementModeCountersBytes>
        movement_mode_counters;
  };
  /* 0x481 */ std::uint8_t reaction_pending;
};

inline constexpr std::size_t kEngagementDataReserved04Bytes = 4;
inline constexpr std::size_t kEngagementDataReserved18Bytes = 0x24;

// decomp: func_800DEB10_t2 (src/BATTLE/BATTLE.PRG/6F014.c:85)
struct EngagementData {
  void* native_pointer0;  // PS1 pointer slot 0x0
  std::array<std::uint8_t, kEngagementDataReserved04Bytes> reserved0_4;
  std::uint8_t opaque8;
  std::uint8_t opaque9;
  std::uint8_t reserved_a;
  std::uint8_t reserved_b_bit0 : 2;
  std::uint8_t opaque_b_bit2 : 2;
  std::uint8_t reserved_b_bit4 : 4;
  std::uint16_t opaque_c;
  std::uint16_t opaque_e;
  VS::Core::Gte::SVECTOR opaque10;
  std::array<std::uint8_t, kEngagementDataReserved18Bytes> reserved18;
  VS::Battle::ActorStats* opaque_3c;
};

// decomp: func_800E1238_t (src/BATTLE/BATTLE.PRG/6F014.c:139)
struct ModelStatePrefix {
  std::int32_t reserved0;
  std::int32_t reserved4;
  std::uint32_t reserved8_bit0 : 21;
  std::uint32_t opaque_a_bit5 : 1;
  std::uint32_t reserved_a_bit6 : 10;
};
inline constexpr std::size_t kModelStatePrefixBytes = 0xC;
static_assert(sizeof(ModelStatePrefix) == kModelStatePrefixBytes,
              "ModelStatePrefix must keep its PS1 layout");

}  // namespace VS::Battle::Ai
