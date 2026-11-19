// MENU5.PRG's area map statics and tables.
#include "Menu/Map.hpp"

#include <cstdint>

#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Field/Map.h"
#include "Legacy/Psx.h"
#include "Menu/MapDrawing.hpp"
#include "Menu/MapLoading.hpp"
#include "Menu/MapMarkers.hpp"

namespace VS::Menu::map {
// decomp: currentMapRow (src/MENU/MENU5.PRG/4D8.c:128)
std::int32_t currentMapRow = -1;

// decomp: sceneArmFiles (src/MENU/MENU5.PRG/4D8.c:380)
const Content_DiscFile kSceneArmFiles[kSceneCount] = {
    {VS_DUMMY_ARM_LBA, VS_DUMMY_ARM_SIZE},
    {VS_SCEN001_ARM_LBA, VS_SCEN001_ARM_SIZE},
    {VS_SCEN002_ARM_LBA, VS_SCEN002_ARM_SIZE},
    {VS_SCEN003_ARM_LBA, VS_SCEN003_ARM_SIZE},
    {VS_SCEN004_ARM_LBA, VS_SCEN004_ARM_SIZE},
    {VS_SCEN005_ARM_LBA, VS_SCEN005_ARM_SIZE},
    {VS_SCEN006_ARM_LBA, VS_SCEN006_ARM_SIZE},
    {VS_SCEN007_ARM_LBA, VS_SCEN007_ARM_SIZE},
    {VS_SCEN008_ARM_LBA, VS_SCEN008_ARM_SIZE},
    {VS_SCEN009_ARM_LBA, VS_SCEN009_ARM_SIZE},
    {VS_SCEN010_ARM_LBA, VS_SCEN010_ARM_SIZE},
    {VS_SCEN011_ARM_LBA, VS_SCEN011_ARM_SIZE},
    {VS_SCEN012_ARM_LBA, VS_SCEN012_ARM_SIZE},
    {VS_SCEN013_ARM_LBA, VS_SCEN013_ARM_SIZE},
    {VS_SCEN014_ARM_LBA, VS_SCEN014_ARM_SIZE},
    {VS_SCEN015_ARM_LBA, VS_SCEN015_ARM_SIZE},
    {VS_SCEN016_ARM_LBA, VS_SCEN016_ARM_SIZE},
    {VS_SCEN017_ARM_LBA, VS_SCEN017_ARM_SIZE},
    {VS_SCEN018_ARM_LBA, VS_SCEN018_ARM_SIZE},
    {VS_SCEN019_ARM_LBA, VS_SCEN019_ARM_SIZE},
    {VS_SCEN020_ARM_LBA, VS_SCEN020_ARM_SIZE},
    {VS_SCEN021_ARM_LBA, VS_SCEN021_ARM_SIZE},
    {VS_SCEN022_ARM_LBA, VS_SCEN022_ARM_SIZE},
    {VS_SCEN023_ARM_LBA, VS_SCEN023_ARM_SIZE},
    {VS_SCEN024_ARM_LBA, VS_SCEN024_ARM_SIZE},
    {VS_SCEN025_ARM_LBA, VS_SCEN025_ARM_SIZE},
    {VS_SCEN026_ARM_LBA, VS_SCEN026_ARM_SIZE},
    {VS_SCEN027_ARM_LBA, VS_SCEN027_ARM_SIZE},
    {VS_SCEN028_ARM_LBA, VS_SCEN028_ARM_SIZE},
    {VS_SCEN029_ARM_LBA, VS_SCEN029_ARM_SIZE},
    {VS_SCEN030_ARM_LBA, VS_SCEN030_ARM_SIZE},
    {VS_SCEN031_ARM_LBA, VS_SCEN031_ARM_SIZE},
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
Field_MapRoomName* roomNamesTable;
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
Content_DiscQueueSlot* sceneCdQueueSlot;
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
