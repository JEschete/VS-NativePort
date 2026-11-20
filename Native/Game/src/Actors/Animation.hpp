#pragma once
// An actor's animation pose and the SEQ animation bank header, plus the battle
// program's shared sequence and weapon animation tables. Animation.cpp defines
// the tables; the module image fills most of them at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Actors/TextureAnimationRecords.hpp"
#include "Core/Gte.hpp"

namespace VS::Actors {

inline constexpr int kPoseBones = 42;

// layout: ledger (upstream merge) for D_800F4538_unkC54
// (src/BATTLE/BATTLE.PRG/entity.h:43); the native record folds the original's
// 41 bones plus one into 42.
struct AnimationPose {
  /* 0x00 */ VS::Core::Gte::SVECTOR bone_keys[kPoseBones];
  /* 0x150 */ VS::Core::Gte::SVECTOR scale_keys[kPoseBones];
  /* 0x2A0 */ VS::Core::Gte::SVECTOR rotation_deltas[kPoseBones];
  /* 0x3F0 */ VS::Core::Gte::SVECTOR track_states[kPoseBones];
  /* 0x540 */ std::int16_t frame;
  /* 0x542 */ std::uint16_t next_event_frame;
  /* 0x544 */ std::uint8_t* event_script;
  /* 0x548 */ std::uint8_t animation_id;
  /* 0x549 */ std::uint8_t event_wait;
  /* 0x54A */ std::int32_t holding_last_frame : 1;
  /* 0x54A */ std::int32_t reached_end : 1;
  /* 0x54A */ std::int32_t reserved_bits : 14;
  /* 0x54C */ std::int32_t reserved54c;
};

// decomp: func_800AD494_t (src/BATTLE/BATTLE.PRG/40564.c:24). animation_map is
// a byte offset from the start of the bank (an on-disc pointer).
struct SeqAnimationBank {
  std::int32_t animation_data;
  std::uint32_t animation_map;
  std::uint16_t headers[0];
};
inline constexpr std::size_t kSeqAnimationBankHeaderBytes = 0x8;
static_assert(sizeof(SeqAnimationBank) == kSeqAnimationBankHeaderBytes,
              "SeqAnimationBank must keep its PS1 layout");

inline constexpr int kSharedSequenceBankSlots = 4;
inline constexpr int kSharedSequenceEntrySlots = 16;
inline constexpr int kSecondarySlotTimelines = 188;
inline constexpr int kSecondarySlotKeys = 8;

// clang-format's alias continuations disagree with cpplint.
template <typename T>
using SharedSequenceRow = std::array<T, kSharedSequenceEntrySlots>;
template <typename T>
using SharedSequenceTable =  // NOLINTNEXTLINE(whitespace/indent_namespace)
    std::array<SharedSequenceRow<T>, kSharedSequenceBankSlots>;
using SecondarySlotTimeline =  // NOLINTNEXTLINE(whitespace/indent_namespace)
    std::array<TextureAnimationKeyRecord, kSecondarySlotKeys>;
using SecondarySlotTimelines =  // NOLINTNEXTLINE(whitespace/indent_namespace)
    std::array<SecondarySlotTimeline, kSecondarySlotTimelines>;

// The byte tables' sizes are the address gaps to the next symbol in the
// module image, not capacities.
template <std::size_t Size>
using ByteTable = std::array<std::uint8_t, Size>;
inline constexpr std::size_t kSharedSequenceIdsByKindBytes = 592;
inline constexpr std::size_t kDrawnAnimationBasesBytes = 3232;
inline constexpr std::size_t kSheathedAnimationBasesBytes = 3220;
inline constexpr std::size_t kAnimationEventLengthsBytes = 2712;
inline constexpr std::size_t kDrawnWeaponSoundIdsBytes = 2644;
inline constexpr std::size_t kTexturePagesOneBasedBytes = 8425;
inline constexpr std::size_t kBankTexturePagesBytes = 8424;
inline constexpr std::size_t kAnimationEventPointEntries = 97;
inline constexpr std::size_t kAnimationEventPointsPerEntry = 4;
using AnimationEventPointRow =  // NOLINTNEXTLINE(whitespace/indent_namespace)
    std::array<std::uint16_t, kAnimationEventPointsPerEntry>;
using AnimationEventPoints =  // NOLINTNEXTLINE(whitespace/indent_namespace)
    std::array<AnimationEventPointRow, kAnimationEventPointEntries>;

// decomp: D_800E8960 (src/BATTLE/BATTLE.PRG/30DB0.c:61)
extern ByteTable<kSharedSequenceIdsByKindBytes> sharedSequenceIdsByKind;
// decomp: D_800E8F2C (src/BATTLE/BATTLE.PRG/30DB0.c:122)
extern std::uint8_t sharedSequenceBankIndex;
// decomp: D_800E8FC0 (src/BATTLE/BATTLE.PRG/30DB0.h:43). Nonzero while the
// first-person camera transition runs (func_800A0870); func_800B28A8
// (49EB8.c:294) then draws every actor through the override drawers.
extern char forcedModelOverrides;
// decomp: D_800E9090, upstream weaponCategorySfx
// (src/BATTLE/BATTLE.PRG/30DB0.c:82)
extern ByteTable<kDrawnAnimationBasesBytes> drawnWeaponAnimationBases;
// decomp: D_800E909C (src/BATTLE/BATTLE.PRG/30DB0.c:120)
extern ByteTable<kSheathedAnimationBasesBytes> sheathedWeaponAnimationBases;
// decomp: D_800E916C (src/BATTLE/BATTLE.PRG/40564.c:36)
extern SecondarySlotTimelines secondarySlotSequences;
// decomp: D_800E9298 (src/BATTLE/BATTLE.PRG/44F14.c:56)
extern ByteTable<kAnimationEventLengthsBytes> animationEventLengths;
// decomp: D_800E92DC (src/BATTLE/BATTLE.PRG/44F14.c:57)
extern ByteTable<kDrawnWeaponSoundIdsBytes> drawnWeaponSoundIds;
// decomp: D_800E9A28 (src/BATTLE/BATTLE.PRG/44F14.c:59)
extern AnimationEventPoints animationEventPoints;
// decomp: D_800F244F (src/BATTLE/BATTLE.PRG/30DB0.c:106)
extern ByteTable<kTexturePagesOneBasedBytes> sequenceTexturePagesOneBased;
// decomp: D_800F2450 (src/BATTLE/BATTLE.PRG/30DB0.c:119)
extern ByteTable<kBankTexturePagesBytes> sharedSequenceBankTexturePages;
// decomp: D_800F4528 (src/BATTLE/BATTLE.PRG/30DB0.c:109)
extern std::array<void*, kSharedSequenceBankSlots> sharedSequenceData;
// decomp: D_800F4628 (src/BATTLE/BATTLE.PRG/30DB0.c:113)
extern SharedSequenceTable<std::int16_t> sharedSequenceAnimationIds;
// decomp: D_800F4768 (src/BATTLE/BATTLE.PRG/30DB0.h:44)
extern void* sequenceWorkBuffer;
// decomp: D_800F4810 (src/BATTLE/BATTLE.PRG/30DB0.c:114)
extern SharedSequenceTable<char*> sharedSequenceAnimationData;
// decomp: D_800F4930 (src/BATTLE/BATTLE.PRG/30DB0.h:45)
extern void* primarySequenceBuffer;
// decomp: D_800F4938 (src/BATTLE/BATTLE.PRG/30DB0.c:115)
extern SharedSequenceTable<std::int16_t> sharedSequenceEntryParams;
// decomp: D_800F49DC (src/BATTLE/BATTLE.PRG/battleUnk.h:36)
extern char lastResolvedSequenceIndex;

}  // namespace VS::Actors
