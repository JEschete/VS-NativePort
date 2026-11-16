// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/SfxLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/MapDrawingLive.hpp"
#include "Menu/MapLoadingLive.hpp"
#include "Menu/MapNavigationLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/UiPacketsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Field/Map.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Manual.h"
#include "Menu/Map.h"
#include "Render/Display.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
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
        Menu_Map_screenMenuBgBackup = (u_long *)(VS::Core::Memory::AllocateLiveFromHighEnd(0xB600));
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
        VS::Menu::map::LiveLoadBackground(1);

        *state = 4;
        break;

    case 4:
        if (VS::Menu::map::LiveLoadBackground(0) == 0) {
            break;
        }

        *state = 5;

        VS::Menu::Framework::SetLiveBackgroundFade(1);

        Menu_Map_screenArmfileLoadDelay = 8;
        Menu_Map_screenArmFileLoaded = 0;
        // Fallthrough

    case 5:
        if (Menu_Map_screenArmFileLoaded == 0) {
            Menu_Map_screenArmFileLoaded = VS::Menu::map::LiveLoadSceneMap();
        }

        if (Menu_Map_screenArmfileLoadDelay != 0) {
            --Menu_Map_screenArmfileLoadDelay;
            break;
        } else if (Menu_Map_screenArmFileLoaded) {
            VS::Field::LiveSetFieldRenderingEnabled(0);
            VS::Battle::SetLiveHudHidden(1);

            Battle_hudSlideOffset = 0x200000;
            *state = 6;
        } else {
            break;
        }

        // Fallthrough

    case 6:
        *state += VS::Menu::Framework::AreLiveMenuRowsIdle();
        break;

    case 7:
        Menu_Map_menuResult = Menu_Map_UpdateMapMenu();

        if (Menu_Map_menuResult != 0) {
            VS::Menu::Framework::SetLiveBackgroundPriority(-4, 128);
            VS::Battle::SetLiveHudHidden(0);

            Battle_hudSlideOffset = 0;

            VS::Field::LiveSetFieldRenderingEnabled(1);
            VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
            VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);

            Menu_Map_screenArmfileLoadDelay = 10;
            *state = 8;
        }

        break;

    case 8:
        VS::Menu::Framework::SetLiveBackgroundFade(0);
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
            VS::Render::LoadLiveClutColors((u_short*)Menu_Map_screenMenuBgBackup, 3, 0, 0x100);
            VS::Render::UploadLiveImageRectangle(0x010002A0, Menu_Map_screenMenuBgBackup + 0x80, 0xF00060);

            *state = 10;
        }
        break;

    case 10:
        if (Menu_Map_menuResult == 2) {
            Menu_Framework_menuState.currentMenu = 6;
        }

        VS::Core::Memory::FreeLiveFromHighEnd(Menu_Map_screenMenuBgBackup);

        Menu_Framework_hideMenu = 0;
        *state = 0;
        return 1;
    }
    return 0;
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
        VS::Menu::Framework::SetLiveBackgroundPriority(2046, 128);
    } else {
        VS::Menu::Framework::SetLiveBackgroundPriority(2046, (Menu_Map_backgroundBrightness + 128) / 2);
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
            VS::Menu::InitLiveTextBox(0, 0, 60, 224, 16, 1, 60, 224);
            VS::Menu::SetLiveTextBoxText(0, Menu_Map_roomNamesTable[Menu_Map_currentRoomIndex].name);
            ++Menu_Map_menuState;
        }

        Menu_Map_DrawFrame();
        break;

    case Menu_Map_MapMenuState_ProcessInput:
        controls_state = 0;

        if (Menu_Map_triggerHideUi != Menu_Map_hideUi) {
            Menu_Map_hideUi = Menu_Map_triggerHideUi;
            VS::Menu::Framework::SetLiveMenuVisible(Menu_Map_triggerHideUi == 0);
        }

        if (Input_buttonsPressed.allButtons & PADRright) {
            controls_state = 5;

            VS::Audio::LivePlaySfxCentered(0x7E, 5);

            Menu_Map_menuState = Menu_Map_MapMenuState_DisplayMapMenu;

            if (Menu_Map_hideUi != 0) {
                Menu_Map_hideUi = 0;
                VS::Menu::Framework::SetLiveMenuVisible(1);
            }

            VS::Menu::map::LiveUpdateAreaSelection(1);

        } else if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

            VS::Audio::LivePlaySfxCentered(0x7E, 6);

            if (Input_buttonsPressed.allButtons & PADRup) {
                Menu_Map_quitState = 2;
            }

            Menu_Map_menuState = Menu_Map_MapMenuState_Quit;

            VS::Menu::DismissLiveTextBox(0);

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
                    Menu_Map_currentRoomIndex = VS::Menu::map::LiveInitialRoomIndex((Field_MapScene *)(Battle_unusedSceneBuffer));

                    VS::Menu::map::LiveCenterOnRoom((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);

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
                            VS::Menu::map::LiveSelectAdjacentVisitedRoom(1);
                            VS::Menu::map::LiveSetRoomCenterTarget((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);
                        } else if (Input_buttonRepeat & 4) {
                            VS::Menu::map::LiveSelectAdjacentVisitedRoom(-1);
                            VS::Menu::map::LiveSetRoomCenterTarget((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);
                        }
                    }

                    if (roomIndex != Menu_Map_currentRoomIndex) {
                        VS::Menu::SetLiveTextBoxText(0, Menu_Map_roomNamesTable[Menu_Map_currentRoomIndex].name);
                    }
                }
            }
        }

        Menu_Map_menuCommandsState = controls_state;

        Menu_Map_DrawFrame();
        break;

    case Menu_Map_MapMenuState_DisplayMapMenu:
        Menu_Map_DrawFrame();

        Menu_Map_selectedMap = area_selection_result = VS::Menu::map::LiveUpdateAreaSelection(0);

        if (area_selection_result == -1) {
            break;
        } else if (area_selection_result == -3) {

            Menu_Map_menuState = Menu_Map_MapMenuState_Quit;
            Menu_Map_quitState = 2;

            VS::Menu::DismissLiveTextBox(0);

        } else if (area_selection_result == -2) {
            Menu_Map_menuState = Menu_Map_MapMenuState_ProcessInput;

            VS::Menu::map::LiveShowAreaName((char*)&Menu_Map_mapNames[Menu_Map_mapNames[Menu_Map_currentScene]],
                Menu_Map_currentScene == Menu_Map_currentSceneRow);
        } else {

            Menu_Map_isPaling = Audio_Seq_unusedStateFlags.mapPaling[Menu_Map_currentScene];
            Menu_Map_currentScene = Menu_Map_sceneRowIdMap[area_selection_result];
            Menu_Map_menuState = Menu_Map_MapMenuState_InitNewMap;

            Menu_Map_armFileBuff = VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);
            Menu_Map_sceneCdQueueSlot =
                VS::Content::AllocateLiveDiscRequest(&Menu_Map_kSceneArmFiles[Menu_Map_currentScene]);
            if (Menu_Map_sceneCdQueueSlot != NULL) {
                VS::Content::EnqueueLiveDiscRequest(Menu_Map_sceneCdQueueSlot, Menu_Map_armFileBuff);
            } else {
                Menu_Map_sceneCdQueueSlot = VS::Content::CompleteLiveRefusedDiscRequest(&Menu_Map_kSceneArmFiles[Menu_Map_currentScene], Menu_Map_armFileBuff);
            }
        }
        break;

    case Menu_Map_MapMenuState_Quit:
        Menu_Map_uiAnimationState = 2;

        if (Menu_Map_backgroundBrightness >= 128) {
            VS::Core::Memory::FreeLiveFromHighEnd(Battle_unusedSceneBuffer);

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

            VS::Content::ReleaseLiveDiscRequest(Menu_Map_sceneCdQueueSlot);
            VS::Core::Memory::FreeLiveFromHighEnd(Battle_unusedSceneBuffer);

            Battle_unusedSceneBuffer =
                VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);

            VS::Core::Memory::CopyLiveAlignedWords(
                Battle_unusedSceneBuffer, Menu_Map_armFileBuff, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);
            VS::Core::Memory::FreeLiveFromHighEnd(Menu_Map_armFileBuff);

            Menu_Map_roomNamesTable = VS::Field::ResolveLiveSceneRoomData(Battle_unusedSceneBuffer, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);

            VS::Field::RestoreLiveSceneRoomVisits(Battle_unusedSceneBuffer, Menu_Map_kSceneArmFiles[Menu_Map_currentScene].size);
            VS::Menu::map::LiveScaleRoomVertices((Field_MapScene *)(Battle_unusedSceneBuffer), sizeof(Field_MapScene));

            Menu_Map_currentRoomIndex = VS::Menu::map::LiveInitialRoomIndex((Field_MapScene *)(Battle_unusedSceneBuffer));

            VS::Menu::map::LiveCenterOnRoom((Field_MapScene *)(Battle_unusedSceneBuffer), Menu_Map_currentRoomIndex);
            VS::Menu::SetLiveTextBoxText(0, Menu_Map_roomNamesTable[Menu_Map_currentRoomIndex].name);

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

    VS::Menu::map::LiveAnimateCentering((Field_MapScene *)(Battle_unusedSceneBuffer));

    return 0;
}

// decomp: MENU5.PRG _renderScreen (src/MENU/MENU5.PRG/4D8.c:905)
void Menu_Map_DrawFrame(void)
{

    DR_STP* stp;
    void** pScratch = (void**)Core_ScratchAddr(0);
    State_StateFlags* flags;
    int scene;

    VS::Menu::map::LiveDrawControls();

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
        VS::Menu::SetLiveTextBoxText(0, (char*)&Menu_Map_framePaling[Menu_Map_framePaling[0]]);
        return;
    }

    Menu_Map_ApplyCamera();
    Menu_Map_DarkenBackground();

    if (Menu_Map_isPaling == 0) {
        VS::Menu::map::LiveDrawVisitedRooms((Field_MapScene *)(Battle_unusedSceneBuffer));
    }
}

// decomp: MENU5.PRG _setCamera (src/MENU/MENU5.PRG/4D8.c:948)
void Menu_Map_ApplyCamera(void)
{
    SVECTOR negative_camera_position;
    MATRIX view_rotation;
    MATRIX sp38;

    Menu_Map_cameraPos.vx =
        (((VS::Core::Math::Sine(Menu_Map_cameraAngles.vy) * Menu_Map_cameraDistance) / ONE) * VS::Core::Math::Cosine(Menu_Map_cameraAngles.vx))
        / ONE;
    Menu_Map_cameraPos.vz =
        (((VS::Core::Math::Cosine(Menu_Map_cameraAngles.vy) * -Menu_Map_cameraDistance) / ONE) * VS::Core::Math::Cosine(Menu_Map_cameraAngles.vx))
        / ONE;
    Menu_Map_cameraPos.vy = (VS::Core::Math::Sine(Menu_Map_cameraAngles.vx) * -Menu_Map_cameraDistance) / ONE;

    VS::Menu::Framework::BuildLiveYawMatrix(&view_rotation, Menu_Map_cameraAngles.vy);
    VS::Menu::Framework::BuildLivePitchMatrix(&sp38, Menu_Map_cameraAngles.vx);
    VS::Core::Math::ApplyMultiplyMatrixIntoRight(sp38, view_rotation);

    applyVector(&negative_camera_position, Menu_Map_cameraPos.vx, Menu_Map_cameraPos.vy, Menu_Map_cameraPos.vz, = -);

    Core_Gte_ApplyMatrix(&view_rotation, &negative_camera_position, (VECTOR*)sp38.t);
    Core_Gte_SetRotMatrix(&view_rotation);
    Core_Gte_SetTransMatrix(&sp38);
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
            x = VS::Core::Math::Sine((Menu_Map_menuFramesCounter + y) << 6);
            if (x < 0) {
                x += 127;
            }
            x = x >> 7;
        } else {
            x = VS::Core::Math::Sine((Menu_Map_menuFramesCounter + y) << 6);
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
            x = VS::Core::Math::Sine((Menu_Map_menuFramesCounter + y) << 6);
            if (x < 0) {
                x += 127;
            }
            x = (x >> 7) + 192;
        } else {
            int offset;
            x = VS::Core::Math::Sine((Menu_Map_menuFramesCounter + y) << 6);
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
