#pragma once
// Battle feedback: the floating label queue and its placement, the actor
// markers, the ability timing cue, and the caption and marker atlas tables
// their drawing reads. Feedback.cpp defines them; the module image fills the
// caption tables at start-up.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/FloatingLabelPlacement.hpp"
#include "Battle/ValueLabels.hpp"
#include "Core/Gte.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Battle {

inline constexpr std::size_t kValueCaptionRectWords = 17;
inline constexpr std::size_t kLockedLabelHalfHeightCount = 22;
inline constexpr std::size_t kLockedLabelPieceCount = 4;
inline constexpr std::size_t kActorMarkerBytes = 40;
inline constexpr std::size_t kMarkerFrameCount = 6;
inline constexpr std::size_t kMarkerFrameCornerCount = 4;
inline constexpr std::size_t kLabelCaptionRectCount = 33;
inline constexpr std::size_t kStatusLabelLayoutCount = 32;
inline constexpr std::size_t kStatusLabelColorCount = 4;
inline constexpr std::size_t kStatChangeFirstWordCount = 7;
inline constexpr std::size_t kStatChangeSecondWordCount = 13;

using MarkerFrameCorners = std::array<std::int32_t, kMarkerFrameCornerCount>;
using MarkerFrameTexCoords = std::array<std::uint16_t, kMarkerFrameCornerCount>;

// clang-format wraps these aliases inside the namespace, which cpplint
// reads as indentation.
// NOLINTBEGIN
using LockedLabelHalfHeightTable =
    std::array<std::int16_t, kLockedLabelHalfHeightCount>;
using MarkerFrameCornerTable =
    std::array<MarkerFrameCorners, kMarkerFrameCount>;
using MarkerFrameTexCoordTable =
    std::array<MarkerFrameTexCoords, kMarkerFrameCount>;
using StatusLabelLayoutTable =
    std::array<StatusLabelLayout, kStatusLabelLayoutCount>;
using StatChangeFirstWordRectTable =
    std::array<VS::Legacy::RECT, kStatChangeFirstWordCount>;
using StatChangeSecondWordRectTable =
    std::array<VS::Legacy::RECT, kStatChangeSecondWordCount>;
// NOLINTEND

// decomp: D_800E861C (src/BATTLE/BATTLE.PRG/2842C.c:230)
extern std::array<std::int32_t, kValueCaptionRectWords> valueCaptionRects;
// decomp: D_800E8634 (src/BATTLE/BATTLE.PRG/2842C.c:231)
extern LockedLabelHalfHeightTable lockedLabelHalfHeights;
// decomp: D_800E8640 (src/BATTLE/BATTLE.PRG/2842C.c:232)
extern std::array<VS::Legacy::RECT, kLockedLabelPieceCount> lockedLabelPieces;
// decomp: D_800F1BAC (src/BATTLE/BATTLE.PRG/146C.h:61)
extern FloatingLabelSlots* floatingLabels;
// decomp: D_800F1CBC (src/BATTLE/BATTLE.PRG/146C.h:51)
extern VS::Core::Gte::DVECTOR* placedLabelPositions;
// decomp: D_800F2260 (src/BATTLE/BATTLE.PRG/2842C.c:239)
extern std::array<std::int8_t, kActorMarkerBytes> actorMarkers;
// decomp: D_800F2270 (src/BATTLE/BATTLE.PRG/2842C.c:240)
extern std::int16_t actorMarkersHidden;
// decomp: D_800F2272 (src/BATTLE/BATTLE.PRG/2842C.c:241)
extern std::int16_t abilityTimingCueFrames;
// decomp: D_800F2274 (src/BATTLE/BATTLE.PRG/2842C.c:242)
extern std::int32_t actorMarkerSides;
// decomp: D_800F2278 (src/BATTLE/BATTLE.PRG/2842C.c:243)
extern std::int16_t statusLabelStepTimer;
// decomp: D_800F227A (src/BATTLE/BATTLE.PRG/2842C.c:244)
extern std::int16_t ashleyStatusLabelsSuppressed;
// decomp: D_800F227C (src/BATTLE/BATTLE.PRG/2842C.c:245)
extern std::int16_t placedLabelCount;
// decomp: D_800F227E (src/BATTLE/BATTLE.PRG/2842C.c:246)
extern std::int16_t floatingLabelCount;
// decomp: D_800F2280 (src/BATTLE/BATTLE.PRG/2842C.c:247)
extern VS::Core::Gte::SVECTOR abilityTimingCuePosition;

// decomp: D_80068F04 (src/BATTLE/BATTLE.PRG/2842C.c:1477)
extern const MarkerFrameCornerTable kMarkerFrameCorners;
// decomp: D_80068F64 (src/BATTLE/BATTLE.PRG/2842C.c:1485)
extern const MarkerFrameTexCoordTable kMarkerFrameTexCoords;
// decomp: D_80068F94 (src/BATTLE/BATTLE.PRG/2842C.c:1493)
extern const std::array<std::int32_t, kMarkerFrameCount> kMarkerIconOffsets;
// decomp: D_80068FAC (src/BATTLE/BATTLE.PRG/2842C.c:1496)
extern const std::array<VS::Legacy::RECT, kLabelCaptionRectCount>
    kLabelCaptionRects;  // NOLINT(whitespace/indent_namespace)
// decomp: D_800690B4 (src/BATTLE/BATTLE.PRG/2842C.c:1505-1518)
extern const StatusLabelLayoutTable kStatusLabelLayouts;
// decomp: D_80069134 (src/BATTLE/BATTLE.PRG/2842C.c:1520)
extern const std::array<VS::Core::Gte::CVECTOR, kStatusLabelColorCount>
    kStatusLabelColors;  // NOLINT(whitespace/indent_namespace)
// decomp: D_80069144 (src/BATTLE/BATTLE.PRG/2842C.c:1523)
extern const StatChangeFirstWordRectTable kStatChangeFirstWordRects;
// decomp: D_8006917C (src/BATTLE/BATTLE.PRG/2842C.c:1527)
extern const StatChangeSecondWordRectTable kStatChangeSecondWordRects;

}  // namespace VS::Battle
