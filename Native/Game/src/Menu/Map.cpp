// MENU5.PRG's area map statics and tables.
#include "Menu/Map.hpp"

#include <cstdint>

#include "Content/DiscLayout.hpp"
#include "Content/DiscQueue.hpp"
#include "Field/MinimapScene.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/MapDrawing.hpp"
#include "Menu/MapLoading.hpp"
#include "Menu/MapMarkers.hpp"

using VS::Content::disc::LocateAs;

using VS::Core::Gte::SVECTOR;

namespace VS::Menu::map {
// decomp: currentMapRow (src/MENU/MENU5.PRG/4D8.c:128)
std::int32_t currentMapRow = -1;

// decomp: sceneArmFiles (src/MENU/MENU5.PRG/4D8.c:380)
const VS::Content::DiscFile kSceneArmFiles[kSceneCount] = {
    LocateAs<VS::Content::DiscFile>("SMALL/DUMMY.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN001.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN002.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN003.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN004.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN005.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN006.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN007.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN008.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN009.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN010.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN011.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN012.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN013.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN014.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN015.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN016.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN017.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN018.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN019.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN020.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN021.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN022.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN023.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN024.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN025.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN026.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN027.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN028.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN029.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN030.ARM"),
    LocateAs<VS::Content::DiscFile>("SMALL/SCEN031.ARM"),
};

// decomp: mapNames (src/MENU/MENU5.PRG/4D8.c:421); contents: the module image
// (ModuleData.cpp).
std::uint16_t mapNames[kMapNameWords];
// decomp: menuState (src/MENU/MENU5.PRG/4D8.c:425)
std::int32_t menuState;
// decomp: currentRoomIndex (src/MENU/MENU5.PRG/4D8.c:426)
std::int32_t currentRoomIndex;
// decomp: backgroundBrightness (src/MENU/MENU5.PRG/4D8.c:427)
std::int32_t backgroundBrightness;
// decomp: menuFramesCounter (src/MENU/MENU5.PRG/4D8.c:428)
std::int32_t menuFramesCounter;
// decomp: geomOffsetX (src/MENU/MENU5.PRG/4D8.c:429)
std::int32_t geomOffsetX;
// decomp: geomOffsetY (src/MENU/MENU5.PRG/4D8.c:430)
std::int32_t geomOffsetY;
// decomp: isCurrentScene (src/MENU/MENU5.PRG/4D8.c:431)
std::int32_t isCurrentScene;
// decomp: cameraDistance (src/MENU/MENU5.PRG/4D8.c:432)
std::int32_t cameraDistance;
// decomp: currentScene (src/MENU/MENU5.PRG/4D8.c:433)
std::int32_t currentScene;
// decomp: chosenMap (src/MENU/MENU5.PRG/4D8.c:434)
std::int32_t chosenMap;
// decomp: isPaling (src/MENU/MENU5.PRG/4D8.c:435)
std::int32_t isPaling;
// decomp: triggerHideUi (src/MENU/MENU5.PRG/4D8.c:436)
std::int32_t triggerHideUi;
// decomp: currentSceneRow (src/MENU/MENU5.PRG/4D8.c:437)
std::int32_t currentSceneRow;
// decomp: hideUi (src/MENU/MENU5.PRG/4D8.c:438)
std::int32_t hideUi;
// decomp: roomNamesTable (src/MENU/MENU5.PRG/4D8.c:439)
Field::MapRoomName* roomNamesTable;
// decomp: menuCommandsState (src/MENU/MENU5.PRG/4D8.c:440)
std::int16_t menuCommandsState;
// decomp: uiAnimationState (src/MENU/MENU5.PRG/4D8.c:441)
std::int16_t uiAnimationState;
// decomp: cameraAngles (src/MENU/MENU5.PRG/4D8.c:443)
SVECTOR cameraAngles;
// decomp: cameraPos (src/MENU/MENU5.PRG/4D8.c:444)
SVECTOR cameraPos;
// decomp: centerPoint (src/MENU/MENU5.PRG/4D8.c:445)
SVECTOR centerPoint;
// decomp: sceneCdQueueSlot (src/MENU/MENU5.PRG/4D8.c:446)
VS::Content::DiscQueueSlot* sceneCdQueueSlot;
// decomp: armFileBuff (src/MENU/MENU5.PRG/4D8.c:447)
void* armFileBuff;
// decomp: sceneRowIdMap (src/MENU/MENU5.PRG/4D8.c:448)
std::int32_t sceneRowIdMap[kSceneCount];
// decomp: quitState (src/MENU/MENU5.PRG/4D8.c:449)
std::int32_t quitState;
// decomp: mapLabelY (src/MENU/MENU5.PRG/4D8.c:450)
std::int32_t mapLabelY;
// decomp: connectingMaps (src/MENU/MENU5.PRG/4D8.c:902); contents: the module
// image (ModuleData.cpp).
std::uint16_t connectingMaps[kConnectingMapWords];
// decomp: doors (src/MENU/MENU5.PRG/4D8.c:905); contents: the module image
// (ModuleData.cpp).
std::uint16_t doors[kDoorNameWords];
// decomp: compassBounds (src/MENU/MENU5.PRG/4D8.c:1069)
CompassPoint compassBounds[kQuadCorners];
// decomp: mapLabels (src/MENU/MENU5.PRG/4D8.c:1074)
MapLabel mapLabels[kMapLabelCapacity];
// decomp: mapLabelCount (src/MENU/MENU5.PRG/4D8.c:1081)
std::int32_t mapLabelCount;

void ResetMenu5Statics() { currentMapRow = -1; }
}  // namespace VS::Menu::map
