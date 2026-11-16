// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/SfxLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/ManualIllustrationLive.hpp"
#include "Menu/ManualPageLive.hpp"
#include "Menu/ManualTopicsLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/TexturePagesLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Hud.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Field/Locations.h"
#include "Field/Manual.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Manual.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

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
u_short Menu_Manual_menuText[516]; /* contents: disc string table (ModuleData.cpp) */

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

// decomp: MENUE.PRG func_80102E9C (src/MENU/MENUE.PRG/494.c:132)
void Menu_Manual_ClearMenuExceptRow8(void) { VS::Menu::Framework::ClearLiveMenuExcept(8); }

// decomp: MENUE.PRG _menuReady (src/MENU/MENUE.PRG/494.c:134)
void Menu_Manual_PollMenuReady(void) { VS::Menu::Framework::AreLiveMenuRowsIdle(); }

// decomp: MENUE.PRG vs_menuE_exec (src/MENU/MENUE.PRG/494.c:142)
int Menu_Manual_UpdateScreen(u_char* state)
{
    enum Menu_Manual_ManualOverlayState { Menu_Manual_ManualOverlayState_None, Menu_Manual_ManualOverlayState_Init = 3, Menu_Manual_ManualOverlayState_InitMenu, Menu_Manual_ManualOverlayState_WaitReady, Menu_Manual_ManualOverlayState_ShowMenu, Menu_Manual_ManualOverlayState_ShutDown, Menu_Manual_ManualOverlayState_Exit };

    static short Menu_Manual_screenLayoutPadding __attribute__((unused)) = 0x54;

    switch (*state) {
    case Menu_Manual_ManualOverlayState_Init:
        VS::Menu::Framework::SetLiveBackgroundFade(1);
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
            VS::Field::LiveSetFieldRenderingEnabled(0);
            VS::Battle::SetLiveHudHidden(1);
            Battle_hudSlideOffset = 0x200000;
            *state = Menu_Manual_ManualOverlayState_WaitReady;
        }
        break;
    case Menu_Manual_ManualOverlayState_WaitReady:
        *state += VS::Menu::Framework::AreLiveMenuRowsIdle();
        break;
    case Menu_Manual_ManualOverlayState_ShowMenu:
        Menu_Manual_screenMenuResult = Menu_Manual_UpdateHelpPages();
        if (Menu_Manual_screenMenuResult == 0) {
            break;
        }
        VS::Menu::Framework::SetLiveBackgroundPriority(-4, 128);
        VS::Battle::SetLiveHudHidden(0);
        Battle_hudSlideOffset = 0;
        VS::Field::LiveSetFieldRenderingEnabled(1);
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
        VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
        *state = Menu_Manual_ManualOverlayState_ShutDown;
        break;
    case Menu_Manual_ManualOverlayState_ShutDown:
        VS::Menu::Framework::SetLiveBackgroundFade(0);
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
        VS::Core::Memory::ZeroLiveAlignedWords(&Field_manualDisplayState, sizeof Field_manualDisplayState);
    }
    VS::Render::ReserveLiveTexturePageRange(0x1D, 3, 5);
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
        VS::Menu::Framework::SetLiveBackgroundPriority(2046, 128);
        ++Menu_Manual_showMenuState;
        break;
    case Menu_Manual_ManualMenuState_InitMenu:
        VS::Menu::Framework::InitializeLiveInformationBox();
        VS::Menu::manual::LiveUpdateTopicMenu(1);
        ++Menu_Manual_showMenuState;
        break;
    case Menu_Manual_ManualMenuState_GetSelection:
        selectedRow = VS::Menu::manual::LiveUpdateTopicMenu(0);
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
                VS::Core::Memory::FreeLiveFromHighEnd(Menu_Manual_helpText);
            }
            if (Menu_Manual_helpAssets != NULL) {
                VS::Core::Memory::FreeLiveFromHighEnd(Menu_Manual_helpAssets);
            }

            Menu_Manual_helpText = (int *)(VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Manual_kHelpFileCdFiles[selectedRow * 2 + 1].size));
            Menu_Manual_helpAssets = (u_long *)(VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Manual_kHelpFileCdFiles[selectedRow * 2].size));
            Menu_Manual_helpAssetsCdQueue =
                VS::Content::AllocateLiveDiscRequest(&Menu_Manual_kHelpFileCdFiles[selectedRow * 2]);
            if (Menu_Manual_helpAssetsCdQueue != NULL) {
                VS::Content::EnqueueLiveDiscRequest(Menu_Manual_helpAssetsCdQueue, Menu_Manual_helpAssets);
            } else {
                Menu_Manual_helpAssetsCdQueue = VS::Content::CompleteLiveRefusedDiscRequest(&Menu_Manual_kHelpFileCdFiles[selectedRow * 2], Menu_Manual_helpAssets);
            }
            Menu_Manual_helpTextCdQueue =
                VS::Content::AllocateLiveDiscRequest(&Menu_Manual_kHelpFileCdFiles[selectedRow * 2 + 1]);
            if (Menu_Manual_helpTextCdQueue != NULL) {
                VS::Content::EnqueueLiveDiscRequest(Menu_Manual_helpTextCdQueue, Menu_Manual_helpText);
            } else {
                Menu_Manual_helpTextCdQueue = VS::Content::CompleteLiveRefusedDiscRequest(&Menu_Manual_kHelpFileCdFiles[selectedRow * 2 + 1], Menu_Manual_helpText);
            }
        }
        Field_manualDisplayState.currentManual =
            Menu_Framework_menuMemory.cursorMemories[4] + Menu_Framework_menuMemory.cursorMemories[5];
        VS::Menu::Framework::DismissLiveInformationBox();
        break;
    case Menu_Manual_ManualMenuState_LoadSubMenu:
        cdState = Menu_Manual_helpAssetsCdQueue->state;
        if (cdState != Content_DiscRequestState_Loaded) {
            break;
        }
        VS::Content::ReleaseLiveDiscRequest(Menu_Manual_helpAssetsCdQueue);
        VS::Menu::manual::LiveUploadSpriteTiles(Menu_Manual_helpAssets + 2, Menu_Manual_helpAssets[0]);
        VS::Menu::manual::LiveUploadCluts(Menu_Manual_helpAssets + ((Menu_Manual_helpAssets[0] << 5) + 2), Menu_Manual_helpAssets[1]);
        if (Menu_Manual_helpAssets != NULL) {
            VS::Core::Memory::FreeLiveFromHighEnd(Menu_Manual_helpAssets);
        }
        Menu_Manual_helpAssets = NULL;
        Menu_Manual_showMenuState = Menu_Manual_ManualMenuState_InitSubMenu;
        break;
    case Menu_Manual_ManualMenuState_InitSubMenu:
        if (Menu_Manual_helpTextCdQueue->state != Content_DiscRequestState_Loaded) {
            break;
        }
        VS::Content::ReleaseLiveDiscRequest(Menu_Manual_helpTextCdQueue);
        VS::Menu::InitLiveTextBox(0, 0, 8, 0x34, 0x19, 0xA, 8, 0x34);
        Menu_Manual_scrollPosition =
            Field_manualDisplayState
                .scrollPositions[Field_manualDisplayState.currentManual];
        Menu_Manual_lineCount = VS::Menu::manual::LiveLastPageLineOffset();
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
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonsPressed.allButtons & PADLdown)
                       && (currentPage == lastPage) && (lastPage > 0)) {
                currentPage = 0;
                Menu_Manual_scrollPosition = 0;
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonRepeat & PADLup) && (currentPage > 0)) {
                --currentPage;
                Menu_Manual_scrollPosition = currentPage * 10;
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonRepeat & PADLdown) && (currentPage < lastPage)) {
                ++currentPage;
                Menu_Manual_scrollPosition = currentPage * 10;
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            }
        } else {
            if ((Input_buttonsPressed.allButtons & PADLup) && (Menu_Manual_scrollPosition == 0)
                && (Menu_Manual_lineCount > 0)) {
                Menu_Manual_scrollPosition = Menu_Manual_lineCount;
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if (Input_buttonsPressed.allButtons & PADLdown
                       && (Menu_Manual_scrollPosition == Menu_Manual_lineCount) && (Menu_Manual_scrollPosition > 0)) {
                Menu_Manual_scrollPosition = 0;
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonRepeat & PADLup) && (Menu_Manual_scrollPosition > 0)) {
                --Menu_Manual_scrollPosition;
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            } else if ((Input_buttonRepeat & PADLdown)
                       && (Menu_Manual_scrollPosition < Menu_Manual_lineCount)) {
                ++Menu_Manual_scrollPosition;
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            }
        }
        frame_scratch = &Render_frameScratch;
        VS::Menu::Framework::DrawLiveButtonIcon(Menu_Framework_ButtonId_Square, 8, 16, ((u_long*)frame_scratch->orderingTable) + 6);
        VS::Render::DrawLiveCompactTextColor("TURN    PAGE", 0x12001C, 0x808080, ((u_long*)frame_scratch->orderingTable) + 6);
        VS::Menu::manual::LiveDrawControlsBackground(0x10, 0x12, 0x60, 0xC);
        VS::Menu::Framework::DrawLiveButtonIcon(Menu_Framework_ButtonId_Circle, 8, 34, ((u_long*)frame_scratch->orderingTable) + 6);
        VS::Render::DrawLiveCompactTextColor("MENU", 0x24001C, 0x808080, ((u_long*)frame_scratch->orderingTable) + 6);
        VS::Menu::manual::LiveDrawControlsBackground(0x10, 0x24, 0x40, 0xC);
        VS::Render::DrawLiveCompactTextColor("PAGE", 0xC400D8, 0x808080, ((u_long*)frame_scratch->orderingTable) + 7);
        sprintf(charBuf, "#%d/", currentPage + 1);
        VS::Render::DrawLiveCompactTextColor(charBuf, 0xC40118, 0x808080, ((u_long*)frame_scratch->orderingTable) + 7);
        sprintf(charBuf, "#%d", lastPage + 1);
        VS::Render::DrawLiveCompactTextColor(charBuf, 0xC40130, 0x808080, ((u_long*)frame_scratch->orderingTable) + 7);
        Field_manualDisplayState
            .scrollPositions[Field_manualDisplayState.currentManual] =
            Menu_Manual_scrollPosition;
        if (Menu_Manual_showMenuState != Menu_Manual_ManualMenuState_ShowSubMenu) {
            if (Input_buttonsPressed.allButtons & PADRright) {
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
            } else {
                VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            }
            VS::Menu::LiveTextBox(0)->state = 0;
            if (Menu_Manual_helpText != NULL) {
                VS::Core::Memory::FreeLiveFromHighEnd(Menu_Manual_helpText);
            }
            Menu_Manual_helpText = NULL;
            break;
        }
        VS::Menu::manual::LiveDrawPageBackground(8, 52, 310, 136, 0);
        Menu_Manual_pageArrowAnimState = (Menu_Manual_pageArrowAnimState + 1) & 0xF;
        if (Menu_Manual_scrollPosition > 0) {
            VS::Menu::manual::LiveDrawUpperScrollFade();
        }
        if (Menu_Manual_scrollPosition < Menu_Manual_lineCount) {
            VS::Menu::manual::LiveDrawLowerScrollFade();
        }
        if (Menu_Manual_helpPageLoadCounter < 8) {
            ++Menu_Manual_helpPageLoadCounter;
        } else {
            VS::Menu::manual::LiveDrawPageContent();
        }
        VS::Menu::manual::LiveCopyPageText();
        break;
    case Menu_Manual_ManualMenuState_ExitToMenu:
        if (Menu_Manual_helpText != NULL) {
            VS::Core::Memory::FreeLiveFromHighEnd(Menu_Manual_helpText);
        }
        if (Menu_Manual_helpAssets != NULL) {
            VS::Core::Memory::FreeLiveFromHighEnd(Menu_Manual_helpAssets);
        }
        VS::Render::RetireLiveTexturePageRange(0x1D, 3, 5);
        return 1;
    case Menu_Manual_ManualMenuState_ExitToBattle:
        if (Menu_Manual_helpText != NULL) {
            VS::Core::Memory::FreeLiveFromHighEnd(Menu_Manual_helpText);
        }
        if (Menu_Manual_helpAssets != NULL) {
            VS::Core::Memory::FreeLiveFromHighEnd(Menu_Manual_helpAssets);
        }
        VS::Render::RetireLiveTexturePageRange(0x1D, 3, 5);
        return 2;
    }
    return 0;
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

}  // extern "C"
