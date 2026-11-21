#pragma once
// Battle targeting's state: a target preview record, the action category
// tables, the status strings and target info templates, and the target
// columns the target list lays out. TargetingState.cpp defines the globals;
// the module image fills the tables at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Core/Gte.hpp"
#include "Menu/Framework/TextBoxes.hpp"

namespace VS::Battle {

// One target candidate of the target list or the sphere's target lists.
// decomp: D_800F19CC_t5 (src/BATTLE/BATTLE.PRG/57B1C.h:25-38)
struct TargetPreview {
  VS::Core::Gte::SVECTOR position;
  std::uint8_t kind;
  std::uint8_t actor_id;
  std::int8_t limb;
  std::int8_t effect;
  std::int16_t preview_amount;
  std::int16_t hit_chance;
  std::int32_t removed_statuses;
  std::int8_t action_kind;
  std::int8_t damage_type;
  std::int8_t affinity;
  std::int8_t enemy_class;
};
inline constexpr std::size_t kTargetPreviewBytes = 0x18;
static_assert(sizeof(TargetPreview) == kTargetPreviewBytes,
              "TargetPreview must keep its PS1 layout");

// The bytes of the module image each table spans.
inline constexpr std::size_t kCategoryNeedsTargetSelectionBytes = 96;
inline constexpr std::size_t kCategoryIsAreaTargetBytes = 80;
inline constexpr std::size_t kCategoryIncludesActingActorBytes = 48;
inline constexpr std::size_t kCategoryTargetsBodyPartsBytes = 32;
inline constexpr std::size_t kCategoryTargetActorMaskBytes = 16;
inline constexpr std::size_t kStatusStringHalfwords = 1712;
inline constexpr std::size_t kTargetInfoTemplateBytes = 3140;
inline constexpr std::size_t kTargetStatusLineTemplateBytes = 3090;
inline constexpr std::size_t kEmptyStatusStringBytes = 2932;
inline constexpr std::size_t kTargetColumnTableCount = 2;
inline constexpr std::size_t kTargetColumnEntryByteCount = 16;
inline constexpr std::size_t kTargetColumnGroupByteCount = 8;

// clang-format wraps these aliases inside the namespace, which cpplint
// reads as indentation.
// NOLINTBEGIN
using TargetColumnEntryBytes =
    std::array<std::uint8_t, kTargetColumnEntryByteCount>;
using TargetColumnGroupBytes =
    std::array<std::uint8_t, kTargetColumnGroupByteCount>;
using CategoryNeedsTargetSelectionTable =
    std::array<std::int8_t, kCategoryNeedsTargetSelectionBytes>;
using CategoryIsAreaTargetTable =
    std::array<std::uint8_t, kCategoryIsAreaTargetBytes>;
using CategoryIncludesActingActorTable =
    std::array<std::uint8_t, kCategoryIncludesActingActorBytes>;
using CategoryTargetsBodyPartTable =
    std::array<std::uint8_t, kCategoryTargetsBodyPartsBytes>;
using CategoryTargetActorMaskTable =
    std::array<std::uint8_t, kCategoryTargetActorMaskBytes>;
using TargetStatusLineTemplateTable =
    std::array<char, kTargetStatusLineTemplateBytes>;
using TargetColumnEntryTable =
    std::array<TargetColumnEntryBytes, kTargetColumnTableCount>;
// NOLINTEND

// decomp: D_800E8184 (src/BATTLE/BATTLE.PRG/146C.c:291)
extern CategoryNeedsTargetSelectionTable categoryNeedsTargetSelection;
// decomp: D_800E8194 (src/BATTLE/BATTLE.PRG/146C.c:292)
extern CategoryIsAreaTargetTable categoryIsAreaTarget;
// decomp: D_800E81B4 (src/BATTLE/BATTLE.PRG/146C.c:293)
extern CategoryIncludesActingActorTable categoryIncludesActingActor;
// decomp: D_800E81C4 (src/BATTLE/BATTLE.PRG/146C.c:294)
extern CategoryTargetsBodyPartTable categoryTargetsBodyParts;
// decomp: D_800E81D4 (src/BATTLE/BATTLE.PRG/146C.c:295)
extern CategoryTargetActorMaskTable categoryTargetActorMasks;
// decomp: vs_battle_statusStrings (src/BATTLE/BATTLE.PRG/battleUi.h:271)
extern std::array<std::uint16_t, kStatusStringHalfwords> statusStrings;
// Module image entries inside vs_battle_statusStrings' block, which upstream
// does not declare separately.
// decomp: D_800EA984 (BATTLE.PRG module image)
extern std::array<char, kTargetInfoTemplateBytes> targetInfoTemplate;
// decomp: D_800EA9B6 (BATTLE.PRG module image)
extern TargetStatusLineTemplateTable targetStatusLineTemplate;
// decomp: D_800EAA54 (BATTLE.PRG module image)
extern std::array<char, kEmptyStatusStringBytes> emptyStatusString;
// decomp: D_800EABAA (BATTLE.PRG module image)
extern std::uint8_t targetSelectionStep;
// decomp: D_800F4C70 (src/BATTLE/BATTLE.PRG/5BF94.c:407)
extern TargetColumnEntryTable targetColumnEntries;
// decomp: D_800F4C90 (src/BATTLE/BATTLE.PRG/5BF94.c:410)
extern TargetColumnGroupBytes secondColumnGroups;
// decomp: D_800F4C98 (src/BATTLE/BATTLE.PRG/5BF94.c:411)
extern TargetColumnGroupBytes firstColumnGroups;
// decomp: D_800F4CA0 (src/BATTLE/BATTLE.PRG/5BF94.c:412)
extern TargetColumnGroupBytes secondColumnGroupFirstTargets;
// decomp: D_800F4CA8 (src/BATTLE/BATTLE.PRG/5BF94.c:413)
extern TargetColumnGroupBytes firstColumnGroupFirstTargets;
// decomp: D_800F4CB0 (src/BATTLE/BATTLE.PRG/5BF94.c:408)
extern std::uint8_t secondColumnEntryCount;
// decomp: D_800F4CB1 (src/BATTLE/BATTLE.PRG/5BF94.c:409)
extern std::uint8_t firstColumnEntryCount;
// decomp: D_800F4CB2 (src/BATTLE/BATTLE.PRG/5BF94.c:414)
extern std::uint8_t secondColumnGroupCount;
// decomp: D_800F4CB3 (src/BATTLE/BATTLE.PRG/5BF94.c:415)
extern std::uint8_t firstColumnGroupCount;
// decomp: D_800F4CB4 (src/BATTLE/BATTLE.PRG/5BF94Private.h:149)
extern std::uint8_t targetColumnTopOffset;
// decomp: vs_battle_stringContext, upstream vs_battle_printfArgs
// (src/BATTLE/BATTLE.PRG/battleUi.h:306)
extern VS::Menu::StringContext stringContext;

}  // namespace VS::Battle
