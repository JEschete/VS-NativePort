#pragma once
// MENU5.PRG's area map statics and tables (src/MENU/MENU5.PRG/4D8.c). The map
// terminal, loading, navigation, drawing and marker bindings (Map*Live.cpp)
// share them as the original's functions did; ModuleData.cpp fills the three
// name tables from the module image.
#include <array>
#include <cstddef>
#include <cstdint>

#include "DiscQueue.h"
#include "Field/MinimapScene.hpp"
#include "Legacy/Psx.h"
#include "Menu/MapDrawing.hpp"
#include "Menu/MapLoading.hpp"
#include "Menu/MapMarkers.hpp"

namespace VS::Menu::map {
// The module's string tables: halfword offsets, then the strings.
inline constexpr std::size_t kMapNameWords = 282;
inline constexpr std::size_t kConnectingMapWords = 320;
inline constexpr std::size_t kDoorNameWords = 522;

// decomp: currentMapRow (src/MENU/MENU5.PRG/4D8.c:128): -1 until an area row
// is chosen.
extern std::int32_t currentMapRow;
// decomp: sceneArmFiles (src/MENU/MENU5.PRG/4D8.c:380): each scene's ARM map.
extern const Content_DiscFile kSceneArmFiles[kSceneCount];
using AshleyBrightness = std::array<std::int32_t, kAshleyBrightnessSteps>;
// decomp: ashleyIconBrightness (src/MENU/MENU5.PRG/4D8.c:413): the position
// marker's pulse, one colour word per step.
inline constexpr AshleyBrightness kAshleyIconBrightness{
    0x00404040, 0x00505050, 0x00606060, 0x00707070, 0x00808080, 0x00808080,
    0x00808080, 0x00808080, 0x00808080, 0x00808080, 0x00808080, 0x00808080,
    0x00707070, 0x00606060, 0x00505050, 0x00404040,
};
// decomp: mapNames (src/MENU/MENU5.PRG/4D8.c:421)
extern std::uint16_t mapNames[kMapNameWords];
// decomp: menuState (src/MENU/MENU5.PRG/4D8.c:425)
extern std::int32_t menuState;
// decomp: currentRoomIndex (src/MENU/MENU5.PRG/4D8.c:426)
extern std::int32_t currentRoomIndex;
// decomp: backgroundBrightness (src/MENU/MENU5.PRG/4D8.c:427)
extern std::int32_t backgroundBrightness;
// decomp: menuFramesCounter (src/MENU/MENU5.PRG/4D8.c:428)
extern std::int32_t menuFramesCounter;
// decomp: geomOffsetX (src/MENU/MENU5.PRG/4D8.c:429)
extern std::int32_t geomOffsetX;
// decomp: geomOffsetY (src/MENU/MENU5.PRG/4D8.c:430)
extern std::int32_t geomOffsetY;
// decomp: isCurrentScene (src/MENU/MENU5.PRG/4D8.c:431)
extern std::int32_t isCurrentScene;
// decomp: cameraDistance (src/MENU/MENU5.PRG/4D8.c:432)
extern std::int32_t cameraDistance;
// decomp: currentScene (src/MENU/MENU5.PRG/4D8.c:433)
extern std::int32_t currentScene;
// decomp: chosenMap (src/MENU/MENU5.PRG/4D8.c:434)
extern std::int32_t chosenMap;
// decomp: isPaling (src/MENU/MENU5.PRG/4D8.c:435)
extern std::int32_t isPaling;
// decomp: triggerHideUi (src/MENU/MENU5.PRG/4D8.c:436)
extern std::int32_t triggerHideUi;
// decomp: currentSceneRow (src/MENU/MENU5.PRG/4D8.c:437)
extern std::int32_t currentSceneRow;
// decomp: hideUi (src/MENU/MENU5.PRG/4D8.c:438)
extern std::int32_t hideUi;
// decomp: roomNamesTable (src/MENU/MENU5.PRG/4D8.c:439)
extern Field::MapRoomName* roomNamesTable;
// decomp: menuCommandsState (src/MENU/MENU5.PRG/4D8.c:440)
extern std::int16_t menuCommandsState;
// decomp: uiAnimationState (src/MENU/MENU5.PRG/4D8.c:441)
extern std::int16_t uiAnimationState;
// decomp: cameraAngles (src/MENU/MENU5.PRG/4D8.c:443)
extern SVECTOR cameraAngles;
// decomp: cameraPos (src/MENU/MENU5.PRG/4D8.c:444)
extern SVECTOR cameraPos;
// decomp: centerPoint (src/MENU/MENU5.PRG/4D8.c:445)
extern SVECTOR centerPoint;
// decomp: sceneCdQueueSlot (src/MENU/MENU5.PRG/4D8.c:446)
extern Content_DiscQueueSlot* sceneCdQueueSlot;
// decomp: armFileBuff (src/MENU/MENU5.PRG/4D8.c:447)
extern void* armFileBuff;
// decomp: sceneRowIdMap (src/MENU/MENU5.PRG/4D8.c:448)
extern std::int32_t sceneRowIdMap[kSceneCount];
// decomp: quitState (src/MENU/MENU5.PRG/4D8.c:449)
extern std::int32_t quitState;
// decomp: mapLabelY (src/MENU/MENU5.PRG/4D8.c:450)
extern std::int32_t mapLabelY;
// decomp: connectingMaps (src/MENU/MENU5.PRG/4D8.c:902)
extern std::uint16_t connectingMaps[kConnectingMapWords];
// decomp: doors (src/MENU/MENU5.PRG/4D8.c:905)
extern std::uint16_t doors[kDoorNameWords];
// decomp: compassBounds (src/MENU/MENU5.PRG/4D8.c:1069): the compass
// needle's last corners.
extern CompassPoint compassBounds[kQuadCorners];
// decomp: mapLabels (src/MENU/MENU5.PRG/4D8.c:1074)
extern MapLabel mapLabels[kMapLabelCapacity];
// decomp: mapLabelCount (src/MENU/MENU5.PRG/4D8.c:1081)
extern std::int32_t mapLabelCount;
// decomp: iconRects (src/MENU/MENU5.PRG/4D8.c:2158): the feature icons'
// texture rectangles.
inline constexpr std::array<MarkerRect, 4> kIconRects{{
    {.x = 104, .y = 144, .w = 18, .h = 8},
    {.x = 144, .y = 216, .w = 44, .h = 7},
    {.x = 128, .y = 56, .w = 22, .h = 8},
    {.x = 218, .y = 144, .w = 36, .h = 8},
}};
// decomp: D_80108D04 (src/MENU/MENU5.PRG/4D8.c:2160): the door-state
// marker's upper rectangle for each of its two variants.
inline constexpr std::array<MarkerRect, 2> kDoorMarkerUpperRects{{
    {.x = 160, .y = 144, .w = 16, .h = 16},
    {.x = 176, .y = 144, .w = 16, .h = 16},
}};
// decomp: D_80108D14 (src/MENU/MENU5.PRG/4D8.c:2161): the door-state
// marker's lower rectangle for each of its two variants.
inline constexpr std::array<MarkerRect, 2> kDoorMarkerLowerRects{{
    {.x = 192, .y = 144, .w = 26, .h = 8},
    {.x = 216, .y = 160, .w = 39, .h = 8},
}};

// Restores currentMapRow's initial value; called where MENU5.PRG reloads
// (PORTING_WATCHLIST.md §1).
void ResetMenu5Statics();
}  // namespace VS::Menu::map
