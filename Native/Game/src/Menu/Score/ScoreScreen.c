// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Battle/TextBoxes.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Effects/Weather.h"
#include "Field/MapProgress.h"
#include "Field/RoomRendering.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Files.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Score/Encyclopedia.h"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreScreen.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/RoomDraw.h"
#include "Render/Sprites.h"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"
#include "State/Settings.h"
#include "State/StateFlags.h"

// decomp: _0 (_0@src/MENU/MENU9.PRG/238.c)
int Menu_Score_layoutPadding1;

// decomp: _1 (_1@src/MENU/MENU9.PRG/238.c)
int Menu_Score_layoutPadding2[2];

// decomp: _bgColorStops0 (_bgColorStops0@src/MENU/MENU9.PRG/238.c)
const CVECTOR Menu_Score_kBgColorStops0[] = { { 0, 65, 107 }, { 25, 130, 108 },
    { 64, 48, 102 }, { 64, 56, 32 } };

// decomp: _bgColorStops1 (_bgColorStops1@src/MENU/MENU9.PRG/238.c)
const CVECTOR Menu_Score_kBgColorStops1[] = { { 0, 5, 51 }, { 1, 40, 38 }, { 8, 8, 32 },
    { 16, 16, 8 } };

// decomp: _buttonHeld (_buttonHeld@src/MENU/MENU9.PRG/238.c)
int Menu_Score_buttonHeld;

// decomp: _cameraAngles (_cameraAngles@src/MENU/MENU9.PRG/238.c)
SVECTOR Menu_Score_cameraAngles;

// decomp: _cameraAnglesBackup (_cameraAnglesBackup@src/MENU/MENU9.PRG/238.c)
VECTOR Menu_Score_cameraAnglesBackup;

// decomp: _cameraDistance (_cameraDistance@src/MENU/MENU9.PRG/238.c)
int Menu_Score_cameraDistance;

// decomp: _cameraHeightOffset (_cameraHeightOffset@src/MENU/MENU9.PRG/238.c)
short Menu_Score_cameraHeightOffset;

// decomp: _cameraPositionBackup (_cameraPositionBackup@src/MENU/MENU9.PRG/238.c)
VECTOR Menu_Score_cameraPositionBackup;

// decomp: _characterRank (_characterRank@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_characterRank;

// decomp: _enemyDescriptions (_enemyDescriptions@src/MENU/MENU9.PRG/238.c)
u_short* Menu_Score_enemyDescriptions;

// decomp: _enemySkipDelay (_enemySkipDelay@src/MENU/MENU9.PRG/238.c)
short Menu_Score_enemySkipDelay;

// decomp: _hideUi (_hideUi@src/MENU/MENU9.PRG/238.c)
int Menu_Score_hideUi;

// decomp: _initCameraDistance (_initCameraDistance@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_initCameraDistance;

// decomp: _mapCompletion (_mapCompletion@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_mapCompletion;

// decomp: _menuState (_menuState@src/MENU/MENU9.PRG/238.c)
int Menu_Score_menuState;

// decomp: _menuText (_menuText@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_menuText[34]; /* contents: disc string table (ModuleData.c) */

// decomp: _monBinCdQueueSlot (_monBinCdQueueSlot@src/MENU/MENU9.PRG/238.c)
Content_DiscQueueSlot* Menu_Score_monBinCdQueueSlot;

// decomp: _monBinData (_monBinData@src/MENU/MENU9.PRG/238.c)
Menu_Score_EnemyEntry* Menu_Score_monBinData;

// decomp: _monBinFile (_monBinFile@src/MENU/MENU9.PRG/238.c)
const Content_DiscFile Menu_Score_kMonBinFile = { VS_MON_BIN_LBA, VS_MON_BIN_SIZE };

// decomp: local static of _renderScrollableMenuItem, at file scope so the module reset reaches it
static char Menu_Score_scrollableRowNavigationAnimState = 0;

// decomp: _nextState (_nextState@src/MENU/MENU9.PRG/238.c)
int Menu_Score_nextState;

// decomp: _noString (_noString@src/MENU/MENU9.PRG/238.c)
char Menu_Score_unusedNoString[] __attribute__((unused)) = "NO\0";

// decomp: _openedChestCount (_openedChestCount@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_openedChestCount;

// decomp: local static of _renderScrollableMenuItem, at file scope so the module reset reaches it
static char Menu_Score_scrollableRowPreviousFrameBuf = 0;

// decomp: _rankText (_rankText@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_rankText[192]; /* contents: disc string table (ModuleData.c) */

// decomp: _rotationColor (_rotationColor@src/MENU/MENU9.PRG/238.c)
int Menu_Score_rotationColor;

// decomp: _selectedEnemy (_selectedEnemy@src/MENU/MENU9.PRG/238.c)
short Menu_Score_selectedEnemy;

// decomp: _yesString (_yesString@src/MENU/MENU9.PRG/238.c)
char Menu_Score_unusedYesString[] __attribute__((unused)) = "YES\0";

// decomp: _zoomColor (_zoomColor@src/MENU/MENU9.PRG/238.c)
int Menu_Score_zoomColor;

// decomp: MENU9.PRG _renderScrollableMenuItem (src/MENU/MENU9.PRG/238.c:54)
void Menu_Score_RenderScrollableRow(Menu_Framework_MenuRow* menuItem)
{

    int w;
    int frameBuf;
    int animState;
    int bgWidth;
    int isScrollable;
    char texture_u;
    int brightness;
    int var_s0;
    int i;
    u_long* prim;
    u_char* menuText;
    u_int texture_v;
    u_int xy;
    u_long* before;
    int state;
    int gradient_intensity_offset;

    menuText = menuItem->text;
    frameBuf = 0;

    if (Render_frameBuf == 0) {
        frameBuf = 320;
    }

    xy = *(int*)&menuItem->x;
    before = Render_frameScratch.overlayOrderingTable + 8;
    w = menuItem->w;
    animState = menuItem->gradientState;
    isScrollable = menuItem->isScrollable - 1;
    bgWidth = menuItem->backgroundWidth;

    if (Render_frameBuf != Menu_Score_scrollableRowPreviousFrameBuf) {

        ++Menu_Score_scrollableRowNavigationAnimState;
        Menu_Score_scrollableRowPreviousFrameBuf = Render_frameBuf;

        if (Menu_Score_scrollableRowNavigationAnimState >= 12) {
            Menu_Score_scrollableRowNavigationAnimState = 0;
        }
    }

    state = menuItem->state;

    if ((state == 1) && (menuItem->x < 128u)) {

        var_s0 = Menu_Score_scrollableRowNavigationAnimState >> 2;

        if (isScrollable == 0) {
            var_s0 -= 5;
        } else {
            var_s0 = state - var_s0;
        }

        Render_QueueSpriteWithTexturePage(128,
            (menuItem->x - 14) | ((menuItem->y + var_s0) << 0x10), vs_getWH(16, 16),
            before - 1)[4] = (isScrollable << 4) | vs_getUV0Clut(0, 48, 912, 223);
    }

    i = menuItem->x + 6;

    while ((var_s0 = *menuText++) != Core_TextCode_PrintString) {
        if (var_s0 == Core_TextCode_Spacing) {
            i += *menuText++;
        } else {
            i = Menu_Framework_DrawFeatheredRowCharacter(
                var_s0 | (isScrollable << 0x1F), i, menuItem, before);
        }
    }

    if (w == 0) {
        return;
    }

    prim = Render_frameScratch.primitiveCursor;

    for (i = 0, gradient_intensity_offset = 0; i < 12; gradient_intensity_offset += 8, ++i) {
        if (isScrollable == 0) {
            var_s0 = gradient_intensity_offset + 32;
        } else {
            var_s0 = gradient_intensity_offset;
            var_s0 = 120 - gradient_intensity_offset;
        }

        prim[0] = (*before & 0xFFFFFF) | (6 << 24);
        prim[1] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_On);
        prim[2] = Menu_Framework_GradientStopColor(8 - animState, bgWidth, var_s0)
                | vs_getRGB0(Render_LegacyPrimitiveCode_LineG2SemiTrans, 0, 0, 0);
        prim[3] = xy;
        prim[4] = Menu_Framework_GradientStopColor(animState, bgWidth, var_s0);
        prim[5] = ((xy + w - 1) & 0xFFFF) | ((xy >> 0x10) << 0x10);
        prim[6] = vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off);

        *before = LegacyGpu_Handle(prim);
        prim += 7;
        xy += vs_getXY(0, 1);
    }

    Render_frameScratch.primitiveCursor = prim;

    if (isScrollable == 0) {
        Render_InsertThreeWordPrimitive(before, vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0),
            ((xy + 2) & 0xFFFF) | ((xy >> 0x10) << 0x10), w | vs_getWH(0, 2));
    }

    if (animState != 0) {
        menuItem->gradientState = animState - 1;
    }

    xy += -12 << 16;
    w |= vs_getWH(0, 1);
    var_s0 = (menuItem->x >= 128) << 7;

    for (i = 0; i < 12; ++i) {

        if (isScrollable == 0) {
            brightness = 120 - i * 8;
        } else {
            brightness = i * 8 + 32;
        }

        prim = Render_QueueSprite(brightness, xy, w, before);

        texture_u = xy - var_s0;
        texture_v = xy >> 0x10;
        xy += 1 << 16;

        prim[1] = ((u_int)(frameBuf + var_s0) >> 6)
                | vs_getTpage(
                    0, 0, Render_BlendMode_Subtract, Render_BlendMode_Add, Render_Dithering_Off);
        prim[4] = texture_u | (texture_v << 8);
    }
}

// decomp: MENU9.PRG _setRows (src/MENU/MENU9.PRG/238.c:196)
void Menu_Score_RestoreTopMenuCursor(int rowCount, int absoluteRow, char* text[])
{
    int rowTypes[rowCount];
    int i;
    char cursorBackup;

    for (i = 0; i < rowCount; ++i) {
        rowTypes[i] = 0;
    }

    if ((rowCount < 9) || (absoluteRow < 8)) {
        Menu_Framework_menuMemory.cursorMemories[2] = absoluteRow;
        Menu_Framework_menuMemory.cursorMemories[3] = 0;
    } else if (absoluteRow >= (rowCount - 8)) {
        Menu_Framework_menuMemory.cursorMemories[2] = absoluteRow - (rowCount - 9);
        Menu_Framework_menuMemory.cursorMemories[3] = rowCount - 9;
    } else {
        Menu_Framework_menuMemory.cursorMemories[2] = 4;
        Menu_Framework_menuMemory.cursorMemories[3] = absoluteRow - 4;
    }

    cursorBackup = Input_settings.cursorMemory;
    Input_settings.cursorMemory = 1;

    Menu_Framework_CreateMenuRows(rowCount, menuRowInfo(1, 0, 1), text, rowTypes);

    Input_settings.cursorMemory = cursorBackup;
}

// decomp: MENU9.PRG _getSelectedRow (src/MENU/MENU9.PRG/238.c:228)
int Menu_Score_GetSelectedTopMenuRow(void)
{
    int row = Menu_Framework_GetSelectedRow();

    if (row < -1) {
        Menu_Framework_ClearMenuExcept(5);
    } else if (row >= 0) {
        Menu_Framework_FlyOutRowsAndHoistSelection(Menu_Framework_menuMemory.cursorMemories[2], 1);
    }

    return row;
}

// decomp: MENU9.PRG _clearMenuExceptRow8 (src/MENU/MENU9.PRG/238.c:258)
void Menu_Score_ClearMenuExceptRow8(void) { Menu_Framework_ClearMenuExcept(8); }

// decomp: MENU9.PRG _menuReady (src/MENU/MENU9.PRG/238.c:264)
void Menu_Score_PollMenuReady(void) { Menu_Framework_AreMenuRowsIdle(); }

// decomp: MENU9.PRG vs_menu9_exec (src/MENU/MENU9.PRG/238.c:269)
int Menu_Score_UpdateScreen(u_char* state)
{
    static int Menu_Score_screenInitDelay;
    static int Menu_Score_screenCalculateProgressComplete;
    static int Menu_Score_screenSelectedRow;

    enum Menu_Score_ScoreOverlayState {
        Menu_Score_ScoreOverlayState_Init = 3,
        Menu_Score_ScoreOverlayState_InitData,
        Menu_Score_ScoreOverlayState_WaitMenuReady,
    };

    switch (*state) {
    case Menu_Score_ScoreOverlayState_Init:
        Menu_Framework_SetBackgroundFade(1);
        Menu_Score_screenInitDelay = 8;
        Menu_Score_screenCalculateProgressComplete = 0;
        *state = Menu_Score_ScoreOverlayState_InitData;
        break;

    case Menu_Score_ScoreOverlayState_InitData:
        if (Menu_Score_screenCalculateProgressComplete == 0) {
            Menu_Score_screenCalculateProgressComplete = Menu_Score_CalculateProgress();
        }

        if (Menu_Score_screenInitDelay != 0) {
            --Menu_Score_screenInitDelay;
            break;
        }

        if (Menu_Score_screenCalculateProgressComplete == 0) {
            break;
        }

        Field_SetFieldRenderingEnabled(0);
        Battle_SetHudHidden(1);

        Battle_hudSlideOffset = 0x200000;
        *state = Menu_Score_ScoreOverlayState_WaitMenuReady;
        // Fallthrough

    case Menu_Score_ScoreOverlayState_WaitMenuReady:
        *state += Menu_Framework_AreMenuRowsIdle();
        break;

    case 6:
        Menu_Score_screenSelectedRow = Menu_Score_Update();

        if (Menu_Score_screenSelectedRow != 0) {

            Menu_Framework_SetBackgroundPriority(-4, 128);
            Battle_SetHudHidden(0);

            Battle_hudSlideOffset = 0;

            Field_SetFieldRenderingEnabled(1);
            Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

            *state = 7;
        }
        break;

    case 7:
        Menu_Framework_SetBackgroundFade(0);
        *state = 8;
        break;

    case 8:
        if (Menu_Framework_backgroundFadeStep == 0) {

            if (Menu_Score_screenSelectedRow == 2) {
                Menu_Framework_menuState.currentMenu = 9;
            }

            Menu_Framework_hideMenu = 0;
            *state = 0;

            return 1;
        }
        break;
    }
    return 0;
}

// decomp: MENU9.PRG _calculateProgress (src/MENU/MENU9.PRG/238.c:418)
int Menu_Score_CalculateProgress(void)
{
    int i;
    int j;

    if (State_roomVisitProgress.roomFlags[1] & 0x800000) {
        State_roomVisitProgress.roomFlags[1] |= 0x400000;
    }

    Menu_Score_menuState = 0;
    Menu_Score_hideUi = 0;

    if (Input_settings.cursorMemory == 0) {
        Core_Memory_ZeroAlignedWords(&Battle_unusedMenu9CursorMemory, sizeof Battle_unusedMenu9CursorMemory);
    }

    Menu_Score_mapCompletion = 0;

    for (i = 0; i < 16; ++i) {
        for (j = 0; j < 32; ++j) {
            int v = 1;
            if (State_roomVisitProgress.roomFlags[i] & Field_countableRoomMask[i]
                & (v << j)) {
                ++Menu_Score_mapCompletion;
            }
        }
    }

    Menu_Score_openedChestCount = 0;

    for (i = 0; i < 64; ++i) {
        if (Audio_Seq_unusedStateFlags.chestsOpened[i] != 0) {
            ++Menu_Score_openedChestCount;
        }
    }

    Menu_Score_clearCount = Audio_Seq_unusedStateFlags.clearCount;

    if (Menu_Score_mapCompletion > Audio_Seq_unusedScoredata.mapCompletion) {
        Audio_Seq_unusedScoredata.mapCompletion = Menu_Score_mapCompletion;
    }
    if (Menu_Score_openedChestCount > Audio_Seq_unusedScoredata.openedChestCount) {
        Audio_Seq_unusedScoredata.openedChestCount = Menu_Score_openedChestCount;
    }

    Core_Memory_CopyAlignedWords(&Menu_Score_cameraPositionBackup,
        &((Render_FrameScratch*)Core_ScratchAddr(0))->camera.position,
        sizeof Menu_Score_cameraPositionBackup);
    Core_Memory_CopyAlignedWords(&Menu_Score_cameraAnglesBackup,
        &((Render_FrameScratch*)Core_ScratchAddr(0))->camera.angles.vx,
        sizeof Menu_Score_cameraAnglesBackup);
    Menu_Score_AwardTitles();
    Menu_Score_CalculateScore();
    Menu_Score_DetermineRiskbreakerRank();

    return 1;
}

// decomp: MENU9.PRG _menuInput (src/MENU/MENU9.PRG/238.c:548)
int Menu_Score_Update(void)
{
    static int Menu_Score_updateMenuAnimState;

    enum Menu_Score_ScoreMenuState {
        Menu_Score_ScoreMenuState_Init,
        Menu_Score_ScoreMenuState_InitData,
        Menu_Score_ScoreMenuState_TopMenu,
    };

    int _[8] __attribute__((unused));
    Menu_TextBoxState* textBox;
    Menu_Score_Row* gazette_row_cursor;
    int lStickX;
    int lStickY;
    int selectedRow;

    switch (Menu_Score_menuState) {
    case Menu_Score_ScoreMenuState_Init:
        Menu_Framework_SetBackgroundPriority(2047, 128);

        ++Menu_Score_menuState;
        break;

    case Menu_Score_ScoreMenuState_InitData:
        Menu_Framework_LoadActorPreview(0);
        Menu_Score_InitializeAshleyPreviewPose();

        Menu_Score_cameraDistance = 0x150;
        Menu_Score_cameraHeightOffset = 0x6B;
        Menu_Score_initCameraDistance = Actors_actorRecords[1]->menuCameraDistance;
        Menu_Score_cameraAngles.vx = 0xF40;
        Menu_Score_cameraAngles.vy = 0xE47;
        Menu_Score_cameraAngles.vz = 0;

        Menu_InitTextBox(0, 0, 0xB, 0xF8, 0x10, 1, 0xB, 0xF8);
        Menu_SetTextBoxText(0, (char*)&Menu_Score_rankText[Menu_Score_rankText[Menu_Score_characterRank]]);
        Menu_Score_UpdateTopMenu(1);

        Menu_Score_updateMenuAnimState = 0;
        ++Menu_Score_menuState;
        break;

    case Menu_Score_ScoreMenuState_TopMenu:
        Menu_Score_RenderOverview(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState < 8) {
            textBox = Menu_TextBox(0);
            textBox->y -= 6;
            textBox->yIndent -= 6;
            ++Menu_Score_updateMenuAnimState;
        }

        selectedRow = Menu_Score_UpdateTopMenu(0);

        if (selectedRow == -1) {
            return 0;
        }

        if (selectedRow == -3) {
            Menu_Score_nextState = 2;
        } else if (selectedRow == -2) {
            Menu_Score_nextState = 1;
        } else if (selectedRow == 3) {
            Menu_Score_monBinData = Core_Memory_AllocateFromHighEnd(Menu_Score_kMonBinFile.size);
            Menu_Score_monBinCdQueueSlot = Content_AllocateDiscRequest(&Menu_Score_kMonBinFile);

            Content_EnqueueDiscRequest(Menu_Score_monBinCdQueueSlot, Menu_Score_monBinData);

            Menu_Score_nextState = 0;
        } else {
            Menu_Score_nextState = 0;
        }

        Battle_unusedMenu9CursorMemory.gazetteRow =
            Menu_Framework_menuMemory.cursorMemories[2] + Menu_Framework_menuMemory.cursorMemories[3];

        Menu_DismissTextBox(0);

        ++Menu_Score_menuState;
        break;

    case 3:
        Menu_Score_RenderOverview(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState > 0) {

            textBox = Menu_TextBox(0);

            if (textBox != NULL) {
                textBox->y += 6;
                textBox->yIndent += 6;
            }

            --Menu_Score_updateMenuAnimState;
            break;
        }

        Menu_Framework_ReleaseActorPreview();

        if (Menu_Score_nextState == 0) {
            Menu_Score_menuState = (Battle_unusedMenu9CursorMemory.gazetteRow * 3) + 4;
            break;
        }

        Menu_Score_RestoreCamera();

        return Menu_Score_nextState;

    case 4:
        Menu_Framework_InitializeInformationBox();
        Menu_Score_CreateTitleRows();

        Menu_Score_updateMenuAnimState = 0;
        ++Menu_Score_menuState;
        break;

    case 5:
        if ((Battle_unusedMenu9CursorMemory.titleRow == 0)
            && (Battle_unusedMenu9CursorMemory.titlePage == 0)
            && (Input_buttonsPressed.allButtons & PADLup)) {

            Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            Battle_unusedMenu9CursorMemory.titlePage = 0x18;
            Battle_unusedMenu9CursorMemory.titleRow = 7;

        } else if ((Battle_unusedMenu9CursorMemory.titleRow == 7)
                   && (Battle_unusedMenu9CursorMemory.titlePage == 24)
                   && (Input_buttonsPressed.allButtons & PADLdown)) {

            Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            Battle_unusedMenu9CursorMemory.titlePage = 0;
            Battle_unusedMenu9CursorMemory.titleRow = 0;

        } else {
            if (Input_buttonRepeat & PADLup) {
                if (Battle_unusedMenu9CursorMemory.titleRow >= 2) {

                    Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
                    --Battle_unusedMenu9CursorMemory.titleRow;

                } else {
                    if (Battle_unusedMenu9CursorMemory.titlePage != 0) {

                        --Battle_unusedMenu9CursorMemory.titlePage;
                        Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);

                    } else if (Battle_unusedMenu9CursorMemory.titleRow != 0) {

                        --Battle_unusedMenu9CursorMemory.titleRow;
                        Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
                    }
                }
            } else if (Input_buttonRepeat & PADLdown) {
                if (Battle_unusedMenu9CursorMemory.titleRow >= 6U) {

                    if (Battle_unusedMenu9CursorMemory.titlePage < 0x18U) {

                        ++Battle_unusedMenu9CursorMemory.titlePage;
                        Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);

                    } else {
                        if (Battle_unusedMenu9CursorMemory.titleRow < 7) {

                            ++Battle_unusedMenu9CursorMemory.titleRow;
                            Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
                        }
                    }

                } else {
                    ++Battle_unusedMenu9CursorMemory.titleRow;
                    Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
                }
            }
        }
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

            Audio_PlaySfxCentered(0x7E, 6);
            Menu_Framework_DismissInformationBox();

            ++Menu_Score_menuState;

            if (Input_buttonsPressed.allButtons & PADRup) {
                Menu_Score_nextState = 1;
            } else {
                Menu_Score_nextState = 0;
            }
        }

        Menu_Score_RenderTitles(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState < 8) {
            ++Menu_Score_updateMenuAnimState;
        }

        break;

    case 6:
        Menu_Score_RenderTitles(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState <= 0) {
            if (Menu_Score_nextState == 0) {
                Menu_Score_menuState = 1;
                break;
            }

            Menu_Score_RestoreCamera();

            return 2;
        }

        --Menu_Score_updateMenuAnimState;
        break;

    case 7:
        Menu_Framework_InitializeInformationBox();
        Menu_Score_CreateStatisticRows();

        Menu_Score_updateMenuAnimState = 0;
        ++Menu_Score_menuState;
        break;

    case 8:
        if ((Battle_unusedMenu9CursorMemory.recordTimeRow == 0)
            && (Battle_unusedMenu9CursorMemory.recordTimePage == 0)
            && (Input_buttonsPressed.allButtons & PADLup)) {

            Audio_PlaySfxCentered(0x7E, 4);

            Battle_unusedMenu9CursorMemory.recordTimePage = 0xF;
            Battle_unusedMenu9CursorMemory.recordTimeRow = 7;

        } else if ((Battle_unusedMenu9CursorMemory.recordTimeRow == 7)
                   && (Battle_unusedMenu9CursorMemory.recordTimePage == 0xF)
                   && (Input_buttonsPressed.allButtons & PADLdown)) {

            Audio_PlaySfxCentered(0x7E, 4);

            Battle_unusedMenu9CursorMemory.recordTimePage = 0;
            Battle_unusedMenu9CursorMemory.recordTimeRow = 0;

        } else {
            if (Input_buttonRepeat & PADLup) {
                if (Battle_unusedMenu9CursorMemory.recordTimeRow >= 2) {

                    Audio_PlaySfxCentered(0x7E, 4);

                    --Battle_unusedMenu9CursorMemory.recordTimeRow;

                } else {
                    if (Battle_unusedMenu9CursorMemory.recordTimePage != 0) {

                        --Battle_unusedMenu9CursorMemory.recordTimePage;

                        Audio_PlaySfxCentered(0x7E, 4);

                    } else if (Battle_unusedMenu9CursorMemory.recordTimeRow != 0) {

                        --Battle_unusedMenu9CursorMemory.recordTimeRow;

                        Audio_PlaySfxCentered(0x7E, 4);
                    }
                }
            } else if (Input_buttonRepeat & PADLdown) {
                if (Battle_unusedMenu9CursorMemory.recordTimeRow >= 6) {
                    if (Battle_unusedMenu9CursorMemory.recordTimePage < 0xF) {

                        ++Battle_unusedMenu9CursorMemory.recordTimePage;

                        Audio_PlaySfxCentered(0x7E, 4);

                    } else {
                        if (Battle_unusedMenu9CursorMemory.recordTimeRow < 7) {

                            ++Battle_unusedMenu9CursorMemory.recordTimeRow;

                            Audio_PlaySfxCentered(0x7E, 4);
                        }
                    }
                } else {
                    ++Battle_unusedMenu9CursorMemory.recordTimeRow;

                    Audio_PlaySfxCentered(0x7E, 4);
                }
            }
        }
        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

            Audio_PlaySfxCentered(0x7E, 6);
            Menu_Framework_DismissInformationBox();

            ++Menu_Score_menuState;

            if (Input_buttonsPressed.allButtons & PADRup) {
                Menu_Score_nextState = 1;
            } else {
                Menu_Score_nextState = 0;
            }
        }

        Menu_Score_RenderStatistics(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState < 8) {
            ++Menu_Score_updateMenuAnimState;
        }
        break;

    case 9:
        Menu_Score_RenderStatistics(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState <= 0) {
            if (Menu_Score_nextState == 0) {
                Menu_Score_menuState = 1;
                break;
            }
            Menu_Score_RestoreCamera();
            return 2;
        }

        --Menu_Score_updateMenuAnimState;
        break;

    case 10:
        Menu_Framework_InitializeInformationBox();
        Menu_Score_CreateTimeTrialRows();

        ++Menu_Score_menuState;
        break;

    case 11:
        if (Input_buttonRepeat & PADLup) {

            Audio_PlaySfxCentered(0x7E, 4);

            Battle_unusedMenu9CursorMemory.gazettePage =
                (Battle_unusedMenu9CursorMemory.gazettePage - 1) & 7;
        } else {
            if (Input_buttonRepeat & PADLdown) {

                Audio_PlaySfxCentered(0x7E, 4);

                Battle_unusedMenu9CursorMemory.gazettePage =
                    (Battle_unusedMenu9CursorMemory.gazettePage + 1) & 7;
            }
        }

        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

            Audio_PlaySfxCentered(0x7E, 6);
            Menu_Framework_DismissInformationBox();
            Menu_Score_LeaveTimeTrialMenu();

            ++Menu_Score_menuState;

            if (Input_buttonsPressed.allButtons & PADRup) {
                Menu_Score_nextState = 1;
            } else {
                Menu_Score_nextState = 0;
            }
        }

        Menu_Score_RenderTimeTrialRecords(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState < 8) {
            ++Menu_Score_updateMenuAnimState;
        }
        break;

    case 12:
        Menu_Score_RenderTimeTrialRecords(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState > 0) {
            --Menu_Score_updateMenuAnimState;
            break;
        }

        for (Menu_Score_updateMenuAnimState = 0, gazette_row_cursor = Menu_Score_gazetteRows; Menu_Score_updateMenuAnimState < 8;
            ++Menu_Score_updateMenuAnimState, ++gazette_row_cursor) {
            if (gazette_row_cursor->animationState == 0) {
                Menu_Framework_DeactivateRow(Menu_Score_updateMenuAnimState);
            }
        }

        Menu_Score_updateMenuAnimState = 0;

        if (Menu_Score_nextState == 0) {
            Menu_Score_menuState = 1;
            break;
        }

        Menu_Score_RestoreCamera();
        return 2;

    case 13:
        if (Menu_Score_monBinCdQueueSlot->state == Content_DiscRequestState_Loaded) {

            Content_ReleaseDiscRequest(Menu_Score_monBinCdQueueSlot);

            Menu_Score_enemyDescriptions = (u_short*)(Menu_Score_monBinData + 150);

            Menu_Framework_InitializeInformationBox();
            Menu_Score_InitializeEnemyList();

            Menu_Score_updateMenuAnimState = 0;
            ++Menu_Score_menuState;
        }
        break;

    case 14:
        if ((Battle_unusedMenu9CursorMemory.encyclopaediaRow == 0)
            && (Battle_unusedMenu9CursorMemory.encyclopaediaPage == 0)
            && (Input_buttonsPressed.allButtons & PADLup)) {

            Audio_PlaySfxCentered(0x7E, 4);
            Battle_unusedMenu9CursorMemory.encyclopaediaPage = 0x46;
            Battle_unusedMenu9CursorMemory.encyclopaediaRow = 7;

        } else if ((Battle_unusedMenu9CursorMemory.encyclopaediaRow == 7)
                   && (Battle_unusedMenu9CursorMemory.encyclopaediaPage == 0x46)
                   && (Input_buttonsPressed.allButtons & PADLdown)) {

            Audio_PlaySfxCentered(0x7E, 4);
            Battle_unusedMenu9CursorMemory.encyclopaediaPage = 0;
            Battle_unusedMenu9CursorMemory.encyclopaediaRow = 0;

        } else if (Input_buttonRepeat & PADLup) {

            if (Battle_unusedMenu9CursorMemory.encyclopaediaRow > 1) {
                Audio_PlaySfxCentered(0x7E, 4);
                --Battle_unusedMenu9CursorMemory.encyclopaediaRow;
            } else if (Battle_unusedMenu9CursorMemory.encyclopaediaPage != 0) {
                --Battle_unusedMenu9CursorMemory.encyclopaediaPage;
                Audio_PlaySfxCentered(0x7E, 4);
            } else if (Battle_unusedMenu9CursorMemory.encyclopaediaRow != 0) {
                --Battle_unusedMenu9CursorMemory.encyclopaediaRow;
                Audio_PlaySfxCentered(0x7E, 4);
            }

        } else if (Input_buttonRepeat & PADLdown) {
            if (Battle_unusedMenu9CursorMemory.encyclopaediaRow >= 6) {
                if (Battle_unusedMenu9CursorMemory.encyclopaediaPage < 0x46) {

                    ++Battle_unusedMenu9CursorMemory.encyclopaediaPage;
                    Audio_PlaySfxCentered(0x7E, 4);

                } else {
                    if (Battle_unusedMenu9CursorMemory.encyclopaediaRow < 7) {

                        ++Battle_unusedMenu9CursorMemory.encyclopaediaRow;
                        Audio_PlaySfxCentered(0x7E, 4);
                    }
                }
            } else {
                ++Battle_unusedMenu9CursorMemory.encyclopaediaRow;
                Audio_PlaySfxCentered(0x7E, 4);
            }
        }
        if (Input_buttonsPressed.allButtons & PADRright) {
            if (Menu_Score_monBinData[Battle_unusedMenu9CursorMemory.encyclopaediaPage
                            + Battle_unusedMenu9CursorMemory.encyclopaediaRow]
                    .unlocked
                != 0) {

                Audio_PlaySfxCentered(0x7E, 5);
                Menu_Score_LoadEnemyPreview(
                    Menu_Score_monBinData[Battle_unusedMenu9CursorMemory.encyclopaediaPage
                                + Battle_unusedMenu9CursorMemory.encyclopaediaRow]
                        .zudId);

                ++Menu_Score_menuState;
                Menu_Score_nextState = 2;

            } else {
                Audio_PlaySfxCentered(0x7E, 7);
            }

        } else if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

            Audio_PlaySfxCentered(0x7E, 6);
            Menu_Framework_DismissInformationBox();

            ++Menu_Score_menuState;

            if (Input_buttonsPressed.allButtons & PADRup) {
                Menu_Score_nextState = 1;
            } else {
                Menu_Score_nextState = 0;
            }
        }

        Menu_Score_RenderEnemyList(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState < 8) {
            ++Menu_Score_updateMenuAnimState;
        }

        break;

    case 15:
        Menu_Score_RenderEnemyList(Menu_Score_updateMenuAnimState);

        if (Menu_Score_updateMenuAnimState <= 0) {

            if (Menu_Score_nextState == 2) {
                Menu_Score_menuState = 19;
                break;
            }

            if (Menu_Score_nextState != 1) {
                Core_Memory_FreeFromHighEnd(Menu_Score_monBinData);
                Menu_Score_menuState = 1;
                break;
            }

            Core_Memory_FreeFromHighEnd(Menu_Score_monBinData);
            Menu_Score_RestoreCamera();
            return 2;
        }

        --Menu_Score_updateMenuAnimState;
        break;

    case 19:
        if (Menu_Score_LoadEnemyPreview(Menu_Score_monBinData[Battle_unusedMenu9CursorMemory.encyclopaediaPage
                                         + Battle_unusedMenu9CursorMemory.encyclopaediaRow]
                    .zudId)
            == 0) {

            int initial_enemy_camera_distance = Actors_actorRecords[1]->menuCameraDistance;
            Menu_Score_initCameraDistance = initial_enemy_camera_distance;
            Menu_Score_cameraHeightOffset = Actors_actorRecords[1]->collisionHeight;
            Menu_Score_cameraDistance = (initial_enemy_camera_distance << 0x10) >> 0xE;

            if (Menu_Score_cameraDistance < 0x280) {
                Menu_Score_cameraDistance = 0x280;
            } else if (Menu_Score_cameraDistance > 0x1800) {
                Menu_Score_cameraDistance = 0x1800;
            }

            Menu_Score_cameraAngles.vy = 0x100;
            Menu_Score_buttonHeld = 0;
            Menu_Score_cameraAngles.vx = 0;
            Menu_Score_cameraAngles.vz = 0;
            Menu_Score_selectedEnemy = Battle_unusedMenu9CursorMemory.encyclopaediaPage
                           + Battle_unusedMenu9CursorMemory.encyclopaediaRow;

            Menu_Score_ShowEnemyDescription();
            Menu_Framework_SetMenuVisible(Menu_Score_hideUi == 0);

            ++Menu_Score_menuState;
        }
        break;

    case 20:
        if (Menu_Score_monBinData[Menu_Score_selectedEnemy].prev != Menu_Score_selectedEnemy) {
            Menu_Score_enemyNavigationState = (Menu_Score_enemyNavigationState + 1) & 0xF;

            Menu_Score_UpdateEnemyNavigation(0);
            Menu_Score_UpdateEnemyNavigation(1);
        }

        if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

            Audio_PlaySfxCentered(0x7E, 6);

            if (Menu_Score_hideUi != 0) {
                Menu_Framework_SetMenuVisible(1);
            }

            ++Menu_Score_menuState;

            if (Input_buttonsPressed.allButtons & PADRup) {
                Menu_Framework_DismissInformationBox();
                Menu_Score_nextState = 1;
            } else {
                Menu_Score_nextState = 0;
            }
        }

        Menu_Score_rotationColor = vs_getRGB888(128, 128, 128);
        Menu_Score_zoomColor = vs_getRGB888(128, 128, 128);

        if ((lStickX = Input_stickPosBuf.lStickX - 0x80, ABS(lStickX) > 0x40)
            || (lStickY = Input_stickPosBuf.lStickY - 0x80, ABS(lStickY) > 0x40)) {

            Menu_Score_zoomColor = vs_getRGB888(32, 32, 32);

            if (lStickX < -0x40) {
                Menu_Score_cameraAngles.vy -= 0x40;
            } else if (lStickX > 0x40) {
                Menu_Score_cameraAngles.vy += 0x40;
            }

            if ((Input_stickPosBuf.lStickY - 0x80) < -0x40) {
                Menu_Score_cameraAngles.vx += 0x40;
            } else if ((Input_stickPosBuf.lStickY - 0x80) > 0x40) {
                Menu_Score_cameraAngles.vx -= 0x40;
            }

            if (Menu_Score_cameraAngles.vx < 0) {
                Menu_Score_cameraAngles.vx = 0;
            }

            if (Menu_Score_cameraAngles.vx > 0x400) {
                Menu_Score_cameraAngles.vx = 0x400;
            }

        } else {
            if (Input_previousButtons & PADL2) {
                int rStickX;
                int rStickY;
                Menu_Score_zoomColor = vs_getRGB888(32, 32, 32);

                if ((rStickX = Input_stickPosBuf.rStickX - 0x80, ABS(rStickX) > 0x40)
                    || (rStickY = Input_stickPosBuf.rStickY - 0x80,
                        ABS(rStickY) > 0x40)) {

                    if (rStickX < -0x40) {
                        Menu_Score_cameraAngles.vy -= 0x40;
                    } else if (rStickX > 0x40) {
                        Menu_Score_cameraAngles.vy += 0x40;
                    }

                    if ((Input_stickPosBuf.rStickY - 0x80) < -0x40) {
                        Menu_Score_cameraAngles.vx += 0x40;
                    } else if ((Input_stickPosBuf.rStickY - 0x80) > 0x40) {
                        Menu_Score_cameraAngles.vx -= 0x40;
                    }

                    if (Menu_Score_cameraAngles.vx < 0) {
                        Menu_Score_cameraAngles.vx = 0;
                    }

                    if (Menu_Score_cameraAngles.vx > 0x400) {
                        Menu_Score_cameraAngles.vx = 0x400;
                    }

                } else {
                    if (Input_previousButtons & PADLup) {
                        Menu_Score_cameraAngles.vx += 0x40;
                    } else if (Input_previousButtons & PADLdown) {
                        Menu_Score_cameraAngles.vx -= 0x40;
                    }

                    if (Input_previousButtons & PADLright) {
                        Menu_Score_cameraAngles.vy = Menu_Score_cameraAngles.vy + 0x40;
                    } else if (Input_previousButtons & PADLleft) {
                        Menu_Score_cameraAngles.vy = Menu_Score_cameraAngles.vy - 0x40;
                    }

                    if (Menu_Score_cameraAngles.vx < 0) {
                        Menu_Score_cameraAngles.vx = 0;
                    }

                    if (Menu_Score_cameraAngles.vx > 0x400) {
                        Menu_Score_cameraAngles.vx = 0x400;
                    }
                }

            } else if (Input_previousButtons & PADR2) {
                int rStickX;
                int rStickY;
                Menu_Score_rotationColor = vs_getRGB888(32, 32, 32);

                if ((rStickX = Input_stickPosBuf.rStickX - 0x80, ABS(rStickX) > 0x40)
                    || (rStickY = Input_stickPosBuf.rStickY - 0x80,
                        ABS(rStickY) > 0x40)) {

                    if ((Input_stickPosBuf.rStickY - 0x80) < -0x40) {
                        Menu_Score_cameraDistance -= 4;
                    } else if ((Input_stickPosBuf.rStickY - 0x80) > 0x40) {
                        Menu_Score_cameraDistance += 4;
                    }

                } else {
                    if (Input_previousButtons & PADLup) {
                        Menu_Score_cameraDistance -= 0x40;
                    } else if (Input_previousButtons & PADLdown) {
                        Menu_Score_cameraDistance += 0x40;
                    }
                }

                if (Menu_Score_cameraDistance >= 0x280) {
                    if (Menu_Score_cameraDistance > 0x1800) {
                        Menu_Score_cameraDistance = 0x1800;
                    }
                } else {
                    Menu_Score_cameraDistance = 0x280;
                }
            }
        }

        if (Input_buttonsPressed.allButtons & PADselect) {
            Menu_Framework_SetMenuVisible(Menu_Score_hideUi);
            Menu_Score_hideUi ^= 1;
        }

        if ((Input_previousButtons & (PADL1 | PADR1)) != (PADL1 | PADR1)) {

            if (Input_buttonsPressed.allButtons & (PADL1 | PADR1)) {
                Menu_Score_enemySkipDelay = 0;
            }

            if (Input_buttonRepeat & PADR1) {
                if (Menu_Score_enemySkipDelay == 0) {
                    if (Menu_Score_monBinData[Menu_Score_selectedEnemy].next != Menu_Score_selectedEnemy) {

                        Audio_PlaySfxCentered(0x7E, 11);

                        Menu_Score_selectedEnemy = Menu_Score_monBinData[Menu_Score_selectedEnemy].next;
                        Menu_Score_buttonHeld = PADR1;

                        Menu_Score_ShowEnemyDescription();
                    }
                }
                Menu_Score_enemySkipDelay = (Menu_Score_enemySkipDelay + 1) & 3;
            }

            if (Input_buttonRepeat & PADL1) {
                if (Menu_Score_enemySkipDelay == 0) {
                    if (Menu_Score_monBinData[Menu_Score_selectedEnemy].prev != Menu_Score_selectedEnemy) {

                        Audio_PlaySfxCentered(0x7E, 11);

                        Menu_Score_selectedEnemy = Menu_Score_monBinData[Menu_Score_selectedEnemy].prev;
                        Menu_Score_buttonHeld = PADL1;

                        Menu_Score_ShowEnemyDescription();
                    }
                }

                Menu_Score_enemySkipDelay = (Menu_Score_enemySkipDelay + 1) & 3;
            }
        }

        Menu_Score_RenderEnemyViewer(Menu_Score_updateMenuAnimState, 0);

        if (Menu_Score_updateMenuAnimState < 8) {
            textBox = Menu_TextBox(0);
            textBox->x -= 0x10;
            textBox->xIndent -= 0x10;
            ++Menu_Score_updateMenuAnimState;
        }

        if (Input_releasedButtons & Menu_Score_buttonHeld) {

            if (Menu_Score_buttonHeld == PADR1) {
                Menu_Score_buttonHeld = 1;
            } else {
                Menu_Score_buttonHeld = 2;
            }

            Menu_Score_rotationColor = vs_getRGB888(32, 32, 32);
            Menu_Score_zoomColor = vs_getRGB888(32, 32, 32);
            Menu_Score_menuState = 0x19;
        }
        break;

    case 21:
        Menu_Score_rotationColor = vs_getRGB888(32, 32, 32);
        Menu_Score_zoomColor = vs_getRGB888(32, 32, 32);

        Menu_Score_RenderEnemyViewer(Menu_Score_updateMenuAnimState, 0);

        if (Menu_Score_updateMenuAnimState > 0) {

            textBox = Menu_TextBox(0);

            if (textBox != NULL) {
                textBox->x += 0x10;
                textBox->xIndent += 0x10;
            }
            --Menu_Score_updateMenuAnimState;
            break;
        }

        Menu_Score_FreeEnemyPreview();

        if (Menu_Score_nextState != 0) {
            Core_Memory_FreeFromHighEnd(Menu_Score_monBinData);
            Menu_Score_RestoreCamera();
            return 2;
        }

        Menu_Score_selectedEnemy = Battle_unusedMenu9CursorMemory.encyclopaediaPage
                       + Battle_unusedMenu9CursorMemory.encyclopaediaRow;

        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Score_enemyDescriptions[Menu_Score_enemyDescriptions[Menu_Score_selectedEnemy]]);

        Menu_Score_menuState = 0xE;
        break;

    case 25:
        Menu_Score_UpdateEnemyNavigation(2);
        Menu_Score_UpdateEnemyNavigation(3);
        Menu_Score_RenderEnemyViewer(Menu_Score_updateMenuAnimState, Menu_Score_buttonHeld);

        if (Menu_Score_updateMenuAnimState > 0) {
            --Menu_Score_updateMenuAnimState;
            break;
        }

        Menu_Score_FreeEnemyPreview();
        Menu_Score_LoadEnemyPreview(Menu_Score_monBinData[Menu_Score_selectedEnemy].zudId);

        ++Menu_Score_menuState;
        break;

    case 26:
        Menu_Score_UpdateEnemyNavigation(2);
        Menu_Score_UpdateEnemyNavigation(3);
        Menu_Score_RenderEnemyViewer(Menu_Score_updateMenuAnimState, 3);

        if (Menu_Score_LoadEnemyPreview(Menu_Score_monBinData[Battle_unusedMenu9CursorMemory.encyclopaediaPage
                                         + Battle_unusedMenu9CursorMemory.encyclopaediaRow]
                    .zudId)
            == 0) {

            int reselected_enemy_camera_distance = Actors_actorRecords[1]->menuCameraDistance;
            Menu_Score_initCameraDistance = reselected_enemy_camera_distance;
            Menu_Score_cameraHeightOffset = Actors_actorRecords[1]->collisionHeight;
            Menu_Score_cameraDistance = (reselected_enemy_camera_distance << 0x10) >> 0xE;

            if (Menu_Score_cameraDistance < 0x280) {
                Menu_Score_cameraDistance = 0x280;
            } else if (Menu_Score_cameraDistance >= 0x1801) {
                Menu_Score_cameraDistance = 0x1800;
            }
            ++Menu_Score_menuState;
        }
        break;

    case 27:
        Menu_Score_UpdateEnemyNavigation(2);
        Menu_Score_UpdateEnemyNavigation(3);
        Menu_Score_RenderEnemyViewer(Menu_Score_updateMenuAnimState, 3 - Menu_Score_buttonHeld);

        if (Menu_Score_updateMenuAnimState < 8) {
            ++Menu_Score_updateMenuAnimState;
        } else {
            Menu_Score_buttonHeld = 0;
            Menu_Score_menuState = 0x14;
        }

        break;
    }

    return 0;
}

// decomp: MENU9.PRG _restoreCamera (src/MENU/MENU9.PRG/238.c:1402)
void Menu_Score_RestoreCamera(void)
{
    Core_Memory_CopyAlignedWords(&((Render_FrameScratch*)Core_ScratchAddr(0))->camera.position,
        &Menu_Score_cameraPositionBackup, sizeof Menu_Score_cameraPositionBackup);
    Core_Memory_CopyAlignedWords(&((Render_FrameScratch*)Core_ScratchAddr(0))->camera.angles.vx,
        &Menu_Score_cameraAnglesBackup, sizeof Menu_Score_cameraAnglesBackup);
}

// decomp: MENU9.PRG _topMenu (src/MENU/MENU9.PRG/238.c:1430)
int Menu_Score_UpdateTopMenu(int initialize)
{
    static int Menu_Score_topMenuState;
    static int Menu_Score_topMenuSelectedRow;

    char* menuText[8];
    int i;

    if (initialize != 0) {
        Menu_Score_topMenuState = 0;
        Menu_Score_topMenuSelectedRow = 0;
    }

    switch (Menu_Score_topMenuState) {
    case 0:
        for (i = 0; i < 4; ++i) {
            menuText[i * 2] = (char*)&Menu_Score_menuText[Menu_Score_menuText[i]];
            menuText[i * 2 + 1] = NULL;
        }

        Menu_Score_RestoreTopMenuCursor(4, Battle_unusedMenu9CursorMemory.gazetteRow, menuText);

        ++Menu_Score_topMenuState;
        break;

    case 1:
        Menu_Score_topMenuSelectedRow = Menu_Score_GetSelectedTopMenuRow();

        if (Menu_Score_topMenuSelectedRow == -1) {
            break;
        }

        return Menu_Score_topMenuSelectedRow;
    }

    return -1;
}

// decomp: MENU9.PRG _buildCameraMatrix (src/MENU/MENU9.PRG/238.c:1474)
void Menu_Score_BuildViewerCamera(MATRIX* cameraMatrix)
{
    SVECTOR negPosition;
    SVECTOR position;
    MATRIX pitchMatrix;
    int angle0;
    int angle1;
    int angle2;
    Render_FrameScratch* scratch;

    angle0 = (angle1 = Core_Gte_Sin(Menu_Score_cameraAngles.vy));
    angle1 = Core_Gte_Cos(Menu_Score_cameraAngles.vx);
    position.vx = (((angle0 * Menu_Score_cameraDistance) / ONE) * angle1) / ONE;

    angle0 = Core_Gte_Cos(Menu_Score_cameraAngles.vy);
    angle1 = Core_Gte_Cos(Menu_Score_cameraAngles.vx);
    position.vz = (((angle0 * -Menu_Score_cameraDistance) / ONE) * angle1) / ONE;

    angle2 = Core_Gte_Sin(Menu_Score_cameraAngles.vx) * -Menu_Score_cameraDistance;
    position.vy = (angle2 / ONE) - (Menu_Score_cameraHeightOffset / 2);

    scratch = (Render_FrameScratch*)Core_ScratchAddr(0);
    setVector(&scratch->camera.position, position.vx * ONE, position.vy * ONE,
        position.vz * ONE);
    scratch->camera.angles.vx = Menu_Score_cameraAngles.vx;
    scratch->camera.angles.vy = Menu_Score_cameraAngles.vy;
    scratch->camera.angles.vz = Menu_Score_cameraAngles.vz;

    Menu_Framework_BuildYawMatrix(cameraMatrix, Menu_Score_cameraAngles.vy);
    Menu_Framework_BuildPitchMatrix(&pitchMatrix, Menu_Score_cameraAngles.vx);
    Core_Math_MultiplyMatrixIntoRight(&pitchMatrix, cameraMatrix);

    applyVector(&negPosition, position.vx, position.vy, position.vz, = -);

    Core_Gte_ApplyMatrix(cameraMatrix, &negPosition, (VECTOR*)&cameraMatrix->t);
}

// decomp: MENU9.PRG _renderEnemyInfoBg (src/MENU/MENU9.PRG/238.c:1552)
void Menu_Score_DrawEnemyBanner(int x, int y, int w, int h, int color)
{
    int i;
    int scanline_width;
    LINE_G2* line;
    POLY_F4* poly;

    line = Render_frameScratch.primitiveCursor;

    for (i = y, scanline_width = w; i < ((y + h) - 1); ++i, --scanline_width) {
        setLineG2(line);
        setXY2(line, x, i, (x + scanline_width) - 1, i);
        setRGB0(line, Menu_Score_kBgColorStops0[color].r, Menu_Score_kBgColorStops0[color].g,
            Menu_Score_kBgColorStops0[color].b);
        setRGB1(line, Menu_Score_kBgColorStops1[color].r, Menu_Score_kBgColorStops1[color].g,
            Menu_Score_kBgColorStops1[color].b);

        AddPrim(Render_frameScratch.orderingTable + 0x1C, line++);
    }

    x += 2;
    y += 2;
    poly = (POLY_F4*)line;

    setPolyF4(poly);
    setXY4(poly, x, y, (x + w) - 1, y, x, (y + h) - 1, (x + w) - h, (y + h) - 1);
    setRGB0(poly, 0, 0, 0);

    AddPrim(Render_frameScratch.orderingTable + 0x1C, poly++);

    Render_frameScratch.primitiveCursor = poly;

    _insertTPage(7, getTPage(0, 3, 0, 0));
}

// decomp: MENU9.PRG _renderGradientQuad (src/MENU/MENU9.PRG/238.c:1594)
void Menu_Score_DrawGradientPanel(int x, int y, int w, int h, int color)
{
    char b1;
    char g0;
    char r1;
    char g1;
    char r0;
    char b0;
    POLY_G4* polyG4;
    POLY_F4* polyF4;
    void** scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

    int right_x = (x + w) - 1;
    int bottom_y = (y + h) - 1;

    polyG4 = scratch[0];
    setPolyG4(polyG4);
    r0 = Menu_Score_kBgColorStops0[color].r;
    g0 = Menu_Score_kBgColorStops0[color].g;
    b0 = Menu_Score_kBgColorStops0[color].b;
    r1 = Menu_Score_kBgColorStops1[color].r;
    g1 = Menu_Score_kBgColorStops1[color].g;
    b1 = Menu_Score_kBgColorStops1[color].b;

    setXY4(polyG4, x, y, right_x, y, x, bottom_y, right_x, bottom_y);
    setRGB0(polyG4, r0, g0, b0);
    setRGB1(polyG4, r1, g1, b1);
    setRGB2(polyG4, r0, g0, b0);
    setRGB3(polyG4, r1, g1, b1);

    AddPrim(scratch[1] + 0x1C, polyG4++);

    x += 2;
    y += 2;

    polyF4 = (POLY_F4*)polyG4;
    setPolyF4(polyF4);
    setXY4(polyF4, x, y, (x + w) - 1, y, x, (y + h) - 1, (x + w) - 1, (y + h) - 1);
    setRGB0(polyF4, 0, 0, 0);

    AddPrim(scratch[1] + 0x1C, polyF4++);

    scratch[0] = polyF4;

    _insertTPage(7, getTPage(0, 3, 0, 0));
}

// decomp: MENU9.PRG _renderFooterBg (src/MENU/MENU9.PRG/238.c:1645)
void Menu_Score_DrawFooterBackground(int x)
{
    POLY_F4* polyF4;
    void** scratch = (void**)Core_ScratchAddr(0);
    POLY_G4* polyG4 = scratch[0];

    setPolyG4(polyG4);
    setXY4(polyG4, 0, x, 320, x, 0, x + 8, 320, x + 8);
    setRGB0(polyG4, 0, 0, 0);
    setRGB1(polyG4, 0, 0, 0);
    setRGB2(polyG4, 128, 128, 128);
    setRGB3(polyG4, 128, 128, 128);
    setSemiTrans(polyG4, 1);

    AddPrim(scratch[1] + 0x1C, polyG4++);

    polyF4 = (POLY_F4*)polyG4;

    setPolyF4(polyF4);
    setXY4(polyF4, 0, x + 8, 320, x + 8, 0, 240, 320, 240);
    setRGB0(polyF4, 0, 0, 0);

    AddPrim(scratch[1] + 0x1C, polyF4++);

    scratch[0] = polyF4;

    _insertTPage(7, getTPage(0, 2, 0, 0));
}

// decomp: MENU9.PRG _renderTopMenu (src/MENU/MENU9.PRG/238.c:1679)
void Menu_Score_RenderOverview(int animStep)
{
    Render_ScreenOffset saved_projection_offset;
    Render_ScreenOffset overview_projection_offset;
    char score_text[128];
    MATRIX viewer_camera_matrix;
    void** scratch;

    if (animStep < 8) {
        Menu_Score_DrawViewerEdgeFade();
    }

    Render_GetProjectionScreenOffset(&saved_projection_offset);

    overview_projection_offset.x = 0x80 - ((8 - animStep) << 5);
    overview_projection_offset.y = 0xF0;

    Render_SetProjectionScreenOffset(&overview_projection_offset);
    Menu_Score_BuildViewerCamera(&viewer_camera_matrix);
    Menu_Framework_DrawActorPreview(&viewer_camera_matrix);
    Render_SetProjectionScreenOffset(&saved_projection_offset);

    sprintf(score_text, "#%ld", Menu_Score_score);

    scratch = (void**)Core_ScratchAddr(0);

    Render_DrawCompactTextColor("SCORE", (((animStep * 16) - 120) & 0xFFFF) | (16 << 16),
        vs_getRGB888(128, 128, 128), scratch[1] + 0x1C);
    Render_DrawCompactTextColor(score_text, ((animStep * 16) & 0xFFFF) | (16 << 16),
        vs_getRGB888(128, 128, 128), scratch[1] + 0x1C);

    Menu_Score_DrawGradientPanel((animStep * 16) - 128, 16, 136, 12, 2);
    Render_DrawCompactTextColor("RISK   BREAKER   RANK",
        (((animStep * 16) - 120) & 0xFFFF) | (184 << 16), vs_getRGB888(128, 128, 128),
        scratch[1] + 0x1C);

    Menu_Score_DrawGradientPanel((animStep * 16) - 128, 184, 136, 12, 1);
    Menu_Score_DrawFooterBackground(236 - (animStep * 6));
}

// decomp: MENU9.PRG _renderScreenEdgeFade (src/MENU/MENU9.PRG/238.c:2146)
void Menu_Score_DrawViewerEdgeFade(void)
{
    void** scratch = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    POLY_G4* poly = scratch[0];

    setPolyG4(poly);
    setXY4(poly, 0, 0, 64, 0, 0, 240, 64, 240);
    setRGB0(poly, 128, 128, 128);
    setRGB1(poly, 0, 0, 0);
    setRGB2(poly, 128, 128, 128);
    setRGB3(poly, 0, 0, 0);
    setSemiTrans(poly, 1);

    AddPrim(scratch[1] + 0x20, poly++);

    setPolyG4(poly);
    setXY4(poly, 256, 0, 320, 0, 256, 240, 320, 240);
    setRGB0(poly, 0, 0, 0);
    setRGB1(poly, 128, 128, 128);
    setRGB2(poly, 0, 0, 0);
    setRGB3(poly, 128, 128, 128);
    setSemiTrans(poly, 1);

    AddPrim(scratch[1] + 32, poly++);

    scratch[0] = poly;

    _insertTPage(8, getTPage(0, 2, 0, 0));
}

// MENU9.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Score_ScoreScreen_ResetMenu9Statics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Score_scrollableRowNavigationAnimState, &initial, sizeof Menu_Score_scrollableRowNavigationAnimState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Score_scrollableRowPreviousFrameBuf, &initial, sizeof Menu_Score_scrollableRowPreviousFrameBuf);
    }
}

