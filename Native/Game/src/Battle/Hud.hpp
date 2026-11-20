#pragma once
// The battle HUD's state and its sprite tables: the limb names, the HUD's
// hide and slide state, the battle mode indicator, the status icon carousel,
// the gauge, status icon, shortcut and limb display tables, and the puzzle
// timer's text. Hud.cpp defines them; the module image fills the tables at
// start-up.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/HudOrchestration.hpp"

namespace VS::Battle {

inline constexpr std::size_t kStatusIconCarouselSlots = 4;

// The status icon carousel: four icon slots, also read as one word.
// decomp: D_800EB9D0 (src/BATTLE/BATTLE.PRG/5BF94.c:362, declared u_char[])
union HudHighlightWord {
  std::int32_t word;
  std::array<std::uint8_t, kStatusIconCarouselSlots> bytes;
};
inline constexpr std::size_t kHudHighlightWordBytes = 0x4;
static_assert(sizeof(HudHighlightWord) == kHudHighlightWordBytes,
              "HudHighlightWord must keep its PS1 layout");

inline constexpr std::size_t kLimbNameCount = 20;
inline constexpr std::size_t kHudGaugeLabelTexUBytes = 20;
inline constexpr std::size_t kStatusIconTexOffsetCount = 32;
inline constexpr std::size_t kLimbStatusColorCount = 10;
inline constexpr std::size_t kUpperStatusIconFlagCount = 10;
// The elements of the module image each shortcut and limb table spans.
inline constexpr std::size_t kFirstShortcutDigitOffsetCount = 58;
inline constexpr std::size_t kSecondShortcutMarkerPositionBytes = 216;
inline constexpr std::size_t kSecondShortcutIconPositionBytes = 208;
inline constexpr std::size_t kSecondShortcutIconSizeCount = 50;
inline constexpr std::size_t kSecondShortcutIconTexCoordCount = 92;
inline constexpr std::size_t kFirstShortcutMarkerPositionBytes = 176;
inline constexpr std::size_t kFirstShortcutIconPositionBytes = 168;
inline constexpr std::size_t kFirstShortcutIconSizeCount = 40;
inline constexpr std::size_t kFirstShortcutIconTexCoordCount = 72;
inline constexpr std::size_t kLimbDisplaySpriteOffsetCount = 33;
inline constexpr std::size_t kLimbDisplaySpriteSizeCount = 23;
inline constexpr std::size_t kLimbDisplaySpriteTexCoordCount = 13;
// "00:00:00" and its terminator.

// clang-format wraps these aliases inside the namespace, which cpplint
// reads as indentation.
// NOLINTBEGIN
using StatusIconTexOffsetTable =
    std::array<std::uint16_t, kStatusIconTexOffsetCount>;
using UpperStatusIconFlagTable =
    std::array<std::uint16_t, kUpperStatusIconFlagCount>;
using FirstShortcutDigitOffsetTable =
    std::array<std::int32_t, kFirstShortcutDigitOffsetCount>;
using SecondShortcutMarkerPositionTable =
    std::array<std::uint8_t, kSecondShortcutMarkerPositionBytes>;
using SecondShortcutIconPositionTable =
    std::array<std::uint8_t, kSecondShortcutIconPositionBytes>;
using SecondShortcutIconSizeTable =
    std::array<std::int32_t, kSecondShortcutIconSizeCount>;
using SecondShortcutIconTexCoordTable =
    std::array<std::uint16_t, kSecondShortcutIconTexCoordCount>;
using FirstShortcutMarkerPositionTable =
    std::array<std::uint8_t, kFirstShortcutMarkerPositionBytes>;
using FirstShortcutIconPositionTable =
    std::array<std::uint8_t, kFirstShortcutIconPositionBytes>;
using FirstShortcutIconSizeTable =
    std::array<std::int32_t, kFirstShortcutIconSizeCount>;
using FirstShortcutIconTexCoordTable =
    std::array<std::uint16_t, kFirstShortcutIconTexCoordCount>;
using LimbDisplaySpriteOffsetTable =
    std::array<std::int32_t, kLimbDisplaySpriteOffsetCount>;
using LimbDisplaySpriteSizeTable =
    std::array<std::int32_t, kLimbDisplaySpriteSizeCount>;
using LimbDisplaySpriteTexCoordTable =
    std::array<std::uint32_t, kLimbDisplaySpriteTexCoordCount>;
// NOLINTEND

// decomp: vs_battle_limbNames (src/BATTLE/BATTLE.PRG/battleUi.h:269);
// relocated pointers (ModulePointers.cpp)
extern std::array<char*, kLimbNameCount> limbNames;
// decomp: D_800EB9AE (src/BATTLE/BATTLE.PRG/battleUi.h:272)
extern std::int8_t hudStatusIconsHidden;
// decomp: D_800EB9AF (src/BATTLE/BATTLE.PRG/battleUi.h:273)
extern std::int8_t hudHidden;
// decomp: D_800EB9B0 (src/BATTLE/BATTLE.PRG/battleUi.h:274)
extern std::int32_t hudSlideOffset;
// decomp: D_800EB9CC (src/BATTLE/BATTLE.PRG/5BF94.c:359)
extern std::int8_t hudBattleMode;
// decomp: D_800EB9CD (src/BATTLE/BATTLE.PRG/5BF94.c:360)
extern std::int8_t battleModeIndicatorStep;
// decomp: D_800EB9D0 (src/BATTLE/BATTLE.PRG/5BF94.c:362)
extern HudHighlightWord statusIconCarousel;
// decomp: D_800EBBDC (src/BATTLE/BATTLE.PRG/5BF94.c:368)
extern std::array<std::int32_t, kHudGaugeCount> hudGaugePositions;
// decomp: D_800EBBEC (src/BATTLE/BATTLE.PRG/battleUi.h:279)
extern std::array<std::int32_t, kHudGaugeCount> hudGaugeLabelPositions;
// decomp: D_800EBBFC (src/BATTLE/BATTLE.PRG/battleUi.h:280)
extern std::array<std::int8_t, kHudGaugeCount> hudGaugeLabelWidths;
// decomp: D_800EBC00 (src/BATTLE/BATTLE.PRG/battleUi.h:281)
extern std::array<std::int8_t, kHudGaugeLabelTexUBytes> hudGaugeLabelTexU;
// decomp: vs_battle_statusIconTexOffsets (src/BATTLE/BATTLE.PRG/battleUi.h:282)
extern StatusIconTexOffsetTable statusIconTexOffsets;
// decomp: D_800EBC54 (src/BATTLE/BATTLE.PRG/battleUi.h:283)
extern std::array<std::uint32_t, kLimbStatusColorCount> limbStatusColors;
// decomp: D_800EBC68 (src/BATTLE/BATTLE.PRG/5BF94.c:370)
extern UpperStatusIconFlagTable upperStatusIconFlags;
// decomp: D_800EBC78 (src/BATTLE/BATTLE.PRG/5BF94.c:371)
extern std::int8_t shortcutHudRequested;
// decomp: D_800EBC88 (src/BATTLE/BATTLE.PRG/5BF94.c:372)
extern FirstShortcutDigitOffsetTable firstShortcutDigitOffsets;
// decomp: D_800EBC98 (src/BATTLE/BATTLE.PRG/5BF94.c:373)
extern SecondShortcutMarkerPositionTable secondShortcutMarkerPositions;
// decomp: D_800EBCA0 (src/BATTLE/BATTLE.PRG/5BF94.c:374)
extern SecondShortcutIconPositionTable secondShortcutIconPositions;
// decomp: D_800EBCA8 (src/BATTLE/BATTLE.PRG/5BF94.c:375)
extern SecondShortcutIconSizeTable secondShortcutIconSizes;
// decomp: D_800EBCB8 (src/BATTLE/BATTLE.PRG/5BF94.c:376)
extern SecondShortcutIconTexCoordTable secondShortcutIconTexCoords;
// decomp: D_800EBCC0 (src/BATTLE/BATTLE.PRG/5BF94.c:377)
extern FirstShortcutMarkerPositionTable firstShortcutMarkerPositions;
// decomp: D_800EBCC8 (src/BATTLE/BATTLE.PRG/5BF94.c:378)
extern FirstShortcutIconPositionTable firstShortcutIconPositions;
// decomp: D_800EBCD0 (src/BATTLE/BATTLE.PRG/5BF94.c:379)
extern FirstShortcutIconSizeTable firstShortcutIconSizes;
// decomp: D_800EBCE0 (src/BATTLE/BATTLE.PRG/5BF94.c:380)
extern FirstShortcutIconTexCoordTable firstShortcutIconTexCoords;
// decomp: D_800EBCE8 (src/BATTLE/BATTLE.PRG/5BF94.c:381)
extern std::uint8_t shortcutHudAnimationStep;
// decomp: D_800EBCEC (src/BATTLE/BATTLE.PRG/5BF94.c:382)
extern LimbDisplaySpriteOffsetTable limbDisplaySpriteOffsets;
// decomp: D_800EBD14 (src/BATTLE/BATTLE.PRG/5BF94.c:383)
extern LimbDisplaySpriteSizeTable limbDisplaySpriteSizes;
// decomp: D_800EBD3C (src/BATTLE/BATTLE.PRG/5BF94.c:384)
extern LimbDisplaySpriteTexCoordTable limbDisplaySpriteTexCoords;
// decomp: D_800EBD64 (src/BATTLE/BATTLE.PRG/5BF94.c:385)
extern std::uint16_t battleModeIndicatorWobble;
// decomp: D_800EC258 (src/BATTLE/BATTLE.PRG/5BF94.c:392)
extern char* puzzleTimerTextPointer;
// decomp: vs_battle_lowerScreenUiState (src/BATTLE/BATTLE.PRG/battleUi.h:298)
extern std::int8_t lowerScreenUiState;
// decomp: D_80069998 (src/BATTLE/BATTLE.PRG/5BF94.c:3408)
extern std::array<char, kPuzzleTimerTextBytes> puzzleTimerText;

}  // namespace VS::Battle
