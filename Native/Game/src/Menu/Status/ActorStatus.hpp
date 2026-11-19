#pragma once
// The status screen's module statics (src/MENU/MENU4.PRG/120.c). The
// terminal, actor-switching, limb and detail controllers' Live hosts load them
// into their typed frames and store the results back. The original compiled
// its char statics unsigned (-funsigned-char).
#include <cstddef>
#include <cstdint>

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kStatusStringWords = 1760;

// decomp: statusStrings (src/MENU/MENU4.PRG/120.c:25): halfword offsets, then
// the strings; ModuleData.cpp fills it from the disc.
extern std::uint16_t statusStrings[kStatusStringWords];

// decomp: screenXPos (src/MENU/MENU4.PRG/120.c:515): the actor preview's yaw.
extern std::int32_t screenXPos;
// decomp: screenYPos (src/MENU/MENU4.PRG/120.c:516): the actor preview's
// pitch.
extern std::int32_t screenYPos;
// decomp: previousActor (src/MENU/MENU4.PRG/120.c:517)
extern std::int32_t previousActor;
// decomp: selectedActor (src/MENU/MENU4.PRG/120.c:518): the shown actor's
// slot plus one.
extern std::int32_t selectedActor;
// decomp: drawBackgroundFirst (src/MENU/MENU4.PRG/120.c:519)
extern std::int32_t drawBackgroundFirst;
// decomp: fadeScreen (src/MENU/MENU4.PRG/120.c:520)
extern std::int32_t fadeScreen;
// decomp: screenAnimationStep (src/MENU/MENU4.PRG/120.c:521)
extern std::int32_t screenAnimationStep;
// decomp: renderBasicStatsState (src/MENU/MENU4.PRG/120.c:522)
extern std::uint8_t renderBasicStatsState;
// decomp: basicStatsAnimState (src/MENU/MENU4.PRG/120.c:523)
extern std::uint8_t basicStatsAnimState;
// decomp: basicStatsAnimDelay (src/MENU/MENU4.PRG/120.c:524)
extern std::uint8_t basicStatsAnimDelay;
// decomp: initializedActor (src/MENU/MENU4.PRG/120.c:525)
extern std::uint8_t initializedActor;
// decomp: delayScreenUpdate (src/MENU/MENU4.PRG/120.c:571)
extern std::uint8_t delayScreenUpdate;
// decomp: selectedElement (src/MENU/MENU4.PRG/120.c:958)
extern std::uint8_t selectedElement;
// decomp: selectedStatRow (src/MENU/MENU4.PRG/120.c:2264)
extern std::uint8_t selectedStatRow;

// MENU4.PRG reloads restore 120.c's initialized statics (PORTING_WATCHLIST.md
// §1).
void ResetMenu4Statics();
}  // namespace VS::Menu::Status
