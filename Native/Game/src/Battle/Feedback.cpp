// Battle feedback's state and caption tables (Feedback.hpp).
#include "Battle/Feedback.hpp"

#include "Render/UiPackets.hpp"

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::DVECTOR;
using VS::Core::Gte::SVECTOR;

using VS::Legacy::RECT;

namespace VS::Battle {
namespace {
using Render::PackScreenWord;

constexpr std::uint32_t kUvVShift = 8;

// decomp: vs_getUV (src/include/gpu.h:112)
constexpr std::uint16_t PackUv(std::uint32_t u, std::uint32_t v) noexcept {
  return static_cast<std::uint16_t>(u | (v << kUvVShift));
}
}  // namespace

// decomp: D_800E861C (D_800E861C@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::int32_t, kValueCaptionRectWords> valueCaptionRects;
// decomp: D_800E8634 (D_800E8634@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
LockedLabelHalfHeightTable lockedLabelHalfHeights;
// decomp: D_800E8640 (D_800E8640@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<RECT, kLockedLabelPieceCount> lockedLabelPieces;
// decomp: D_800F1BAC (D_800F1BAC@BATTLE/BATTLE.PRG)
FloatingLabelSlots* floatingLabels;
// decomp: D_800F1CBC (D_800F1CBC@BATTLE/BATTLE.PRG)
DVECTOR* placedLabelPositions;
// decomp: D_800F2260 (D_800F2260@BATTLE/BATTLE.PRG)
std::array<std::int8_t, kActorMarkerBytes> actorMarkers;
// decomp: D_800F2270 (D_800F2270@BATTLE/BATTLE.PRG)
std::int16_t actorMarkersHidden;
// decomp: D_800F2272 (D_800F2272@BATTLE/BATTLE.PRG)
std::int16_t abilityTimingCueFrames;
// decomp: D_800F2274 (D_800F2274@BATTLE/BATTLE.PRG)
std::int32_t actorMarkerSides;
// decomp: D_800F2278 (D_800F2278@BATTLE/BATTLE.PRG)
std::int16_t statusLabelStepTimer;
// decomp: D_800F227A (D_800F227A@BATTLE/BATTLE.PRG)
std::int16_t ashleyStatusLabelsSuppressed;
// decomp: D_800F227C (D_800F227C@BATTLE/BATTLE.PRG)
std::int16_t placedLabelCount;
// decomp: D_800F227E (D_800F227E@BATTLE/BATTLE.PRG)
std::int16_t floatingLabelCount;
// decomp: D_800F2280 (D_800F2280@BATTLE/BATTLE.PRG)
SVECTOR abilityTimingCuePosition;

// decomp: D_80068F04 (D_80068F04@src/BATTLE/BATTLE.PRG/2842C.c)
const MarkerFrameCornerTable kMarkerFrameCorners{{
    {PackScreenWord(-6, -22), PackScreenWord(18, -22), PackScreenWord(-6, 1),
     PackScreenWord(18, 1)},
    {PackScreenWord(-17, -22), PackScreenWord(7, -22), PackScreenWord(-17, 1),
     PackScreenWord(7, 1)},
    {PackScreenWord(-10, -29), PackScreenWord(20, -29), PackScreenWord(-10, 1),
     PackScreenWord(20, 1)},
    {PackScreenWord(-19, -29), PackScreenWord(9, -29), PackScreenWord(-19, 1),
     PackScreenWord(9, 1)},
    {PackScreenWord(-9, -16), PackScreenWord(11, -16), PackScreenWord(-9, -3),
     PackScreenWord(11, -3)},
    {PackScreenWord(-10, -16), PackScreenWord(10, -16), PackScreenWord(-10, -3),
     PackScreenWord(10, -3)},
}};

// decomp: D_80068F64 (D_80068F64@src/BATTLE/BATTLE.PRG/2842C.c)
const MarkerFrameTexCoordTable kMarkerFrameTexCoords{{
    {PackUv(64, 16), PackUv(88, 16), PackUv(64, 39), PackUv(88, 39)},
    {PackUv(87, 16), PackUv(63, 16), PackUv(87, 39), PackUv(63, 39)},
    {PackUv(121, 17), PackUv(151, 17), PackUv(121, 47), PackUv(151, 47)},
    {PackUv(150, 17), PackUv(120, 17), PackUv(150, 47), PackUv(120, 47)},
    {PackUv(160, 17), PackUv(180, 17), PackUv(160, 30), PackUv(180, 30)},
    {PackUv(179, 17), PackUv(159, 17), PackUv(179, 30), PackUv(159, 30)},
}};

// decomp: D_80068F94 (D_80068F94@src/BATTLE/BATTLE.PRG/2842C.c)
const std::array<std::int32_t, kMarkerFrameCount> kMarkerIconOffsets{{
    PackScreenWord(-2, -16),
    PackScreenWord(-14, -16),
    PackScreenWord(0, -21),
    PackScreenWord(-7, -21),
    PackScreenWord(3, -11),
    PackScreenWord(3, -11),
}};

// decomp: D_80068FAC (D_80068FAC@src/BATTLE/BATTLE.PRG/2842C.c)
const std::array<RECT, kLabelCaptionRectCount> kLabelCaptionRects{{
    {0, 64, 36, 12},    {0, 64, 24, 12},   {24, 64, 20, 12},  {44, 64, 24, 12},
    {224, 64, 16, 12},  {212, 76, 32, 12}, {150, 64, 26, 12}, {68, 64, 40, 12},
    {108, 64, 30, 12},  {138, 64, 50, 12}, {188, 64, 36, 12}, {0, 76, 52, 12},
    {52, 76, 30, 12},   {144, 88, 58, 12}, {120, 76, 60, 12}, {180, 76, 30, 12},
    {136, 88, 8, 12},   {104, 88, 32, 12}, {76, 88, 40, 12},  {0, 88, 16, 12},
    {20, 88, 20, 12},   {40, 88, 30, 12},  {72, 88, 32, 12},  {102, 76, 16, 12},
    {220, 100, 18, 12}, {152, 52, 26, 12}, {182, 40, 30, 12}, {214, 40, 24, 12},
    {152, 52, 28, 12},  {180, 52, 40, 12}, {220, 52, 30, 12}, {108, 64, 30, 12},
    {216, 28, 28, 12},
}};

// decomp: D_800690B4 (D_800690B4@BATTLE/BATTLE.PRG)
const StatusLabelLayoutTable kStatusLabelLayouts{{
    {1, {31, 0}, 0},  {2, {29, 27}, 0}, {2, {30, 27}, 0}, {2, {28, 26}, 0},
    {2, {32, 27}, 0}, {2, {1, 5}, 0},   {2, {1, 4}, 1},   {2, {2, 5}, 0},
    {2, {2, 4}, 1},   {2, {3, 5}, 0},   {2, {3, 4}, 1},   {1, {7, 0}, 2},
    {1, {8, 0}, 2},   {1, {9, 0}, 2},   {1, {10, 0}, 2},  {1, {11, 0}, 2},
    {1, {12, 0}, 2},  {1, {13, 0}, 3},  {1, {14, 0}, 3},  {2, {15, 5}, 0},
    {2, {15, 4}, 1},  {2, {16, 19}, 2}, {2, {16, 20}, 2}, {2, {16, 21}, 2},
    {2, {16, 22}, 2}, {2, {17, 19}, 2}, {2, {17, 20}, 2}, {2, {17, 21}, 2},
    {2, {17, 22}, 2}, {0, {0, 0}, 0},   {0, {0, 0}, 0},   {0, {0, 0}, 0},
}};

// decomp: D_80069134 (D_80069134@src/BATTLE/BATTLE.PRG/2842C.c)
const std::array<CVECTOR, kStatusLabelColorCount> kStatusLabelColors{{
    {208, 208, 208, 0},
    {64, 192, 255, 0},
    {255, 208, 48, 0},
    {160, 224, 96, 0},
}};

// decomp: D_80069144 (D_80069144@src/BATTLE/BATTLE.PRG/2842C.c)
const StatChangeFirstWordRectTable kStatChangeFirstWordRects{{
    {72, 160, 32, 8},
    {76, 168, 28, 8},
    {78, 176, 26, 8},
    {78, 184, 26, 8},
    {82, 192, 22, 8},
    {82, 200, 22, 8},
    {82, 208, 22, 8},
}};

// decomp: D_8006917C (D_8006917C@src/BATTLE/BATTLE.PRG/2842C.c)
const StatChangeSecondWordRectTable kStatChangeSecondWordRects{{
    {104, 160, 28, 8},
    {104, 168, 26, 8},
    {104, 176, 30, 8},
    {104, 184, 36, 8},
    {104, 192, 32, 8},
    {104, 200, 18, 8},
    {144, 160, 30, 8},
    {144, 168, 14, 8},
    {144, 176, 18, 8},
    {144, 184, 26, 8},
    {144, 192, 26, 8},
    {144, 200, 22, 8},
    {144, 208, 36, 8},
}};
}  // namespace VS::Battle
