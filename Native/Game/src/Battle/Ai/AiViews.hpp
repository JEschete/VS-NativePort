#pragma once
// The views each AI routine takes of an actor's AI state block, and the
// model, stats and tile words those views reach, in their PS1 layouts.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/Ai/ActionRecords.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Core/Gte.hpp"

namespace VS::Actors {
struct ActorRecord;
}
namespace VS::Battle {
struct Actor;
struct ActorStats;
}  // namespace VS::Battle

namespace VS::Battle::Ai {

struct ApproachActorView;
struct ApproachTargetRecordView;
struct CreationModelView;
struct CreationStatsView;
struct EscortEntryView;
struct ModelActiveView;
struct OrbitStatsView;
struct PerceivedObjectView;
struct PositionLinkView;
struct SettingsActorView;
struct TerrainMoveActorView;
struct WanderRecordView;

inline constexpr std::size_t kRelationByteViewReserved1Bytes = 3;

// decomp: actionCandidateTarget (src/BATTLE/BATTLE.PRG/6FC00.c:503)
struct RelationByteView {
  std::uint8_t opaque0;
  std::array<std::uint8_t, kRelationByteViewReserved1Bytes> reserved1;
};
inline constexpr std::size_t kRelationByteViewBytes = 0x4;
static_assert(sizeof(RelationByteView) == kRelationByteViewBytes,
              "RelationByteView must keep its PS1 layout");

inline constexpr std::size_t kCandidateReadinessViewReserved00Bytes = 84;
inline constexpr std::size_t kCandidateReadinessViewReserved06cBytes = 197;
inline constexpr std::size_t kCandidateReadinessViewReserved132132Bytes = 90;
inline constexpr std::size_t kCandidateReadinessViewReserved132190Bytes = 16;
inline constexpr std::size_t kCandidateReadinessViewRelationsCount = 16;
inline constexpr std::size_t kCandidateReadinessViewDistancesSquaredCount = 16;
inline constexpr std::size_t kCandidateReadinessViewReserved224224Bytes = 8;
inline constexpr std::size_t kCandidateReadinessViewReserved224230Bytes = 512;
inline constexpr std::size_t kCandidateReadinessViewCandidateOrderCount = 16;

// decomp: actionCandidateState (src/BATTLE/BATTLE.PRG/6FC00.c:513)
struct CandidateReadinessView {
  std::array<std::uint8_t, kCandidateReadinessViewReserved00Bytes> reserved0_0;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kCandidateReadinessViewReserved06cBytes>
      reserved_0_6c;
  std::uint8_t action_status;
  std::array<std::uint8_t, kCandidateReadinessViewReserved132132Bytes>
      reserved132_132;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kCandidateReadinessViewReserved132190Bytes>
      reserved132_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<RelationByteView, kCandidateReadinessViewRelationsCount> relations;
  std::array<std::uint32_t, kCandidateReadinessViewDistancesSquaredCount>
      distances_squared;
  std::array<std::uint8_t, kCandidateReadinessViewReserved224224Bytes>
      reserved224_224;
  void* native_pointer_22c;  // PS1 pointer slot 0x22C
  std::array<std::uint8_t, kCandidateReadinessViewReserved224230Bytes>
      reserved224_230;
  std::array<CandidateEntryView*, kCandidateReadinessViewCandidateOrderCount>
      candidate_order;
  std::int32_t candidate_count;
};

// decomp: actionDispatchEntry (src/BATTLE/BATTLE.PRG/71618.c:1040)
struct DispatchEntryView {
  std::uint32_t reserved0;
  std::uint32_t reserved4;
  std::uint32_t reserved8;
  std::uint32_t reserved_c;
};
inline constexpr std::size_t kDispatchEntryViewBytes = 0x10;
static_assert(sizeof(DispatchEntryView) == kDispatchEntryViewBytes,
              "DispatchEntryView must keep its PS1 layout");

inline constexpr std::size_t kDecisionDispatchViewReserved10Bytes = 4;
inline constexpr std::size_t kDecisionDispatchViewReserved15Bytes = 8;
inline constexpr std::size_t kDecisionDispatchViewReserved1eBytes = 0x16;
inline constexpr std::size_t kDecisionDispatchViewReserved37Bytes = 0x11;
inline constexpr std::size_t kDecisionDispatchViewPositionCount = 3;
inline constexpr std::size_t kDecisionDispatchViewReserved526cBytes = 22;
inline constexpr std::size_t kDecisionDispatchViewReserved84Bytes = 0x34;
inline constexpr std::size_t kDecisionDispatchViewReservedBaBytes = 0x78;
inline constexpr std::size_t kDecisionDispatchViewReserved133Bytes = 0xD;
inline constexpr std::size_t kDecisionDispatchViewReserved150Bytes = 4;
inline constexpr std::size_t kDecisionDispatchViewReserved155Bytes = 7;
inline constexpr std::size_t kDecisionDispatchViewReserved15d15dBytes = 47;
inline constexpr std::size_t kDecisionDispatchViewReserved15d190Bytes = 16;
inline constexpr std::size_t kDecisionDispatchViewReserved15d1a4Bytes = 64;
inline constexpr std::size_t kDecisionDispatchViewReserved1e81e8Bytes = 68;
inline constexpr std::size_t kDecisionDispatchViewReserved1e8230Bytes = 512;
inline constexpr std::size_t kDecisionDispatchViewCandidateOrderCount = 16;

// decomp: actionDispatchState (src/BATTLE/BATTLE.PRG/71618.c:1085)
struct DecisionDispatchView {
  std::uint32_t opaque0;
  std::uint32_t opaque4;
  std::uint32_t opaque8;
  std::uint16_t reserved_c;
  std::uint16_t reserved_e;
  std::array<std::uint8_t, kDecisionDispatchViewReserved10Bytes> reserved10;
  std::uint8_t opaque14;
  std::array<std::uint8_t, kDecisionDispatchViewReserved15Bytes> reserved15;
  std::uint8_t opaque_1d;
  std::array<std::uint8_t, kDecisionDispatchViewReserved1eBytes> reserved_1e;
  std::uint8_t tile_x;
  std::uint8_t reserved35;
  std::uint8_t tile_z;
  std::array<std::uint8_t, kDecisionDispatchViewReserved37Bytes> reserved37;
  std::int32_t saved_destination;
  std::array<std::int16_t, kDecisionDispatchViewPositionCount> position;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kDecisionDispatchViewReserved526cBytes>
      reserved_52_6c;
  std::int16_t point_height;
  std::array<std::uint8_t, kDecisionDispatchViewReserved84Bytes> reserved84;
  std::int16_t body_height;
  std::array<std::uint8_t, kDecisionDispatchViewReservedBaBytes> reserved_ba;
  std::int8_t opaque132;
  std::array<std::uint8_t, kDecisionDispatchViewReserved133Bytes> reserved133;
  DispatchEntryView fallback;
  std::array<std::uint8_t, kDecisionDispatchViewReserved150Bytes> reserved150;
  std::int8_t opaque154;
  std::array<std::uint8_t, kDecisionDispatchViewReserved155Bytes> reserved155;
  std::uint8_t movement_target;
  std::array<std::uint8_t, kDecisionDispatchViewReserved15d15dBytes>
      reserved_15d_15d;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kDecisionDispatchViewReserved15d190Bytes>
      reserved_15d_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<std::uint8_t, kDecisionDispatchViewReserved15d1a4Bytes>
      reserved_15d_1a4;
  std::uint32_t distance;
  std::array<std::uint8_t, kDecisionDispatchViewReserved1e81e8Bytes>
      reserved_1e8_1e8;
  void* native_pointer_22c;  // PS1 pointer slot 0x22C
  std::array<std::uint8_t, kDecisionDispatchViewReserved1e8230Bytes>
      reserved_1e8_230;
  std::array<DispatchEntryView*, kDecisionDispatchViewCandidateOrderCount>
      candidate_order;
  std::int32_t candidate_count;
};

inline constexpr std::size_t kActionPlanViewReserved0Bytes = 0x26;
inline constexpr std::size_t kActionPlanViewReserved27Bytes = 0x2D;
inline constexpr std::size_t kActionPlanViewReserved606cBytes = 28;
inline constexpr std::size_t kActionPlanViewReserved89Bytes = 0xA5;
inline constexpr std::size_t kActionPlanViewReserved12fBytes = 0x10;
inline constexpr std::size_t kActionPlanViewReserved140140Bytes = 76;
inline constexpr std::size_t kActionPlanViewReserved140190Bytes = 16;
inline constexpr std::size_t kActionPlanViewReserved1401a4Bytes = 136;
inline constexpr std::size_t kActionPlanViewReserved140230Bytes = 192;
inline constexpr std::size_t kActionPlanViewCandidatesCount = 16;
inline constexpr std::size_t kActionPlanViewCandidateOrderCount = 16;

// decomp: actionPlanState (src/BATTLE/BATTLE.PRG/74088.c:1284)
struct ActionPlanView {
  std::array<std::uint8_t, kActionPlanViewReserved0Bytes> reserved0;
  std::uint8_t allow_fallback_action;
  std::array<std::uint8_t, kActionPlanViewReserved27Bytes> reserved27;
  VS::Battle::Actor* actor;
  void* native_pointer58;  // PS1 pointer slot 0x58
  VS::Battle::ActorStats* stats;
  void* native_pointer60;  // PS1 pointer slot 0x60
  void* native_pointer64;  // PS1 pointer slot 0x64
  void* native_pointer68;  // PS1 pointer slot 0x68
  std::array<std::uint8_t, kActionPlanViewReserved606cBytes> reserved_60_6c;
  std::uint8_t actor_id;
  std::array<std::uint8_t, kActionPlanViewReserved89Bytes> reserved89;
  std::uint8_t plan_mode;
  std::array<std::uint8_t, kActionPlanViewReserved12fBytes> reserved_12f;
  std::uint8_t min_action_delay;
  std::array<std::uint8_t, kActionPlanViewReserved140140Bytes> reserved140_140;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kActionPlanViewReserved140190Bytes> reserved140_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<std::uint8_t, kActionPlanViewReserved1401a4Bytes> reserved_140_1a4;
  void* native_pointer_22c;  // PS1 pointer slot 0x22C
  std::array<std::uint8_t, kActionPlanViewReserved140230Bytes> reserved140_230;
  std::array<CandidateEntryView, kActionPlanViewCandidatesCount> candidates;
  std::array<CandidateEntryView*, kActionPlanViewCandidateOrderCount>
      candidate_order;
  std::int32_t candidate_count;
};

inline constexpr std::size_t kRelationWordViewReserved1Bytes = 3;

// decomp: actionReactionTarget (src/BATTLE/BATTLE.PRG/74088.c:685)
union RelationWordView {
  std::uint32_t word;
  struct {
    std::uint32_t reserved0_bits : 6;
    std::uint32_t classification : 2;
    std::uint32_t reserved8_bits : 7;
    std::uint32_t timer : 5;
    std::uint32_t counter : 7;
    std::uint32_t reserved27_bit : 1;
    std::uint32_t previous_classification : 2;
    std::uint32_t reserved30_bits : 2;
  } reaction_bits;
  struct {
    std::uint8_t first_byte;
    std::array<std::uint8_t, kRelationWordViewReserved1Bytes> reserved1;
  } bytes;
};

inline constexpr std::size_t kReactionViewReserved0Bytes = 0x13;
inline constexpr std::size_t kReactionViewReserved14Bytes = 12;
inline constexpr std::size_t kReactionViewReserved22Bytes = 5;
inline constexpr std::size_t kReactionViewReserved28Bytes = 4;
inline constexpr std::size_t kReactionViewReserved2dBytes = 3;
inline constexpr std::size_t kReactionViewReserved3131Bytes = 35;
inline constexpr std::size_t kReactionViewReserved316cBytes = 28;
inline constexpr std::size_t kReactionViewReserved89Bytes = 0xA4;
inline constexpr std::size_t kReactionViewReserved89PrefixBytes = 0x9D;
inline constexpr std::size_t kReactionViewReserved127Bytes = 6;
inline constexpr std::size_t kReactionViewReserved12e12eBytes = 94;
inline constexpr std::size_t kReactionViewReserved12e190Bytes = 16;
inline constexpr std::size_t kReactionViewRelationsCount = 16;

// decomp: actionReactionState (src/BATTLE/BATTLE.PRG/74088.c:703)
struct ReactionView {
  std::array<std::uint8_t, kReactionViewReserved0Bytes> reserved0;
  union {
    std::uint8_t uses_room_action_delay;
    std::uint8_t temporary_ashley_alliance;
  };
  std::array<std::uint8_t, kReactionViewReserved14Bytes> reserved14;
  std::uint8_t hostile_on_grievance;
  std::uint8_t shares_ally_hostility;
  std::array<std::uint8_t, kReactionViewReserved22Bytes> reserved22;
  std::uint8_t can_ally_with_ashley;
  std::array<std::uint8_t, kReactionViewReserved28Bytes> reserved28;
  std::uint8_t relation_category;
  std::array<std::uint8_t, kReactionViewReserved2dBytes> reserved_2d;
  std::uint8_t hostile_on_harm;
  std::array<std::uint8_t, kReactionViewReserved3131Bytes> reserved31_31;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kReactionViewReserved316cBytes> reserved_31_6c;
  std::uint8_t actor_id;
  union {
    std::array<std::uint8_t, kReactionViewReserved89Bytes> reserved89;
    struct {
      std::array<std::uint8_t, kReactionViewReserved89PrefixBytes>
          reserved89_prefix;
      std::uint8_t alliance_reaction_count;
      std::array<std::uint8_t, kReactionViewReserved127Bytes> reserved127;
    };
  };
  std::uint8_t behavior_mode;
  std::array<std::uint8_t, kReactionViewReserved12e12eBytes> reserved_12e_12e;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kReactionViewReserved12e190Bytes> reserved_12e_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<RelationWordView, kReactionViewRelationsCount> relations;
};

inline constexpr std::size_t kPlanEvaluationViewReserved0Bytes = 19;
inline constexpr std::size_t kPlanEvaluationViewReserved14Bytes = 7;
inline constexpr std::size_t kPlanEvaluationViewReserved1c1cBytes = 56;
inline constexpr std::size_t kPlanEvaluationViewReserved1c6cBytes = 28;
inline constexpr std::size_t kPlanEvaluationViewReserved89Bytes = 17;
inline constexpr std::size_t kPlanEvaluationViewReserved9cBytes = 75;
inline constexpr std::size_t kPlanEvaluationViewReservedE9E9Bytes = 163;
inline constexpr std::size_t kPlanEvaluationViewReservedE9190Bytes = 10;

// decomp: actorEvaluationState (src/BATTLE/BATTLE.PRG/74088.c:1538)
struct PlanEvaluationView {
  std::array<std::uint8_t, kPlanEvaluationViewReserved0Bytes> reserved0;
  std::uint8_t uses_room_action_delay;
  std::array<std::uint8_t, kPlanEvaluationViewReserved14Bytes> reserved14;
  std::uint8_t is_dead;
  std::array<std::uint8_t, kPlanEvaluationViewReserved1c1cBytes> reserved_1c_1c;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kPlanEvaluationViewReserved1c6cBytes> reserved_1c_6c;
  std::uint8_t actor_id;
  std::array<std::uint8_t, kPlanEvaluationViewReserved89Bytes> reserved89;
  std::uint16_t reengagement_grace_frames;
  std::array<std::uint8_t, kPlanEvaluationViewReserved9cBytes> reserved_9c;
  std::uint8_t opaque_e7;
  std::uint8_t opaque_e8;
  std::array<std::uint8_t, kPlanEvaluationViewReservedE9E9Bytes> reserved_e9_e9;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kPlanEvaluationViewReservedE9190Bytes>
      reserved_e9_190;
  std::uint8_t opaque_19a;
};

// decomp: actorInitializationFlags (src/BATTLE/BATTLE.PRG/7CD68.c:827)
union ActorStateFlags {
  std::uint32_t word;
  struct {
    std::uint8_t byte0;
    std::uint8_t byte1;
    std::uint8_t byte2;
    std::uint8_t byte3;
  } bytes;
  struct {
    std::uint32_t low_bits : 24;
    std::uint32_t bits24 : 2;
    std::uint32_t bits26 : 2;
    std::uint32_t high_bits : 4;
  } bits;
};

inline constexpr std::size_t kAiActorViewReservedCBytes = 16;
inline constexpr std::size_t kAiActorViewReserved20Bytes = 16;

// decomp: actorInitializationActor (src/BATTLE/BATTLE.PRG/7CD68.c:869)
struct AiActorView {
  void* native_pointer0;  // PS1 pointer slot 0x0
  std::int32_t actor_id;
  ActorStateFlags state_flags;
  std::array<std::uint8_t, kAiActorViewReservedCBytes> reserved_c;
  std::uint16_t ai_control_flags;
  std::array<std::uint8_t, 1> reserved_1e;
  std::uint8_t start_command_flag;
  std::array<std::uint8_t, kAiActorViewReserved20Bytes> reserved20;
  std::int32_t saved_ai_settings;
  std::uint32_t saved_ai_settings_flags;
  std::int32_t ai_settings;
  CreationStatsView* stats;
  std::int32_t list_exempt_flag;
  CreationModelView* model;
};

inline constexpr std::size_t kAiCreationViewReserved0Bytes = 42;
inline constexpr std::size_t kAiCreationViewReserved2bBytes = 9;
inline constexpr std::size_t kAiCreationViewReserved3cBytes = 6;
inline constexpr std::size_t kAiCreationViewReserved44Bytes = 16;
inline constexpr std::size_t kAiCreationViewReserved686cBytes = 22;
inline constexpr std::size_t kAiCreationViewReserved84Bytes = 4;
inline constexpr std::size_t kAiCreationViewReserved8bBytes = 7;
inline constexpr std::size_t kAiCreationViewReserved96Bytes = 6;
inline constexpr std::size_t kAiCreationViewReservedA0Bytes = 12;
inline constexpr std::size_t kAiCreationViewReservedB0Bytes = 3;
inline constexpr std::size_t kAiCreationViewReservedC0Bytes = 67;
inline constexpr std::size_t kAiCreationViewReserved104Bytes = 12;
inline constexpr std::size_t kAiCreationViewReserved114Bytes = 16;
inline constexpr std::size_t kAiCreationViewReserved125Bytes = 8;
inline constexpr std::size_t kAiCreationViewReserved12eBytes = 14;
inline constexpr std::size_t kAiCreationViewReserved13eBytes = 32;
inline constexpr std::size_t kAiCreationViewReserved160Bytes = 16;
inline constexpr std::size_t kAiCreationViewReserved174Bytes = 16;
inline constexpr std::size_t kAiCreationViewReserved188188Bytes = 4;
inline constexpr std::size_t kAiCreationViewReserved188190Bytes = 4;
inline constexpr std::size_t kAiCreationViewReserved198198Bytes = 8;
inline constexpr std::size_t kAiCreationViewReserved1981a4Bytes = 128;

// decomp: actorInitializationState (src/BATTLE/BATTLE.PRG/7CD68.c:927)
struct AiCreationView {
  std::array<std::uint8_t, kAiCreationViewReserved0Bytes> reserved0;
  std::uint8_t has_tagged_bone3;
  std::array<std::uint8_t, kAiCreationViewReserved2bBytes> reserved_2b;
  std::int32_t tile;
  std::int32_t previous_tile;
  std::array<std::uint8_t, kAiCreationViewReserved3cBytes> reserved_3c;
  std::int16_t options;
  std::array<std::uint8_t, kAiCreationViewReserved44Bytes> reserved44;
  AiActorView* actor;
  CreationModelView* model;
  CreationStatsView* stats;
  std::int16_t* position_source;
  std::uint32_t* tile_source;
  void* native_pointer68;  // PS1 pointer slot 0x68
  std::array<std::uint8_t, kAiCreationViewReserved686cBytes> reserved_68_6c;
  std::uint16_t point_height;
  std::array<std::uint8_t, kAiCreationViewReserved84Bytes> reserved84;
  std::uint8_t actor_id;
  std::uint8_t opaque89;
  std::uint8_t opaque_8a;
  std::array<std::uint8_t, kAiCreationViewReserved8bBytes> reserved_8b;
  std::uint16_t radius;
  std::uint16_t collision_radius;
  std::array<std::uint8_t, kAiCreationViewReserved96Bytes> reserved96;
  std::int32_t opaque_9c;
  std::array<std::uint8_t, kAiCreationViewReservedA0Bytes> reserved_a0;
  std::uint32_t far_distance_squared;
  std::array<std::uint8_t, kAiCreationViewReservedB0Bytes> reserved_b0;
  std::uint8_t max_range;
  std::int32_t move_distance;
  std::uint16_t body_height;
  std::array<std::uint8_t, 2> reserved_ba;
  std::int16_t ground_clearance;
  std::int16_t flight_height;
  std::array<std::uint8_t, kAiCreationViewReservedC0Bytes> reserved_c0;
  std::uint8_t opaque103;
  std::array<std::uint8_t, kAiCreationViewReserved104Bytes> reserved104;
  std::int32_t opaque110;
  std::array<std::uint8_t, kAiCreationViewReserved114Bytes> reserved114;
  std::uint8_t opaque124;
  std::array<std::uint8_t, kAiCreationViewReserved125Bytes> reserved125;
  std::uint8_t behavior_mode;
  std::array<std::uint8_t, kAiCreationViewReserved12eBytes> reserved_12e;
  std::uint16_t step_height;
  std::array<std::uint8_t, kAiCreationViewReserved13eBytes> reserved_13e;
  std::uint16_t movement_flags;
  std::array<std::uint8_t, kAiCreationViewReserved160Bytes> reserved160;
  std::int32_t next_tile;
  std::array<std::uint8_t, kAiCreationViewReserved174Bytes> reserved174;
  std::int32_t move_y;
  std::array<std::uint8_t, kAiCreationViewReserved188188Bytes> reserved188_188;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kAiCreationViewReserved188190Bytes> reserved188_190;
  std::int32_t opaque194;
  std::array<std::uint8_t, kAiCreationViewReserved198198Bytes> reserved198_198;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<std::uint8_t, kAiCreationViewReserved1981a4Bytes> reserved_198_1a4;
  std::uint8_t opaque224;
  std::uint8_t opaque225;
  std::uint8_t opaque226;
  std::uint8_t opaque227;
};

// decomp: actorRelation (src/BATTLE/BATTLE.PRG/6FC00.c:608)
union TargetRelationWord {
  std::uint32_t word;
  struct {
    std::uint32_t active : 1;
    std::uint32_t was_active : 1;
    std::uint32_t hidden : 1;
    std::uint32_t in_view : 1;
    std::uint32_t reserved4 : 1;
    std::uint32_t visible : 1;
    std::uint32_t classification : 2;
    std::uint32_t counter : 7;
    std::uint32_t timer : 5;
    std::uint32_t classification_timer : 7;
    std::uint32_t blocked : 1;
    std::uint32_t previous_classification : 2;
    std::uint32_t out_of_reach : 1;
    std::uint32_t on_marked_tile : 1;
  } bits;
};

inline constexpr std::size_t kPerceptionViewReserved0Bytes = 0x22;
inline constexpr std::size_t kPerceptionViewReserved26Bytes = 0xE;
inline constexpr std::size_t kPerceptionViewReserved38Bytes = 0x14;
inline constexpr std::size_t kPerceptionViewReserved5c6cBytes = 22;
inline constexpr std::size_t kPerceptionViewReserved84Bytes = 4;
inline constexpr std::size_t kPerceptionViewReserved89Bytes = 9;
inline constexpr std::size_t kPerceptionViewReserved94Bytes = 0x10;
inline constexpr std::size_t kPerceptionViewReservedB0Bytes = 8;
inline constexpr std::size_t kPerceptionViewReservedBaBytes = 0x71;
inline constexpr std::size_t kPerceptionViewReserved12c12cBytes = 96;
inline constexpr std::size_t kPerceptionViewReserved12c190Bytes = 16;
inline constexpr std::size_t kPerceptionViewRelationsCount = 16;
inline constexpr std::size_t kPerceptionViewDistancesSquaredCount = 16;

// decomp: actorRelationState (src/BATTLE/BATTLE.PRG/6FC00.c:638)
struct PerceptionView {
  std::array<std::uint8_t, kPerceptionViewReserved0Bytes> reserved0;
  std::uint8_t decays_hostility;
  std::array<std::uint8_t, 2> reserved23;
  std::uint8_t ignores_body_line;
  std::array<std::uint8_t, kPerceptionViewReserved26Bytes> reserved26;
  TilePosition tile;
  std::array<std::uint8_t, kPerceptionViewReserved38Bytes> reserved38;
  VS::Core::Gte::SVECTOR position;
  void* native_pointer54;  // PS1 pointer slot 0x54
  PerceivedObjectView* model;
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kPerceptionViewReserved5c6cBytes> reserved_5c_6c;
  std::int16_t point_height;
  std::array<std::uint8_t, kPerceptionViewReserved84Bytes> reserved84;
  std::uint8_t actor_id;
  std::array<std::uint8_t, kPerceptionViewReserved89Bytes> reserved89;
  std::uint16_t radius;
  std::array<std::uint8_t, kPerceptionViewReserved94Bytes> reserved94;
  std::uint32_t near_distance_squared;
  std::uint32_t view_cosine;
  std::uint32_t far_distance_squared;
  std::array<std::uint8_t, kPerceptionViewReservedB0Bytes> reserved_b0;
  std::int16_t body_height;
  std::array<std::uint8_t, kPerceptionViewReservedBaBytes> reserved_ba;
  std::uint8_t phase;
  std::array<std::uint8_t, kPerceptionViewReserved12c12cBytes> reserved_12c_12c;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kPerceptionViewReserved12c190Bytes> reserved_12c_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<TargetRelationWord, kPerceptionViewRelationsCount> relations;
  std::array<std::uint32_t, kPerceptionViewDistancesSquaredCount>
      distances_squared;
};

inline constexpr std::size_t kSettingsViewReserved0Bytes = 22;
inline constexpr std::size_t kSettingsViewReserved1aBytes = 6;
inline constexpr std::size_t kSettingsViewReserved34Bytes = 32;
inline constexpr std::size_t kSettingsViewReserved6cBytes = 40;
inline constexpr std::size_t kSettingsViewReserved96Bytes = 14;
inline constexpr std::size_t kSettingsViewReservedB3Bytes = 7;
inline constexpr std::size_t kSettingsViewReservedBcBytes = 12;
inline constexpr std::size_t kSettingsViewReservedCfBytes = 5;
inline constexpr std::size_t kSettingsViewReservedD9Bytes = 40;
inline constexpr std::size_t kSettingsViewReserved103Bytes = 37;
inline constexpr std::size_t kSettingsViewReserved12aBytes = 52;

// decomp: actorSettingsState (src/BATTLE/BATTLE.PRG/7CD68.c:673)
struct SettingsView {
  std::array<std::uint8_t, kSettingsViewReserved0Bytes> reserved0;
  std::uint8_t linked_actor;
  std::array<std::uint8_t, 2> reserved17;
  std::uint8_t settings_constant19;
  std::array<std::uint8_t, kSettingsViewReserved1aBytes> reserved_1a;
  std::uint8_t settings_bit26;
  std::uint8_t settings_byte3_bit0;
  std::uint8_t settings_bit23;
  std::uint8_t settings_constant23;
  std::uint8_t settings_bit13;
  std::uint8_t settings_bit15;
  std::uint8_t allow_fallback_action;
  std::uint8_t settings_bit27;
  std::uint8_t reserved28;
  std::uint8_t settings_bit14;
  std::uint8_t has_tagged_bone3;
  std::uint8_t settings_mode_low;
  std::uint8_t settings_bits21;
  std::uint8_t opaque_2d;
  std::uint8_t reserved_2e;
  std::uint8_t settings_bits28;
  std::uint8_t settings_bit25;
  std::uint8_t settings_bits30;
  std::uint8_t settings_bits30_value;
  std::uint8_t settings_byte0_bit7;
  std::array<std::uint8_t, kSettingsViewReserved34Bytes> reserved34;
  SettingsActorView* actor;
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  std::int32_t (*decision_callback)(ActorAiState*);
  union {
    std::array<std::uint8_t, kSettingsViewReserved6cBytes> reserved_6c;
    std::int32_t (*saved_decision_callback)(ActorAiState*);
  };
  std::uint16_t collision_radius;
  std::array<std::uint8_t, kSettingsViewReserved96Bytes> reserved96;
  std::uint32_t near_distance_squared;
  std::int32_t view_cosine;
  std::uint32_t far_distance_squared;
  std::uint16_t opaque_b0;
  std::uint8_t opaque_b2;
  std::array<std::uint8_t, kSettingsViewReservedB3Bytes> reserved_b3;
  std::uint16_t facing_delay;
  std::array<std::uint8_t, kSettingsViewReservedBcBytes> reserved_bc;
  std::uint32_t opaque_c8;
  std::uint8_t opaque_cc;
  std::array<std::uint8_t, 1> reserved_cd;
  std::uint8_t opaque_ce;
  std::array<std::uint8_t, kSettingsViewReservedCfBytes> reserved_cf;
  std::uint16_t rest_period;
  std::uint16_t move_period;
  std::uint8_t wander_rate;
  std::array<std::uint8_t, kSettingsViewReservedD9Bytes> reserved_d9;
  std::uint8_t opaque101;
  std::uint8_t opaque102;
  std::array<std::uint8_t, kSettingsViewReserved103Bytes> reserved103;
  std::uint16_t angle;
  std::array<std::uint8_t, kSettingsViewReserved12aBytes> reserved_12a;
  std::uint16_t movement_flags;
};

inline constexpr std::size_t kPerceptionUpdateViewReserved0Bytes = 8;
inline constexpr std::size_t kPerceptionUpdateViewReserved9Bytes = 5;
inline constexpr std::size_t kPerceptionUpdateViewReservedFBytes = 5;
inline constexpr std::size_t kPerceptionUpdateViewReserved15Bytes = 3;
inline constexpr std::size_t kPerceptionUpdateViewReserved19Bytes = 3;
inline constexpr std::size_t kPerceptionUpdateViewReserved1eBytes = 0x11;
inline constexpr std::size_t kPerceptionUpdateViewReserved30Bytes = 4;
inline constexpr std::size_t kPerceptionUpdateViewBytes = 4;
inline constexpr std::size_t kPerceptionUpdateViewReserved42Bytes = 3;
inline constexpr std::size_t kPerceptionUpdateViewReserved48Bytes = 4;
inline constexpr std::size_t kPerceptionUpdateViewReserved686cBytes = 4;
inline constexpr std::size_t kPerceptionUpdateViewSamplesBytes = 8;
inline constexpr std::size_t kPerceptionUpdateViewSamplesBytes2 = 8;
inline constexpr std::size_t kPerceptionUpdateViewReserved82Bytes = 0xE;
inline constexpr std::size_t kPerceptionUpdateViewReserved92Bytes = 0x68;
inline constexpr std::size_t kPerceptionUpdateViewReservedFcBytes = 0x18;
inline constexpr std::size_t kPerceptionUpdateViewReserved116Bytes = 0xA;
inline constexpr std::size_t kPerceptionUpdateViewReserved125Bytes = 5;
inline constexpr std::size_t kPerceptionUpdateViewReserved12bBytes = 6;
inline constexpr std::size_t kPerceptionUpdateViewReserved132Bytes = 6;
inline constexpr std::size_t kPerceptionUpdateViewReserved13aBytes = 0x1C;
inline constexpr std::size_t kPerceptionUpdateViewReserved158Bytes = 0x34;
inline constexpr std::size_t kPerceptionUpdateViewReserved194Bytes = 6;
inline constexpr std::size_t kPerceptionUpdateViewReserved19b19bBytes = 5;
inline constexpr std::size_t kPerceptionUpdateViewRelationsCount = 16;

// decomp: actorUpdateState (src/BATTLE/BATTLE.PRG/6FC00.c:958)
struct PerceptionUpdateView {
  std::array<std::uint8_t, kPerceptionUpdateViewReserved0Bytes> reserved0;
  std::uint8_t opaque8;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved9Bytes> reserved9;
  std::uint8_t opaque_e;
  std::array<std::uint8_t, kPerceptionUpdateViewReservedFBytes> reserved_f;
  std::uint8_t opaque14;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved15Bytes> reserved15;
  std::uint8_t alerted;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved19Bytes> reserved19;
  std::uint8_t holds_stall_tile;
  std::uint8_t opaque_1d;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved1eBytes> reserved_1e;
  std::uint8_t alert_rule;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved30Bytes> reserved30;
  std::uint32_t tile;
  union {
    std::int32_t word;
    std::array<std::uint8_t, kPerceptionUpdateViewBytes> bytes;
  } previous_tile;
  std::uint32_t remembered_target_tile;
  std::uint16_t target_memory_frames;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved42Bytes> reserved42;
  std::uint8_t detour_step_frames;
  std::uint16_t stall_frames;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved48Bytes> reserved48;
  std::int32_t position_xy;
  std::int32_t position_z_pad;
  VS::Battle::Actor* actor;
  PerceivedObjectView* model;
  PositionLinkView* stats;
  PositionLinkView* position_source;
  PositionLinkView* tile_source;
  void* native_pointer68;  // PS1 pointer slot 0x68
  std::array<std::uint8_t, kPerceptionUpdateViewReserved686cBytes>
      reserved_68_6c;
  union {
    std::array<std::uint8_t, kPerceptionUpdateViewSamplesBytes> samples;
    std::array<std::int32_t, 2> words;
  } history_x;
  union {
    std::array<std::uint8_t, kPerceptionUpdateViewSamplesBytes2> samples;
    std::array<std::int32_t, 2> words;
  } history_z;
  std::uint8_t history_index;
  std::uint8_t history_mask;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved82Bytes> reserved82;
  std::uint16_t status_flags;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved92Bytes> reserved92;
  std::uint8_t detour_step;
  std::uint8_t opaque_fb;
  std::array<std::uint8_t, kPerceptionUpdateViewReservedFcBytes> reserved_fc;
  std::uint16_t flee_timer;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved116Bytes> reserved116;
  std::uint32_t opaque120;
  std::uint8_t observed_command;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved125Bytes> reserved125;
  std::uint8_t awareness_frames_remaining;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved12bBytes> reserved_12b;
  std::uint8_t action_status;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved132Bytes> reserved132;
  std::uint16_t awareness_marker;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved13aBytes> reserved_13a;
  std::uint16_t movement_wait;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved158Bytes> reserved158;
  void* scripted_route;
  std::uint32_t reserved190 : 5;
  std::uint32_t detour_steps_left : 7;
  std::uint32_t reserved191 : 20;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved194Bytes> reserved194;
  std::uint8_t recovery_frames;
  std::array<std::uint8_t, kPerceptionUpdateViewReserved19b19bBytes>
      reserved_19b_19b;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<std::uint32_t, kPerceptionUpdateViewRelationsCount> relations;
};

inline constexpr std::size_t kCandidateTileWordByteCount = 4;

// decomp: candidateMovementWord (src/BATTLE/BATTLE.PRG/71618.c:479)
union CandidateTileWord {
  std::uint32_t word;
  std::array<std::uint8_t, kCandidateTileWordByteCount> bytes;
};
inline constexpr std::size_t kCandidateTileWordBytes = 0x4;
static_assert(sizeof(CandidateTileWord) == kCandidateTileWordBytes,
              "CandidateTileWord must keep its PS1 layout");

inline constexpr std::size_t kCandidateMovementViewReserved2Bytes = 74;
inline constexpr std::size_t kCandidateMovementViewPositionCount = 3;
inline constexpr std::size_t kCandidateMovementViewReserved526cBytes = 22;
inline constexpr std::size_t kCandidateMovementViewReserved84Bytes = 47;
inline constexpr std::size_t kCandidateMovementViewReservedB4Bytes = 122;
inline constexpr std::size_t kCandidateMovementViewReserved12fBytes = 5;
inline constexpr std::size_t kCandidateMovementViewReserved138Bytes = 36;
inline constexpr std::size_t kCandidateMovementViewReserved160Bytes = 8;
inline constexpr std::size_t kCandidateMovementViewReserved16c16cBytes = 32;
inline constexpr std::size_t kCandidateMovementViewReserved16c190Bytes = 16;
inline constexpr std::size_t kCandidateMovementViewRelationsCount = 16;
inline constexpr std::size_t kCandidateMovementViewDistancesSquaredCount = 16;
inline constexpr std::size_t kCandidateMovementViewReserved224224Bytes = 8;
inline constexpr std::size_t kCandidateMovementViewReserved224230Bytes = 512;

// decomp: candidateMovementState (src/BATTLE/BATTLE.PRG/71618.c:504)
struct CandidateMovementView {
  std::array<std::uint8_t, 1> reserved0;
  std::uint8_t opaque1;
  std::array<std::uint8_t, kCandidateMovementViewReserved2Bytes> reserved2;
  std::array<std::int16_t, kCandidateMovementViewPositionCount> position;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kCandidateMovementViewReserved526cBytes>
      reserved_52_6c;
  std::int16_t point_height;
  std::array<std::uint8_t, kCandidateMovementViewReserved84Bytes> reserved84;
  std::uint8_t max_range;
  std::array<std::uint8_t, kCandidateMovementViewReservedB4Bytes> reserved_b4;
  std::uint8_t plan_mode;
  std::array<std::uint8_t, kCandidateMovementViewReserved12fBytes> reserved_12f;
  CandidateTileWord planned_tile;
  std::array<std::uint8_t, kCandidateMovementViewReserved138Bytes> reserved138;
  std::uint8_t movement_target;
  std::array<std::uint8_t, 1> reserved_15d;
  std::uint16_t movement_flags;
  std::array<std::uint8_t, kCandidateMovementViewReserved160Bytes> reserved160;
  CandidateTileWord destination;
  std::array<std::uint8_t, kCandidateMovementViewReserved16c16cBytes>
      reserved_16c_16c;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kCandidateMovementViewReserved16c190Bytes>
      reserved_16c_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<CandidateTileWord, kCandidateMovementViewRelationsCount> relations;
  std::array<std::uint32_t, kCandidateMovementViewDistancesSquaredCount>
      distances_squared;
  std::array<std::uint8_t, kCandidateMovementViewReserved224224Bytes>
      reserved224_224;
  void* native_pointer_22c;  // PS1 pointer slot 0x22C
  std::array<std::uint8_t, kCandidateMovementViewReserved224230Bytes>
      reserved224_230;
  void* native_pointer430;   // PS1 pointer slot 0x430
  void* native_pointer434;   // PS1 pointer slot 0x434
  void* native_pointer438;   // PS1 pointer slot 0x438
  void* native_pointer_43c;  // PS1 pointer slot 0x43C
  void* native_pointer440;   // PS1 pointer slot 0x440
  void* native_pointer444;   // PS1 pointer slot 0x444
  void* native_pointer448;   // PS1 pointer slot 0x448
  void* native_pointer_44c;  // PS1 pointer slot 0x44C
  void* native_pointer450;   // PS1 pointer slot 0x450
  void* native_pointer454;   // PS1 pointer slot 0x454
  void* native_pointer458;   // PS1 pointer slot 0x458
  void* native_pointer_45c;  // PS1 pointer slot 0x45C
  void* native_pointer460;   // PS1 pointer slot 0x460
  void* native_pointer464;   // PS1 pointer slot 0x464
  void* native_pointer468;   // PS1 pointer slot 0x468
  void* native_pointer_46c;  // PS1 pointer slot 0x46C
  std::int32_t candidate_count;
};

inline constexpr std::size_t kEscortViewReserved0Bytes = 14;
inline constexpr std::size_t kEscortViewReserved10Bytes = 6;
inline constexpr std::size_t kEscortViewReserved19Bytes = 27;
inline constexpr std::size_t kEscortViewReserved38Bytes = 12;
inline constexpr std::size_t kEscortViewReserved45Bytes = 7;
inline constexpr std::size_t kEscortViewReserved526cBytes = 28;
inline constexpr std::size_t kEscortViewReserved89Bytes = 42;
inline constexpr std::size_t kEscortViewReservedB4Bytes = 53;
inline constexpr std::size_t kEscortViewReservedEaBytes = 68;
inline constexpr std::size_t kEscortViewReserved132Bytes = 42;
inline constexpr std::size_t kEscortViewReserved15d15dBytes = 47;
inline constexpr std::size_t kEscortViewReserved15d190Bytes = 16;
inline constexpr std::size_t kEscortViewRelationsCount = 16;
inline constexpr std::size_t kEscortViewDistancesSquaredCount = 16;
inline constexpr std::size_t kEscortViewReserved224224Bytes = 8;
inline constexpr std::size_t kEscortViewReserved224230Bytes = 512;
inline constexpr std::size_t kEscortViewCandidateOrderCount = 16;

// decomp: da4bcState (src/BATTLE/BATTLE.PRG/71618.c:747)
struct EscortView {
  std::array<std::uint8_t, kEscortViewReserved0Bytes> reserved0;
  std::uint8_t opaque_e;
  std::uint8_t opaque_f;
  std::array<std::uint8_t, kEscortViewReserved10Bytes> reserved10;
  std::int8_t linked_actor;
  std::uint8_t reserved17;
  std::uint8_t opaque18;
  std::array<std::uint8_t, kEscortViewReserved19Bytes> reserved19;
  std::uint32_t tile;
  std::array<std::uint8_t, kEscortViewReserved38Bytes> reserved38;
  std::uint8_t opaque44;
  std::array<std::uint8_t, kEscortViewReserved45Bytes> reserved45;
  std::int16_t x;
  std::array<std::uint8_t, 2> reserved_4e;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kEscortViewReserved526cBytes> reserved_52_6c;
  std::uint8_t actor_id;
  std::array<std::uint8_t, kEscortViewReserved89Bytes> reserved89;
  std::uint8_t max_range;
  std::array<std::uint8_t, kEscortViewReservedB4Bytes> reserved_b4;
  std::uint8_t opaque_e9;
  std::array<std::uint8_t, kEscortViewReservedEaBytes> reserved_ea;
  std::uint8_t plan_mode;
  std::array<std::uint8_t, 2> reserved_12f;
  std::uint8_t action_status;
  std::array<std::uint8_t, kEscortViewReserved132Bytes> reserved132;
  std::uint8_t movement_target;
  std::array<std::uint8_t, kEscortViewReserved15d15dBytes> reserved_15d_15d;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kEscortViewReserved15d190Bytes> reserved_15d_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<std::uint32_t, kEscortViewRelationsCount> relations;
  std::array<std::uint32_t, kEscortViewDistancesSquaredCount> distances_squared;
  std::array<std::uint8_t, kEscortViewReserved224224Bytes> reserved224_224;
  void* native_pointer_22c;  // PS1 pointer slot 0x22C
  std::array<std::uint8_t, kEscortViewReserved224230Bytes> reserved224_230;
  std::array<EscortEntryView*, kEscortViewCandidateOrderCount> candidate_order;
};

inline constexpr std::size_t kMoveTargetViewReserved0Bytes = 7;
inline constexpr std::size_t kMoveTargetViewReserved8Bytes = 0x2C;
inline constexpr std::size_t kMoveTargetViewReserved3838Bytes = 28;
inline constexpr std::size_t kMoveTargetViewReserved386cBytes = 252;

// decomp: func_800D8400_t (src/BATTLE/BATTLE.PRG/6FC00.c:426)
struct MoveTargetView {
  std::array<std::uint8_t, kMoveTargetViewReserved0Bytes> reserved0;
  std::uint8_t movement_disabled;
  std::array<std::uint8_t, kMoveTargetViewReserved8Bytes> reserved8;
  TilePosition tile;
  std::array<std::uint8_t, kMoveTargetViewReserved3838Bytes> reserved38_38;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kMoveTargetViewReserved386cBytes> reserved_38_6c;
  TilePosition destination;
};

inline constexpr std::size_t kMovementDecisionViewReserved0Bytes = 0x4C;
inline constexpr std::size_t kMovementDecisionViewPadding5c6cBytes = 28;
inline constexpr std::size_t kMovementDecisionViewReserved89Bytes = 0x31;
inline constexpr std::size_t kMovementDecisionViewPaddingBcBytes = 0x4C;
inline constexpr std::size_t kMovementDecisionViewPadding10dBytes = 0x2D;
inline constexpr std::size_t kMovementDecisionViewPadding13cBytes = 0x2C;
inline constexpr std::size_t kMovementDecisionViewPadding16cBytes = 0x10;
inline constexpr std::size_t kMovementDecisionViewReserved184Bytes = 4;
inline constexpr std::size_t kMovementDecisionViewPadding18c190Bytes = 16;
inline constexpr std::size_t kMovementDecisionViewTargetFlagsCount = 17;
inline constexpr std::size_t kMovementDecisionViewPadding1e81e8Bytes = 68;
inline constexpr std::size_t kMovementDecisionViewPadding1e8230Bytes = 512;
inline constexpr std::size_t kMovementDecisionViewPadding1e8470Bytes = 17;

// decomp: movementDecisionContext (src/BATTLE/BATTLE.PRG/771A8.c:2182)
struct MovementDecisionView {
  std::array<std::uint8_t, kMovementDecisionViewReserved0Bytes> reserved0;
  std::int16_t x;
  std::int16_t padding_4e;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;  // PS1 pointer slot 0x54
  VS::Actors::ActorRecord* model;
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kMovementDecisionViewPadding5c6cBytes> padding_5c_6c;
  std::uint8_t actor_id;
  std::array<std::uint8_t, kMovementDecisionViewReserved89Bytes> reserved89;
  std::uint16_t facing_delay;
  std::array<std::uint8_t, kMovementDecisionViewPaddingBcBytes> padding_bc;
  std::int32_t previous_facing_tile;
  std::uint8_t facing_pending;
  std::array<std::uint8_t, kMovementDecisionViewPadding10dBytes> padding_10d;
  std::uint16_t opaque_13a;
  std::array<std::uint8_t, kMovementDecisionViewPadding13cBytes> padding_13c;
  std::int32_t destination;
  std::array<std::uint8_t, kMovementDecisionViewPadding16cBytes> padding_16c;
  std::int32_t move_x;
  std::int32_t move_z;
  std::array<std::uint8_t, kMovementDecisionViewReserved184Bytes> reserved184;
  std::uint32_t reserved188 : 16;
  std::int32_t angle : 16;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kMovementDecisionViewPadding18c190Bytes>
      padding_18c_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<std::uint32_t, kMovementDecisionViewTargetFlagsCount> target_flags;
  std::array<std::uint8_t, kMovementDecisionViewPadding1e81e8Bytes>
      padding_1e8_1e8;
  void* native_pointer_22c;  // PS1 pointer slot 0x22C
  std::array<std::uint8_t, kMovementDecisionViewPadding1e8230Bytes>
      padding_1e8_230;
  void* native_pointer430;   // PS1 pointer slot 0x430
  void* native_pointer434;   // PS1 pointer slot 0x434
  void* native_pointer438;   // PS1 pointer slot 0x438
  void* native_pointer_43c;  // PS1 pointer slot 0x43C
  void* native_pointer440;   // PS1 pointer slot 0x440
  void* native_pointer444;   // PS1 pointer slot 0x444
  void* native_pointer448;   // PS1 pointer slot 0x448
  void* native_pointer_44c;  // PS1 pointer slot 0x44C
  void* native_pointer450;   // PS1 pointer slot 0x450
  void* native_pointer454;   // PS1 pointer slot 0x454
  void* native_pointer458;   // PS1 pointer slot 0x458
  void* native_pointer_45c;  // PS1 pointer slot 0x45C
  void* native_pointer460;   // PS1 pointer slot 0x460
  void* native_pointer464;   // PS1 pointer slot 0x464
  void* native_pointer468;   // PS1 pointer slot 0x468
  void* native_pointer_46c;  // PS1 pointer slot 0x46C
  std::array<std::uint8_t, kMovementDecisionViewPadding1e8470Bytes>
      padding_1e8_470;
  std::uint8_t reaction_pending;
};

// decomp: movementPath (src/BATTLE/BATTLE.PRG/771A8.c:2351)
union DetourWord {
  std::uint32_t word;
  struct {
    std::uint32_t reserved0 : 1;
    std::uint32_t heading : 3;
    std::uint32_t clockwise : 1;
    std::uint32_t steps : 7;
    std::uint32_t visited : 4;
    std::uint32_t stop : 1;
    std::uint32_t changed : 1;
    std::uint32_t cell : 4;
    std::uint32_t bound_x : 5;
    std::uint32_t bound_z : 5;
  } bits;
};

inline constexpr std::size_t kMovementPathViewReserved6Bytes = 0x2E;
inline constexpr std::size_t kMovementPathViewReserved38Bytes = 0x14;
inline constexpr std::size_t kMovementPathViewReserved526cBytes = 120;
inline constexpr std::size_t kMovementPathViewReservedE9Bytes = 0x75;
inline constexpr std::size_t kMovementPathViewReserved160Bytes = 8;
inline constexpr std::size_t kMovementPathViewReserved16cBytes = 4;
inline constexpr std::size_t kMovementPathViewReserved174174Bytes = 24;
inline constexpr std::size_t kMovementPathViewReserved194Bytes = 5;

// decomp: movementPathState (src/BATTLE/BATTLE.PRG/771A8.c:2381)
struct MovementPathView {
  std::uint8_t reserved0;
  std::uint8_t opaque1;
  std::array<std::uint8_t, 2> reserved2;
  std::uint8_t opaque4;
  std::uint8_t opaque5;
  std::array<std::uint8_t, kMovementPathViewReserved6Bytes> reserved6;
  TilePosition tile;
  std::array<std::uint8_t, kMovementPathViewReserved38Bytes> reserved38;
  std::int16_t x;
  std::array<std::uint8_t, 2> reserved_4e;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kMovementPathViewReserved526cBytes> reserved_52_6c;
  std::uint8_t opaque_e4;
  std::array<std::uint8_t, 2> reserved_e5;
  std::uint8_t opaque_e7;
  std::uint8_t opaque_e8;
  std::array<std::uint8_t, kMovementPathViewReservedE9Bytes> reserved_e9;
  std::uint16_t movement_flags;
  std::array<std::uint8_t, kMovementPathViewReserved160Bytes> reserved160;
  TilePosition destination;
  std::array<std::uint8_t, kMovementPathViewReserved16cBytes> reserved_16c;
  TilePosition next_tile;
  std::array<std::uint8_t, kMovementPathViewReserved174174Bytes>
      reserved174_174;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  DetourWord path;
  std::array<std::uint8_t, kMovementPathViewReserved194Bytes> reserved194;
  std::uint8_t opaque199;
  std::uint8_t wait_timer;
};

inline constexpr std::size_t kWaypointSearchViewReserved0Bytes = 0x4C;

// decomp: nearestWaypointState (src/BATTLE/BATTLE.PRG/74088.c:941)
struct WaypointSearchView {
  std::array<std::uint8_t, kWaypointSearchViewReserved0Bytes> reserved0;
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
};

inline constexpr std::size_t kPatrolViewReserved0Bytes = 0x34;
inline constexpr std::size_t kPatrolViewReserved3838Bytes = 28;
inline constexpr std::size_t kPatrolViewReserved386cBytes = 148;
inline constexpr std::size_t kPatrolViewReserved105Bytes = 0x63;

// decomp: patrolWaypointState (src/BATTLE/BATTLE.PRG/771A8.c:1854)
struct PatrolView {
  std::array<std::uint8_t, kPatrolViewReserved0Bytes> reserved0;
  TilePosition tile;
  std::array<std::uint8_t, kPatrolViewReserved3838Bytes> reserved38_38;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kPatrolViewReserved386cBytes> reserved_38_6c;
  std::uint8_t index;
  std::uint8_t row;
  std::uint8_t rate;
  std::uint8_t direction;
  std::uint8_t opaque104;
  std::array<std::uint8_t, kPatrolViewReserved105Bytes> reserved105;
  TilePosition destination;
};

inline constexpr std::size_t kRadialRetreatViewReserved0Bytes = 0x4C;
inline constexpr std::size_t kRadialRetreatViewReserved5c6cBytes = 272;
inline constexpr std::size_t kRadialRetreatViewReserved188188Bytes = 4;
inline constexpr std::size_t kRadialRetreatViewReserved188190Bytes = 16;
inline constexpr std::size_t kRadialRetreatViewReserved1881a4Bytes = 136;
inline constexpr std::size_t kRadialRetreatViewReserved188230Bytes = 512;

// decomp: radialMotionState (src/BATTLE/BATTLE.PRG/7EE98.c:480)
struct RadialRetreatView {
  std::array<std::uint8_t, kRadialRetreatViewReserved0Bytes> reserved0;
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;  // PS1 pointer slot 0x54
  ModelActiveView* model;
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kRadialRetreatViewReserved5c6cBytes> reserved_5c_6c;
  std::int32_t move_x;
  std::int32_t move_z;
  std::int32_t move_y;
  std::array<std::uint8_t, kRadialRetreatViewReserved188188Bytes>
      reserved188_188;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kRadialRetreatViewReserved188190Bytes>
      reserved188_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<std::uint8_t, kRadialRetreatViewReserved1881a4Bytes>
      reserved_188_1a4;
  void* native_pointer_22c;  // PS1 pointer slot 0x22C
  std::array<std::uint8_t, kRadialRetreatViewReserved188230Bytes>
      reserved188_230;
  void* native_pointer430;   // PS1 pointer slot 0x430
  void* native_pointer434;   // PS1 pointer slot 0x434
  void* native_pointer438;   // PS1 pointer slot 0x438
  void* native_pointer_43c;  // PS1 pointer slot 0x43C
  void* native_pointer440;   // PS1 pointer slot 0x440
  void* native_pointer444;   // PS1 pointer slot 0x444
  void* native_pointer448;   // PS1 pointer slot 0x448
  void* native_pointer_44c;  // PS1 pointer slot 0x44C
  void* native_pointer450;   // PS1 pointer slot 0x450
  void* native_pointer454;   // PS1 pointer slot 0x454
  void* native_pointer458;   // PS1 pointer slot 0x458
  void* native_pointer_45c;  // PS1 pointer slot 0x45C
  void* native_pointer460;   // PS1 pointer slot 0x460
  void* native_pointer464;   // PS1 pointer slot 0x464
  void* native_pointer468;   // PS1 pointer slot 0x468
  void* native_pointer_46c;  // PS1 pointer slot 0x46C
  std::int32_t candidate_count;
};

// decomp: func_800DFA54_t (src/BATTLE/BATTLE.PRG/6F014.c:49)
struct RouteGateTile {
  std::uint8_t opaque0;
  std::uint8_t opaque1;
  std::uint8_t opaque2;
};
inline constexpr std::size_t kRouteGateTileBytes = 0x3;
static_assert(sizeof(RouteGateTile) == kRouteGateTileBytes,
              "RouteGateTile must keep its PS1 layout");

inline constexpr std::size_t kAreaRouteViewReserved0Bytes = 5;
inline constexpr std::size_t kAreaRouteViewReserved6Bytes = 0x2E;
inline constexpr std::size_t kAreaRouteViewReserved38Bytes = 0x14;
inline constexpr std::size_t kAreaRouteViewReserved526cBytes = 28;
inline constexpr std::size_t kAreaRouteViewReserved89Bytes = 0xB1;
inline constexpr std::size_t kAreaRouteViewReserved13eBytes = 0x16;
inline constexpr std::size_t kAreaRouteViewReserved155Bytes = 7;
inline constexpr std::size_t kAreaRouteViewReserved15dBytes = 0xB;
inline constexpr std::size_t kAreaRouteViewReserved16cBytes = 4;
inline constexpr std::size_t kAreaRouteViewReserved174Bytes = 8;
inline constexpr std::size_t kAreaRouteViewReserved184Bytes = 4;
inline constexpr std::size_t kAreaRouteViewReserved18c190Bytes = 11;

// decomp: routeEdgeState (src/BATTLE/BATTLE.PRG/771A8.c:554)
struct AreaRouteView {
  std::array<std::uint8_t, kAreaRouteViewReserved0Bytes> reserved0;
  std::uint8_t opaque5;
  std::array<std::uint8_t, kAreaRouteViewReserved6Bytes> reserved6;
  TilePosition tile;
  std::array<std::uint8_t, kAreaRouteViewReserved38Bytes> reserved38;
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kAreaRouteViewReserved526cBytes> reserved_52_6c;
  std::uint8_t actor_id;
  std::array<std::uint8_t, kAreaRouteViewReserved89Bytes> reserved89;
  std::uint16_t cooldown;
  std::uint16_t period;
  std::array<std::uint8_t, kAreaRouteViewReserved13eBytes> reserved_13e;
  std::uint8_t area;
  std::array<std::uint8_t, kAreaRouteViewReserved155Bytes> reserved155;
  std::uint8_t movement_target;
  std::array<std::uint8_t, kAreaRouteViewReserved15dBytes> reserved_15d;
  TilePosition destination;
  std::array<std::uint8_t, kAreaRouteViewReserved16cBytes> reserved_16c;
  TilePosition next_tile;
  std::array<std::uint8_t, kAreaRouteViewReserved174Bytes> reserved174;
  std::int32_t move_x;
  std::int32_t move_z;
  std::array<std::uint8_t, kAreaRouteViewReserved184Bytes> reserved184;
  std::int16_t heading;
  std::int16_t angle;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kAreaRouteViewReserved18c190Bytes> reserved_18c_190;
  std::uint8_t gate;
  RouteGateTile exit;
};

// decomp: stuckMovementPosition (src/BATTLE/BATTLE.PRG/71618.c:1187)
union BlockedTileWord {
  std::uint32_t word;
  struct {
    std::uint8_t tile_x;
    std::uint8_t tag;
    std::uint8_t tile_z;
    std::uint8_t search_status;
  } tile;
};

inline constexpr std::size_t kBlockedRecoveryViewReserved3Bytes = 0x19;
inline constexpr std::size_t kBlockedRecoveryViewReserved1dBytes = 0x17;
inline constexpr std::size_t kBlockedRecoveryViewReserved3cBytes = 0x10;
inline constexpr std::size_t kBlockedRecoveryViewReserved526cBytes = 142;
inline constexpr std::size_t kBlockedRecoveryViewReservedFcBytes = 0x1C;
inline constexpr std::size_t kBlockedRecoveryViewDetourDirectionsBytes = 4;
inline constexpr std::size_t kBlockedRecoveryViewReserved11cBytes = 0x40;
inline constexpr std::size_t kBlockedRecoveryViewReserved15dBytes = 0xB;

// decomp: stuckMovementState (src/BATTLE/BATTLE.PRG/71618.c:1210)
struct BlockedRecoveryView {
  std::array<std::uint8_t, 2> reserved0;
  std::uint8_t is_stuck;
  std::array<std::uint8_t, kBlockedRecoveryViewReserved3Bytes> reserved3;
  std::uint8_t opaque_1c;
  std::array<std::uint8_t, kBlockedRecoveryViewReserved1dBytes> reserved_1d;
  BlockedTileWord tile;
  BlockedTileWord previous_tile;
  std::array<std::uint8_t, kBlockedRecoveryViewReserved3cBytes> reserved_3c;
  std::int16_t x;
  std::array<std::uint8_t, 2> reserved_4e;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;   // PS1 pointer slot 0x54
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kBlockedRecoveryViewReserved526cBytes>
      reserved_52_6c;
  std::uint8_t detour_step;
  std::uint8_t opaque_fb;
  std::array<std::uint8_t, kBlockedRecoveryViewReservedFcBytes> reserved_fc;
  std::array<std::uint8_t, kBlockedRecoveryViewDetourDirectionsBytes>
      detour_directions;
  std::array<std::uint8_t, kBlockedRecoveryViewReserved11cBytes> reserved_11c;
  std::uint8_t movement_target;
  std::array<std::uint8_t, kBlockedRecoveryViewReserved15dBytes> reserved_15d;
  BlockedTileWord destination;
};

inline constexpr std::size_t kApproachTileWordByteCount = 4;

// decomp: targetMovementPoint (src/BATTLE/BATTLE.PRG/771A8.c:1893)
union ApproachTileWord {
  std::uint32_t word;
  std::array<std::uint8_t, kApproachTileWordByteCount> bytes;
};
inline constexpr std::size_t kApproachTileWordBytes = 0x4;
static_assert(sizeof(ApproachTileWord) == kApproachTileWordBytes,
              "ApproachTileWord must keep its PS1 layout");

// decomp: targetMovementFlags (src/BATTLE/BATTLE.PRG/771A8.c:1896)
struct ApproachRelationBits {
  std::uint32_t active : 1;
  std::uint32_t opaque0_bit1 : 1;
  std::uint32_t opaque0_bit2 : 1;
  std::uint32_t reserved0_bit3 : 2;
  std::uint32_t visible : 1;
  std::uint32_t reserved0_bit6 : 26;
};
inline constexpr std::size_t kApproachRelationBitsBytes = 0x4;
static_assert(sizeof(ApproachRelationBits) == kApproachRelationBitsBytes,
              "ApproachRelationBits must keep its PS1 layout");

inline constexpr std::size_t kTargetApproachViewReserved5Bytes = 4;
inline constexpr std::size_t kTargetApproachViewReservedEBytes = 22;
inline constexpr std::size_t kTargetApproachViewReserved25Bytes = 14;
inline constexpr std::size_t kTargetApproachViewReserved38Bytes = 16;
inline constexpr std::size_t kTargetApproachViewReserved5c6cBytes = 28;
inline constexpr std::size_t kTargetApproachViewReserved89Bytes = 83;
inline constexpr std::size_t kTargetApproachViewReservedF0Bytes = 4;
inline constexpr std::size_t kTargetApproachViewReservedFaBytes = 94;
inline constexpr std::size_t kTargetApproachViewReserved160Bytes = 8;
inline constexpr std::size_t kTargetApproachViewReserved16cBytes = 16;
inline constexpr std::size_t kTargetApproachViewReserved184184Bytes = 8;
inline constexpr std::size_t kTargetApproachViewReserved184190Bytes = 16;
inline constexpr std::size_t kTargetApproachViewRelationsCount = 16;
inline constexpr std::size_t kTargetApproachViewDistancesSquaredCount = 16;

// decomp: targetMovementState (src/BATTLE/BATTLE.PRG/771A8.c:1967)
struct TargetApproachView {
  std::uint8_t close_approach;
  std::uint8_t gait;
  std::array<std::uint8_t, 1> reserved2;
  std::uint8_t height_filter;
  std::uint8_t near_target_detour;
  std::array<std::uint8_t, kTargetApproachViewReserved5Bytes> reserved5;
  std::uint8_t toward_point;
  std::array<std::uint8_t, 2> reserved_a;
  std::uint8_t facing_locked;
  std::uint8_t approach_directly;
  std::array<std::uint8_t, kTargetApproachViewReservedEBytes> reserved_e;
  std::uint8_t chases_lost_target;
  std::array<std::uint8_t, kTargetApproachViewReserved25Bytes> reserved25;
  std::uint8_t approaches_target_point;
  TilePosition tile;
  std::array<std::uint8_t, kTargetApproachViewReserved38Bytes> reserved38;
  ApproachTileWord saved_destination;
  std::int16_t x;
  std::array<std::uint8_t, 2> reserved_4e;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52;
  ApproachActorView* actor;
  ApproachTargetRecordView* model;
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kTargetApproachViewReserved5c6cBytes> reserved_5c_6c;
  std::uint8_t actor_id;
  std::array<std::uint8_t, kTargetApproachViewReserved89Bytes> reserved89;
  std::int32_t range;
  std::int32_t previous_range;
  std::uint8_t chasing_lost_target;
  std::uint8_t target_id;
  std::uint8_t timeout;
  std::uint8_t recover_in_range;
  std::uint8_t walked_close;
  std::uint8_t range_mode;
  std::uint8_t previous_option;
  std::uint8_t previous_target_id;
  ApproachTileWord cached_destination;
  std::array<std::uint8_t, kTargetApproachViewReservedF0Bytes> reserved_f0;
  std::uint8_t target_tile_x;
  std::array<std::uint8_t, 1> reserved_f5;
  std::uint8_t target_tile_z;
  std::array<std::uint8_t, 1> reserved_f7;
  std::uint16_t retarget_timer;
  std::array<std::uint8_t, kTargetApproachViewReservedFaBytes> reserved_fa;
  std::int16_t target_x;
  std::int16_t target_z;
  std::uint8_t movement_target;
  std::array<std::uint8_t, 1> reserved_15d;
  std::uint16_t movement_flags;
  std::array<std::uint8_t, kTargetApproachViewReserved160Bytes> reserved160;
  ApproachTileWord destination;
  std::array<std::uint8_t, kTargetApproachViewReserved16cBytes> reserved_16c;
  std::int32_t move_x;
  std::int32_t move_z;
  std::array<std::uint8_t, kTargetApproachViewReserved184184Bytes>
      reserved184_184;
  void* native_pointer_18c;  // PS1 pointer slot 0x18C
  std::array<std::uint8_t, kTargetApproachViewReserved184190Bytes>
      reserved184_190;
  void* native_pointer_1a0;  // PS1 pointer slot 0x1A0
  std::array<ApproachRelationBits, kTargetApproachViewRelationsCount> relations;
  std::array<std::uint32_t, kTargetApproachViewDistancesSquaredCount>
      distances_squared;
};

// decomp: terrainMovementPoint (src/BATTLE/BATTLE.PRG/771A8.c:1478)
union TerrainTileWord {
  std::uint32_t word;
  struct {
    std::uint8_t tile_x;
    std::uint8_t tag;
    std::uint8_t tile_z;
    std::uint8_t search_status;
  } tile;
};

inline constexpr std::size_t kTerrainMoveViewReserved6Bytes = 20;
inline constexpr std::size_t kTerrainMoveViewReserved1bBytes = 14;
inline constexpr std::size_t kTerrainMoveViewReserved2aBytes = 10;
inline constexpr std::size_t kTerrainMoveViewReserved38Bytes = 20;
inline constexpr std::size_t kTerrainMoveViewPositionCount = 3;
inline constexpr std::size_t kTerrainMoveViewReserved586cBytes = 252;
inline constexpr std::size_t kTerrainMoveViewReserved16cBytes = 16;
inline constexpr std::size_t kTerrainMoveViewReserved184Bytes = 4;

// decomp: terrainMovementState (src/BATTLE/BATTLE.PRG/771A8.c:1508)
struct TerrainMoveView {
  std::array<std::uint8_t, 2> reserved0;
  std::uint8_t is_stuck;
  std::array<std::uint8_t, 1> reserved3;
  std::uint8_t opaque4;
  std::uint8_t opaque5;
  std::array<std::uint8_t, kTerrainMoveViewReserved6Bytes> reserved6;
  std::uint8_t opaque_1a;
  std::array<std::uint8_t, kTerrainMoveViewReserved1bBytes> reserved_1b;
  std::uint8_t settings_bit14;
  std::array<std::uint8_t, kTerrainMoveViewReserved2aBytes> reserved_2a;
  TerrainTileWord tile;
  std::array<std::uint8_t, kTerrainMoveViewReserved38Bytes> reserved38;
  std::array<std::int16_t, kTerrainMoveViewPositionCount> position;
  std::array<std::uint8_t, 2> reserved52;
  TerrainMoveActorView* actor;
  void* native_pointer58;   // PS1 pointer slot 0x58
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kTerrainMoveViewReserved586cBytes> reserved_58_6c;
  TerrainTileWord destination;
  std::array<std::uint8_t, kTerrainMoveViewReserved16cBytes> reserved_16c;
  std::int32_t move_x;
  std::int32_t move_z;
  std::array<std::uint8_t, kTerrainMoveViewReserved184Bytes> reserved184;
  std::int16_t angle;
};

inline constexpr std::size_t kOrbitTrackingViewReserved0Bytes = 0x4C;
inline constexpr std::size_t kOrbitTrackingViewReserved606cBytes = 240;
inline constexpr std::size_t kOrbitTrackingViewReserved15dBytes = 0x1F;

// decomp: trackingMotionState (src/BATTLE/BATTLE.PRG/7EE98.c:571)
struct OrbitTrackingView {
  std::array<std::uint8_t, kOrbitTrackingViewReserved0Bytes> reserved0;
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;  // PS1 pointer slot 0x54
  ModelActiveView* model;
  OrbitStatsView* stats;
  void* native_pointer60;  // PS1 pointer slot 0x60
  void* native_pointer64;  // PS1 pointer slot 0x64
  void* native_pointer68;  // PS1 pointer slot 0x68
  std::array<std::uint8_t, kOrbitTrackingViewReserved606cBytes> reserved_60_6c;
  std::uint8_t movement_target;
  std::array<std::uint8_t, kOrbitTrackingViewReserved15dBytes> reserved_15d;
  std::int32_t move_x;
  std::int32_t move_z;
  std::int32_t move_y;
};

inline constexpr std::size_t kWanderViewReserved4Bytes = 4;
inline constexpr std::size_t kWanderViewReserved9Bytes = 0x2B;
inline constexpr std::size_t kWanderViewReserved38Bytes = 0x10;
inline constexpr std::size_t kWanderViewReserved5c6cBytes = 72;
inline constexpr std::size_t kWanderViewReservedB8Bytes = 8;
inline constexpr std::size_t kWanderViewReservedC8Bytes = 8;
inline constexpr std::size_t kWanderViewReservedD9Bytes = 0x43;
inline constexpr std::size_t kWanderViewReserved120Bytes = 13;
inline constexpr std::size_t kWanderViewReserved12eBytes = 0x2F;
inline constexpr std::size_t kWanderViewReserved160Bytes = 8;

// decomp: wanderState (src/BATTLE/BATTLE.PRG/771A8.c:1625)
struct WanderView {
  std::array<std::uint8_t, 2> reserved0;
  std::uint8_t is_stuck;
  std::uint8_t opaque3;
  std::array<std::uint8_t, kWanderViewReserved4Bytes> reserved4;
  std::uint8_t stayed_on_tile;
  std::array<std::uint8_t, kWanderViewReserved9Bytes> reserved9;
  TilePosition tile;
  std::array<std::uint8_t, kWanderViewReserved38Bytes> reserved38;
  TilePosition saved_destination;
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
  std::array<std::uint8_t, 2> reserved52_52;
  void* native_pointer54;  // PS1 pointer slot 0x54
  WanderRecordView* model;
  void* native_pointer_5c;  // PS1 pointer slot 0x5C
  void* native_pointer60;   // PS1 pointer slot 0x60
  void* native_pointer64;   // PS1 pointer slot 0x64
  void* native_pointer68;   // PS1 pointer slot 0x68
  std::array<std::uint8_t, kWanderViewReserved5c6cBytes> reserved_5c_6c;
  std::uint32_t move_distance;
  std::array<std::uint8_t, kWanderViewReservedB8Bytes> reserved_b8;
  std::uint16_t wander_phase;
  std::uint16_t wander_timer;
  std::int32_t wander_range_squared;
  std::array<std::uint8_t, kWanderViewReservedC8Bytes> reserved_c8;
  std::int16_t wander_heading;
  std::uint16_t wander_cooldown;
  std::uint16_t rest_period;
  std::uint16_t move_period;
  std::uint8_t wander_rate;
  std::array<std::uint8_t, kWanderViewReservedD9Bytes> reserved_d9;
  TilePosition goal;
  std::array<std::uint8_t, kWanderViewReserved120Bytes> reserved120;
  std::uint8_t behavior_mode;
  std::array<std::uint8_t, kWanderViewReserved12eBytes> reserved_12e;
  std::uint8_t count;
  std::uint16_t movement_flags;
  std::array<std::uint8_t, kWanderViewReserved160Bytes> reserved160;
  TilePosition destination;
};

// decomp: actorInitializationPoint (src/BATTLE/BATTLE.PRG/7CD68.c:833)
union CreationTileWord {
  std::uint32_t word;
  struct {
    std::uint8_t tile_x;
    std::uint8_t tag;
    std::uint8_t tile_z;
    std::uint8_t search_status;
  } tile;
};

inline constexpr std::size_t kCreationModelViewReserved0Bytes = 8;
inline constexpr std::size_t kCreationModelViewReservedCBytes = 16;
inline constexpr std::size_t kCreationModelViewPositionCount = 3;
inline constexpr std::size_t kCreationModelViewReserved22Bytes = 58;
inline constexpr std::size_t kCreationModelViewReserved6060Bytes = 8;
inline constexpr std::size_t kCreationModelViewReserved606cBytes = 256;
inline constexpr std::size_t kCreationModelViewReserved170170Bytes = 1120;
inline constexpr std::size_t kCreationModelViewReserved1705ecBytes = 80;

// decomp: actorInitializationModel (src/BATTLE/BATTLE.PRG/7CD68.c:853)
struct CreationModelView {
  std::array<std::uint8_t, kCreationModelViewReserved0Bytes> reserved0;
  std::uint32_t opaque8;
  std::array<std::uint8_t, kCreationModelViewReservedCBytes> reserved_c;
  std::array<std::int16_t, kCreationModelViewPositionCount> position;
  std::array<std::uint8_t, kCreationModelViewReserved22Bytes> reserved22;
  CreationTileWord tile;
  std::array<std::uint8_t, kCreationModelViewReserved6060Bytes> reserved60_60;
  void* native_pointer68;  // PS1 pointer slot 0x68
  std::array<std::uint8_t, kCreationModelViewReserved606cBytes> reserved_60_6c;
  std::uint32_t kind;
  std::array<std::uint8_t, kCreationModelViewReserved170170Bytes>
      reserved170_170;
  void* native_pointer_5d0;  // PS1 pointer slot 0x5D0
  void* native_pointer_5d4;  // PS1 pointer slot 0x5D4
  void* native_pointer_5d8;  // PS1 pointer slot 0x5D8
  void* native_pointer_5dc;  // PS1 pointer slot 0x5DC
  void* native_pointer_5e0;  // PS1 pointer slot 0x5E0
  void* native_pointer_5e4;  // PS1 pointer slot 0x5E4
  void* native_pointer_5e8;  // PS1 pointer slot 0x5E8
  std::array<std::uint8_t, kCreationModelViewReserved1705ecBytes>
      reserved_170_5ec;
  std::uint16_t radius;
  std::uint16_t opaque_63e;
};

inline constexpr std::size_t kCreationStatsViewReserved0Bytes = 48;

// decomp: actorInitializationStats (src/BATTLE/BATTLE.PRG/7CD68.c:840)
struct CreationStatsView {
  std::array<std::uint8_t, kCreationStatsViewReserved0Bytes> reserved0;
  std::uint8_t opaque30;
  std::uint8_t opaque31;
  std::uint8_t opaque32;
  std::uint8_t opaque33;
};
inline constexpr std::size_t kCreationStatsViewBytes = 0x34;
static_assert(sizeof(CreationStatsView) == kCreationStatsViewBytes,
              "CreationStatsView must keep its PS1 layout");

inline constexpr std::size_t kPerceivedObjectViewReserved0Bytes = 8;
inline constexpr std::size_t kPerceivedObjectViewReservedCBytes = 8;
inline constexpr std::size_t kPerceivedObjectViewReserved16Bytes = 6;
inline constexpr std::size_t kPerceivedObjectViewReserved28Bytes = 0x34;
inline constexpr std::size_t kPerceivedObjectViewReserved6464Bytes = 4;
inline constexpr std::size_t kPerceivedObjectViewReserved646cBytes = 1344;

// decomp: actorObject (src/BATTLE/BATTLE.PRG/6FC00.c:588)
struct PerceivedObjectView {
  std::array<std::uint8_t, kPerceivedObjectViewReserved0Bytes> reserved0;
  union {
    std::uint32_t word;
    struct {
      std::uint32_t reserved0 : 16;
      std::uint32_t skip_state_a : 3;
      std::uint32_t skip_state_b : 2;
      std::uint32_t reserved21 : 10;
      std::uint32_t forces_perception : 1;
    } bits;
  } opaque8;
  std::array<std::uint8_t, kPerceivedObjectViewReservedCBytes> reserved_c;
  std::int16_t opaque14;
  std::array<std::uint8_t, kPerceivedObjectViewReserved16Bytes> reserved16;
  VS::Core::Gte::SVECTOR position;
  std::int16_t reserved24;
  std::int16_t facing;
  std::array<std::uint8_t, kPerceivedObjectViewReserved28Bytes> reserved28;
  TilePosition tile;
  TilePosition last_tile;
  std::array<std::uint8_t, kPerceivedObjectViewReserved6464Bytes> reserved64_64;
  void* native_pointer68;  // PS1 pointer slot 0x68
  std::array<std::uint8_t, kPerceivedObjectViewReserved646cBytes>
      reserved_64_6c;
  std::uint32_t reserved_5ac_bit0 : 15;
  std::uint32_t opaque_5ad_bit7 : 1;
  std::uint32_t reserved_5ae_bit0 : 16;
};

inline constexpr std::size_t kSettingsWordByteCount = 4;

// decomp: actorSettingsWord (src/BATTLE/BATTLE.PRG/7CD68.c:604)
union SettingsWord {
  std::uint32_t word;
  std::array<std::uint16_t, 2> halves;
  std::array<std::uint8_t, kSettingsWordByteCount> bytes;
};
inline constexpr std::size_t kSettingsWordBytes = 0x4;
static_assert(sizeof(SettingsWord) == kSettingsWordBytes,
              "SettingsWord must keep its PS1 layout");

inline constexpr std::size_t kSettingsActorViewReserved04Bytes = 22;
inline constexpr std::size_t kSettingsActorViewReserved1cBytes = 20;

// decomp: actorSettingsActor (src/BATTLE/BATTLE.PRG/7CD68.c:616)
struct SettingsActorView {
  void* native_pointer0;  // PS1 pointer slot 0x0
  std::array<std::uint8_t, kSettingsActorViewReserved04Bytes> reserved0_4;
  std::uint16_t kind;
  std::array<std::uint8_t, kSettingsActorViewReserved1cBytes> reserved_1c;
  SettingsWord opaque30;
  union {
    std::uint32_t word;
    struct {
      std::uint32_t reserved0 : 12;
      std::uint32_t mode : 2;
      std::uint32_t upper_bits : 18;
    } bits;
  } opaque34;
};

// decomp: actorUpdatePoint (src/BATTLE/BATTLE.PRG/6FC00.c:873)
struct PositionLinkView {
  std::uint32_t position;
  std::uint32_t opaque4;
};
inline constexpr std::size_t kPositionLinkViewBytes = 0x8;
static_assert(sizeof(PositionLinkView) == kPositionLinkViewBytes,
              "PositionLinkView must keep its PS1 layout");

// decomp: da4bcEntry (src/BATTLE/BATTLE.PRG/71618.c:713)
struct EscortEntryView {
  std::uint32_t reserved0;
  std::uint32_t reserved4;
  std::uint32_t reserved8;
  std::uint32_t opaque_c;
};
inline constexpr std::size_t kEscortEntryViewBytes = 0x10;
static_assert(sizeof(EscortEntryView) == kEscortEntryViewBytes,
              "EscortEntryView must keep its PS1 layout");

// decomp: func_800E5A74_t (src/BATTLE/BATTLE.PRG/6F014.c:19)
struct SavedSettingsPair {
  std::int32_t reserved0;
  std::uint16_t opaque4;
};
inline constexpr std::size_t kSavedSettingsPairBytes = 0x8;
static_assert(sizeof(SavedSettingsPair) == kSavedSettingsPairBytes,
              "SavedSettingsPair must keep its PS1 layout");

// decomp: nearestWaypointDestination (src/BATTLE/BATTLE.PRG/74088.c:944)
struct WaypointTileRecord {
  std::uint8_t x;
  std::uint8_t reserved1;
  std::uint8_t z;
  std::uint8_t reserved3;
};
inline constexpr std::size_t kWaypointTileRecordBytes = 0x4;
static_assert(sizeof(WaypointTileRecord) == kWaypointTileRecordBytes,
              "WaypointTileRecord must keep its PS1 layout");

inline constexpr std::size_t kModelActiveViewReserved00Bytes = 104;
inline constexpr std::size_t kModelActiveViewReserved06cBytes = 1348;

// decomp: radialMotionContext (src/BATTLE/BATTLE.PRG/7EE98.c:479)
struct ModelActiveView {
  std::array<std::uint8_t, kModelActiveViewReserved00Bytes> reserved0_0;
  void* native_pointer68;  // PS1 pointer slot 0x68
  std::array<std::uint8_t, kModelActiveViewReserved06cBytes> reserved_0_6c;
  std::uint32_t reserved_5b0_bit0 : 4;
  std::uint32_t active : 1;
  std::uint32_t reserved_5b0_bit5 : 27;
};

inline constexpr std::size_t kApproachActorViewReserved04Bytes = 5;

// decomp: targetMovementActor (src/BATTLE/BATTLE.PRG/771A8.c:1900)
struct ApproachActorView {
  void* native_pointer0;  // PS1 pointer slot 0x0
  std::array<std::uint8_t, kApproachActorViewReserved04Bytes> reserved0_4;
  std::uint8_t id;
};

inline constexpr std::size_t kApproachTargetRecordViewReserved0Bytes = 8;
inline constexpr std::size_t kApproachTargetRecordViewReservedCBytes = 16;

// decomp: targetMovementModel (src/BATTLE/BATTLE.PRG/771A8.c:1908)
struct ApproachTargetRecordView {
  std::array<std::uint8_t, kApproachTargetRecordViewReserved0Bytes> reserved0;
  std::uint32_t opaque8;
  std::array<std::uint8_t, kApproachTargetRecordViewReservedCBytes> reserved_c;
  std::int16_t x;
  std::array<std::uint8_t, 2> reserved_1e;
  std::int16_t z;
};

inline constexpr std::size_t kTerrainMoveActorViewReserved04Bytes = 5;

// decomp: terrainMovementActor (src/BATTLE/BATTLE.PRG/771A8.c:1484)
struct TerrainMoveActorView {
  void* native_pointer0;  // PS1 pointer slot 0x0
  std::array<std::uint8_t, kTerrainMoveActorViewReserved04Bytes> reserved0_4;
  std::uint8_t command_opcode;
  std::array<std::uint8_t, 2> reserved_a;
  std::uint16_t command_heading;
};

inline constexpr std::size_t kOrbitStatsViewReserved0Bytes = 0x18;

// decomp: trackingMotionStats (src/BATTLE/BATTLE.PRG/7EE98.c:570)
struct OrbitStatsView {
  std::array<std::uint8_t, kOrbitStatsViewReserved0Bytes> reserved0;
  std::int16_t current_hp;
  std::int16_t max_hp;
};
inline constexpr std::size_t kOrbitStatsViewBytes = 0x1C;
static_assert(sizeof(OrbitStatsView) == kOrbitStatsViewBytes,
              "OrbitStatsView must keep its PS1 layout");

inline constexpr std::size_t kWanderRecordViewReserved0Bytes = 8;

// decomp: wanderObject (src/BATTLE/BATTLE.PRG/771A8.c:1590)
struct WanderRecordView {
  std::array<std::uint8_t, kWanderRecordViewReserved0Bytes> reserved0;
  std::uint32_t opaque8;
};

inline constexpr std::size_t kGuardTileWordByteCount = 4;

// decomp: da4bcPoint (src/BATTLE/BATTLE.PRG/71618.c:752)
union GuardTileWord {
  std::uint32_t word;
  std::array<std::uint8_t, kGuardTileWordByteCount> bytes;
};
inline constexpr std::size_t kGuardTileWordBytes = 0x4;
static_assert(sizeof(GuardTileWord) == kGuardTileWordBytes,
              "GuardTileWord must keep its PS1 layout");

}  // namespace VS::Battle::Ai
