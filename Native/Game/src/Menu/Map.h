// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_MAP_H_TYPES
#define VS_MENU_MAP_H_TYPES
#include "Legacy/Psx.h"

struct Menu_Map_CompassBound;
struct Menu_Map_MapLabel;

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\MENU\MENU5.PRG\4D8.c:1064:8) (src/MENU/MENU5.PRG/4D8.c:1064)
struct Menu_Map_CompassBound {
    int x;
    int y;
};
struct Menu_Map_CompassBound;
static_assert(sizeof(Menu_Map_CompassBound) == 0x8, "Menu_Map_CompassBound must keep its PS1 layout");

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\MENU\MENU5.PRG\4D8.c:1069:8) (src/MENU/MENU5.PRG/4D8.c:1069)
struct Menu_Map_MapLabel {
    short x;
    short y;
    short type;
    short id;
};
struct Menu_Map_MapLabel;
static_assert(sizeof(Menu_Map_MapLabel) == 0x8, "Menu_Map_MapLabel must keep its PS1 layout");

#endif // VS_MENU_MAP_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_MAP_H_DECLS)
#define VS_MENU_MAP_H_DECLS
#include "DiscQueue.h"
#include "Field/Map.h"

extern int const Menu_Map_unusedGlobal1[];
extern RECT Menu_Map_kDoorMarkerUpperRects[];
extern RECT Menu_Map_kDoorMarkerLowerRects[];
extern short Menu_Map_layoutPadding1[2];;
extern int Menu_Map_layoutPadding2[2];;
extern void* Menu_Map_armFileBuff;;
extern int const Menu_Map_kAshleyIconBrightness[];
extern int Menu_Map_backgroundBrightness;;
extern SVECTOR Menu_Map_cameraAngles;;
extern int Menu_Map_cameraDistance;;
extern SVECTOR Menu_Map_cameraPos;;
extern SVECTOR Menu_Map_centerPoint;;
extern struct Menu_Map_CompassBound Menu_Map_compassBounds[4];;
extern u_short Menu_Map_connectingMaps[320]; /* contents: disc string table (ModuleData.cpp) */;
extern int Menu_Map_currentMapRow;
extern int Menu_Map_currentRoomIndex;;
extern int Menu_Map_currentScene;;
extern int Menu_Map_currentSceneRow;;
extern u_short Menu_Map_doors[522]; /* contents: disc string table (ModuleData.cpp) */;
extern int Menu_Map_geomOffsetX;;
extern int Menu_Map_geomOffsetY;;
extern int Menu_Map_hideUi;;
extern RECT Menu_Map_iconRects[];
extern int Menu_Map_isCurrentScene;;
extern int Menu_Map_isPaling;;
extern int Menu_Map_mapLabelCount;;
extern int Menu_Map_mapLabelY;;
extern struct Menu_Map_MapLabel Menu_Map_mapLabels[8];;
extern u_short Menu_Map_mapNames[282]; /* contents: disc string table (ModuleData.cpp) */;
extern short Menu_Map_menuCommandsState;;
extern int Menu_Map_menuFramesCounter;;
extern int Menu_Map_menuState;;
extern int Menu_Map_quitState;;
extern Field_MapRoomName* Menu_Map_roomNamesTable;;
extern Content_DiscFile const Menu_Map_kSceneArmFiles[];
extern Content_DiscQueueSlot* Menu_Map_sceneCdQueueSlot;;
extern int Menu_Map_sceneRowIdMap[32];;
extern int Menu_Map_selectedMap;;
extern int Menu_Map_triggerHideUi;;
extern short Menu_Map_uiAnimationState;;
void Menu_Map_ResetMenu5Statics();

#endif // VS_MENU_MAP_H_DECLS
