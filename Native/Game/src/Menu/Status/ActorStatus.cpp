#include "Menu/Status/ActorStatus.hpp"

#include <cstdint>

namespace VS::Menu::Status {  // NOLINT(readability-identifier-naming)
namespace {
// 120.c:515-525 and :571 initialize the screen's statics; selectedElement and
// selectedStatRow have no initializer and keep their values.
constexpr std::int32_t kInitialScreenPos = 0;
constexpr std::int32_t kInitialActor = 0;
constexpr std::int32_t kInitialFlag = 0;
constexpr std::int32_t kInitialAnimationStep = 0;
constexpr std::uint8_t kInitialByte = 0;
}  // namespace

// decomp: statusStrings (src/MENU/MENU4.PRG/120.c:25): filled from the disc
// (ModuleData.cpp).
std::uint16_t statusStrings[kStatusStringWords];

// decomp: screenXPos (src/MENU/MENU4.PRG/120.c:515)
std::int32_t screenXPos = kInitialScreenPos;
// decomp: screenYPos (src/MENU/MENU4.PRG/120.c:516)
std::int32_t screenYPos = kInitialScreenPos;
// decomp: previousActor (src/MENU/MENU4.PRG/120.c:517)
std::int32_t previousActor = kInitialActor;
// decomp: selectedActor (src/MENU/MENU4.PRG/120.c:518)
std::int32_t selectedActor = kInitialActor;
// decomp: drawBackgroundFirst (src/MENU/MENU4.PRG/120.c:519)
std::int32_t drawBackgroundFirst = kInitialFlag;
// decomp: fadeScreen (src/MENU/MENU4.PRG/120.c:520)
std::int32_t fadeScreen = kInitialFlag;
// decomp: screenAnimationStep (src/MENU/MENU4.PRG/120.c:521)
std::int32_t screenAnimationStep = kInitialAnimationStep;
// decomp: renderBasicStatsState (src/MENU/MENU4.PRG/120.c:522)
std::uint8_t renderBasicStatsState = kInitialByte;
// decomp: basicStatsAnimState (src/MENU/MENU4.PRG/120.c:523)
std::uint8_t basicStatsAnimState = kInitialByte;
// decomp: basicStatsAnimDelay (src/MENU/MENU4.PRG/120.c:524)
std::uint8_t basicStatsAnimDelay = kInitialByte;
// decomp: initializedActor (src/MENU/MENU4.PRG/120.c:525)
std::uint8_t initializedActor = kInitialByte;
// decomp: delayScreenUpdate (src/MENU/MENU4.PRG/120.c:571)
std::uint8_t delayScreenUpdate = kInitialByte;
// decomp: selectedElement (src/MENU/MENU4.PRG/120.c:958)
std::uint8_t selectedElement;
// decomp: selectedStatRow (src/MENU/MENU4.PRG/120.c:2264)
std::uint8_t selectedStatRow;

void ResetMenu4Statics() {
  screenXPos = kInitialScreenPos;
  screenYPos = kInitialScreenPos;
  previousActor = kInitialActor;
  selectedActor = kInitialActor;
  drawBackgroundFirst = kInitialFlag;
  fadeScreen = kInitialFlag;
  screenAnimationStep = kInitialAnimationStep;
  renderBasicStatsState = kInitialByte;
  basicStatsAnimState = kInitialByte;
  basicStatsAnimDelay = kInitialByte;
  initializedActor = kInitialByte;
  delayScreenUpdate = kInitialByte;
}
}  // namespace VS::Menu::Status
