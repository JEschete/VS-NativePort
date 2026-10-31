// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_MAP_H_TYPES
#define VS_MENU_MAP_H_TYPES
#include "Legacy/Psx.h"

typedef struct Menu_Map_CompassBound Menu_Map_CompassBound;
typedef struct Menu_Map_MapLabel Menu_Map_MapLabel;

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\MENU\MENU5.PRG\4D8.c:1064:8) (src/MENU/MENU5.PRG/4D8.c:1064)
struct Menu_Map_CompassBound {
    int x;
    int y;
};
typedef struct Menu_Map_CompassBound Menu_Map_CompassBound;
_Static_assert(sizeof(Menu_Map_CompassBound) == 0x8, "Menu_Map_CompassBound must keep its PS1 layout");

// decomp: (unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src\MENU\MENU5.PRG\4D8.c:1069:8) (src/MENU/MENU5.PRG/4D8.c:1069)
struct Menu_Map_MapLabel {
    short x;
    short y;
    short type;
    short id;
};
typedef struct Menu_Map_MapLabel Menu_Map_MapLabel;
_Static_assert(sizeof(Menu_Map_MapLabel) == 0x8, "Menu_Map_MapLabel must keep its PS1 layout");

#endif // VS_MENU_MAP_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_MAP_H_DECLS)
#define VS_MENU_MAP_H_DECLS
#include "DiscQueue.h"
#include "Field/Map.h"

extern int const Menu_Map_unusedGlobal1[] __attribute__((unused));
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
void Menu_Map_DrawAshleyMarker(int id, int xy, int brightness, u_long* nextPrim);
void Menu_Map_ConfigureAreaList(int rowCount, int currentMap, char* menuText[]);
int Menu_Map_PollAreaSelection(void);
void Menu_Map_ShowAreaName(char* mapName, char fontColor);
void Menu_Map_PollMenuReady(void);
int Menu_Map_LoadBackground(int begin_background_load);
int Menu_Map_UpdateScreen(u_char* state);
int Menu_Map_LoadSceneMap(void);
int Menu_Map_UpdateMapMenu(void);
void Menu_Map_DrawFrame(void);
void Menu_Map_ApplyCamera(void);
int Menu_Map_UpdateAreaSelection(int initialize);
void Menu_Map_MarkAllRoomsVisited(int* data);
void Menu_Map_DrawVisitedRooms(Field_MapScene* scene);
int Menu_Map_DrawRoom(int roomIndex, void* roomData, int zone_id, int map_id);
void Menu_Map_CenterOnRoom(Field_MapScene* scene, int roomId);
void Menu_Map_SetRoomCenterTarget(Field_MapScene* scene, int roomId);
void Menu_Map_AnimateCentering(Field_MapScene* scene);
void Menu_Map_ScaleRoomVertices(Field_MapScene* scene, int factor);
void Menu_Map_DarkenBackground(void);
int Menu_Map_InitialRoomIndex(Field_MapScene* scene);
void Menu_Map_SelectAdjacentVisitedRoom(int searchForward);
void Menu_Map_DrawPalingDistortion(void);
void Menu_Map_DrawControlBackdrop(int x, int y, int w, int h);
void Menu_Map_DrawControls(void);
void Menu_Map_DrawFeatureMarker(int id, int x, int y);
void Menu_Map_DrawDoorStateMarker(int x, int y, int id);
void Menu_Map_DrawDoorLabel(int x, int y, int doorId);
void Menu_Map_AppendLabelLine(int x0, int y0, int x1, int y1);
void Menu_Map_DrawCompass(int x, int y, int angle);
void Menu_Map_ComputeCompassCorners(int compass_center_x, int compass_center_y, int compass_half_width, int compass_half_height, int compass_angle);
void Menu_Map_DrawConnectingAreaLabel(int x, int y, int mapId);
void Menu_Map_QueueLabel(int x, int y, int type, int labelId);
void Menu_Map_DrawLabels(void);
int Menu_Map_ShouldDrawDoorStateMarker(int door_state_id, int zone_id, int map_id);
void Menu_Map_ResetMenu5Statics(void);

#endif // VS_MENU_MAP_H_DECLS
