#pragma once
// The action records the AI planner and targeting read: the candidate
// entry, the action timing, the reaction record and the action range.

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Battle::Ai {

struct ActionRangeView2;

// decomp: actionCandidateEntry (src/BATTLE/BATTLE.PRG/6FC00.c:500)
struct CandidateEntryView {
  std::int32_t reserved0;
  std::uint32_t row : 3;
  std::uint32_t column : 2;
  std::uint32_t action : 8;
  std::uint32_t threshold : 16;
  std::uint32_t reserved7_bit5 : 3;
  std::uint32_t distance_squared;
  std::uint32_t target_id : 4;
  std::uint32_t reserved_c_bit4 : 3;
  std::uint32_t reserved_c_bit7 : 12;
  std::uint32_t weight : 8;
  std::uint32_t reserved_f_bit3 : 5;
  std::uint32_t reserved10_bit0 : 1;
  std::uint32_t opaque10_bit1 : 1;
  std::uint32_t reserved10_bit2 : 1;
  std::uint32_t reserved10_bit3 : 1;
  std::uint32_t reserved10_bit4 : 1;
  std::uint32_t reserved10_bit5 : 1;
  std::uint32_t range : 16;
  std::uint32_t reserved12_bit6 : 10;
};
inline constexpr std::size_t kCandidateEntryViewBytes = 0x14;
static_assert(sizeof(CandidateEntryView) == kCandidateEntryViewBytes,
              "CandidateEntryView must keep its PS1 layout");

// decomp: func_800DC888_t (src/BATTLE/BATTLE.PRG/6F014.c:253)
struct ActionTiming {
  std::uint32_t opaque0_bit0 : 4;
  std::uint32_t reserved0_bit4 : 28;
  std::int16_t opaque4;
  std::int16_t opaque6;
  std::int16_t opaque8;
  std::int16_t opaque_a;
  std::uint32_t opaque_c;
  std::int16_t opaque10;
  std::int16_t opaque12;
  std::int16_t opaque14;
  std::int16_t opaque16;
};
inline constexpr std::size_t kActionTimingBytes = 0x18;
static_assert(sizeof(ActionTiming) == kActionTimingBytes,
              "ActionTiming must keep its PS1 layout");

inline constexpr std::size_t kEngagementActorReserved8Bytes = 0xC;
inline constexpr std::size_t kEngagementActorReserved20Bytes = 0x18;

// decomp: func_800DEB10_t (src/BATTLE/BATTLE.PRG/6F014.c:278)
struct EngagementActor {
  std::uint8_t opaque0;
  std::uint8_t reserved1;
  std::uint8_t opaque2;
  std::uint8_t reserved3;
  std::int16_t opaque4;
  std::int16_t opaque6;
  std::array<std::uint8_t, kEngagementActorReserved8Bytes> reserved8;
  std::uint32_t opaque14;
  std::uint32_t opaque18;
  std::uint32_t opaque_1c_bit0 : 2;
  std::uint32_t opaque_1c_bit2 : 2;
  std::uint32_t reserved_1c_bit4 : 14;
  std::uint32_t opaque_1e_bit2 : 2;
  std::uint32_t reserved_1e_bit4 : 2;
  std::uint32_t opaque_1e_bit6 : 2;
  std::uint32_t reserved_1f_bit0 : 8;
  std::array<std::uint8_t, kEngagementActorReserved20Bytes> reserved20;
  std::int16_t opaque38;
  std::array<std::uint8_t, 2> reserved_3a;
  std::int16_t opaque_3c;
  std::array<std::uint8_t, 2> reserved_3e;
  std::int32_t opaque40;
};
inline constexpr std::size_t kEngagementActorBytes = 0x44;
static_assert(sizeof(EngagementActor) == kEngagementActorBytes,
              "EngagementActor must keep its PS1 layout");

// decomp: func_800DEEA4_t (src/BATTLE/BATTLE.PRG/6F014.c:287)
struct ReactionRecord {
  std::uint16_t action_id;
  std::array<std::uint8_t, 2> reserved2;
  EngagementActor primary_target;
  std::array<std::uint8_t, 2> reserved48;
  std::uint16_t target_count;
  EngagementActor targets[0];
};
inline constexpr std::size_t kReactionRecordBytes = 0x4C;
static_assert(sizeof(ReactionRecord) == kReactionRecordBytes,
              "ReactionRecord must keep its PS1 layout");

// decomp: func_800E5568_t (src/BATTLE/BATTLE.PRG/6F014.c:27)
struct ActionRangeShape {
  std::uint8_t reserved0;
  std::uint8_t range_y;
  std::uint8_t range_z;
  std::uint8_t shape : 3;
  std::uint8_t angle : 5;
};
inline constexpr std::size_t kActionRangeShapeBytes = 0x4;
static_assert(sizeof(ActionRangeShape) == kActionRangeShapeBytes,
              "ActionRangeShape must keep its PS1 layout");

inline constexpr std::size_t kActionRangeViewReserved00Bytes = 84;

// decomp: func_800E5698_t (src/BATTLE/BATTLE.PRG/6F014.c:37)
struct ActionRangeView {
  std::array<std::uint8_t, kActionRangeViewReserved00Bytes> reserved0_0;
  void* native_pointer54;  // PS1 pointer slot 0x54
  void* native_pointer58;  // PS1 pointer slot 0x58
  ActionRangeView2* opaque_5c;
};

inline constexpr std::size_t kActionRangeView2Reserved0Bytes = 0x38;

// decomp: func_800E5698_t2 (src/BATTLE/BATTLE.PRG/6F014.c:32)
struct ActionRangeView2 {
  std::array<std::uint8_t, kActionRangeView2Reserved0Bytes> reserved0;
  ActionRangeShape opaque38;
};
inline constexpr std::size_t kActionRangeView2Bytes = 0x3C;
static_assert(sizeof(ActionRangeView2) == kActionRangeView2Bytes,
              "ActionRangeView2 must keep its PS1 layout");

}  // namespace VS::Battle::Ai
