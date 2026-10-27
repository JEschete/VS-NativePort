// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Sfx.h"
#include "Battle/Hud.h"
#include "Battle/TextBoxes.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Effects/Weather.h"
#include "Event/ScriptAudio.h"
#include "Field/Locations.h"
#include "Field/Manual.h"
#include "Field/RoomRendering.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/Files.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Manual.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Render.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"

// decomp: local static of vs_menuE_exec, at file scope so the module reset reaches it
static char Menu_Manual_openDelay = 0;

// decomp: _0 (_0@src/MENU/MENUE.PRG/494.c)
u_short Menu_Manual_layoutPadding1 __attribute__((unused)) = 0x0380;

// decomp: _colorStops0 (_colorStops0@src/MENU/MENUE.PRG/494.c)
CVECTOR const Menu_Manual_kColorStops0[] = { { 0, 0x20, 0x50, 0 }, { 0x19, 0x82, 0x6C, 0 },
    { 0x40, 0x30, 0x66, 0 }, { 0x40, 0x38, 0x20, 0 } };

// decomp: _colorStops1 (_colorStops1@src/MENU/MENUE.PRG/494.c)
CVECTOR const Menu_Manual_kColorStops1[] = { { 0, 0x5, 0x33, 0 }, { 0x1, 0x28, 0x26, 0 },
    { 0x8, 0x8, 0x20, 0 }, { 0x10, 0x10, 0x8, 0 } };

// decomp: _helpAssets (_helpAssets@src/MENU/MENUE.PRG/494.c)
u_long* Menu_Manual_helpAssets;

// decomp: _helpAssetsCdQueue (_helpAssetsCdQueue@src/MENU/MENUE.PRG/494.c)
Content_DiscQueueSlot* Menu_Manual_helpAssetsCdQueue;

// decomp: _helpFileCdFiles (_helpFileCdFiles@src/MENU/MENUE.PRG/494.c)
Content_DiscFile const Menu_Manual_kHelpFileCdFiles[] = { mkHfoPair(HELP01), mkHfoPair(HELP02),
    mkHfoPair(HELP03), mkHfoPair(HELP04), mkHfoPair(HELP05), mkHfoPair(HELP06),
    mkHfoPair(HELP07), mkHfoPair(HELP08), mkHfoPair(HELP09), mkHfoPair(HELP10),
    mkHfoPair(HELP11), mkHfoPair(HELP12), mkHfoPair(HELP13), mkHfoPair(HELP14) };

// decomp: _helpPageLoadCounter (_helpPageLoadCounter@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_helpPageLoadCounter;

// decomp: _helpText (_helpText@src/MENU/MENUE.PRG/494.c)
int* Menu_Manual_helpText;

// decomp: _helpTextCdQueue (_helpTextCdQueue@src/MENU/MENUE.PRG/494.c)
Content_DiscQueueSlot* Menu_Manual_helpTextCdQueue;

// decomp: _lineCount (_lineCount@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_lineCount;

// decomp: _menuBgChunkWidths (_menuBgChunkWidths@src/MENU/MENUE.PRG/494.c)
Menu_Manual_BackgroundChunkWidths const Menu_Manual_kMenuBgChunkWidths = { 128, 128, 64 };

// decomp: _menuBgTransparencies (_menuBgTransparencies@src/MENU/MENUE.PRG/494.c)
Menu_Manual_BackgroundTransparencies const Menu_Manual_kMenuBgTransparencies[] = { { 128, 115, 121, 88 },
    { 76, 64, 53, 42 }, { 33, 24, 17, 11 }, { 6, 3, 1, 0 } };

// decomp: _menuText (_menuText@src/MENU/MENUE.PRG/494.c)
u_short Menu_Manual_menuText[516]; /* contents: disc string table (ModuleData.c) */

// decomp: _pageArrowAnimState (_pageArrowAnimState@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_pageArrowAnimState;

// decomp: _scrollPosition (_scrollPosition@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_scrollPosition;

// decomp: _selectedRow (_selectedRow@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_selectedRow;

// decomp: _showMenuState (_showMenuState@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_showMenuState;

// decomp: _topMenu_state (_topMenu_state@src/MENU/MENUE.PRG/494.c)
int Menu_Manual_topMenuState;

// decomp: local static of vs_menuE_exec, at file scope so the module reset reaches it
static char Menu_Manual_screenIsInit = 0;

// decomp: local static of vs_menuE_exec, at file scope so the module reset reaches it
static int Menu_Manual_screenMenuResult = 0;

// decomp: MENUE.PRG _setMenuRows (src/MENU/MENUE.PRG/494.c:76)
void Menu_Manual_SetTopicRows(int rowCount, int highlightedRow, char** strings)
{
    int rowTypes[rowCount];
    int i;
    char cursor;

    for (i = 0; i < rowCount; ++i) {
        rowTypes[i] = 0;
    }

    rowTypes[rowCount - 1] |= 4;

    if ((rowCount < 9) || (highlightedRow < 8)) {
        Menu_Framework_menuMemory.cursorMemories[4] = highlightedRow;
        Menu_Framework_menuMemory.cursorMemories[5] = 0;
    } else if (highlightedRow >= (rowCount - 8)) {
        Menu_Framework_menuMemory.cursorMemories[4] = highlightedRow - (rowCount - 9);
        Menu_Framework_menuMemory.cursorMemories[5] = rowCount - 9;
    } else {
        Menu_Framework_menuMemory.cursorMemories[4] = 4;
        Menu_Framework_menuMemory.cursorMemories[5] = highlightedRow - 4;
    }
    cursor = Input_settings.cursorMemory;
    Input_settings.cursorMemory = 1;
    Menu_Framework_CreateMenuRows(rowCount, 0x102, strings, rowTypes);
    Input_settings.cursorMemory = cursor;
}

// decomp: MENUE.PRG _getSelectedRow (src/MENU/MENUE.PRG/494.c:104)
int Menu_Manual_GetSelectedTopicRow(void)
{
    int row;

    row = Menu_Framework_GetSelectedRow();
    if (row < -1) {
        Menu_Framework_ClearMenuExcept(5);
        return row;
    }
    if (row >= 0) {
        Menu_Framework_FlyOutRowsAndHoistSelection(Menu_Framework_menuMemory.cursorMemories[4], 1);
    }
    return row;
}

// decomp: MENUE.PRG func_80102E9C (src/MENU/MENUE.PRG/494.c:132)
void Menu_Manual_ClearMenuExceptRow8(void) { Menu_Framework_ClearMenuExcept(8); }

// decomp: MENUE.PRG _menuReady (src/MENU/MENUE.PRG/494.c:134)
void Menu_Manual_PollMenuReady(void) { Menu_Framework_AreMenuRowsIdle(); }

// decomp: MENUE.PRG vs_menuE_exec (src/MENU/MENUE.PRG/494.c:142)
int Menu_Manual_UpdateScreen(u_char* state)
{
    enum Menu_Manual_ManualOverlayState { Menu_Manual_ManualOverlayState_None, Menu_Manual_ManualOverlayState_Init = 3, Menu_Manual_ManualOverlayState_InitMenu, Menu_Manual_ManualOverlayState_WaitReady, Menu_Manual_ManualOverlayState_ShowMenu, Menu_Manual_ManualOverlayState_ShutDown, Menu_Manual_ManualOverlayState_Exit };

    static short Menu_Manual_screenLayoutPadding __attribute__((unused)) = 0x54;

    switch (*state) {
    case Menu_Manual_ManualOverlayState_Init:
        Menu_Framework_SetBackgroundFade(1);
        Menu_Manual_openDelay = 8;
        Menu_Manual_screenIsInit = 0;
        *state = Menu_Manual_ManualOverlayState_InitMenu;
        break;
    case Menu_Manual_ManualOverlayState_InitMenu:
        if (Menu_Manual_screenIsInit == 0) {
            Menu_Manual_screenIsInit = Menu_Manual_InitHelpState();
        }
        if (Menu_Manual_openDelay != 0) {
            Menu_Manual_openDelay -= 1;
        } else if (Menu_Manual_screenIsInit != 0) {
            Field_SetFieldRenderingEnabled(0);
            Battle_SetHudHidden(1);
            Battle_hudSlideOffset = 0x200000;
            *state = Menu_Manual_ManualOverlayState_WaitReady;
        }
        break;
    case Menu_Manual_ManualOverlayState_WaitReady:
        *state += Menu_Framework_AreMenuRowsIdle();
        break;
    case Menu_Manual_ManualOverlayState_ShowMenu:
        Menu_Manual_screenMenuResult = Menu_Manual_UpdateHelpPages();
        if (Menu_Manual_screenMenuResult == 0) {
            break;
        }
        Menu_Framework_SetBackgroundPriority(-4, 128);
        Battle_SetHudHidden(0);
        Battle_hudSlideOffset = 0;
        Field_SetFieldRenderingEnabled(1);
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        *state = Menu_Manual_ManualOverlayState_ShutDown;
        break;
    case Menu_Manual_ManualOverlayState_ShutDown:
        Menu_Framework_SetBackgroundFade(0);
        *state = Menu_Manual_ManualOverlayState_Exit;
        break;
    case Menu_Manual_ManualOverlayState_Exit:
        if (Menu_Framework_backgroundFadeStep != 0) {
            break;
        }
        if (Menu_Manual_screenMenuResult == 2) {
            Menu_Framework_menuState.currentMenu = 10;
        }
        Menu_Framework_hideMenu = 0;
        *state = Menu_Manual_ManualOverlayState_None;
        return 1;
    }
    return 0;
}

// decomp: MENUE.PRG _initMenuState (src/MENU/MENUE.PRG/494.c:219)
int Menu_Manual_InitHelpState(void)
{
    if (Input_settings.cursorMemory == 0) {
        Core_Memory_ZeroAlignedWords(&Field_manualDisplayState, sizeof Field_manualDisplayState);
    }
    Render_ReserveTexturePageRange(0x1D, 3, 5);
    Menu_Manual_showMenuState = 0;
    Menu_Manual_scrollPosition = 0;
    Menu_Manual_lineCount = 0;
    Menu_Manual_helpText = NULL;
    Menu_Manual_helpAssets = NULL;
    return 1;
}

// decomp: MENUE.PRG _showMenu (src/MENU/MENUE.PRG/494.c:245)
int Menu_Manual_UpdateHelpPages(void)
{
    enum Menu_Manual_ManualMenuState {
        Menu_Manual_ManualMenuState_Init,
        Menu_Manual_ManualMenuState_InitMenu,
        Menu_Manual_ManualMenuState_GetSelection,
        Menu_Manual_ManualMenuState_LoadSubMenu,
        Menu_Manual_ManualMenuState_InitSubMenu,
        Menu_Manual_ManualMenuState_ShowSubMenu,
        Menu_Manual_ManualMenuState_ExitToMenu,
        Menu_Manual_ManualMenuState_ExitToBattle,
    };

    char charBuf[60];
    int lastPage;
    int selectedRow;
    int currentPage;
    Render_FrameScratch* frame_scratch;
    int cdState;

    switch (Menu_Manual_showMenuState) {
    case Menu_Manual_ManualMenuState_Init:
        Menu_Framework_SetBackgroundPriority(2046, 128);
        ++Menu_Manual_showMenuState;
        break;
    case Menu_Manual_ManualMenuState_InitMenu:
        Menu_Framework_InitializeInformationBox();
        Menu_Manual_UpdateTopicMenu(1);
        ++Menu_Manual_showMenuState;
        break;
    case Menu_Manual_ManualMenuState_GetSelection:
        selectedRow = Menu_Manual_UpdateTopicMenu(0);
        if (selectedRow == -1) {
            break;
        }
        if (selectedRow < 0) {
            if (selectedRow == -3) {
                Menu_Manual_showMenuState = Menu_Manual_ManualMenuState_ExitToBattle;
            } else {
                Menu_Manual_showMenuState = Menu_Manual_ManualMenuState_ExitToMenu;
            }
        } else {
            Menu_Manual_showMenuState = Menu_Manual_ManualMenuState_LoadSubMenu;
            if (Menu_Manual_helpText != NULL) {
                Core_Memory_FreeFromHighEnd(Menu_Manual_helpText);
            }
            if (Menu_Manual_helpAssets != NULL) {
                Core_Memory_FreeFromHighEnd(Menu_Manual_helpAssets);
            }

            Menu_Manual_helpText = Core_Memory_AllocateFromHighEnd(Menu_Manual_kHelpFileCdFiles[selectedRow * 2 + 1].size);
            Menu_Manual_helpAssets = Core_Memory_AllocateFromHighEnd(Menu_Manual_kHelpFileCdFiles[selectedRow * 2].size);
            Menu_Manual_helpAssetsCdQueue =
                Content_AllocateDiscRequest(&Menu_Manual_kHelpFileCdFiles[selectedRow * 2]);
            Content_EnqueueDiscRequest(Menu_Manual_helpAssetsCdQueue, Menu_Manual_helpAssets);
            Menu_Manual_helpTextCdQueue =
                Content_AllocateDiscRequest(&Menu_Manual_kHelpFileCdFiles[selectedRow * 2 + 1]);
            Content_EnqueueDiscRequest(Menu_Manual_helpTextCdQueue, Menu_Manual_helpText);
        }
        Field_manualDisplayState.currentManual =
            Menu_Framework_menuMemory.cursorMemories[4] + Menu_Framework_menuMemory.cursorMemories[5];
        Menu_Framework_DismissInformationBox();
        break;
    case Menu_Manual_ManualMenuState_LoadSubMenu:
        cdState = Menu_Manual_helpAssetsCdQueue->state;
        if (cdState != Content_DiscRequestState_Loaded) {
            break;
        }
        Content_ReleaseDiscRequest(Menu_Manual_helpAssetsCdQueue);
        Menu_Manual_UploadSpriteTiles(Menu_Manual_helpAssets + 2, Menu_Manual_helpAssets[0]);
        Menu_Manual_UploadCluts(Menu_Manual_helpAssets + ((Menu_Manual_helpAssets[0] << 5) + 2), Menu_Manual_helpAssets[1]);
        if (Menu_Manual_helpAssets != NULL) {
            Core_Memory_FreeFromHighEnd(Menu_Manual_helpAssets);
        }
        Menu_Manual_helpAssets = NULL;
        Menu_Manual_showMenuState = Menu_Manual_ManualMenuState_InitSubMenu;
        break;
    case Menu_Manual_ManualMenuState_InitSubMenu:
        if (Menu_Manual_helpTextCdQueue->state != Content_DiscRequestState_Loaded) {
            break;
        }
        Content_ReleaseDiscRequest(Menu_Manual_helpTextCdQueue);
        Menu_InitTextBox(0, 0, 8, 0x34, 0x19, 0xA, 8, 0x34);
        Menu_Manual_scrollPosition =
            Field_manualDisplayState
                .scrollPositions[Field_manualDisplayState.currentManual];
        Menu_Manual_lineCount = Menu_Manual_LastPageLineOffset();
        Menu_Manual_showMenuState = Menu_Manual_ManualMenuState_ShowSubMenu;
        Menu_Manual_helpPageLoadCounter = 0;
        break;
    case Menu_Manual_ManualMenuState_ShowSubMenu:
        currentPage = Menu_Manual_scrollPosition / 10;
        lastPage = Menu_Manual_lineCount / 10;
        if (Input_buttonsPressed.allButtons & (PADRright | PADRdown)) {
            Menu_Manual_showMenuState = Menu_Manual_ManualMenuState_InitMenu;
        }
        if (Input_buttonsPressed.allButtons & PADRup) {
            Menu_Manual_showMenuState = 7;
        }
        if (Input_buttonsPressed.allButtons & PADstart) {
            currentPage = 0;
            Menu_Manual_scrollPosition = 0;
        } else if (Input_previousButtons & PADRleft) {
            if ((Input_buttonsPressed.allButtons & PADLup) && (currentPage == 0)
                && (lastPage > 0)) {
                currentPage = lastPage;
                Menu_Manual_scrollPosition = lastPage * 10;
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonsPressed.allButtons & PADLdown)
                       && (currentPage == lastPage) && (lastPage > 0)) {
                currentPage = 0;
                Menu_Manual_scrollPosition = 0;
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonRepeat & PADLup) && (currentPage > 0)) {
                --currentPage;
                Menu_Manual_scrollPosition = currentPage * 10;
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonRepeat & PADLdown) && (currentPage < lastPage)) {
                ++currentPage;
                Menu_Manual_scrollPosition = currentPage * 10;
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            }
        } else {
            if ((Input_buttonsPressed.allButtons & PADLup) && (Menu_Manual_scrollPosition == 0)
                && (Menu_Manual_lineCount > 0)) {
                Menu_Manual_scrollPosition = Menu_Manual_lineCount;
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if (Input_buttonsPressed.allButtons & PADLdown
                       && (Menu_Manual_scrollPosition == Menu_Manual_lineCount) && (Menu_Manual_scrollPosition > 0)) {
                Menu_Manual_scrollPosition = 0;
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonRepeat & PADLup) && (Menu_Manual_scrollPosition > 0)) {
                --Menu_Manual_scrollPosition;
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonRepeat & PADLdown)
                       && (Menu_Manual_scrollPosition < Menu_Manual_lineCount)) {
                ++Menu_Manual_scrollPosition;
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            }
        }
        frame_scratch = &Render_frameScratch;
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, 8, 16, ((u_long*)frame_scratch->orderingTable) + 6);
        Render_DrawCompactTextColor("TURN    PAGE", 0x12001C, 0x808080, ((u_long*)frame_scratch->orderingTable) + 6);
        Menu_Manual_DrawControlsBackground(0x10, 0x12, 0x60, 0xC);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Circle, 8, 34, ((u_long*)frame_scratch->orderingTable) + 6);
        Render_DrawCompactTextColor("MENU", 0x24001C, 0x808080, ((u_long*)frame_scratch->orderingTable) + 6);
        Menu_Manual_DrawControlsBackground(0x10, 0x24, 0x40, 0xC);
        Render_DrawCompactTextColor("PAGE", 0xC400D8, 0x808080, ((u_long*)frame_scratch->orderingTable) + 7);
        sprintf(charBuf, "#%d/", currentPage + 1);
        Render_DrawCompactTextColor(charBuf, 0xC40118, 0x808080, ((u_long*)frame_scratch->orderingTable) + 7);
        sprintf(charBuf, "#%d", lastPage + 1);
        Render_DrawCompactTextColor(charBuf, 0xC40130, 0x808080, ((u_long*)frame_scratch->orderingTable) + 7);
        Field_manualDisplayState
            .scrollPositions[Field_manualDisplayState.currentManual] =
            Menu_Manual_scrollPosition;
        if (Menu_Manual_showMenuState != Menu_Manual_ManualMenuState_ShowSubMenu) {
            if (Input_buttonsPressed.allButtons & PADRright) {
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
            } else {
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            }
            Menu_TextBox(0)->state = 0;
            if (Menu_Manual_helpText != NULL) {
                Core_Memory_FreeFromHighEnd(Menu_Manual_helpText);
            }
            Menu_Manual_helpText = NULL;
            break;
        }
        Menu_Manual_DrawPageBackground(8, 52, 310, 136, 0);
        Menu_Manual_pageArrowAnimState = (Menu_Manual_pageArrowAnimState + 1) & 0xF;
        if (Menu_Manual_scrollPosition > 0) {
            Menu_Manual_DrawUpperScrollFade();
        }
        if (Menu_Manual_scrollPosition < Menu_Manual_lineCount) {
            Menu_Manual_DrawLowerScrollFade();
        }
        if (Menu_Manual_helpPageLoadCounter < 8) {
            ++Menu_Manual_helpPageLoadCounter;
        } else {
            Menu_Manual_DrawPageContent();
        }
        Menu_Manual_CopyPageText();
        break;
    case Menu_Manual_ManualMenuState_ExitToMenu:
        if (Menu_Manual_helpText != NULL) {
            Core_Memory_FreeFromHighEnd(Menu_Manual_helpText);
        }
        if (Menu_Manual_helpAssets != NULL) {
            Core_Memory_FreeFromHighEnd(Menu_Manual_helpAssets);
        }
        Render_RetireTexturePageRange(0x1D, 3, 5);
        return 1;
    case Menu_Manual_ManualMenuState_ExitToBattle:
        if (Menu_Manual_helpText != NULL) {
            Core_Memory_FreeFromHighEnd(Menu_Manual_helpText);
        }
        if (Menu_Manual_helpAssets != NULL) {
            Core_Memory_FreeFromHighEnd(Menu_Manual_helpAssets);
        }
        Render_RetireTexturePageRange(0x1D, 3, 5);
        return 2;
    }
    return 0;
}

// decomp: MENUE.PRG _topMenu (src/MENU/MENUE.PRG/494.c:450)
int Menu_Manual_UpdateTopicMenu(int init)
{
    enum Menu_Manual_TopicMenuState { Menu_Manual_TopicMenuState_InitRows, Menu_Manual_TopicMenuState_SelectRow };

    char* strings[28];
    int i;

    if (init != 0) {
        Menu_Manual_topMenuState = Menu_Manual_TopicMenuState_InitRows;
        Menu_Manual_selectedRow = 0;
    }

    switch (Menu_Manual_topMenuState) {
    case Menu_Manual_TopicMenuState_InitRows:
        for (i = 0; i < 14; ++i) {
            strings[i * 2] = (char*)&Menu_Manual_menuText[Menu_Manual_menuText[i]];
            strings[i * 2 + 1] = (char*)&Menu_Manual_menuText[Menu_Manual_menuText[14 + i]];
        }
        Menu_Manual_SetTopicRows(14, Field_manualDisplayState.currentManual, strings);
        ++Menu_Manual_topMenuState;
        break;
    case Menu_Manual_TopicMenuState_SelectRow:
        Menu_Manual_selectedRow = Menu_Manual_GetSelectedTopicRow();
        if (Menu_Manual_selectedRow != -1) {
            return Menu_Manual_selectedRow;
        }
        break;
    }
    return -1;
}

// decomp: MENUE.PRG _copySprites (src/MENU/MENUE.PRG/494.c:481)
void Menu_Manual_UploadSpriteTiles(u_long const* buffer, int count)
{
    RECT rect;
    int i;

    i = 0;
    rect.w = 4;
    rect.h = 16;

    for (i = 0; i < count; ++i) {
        rect.x = (i % 48) * 4 + 832;
        rect.y = (i / 48) * 16 + 256;
        LoadImage2(&rect, (u_long*)buffer);
        buffer += 4 * 16 / 2;
    }
}

// decomp: MENUE.PRG _copyCluts (src/MENU/MENUE.PRG/494.c:498)
void Menu_Manual_UploadCluts(u_long* buffer, int count)
{
    RECT rect;
    int i;

    rect.w = 16;
    rect.h = 1;

    for (i = 0; i < count; ++i) {
        rect.x = (i % 16) * 16 + 768;
        rect.y = i / 16 + 496;
        LoadImage2(&rect, buffer);
        buffer += 8;
    }
}

// decomp: MENUE.PRG _getRoundedLineCount (src/MENU/MENUE.PRG/494.c:514)
int Menu_Manual_LastPageLineOffset(void)
{
    int remainder;
    int lineCount;

    lineCount = Menu_Manual_helpText[4] & 0xFFFF;
    remainder = (Menu_Manual_helpText[4] & 0xFFFF) % 10;
    if (remainder > 0) {
        lineCount += 10 - remainder;
    }
    lineCount -= 10;
    if (lineCount < 0) {
        lineCount = 0;
    }
    return lineCount;
}

// decomp: MENUE.PRG _copyNextPageText (src/MENU/MENUE.PRG/494.c:531)
void Menu_Manual_CopyPageText(void)
{
    static char buffer[1024];
    int i;
    int nextPage;
    u_short* offsets;
    char* pBuffer;

    buffer[0] = Core_TextCode_ChunkSize;
    buffer[1] = Core_TextCode_NoChunking;
    buffer[2] = Core_TextCode_FontTable;
    buffer[3] = Core_TextCode_FontTableZero;
    buffer[4] = Core_TextCode_Newline;

    offsets = (ushort*)(Menu_Manual_helpText + 4);
    nextPage = Menu_Manual_scrollPosition + 10;
    pBuffer = buffer + 4;

    if ((u_short)Menu_Manual_helpText[4] < nextPage) {
        nextPage = (u_short)Menu_Manual_helpText[4];
    }

    for (i = Menu_Manual_scrollPosition; i < nextPage; ++i) {
        pBuffer = Menu_Manual_CopyEncodedLine(pBuffer, (char*)(offsets + offsets[i + 1]));
    }

    pBuffer[-1] = Core_TextCode_Terminator;
    Menu_SetTextBoxText(0, buffer);
}

// decomp: MENUE.PRG _vsStrcpy (src/MENU/MENUE.PRG/494.c:561)
char* Menu_Manual_CopyEncodedLine(char* destination_text, char* source_text)
{
    char c;

    for (c = *source_text; c != Core_TextCode_Terminator; c = *source_text) {
        if (c >= Core_TextCode_Control) {
            *destination_text = c;
            ++source_text;
            ++destination_text;
        }
        *destination_text++ = *source_text++;
    }
    *destination_text = Core_TextCode_Newline;
    return destination_text + 1;
}

// decomp: MENUE.PRG _drawContent (src/MENU/MENUE.PRG/494.c:577)
void Menu_Manual_DrawPageContent(void)
{
    RECT rect;
    DR_AREA* area;
    int i;
    int spriteOffset;
    u_short* spriteData;
    Render_FrameScratch* q;
    Render_FrameScratch* p = &Render_frameScratch;

    area = p->primitiveCursor;
    SetDrawArea(area, &Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip);
    AddPrim(p->orderingTable, area++);
    p->primitiveCursor = area;

    Menu_Manual_DrawDiagramLines();

    spriteOffset = Menu_Manual_helpText[0];
    if (spriteOffset < 0) {
        spriteOffset += 3;
    }
    spriteData = (u_short*)Menu_Manual_helpText + 8 + (spriteOffset >> 2) * 2;

    for (i = 0; i < spriteData[0]; ++i) {
        Menu_Manual_DrawIllustration(&spriteData[spriteData[i + 1]]);
    }
    if (Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip.x >= 320) {
        rect.x = 333;
    } else {
        rect.x = 13;
    }
    rect.w = 300;
    rect.y = 55;
    rect.h = 130;

    q = &Render_frameScratch;
    area = q->primitiveCursor;
    SetDrawArea(area, &rect);
    AddPrim(q->orderingTable, area++);
    q->primitiveCursor = area;
}

// decomp: MENUE.PRG _min (src/MENU/MENUE.PRG/494.c:619)
int Menu_Manual_MinTileExtent(int remaining_extent, int tile_extent_limit)
{
    if (remaining_extent >= tile_extent_limit) {
        return tile_extent_limit;
    }
    return remaining_extent;
}

// decomp: MENUE.PRG _add_int (src/MENU/MENUE.PRG/494.c:627)
int Menu_Manual_AddSpriteOffset(int sprite_origin, int tile_offset) { return (sprite_origin + tile_offset); }

// decomp: MENUE.PRG _drawSprite (src/MENU/MENUE.PRG/494.c:629)
void Menu_Manual_DrawIllustration(short* data)
{
    short posX;
    short posY;
    short width;
    short height;
    short colors;
    short clutPacked;
    int spriteW;
    int spriteH;
    int tileStride;
    int spriteX;
    int spriteY;
    int j;
    int i;
    SPRT* sprite;
    Render_FrameScratch* scratch;
    short tPage;

    if ((*data != 0) && ((Menu_Manual_AdvanceVisibilityAnimation((Menu_Manual_AnimationState*)data) << 0x10) != 0)) {
        return;
    }

    data += 8;
    posX = *data++;
    posY = *data++;
    width = *data++;
    height = *data++;
    ++data; // num_sprites
    colors = *data++;
    clutPacked = *data++;

    clutPacked =
        ((((clutPacked % 16) << 4) + 0x300) >> 4) | (((clutPacked / 16) + 0x1F0) << 6);

    tileStride = 8;

    if (colors == 16) {
        tileStride = 16;
    }

    for (i = 0; i < height; i += 16) {
        spriteH = Menu_Manual_MinTileExtent(height - i, 16);

        for (j = 0; j < width; ++data, j += tileStride) {
            int spriteU0;
            int spriteV0;

            spriteW = Menu_Manual_MinTileExtent(width - j, tileStride);
            spriteX = 0x0D;
            spriteX = posX + (j + spriteX);
            spriteY = Menu_Manual_AddSpriteOffset(posY, i + 0x37);
            spriteY -= Menu_Manual_scrollPosition * 0xD;

            if ((spriteY < 0x27) || (0xB9 <= spriteY)) {
                continue;
            }

            sprite = Render_frameScratch.primitiveCursor;
            setSprt(sprite);
            setShadeTex(sprite, 1);
            sprite->clut = clutPacked;
            setXY0(sprite, spriteX, spriteY);

            spriteU0 = (*data / 16);
            tPage = spriteU0;
            tPage = tPage % 3 + 0x1D;

            if (colors == 16) {
                sprite->u0 = ((*data - (spriteU0 << 4)) << 16) >> 12;
            } else {
                tPage |= 0x80;
                sprite->u0 = ((*data - (spriteU0 << 4)) << 16) >> 13;
            }

            spriteV0 = (*data / 48);
            sprite->v0 = (spriteV0 << 4);
            setWH(sprite, spriteW, spriteH);

            scratch = &Render_frameScratch;
            AddPrim(scratch->orderingTable, sprite++);
            scratch->primitiveCursor = (void*)sprite;
            _insertTPage(0, tPage);
        }
    }
}

// decomp: MENUE.PRG _setPageBg (src/MENU/MENUE.PRG/494.c:716)
void Menu_Manual_DrawPageBackground(int x, int y, int w, int h, int color)
{
    POLY_G4* poly;
    Render_FrameScratch* p;

    poly = Render_frameScratch.primitiveCursor;
    setPolyG4(poly);
    setXY4(poly, x, y, (x + w) - 1, y, x, (y + h) - 1, (x + w) - 1, (y + h) - 1);
    setRGB0(poly, Menu_Manual_kColorStops0[color].r, Menu_Manual_kColorStops0[color].g, Menu_Manual_kColorStops0[color].b);
    setRGB1(poly, Menu_Manual_kColorStops1[color].r, Menu_Manual_kColorStops1[color].g, Menu_Manual_kColorStops1[color].b);
    setRGB2(poly, Menu_Manual_kColorStops0[color].r, Menu_Manual_kColorStops0[color].g, Menu_Manual_kColorStops0[color].b);
    setRGB3(poly, Menu_Manual_kColorStops1[color].r, Menu_Manual_kColorStops1[color].g, Menu_Manual_kColorStops1[color].b);

    p = &Render_frameScratch;
    AddPrim(((u_long*)p->orderingTable) + 7, poly++);
    p->primitiveCursor = (u_long*)poly;
    _insertTPage(7, getTPage(0, 3, 0, 0));
}

// decomp: MENUE.PRG _fadeMenuUpper (src/MENU/MENUE.PRG/494.c:740)
void Menu_Manual_DrawUpperScrollFade(void)
{
    int widths[4];
    int transparency[16];
    Menu_Manual_BackgroundTransparencies const* transparency_source;
    Menu_Manual_BackgroundTransparencies* transparency_destination;
    int i;
    int j;
    POLY_FT4* poly;
    TILE* tile;

    *(Menu_Manual_BackgroundChunkWidths*)widths = Menu_Manual_kMenuBgChunkWidths;

    transparency_destination = (Menu_Manual_BackgroundTransparencies*)transparency;
    transparency_source = Menu_Manual_kMenuBgTransparencies;

    do {
        *transparency_destination++ = *transparency_source++;
    } while (transparency_source != (Menu_Manual_kMenuBgTransparencies + 4));

    Menu_Manual_DrawPageArrow(Menu_Manual_ArrowType_Up);
    poly = Render_frameScratch.primitiveCursor;

    for (i = 0; i < 16; ++i) {
        for (j = 0; j < 3; ++j) {
            setPolyFT4(poly);
            setXY4(poly, j << 7, i + 52, widths[j] + (j << 7), i + 52, j << 7, i + 53,
                widths[j] + (j << 7), i + 53);
            setUV4(poly, 0, i + 52, widths[j], i + 52, 0, i + 53, widths[j], i + 53);
            setClut(poly, 768, 227);
            poly->tpage = (j + 32) | 128 | (640 >> 6) | (256 >> 4);
            setSemiTrans(poly, 1);
            setRGB0(poly, transparency[i], transparency[i], transparency[i]);
            AddPrim(((u_long*)Render_frameScratch.orderingTable) - 7, poly++);
        }
    }

    tile = (TILE*)poly;
    for (i = 0; i < 16; ++i) {
        setTile(&tile[i]);
        setSemiTrans(&tile[i], 1);
        setXY0(&tile[i], 0, i + 52);
        setWH(&tile[i], 320, 1);
        setRGB0(&tile[i], transparency[i], transparency[i], transparency[i]);
        AddPrim(((u_long*)Render_frameScratch.orderingTable) - 7, &tile[i]);
    }

    Render_frameScratch.primitiveCursor = (void*)poly + 16 * sizeof(TILE);
    _insertTPage(-7, getTPage(0, 2, 0, 0));
}

// decomp: MENUE.PRG _fadeMenuLower (src/MENU/MENUE.PRG/494.c:791)
void Menu_Manual_DrawLowerScrollFade(void)
{
    int widths[4];
    int transparency[16];
    Menu_Manual_BackgroundTransparencies const* transparency_source;
    Menu_Manual_BackgroundTransparencies* transparency_destination;
    int i;
    int j;
    POLY_FT4* poly;
    TILE* tile;

    *(Menu_Manual_BackgroundChunkWidths*)widths = Menu_Manual_kMenuBgChunkWidths;

    transparency_destination = (Menu_Manual_BackgroundTransparencies*)transparency;
    transparency_source = Menu_Manual_kMenuBgTransparencies;

    do {
        *transparency_destination++ = *transparency_source++;
    } while (transparency_source != (Menu_Manual_kMenuBgTransparencies + 4));

    Menu_Manual_DrawPageArrow(Menu_Manual_ArrowType_Down);

    poly = Render_frameScratch.primitiveCursor;

    for (i = 0; i < 16; ++i) {
        for (j = 0; j < 3; ++j) {
            setPolyFT4(poly);
            setXY4(poly, j << 7, 187 - i, widths[j] + (j << 7), 187 - i, j << 7, 188 - i,
                widths[j] + (j << 7), 188 - i);
            setUV4(poly, 0, -69 - i, widths[j], -69 - i, 0, -68 - i, widths[j], -68 - i);
            setClut(poly, 768, 227);
            poly->tpage = (j + 32) | 128 | (640 >> 6) | (256 >> 4);
            setSemiTrans(poly, 1);
            setRGB0(poly, transparency[i], transparency[i], transparency[i]);
            AddPrim(((u_long*)Render_frameScratch.orderingTable) - 7, poly++);
        }
    }

    tile = (TILE*)poly;

    for (i = 0; i < 16; ++i) {
        setTile(&tile[i]);
        setSemiTrans(&tile[i], 1);
        setXY0(&tile[i], 0, 0xBB - i);
        setWH(&tile[i], 320, 1);
        setRGB0(&tile[i], transparency[i], transparency[i], transparency[i]);
        AddPrim(((u_long*)Render_frameScratch.orderingTable) - 7, &tile[i]);
    }

    Render_frameScratch.primitiveCursor = (void*)poly + 16 * sizeof(TILE);
    _insertTPage(-7, getTPage(0, 2, 0, 0));
}

// decomp: MENUE.PRG _drawPaginationArrow (src/MENU/MENUE.PRG/494.c:844)
void Menu_Manual_DrawPageArrow(enum Menu_Manual_ArrowType arrowType)
{
    int y;
    POLY_FT4* poly;
    Render_FrameScratch* p;

    if (arrowType != Menu_Manual_ArrowType_Up) {
        if (Menu_Manual_pageArrowAnimState < 9) {
            y = (Menu_Manual_pageArrowAnimState >> 1) + 184;
        } else {
            y = -(Menu_Manual_pageArrowAnimState >> 2) + 190;
        }
    } else {
        if (Menu_Manual_pageArrowAnimState < 9) {
            int upper_arrow_base_y = 40;
            y = upper_arrow_base_y - (Menu_Manual_pageArrowAnimState >> 1);
        } else {
            y = (Menu_Manual_pageArrowAnimState >> 2) + 34;
        }
    }

    poly = (POLY_FT4*)Render_frameScratch.primitiveCursor;
    setPolyFT4(poly);
    setShadeTex(poly, 1);
    setXY4(poly, 152, y, 168, y, 152, y + 16, 168, y + 16);
    setUV4(poly, arrowType * 16, 48, arrowType * 16 + 16, 48, arrowType * 16, 64,
        arrowType * 16 + 16, 64);
    setTPage(poly, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, 768, 0);

    if (Input_previousButtons & PADRleft) {
        setClut(poly, 896, 223);
    } else {
        setClut(poly, 912, 223);
    }
    p = &Render_frameScratch;
    AddPrim(((u_long*)p->orderingTable) - 7, poly++);
    p->primitiveCursor = (void*)poly;
}

// decomp: MENUE.PRG _drawContentLines (src/MENU/MENUE.PRG/494.c:883)
void Menu_Manual_DrawDiagramLines(void)
{
    short animationState;
    short i;
    short* linesData;
    int linesOffset;
    int spriteOffset;
    LINE_F2* line;
    short linesCount;
    char* c;

    if (Menu_Manual_helpText[2] <= 0) {
        return;
    }

    line = Render_frameScratch.primitiveCursor;

    spriteOffset = Menu_Manual_helpText[0];
    if (spriteOffset < 0) {
        spriteOffset += 3;
    }
    spriteOffset >>= 2;

    linesOffset = Menu_Manual_helpText[1];
    if (linesOffset < 0) {
        linesOffset += 3;
    }

    linesOffset = spriteOffset + (linesOffset >> 2);
    linesData = (short*)&(Menu_Manual_helpText)[linesOffset + 4];

    linesCount = *linesData++;

    for (i = 0; i < linesCount; ++i) {
        animationState = 0;
        if (*linesData != 0) {
            animationState = Menu_Manual_AdvanceVisibilityAnimation((Menu_Manual_AnimationState*)linesData);
        }
        linesData += 8;
        setLineF2(line);
        line->x0 = *linesData++ + 13;
        line->y0 = *linesData++ + 55 - Menu_Manual_scrollPosition * 13;
        line->x1 = *linesData++ + 13;
        line->y1 = *linesData++ + 55 - Menu_Manual_scrollPosition * 13;
        c = (char*)linesData;
        setRGB0(line, c[0], c[1], c[2]);
        linesData += 2;
        if ((line->y0 < 55) && ((line->y1 >= 55) == 0)) {
            continue;
        }
        if (((line->y0 < 186) || (line->y1 < 186)) && ((animationState << 16) == 0)) {
            AddPrim(Render_frameScratch.orderingTable, line++);
        }
    }
    Render_frameScratch.primitiveCursor = line;
}

// decomp: MENUE.PRG _getAnimationState (src/MENU/MENUE.PRG/494.c:940)
short Menu_Manual_AdvanceVisibilityAnimation(Menu_Manual_AnimationState* state)
{
    short nextState;

    if (state->currentFrame >= 16) {
        return state->isActive;
    }

    nextState = state->frameMask >> state->currentFrame;
    nextState = nextState & 1;

    if ((++state->timer) >= state->frameDuration) {
        state->timer = 0;
        if ((++state->currentFrame) >= 16) {
            if (state->repeat != 0) {
                state->currentFrame = 0;
            }
        }
    }
    return state->isActive = nextState ^ 1;
}

// decomp: MENUE.PRG _drawControlsBg (src/MENU/MENUE.PRG/494.c:962)
void Menu_Manual_DrawControlsBackground(int x, int y, int w, int h)
{
    int i;
    int j;
    LINE_G2* line;
    POLY_F4* poly;
    Render_FrameScratch* p;

    line = (LINE_G2*)Render_frameScratch.primitiveCursor;
    for (i = y, j = w; i < (y + h) - 1; ++i, --j) {
        setLineG2(line);
        setXY2(line, x, i, (x + j) - 1, i);
        setRGB0(line, 64, 56, 32);
        setRGB1(line, 16, 16, 8);
        AddPrim(((u_long*)Render_frameScratch.orderingTable) + 7, line++);
    }
    x += 2;
    y += 2;
    poly = (POLY_F4*)line;
    setPolyF4(poly);
    setXY4(poly, x, y, (x + w) - 1, y, x, (y + h) - 1, (x + w) - h, (y + h) - 1);
    setRGB0(poly, 0, 0, 0);

    p = &Render_frameScratch;
    AddPrim(((u_long*)p->orderingTable) + 7, poly);
    p->primitiveCursor = (u_long*)(poly + 1);
}

// MENUE.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Manual_ResetMenueStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Manual_openDelay, &initial, sizeof Menu_Manual_openDelay);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Manual_screenIsInit, &initial, sizeof Menu_Manual_screenIsInit);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Manual_screenMenuResult, &initial, sizeof Menu_Manual_screenMenuResult);
    }
}

