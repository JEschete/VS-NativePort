// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Battle/TextBoxes.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Effects/Weather.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/MinimapScene.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoading.h"
#include "Field/RoomRendering.h"
#include "Field/Zone.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Map.h"
#include "Menu/TextBox.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/RoomDraw.h"
#include "Render/Sprites.h"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"
#include "State/Settings.h"
#include "State/StateFlags.h"

// decomp: D_80102C40 (D_80102C40@src/MENU/MENU5.PRG/4D8.c)
int const Menu_Map_unusedGlobal1[] __attribute__((unused)) = { 0x006B4100, 0x006C8219,
    0x00663040, 0x00203840, 0x00330500, 0x00262801, 0x00200808, 0x00081010 };

// decomp: D_80108D04 (D_80108D04@src/MENU/MENU5.PRG/4D8.c)
RECT Menu_Map_kDoorMarkerUpperRects[] = { { 160, 144, 16, 16 }, { 176, 144, 16, 16 } };

// decomp: D_80108D14 (D_80108D14@src/MENU/MENU5.PRG/4D8.c)
RECT Menu_Map_kDoorMarkerLowerRects[] = { { 192, 144, 26, 8 }, { 216, 160, 39, 8 } };

// decomp: _1 (_1@src/MENU/MENU5.PRG/4D8.c)
short Menu_Map_layoutPadding1[2];

// decomp: _2 (_2@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_layoutPadding2[2];

// decomp: _armFileBuff (_armFileBuff@src/MENU/MENU5.PRG/4D8.c)
void* Menu_Map_armFileBuff;

// decomp: _ashleyIconBrightness (_ashleyIconBrightness@src/MENU/MENU5.PRG/4D8.c)
int const Menu_Map_kAshleyIconBrightness[] = { 0x00404040, 0x00505050, 0x00606060,
    0x00707070, 0x00808080, 0x00808080, 0x00808080, 0x00808080, 0x00808080, 0x00808080,
    0x00808080, 0x00808080, 0x00707070, 0x00606060, 0x00505050, 0x00404040 };

// decomp: _backgroundBrightness (_backgroundBrightness@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_backgroundBrightness;

// decomp: _cameraAngles (_cameraAngles@src/MENU/MENU5.PRG/4D8.c)
SVECTOR Menu_Map_cameraAngles;

// decomp: _cameraDistance (_cameraDistance@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_cameraDistance;

// decomp: _cameraPos (_cameraPos@src/MENU/MENU5.PRG/4D8.c)
SVECTOR Menu_Map_cameraPos;

// decomp: _centerPoint (_centerPoint@src/MENU/MENU5.PRG/4D8.c)
SVECTOR Menu_Map_centerPoint;

// decomp: _compassBounds (_compassBounds@src/MENU/MENU5.PRG/4D8.c)
struct Menu_Map_CompassBound Menu_Map_compassBounds[4];

// decomp: _connectingMaps (_connectingMaps@src/MENU/MENU5.PRG/4D8.c)
u_short Menu_Map_connectingMaps[320]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _currentMapRow (_currentMapRow@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_currentMapRow = -1;

// decomp: _currentRoomIndex (_currentRoomIndex@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_currentRoomIndex;

// decomp: _currentScene (_currentScene@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_currentScene;

// decomp: _currentSceneRow (_currentSceneRow@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_currentSceneRow;

// decomp: _doors (_doors@src/MENU/MENU5.PRG/4D8.c)
u_short Menu_Map_doors[522]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _geomOffsetX (_geomOffsetX@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_geomOffsetX;

// decomp: _geomOffsetY (_geomOffsetY@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_geomOffsetY;

// decomp: _hideUi (_hideUi@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_hideUi;

// decomp: _iconRects (_iconRects@src/MENU/MENU5.PRG/4D8.c)
RECT Menu_Map_iconRects[] = { { 104, 144, 18, 8 }, { 144, 216, 44, 7 }, { 128, 56, 22, 8 },
    { 218, 144, 36, 8 } };

// decomp: _isCurrentScene (_isCurrentScene@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_isCurrentScene;

// decomp: _isPaling (_isPaling@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_isPaling;

// decomp: _mapLabelCount (_mapLabelCount@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_mapLabelCount;

// decomp: _mapLabelY (_mapLabelY@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_mapLabelY;

// decomp: _mapLabels (_mapLabels@src/MENU/MENU5.PRG/4D8.c)
struct Menu_Map_MapLabel Menu_Map_mapLabels[8];

// decomp: _mapNames (_mapNames@MENU/MENU5.PRG)
u_short Menu_Map_mapNames[282]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _menuCommandsState (_menuCommandsState@src/MENU/MENU5.PRG/4D8.c)
short Menu_Map_menuCommandsState;

// decomp: _menuFramesCounter (_menuFramesCounter@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_menuFramesCounter;

// decomp: _menuState (_menuState@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_menuState;

// decomp: local static of _renderScreen, at file scope so the module reset reaches it
static u_short Menu_Map_framePaling[] = {
0 /* disc text table: loaded in Step 5 */
    };

// decomp: _quitState (_quitState@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_quitState;

// decomp: _roomNamesTable (_roomNamesTable@src/MENU/MENU5.PRG/4D8.c)
Field_MapRoomName* Menu_Map_roomNamesTable;

// decomp: _sceneArmFiles (_sceneArmFiles@src/MENU/MENU5.PRG/4D8.c)
Content_DiscFile const Menu_Map_kSceneArmFiles[] = { { VS_DUMMY_ARM_LBA, VS_DUMMY_ARM_SIZE },
    { VS_SCEN001_ARM_LBA, VS_SCEN001_ARM_SIZE },
    { VS_SCEN002_ARM_LBA, VS_SCEN002_ARM_SIZE },
    { VS_SCEN003_ARM_LBA, VS_SCEN003_ARM_SIZE },
    { VS_SCEN004_ARM_LBA, VS_SCEN004_ARM_SIZE },
    { VS_SCEN005_ARM_LBA, VS_SCEN005_ARM_SIZE },
    { VS_SCEN006_ARM_LBA, VS_SCEN006_ARM_SIZE },
    { VS_SCEN007_ARM_LBA, VS_SCEN007_ARM_SIZE },
    { VS_SCEN008_ARM_LBA, VS_SCEN008_ARM_SIZE },
    { VS_SCEN009_ARM_LBA, VS_SCEN009_ARM_SIZE },
    { VS_SCEN010_ARM_LBA, VS_SCEN010_ARM_SIZE },
    { VS_SCEN011_ARM_LBA, VS_SCEN011_ARM_SIZE },
    { VS_SCEN012_ARM_LBA, VS_SCEN012_ARM_SIZE },
    { VS_SCEN013_ARM_LBA, VS_SCEN013_ARM_SIZE },
    { VS_SCEN014_ARM_LBA, VS_SCEN014_ARM_SIZE },
    { VS_SCEN015_ARM_LBA, VS_SCEN015_ARM_SIZE },
    { VS_SCEN016_ARM_LBA, VS_SCEN016_ARM_SIZE },
    { VS_SCEN017_ARM_LBA, VS_SCEN017_ARM_SIZE },
    { VS_SCEN018_ARM_LBA, VS_SCEN018_ARM_SIZE },
    { VS_SCEN019_ARM_LBA, VS_SCEN019_ARM_SIZE },
    { VS_SCEN020_ARM_LBA, VS_SCEN020_ARM_SIZE },
    { VS_SCEN021_ARM_LBA, VS_SCEN021_ARM_SIZE },
    { VS_SCEN022_ARM_LBA, VS_SCEN022_ARM_SIZE },
    { VS_SCEN023_ARM_LBA, VS_SCEN023_ARM_SIZE },
    { VS_SCEN024_ARM_LBA, VS_SCEN024_ARM_SIZE },
    { VS_SCEN025_ARM_LBA, VS_SCEN025_ARM_SIZE },
    { VS_SCEN026_ARM_LBA, VS_SCEN026_ARM_SIZE },
    { VS_SCEN027_ARM_LBA, VS_SCEN027_ARM_SIZE },
    { VS_SCEN028_ARM_LBA, VS_SCEN028_ARM_SIZE },
    { VS_SCEN029_ARM_LBA, VS_SCEN029_ARM_SIZE },
    { VS_SCEN030_ARM_LBA, VS_SCEN030_ARM_SIZE },
    { VS_SCEN031_ARM_LBA, VS_SCEN031_ARM_SIZE } };

// decomp: _sceneCdQueueSlot (_sceneCdQueueSlot@src/MENU/MENU5.PRG/4D8.c)
Content_DiscQueueSlot* Menu_Map_sceneCdQueueSlot;

// decomp: _sceneRowIdMap (_sceneRowIdMap@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_sceneRowIdMap[32];

// decomp: _selectedMap (_selectedMap@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_selectedMap;

// decomp: _triggerHideUi (_triggerHideUi@src/MENU/MENU5.PRG/4D8.c)
int Menu_Map_triggerHideUi;

// decomp: _uiAnimationState (_uiAnimationState@src/MENU/MENU5.PRG/4D8.c)
short Menu_Map_uiAnimationState;

// decomp: local static of vs_menu5_exec, at file scope so the module reset reaches it
static RECT Menu_Map_screenMenuBgBackRects[2];

// decomp: MENU5.PRG _renderDefaultIcon (src/MENU/MENU5.PRG/4D8.c:106)
void Menu_Map_DrawAshleyMarker(int id, int xy, int brightness, u_long* nextPrim)
{
    if (nextPrim == 0) {
        nextPrim = (u_long *)(Render_frameScratch.overlayOrderingTable);
    }

    Render_QueueSpriteWithTexturePage(128, xy, vs_getWH(16, 16), nextPrim)[4] =
        ((id + 4) << 4) | (getClut(976, 223) << 16) | 0x8000;

    ((u_long*)Render_frameScratch.primitiveCursor)[-4] = (int)(brightness | 0x64000000);
}

// decomp: MENU5.PRG _configureMapMenu (src/MENU/MENU5.PRG/4D8.c:129)
void Menu_Map_ConfigureAreaList(int rowCount, int currentMap, char* menuText[])
{
    int rowTypes[rowCount];
    int i;
    int cursorMemory;

    if (Menu_Map_currentMapRow == -1) {
        Menu_Map_currentMapRow = currentMap;
    }

    for (i = 0; i < rowCount; ++i) {
        rowTypes[i] = 0;
    }

    if (!(currentMap & 0x80)) {
        if (Menu_Map_currentMapRow < rowCount) {
            rowTypes[Menu_Map_currentMapRow] |= 4;
        }
    }

    currentMap &= 0x7F;

    if ((rowCount < 9) || (currentMap < 8)) {
        Menu_Framework_menuMemory.cursorMemories[0] = currentMap;
        Menu_Framework_menuMemory.cursorMemories[1] = 0;
    } else if (currentMap >= (rowCount - 8)) {
        Menu_Framework_menuMemory.cursorMemories[0] = currentMap - (rowCount - 9);
        Menu_Framework_menuMemory.cursorMemories[1] = rowCount - 9;
    } else {
        Menu_Framework_menuMemory.cursorMemories[0] = 4;
        Menu_Framework_menuMemory.cursorMemories[1] = currentMap - 4;
    }

    cursorMemory = Input_settings.cursorMemory;
    Input_settings.cursorMemory = 1;

    Menu_Framework_CreateMenuRows(rowCount, 256, menuText, rowTypes);

    Input_settings.cursorMemory = cursorMemory;
}

// decomp: MENU5.PRG _mapSelection (src/MENU/MENU5.PRG/4D8.c:175)
int Menu_Map_PollAreaSelection(void)
{
    int row = Menu_Framework_GetSelectedRow();

    if (row < -1) {
        Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(5));
    } else if (row >= 0) {
        Menu_Framework_FlyOutRowsAndHoistSelection(Menu_Framework_menuMemory.cursorMemories[0], 1);
    }

    return row;
}

// decomp: MENU5.PRG _setMenuItemMapName (src/MENU/MENU5.PRG/4D8.c:188)
void Menu_Map_ShowAreaName(char* mapName, char fontColor)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(10, 320, 34, 0x8C, 8, mapName);

    menuItem->state = 2;
    menuItem->targetPosition0 = 0xB4;
    menuItem->selected = 1;
    menuItem->fontColor = fontColor;
}

// decomp: MENU5.PRG _menuReady (src/MENU/MENU5.PRG/4D8.c:204)
void Menu_Map_PollMenuReady(void) { Menu_Framework_AreMenuRowsIdle(); }

// decomp: MENU5.PRG _loadMapBackground (src/MENU/MENU5.PRG/4D8.c:206)
int Menu_Map_LoadBackground(int begin_background_load)
{
    static Content_DiscQueueSlot* Menu_Map_loadBackgroundSlot;
    static u_short* Menu_Map_backgroundImage;
    static int Menu_Map_loadBackgroundState;

    if (begin_background_load != 0) {
        Content_DiscFile cdFile;
        cdFile.lba = VS_MAPBG_BIN_LBA;
        cdFile.size = VS_MAPBG_BIN_SIZE;
        Menu_Map_loadBackgroundSlot = Content_AllocateDiscRequest(&cdFile);
        Menu_Map_backgroundImage = (u_short *)(Core_Memory_AllocateFromHighEnd(VS_MAPBG_BIN_SIZE));
        Content_EnqueueDiscRequest(Menu_Map_loadBackgroundSlot, Menu_Map_backgroundImage);
        Menu_Map_loadBackgroundState = 0;
        return 0;
    }
    switch (Menu_Map_loadBackgroundState) {
    case 0:
        if (Menu_Map_loadBackgroundSlot->state == 4) {
            Content_ReleaseDiscRequest(Menu_Map_loadBackgroundSlot);
            Render_LoadClutColors(Menu_Map_backgroundImage, 3, 0, 256);
            Render_UploadImageRectangle(vs_getXY(640, 256), Menu_Map_backgroundImage + 256, vs_getWH(160, 240));
            Menu_Framework_SetBackgroundPriority(-4, 128);
            Menu_Map_loadBackgroundState = 1;
        }
        return 0;
    case 1:
        Core_Memory_FreeFromHighEnd(Menu_Map_backgroundImage);
        return 1;
    default:
        return 0;
    }
}

// decomp: MENU5.PRG vs_menu5_exec (src/MENU/MENU5.PRG/4D8.c:240)
int Menu_Map_UpdateScreen(u_char* state)
{
    enum Menu_Framework_InitOnlyState { Menu_Framework_InitOnlyState_Init = 3 };

    static int Menu_Map_screenArmfileLoadDelay;
    static int Menu_Map_screenArmFileLoaded;
    static int Menu_Map_menuResult;
    static u_long* Menu_Map_screenMenuBgBackup;
    static int Menu_Map_screenLayoutPadding __attribute__((unused));

    switch (*state) {
    case Menu_Framework_InitOnlyState_Init:
        Menu_Map_currentMapRow = -1;
        Menu_Map_screenMenuBgBackup = (u_long *)(Core_Memory_AllocateFromHighEnd(0xB600));
        Menu_Map_screenMenuBgBackRects[0].x = 768;
        Menu_Map_screenMenuBgBackRects[0].y = 227;
        Menu_Map_screenMenuBgBackRects[0].w = 256;
        Menu_Map_screenMenuBgBackRects[0].h = 1;
        (&Menu_Map_screenMenuBgBackRects[1])->x = 672;
        (&Menu_Map_screenMenuBgBackRects[1])->y = 256;
        (&Menu_Map_screenMenuBgBackRects[1])->w = 96;
        (&Menu_Map_screenMenuBgBackRects[1])->h = 240;

        StoreImage(&Menu_Map_screenMenuBgBackRects[0], Menu_Map_screenMenuBgBackup);
        StoreImage(&Menu_Map_screenMenuBgBackRects[1], Menu_Map_screenMenuBgBackup + 128);
        Menu_Map_LoadBackground(1);

        *state = 4;
        break;

    case 4:
        if (Menu_Map_LoadBackground(0) == 0) {
            break;
        }

        *state = 5;

        Menu_Framework_SetBackgroundFade(1);

        Menu_Map_screenArmfileLoadDelay = 8;
        Menu_Map_screenArmFileLoaded = 0;
        // Fallthrough

    case 5:
        if (Menu_Map_screenArmFileLoaded == 0) {
            Menu_Map_screenArmFileLoaded = Menu_Map_LoadSceneMap();
        }

        if (Menu_Map_screenArmfileLoadDelay != 0) {
            --Menu_Map_screenArmfileLoadDelay;
            break;
        } else if (Menu_Map_screenArmFileLoaded) {
            Field_SetFieldRenderingEnabled(0);
            Battle_SetHudHidden(1);

            Battle_hudSlideOffset = 0x200000;
            *state = 6;
        } else {
            break;
        }

        // Fallthrough

    case 6:
        *state += Menu_Framework_AreMenuRowsIdle();
        break;

    case 7:
        Menu_Map_menuResult = Menu_Map_UpdateMapMenu();

        if (Menu_Map_menuResult != 0) {
            Menu_Framework_SetBackgroundPriority(-4, 128);
            Battle_SetHudHidden(0);

            Battle_hudSlideOffset = 0;

            Field_SetFieldRenderingEnabled(1);
            Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

            Menu_Map_screenArmfileLoadDelay = 10;
            *state = 8;
        }

        break;

    case 8:
        Menu_Framework_SetBackgroundFade(0);
        *state = 9;
        break;

    case 9:
        if (Menu_Framework_backgroundFadeStep == 0) {

            Menu_Map_screenMenuBgBackRects[0].x = 640;
            Menu_Map_screenMenuBgBackRects[0].y = 256;
            Menu_Map_screenMenuBgBackRects[0].w = 32;
            Menu_Map_screenMenuBgBackRects[0].h = 240;

            (&Menu_Map_screenMenuBgBackRects[1])->x = 768;
            (&Menu_Map_screenMenuBgBackRects[1])->y = 256;
            (&Menu_Map_screenMenuBgBackRects[1])->w = 32;
            (&Menu_Map_screenMenuBgBackRects[1])->h = 240;

            ClearImage(Menu_Map_screenMenuBgBackRects, 0, 0, 0);
            ClearImage(&Menu_Map_screenMenuBgBackRects[1], 0, 0, 0);
            Render_LoadClutColors((u_short*)Menu_Map_screenMenuBgBackup, 3, 0, 0x100);
            Render_UploadImageRectangle(0x010002A0, Menu_Map_screenMenuBgBackup + 0x80, 0xF00060);

            *state = 10;
        }
        break;

    case 10:
        if (Menu_Map_menuResult == 2) {
            Menu_Framework_menuState.currentMenu = 6;
        }

        Core_Memory_FreeFromHighEnd(Menu_Map_screenMenuBgBackup);

        Menu_Framework_hideMenu = 0;
        *state = 0;
        return 1;
    }
    return 0;
}

// decomp: MENU5.PRG _loadArmFile (src/MENU/MENU5.PRG/4D8.c:448)
int Menu_Map_LoadSceneMap(void)
{

    Audio_Seq_unusedStateFlags.mapPaling[0] = 1;

    if (Battle_unusedSceneBuffer == NULL) {
        int paling;

        if (State_roomVisitProgress.roomFlags[1] & 0x800000) {
            State_roomVisitProgress.roomFlags[1] |= 0x400000;
        }

        Menu_Map_triggerHideUi = 0;
        Menu_Map_hideUi = 0;

        Core_Memory_ZeroAlignedWords(&Menu_Map_cameraAngles, sizeof Menu_Map_cameraAngles);
        Core_Memory_ZeroAlignedWords(&Menu_Map_cameraPos, sizeof Menu_Map_cameraPos);

        Menu_Map_cameraAngles.vx = 568;
        Menu_Map_currentRoomIndex = 0;
        Menu_Map_cameraAngles.vz = Menu_Map_cameraAngles.vy = 2048 - (u_short)Render_frameScratch.camera.angles.vy;

        Menu_Map_currentScene = Field_CurrentSceneId();

        if (Menu_Map_currentScene >= 32) {
            Menu_Map_currentScene = 0;
        }

        paling = Audio_Seq_unusedStateFlags.mapPaling[Menu_Map_currentScene];

        Menu_Map_currentSceneRow = Menu_Map_currentScene;
        Menu_Map_menuState = 0;
        Menu_Map_backgroundBrightness = 128;
        Menu_Map_menuFramesCounter = 0;
        Menu_Map_geomOffsetX = -384;
        Menu_Map_geomOffsetY = 0;
        Menu_Map_quitState = 1;
        Menu_Map_cameraDistance = ONE * 3 / 16;
        Menu_Map_menuCommandsState = 5;
        Menu_Map_uiAnimationState = 0;
        Menu_Map_isPaling = paling;

        Battle_unusedSceneBuffer = Core_Memory_AllocateFromHighEnd(Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);
        Menu_Map_sceneCdQueueSlot = Content_AllocateDiscRequest(&Menu_Map_kSceneArmFiles[Menu_Map_currentScene]);
        Content_EnqueueDiscRequest(Menu_Map_sceneCdQueueSlot, Battle_unusedSceneBuffer);

        return 0;
    }

    if (Menu_Map_sceneCdQueueSlot->state != Content_DiscRequestState_Loaded) {
        return 0;
    }

    Content_ReleaseDiscRequest(Menu_Map_sceneCdQueueSlot);

    Menu_Map_roomNamesTable = Field_ResolveSceneRoomData(Battle_unusedSceneBuffer, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);

    Field_RestoreSceneRoomVisits((Field_MapScene *)(Battle_unusedSceneBuffer));
    Menu_Map_ScaleRoomVertices((Field_MapScene *)(Battle_unusedSceneBuffer), sizeof(Field_MapScene));

    Menu_Map_currentRoomIndex = Menu_Map_InitialRoomIndex((Field_MapScene *)(Battle_unusedSceneBuffer));

    Menu_Map_CenterOnRoom((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);
    Core_Gte_SetFogNear(768, Render_projectionDistance);
    Core_Gte_SetFarColor(0, 0, 0);
    Menu_Map_ShowAreaName((char*)&Menu_Map_mapNames[Menu_Map_mapNames[Menu_Map_currentScene]], 1);

    return 1;
}

// decomp: MENU5.PRG _menuManager (src/MENU/MENU5.PRG/4D8.c:523)
int Menu_Map_UpdateMapMenu(void)
{
    enum Menu_Map_MapMenuState {
        Menu_Map_MapMenuState_Init,
        Menu_Map_MapMenuState_ProcessInput,
        Menu_Map_MapMenuState_DisplayMapMenu,
        Menu_Map_MapMenuState_Quit,
        Menu_Map_MapMenuState_InitNewMap,
        Menu_Map_MapMenuState_WaitForMapAnimation
    };

    int controls_state;
    int roomIndex;
    int area_selection_result;

    Core_Gte_SetFarColor(0, 0, 0);

    Menu_Map_mapLabelY = 188;
    roomIndex = Menu_Map_currentRoomIndex;

    if (Input_buttonsPressed.allButtons & PADselect) {
        Menu_Map_triggerHideUi ^= 1;
    }

    if (Audio_Seq_unusedStateFlags.mapPaling[Menu_Map_currentScene] != 0) {
        Menu_Framework_SetBackgroundPriority(2046, 128);
    } else {
        Menu_Framework_SetBackgroundPriority(2046, (Menu_Map_backgroundBrightness + 128) / 2);
    }

    if (Menu_Map_menuState != 1) {
        Menu_Map_menuCommandsState = 5;
    }

    switch (Menu_Map_menuState) {
    case Menu_Map_MapMenuState_Init:
        if (Menu_Map_backgroundBrightness > 32) {
            Menu_Map_backgroundBrightness -= 16;
            Menu_Map_geomOffsetX += 64;
        } else {
            Menu_InitTextBox(0, 0, 60, 224, 16, 1, 60, 224);
            Menu_SetTextBoxText(0, Menu_Map_roomNamesTable[Menu_Map_currentRoomIndex].name);
            ++Menu_Map_menuState;
        }

        Menu_Map_DrawFrame();
        break;

    case Menu_Map_MapMenuState_ProcessInput:
        controls_state = 0;

        if (Menu_Map_triggerHideUi != Menu_Map_hideUi) {
            Menu_Map_hideUi = Menu_Map_triggerHideUi;
            Menu_Framework_SetMenuVisible(Menu_Map_triggerHideUi == 0);
        }

        if (Input_buttonsPressed.allButtons & PADRright) {
            controls_state = 5;

            Audio_PlaySfxCentered(0x7E, 5);

            Menu_Map_menuState = Menu_Map_MapMenuState_DisplayMapMenu;

            if (Menu_Map_hideUi != 0) {
                Menu_Map_hideUi = 0;
                Menu_Framework_SetMenuVisible(1);
            }

            Menu_Map_UpdateAreaSelection(1);

        } else if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

            Audio_PlaySfxCentered(0x7E, 6);

            if (Input_buttonsPressed.allButtons & PADRup) {
                Menu_Map_quitState = 2;
            }

            Menu_Map_menuState = Menu_Map_MapMenuState_Quit;

            Menu_DismissTextBox(0);

        } else if (Audio_Seq_unusedStateFlags.mapPaling[Menu_Map_currentScene] != 0) {
            controls_state = 5;
        } else {
            int lStickX;
            int lStickY;

            if ((lStickX = Input_stickPosBuf.lStickX - 128, ABS(lStickX)) > 64
                || (lStickY = Input_stickPosBuf.lStickY - 128, ABS(lStickY) > 64)) {

                controls_state = 3;

                if ((Input_stickPosBuf.lStickX - 128) < -64) {
                    Menu_Map_cameraAngles.vy -= 64;
                } else if ((Input_stickPosBuf.lStickX - 128) > 64) {
                    Menu_Map_cameraAngles.vy += 64;
                }

                if ((Input_stickPosBuf.lStickY - 128) < -64) {
                    Menu_Map_cameraAngles.vx += 64;
                } else if ((Input_stickPosBuf.lStickY - 128) > 64) {
                    Menu_Map_cameraAngles.vx -= 64;
                }

                if (Menu_Map_cameraAngles.vx >= 0) {
                    if (Menu_Map_cameraAngles.vx > 1024) {
                        Menu_Map_cameraAngles.vx = 1024;
                    }
                } else {
                    Menu_Map_cameraAngles.vx = 0;
                }

            } else {
                if (Input_previousButtons & PADL2) {
                    int rstickX;
                    int rstickY;

                    controls_state = 3;

                    if ((rstickX = Input_stickPosBuf.rStickX - 128, ABS(rstickX) > 64)
                        || (rstickY = Input_stickPosBuf.rStickY - 128,
                            ABS(rstickY) > 64)) {
                        if ((Input_stickPosBuf.rStickX - 128) < -64) {
                            Menu_Map_cameraAngles.vy -= 64;
                        } else if ((Input_stickPosBuf.rStickX - 128) > 64) {
                            Menu_Map_cameraAngles.vy += 64;
                        }
                        if ((Input_stickPosBuf.rStickY - 128) < -64) {
                            Menu_Map_cameraAngles.vx += 64;
                        } else if ((Input_stickPosBuf.rStickY - 128) > 64) {
                            Menu_Map_cameraAngles.vx -= 64;
                        }
                        if (Menu_Map_cameraAngles.vx < 0) {
                            Menu_Map_cameraAngles.vx = 0;
                            // dummy assignment to break deduplication optimization around
                            // line 184
                        } else if ((lStickX = Menu_Map_cameraAngles.vx) > 1024) {
                            Menu_Map_cameraAngles.vx = 1024;
                        }
                    } else {
                        if (Input_previousButtons & PADLleft) {
                            Menu_Map_cameraAngles.vy -= 64;
                        } else if (Input_previousButtons & PADLright) {
                            Menu_Map_cameraAngles.vy += 64;
                        }
                        if ((Input_previousButtons & PADLdown)
                            && (Menu_Map_cameraAngles.vx > 0)) {
                            Menu_Map_cameraAngles.vx -= 64;
                        } else if ((Input_previousButtons & PADLup)
                                   && (Menu_Map_cameraAngles.vx < 1024)) {
                            Menu_Map_cameraAngles.vx += 64;
                        }
                    }
                } else if (Input_previousButtons & PADR2) {
                    int rstickX;
                    int rstickY;

                    controls_state = 2;

                    if ((rstickX = Input_stickPosBuf.rStickX - 128, ABS(rstickX) > 64)
                        || (rstickY = Input_stickPosBuf.rStickY - 128,
                            ABS(rstickY) > 64)) {

                        if ((Input_stickPosBuf.rStickY - 128) < -0x40) {
                            Menu_Map_cameraDistance -= ONE / 256;
                        } else if ((Input_stickPosBuf.rStickY - 128) > 64) {
                            Menu_Map_cameraDistance += ONE / 256;
                        }

                        if (Menu_Map_cameraDistance < ONE / 8) {
                            Menu_Map_cameraDistance = ONE / 8;
                        } else if (Menu_Map_cameraDistance > ONE / 4) {
                            Menu_Map_cameraDistance = ONE / 4;
                        }

                    } else {
                        if (Input_previousButtons & PADLup
                            && (Menu_Map_cameraDistance > ONE / 8)) {
                            Menu_Map_cameraDistance -= ONE / 256;
                        } else if ((Input_previousButtons & PADLdown)
                                   && (Menu_Map_cameraDistance < ONE / 4)) {
                            Menu_Map_cameraDistance += ONE / 256;
                        }
                    }
                } else if (Input_previousButtons & PADstart) {

                    Menu_Map_geomOffsetX = 0;
                    Menu_Map_geomOffsetY = 0;
                    Menu_Map_cameraDistance = ONE * 3 / 16;
                    Menu_Map_cameraAngles.vx = 0x238;
                    Menu_Map_cameraAngles.vy = 0x800 - (u_short)Render_frameScratch.camera.angles.vy;
                    Menu_Map_currentRoomIndex = Menu_Map_InitialRoomIndex((Field_MapScene *)(Battle_unusedSceneBuffer));

                    Menu_Map_CenterOnRoom((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);

                } else {
                    int rstickX;
                    int rstickY;

                    if ((rstickX = Input_stickPosBuf.rStickX - 128, ABS(rstickX) > 64)
                        || (rstickY = Input_stickPosBuf.rStickY - 128,
                            ABS(rstickY) > 64)) {

                        if (rstickX < -64) {
                            Menu_Map_geomOffsetX -= 4;
                        } else if (rstickX > 64) {
                            Menu_Map_geomOffsetX += 4;
                        }

                        if ((Input_stickPosBuf.rStickY - 128) < -64) {
                            Menu_Map_geomOffsetY -= 4;
                        } else if ((Input_stickPosBuf.rStickY - 128) > 64) {
                            Menu_Map_geomOffsetY += 4;
                        }

                        if (Menu_Map_geomOffsetY < -80) {
                            Menu_Map_geomOffsetY = -80;
                        } else if (Menu_Map_geomOffsetY > 80) {
                            Menu_Map_geomOffsetY = 80;
                        }

                        if (Menu_Map_geomOffsetX < -96) {
                            Menu_Map_geomOffsetX = -96;
                        } else if (Menu_Map_geomOffsetX > 96) {
                            Menu_Map_geomOffsetX = 96;
                        }

                    } else {
                        if ((Input_previousButtons & PADLup)
                            && (Menu_Map_geomOffsetY > -80)) {
                            Menu_Map_geomOffsetY -= 4;
                        } else if ((Input_previousButtons & PADLdown)
                                   && (Menu_Map_geomOffsetY < 80)) {
                            Menu_Map_geomOffsetY += 4;
                        }

                        if ((Input_previousButtons & PADLleft)
                            && (Menu_Map_geomOffsetX > -96)) {
                            Menu_Map_geomOffsetX -= 4;
                        } else if ((Input_previousButtons & PADLright)
                                   && (Menu_Map_geomOffsetX < 96)) {
                            Menu_Map_geomOffsetX += 4;
                        }
                    }

                    if ((Input_previousButtons & (PADL1 | PADR1))
                        != (PADL1 | PADR1)) {

                        if (Input_buttonRepeat & 8) {
                            Menu_Map_SelectAdjacentVisitedRoom(1);
                            Menu_Map_SetRoomCenterTarget((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);
                        } else if (Input_buttonRepeat & 4) {
                            Menu_Map_SelectAdjacentVisitedRoom(-1);
                            Menu_Map_SetRoomCenterTarget((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);
                        }
                    }

                    if (roomIndex != Menu_Map_currentRoomIndex) {
                        Menu_SetTextBoxText(0, Menu_Map_roomNamesTable[Menu_Map_currentRoomIndex].name);
                    }
                }
            }
        }

        Menu_Map_menuCommandsState = controls_state;

        Menu_Map_DrawFrame();
        break;

    case Menu_Map_MapMenuState_DisplayMapMenu:
        Menu_Map_DrawFrame();

        Menu_Map_selectedMap = area_selection_result = Menu_Map_UpdateAreaSelection(0);

        if (area_selection_result == -1) {
            break;
        } else if (area_selection_result == -3) {

            Menu_Map_menuState = Menu_Map_MapMenuState_Quit;
            Menu_Map_quitState = 2;

            Menu_DismissTextBox(0);

        } else if (area_selection_result == -2) {
            Menu_Map_menuState = Menu_Map_MapMenuState_ProcessInput;

            Menu_Map_ShowAreaName((char*)&Menu_Map_mapNames[Menu_Map_mapNames[Menu_Map_currentScene]],
                Menu_Map_currentScene == Menu_Map_currentSceneRow);
        } else {

            Menu_Map_isPaling = Audio_Seq_unusedStateFlags.mapPaling[Menu_Map_currentScene];
            Menu_Map_currentScene = Menu_Map_sceneRowIdMap[area_selection_result];
            Menu_Map_menuState = Menu_Map_MapMenuState_InitNewMap;

            Menu_Map_armFileBuff = Core_Memory_AllocateFromHighEnd(Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);
            Menu_Map_sceneCdQueueSlot =
                Content_AllocateDiscRequest(&Menu_Map_kSceneArmFiles[Menu_Map_currentScene]);
            Content_EnqueueDiscRequest(Menu_Map_sceneCdQueueSlot, Menu_Map_armFileBuff);
        }
        break;

    case Menu_Map_MapMenuState_Quit:
        Menu_Map_uiAnimationState = 2;

        if (Menu_Map_backgroundBrightness >= 128) {
            Core_Memory_FreeFromHighEnd(Battle_unusedSceneBuffer);

            Battle_unusedSceneBuffer = NULL;
            return Menu_Map_quitState;
        }

        Menu_Map_backgroundBrightness += 16;
        Menu_Map_geomOffsetX += 64;

        Menu_Map_DrawFrame();
        break;

    case Menu_Map_MapMenuState_InitNewMap:
        Menu_Map_geomOffsetX += 64;

        if ((Menu_Map_sceneCdQueueSlot->state == Content_DiscRequestState_Loaded)
            && (Menu_Map_geomOffsetX >= 384)) {

            Menu_Map_isPaling = 0;

            Content_ReleaseDiscRequest(Menu_Map_sceneCdQueueSlot);
            Core_Memory_FreeFromHighEnd(Battle_unusedSceneBuffer);

            Battle_unusedSceneBuffer =
                Core_Memory_AllocateFromHighEnd(Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);

            Core_Memory_CopyAlignedWords(
                Battle_unusedSceneBuffer, Menu_Map_armFileBuff, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);
            Core_Memory_FreeFromHighEnd(Menu_Map_armFileBuff);

            Menu_Map_roomNamesTable = Field_ResolveSceneRoomData(Battle_unusedSceneBuffer, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);

            Field_RestoreSceneRoomVisits((Field_MapScene *)(Battle_unusedSceneBuffer));
            Menu_Map_ScaleRoomVertices((Field_MapScene *)(Battle_unusedSceneBuffer), sizeof(Field_MapScene));

            Menu_Map_currentRoomIndex = Menu_Map_InitialRoomIndex((Field_MapScene *)(Battle_unusedSceneBuffer));

            Menu_Map_CenterOnRoom((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);
            Menu_SetTextBoxText(0, Menu_Map_roomNamesTable[Menu_Map_currentRoomIndex].name);

            Menu_Map_geomOffsetX = -384;
            Menu_Map_geomOffsetY = 0;
            Menu_Map_menuState = Menu_Map_MapMenuState_WaitForMapAnimation;
        }

        Menu_Map_DrawFrame();
        break;

    case Menu_Map_MapMenuState_WaitForMapAnimation:
        Menu_Map_geomOffsetX += 64;

        if (Menu_Map_geomOffsetX >= 0) {
            Menu_Map_menuState = Menu_Map_MapMenuState_ProcessInput;
        }

        Menu_Map_DrawFrame();
        break;
    }

    ++Menu_Map_menuFramesCounter;

    Menu_Map_AnimateCentering((Field_MapScene *)(Battle_unusedSceneBuffer));

    return 0;
}

// decomp: MENU5.PRG _renderScreen (src/MENU/MENU5.PRG/4D8.c:905)
void Menu_Map_DrawFrame(void)
{

    DR_STP* stp;
    void** pScratch = (void**)Core_ScratchAddr(0);
    State_StateFlags* flags;
    int scene;

    Menu_Map_DrawControls();

    stp = (DR_STP*)(pScratch[0]);

    SetDrawStp(stp, 1);
    AddPrim((DR_STP*)(((u_char*)(pScratch[1])) + 0x1FFC), stp++);

    flags = &Audio_Seq_unusedStateFlags;
    scene = Menu_Map_currentScene;

    do {
    } while (0);

    *(DR_STP**)pScratch = stp;

    if (flags->mapPaling[scene] != 0) {
        Menu_Map_DrawPalingDistortion();
        Menu_SetTextBoxText(0, (char*)&Menu_Map_framePaling[Menu_Map_framePaling[0]]);
        return;
    }

    Menu_Map_ApplyCamera();
    Menu_Map_DarkenBackground();

    if (Menu_Map_isPaling == 0) {
        Menu_Map_DrawVisitedRooms((Field_MapScene *)(Battle_unusedSceneBuffer));
    }
}

// decomp: MENU5.PRG _setCamera (src/MENU/MENU5.PRG/4D8.c:948)
void Menu_Map_ApplyCamera(void)
{
    SVECTOR negative_camera_position;
    MATRIX view_rotation;
    MATRIX sp38;

    Menu_Map_cameraPos.vx =
        (((Core_Gte_Sin(Menu_Map_cameraAngles.vy) * Menu_Map_cameraDistance) / ONE) * Core_Gte_Cos(Menu_Map_cameraAngles.vx))
        / ONE;
    Menu_Map_cameraPos.vz =
        (((Core_Gte_Cos(Menu_Map_cameraAngles.vy) * -Menu_Map_cameraDistance) / ONE) * Core_Gte_Cos(Menu_Map_cameraAngles.vx))
        / ONE;
    Menu_Map_cameraPos.vy = (Core_Gte_Sin(Menu_Map_cameraAngles.vx) * -Menu_Map_cameraDistance) / ONE;

    Menu_Framework_BuildYawMatrix(&view_rotation, Menu_Map_cameraAngles.vy);
    Menu_Framework_BuildPitchMatrix(&sp38, Menu_Map_cameraAngles.vx);
    Core_Math_MultiplyMatrixIntoRight(&sp38, &view_rotation);

    applyVector(&negative_camera_position, Menu_Map_cameraPos.vx, Menu_Map_cameraPos.vy, Menu_Map_cameraPos.vz, = -);

    Core_Gte_ApplyMatrix(&view_rotation, &negative_camera_position, (VECTOR*)sp38.t);
    Core_Gte_SetRotMatrix(&view_rotation);
    Core_Gte_SetTransMatrix(&sp38);
}

// decomp: MENU5.PRG _mapSelectionMenu (src/MENU/MENU5.PRG/4D8.c:979)
int Menu_Map_UpdateAreaSelection(int initialize)
{
    enum Menu_Map_AreaSelectionState { Menu_Map_AreaSelectionState_Init, Menu_Map_AreaSelectionState_GetSelection };

    static int Menu_Map_areaSelectionState;
    static int Menu_Map_areaSelectionSelectedMap;

    char* menuText[64];
    int mapCount;
    int currentMap;
    int i;
    int areaVisited;

    if (initialize != 0) {
        Menu_Map_areaSelectionState = Menu_Map_AreaSelectionState_Init;
        Menu_Map_areaSelectionSelectedMap = 0;
    }

    switch (Menu_Map_areaSelectionState) {
    case Menu_Map_AreaSelectionState_Init:
        currentMap = 0;

        for (i = 1, mapCount = 0; i < 32; ++i) {

            areaVisited = State_roomVisitProgress.areaFlags[i >> 5] & (1 << (i & 0x1F));

            if (areaVisited) {

                Menu_Map_sceneRowIdMap[mapCount] = i;
                menuText[mapCount * 2] = (char*)&Menu_Map_mapNames[Menu_Map_mapNames[i]];
                menuText[mapCount * 2 + 1] = NULL;

                if (Menu_Map_currentScene == i) {
                    currentMap = mapCount;
                }

                ++mapCount;
            }
        }

        if (mapCount == 0) {
            return -2;
        }

        if (Menu_Map_currentSceneRow == 0) {
            currentMap = 0x80;
        }

        Menu_Map_ConfigureAreaList(mapCount, currentMap, menuText);

        ++Menu_Map_areaSelectionState;
        break;

    case Menu_Map_AreaSelectionState_GetSelection:
        Menu_Map_areaSelectionSelectedMap = Menu_Map_PollAreaSelection();

        if (Menu_Map_areaSelectionSelectedMap != -1) {
            return Menu_Map_areaSelectionSelectedMap;
        }

        break;
    }

    return -1;
}

// decomp: MENU5.PRG func_8010467C (src/MENU/MENU5.PRG/4D8.c:1048)
void Menu_Map_MarkAllRoomsVisited(int* data)
{
    int count;
    int i;
    int* ptr;

    ptr = data + 1;
    count = *data;

    for (i = 0; i < count; ++i) {
        *ptr = 1;
        ptr += 3;
    }
}

// decomp: MENU5.PRG _renderMap (src/MENU/MENU5.PRG/4D8.c:1081)
void Menu_Map_DrawVisitedRooms(Field_MapScene* scene)
{
    int roomCount;
    int i;
    Field_MapRoom* room;
    int* room_count_address;
    Render_ScreenOffset prevOffset;
    Render_ScreenOffset tmpOffset;

    tmpOffset.x = Menu_Map_geomOffsetX + 0x90;
    tmpOffset.y = Menu_Map_geomOffsetY + 0x88;

    Render_GetProjectionScreenOffset(&prevOffset);
    Render_SetProjectionScreenOffset(&tmpOffset);

    Menu_Map_mapLabelCount = 0;
    room_count_address = &scene->roomCount;
    roomCount = *room_count_address;
    room = (Field_MapRoom*)scene;
    room = scene->rooms;

    for (i = 0; i < roomCount; ++i, ++room) {
        if (room->visited != 0) {
            Menu_Map_DrawRoom(i, VS_ONDISC(room, dataAddress, void *), room->zoneId, room->mapId);
        }
    }

    Menu_Map_DrawLabels();
    Render_SetProjectionScreenOffset(&prevOffset);
}

// decomp: MENU5.PRG _renderRoom (src/MENU/MENU5.PRG/4D8.c:1112)
int Menu_Map_DrawRoom(int roomIndex, void* roomData, int zone_id, int map_id)
{
    CVECTOR colorIn;
    CVECTOR colorOut0;
    CVECTOR colorOut1;
    int available_marker_kinds[4];
    long sp38;
    long sp3C;
    u_char* room_element_cursor;
    int i;
    int j;
    POLY_G3* polyG3;
    POLY_G4* polyG4;
    LINE_G2* lineG2;
    TILE* tile;
    int const* new_var;

    enum { kMapVertexCapacity = 256 };
    short projectedVertices[kMapVertexCapacity][4];
    short (*projected_vertex_rows)[4] = projectedVertices;
    int elementCount = *(int*)roomData;
    roomData = (u_char*)(roomData) + (4);

    for (i = 0; i < elementCount; ++i) {
        u_int projected_depth = Core_Gte_RotTransPers((SVECTOR *)(roomData), (long*)projected_vertex_rows[i], &sp38, &sp3C);
        projected_vertex_rows[i][2] = projected_depth;
        projected_depth -= 0x40;
        projected_vertex_rows[i][3] = sp38;

        if (projected_depth >= 0x7BF) {
            return 0;
        }

        roomData = (u_char*)(roomData) + (8);
    }

    if (roomIndex == Menu_Map_currentRoomIndex) {
        colorIn.r = 0;
        colorIn.g = 0x80;
        colorIn.b = 0x40;
    } else {
        colorIn.r = 0;
        colorIn.g = 0x40;
        colorIn.b = 0x80;
    }

    room_element_cursor = (u_char *)(roomData);
    room_element_cursor += 4;
    elementCount = *(int*)roomData;
    polyG3 = (POLY_G3 *)(*(void**)Core_ScratchAddr(0));

    for (i = 0; i < elementCount; ++i) {

        sp38 = projected_vertex_rows[room_element_cursor[0]][2];

        if (sp38 < projected_vertex_rows[room_element_cursor[1]][2]) {
            sp38 = projected_vertex_rows[room_element_cursor[1]][2];
        }

        if (sp38 < projected_vertex_rows[room_element_cursor[2]][2]) {
            sp38 = projected_vertex_rows[room_element_cursor[2]][2];
        }

        setPolyG3(polyG3);
        polyG3->x0 = projected_vertex_rows[room_element_cursor[0]][0];
        polyG3->x1 = projected_vertex_rows[room_element_cursor[1]][0];
        polyG3->x2 = projected_vertex_rows[room_element_cursor[2]][0];
        polyG3->y0 = projected_vertex_rows[room_element_cursor[0]][1];
        polyG3->y1 = projected_vertex_rows[room_element_cursor[1]][1];
        polyG3->y2 = projected_vertex_rows[room_element_cursor[2]][1];

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[0]][3], &colorOut0);
        setRGB0(polyG3, colorOut0.r, colorOut0.g, colorOut0.b);

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[1]][3], &colorOut0);
        setRGB1(polyG3, colorOut0.r, colorOut0.g, colorOut0.b);

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[2]][3], &colorOut0);
        setRGB2(polyG3, colorOut0.r, colorOut0.g, colorOut0.b);

        AddPrim(((int**)Core_ScratchAddr(0))[1] + sp38, polyG3++);

        room_element_cursor += 4;
    }

    elementCount = *(int*)room_element_cursor;
    room_element_cursor += 4;
    polyG4 = (POLY_G4*)polyG3;

    for (i = 0; i < elementCount; ++i) {
        sp38 = projected_vertex_rows[room_element_cursor[0]][2];

        if (sp38 < projected_vertex_rows[room_element_cursor[1]][2]) {
            sp38 = projected_vertex_rows[room_element_cursor[1]][2];
        }

        if (sp38 < projected_vertex_rows[room_element_cursor[2]][2]) {
            sp38 = projected_vertex_rows[room_element_cursor[2]][2];
        }

        if (sp38 < projected_vertex_rows[room_element_cursor[3]][2]) {
            sp38 = projected_vertex_rows[room_element_cursor[3]][2];
        }

        setPolyG4(polyG4);
        polyG4->x0 = projected_vertex_rows[room_element_cursor[0]][0];
        polyG4->x1 = projected_vertex_rows[room_element_cursor[1]][0];
        polyG4->x2 = projected_vertex_rows[room_element_cursor[3]][0];
        polyG4->x3 = projected_vertex_rows[room_element_cursor[2]][0];
        polyG4->y0 = projected_vertex_rows[room_element_cursor[0]][1];
        polyG4->y1 = projected_vertex_rows[room_element_cursor[1]][1];
        polyG4->y2 = projected_vertex_rows[room_element_cursor[3]][1];
        polyG4->y3 = projected_vertex_rows[room_element_cursor[2]][1];

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[0]][3], &colorOut0);
        setRGB0(polyG4, colorOut0.r, colorOut0.g, colorOut0.b);

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[1]][3], &colorOut0);
        setRGB1(polyG4, colorOut0.r, colorOut0.g, colorOut0.b);

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[3]][3], &colorOut0);
        setRGB2(polyG4, colorOut0.r, colorOut0.g, colorOut0.b);

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[2]][3], &colorOut0);
        setRGB3(polyG4, colorOut0.r, colorOut0.g, colorOut0.b);

        AddPrim(((int**)Core_ScratchAddr(0))[1] + sp38, polyG4++);

        room_element_cursor += 4;
    }

    new_var = (int*)room_element_cursor;
    elementCount = *new_var;
    room_element_cursor += 4;
    lineG2 = (LINE_G2*)polyG4;

    for (i = 0; i < elementCount; ++i) {
        long scratch = 0;
        sp38 = projected_vertex_rows[room_element_cursor[0]][2] - 1;

        if (sp38 < projected_vertex_rows[room_element_cursor[1]][2]) {
            sp38 = projected_vertex_rows[room_element_cursor[1]][2] - 1;
        }

        colorIn.g = 0x80;
        colorIn.r = 0;
        colorIn.b = 0xFF;

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[0]][3], &colorOut0);
        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[1]][3], &colorOut1);

        setLineG2(lineG2);
        setSemiTrans(lineG2, 1);

        lineG2->x0 = projected_vertex_rows[room_element_cursor[0]][0] - 1;
        lineG2->x1 = projected_vertex_rows[room_element_cursor[1]][0] - 1;
        lineG2->y0 = projected_vertex_rows[room_element_cursor[0]][1];
        lineG2->y1 = projected_vertex_rows[room_element_cursor[1]][1];

        setRGB0(lineG2, colorOut0.r, colorOut0.g, colorOut0.b);
        setRGB1(lineG2, colorOut1.r, colorOut1.g, colorOut1.b);

        AddPrim(((int**)(((uintptr_t)Core_ScratchBase() + 0x000) + scratch))[1] + sp38, lineG2++);

        setLineG2(lineG2);
        setSemiTrans(lineG2, 1);

        lineG2->x0 = projected_vertex_rows[room_element_cursor[0]][0];
        lineG2->x1 = projected_vertex_rows[room_element_cursor[1]][0];
        lineG2->y0 = projected_vertex_rows[room_element_cursor[0]][1] - 1;
        lineG2->y1 = projected_vertex_rows[room_element_cursor[1]][1] - 1;

        setRGB0(lineG2, colorOut0.r, colorOut0.g, colorOut0.b);
        setRGB1(lineG2, colorOut1.r, colorOut1.g, colorOut1.b);

        AddPrim(((int**)(((uintptr_t)Core_ScratchBase() + 0x000) + scratch))[1] + sp38, lineG2++);

        setLineG2(lineG2);
        setSemiTrans(lineG2, 1);

        lineG2->x0 = projected_vertex_rows[room_element_cursor[0]][0] + 1;
        lineG2->x1 = projected_vertex_rows[room_element_cursor[1]][0] + 1;
        lineG2->y0 = projected_vertex_rows[room_element_cursor[0]][1];
        lineG2->y1 = projected_vertex_rows[room_element_cursor[1]][1];

        setRGB0(lineG2, colorOut0.r, colorOut0.g, colorOut0.b);
        setRGB1(lineG2, colorOut1.r, colorOut1.g, colorOut1.b);

        AddPrim(((int**)(((uintptr_t)Core_ScratchBase() + 0x000) + scratch))[1] + sp38, lineG2++);

        setLineG2(lineG2);
        setSemiTrans(lineG2, 1);

        lineG2->x0 = projected_vertex_rows[room_element_cursor[0]][0];
        lineG2->x1 = projected_vertex_rows[room_element_cursor[1]][0];
        lineG2->y0 = projected_vertex_rows[room_element_cursor[0]][1] + 1;
        lineG2->y1 = projected_vertex_rows[room_element_cursor[1]][1] + 1;

        setRGB0(lineG2, colorOut0.r, colorOut0.g, colorOut0.b);
        setRGB1(lineG2, colorOut1.r, colorOut1.g, colorOut1.b);

        AddPrim(((int**)(((uintptr_t)Core_ScratchBase() + 0x000) + scratch))[1] + sp38, lineG2++);

        if (roomIndex == Menu_Map_currentRoomIndex) {
            colorIn.g = 0xFF;
            colorIn.r = 0;
            colorIn.b = 0x80;
            Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[0]][3], &colorOut0);
            Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[1]][3], &colorOut1);
        }

        setLineG2(lineG2);

        lineG2->x0 = projected_vertex_rows[room_element_cursor[0]][0];
        lineG2->x1 = projected_vertex_rows[room_element_cursor[1]][0];
        lineG2->y0 = projected_vertex_rows[room_element_cursor[0]][1];
        lineG2->y1 = projected_vertex_rows[room_element_cursor[1]][1];

        setRGB0(lineG2, colorOut0.r, colorOut0.g, colorOut0.b);
        setRGB1(lineG2, colorOut1.r, colorOut1.g, colorOut1.b);

        AddPrim(((int**)(((uintptr_t)Core_ScratchBase() + 0x000) + scratch))[1] + sp38, lineG2++);

        room_element_cursor += 4;
    }

    elementCount = *(int*)room_element_cursor;
    room_element_cursor += 4;

    for (i = 0; i < elementCount; ++i, room_element_cursor += 4) {

        sp38 = projected_vertex_rows[room_element_cursor[0]][2] - 1;

        if (sp38 < projected_vertex_rows[room_element_cursor[1]][2]) {
            sp38 = projected_vertex_rows[room_element_cursor[1]][2] - 1;
        }

        if (roomIndex == Menu_Map_currentRoomIndex) {
            colorIn.g = 0xFF;
        } else {
            colorIn.g = 0x40;
        }

        colorIn.r = 0;
        colorIn.b = 0x80;

        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[0]][3], &colorOut0);
        Core_Gte_DpqColor(&colorIn, projected_vertex_rows[room_element_cursor[1]][3], &colorOut1);

        setLineG2(lineG2);

        do {
            setSemiTrans(lineG2, 1);
        } while (0);

        lineG2->x0 = projected_vertex_rows[room_element_cursor[0]][0];
        lineG2->x1 = projected_vertex_rows[room_element_cursor[1]][0];
        lineG2->y0 = projected_vertex_rows[room_element_cursor[0]][1];
        lineG2->y1 = projected_vertex_rows[room_element_cursor[1]][1];

        setRGB0(lineG2, colorOut0.r, colorOut0.g, colorOut0.b);
        setRGB1(lineG2, colorOut1.r, colorOut1.g, colorOut1.b);

        AddPrim(((int**)Core_ScratchAddr(0))[1] + sp38, lineG2++);
    }

    tile = (TILE*)lineG2;
    elementCount = *(int*)room_element_cursor;
    room_element_cursor += 4;

    for (i = 0; i < elementCount; ++i, room_element_cursor += 4) {

        sp3C = 0;
        *(void**)Core_ScratchAddr(0) = tile;

        if ((room_element_cursor[3] != 0) && (Menu_Map_ShouldDrawDoorStateMarker(room_element_cursor[3], zone_id, map_id) != 0)
            && (roomIndex == Menu_Map_currentRoomIndex)) {

            Menu_Map_DrawDoorStateMarker(projected_vertex_rows[room_element_cursor[0]][0], projected_vertex_rows[room_element_cursor[0]][1] - 0xE, room_element_cursor[3]);

            tile = (TILE *)(*(void**)Core_ScratchAddr(0));
            sp3C = 1;
        }

        if (sp3C == 0) {
            if ((room_element_cursor[2] & 4) && (roomIndex == Menu_Map_currentRoomIndex)) {

                Menu_Map_DrawFeatureMarker(0, projected_vertex_rows[room_element_cursor[0]][0], projected_vertex_rows[room_element_cursor[0]][1] - 0xE);
                Menu_Map_QueueLabel(projected_vertex_rows[room_element_cursor[0]][0], projected_vertex_rows[room_element_cursor[0]][1], 1, room_element_cursor[1]);

                if ((roomIndex == Menu_Map_isCurrentScene) && (i == Field_enteredDoorId)) {
                    if (Menu_Map_menuFramesCounter & 0x10) {
                        colorIn.r = 0xFF;
                        colorIn.g = 0;
                        colorIn.b = 0x80;
                        sp3C = 1;
                    }
                } else {
                    colorIn.r = 0;
                    colorIn.g = 0xC0;
                    colorIn.b = 0xC0;
                    sp3C = 1;
                }
            }

            if ((roomIndex == Menu_Map_currentRoomIndex) && (room_element_cursor[2] == 0)) {
                if ((roomIndex == Menu_Map_isCurrentScene) && (i == Field_enteredDoorId)) {
                    if (Menu_Map_menuFramesCounter & 0x10) {
                        colorIn.r = 0xFF;
                        colorIn.g = 0;
                        colorIn.b = 0x80;
                        sp3C = 1;
                    }
                } else {
                    colorIn.r = 0xC0;
                    colorIn.g = 0xC0;
                    colorIn.b = 0xC0;
                    sp3C = 1;
                }
            }

            j = 0;

            if ((room_element_cursor[2] & 0xB) && (roomIndex == Menu_Map_isCurrentScene)) {
                available_marker_kinds[j++] = 0;
            }

            if ((room_element_cursor[2] & 2) && (roomIndex == Menu_Map_currentRoomIndex)) {
                available_marker_kinds[j++] = 1;
            }

            if ((room_element_cursor[2] & 8) && (roomIndex == Menu_Map_currentRoomIndex)) {
                available_marker_kinds[j++] = 2;
                available_marker_kinds[j++] = 1;
                available_marker_kinds[j++] = 3;
            }

            if ((room_element_cursor[2] & 0x10) && (roomIndex == Menu_Map_currentRoomIndex)) {
                available_marker_kinds[j++] = 3;
            }

            if (j > 0) {

                colorIn.r = 0xA0;
                colorIn.g = 0xA0;
                colorIn.b = 0;
                sp3C = 1;

                switch (available_marker_kinds[(Menu_Map_menuFramesCounter >> 5) % j]) {
                case 0:
                    Menu_Map_DrawAshleyMarker(0,
                        ((projected_vertex_rows[room_element_cursor[0]][0] - 8) & 0xFFFF)
                            | ((projected_vertex_rows[room_element_cursor[0]][1] << 16) + 0xFFF80000),
                        *((Menu_Map_menuFramesCounter & 0xF) + (new_var = Menu_Map_kAshleyIconBrightness)),
                        (u_long *)(((u_char*)(((void**)Core_ScratchAddr(0))[1])) + 32));
                    break;

                case 1:
                    Menu_Map_DrawFeatureMarker(2, projected_vertex_rows[room_element_cursor[0]][0], projected_vertex_rows[room_element_cursor[0]][1] - 0xE);
                    break;

                case 2:
                    Menu_Map_DrawFeatureMarker(1, projected_vertex_rows[room_element_cursor[0]][0], projected_vertex_rows[room_element_cursor[0]][1] - 0xE);
                    break;

                case 3:
                    Menu_Map_DrawFeatureMarker(3, projected_vertex_rows[room_element_cursor[0]][0], projected_vertex_rows[room_element_cursor[0]][1] - 0xE);
                    break;
                }
            }

            tile = (TILE *)(*(void**)Core_ScratchAddr(0));

            if (sp3C != 0) {
                int** legacy_render_scratch;
                sp38 = projected_vertex_rows[room_element_cursor[0]][2] - 1;

                setTile(tile);
                setXY0(tile, projected_vertex_rows[room_element_cursor[0]][0] - 1, projected_vertex_rows[room_element_cursor[0]][1]);
                setWH(tile, 3, 1);
                setRGB0(tile, colorIn.r, colorIn.g, colorIn.b);

                AddPrim(((int**)Core_ScratchAddr(0))[1] + sp38, tile++);

                setTile(tile);
                setXY0(tile, projected_vertex_rows[room_element_cursor[0]][0], projected_vertex_rows[room_element_cursor[0]][1] - 1);
                setWH(tile, 1, 3);
                setRGB0(tile, colorIn.r, colorIn.g, colorIn.b);

                legacy_render_scratch = ((int**)Core_ScratchAddr(0));

                AddPrim(legacy_render_scratch[1] + sp38, tile++);
            }
        }
    }

    *(void**)Core_ScratchAddr(0) = tile;

    return 1;
}

// decomp: MENU5.PRG _snapMapToRoom (src/MENU/MENU5.PRG/4D8.c:1511)
void Menu_Map_CenterOnRoom(Field_MapScene* scene, int roomId)
{
    SVECTOR maxCorner;
    SVECTOR minCorner;
    int roomCount;
    int vertexCount;
    int i;
    int j;
    Field_MapRoomVertices* roomVertices;
    SVECTOR* vector;
    u_int zSum;
    Field_MapRoom* room = scene->rooms;

    maxCorner.vx = 0x7FFF;
    maxCorner.vy = 0x7FFF;
    maxCorner.vz = 0x7FFF;
    minCorner.vx = -0x8000;
    minCorner.vy = -0x8000;
    minCorner.vz = -0x8000;

    roomVertices = (Field_MapRoomVertices *)(VS_ONDISC_DOT(room[roomId], dataAddress, void *));
    roomCount = scene->roomCount;
    vertexCount = roomVertices->vertexCount;
    vector = roomVertices->vertices;

    for (i = 0; i < vertexCount; ++i, ++vector) {
        if (maxCorner.vx > vector->vx) {
            maxCorner.vx = vector->vx;
        }
        if (maxCorner.vy > vector->vy) {
            maxCorner.vy = vector->vy;
        }
        if (maxCorner.vz > vector->vz) {
            maxCorner.vz = vector->vz;
        }
        if (minCorner.vx < vector->vx) {
            minCorner.vx = vector->vx;
        }
        if (minCorner.vy < vector->vy) {
            minCorner.vy = vector->vy;
        }
        if (minCorner.vz < vector->vz) {
            minCorner.vz = vector->vz;
        }
    }

    Menu_Map_centerPoint.vx = ((maxCorner.vx + minCorner.vx) / 2);
    zSum = maxCorner.vz + minCorner.vz;
    Menu_Map_centerPoint.vz = (int)(zSum + ((u_int)zSum >> 0x1F)) >> 1;
    Menu_Map_centerPoint.vy = minCorner.vy;

    for (j = 0; j < roomCount; ++j, ++room) {
        roomVertices = (Field_MapRoomVertices *)(VS_ONDISC(room, dataAddress, void *));
        vertexCount = roomVertices->vertexCount;
        vector = (SVECTOR*)roomVertices;
        vector = (SVECTOR*)((short*)vector + 2);
        for (i = 0; i < vertexCount; ++i, ++vector) {
            vector->vx -= Menu_Map_centerPoint.vx;
            vector->vy -= Menu_Map_centerPoint.vy;
            vector->vz -= Menu_Map_centerPoint.vz;
        }
    }
    Menu_Map_centerPoint.vx = 0;
    Menu_Map_centerPoint.vz = 0;
    Menu_Map_centerPoint.vy = 0;
}

// decomp: MENU5.PRG _setCenterpoint (src/MENU/MENU5.PRG/4D8.c:1578)
void Menu_Map_SetRoomCenterTarget(Field_MapScene* scene, int roomId)
{
    SVECTOR maxCorner;
    SVECTOR minCorner;
    int roomCount;
    int vertexCount;
    int i;
    Field_MapRoomVertices* roomVertices;
    SVECTOR* vector;
    u_int zSum;
    Field_MapRoom* room = scene->rooms;

    maxCorner.vx = 0x7FFF;
    maxCorner.vy = 0x7FFF;
    maxCorner.vz = 0x7FFF;
    minCorner.vx = -0x8000;
    minCorner.vy = -0x8000;
    minCorner.vz = -0x8000;

    roomVertices = (Field_MapRoomVertices *)(VS_ONDISC_DOT(room[roomId], dataAddress, void *));
    roomCount = scene->roomCount;
    vertexCount = roomVertices->vertexCount;
    vector = roomVertices->vertices;

    for (i = 0; i < vertexCount; ++i, ++vector) {
        if (maxCorner.vx > vector->vx) {
            maxCorner.vx = vector->vx;
        }
        if (maxCorner.vy > vector->vy) {
            maxCorner.vy = vector->vy;
        }
        if (maxCorner.vz > vector->vz) {
            maxCorner.vz = vector->vz;
        }
        if (minCorner.vx < vector->vx) {
            minCorner.vx = vector->vx;
        }
        if (minCorner.vy < vector->vy) {
            minCorner.vy = vector->vy;
        }
        if (minCorner.vz < vector->vz) {
            minCorner.vz = vector->vz;
        }
    }

    Menu_Map_centerPoint.vx = ((maxCorner.vx + minCorner.vx) / 2);
    zSum = maxCorner.vz + minCorner.vz;
    Menu_Map_centerPoint.vz = (int)(zSum + ((u_int)zSum >> 0x1F)) >> 1;
    Menu_Map_centerPoint.vy = minCorner.vy;
}

// decomp: MENU5.PRG _smoothMapToCenterpoint (src/MENU/MENU5.PRG/4D8.c:1629)
void Menu_Map_AnimateCentering(Field_MapScene* scene)
{
    SVECTOR svector;
    int coord;
    int roomCount;
    int vertexCount;
    int j;
    int i;
    Field_MapRoomVertices* dataAddress;
    SVECTOR* vector;
    Field_MapRoom* room;

    if (Menu_Map_centerPoint.vx == 0 && Menu_Map_centerPoint.vy == 0 && Menu_Map_centerPoint.vz == 0) {
        return;
    }

    coord = ABS(Menu_Map_centerPoint.vx);

    if (coord < 8) {
        svector.vx = Menu_Map_centerPoint.vx;
    } else {
        if (coord >= 0x81) {
            int var_v0 = Menu_Map_centerPoint.vx;
            if (Menu_Map_centerPoint.vx < 0) {
                var_v0 = Menu_Map_centerPoint.vx + 7;
            }
            svector.vx = var_v0 >> 3;
        } else if (Menu_Map_centerPoint.vx <= 0) {
            if (Menu_Map_centerPoint.vx < 0) {
                svector.vx = -8;
            }
        } else {
            svector.vx = 8;
        }
    }

    coord = ABS(Menu_Map_centerPoint.vy);

    if (coord < 8) {
        svector.vy = Menu_Map_centerPoint.vy;
    } else {
        if (coord >= 0x81) {
            int var_v0 = Menu_Map_centerPoint.vy;
            if (Menu_Map_centerPoint.vy < 0) {
                var_v0 = Menu_Map_centerPoint.vy + 7;
            }
            svector.vy = var_v0 >> 3;
        } else if (Menu_Map_centerPoint.vy <= 0) {
            if (Menu_Map_centerPoint.vy < 0) {
                svector.vy = -8;
            }
        } else {
            svector.vy = 8;
        }
    }

    coord = ABS(Menu_Map_centerPoint.vz);

    if (coord < 8) {
        svector.vz = Menu_Map_centerPoint.vz;
    } else {
        if (coord >= 0x81) {
            int var_v0 = Menu_Map_centerPoint.vz;
            do {
                if (Menu_Map_centerPoint.vz < 0) {
                    var_v0 = Menu_Map_centerPoint.vz + 7;
                }
                svector.vz = var_v0 >> 3;
            } while (0);
        } else if (Menu_Map_centerPoint.vz <= 0) {
            if (Menu_Map_centerPoint.vz < 0) {
                svector.vz = -8;
            }
        } else {
            svector.vz = 8;
        }
    }

    room = scene->rooms;
    roomCount = scene->roomCount;

    for (i = 0; i < roomCount; ++i) {
        dataAddress = (Field_MapRoomVertices *)(VS_ONDISC(room, dataAddress, void *));
        vertexCount = dataAddress->vertexCount;
        vector = (SVECTOR*)dataAddress;
        vector = dataAddress->vertices;
        for (j = 0; j < vertexCount; ++j) {
            vector->vx -= svector.vx;
            vector->vy -= svector.vy;
            vector->vz -= svector.vz;
            ++vector;
        }
        ++room;
    }
    Menu_Map_centerPoint.vx -= svector.vx;
    Menu_Map_centerPoint.vy -= svector.vy;
    Menu_Map_centerPoint.vz -= svector.vz;
}

// decomp: MENU5.PRG _scaleRoomVertices (src/MENU/MENU5.PRG/4D8.c:1728)
void Menu_Map_ScaleRoomVertices(Field_MapScene* scene, int factor)
{
    SVECTOR* vector;
    int roomCount;
    int vertextCount;
    int j;
    int i;
    Field_MapRoomVertices* roomVertices;
    Field_MapRoom* room;

    room = scene->rooms;
    roomCount = scene->roomCount;

    for (i = 0; i < roomCount; ++i) {
        roomVertices = (Field_MapRoomVertices *)(VS_ONDISC(room, dataAddress, void *));
        vertextCount = roomVertices->vertexCount;
        vector = (SVECTOR*)roomVertices;
        vector = (SVECTOR*)((int*)vector + 1);
        for (j = 0; j < vertextCount; ++j) {
            vector->vx *= factor;
            vector->vy *= factor;
            vector->vz *= factor;
            ++vector;
        }
        ++room;
    }
}

// decomp: MENU5.PRG _darkenBackground (src/MENU/MENU5.PRG/4D8.c:1788)
void Menu_Map_DarkenBackground(void)
{
    SPRT* sprt;

    _insertTPage(0x7FE, getTPage(0, 3, 0, 0));

    sprt = (SPRT *)(Render_frameScratch.primitiveCursor);
    setSprt(sprt);
    setSemiTrans(sprt, 1);
    setShadeTex(sprt, 1);
    setXY0(sprt, 0, 0);
    setWH(sprt, 192, 240);
    setUV0(sprt, 0, 0);
    setClut(sprt, 0, 0);
    AddPrim((u_long*)Render_frameScratch.orderingTable + 0x7FE, sprt++);
    Render_frameScratch.primitiveCursor = sprt;

    if (Render_frameBuf != 0) {
        _insertTPage(0x7FE, getTPage(2, 0, 320, 0));
    } else {
        _insertTPage(0x7FE, getTPage(2, 0, 0, 0));
    }

    sprt = (SPRT *)(Render_frameScratch.primitiveCursor);
    setSprt(sprt);
    setSemiTrans(sprt, 1);
    setShadeTex(sprt, 1);
    setXY0(sprt, 192, 0);
    setWH(sprt, 128, 240);
    setUV0(sprt, 0, 0);
    setClut(sprt, 0, 0);

    AddPrim((u_long*)Render_frameScratch.orderingTable + 0x7FE, sprt++);
    Render_frameScratch.primitiveCursor = sprt;

    if (Render_frameBuf != 0) {
        _insertTPage(0x7FE, getTPage(2, 0, 512, 0));
    } else {
        _insertTPage(0x7FE, getTPage(2, 0, 192, 0));
    }
}

// decomp: MENU5.PRG _getCurrentRoomIndex (src/MENU/MENU5.PRG/4D8.c:1834)
int Menu_Map_InitialRoomIndex(Field_MapScene* scene)
{
    int i;

    int roomCount = scene->roomCount;
    Field_MapRoom* room = Field_roomData.minimapScene->rooms;
    int zoneId = room->zoneId;
    int mapId = room->mapId;
    room = scene->rooms;

    for (i = 0; i < roomCount; ++i, ++room) {
        if ((room->zoneId == zoneId) && (room->mapId == mapId)) {
            return Menu_Map_isCurrentScene = i;
        }
    }

    room = scene->rooms;
    Menu_Map_isCurrentScene = -1;
    mapId = 0;

    for (i = 0; i < roomCount; ++i, ++room) {
        if (room->visited != 0) {
            return i;
        }
    }
    return 0;
}

// decomp: MENU5.PRG _updateRoomIndex (src/MENU/MENU5.PRG/4D8.c:1862)
void Menu_Map_SelectAdjacentVisitedRoom(int searchForward)
{
    int currentIndex = Menu_Map_currentRoomIndex;
    Field_MapRoom* room = ((Field_MapScene*)Battle_unusedSceneBuffer)->rooms;

    do {
        if (searchForward > 0) {
            Menu_Map_currentRoomIndex = Menu_Map_roomNamesTable[Menu_Map_currentRoomIndex].next;
        } else {
            Menu_Map_currentRoomIndex = Menu_Map_roomNamesTable[Menu_Map_currentRoomIndex].prev;
        }
    } while (room[Menu_Map_currentRoomIndex].visited == 0);

    if (currentIndex != Menu_Map_currentRoomIndex) {
        Audio_PlaySfxCentered(0x7E, 0xB);
    }
}

// decomp: MENU5.PRG _applyPalingScreenEffect (src/MENU/MENU5.PRG/4D8.c:1880)
void Menu_Map_DrawPalingDistortion(void)
{
    int y;

    _insertTPage(0x7FE, getTPage(0, 3, 0, 0));

    for (y = 0; y < 239; ++y) {
        int x;
        void** legacy_render_scratch;
        int v;
        SPRT* sprt = (SPRT *)(*(void**)Core_ScratchAddr(0));

        setSprt(sprt);
        setSemiTrans(sprt, 1);
        setRGB0(sprt, 128, 128, 128);

        v = Menu_Map_menuFramesCounter;
        legacy_render_scratch = (void**)Core_ScratchAddr(0);

        if (y & 1) {
            x = Core_Gte_Sin((Menu_Map_menuFramesCounter + y) << 6);
            if (x < 0) {
                x += 127;
            }
            x = x >> 7;
        } else {
            x = Core_Gte_Sin((Menu_Map_menuFramesCounter + y) << 6);
            if (x < 0) {
                x += 127;
            }
            x = -(x >> 7);
        }

        setXY0(sprt, x, y);
        setWH(sprt, 192, 1);
        setUV0(sprt, 0, y);
        setClut(sprt, 0, 0);

        AddPrim(((u_char*)(legacy_render_scratch[1])) + 0x20, sprt++);

        legacy_render_scratch[0] = (void*)sprt;

        if (Render_frameBuf != 0) {
            _insertTPage(8, getTPage(2, 0, 320, 0));
        } else {
            _insertTPage(8, getTPage(2, 0, 0, 0));
        }

        sprt = (SPRT *)(*(void**)Core_ScratchAddr(0));
        setSprt(sprt);
        setSemiTrans(sprt, 1);
        setRGB0(sprt, 128, 128, 128);

        if (y & 1) {
            x = Core_Gte_Sin((Menu_Map_menuFramesCounter + y) << 6);
            if (x < 0) {
                x += 127;
            }
            x = (x >> 7) + 192;
        } else {
            int offset;
            x = Core_Gte_Sin((Menu_Map_menuFramesCounter + y) << 6);
            offset = x >> 7;
            if (x < 0) {
                offset = (x + 127) >> 7;
            }
            x = 192 - offset;
        }

        setXY0(sprt, x, y);
        setWH(sprt, 128, 1);
        setUV0(sprt, 0, y);

        AddPrim(((u_char*)(legacy_render_scratch[1])) + 0x20, sprt++);

        legacy_render_scratch[0] = (void*)sprt;

        if (Render_frameBuf != 0) {
            _insertTPage(8, getTPage(2, 0, 512, 0));
        } else {
            _insertTPage(8, getTPage(2, 0, 192, 0));
        }
    }
}

// decomp: MENU5.PRG _drawUIControlsBackground (src/MENU/MENU5.PRG/4D8.c:1965)
void Menu_Map_DrawControlBackdrop(int x, int y, int w, int h)
{
    int i;
    int lineW;
    POLY_F4* poly;
    void** pScratch;
    LINE_G2* line = (LINE_G2 *)(*(void**)Core_ScratchAddr(0));

    for (i = y, lineW = w; i < (y + h - 1); ++i, --lineW) {
        setLineG2(line);
        setXY2(line, x, i, x + lineW - 1, i);
        setRGB0(line, 0x40, 0x38, 0x20);
        setRGB1(line, 0x10, 0x10, 8);
        AddPrim((u_long*)Render_frameScratch.orderingTable + 7, line++);
    }
    x += 2;
    y += 2;
    poly = (POLY_F4*)line;
    setPolyF4(poly);
    setXY4(poly, x, y, x + w - 1, y, x, y + h - 1, x + w - h, y + h - 1);
    setRGB0(poly, 0, 0, 0);
    pScratch = (void**)Core_ScratchAddr(0);
    AddPrim(((u_char*)(pScratch[1])) + 28, poly++);
    pScratch[0] = (void*)poly;
}

// decomp: MENU5.PRG _drawUIControls (src/MENU/MENU5.PRG/4D8.c:1991)
void Menu_Map_DrawControls(void)
{
    static int Menu_Map_controlsUiXOffset;

    Menu_TextBoxState* textBox = Menu_TextBox(0);

    if (Menu_Map_uiAnimationState == 0) {

        Menu_Map_controlsUiXOffset = -128;
        ++Menu_Map_uiAnimationState;

    } else if (Menu_Map_uiAnimationState == 1) {
        if (Menu_Map_controlsUiXOffset < 0) {

            Menu_Map_controlsUiXOffset += 16;

            if (textBox != NULL) {
                textBox->y -= 8;
                textBox->yIndent -= 8;
            }
        }
    } else if (Menu_Map_uiAnimationState == 2) {
        if (Menu_Map_controlsUiXOffset >= -127) {

            Menu_Map_controlsUiXOffset -= 16;

            if (textBox != NULL) {
                textBox->y += 8;
                textBox->yIndent += 8;
            }
        }
    }

    if (Menu_Map_triggerHideUi != 0) {
        return;
    }

    if (Audio_Seq_unusedStateFlags.mapPaling[Menu_Map_currentScene] == 0) {
        int index = Menu_Map_controlsUiXOffset;
        void** legacy_button_render_scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_L1, index + 8, 15, (u_long *)(((u_char*)(legacy_button_render_scratch[1])) + 24));
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_R1, Menu_Map_controlsUiXOffset + 64, 15, (u_long *)(((u_char*)(legacy_button_render_scratch[1])) + 24));
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_L2, Menu_Map_controlsUiXOffset + 8, 33, (u_long *)(((u_char*)(legacy_button_render_scratch[1])) + 24));
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_R2, Menu_Map_controlsUiXOffset + 8, 52, (u_long *)(((u_char*)(legacy_button_render_scratch[1])) + 24));
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Circle, Menu_Map_controlsUiXOffset + 8, 70, (u_long *)(((u_char*)(legacy_button_render_scratch[1])) + 24));

        switch (Menu_Map_menuCommandsState) {
        case 0: {
            int v = ((Menu_Map_controlsUiXOffset + 28) & 0xFFFF) | 0x120000;
            void** s0 = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

            Render_DrawCompactTextColor(
                "BACK", v, vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("NEXT", vs_getXY(Menu_Map_controlsUiXOffset + 84, 18),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ROTATION", vs_getXY(Menu_Map_controlsUiXOffset + 28, 36),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ZOOM", vs_getXY(Menu_Map_controlsUiXOffset + 28, 54),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("MENU", vs_getXY(Menu_Map_controlsUiXOffset + 28, 72),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            break;
        }

        case 1: {
            int v = ((Menu_Map_controlsUiXOffset + 28) & 0xFFFF) | 0x120000;
            void** s0 = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

            Render_DrawCompactTextColor(
                "BACK", v, vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("NEXT", vs_getXY(Menu_Map_controlsUiXOffset + 84, 18),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ROTATION", vs_getXY(Menu_Map_controlsUiXOffset + 28, 36),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ZOOM", vs_getXY(Menu_Map_controlsUiXOffset + 28, 54),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("MENU", vs_getXY(Menu_Map_controlsUiXOffset + 28, 72),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            break;
        }

        case 2: {
            int v = ((Menu_Map_controlsUiXOffset + 28) & 0xFFFF) | 0x120000;
            void** s0 = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

            Render_DrawCompactTextColor("BACK", v, vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("NEXT", vs_getXY(Menu_Map_controlsUiXOffset + 84, 18),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ROTATION", vs_getXY(Menu_Map_controlsUiXOffset + 28, 36),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ZOOM", vs_getXY(Menu_Map_controlsUiXOffset + 28, 54),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("MENU", vs_getXY(Menu_Map_controlsUiXOffset + 28, 72),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            break;
        }

        case 3: {
            int v = ((Menu_Map_controlsUiXOffset + 28) & 0xFFFF) | 0x120000;
            void** s0 = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

            Render_DrawCompactTextColor("BACK", v, vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("NEXT", vs_getXY(Menu_Map_controlsUiXOffset + 84, 18),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ROTATION", vs_getXY(Menu_Map_controlsUiXOffset + 28, 36),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ZOOM", vs_getXY(Menu_Map_controlsUiXOffset + 28, 54),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("MENU", vs_getXY(Menu_Map_controlsUiXOffset + 28, 72),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            break;
        }

        case 4: {
            int v = ((Menu_Map_controlsUiXOffset + 28) & 0xFFFF) | 0x120000;
            void** s0 = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

            Render_DrawCompactTextColor("BACK", v, vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("NEXT", vs_getXY(Menu_Map_controlsUiXOffset + 84, 18),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ROTATION", vs_getXY(Menu_Map_controlsUiXOffset + 28, 36),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ZOOM", vs_getXY(Menu_Map_controlsUiXOffset + 28, 54),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("MENU", vs_getXY(Menu_Map_controlsUiXOffset + 28, 72),
                vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(s0[1])) + 24));
            break;
        }

        case 5: {
            int v = ((Menu_Map_controlsUiXOffset + 28) & 0xFFFF) | 0x120000;
            void** s0 = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

            Render_DrawCompactTextColor("BACK", v, vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("NEXT", vs_getXY(Menu_Map_controlsUiXOffset + 84, 18),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ROTATION", vs_getXY(Menu_Map_controlsUiXOffset + 28, 36),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("ZOOM", vs_getXY(Menu_Map_controlsUiXOffset + 28, 54),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            Render_DrawCompactTextColor("MENU", vs_getXY(Menu_Map_controlsUiXOffset + 28, 72),
                vs_getRGB888(32, 32, 32), (u_long *)(((u_char*)(s0[1])) + 24));
            break;
        }
        }

        Menu_Map_DrawControlBackdrop(Menu_Map_controlsUiXOffset + 16, 18, 114, 12);
        Menu_Map_DrawControlBackdrop(Menu_Map_controlsUiXOffset + 16, 36, 96, 12);
        Menu_Map_DrawControlBackdrop(Menu_Map_controlsUiXOffset + 16, 54, 78, 12);
        Menu_Map_DrawControlBackdrop(Menu_Map_controlsUiXOffset + 16, 72, 60, 12);

        _insertTPage(7, getTPage(2, 0, 192, 0));

        Menu_Map_DrawCompass(62, 184, Menu_Map_cameraAngles.vy);

    } else {
        Render_DrawCompactTextColor("JAMMING", vs_getXY(Menu_Map_controlsUiXOffset + 28, 18),
            vs_getRGB888(128, 128, 128), (u_long *)(((u_char*)(*(void**)((uintptr_t)Core_ScratchBase() + 0x004))) + 24));
    }
}

// decomp: MENU5.PRG _renderIcon (src/MENU/MENU5.PRG/4D8.c:2158)
void Menu_Map_DrawFeatureMarker(int id, int x, int y)
{
    RECT* icon;
    void** scratch = (void**)Core_ScratchAddr(0);
    POLY_FT4* poly = (POLY_FT4 *)(scratch[0]);

    setPolyFT4(poly);
    setShadeTex(poly, 1);

    icon = &Menu_Map_iconRects[id];

    setXY4(poly, x - icon->w / 2, y, icon->w + (x - icon->w / 2), y, x - icon->w / 2,
        y + 8, icon->w + (x - icon->w / 2), y + 8);
    setUV4(poly, icon->x, icon->y, icon->x + icon->w, icon->y, icon->x, icon->y + 8,
        icon->x + icon->w, icon->y + 8);
    setTPage(poly, 0, 0, 768, 0);
    setClut(poly, 848, 223);

    AddPrim(((u_char*)(scratch[1])) + 28, poly++);
    scratch[0] = poly;
}

// decomp: MENU5.PRG func_80107630 (src/MENU/MENU5.PRG/4D8.c:2180)
void Menu_Map_DrawDoorStateMarker(int x, int y, int id)
{
    RECT* rect0;
    RECT* rect1;
    int temp;
    void** legacy_render_scratch = (void**)Core_ScratchAddr(0);
    POLY_FT4* poly = (POLY_FT4 *)(legacy_render_scratch[0]);
    int zero_based_marker_id = (id - 1);

    id = Audio_Seq_unusedStateFlags.opaque33F[id] & 1;
    rect0 = &Menu_Map_kDoorMarkerUpperRects[id];

    setPolyFT4(poly);
    setShadeTex(poly, 1);
    setXY4(poly, x - (rect0->w / 2), y, rect0->w + (x - rect0->w / 2), y,
        x - (rect0->w / 2), rect0->h + y, rect0->w + (x - rect0->w / 2), rect0->h + y);
    setUV4(poly, rect0->x, rect0->y, rect0->x + rect0->w, rect0->y, rect0->x,
        rect0->y + rect0->h, rect0->x + rect0->w, rect0->y + rect0->h);
    setTPage(poly, 0, 0, 768, 0);
    setClut(poly, 976, 223);

    temp = zero_based_marker_id & 0x7F;

    AddPrim(((u_char*)(legacy_render_scratch[1])) + 28, poly++);

    y += rect0->h;
    rect1 = &Menu_Map_kDoorMarkerLowerRects[id];

    setPolyFT4(poly);
    setShadeTex(poly, 1);
    setXY4(poly, x - (rect1->w / 2), y, rect1->w + (x - rect1->w / 2), y,
        x - (rect1->w / 2), rect1->h + y, rect1->w + (x - rect1->w / 2), rect1->h + y);
    setUV4(poly, rect1->x, rect1->y, rect1->x + rect1->w, rect1->y, rect1->x,
        rect1->y + rect1->h, rect1->x + rect1->w, rect1->y + rect1->h);
    setTPage(poly, 0, 0, 768, 0);
    setClut(poly, 864, 223);

    AddPrim(((u_char*)(legacy_render_scratch[1])) + 28, poly++);

    legacy_render_scratch[0] = poly;

    Menu_Map_QueueLabel(x, y, 0, temp);
}

// decomp: MENU5.PRG _renderDoorName (src/MENU/MENU5.PRG/4D8.c:2227)
void Menu_Map_DrawDoorLabel(int x, int y, int doorId)
{
    int linePivotX;
    int xOffset = 186 - x;
    Menu_Framework_MenuRow* menuItem;

    if (xOffset < 0) {
        xOffset += 3;
    }

    linePivotX = xOffset >> 2;

    if (linePivotX >= 0) {
        linePivotX = 186 - linePivotX;
    } else {
        linePivotX = 186;
    }

    Menu_Map_AppendLabelLine(x, y, linePivotX, Menu_Map_mapLabelY + 8);
    Menu_Map_AppendLabelLine(linePivotX, Menu_Map_mapLabelY + 8, 194, Menu_Map_mapLabelY + 8);

    menuItem =
        Menu_Framework_SetMenuItem(0, 194, Menu_Map_mapLabelY, 128, 0, (char*)&Menu_Map_doors[Menu_Map_doors[doorId]]);
    menuItem->backgroundWidth = 16;

    Menu_Map_mapLabelY -= 16;

    Menu_Framework_DrawMenuItem(menuItem);
    Menu_Framework_MenuItem(0)->state = 0;
}

// decomp: MENU5.PRG _insertLine (src/MENU/MENU5.PRG/4D8.c:2258)
void Menu_Map_AppendLabelLine(int x0, int y0, int x1, int y1)
{
    u_long** p = (u_long**)Core_ScratchAddr(0);
    LINE_F2* line = *(LINE_F2**)p;

    setLineF2(line);
    setXY2(line, x0, y0, x1, y1);
    setRGB0(line, 128, 128, 128);

    AddPrim(p[1] + 8, line++);

    p[0] = (u_long*)line;
}

// decomp: MENU5.PRG _renderCompass (src/MENU/MENU5.PRG/4D8.c:2272)
void Menu_Map_DrawCompass(int x, int y, int angle)
{
    int i;
    int uCoords[] = { 0x30, 0x40, 0x48, 0x38 };
    void** legacy_primitive_scratch;
    void** legacy_ordering_table_scratch;
    int v;
    int compass_texture_right_u;

    POLY_FT4* poly = (POLY_FT4 *)(*(void**)((uintptr_t)Core_ScratchBase() + 0x000));

    for (i = 0; i < 8; ++i) {

        Menu_Map_ComputeCompassCorners(x, y, 43, 3, angle + (i * 256));

        setPolyFT4(poly);
        setSemiTrans(poly, 1);
        setShadeTex(poly, 1);

        compass_texture_right_u = 45;

        if (!(i & 3)) {
            v = 128;
            setUV4(poly, 2, v, compass_texture_right_u, v, 2, 131, compass_texture_right_u, 131);
        } else {
            v = 132;
            setUV4(poly, 2, v, compass_texture_right_u, v, 2, 135, compass_texture_right_u, 135);
        }

        setXY4(poly, Menu_Map_compassBounds[0].x, Menu_Map_compassBounds[0].y, Menu_Map_compassBounds[1].x,
            Menu_Map_compassBounds[1].y, Menu_Map_compassBounds[2].x, Menu_Map_compassBounds[2].y,
            Menu_Map_compassBounds[3].x, Menu_Map_compassBounds[3].y);
        setTPage(poly, 0, 1, 448, 256);
        setClut(poly, 960, 222);

        legacy_ordering_table_scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

        AddPrim(((u_char*)(legacy_ordering_table_scratch[1])) + 28, poly++);
    }

    Menu_Map_ComputeCompassCorners(x, y, 39, 39, angle + 0x200);

    for (i = 0; i < 4; ++i) {

        setPolyFT4(poly);
        setShadeTex(poly, 1);
        setXY4(poly, Menu_Map_compassBounds[i].x - 3, Menu_Map_compassBounds[i].y - 3,
            Menu_Map_compassBounds[i].x + 4, Menu_Map_compassBounds[i].y - 3, Menu_Map_compassBounds[i].x - 3,
            Menu_Map_compassBounds[i].y + 4, Menu_Map_compassBounds[i].x + 4, Menu_Map_compassBounds[i].y + 4);
        setUV4(poly, uCoords[i], 128, uCoords[i] + 7, 128, uCoords[i], 135,
            uCoords[i] + 7, 135);
        setTPage(poly, 0, 0, 448, 256);

        if (i == 0) {
            setClut(poly, 960, 221);
        } else {
            setClut(poly, 1008, 220);
        }

        legacy_ordering_table_scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

        AddPrim(((u_char*)(legacy_ordering_table_scratch[1])) + 0x18, poly++);

        setPolyFT4(poly);
        setSemiTrans(poly, 1);
        setShadeTex(poly, 1);
        setXY4(poly, Menu_Map_compassBounds[i].x - 6, Menu_Map_compassBounds[i].y - 6,
            Menu_Map_compassBounds[i].x + 7, Menu_Map_compassBounds[i].y - 6, Menu_Map_compassBounds[i].x - 6,
            Menu_Map_compassBounds[i].y + 7, Menu_Map_compassBounds[i].x + 7, Menu_Map_compassBounds[i].y + 7);
        setUV4(poly, 1, 137, 14, 137, 1, 150, 14, 150);
        setTPage(poly, 0, 1, 448, 256);

        if (i == 0) {
            setClut(poly, 976, 222);
        } else {
            setClut(poly, 960, 222);
        }

        legacy_ordering_table_scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

        AddPrim(((u_char*)(legacy_ordering_table_scratch[1])) + 0x18, poly++);
    }

    Menu_Map_ComputeCompassCorners(x, y, 15, 15, (angle - Menu_Map_cameraAngles.vz) + 0x200);
    Menu_Map_ComputeCompassCorners(
        Menu_Map_compassBounds[0].x, Menu_Map_compassBounds[0].y, 15, 17, angle - Menu_Map_cameraAngles.vz);

    setPolyFT4(poly);
    setSemiTrans(poly, 1);
    setShadeTex(poly, 1);
    setXY4(poly, Menu_Map_compassBounds[0].x, Menu_Map_compassBounds[0].y, Menu_Map_compassBounds[1].x,
        Menu_Map_compassBounds[1].y, Menu_Map_compassBounds[2].x, Menu_Map_compassBounds[2].y,
        Menu_Map_compassBounds[3].x, Menu_Map_compassBounds[3].y);
    setUV4(poly, 0x14, 0x88, 0x23, 0x88, 0x14, 0x99, 0x23, 0x99);
    setTPage(poly, 0, 0, 448, 256);
    setClut(poly, 992, 222);

    legacy_primitive_scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    legacy_ordering_table_scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

    AddPrim(((u_char*)(legacy_ordering_table_scratch[1])) + 0x18, poly++);

    legacy_primitive_scratch[0] = (void*)poly;
}

// decomp: MENU5.PRG _computeRotatedRect (src/MENU/MENU5.PRG/4D8.c:2377)
void Menu_Map_ComputeCompassCorners(int compass_center_x, int compass_center_y, int compass_half_width, int compass_half_height, int compass_angle)
{
    int i;
    int angle_sine = Core_Gte_Sin(compass_angle);
    int angle_cosine = Core_Gte_Cos(compass_angle);

    Menu_Map_compassBounds[0].x = (-compass_half_width * angle_cosine) + (compass_half_height * angle_sine);
    Menu_Map_compassBounds[0].y = (-compass_half_height * angle_cosine) - (compass_half_width * angle_sine);
    Menu_Map_compassBounds[1].x = (compass_half_width * angle_cosine) + (compass_half_height * angle_sine);
    Menu_Map_compassBounds[1].y = (-compass_half_height * angle_cosine) + (compass_half_width * angle_sine);
    Menu_Map_compassBounds[2].x = -Menu_Map_compassBounds[1].x;
    Menu_Map_compassBounds[2].y = -Menu_Map_compassBounds[1].y;
    Menu_Map_compassBounds[3].x = -Menu_Map_compassBounds[0].x;
    Menu_Map_compassBounds[3].y = -Menu_Map_compassBounds[0].y;

    for (i = 0; i < 4; ++i) {
        Menu_Map_compassBounds[i].x = Menu_Map_compassBounds[i].x / ONE / 2;
        Menu_Map_compassBounds[i].y = Menu_Map_compassBounds[i].y / ONE / 2;
        Menu_Map_compassBounds[i].x = Menu_Map_compassBounds[i].x + compass_center_x;
        Menu_Map_compassBounds[i].y = Menu_Map_compassBounds[i].y + compass_center_y;
    }
}

// decomp: MENU5.PRG _renderConnectingMapName (src/MENU/MENU5.PRG/4D8.c:2403)
void Menu_Map_DrawConnectingAreaLabel(int x, int y, int mapId)
{
    int linePivotX;
    Menu_Framework_MenuRow* menuItem;
    int areaVisited;
    int xOffset = 170 - x;

    if (xOffset < 0) {
        xOffset += 3;
    }

    linePivotX = xOffset >> 2;

    if (linePivotX < 0) {
        linePivotX = 170;
    } else {
        linePivotX = 170 - linePivotX;
    }

    Menu_Map_AppendLabelLine(x, y, linePivotX, Menu_Map_mapLabelY + 8);
    Menu_Map_AppendLabelLine(linePivotX, Menu_Map_mapLabelY + 8, 194, Menu_Map_mapLabelY + 8);

    areaVisited = State_roomVisitProgress.areaFlags[mapId >> 5] & (1 << (mapId & 0x1F));

    if (!areaVisited) {
        mapId = Text_Menu5_connectingMaps_Index_notVisited;
    }

    menuItem = Menu_Framework_SetMenuItem(
        0, 178, Menu_Map_mapLabelY, 144, 0, (char*)&Menu_Map_connectingMaps[Menu_Map_connectingMaps[mapId]]);
    menuItem->backgroundWidth = 8;

    Menu_Map_mapLabelY -= 16;

    Menu_Framework_DrawMenuItem(menuItem);
    Menu_Framework_MenuItem(0)->state = 0;
}

// decomp: MENU5.PRG _addMapLabel (src/MENU/MENU5.PRG/4D8.c:2441)
void Menu_Map_QueueLabel(int x, int y, int type, int labelId)
{
    Menu_Map_mapLabels[Menu_Map_mapLabelCount].x = x;
    Menu_Map_mapLabels[Menu_Map_mapLabelCount].y = y;
    Menu_Map_mapLabels[Menu_Map_mapLabelCount].type = type;
    Menu_Map_mapLabels[Menu_Map_mapLabelCount].id = labelId;
    ++Menu_Map_mapLabelCount;
}

// decomp: MENU5.PRG renderMapLabels (src/MENU/MENU5.PRG/4D8.c:2450)
void Menu_Map_DrawLabels(void)
{
    int i;

    if ((Menu_Map_menuCommandsState == 5) || (Menu_Map_triggerHideUi != 0)) {
        return;
    }

    for (i = 0; i < Menu_Map_mapLabelCount; ++i) {
        if (Menu_Map_mapLabels[i].type == 0) {
            Menu_Map_DrawDoorLabel(Menu_Map_mapLabels[i].x, Menu_Map_mapLabels[i].y, Menu_Map_mapLabels[i].id);
        }
    }

    for (i = 0; i < Menu_Map_mapLabelCount; ++i) {
        if (Menu_Map_mapLabels[i].type != 0) {
            Menu_Map_DrawConnectingAreaLabel(Menu_Map_mapLabels[i].x, Menu_Map_mapLabels[i].y, Menu_Map_mapLabels[i].id);
        }
    }
}

// decomp: MENU5.PRG func_8010839C (src/MENU/MENU5.PRG/4D8.c:2471)
int Menu_Map_ShouldDrawDoorStateMarker(int door_state_id, int zone_id, int map_id)
{
    u_int sigil_door_kind = door_state_id - 0x41;

    if ((sigil_door_kind < 7) && ((Field_IsRoomSigilDoorUnlocked(zone_id, map_id, sigil_door_kind) != 0))) {
        return 0;
    }
    return Audio_Seq_unusedStateFlags.opaque33F[door_state_id] < 2;
}

// MENU5.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Map_ResetMenu5Statics(void)
{
    {
        static const int initial = -1;
        memcpy((void*)&Menu_Map_currentMapRow, &initial, sizeof Menu_Map_currentMapRow);
    }
    {
        static const u_short initial[] = {
0 /* disc text table: loaded in Step 5 */
    };
        memcpy((void*)&Menu_Map_framePaling, &initial, sizeof Menu_Map_framePaling);
    }
    {
        static const RECT initial[2] = {};
        memcpy((void*)&Menu_Map_screenMenuBgBackRects, &initial, sizeof Menu_Map_screenMenuBgBackRects);
    }
}

}  // extern "C"
