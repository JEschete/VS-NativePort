// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Battle/Feedback.h"
#include "Boot/GameLoop.h"
#include "Content/StringTables.h"
#include "Core/Memory/Heap.h"
#include "Event/ScriptAudio.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Files.h"
#include "Movie/Intro/Playback.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/TextureAnimation.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/RoomCache.h"
#include "State/RoomMemory.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
#include "State/Timers.h"
#include "Title/Credits.h"
#include "Title/NewGame.h"
#include "Title/SaveFiles.h"
#include "Title/TitleMenu.h"
#include "Title/TitleScreen.h"

// decomp: vs_main_titleScreenCount (vs_main_titleScreenCount@SLUS_010.40)
int Title_titleScreenVisitCount = 0;

// decomp: _menuItemOutlineClut (_menuItemOutlineClut@TITLE/TITLE.PRG)
u_short Title_kMenuItemOutlinePalettes[][16] = {
    { vs_getRGB5551(0, 0, 0, 0), vs_getRGB5551(0, 0, 1, 1), vs_getRGB5551(0, 0, 4, 1),
        vs_getRGB5551(0, 0, 5, 1), vs_getRGB5551(0, 0, 8, 1), vs_getRGB5551(0, 0, 10, 1),
        vs_getRGB5551(0, 0, 12, 1), vs_getRGB5551(0, 0, 14, 1),
        vs_getRGB5551(0, 0, 15, 1), vs_getRGB5551(0, 0, 18, 1),
        vs_getRGB5551(0, 0, 20, 1), vs_getRGB5551(0, 0, 22, 1),
        vs_getRGB5551(0, 0, 24, 1), vs_getRGB5551(0, 0, 26, 1),
        vs_getRGB5551(1, 1, 29, 1), vs_getRGB5551(2, 3, 31, 1) },

    { vs_getRGB5551(0, 0, 0, 0), vs_getRGB5551(0, 0, 3, 1), vs_getRGB5551(0, 0, 3, 1),
        vs_getRGB5551(4, 2, 0, 1), vs_getRGB5551(3, 0, 7, 1), vs_getRGB5551(7, 3, 3, 1),
        vs_getRGB5551(12, 7, 4, 1), vs_getRGB5551(19, 12, 1, 1),
        vs_getRGB5551(24, 17, 1, 1), vs_getRGB5551(28, 20, 1, 1),
        vs_getRGB5551(30, 20, 1, 1), vs_getRGB5551(31, 22, 0, 1),
        vs_getRGB5551(31, 22, 2, 1), vs_getRGB5551(26, 20, 10, 1),
        vs_getRGB5551(31, 25, 4, 1), vs_getRGB5551(28, 23, 10, 1) }
};

// decomp: _menuItemStates (_menuItemStates@src/TITLE/TITLE.PRG/22C.c)
Title_MenuItemAnimationState Title_menuItemStates[10];

// decomp: _menuItemTextClut (_menuItemTextClut@TITLE/TITLE.PRG)
u_short Title_kMenuItemTextPalettes[][16] = {
    { vs_getRGB5551(0, 0, 0, 0), vs_getRGB5551(0, 0, 2, 1), vs_getRGB5551(2, 0, 3, 1),
        vs_getRGB5551(3, 0, 4, 1), vs_getRGB5551(6, 0, 0, 1), vs_getRGB5551(9, 1, 0, 1),
        vs_getRGB5551(11, 2, 0, 1), vs_getRGB5551(13, 3, 0, 1),
        vs_getRGB5551(14, 2, 0, 1), vs_getRGB5551(15, 3, 0, 1),
        vs_getRGB5551(16, 4, 0, 1), vs_getRGB5551(16, 3, 0, 1),
        vs_getRGB5551(16, 4, 0, 1), vs_getRGB5551(17, 5, 0, 1),
        vs_getRGB5551(18, 7, 0, 1), vs_getRGB5551(19, 8, 0, 1) },
    { vs_getRGB5551(0, 0, 0, 0), vs_getRGB5551(0, 0, 2, 1), vs_getRGB5551(5, 3, 5, 1),
        vs_getRGB5551(7, 5, 7, 1), vs_getRGB5551(11, 8, 7, 1),
        vs_getRGB5551(16, 12, 9, 1), vs_getRGB5551(20, 15, 10, 1),
        vs_getRGB5551(24, 18, 11, 1), vs_getRGB5551(28, 19, 11, 1),
        vs_getRGB5551(31, 21, 10, 1), vs_getRGB5551(27, 22, 15, 1),
        vs_getRGB5551(31, 22, 12, 1), vs_getRGB5551(31, 23, 13, 1),
        vs_getRGB5551(31, 25, 16, 1), vs_getRGB5551(31, 27, 18, 1),
        vs_getRGB5551(31, 28, 19, 1) }
};

// decomp: outlineWhs (outlineWhs@TITLE/TITLE.PRG)
int Title_kMenuOutlineSizes[] = { vs_getWH(144, 64), vs_getWH(160, 64), vs_getWH(160, 64),
    vs_getWH(128, 64), vs_getWH(112, 64), vs_getWH(96, 64), vs_getWH(96, 64),
    vs_getWH(128, 64), vs_getWH(128, 64), vs_getWH(128, 64) };

// decomp: outlineXys (outlineXys@TITLE/TITLE.PRG)
int Title_kMenuOutlinePositions[] = { vs_getXY(120, 224), vs_getXY(112, 224), vs_getXY(112, 224),
    vs_getXY(128, 224), vs_getXY(136, 224), vs_getXY(144, 224), vs_getXY(144, 224),
    vs_getXY(128, 224), vs_getXY(128, 224), vs_getXY(128, 224) };

// decomp: TITLE.PRG _playNewGameSfx (src/TITLE/TITLE.PRG/22C.c:18)
void Title_PlayNewGameSounds(void)
{
    Audio_PlaySfxCentered(0x7E, 1);
    Audio_PlaySfxCentered(0x7E, 2);
    Audio_PlaySfxCentered(0x7E, 3);
}

// decomp: TITLE.PRG _playMenuChangeSfx (src/TITLE/TITLE.PRG/22C.c:25)
void Title_PlayMenuChangeSound(void) { Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE); }

// decomp: TITLE.PRG _playMenuSelectSfx (src/TITLE/TITLE.PRG/22C.c:27)
void Title_PlayMenuSelectSound(void) { Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT); }

// decomp: TITLE.PRG _playMenuLeaveSfx (src/TITLE/TITLE.PRG/22C.c:29)
void Title_PlayMenuLeaveSound(void) { Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE); }

// decomp: TITLE.PRG _gameLoadScreen (src/TITLE/TITLE.PRG/22C.c:3129)
int Title_ShowContinueScreen(void)
{
    enum Title_LoadScreenPhase {
        Title_LoadScreenPhase_Initialize = 0,
        Title_LoadScreenPhase_LoadCardResources = 1,
        Title_LoadScreenPhase_RunFileMenu = 2,
    };

    static RECT Title_loadScreenUploadRect;
    static char Title_loadScreenPhase;

    int exit;

    Render_UploadPackedRectangle(vs_getXY(768, 0), &Title_saveMenuUiAtlas.pageZeroPixels, vs_getWH(64, 224));
    Render_UploadPackedRectangle(vs_getXY(448, 256), &Title_saveMenuUiAtlas.pageOnePixels, vs_getWH(64, 256));
    Render_UploadPackedRectangle(vs_getXY(832, 0), &Title_saveMenuFontAtlas[0], vs_getWH(64, 224));
    Render_UploadPackedRectangle(vs_getXY(896, 0), &Title_saveMenuFontAtlas[1], vs_getWH(64, 224));
    Render_UploadPackedRectangle(vs_getXY(960, 66), Title_debugFontAtlas, vs_getWH(64, 158));
    Render_UploadPackedRectangle(vs_getXY(768, 227), &Title_saveMenuBackground.palette, vs_getWH(256, 1));
    setRECT(&Title_loadScreenUploadRect, 640, 256, 32, 240);
    ClearImage(&Title_loadScreenUploadRect, 0, 0, 0);
    setRECT(&Title_loadScreenUploadRect, 768, 256, 32, 240);
    ClearImage(&Title_loadScreenUploadRect, 0, 0, 0);
    Render_UploadPackedRectangle(vs_getXY(672, 256), &Title_saveMenuBackground.indexedPixels, vs_getWH(96, 240));
    Menu_Files_InitializeDisplay();
    Title_loadScreenPhase = Title_LoadScreenPhase_Initialize;

    while (1) {
        State_UpdatePlayTimeAndLoading(2);
        Menu_Files_SwapBuffers();
        Input_UpdateControllers();
        switch (Title_loadScreenPhase) {
        case Title_LoadScreenPhase_Initialize:
            if (Title_loadScreenPhase == 0) {
                Save_MemoryCard_InitializeResourcesStep(1);
                Title_loadScreenPhase = Title_LoadScreenPhase_LoadCardResources;
            }
            break;
        case Title_LoadScreenPhase_LoadCardResources:
            if (Save_MemoryCard_InitializeResourcesStep(0) != 0) {
                Title_SaveFiles_ResetElements();
                Menu_Files_UpdateLoadMenu(2);
                Title_loadScreenPhase = Title_LoadScreenPhase_RunFileMenu;
            }
            break;
        case Title_LoadScreenPhase_RunFileMenu:
            exit = Menu_Files_UpdateLoadMenu(0);
            if (exit != 0) {
                Save_MemoryCard_ReleaseResources();
                return exit;
            }
            Menu_Files_DrawBackground();
            Title_SaveFiles_DrawFrame(Menu_Files_frameBuf);
            break;
        }
    }
}

// decomp: TITLE.PRG _gameSaveScreen (src/TITLE/TITLE.PRG/22C.c:3313)
void Title_ShowClearSaveScreen(void)
{
    enum Title_ClearSaveScreenPhase { Title_ClearSaveScreenPhase_Initialize = 0, Title_ClearSaveScreenPhase_Loading = 1, Title_ClearSaveScreenPhase_Ready = 2, Title_ClearSaveScreenPhase_Confirmation = 3, Title_ClearSaveScreenPhase_Exit = 4 };

    static RECT Title_clearSaveUploadRect;
    static char Title_clearSaveScreenPhase;
    static char Title_clearSaveFadeTimerFrames;
    static char Title_clearSaveScreenPadding[6] __attribute__((unused));

    int val;

    Render_UploadPackedRectangle(vs_getXY(768, 0), &Title_saveMenuUiAtlas.pageZeroPixels, vs_getWH(64, 224));
    Render_UploadPackedRectangle(vs_getXY(448, 256), &Title_saveMenuUiAtlas.pageOnePixels, vs_getWH(64, 256));
    Render_UploadPackedRectangle(vs_getXY(832, 0), &Title_saveMenuFontAtlas[0], vs_getWH(64, 224));
    Render_UploadPackedRectangle(vs_getXY(896, 0), &Title_saveMenuFontAtlas[1], vs_getWH(64, 224));
    Render_UploadPackedRectangle(vs_getXY(960, 66), Title_debugFontAtlas, vs_getWH(64, 158));
    Render_UploadPackedRectangle(vs_getXY(768, 227), &Title_saveMenuBackground.palette, vs_getWH(256, 1));
    setRECT(&Title_clearSaveUploadRect, 640, 256, 32, 240);
    ClearImage(&Title_clearSaveUploadRect, 0, 0, 0);
    setRECT(&Title_clearSaveUploadRect, 768, 256, 32, 240);
    ClearImage(&Title_clearSaveUploadRect, 0, 0, 0);
    Render_UploadPackedRectangle(vs_getXY(672, 256), &Title_saveMenuBackground.indexedPixels, vs_getWH(96, 240));
    Menu_Files_InitializeDisplay();

    Title_clearSaveScreenPhase = Title_ClearSaveScreenPhase_Initialize;

    while (1) {
        State_UpdatePlayTimeAndLoading(2);
        Menu_Files_SwapBuffers();
        Input_UpdateControllers();
        Save_MemoryCard_NextCipherBits(0);

        switch (Title_clearSaveScreenPhase) {
        case Title_ClearSaveScreenPhase_Initialize:
            Save_MemoryCard_InitializeResourcesStep(1);
            Title_clearSaveScreenPhase = Title_ClearSaveScreenPhase_Loading;
            continue;
        case Title_ClearSaveScreenPhase_Loading:
            if (Save_MemoryCard_InitializeResourcesStep(0) != 0) {
                Title_SaveFiles_ResetElements();
                Menu_Files_UpdateSaveMenu(1);
                Title_clearSaveScreenPhase = Title_ClearSaveScreenPhase_Ready;
            }
            continue;
        case Title_ClearSaveScreenPhase_Ready:
            val = Menu_Files_UpdateSaveMenu(0);
            if (val != 0) {
                if (val < 0) {
                    Title_memoryCardMessage =
                        (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_savePrompt);
                    Menu_Files_UpdateClearSaveReminder(1);
                    Title_clearSaveScreenPhase = Title_ClearSaveScreenPhase_Confirmation;
                } else {
                    Save_MemoryCard_ReleaseResources();
                    Menu_Files_DrawExitFade();
                    Title_clearSaveFadeTimerFrames = 30;
                    Title_clearSaveScreenPhase = Title_ClearSaveScreenPhase_Exit;
                    continue;
                }
            }
            Menu_Files_DrawBackground();
            Title_SaveFiles_DrawFrame(Menu_Files_frameBuf);
            continue;
        case Title_ClearSaveScreenPhase_Confirmation:
            val = Menu_Files_UpdateClearSaveReminder(0);
            if (val != 0) {
                if (val != 1) {
                    Save_MemoryCard_ReleaseResources();
                    Menu_Files_DrawExitFade();
                    Title_clearSaveFadeTimerFrames = 30;
                    Title_clearSaveScreenPhase = Title_ClearSaveScreenPhase_Exit;
                    continue;
                }
                Menu_Files_UpdateSaveMenu(1);
                Title_clearSaveScreenPhase = Title_ClearSaveScreenPhase_Ready;
            }
            Menu_Files_DrawBackground();
            Title_SaveFiles_DrawFrame(Menu_Files_frameBuf);
            continue;
        case Title_ClearSaveScreenPhase_Exit:
            Menu_Files_DrawExitFade();
            if (--Title_clearSaveFadeTimerFrames == 0) {
                return;
            }
            break;
        }
    }
}

// decomp: TITLE.PRG _displayPublisherAndDeveloper (src/TITLE/TITLE.PRG/22C.c:3579)
void Title_ShowPublisherAndDeveloper()
{
    extern char Title_publisherLogo[];
    extern char Title_developerLogo[];

    DISPENV disp;
    DRAWENV draw;
    RECT rect;
    int var_a3;
    int var_a3_2;
    int i;

    setRECT(&rect, 0, 0, 320, 512);
    ClearImage(&rect, 0, 0, 0);
    Render_UploadPackedRectangle(vs_getXY(320, 64), Title_publisherLogo, vs_getWH(16, 1));
    Render_UploadPackedRectangle(vs_getXY(320, 0), Title_publisherLogo + 32, vs_getWH(256 / 4, 48));
    SetDefDispEnv(&disp, 0, 256, 320, 240);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    disp.screen.y = 8;
    disp.screen.h = 224;
    PutDispEnv(&disp);
    PutDrawEnv(&draw);
    VSync(0);

    for (i = 0; i < 364; ++i) {
        var_a3 = 0;
        if (i < 32) {
            var_a3 = (31 - i) * 4;
        }
        if (i > 331) {
            var_a3 = (i - 331) * 4;
        }
        Title_SaveFiles_DrawImmediateSprite(vs_getXY(32, 88), vs_getUV0Clut(0, 0, 320, 64), vs_getWH(256, 48),
            (var_a3 << 16) | (320 / 64));
        SetDefDispEnv(&disp, 0, (i & 1) * 256, 320, 240);
        SetDefDrawEnv(&draw, 0, (1 - (i & 1)) * 256, 320, 240);
        disp.screen.y = 8;
        disp.screen.h = 224;
        VSync(0);
        PutDispEnv(&disp);
        PutDrawEnv(&draw);
    }

    Render_UploadPackedRectangle(vs_getXY(0, 240), Title_developerLogo, vs_getWH(128 / 4, 14));

    for (i = 0; i < 364; ++i) {
        var_a3_2 = 0;
        if (i < 32) {
            var_a3_2 = (31 - i) * 4;
        } else if (i > 331) {
            var_a3_2 = (i - 331) * 4;
        } else if ((Input_buttonsState & 0xFFFF) != 0) {
            i = 331;
        }
        Title_SaveFiles_DrawImmediateSprite(vs_getXY(96, 104), vs_getUV0Clut(0, 240, 0, 253), vs_getWH(128, 13),
            var_a3_2 << 16);
        SetDefDispEnv(&disp, 0, (i & 1) * 256, 320, 240);
        SetDefDrawEnv(&draw, 0, (1 - (i & 1)) * 256, 320, 240);
        disp.screen.y = 8;
        disp.screen.h = 224;
        VSync(0);
        Input_UpdateControllers();
        PutDispEnv(&disp);
        PutDrawEnv(&draw);
    }
}

// decomp: TITLE.PRG _setMenuItemFadeIn (src/TITLE/TITLE.PRG/22C.c:3761)
void Title_InitializeMenuItem(int menuItem, char pos)
{
    Title_menuItemStates[menuItem].isEnabled = 1;
    Title_menuItemStates[menuItem].motion = Title_MenuItemMotion_Stationary;
    Title_menuItemStates[menuItem].textSaturation = 0;
    Title_menuItemStates[menuItem].outlineSaturation = 0;
    Title_menuItemStates[menuItem].textPosition = pos;
    Title_menuItemStates[menuItem].targetPosition = pos;
    Title_menuItemStates[menuItem].outlinePosition = pos;
    Title_menuItemStates[menuItem].textBlendFactor = 0;
}

// decomp: TITLE.PRG _copyTitleBgData (src/TITLE/TITLE.PRG/22C.c:3773)
void Title_LoadBackgroundTextures(void)
{
    RECT rect;
    int j;
    u_int pixelCounts;
    u_int i;
    u_long* data;
    u_long* p;

    i = 0;
    data = Core_Memory_AllocateFromLowEnd(544 * (512 + 64) * sizeof(u_short));
    p = data;


    for (; i < 222932 /* MENUBGSZ: size of Title::titleScreenBackground */ / sizeof(int);) {
        extern u_int Title_titleScreenBackground[];

        pixelCounts = Title_titleScreenBackground[i++];
        for (j = pixelCounts & 0xFFFF; j != 0; --j) {
            *p++ = 0;
        }

        for (j = pixelCounts >> 16; j != 0; --j) {
            *p++ = Title_titleScreenBackground[i++];
        }
    }
    setRECT(&rect, 480, 0, 544, 512);
    LoadImage(&rect, data);
    setRECT(&rect, 0, 448, 544, 64);
    LoadImage(&rect, data + 0x22000); // Outlines and menu BG
    DrawSync(0);

    for (i = 0; i < 8; ++i) {
        setRECT(&rect, 480, i + 384, 16, 1);
        LoadImage(&rect, (u_long*)Title_kMenuItemTextPalettes);
    }
    setRECT(&rect, 480, 394, 16, 1);
    LoadImage(&rect, (u_long*)Title_kMenuItemOutlinePalettes);
    Core_Memory_FreeFromLowEnd(data);
}

// decomp: TITLE.PRG _initTitleScreen (src/TITLE/TITLE.PRG/22C.c:3821)
void Title_InitializeTitleDisplay(void)
{
    DISPENV dispenv;
    DRAWENV drawenv;

    SetDefDispEnv(&dispenv, 512, 16, 512, 480);
    dispenv.screen.y = 8;
    dispenv.screen.h = 224;
    dispenv.isinter = 1;
    SetDefDrawEnv(&drawenv, 512, 0, 512, 480);
    while (VSync(1) < 248)
        ;
    PutDispEnv(&dispenv);
    PutDrawEnv(&drawenv);
    VSync(0);
}

// decomp: TITLE.PRG _initMenu (src/TITLE/TITLE.PRG/22C.c:3838)
void* Title_InitializeMenu(int menuItem)
{
    RECT rect;
    u_long* p;
    u_long* offset;
    int i;

    for (i = 9; i >= 0; --i) {
        Title_menuItemStates[i].isEnabled = 0;
    }

    Title_InitializeMenuItem(menuItem, 64);
    Title_InitializeMenuItem((menuItem + 1) & 3, 96);
    Title_InitializeMenuItem((menuItem + 3) & 3, 32);
    p = Core_Memory_AllocateFromLowEnd(0x22380);
    offset = p + 160 * 64;
    setRECT(&rect, 192, 448, 160, 64);
    StoreImage(&rect, offset);
    offset += 160 * 64 / sizeof(u_short);
    setRECT(&rect, 352, 448, 160, 64);
    StoreImage(&rect, offset);
    setRECT(&rect, 678, 376, 180, 24);
    StoreImage(&rect, offset + 160 * 64 * 3 / sizeof(u_short));
    return p;
}

// decomp: TITLE.PRG _freeHeap (src/TITLE/TITLE.PRG/22C.c:3867)
void Title_ReleaseMenuCapture(void* arg0) { Core_Memory_FreeFromLowEnd(arg0); }

// decomp: TITLE.PRG _fadeInMenu (src/TITLE/TITLE.PRG/22C.c:3869)
void Title_BlendMenuBackground(u_short* bgData, int textBlendFactor)
{
    RECT rect;
    int g0;
    int b0;
    int bdiff;
    int rdiff;
    int r0;
    int gdiff;
    int i;
    u_short* src;
    u_short* dst;
    int r1, g1, b1;

    i = 0;
    src = bgData + (160 * 64 * 2);
    dst = src + (160 * 64 * 2);

    for (i = 0; i < (160 * 64 * 2); ++i) {
        r0 = bgData[i];
        r1 = src[i];
        g0 = r0 & 0x3E0;
        b0 = r0 & 0x7C00;
        r0 &= 0x1F;
        g1 = r1 & 0x3E0;
        b1 = r1 & 0x7C00;
        r1 &= 0x1F;
        rdiff = (r0 - r1) * textBlendFactor;
        gdiff = (g0 - g1) * textBlendFactor;
        bdiff = (b0 - b1) * textBlendFactor;
        dst[i] = ((u_int)((((r1 << 5) + rdiff) & 0x3E0) | (((g1 << 5) + gdiff) & 0x7C00)
                          | (((b1 << 5) + bdiff) & 0xF8000))
                     >> 5)
               | 0x8000;
    }
    setRECT(&rect, 192, 448, 160, 64);
    LoadImage(&rect, (u_long*)dst);
    setRECT(&rect, 352, 448, 160, 64);
    LoadImage(&rect, (u_long*)dst + 0x1400);
    VSync(4);
    Input_UpdateControllers();
}

// decomp: TITLE.PRG _fadeInMenuCopyright (src/TITLE/TITLE.PRG/22C.c:3914)
void Title_BlendCopyright(u_short* arg0, int textBlendFactor)
{
    RECT rect;
    int i;
    int g0;
    int b0;
    int bdiff;
    int rdiff;
    int gdiff;
    int r1;
    int r0;
    int g1, b1;
    u_short* src;
    u_short* dst;

    src = arg0 + 0xF000;
    dst = src + (180 * 24);

    for (i = 0; i < (180 * 24); ++i) {
        extern u_short Title_copyrightImage[];

        r1 = src[i];
        r0 = Title_copyrightImage[i];
        g0 = r0 & 0x3E0;
        b0 = r0 & 0x7C00;
        g1 = r1 & 0x3E0;
        b1 = r1 & 0x7C00;
        r1 &= 0x1F;
        r0 &= 0x1F;
        rdiff = (r1 - r0) * textBlendFactor;
        gdiff = (g1 - g0) * textBlendFactor;
        bdiff = (b1 - b0) * textBlendFactor;
        dst[i] = ((u_int)((((r0 << 5) + rdiff) & 0x3E0) | (((g0 << 5) + gdiff) & 0x7C00)
                          | (((b0 << 5) + bdiff) & 0xF8000))
                  >> 5);
    }
    VSync(2);
    Input_UpdateControllers();
    setRECT(&rect, 678, 376, 180, 24);
    LoadImage(&rect, (u_long*)dst);
}

// decomp: TITLE.PRG _setMenuItemClut (src/TITLE/TITLE.PRG/22C.c:3957)
void Title_BlendMenuItemClut(int menuItem, int textBlendFactor, int clut0, int clut1)
{
    RECT rect;
    short clut[16];
    int i;
    int r0;
    int g0;
    int b0;
    int r1;
    int g1;
    int b1;
    int rdiff;
    int gdiff;
    int bdiff;

    clut[0] = 0;

    for (i = 1; i < 16; ++i) {
        r0 = Title_kMenuItemTextPalettes[clut0][i];
        r1 = Title_kMenuItemTextPalettes[clut1][i];
        g0 = r0 & 0x3E0;
        b0 = r0 & 0x7C00;
        r0 &= 0x1F;
        g1 = r1 & 0x3E0;
        b1 = r1 & 0x7C00;
        r1 &= 0x1F;
        rdiff = (r1 - r0) * textBlendFactor;
        gdiff = (g1 - g0) * textBlendFactor;
        bdiff = (b1 - b0) * textBlendFactor;
        clut[i] = 0x1F0;
        clut[i] = ((((r0 * 16 + rdiff) & 0x1F0) | ((g0 * 16 + gdiff) & 0x3E00)
                       | ((b0 * 16 + bdiff) & 0x7C000))
                      >> 4)
                | 0x8000;
    }
    setRECT(&rect, 480, menuItem + 384, 16, 1);
    LoadImage(&rect, (u_long*)&clut);
}

// decomp: TITLE.PRG _setTitleMenuState (src/TITLE/TITLE.PRG/22C.c:3997)
void Title_UpdateMenuAnimation(void)
{
    int i;
    int saturation;
    int textBlendFactor;

    i = 0;

    for (i = 0; i < 10; ++i) {
        if (Title_menuItemStates[i].isEnabled == 0) {
            continue;
        }
        saturation = Title_menuItemStates[i].textSaturation + 8;

        if (saturation > 128) {
            saturation = 128;
        }

        Title_menuItemStates[i].textSaturation = saturation;

        switch (Title_menuItemStates[i].motion) {
        case Title_MenuItemMotion_Stationary:
            if (saturation != 128 || Title_menuItemStates[i].textPosition != 64) {
                break;
            }
            Title_menuItemStates[i].outlinePosition = Title_menuItemStates[i].textPosition;
            textBlendFactor = Title_menuItemStates[i].textBlendFactor;
            if (textBlendFactor < 16) {
                Title_menuItemStates[i].outlineSaturation = textBlendFactor * 16;
                textBlendFactor += 4;
                if (textBlendFactor > 16) {
                    textBlendFactor = 16;
                }
                Title_menuItemStates[i].textBlendFactor = textBlendFactor;
            }
            break;
        case Title_MenuItemMotion_MovingUp:
            Title_menuItemStates[i].textPosition -= 4;
            if (Title_menuItemStates[i].textPosition == Title_menuItemStates[i].targetPosition) {
                Title_menuItemStates[i].motion = Title_MenuItemMotion_Stationary;
            }
            break;
        case Title_MenuItemMotion_MovingDown:
            Title_menuItemStates[i].textPosition += 4;
            if (Title_menuItemStates[i].textPosition == Title_menuItemStates[i].targetPosition) {
                Title_menuItemStates[i].motion = Title_MenuItemMotion_Stationary;
            }
            break;
        }

        textBlendFactor = Title_menuItemStates[i].textBlendFactor;

        if (Title_menuItemStates[i].motion < Title_MenuItemMotion_Submenu) {
            Title_BlendMenuItemClut(i, textBlendFactor, 0, 1);
        }
        if ((textBlendFactor != 0) && (Title_menuItemStates[i].textPosition != 64)) {
            Title_menuItemStates[i].textBlendFactor = textBlendFactor - 1;
        }

        if (Title_menuItemStates[i].outlineSaturation != 0) {
            if (Title_menuItemStates[i].outlinePosition != Title_menuItemStates[i].textPosition) {
                Title_menuItemStates[i].outlineSaturation -= 16;

                if (Title_menuItemStates[i].outlinePosition < Title_menuItemStates[i].textPosition) {
                    Title_menuItemStates[i].outlinePosition += 2;
                } else {
                    Title_menuItemStates[i].outlinePosition -= 2;
                }
            } else if ((Title_menuItemStates[i].textBlendFactor == 16)
                       && (Title_menuItemStates[i].outlineSaturation > 128)) {
                Title_menuItemStates[i].outlineSaturation -= 16;
            }
        }
    }
}

// decomp: TITLE.PRG _drawTitleMenuItems (src/TITLE/TITLE.PRG/22C.c:4073)
void Title_DrawMenuItems(void)
{
    typedef struct {
        u_long dmaTag;
        Render_SpriteTexturePagePacket sprite;
    } Title_MenuItemSpritePacket;

    static Title_MenuItemSpritePacket Title_menuItemSpritePackets[341];
    static char Title_menuItemDrawPadding[8] __attribute__((unused));

    int textUv;
    int j;
    Title_MenuItemAnimationState* menuState;
    int x;
    int wh;
    int i;
    Title_MenuItemSpritePacket* prim;
    int saturation;

    prim = Title_menuItemSpritePackets;

    for (i = 9; i >= 0; --i) {
        if ((Title_menuItemStates[i].isEnabled == 0)
            || (Title_menuItemStates[i].outlineSaturation == 0)) {
            continue;
        }
        prim->dmaTag = vs_getTag(prim->sprite, Render_LegacyPrimitiveAddress_End);
        prim->sprite.texturePageCommand = Title_outlineTexturePageWords[i];
        prim->sprite.colorCommand = vs_getRGB0Raw(Render_LegacyPrimitiveCode_SpriteSemiTrans,
            Title_menuItemStates[i].outlineSaturation * vs_getRGB888(1, 1, 1));
        if (Title_menuItemStates[i].motion == Title_MenuItemMotion_Submenu) {
            prim->sprite.position =
                (Title_kMenuOutlinePositions[i] + (Title_menuItemStates[i].outlinePosition << 16)) + 64;
        } else {
            prim->sprite.position =
                Title_kMenuOutlinePositions[i] + Title_menuItemStates[i].outlinePosition * vs_getXY(1, 1);
        }
        prim->sprite.uvPalette = Title_outlineUvClutWords[i];
        prim->sprite.size = Title_kMenuOutlineSizes[i];
        DrawPrim(prim);
        ++prim;
    }

    for (i = 9; i >= 0; --i) {
        menuState = Title_menuItemStates;
        if (menuState[i].isEnabled == 0) {
            continue;
        }
        saturation = menuState[i].textSaturation;
        x = 192;
        if (menuState[i].motion != 3) {
            x = menuState[i].textPosition + 128;
        }

        textUv = Title_textUvClutWords[i];
        wh = 128;
        if (x < 176) {
            textUv += 176 - x;
            wh = x - 48;
            x = 176;
        }
        if ((x + wh) > 336) {
            wh = 336 - x;
        }
        wh |= vs_getWH(0, 1);

        for (j = 0; j < 32; ++j) {
            int new_var = j + menuState[i].textPosition;
            prim->dmaTag = vs_getTag(prim->sprite, Render_LegacyPrimitiveAddress_End);
            prim->sprite.texturePageCommand = Title_textTexturePageWords[i];

            switch ((new_var >> 5) & 3) {
            case 0:
                prim->dmaTag = Render_LegacyPrimitiveAddress_End;
                break;
            case 1:
                prim->sprite.colorCommand = vs_getRGB0Raw(Render_LegacyPrimitiveCode_SpriteSemiTrans,
                    ((saturation * (new_var - 32)) >> 5) * vs_getRGB888(1, 1, 1));
                break;
            case 2:
                if (saturation == 128) {
                    prim->sprite.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_Sprite, 128, 128, 128);
                } else {
                    prim->sprite.colorCommand = vs_getRGB0Raw(
                        Render_LegacyPrimitiveCode_SpriteSemiTrans, saturation * vs_getRGB888(1, 1, 1));
                }
                break;
            case 3:
                prim->sprite.colorCommand = vs_getRGB0Raw(Render_LegacyPrimitiveCode_SpriteSemiTrans,
                    (((saturation * (127 - new_var)) >> 5) * vs_getRGB888(1, 1, 1)));
                break;
            }

            if (menuState[i].motion == 3) {
                if (saturation == 128) {
                    prim->sprite.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_Sprite, 128, 128, 128);
                } else {
                    prim->sprite.colorCommand = vs_getRGB0Raw(
                        Render_LegacyPrimitiveCode_SpriteSemiTrans, saturation * vs_getRGB888(1, 1, 1));
                }
            }
            prim->sprite.position = x | ((new_var + 240) << 16);
            prim->sprite.uvPalette = textUv + (j << 8);
            prim->sprite.size = wh;
            DrawPrim(prim);
            ++prim;
        }
    }
}

// decomp: TITLE.PRG _drawTitleMenu (src/TITLE/TITLE.PRG/22C.c:4183)
void Title_DrawMenu(void)
{
    typedef struct {
        u_long dmaTag;
        Render_SpriteTexturePagePacket sprite;
    } Title_MenuSpritePacket;

    static Title_MenuSpritePacket Title_menuSpritePackets[10];
    static char Title_menuDrawPadding[16] __attribute__((unused));

    int i;
    Title_MenuSpritePacket* sprt;

    Title_UpdateMenuAnimation();
    for (i = 0; i < 2; ++i) {
        sprt = Title_menuSpritePackets;
        sprt->sprite.texturePageCommand =
            vs_getTpage(192, 256, Render_TextureFormat_Direct16Bit, Render_BlendMode_Average, Render_Dithering_Off);
        sprt->dmaTag = vs_getTag(sprt->sprite, Render_LegacyPrimitiveAddress_End);
        sprt->sprite.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_Sprite, 128, 128, 128);
        sprt->sprite.position = vs_getXY(176, 256);
        sprt->sprite.uvPalette = vs_getUV0Clut(0, 192, 0, 0);
        sprt->sprite.size = vs_getWH(160, 64);
        DrawPrim(sprt);
        ++sprt;
        sprt->sprite.texturePageCommand =
            vs_getTpage(320, 256, Render_TextureFormat_Direct16Bit, Render_BlendMode_Average, Render_Dithering_Off);
        sprt->dmaTag = vs_getTag(sprt->sprite, Render_LegacyPrimitiveAddress_End);
        sprt->sprite.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_Sprite, 128, 128, 128);
        sprt->sprite.position = vs_getXY(176, 320);
        sprt->sprite.uvPalette = vs_getUV0Clut(32, 192, 0, 0);
        sprt->sprite.size = vs_getWH(160, 64);
        DrawPrim(sprt);
        Title_DrawMenuItems();
        if (i == 0) {
            VSync(0);
            Input_UpdateVibrationPatterns();
        } else {
            Movie_Intro_StopAfterAudioTail();
        }
    }
}

// decomp: TITLE.PRG _drawAndSyncTitleMenu (src/TITLE/TITLE.PRG/22C.c:4227)
void Title_DrawMenuAndPollInput(void)
{
    Title_DrawMenu();
    VSync(0);
    Input_UpdateControllers();
}

// decomp: TITLE.PRG _menuVibrationSettings (src/TITLE/TITLE.PRG/22C.c:4246)
void Title_ShowVibrationSettings(void)
{
    int i;
    int vibrationSetting;

    Title_menuItemStates[Title_MenuItemId_Continue].motion = Title_MenuItemMotion_MovingUp;
    Title_menuItemStates[Title_MenuItemId_Sound].motion = Title_MenuItemMotion_MovingDown;
    Title_menuItemStates[Title_MenuItemId_Sound].targetPosition = 128;
    Title_menuItemStates[Title_MenuItemId_Continue].targetPosition = 0;
    Title_menuItemStates[Title_MenuItemId_Vibration].motion = Title_MenuItemMotion_Submenu;
    vibrationSetting = Input_settings.vibrationOn + Title_MenuItemId_VibrationOff;
    Title_InitializeMenuItem(Title_MenuItemId_VibrationOn, 64);
    Title_InitializeMenuItem(Title_MenuItemId_VibrationOff, 96);
    Title_menuItemStates[Title_MenuItemId_VibrationOff].motion = Title_MenuItemMotion_Submenu;
    Title_menuItemStates[Title_MenuItemId_VibrationOn].motion = Title_MenuItemMotion_Submenu;
    Title_BlendMenuItemClut(Title_MenuItemId_VibrationOff, 0, 0, 0);
    Title_BlendMenuItemClut(Title_MenuItemId_VibrationOn, 0, 0, 0);
    for (i = 1; i < 9; ++i) {
        Title_BlendMenuItemClut(Title_MenuItemId_Vibration, i * 2, 1, 3);
        Title_menuItemStates[Title_MenuItemId_Vibration].textPosition -= 4;
        Title_DrawMenuAndPollInput();
    }
    Title_menuItemStates[Title_MenuItemId_NewGame].motion = Title_MenuItemMotion_Stationary;
    Title_menuItemStates[Title_MenuItemId_Continue].motion = Title_MenuItemMotion_Stationary;
    Title_menuItemStates[Title_MenuItemId_Sound].motion = Title_MenuItemMotion_Stationary;
    for (i = 1; i < 9; ++i) {
        Title_DrawMenuAndPollInput();
    }

    for (i = 0; i < 8; ++i) {
        Title_BlendMenuItemClut(vibrationSetting, i * 2, 0, 1);
        Title_menuItemStates[vibrationSetting].outlineSaturation =
            i < 4 ? i * 64 : Title_menuItemStates[vibrationSetting].outlineSaturation - 16;
        Title_DrawMenuAndPollInput();
    }
    while (1) {
        Title_DrawMenuAndPollInput();
        if (Input_buttonsState & (PADstart | PADRright)) {
            Title_PlayMenuSelectSound();
            Input_settings.vibrationOn = vibrationSetting - 5;
            if (vibrationSetting == Title_MenuItemId_VibrationOn) {
                Input_PlayVibrationPreset(0);
            }
            break;
        } else if (Input_buttonsState & PADRdown) {
            Title_PlayMenuLeaveSound();
            break;
        } else {
            if (Input_buttonsPressed.allButtons & (PADLup | PADLdown | PADselect)) {
                vibrationSetting = 11 - vibrationSetting;
                Title_PlayMenuChangeSound();

                for (i = 1; i < 8; ++i) {
                    Title_BlendMenuItemClut(vibrationSetting, i * 2, 0, 1);
                    Title_BlendMenuItemClut(11 - vibrationSetting, i * 2, 1, 0);
                    Title_menuItemStates[vibrationSetting].outlineSaturation =
                        i < 4 ? i * 64
                              : Title_menuItemStates[vibrationSetting].outlineSaturation - 16;
                    Title_menuItemStates[11 - vibrationSetting].outlineSaturation -= 16;
                    if (i == 7) {
                        break;
                    }
                    Title_DrawMenuAndPollInput();
                }
            }
        }
    }
    Title_InitializeMenuItem(Title_MenuItemId_Continue, 0);
    Title_InitializeMenuItem(Title_MenuItemId_Sound, 128);
    Title_menuItemStates[Title_MenuItemId_Continue].motion = Title_MenuItemMotion_MovingDown;
    Title_menuItemStates[Title_MenuItemId_Continue].targetPosition = 32;
    Title_menuItemStates[Title_MenuItemId_Sound].motion = Title_MenuItemMotion_MovingUp;
    Title_menuItemStates[Title_MenuItemId_Sound].targetPosition = 96;
    for (i = 1; i < 9; ++i) {
        Title_BlendMenuItemClut(2, i * 2, 3, 1);
        Title_menuItemStates[2].textPosition += 4;
        if (Title_menuItemStates[Title_MenuItemId_VibrationOff].textSaturation < 24) {
            Title_menuItemStates[Title_MenuItemId_VibrationOff].textSaturation = 0;
        } else {
            Title_menuItemStates[Title_MenuItemId_VibrationOff].textSaturation -= 24;
        }
        if (Title_menuItemStates[Title_MenuItemId_VibrationOn].textSaturation < 24) {
            Title_menuItemStates[Title_MenuItemId_VibrationOn].textSaturation = 0;
        } else {
            Title_menuItemStates[Title_MenuItemId_VibrationOn].textSaturation -= 24;
        }
        if (Title_menuItemStates[Title_MenuItemId_VibrationOff].outlineSaturation != 0) {
            Title_menuItemStates[Title_MenuItemId_VibrationOff].outlineSaturation -= 16;
        }
        if (Title_menuItemStates[Title_MenuItemId_VibrationOn].outlineSaturation != 0) {
            Title_menuItemStates[Title_MenuItemId_VibrationOn].outlineSaturation -= 16;
        }
        Title_DrawMenuAndPollInput();
    }
    Title_menuItemStates[Title_MenuItemId_VibrationOff].isEnabled = 0;
    Title_menuItemStates[Title_MenuItemId_VibrationOn].isEnabled = 0;
    Title_menuItemStates[Title_MenuItemId_Vibration].outlinePosition = 64;
    Title_menuItemStates[Title_MenuItemId_Vibration].outlineSaturation = 64;
    Title_DrawMenuAndPollInput();
    Title_menuItemStates[Title_MenuItemId_Vibration].outlineSaturation = 128;
    Title_DrawMenuAndPollInput();
    Title_menuItemStates[Title_MenuItemId_Vibration].outlineSaturation = 192;
    Title_DrawMenu();
    Title_menuItemStates[Title_MenuItemId_Vibration].textBlendFactor = 16;
    Title_menuItemStates[Title_MenuItemId_Vibration].motion = Title_MenuItemMotion_Stationary;
}

// decomp: TITLE.PRG _menuSoundSettings (src/TITLE/TITLE.PRG/22C.c:4353)
void Title_ShowSoundSettings(void)
{
    int i;
    int soundSetting;

    Title_menuItemStates[Title_MenuItemId_Vibration].motion = Title_MenuItemMotion_MovingUp;
    Title_menuItemStates[Title_MenuItemId_NewGame].motion = Title_MenuItemMotion_MovingDown;
    Title_menuItemStates[Title_MenuItemId_NewGame].targetPosition = 128;
    Title_menuItemStates[Title_MenuItemId_Vibration].targetPosition = 0;
    Title_menuItemStates[Title_MenuItemId_Sound].motion = Title_MenuItemMotion_Submenu;
    soundSetting = Title_MenuItemId_Stereo - (Input_settings.monoSound * 3);
    Title_InitializeMenuItem(Title_MenuItemId_Stereo, 64);
    Title_InitializeMenuItem(Title_MenuItemId_Mono, 96);
    Title_menuItemStates[Title_MenuItemId_Mono].motion = Title_MenuItemMotion_Submenu;
    Title_menuItemStates[Title_MenuItemId_Stereo].motion = Title_MenuItemMotion_Submenu;
    Title_BlendMenuItemClut(Title_MenuItemId_Mono, 0, 0, 0);
    Title_BlendMenuItemClut(Title_MenuItemId_Stereo, 0, 0, 0);
    for (i = 1; i < 9; ++i) {
        Title_BlendMenuItemClut(Title_MenuItemId_Sound, i * 2, 1, 3);
        Title_menuItemStates[Title_MenuItemId_Sound].textPosition -= 4;
        Title_DrawMenuAndPollInput();
    }
    Title_menuItemStates[Title_MenuItemId_NewGame].motion = Title_MenuItemMotion_Stationary;
    Title_menuItemStates[Title_MenuItemId_Continue].motion = Title_MenuItemMotion_Stationary;
    Title_menuItemStates[Title_MenuItemId_Vibration].motion = Title_MenuItemMotion_Stationary;
    for (i = 1; i < 9; ++i) {
        Title_DrawMenuAndPollInput();
    }
    for (i = 0; i < 8; ++i) {
        Title_BlendMenuItemClut(soundSetting, i * 2, 0, 1);
        Title_menuItemStates[soundSetting].outlineSaturation =
            i < 4 ? i << 6 : Title_menuItemStates[soundSetting].outlineSaturation - 16;
        Title_DrawMenuAndPollInput();
    }
    while (1) {
        Title_DrawMenuAndPollInput();
        if (Input_buttonsState & (PADstart | PADRright)) {
            Title_PlayMenuSelectSound();
            soundSetting = (soundSetting + 1) & 1;
            Input_settings.monoSound = soundSetting;
            Audio_SetMonoOutput(soundSetting);
            Audio_Control_SetDiscMixVolume(0x7F);
            break;
        } else if (Input_buttonsState & PADRdown) {
            Title_PlayMenuLeaveSound();
            break;
        } else {
            if (Input_buttonsPressed.allButtons & (PADLup | PADLdown | PADselect)) {
                soundSetting = 11 - soundSetting;
                Title_PlayMenuChangeSound();
                for (i = 1; i < 8; ++i) {
                    Title_BlendMenuItemClut(soundSetting, i * 2, 0, 1);
                    Title_BlendMenuItemClut(11 - soundSetting, i * 2, 1, 0);
                    Title_menuItemStates[soundSetting].outlineSaturation =
                        i < 4 ? i << 6
                              : Title_menuItemStates[soundSetting].outlineSaturation - 16;
                    Title_menuItemStates[11 - soundSetting].outlineSaturation -= 16;
                    if (i == 7) {
                        break;
                    }
                    Title_DrawMenuAndPollInput();
                }
            }
        }
    }
    Title_InitializeMenuItem(Title_MenuItemId_Vibration, 0);
    Title_InitializeMenuItem(Title_MenuItemId_NewGame, 128);
    Title_menuItemStates[Title_MenuItemId_Vibration].motion = Title_MenuItemMotion_MovingDown;
    Title_menuItemStates[Title_MenuItemId_Vibration].targetPosition = 32;
    Title_menuItemStates[Title_MenuItemId_NewGame].motion = Title_MenuItemMotion_MovingUp;
    Title_menuItemStates[Title_MenuItemId_NewGame].targetPosition = 96;
    for (i = 1; i < 9; ++i) {
        Title_BlendMenuItemClut(Title_MenuItemId_Sound, i * 2, 3, 1);
        Title_menuItemStates[Title_MenuItemId_Sound].textPosition += 4;
        if (Title_menuItemStates[Title_MenuItemId_Mono].textSaturation < 24) {
            Title_menuItemStates[Title_MenuItemId_Mono].textSaturation = 0;
        } else {
            Title_menuItemStates[Title_MenuItemId_Mono].textSaturation -= 24;
        }
        if (Title_menuItemStates[Title_MenuItemId_Stereo].textSaturation < 24) {
            Title_menuItemStates[Title_MenuItemId_Stereo].textSaturation = 0;
        } else {
            Title_menuItemStates[Title_MenuItemId_Stereo].textSaturation -= 24;
        }
        if (Title_menuItemStates[Title_MenuItemId_Mono].outlineSaturation != 0) {
            Title_menuItemStates[Title_MenuItemId_Mono].outlineSaturation -= 16;
        }
        if (Title_menuItemStates[Title_MenuItemId_Stereo].outlineSaturation != 0) {
            Title_menuItemStates[Title_MenuItemId_Stereo].outlineSaturation -= 16;
        }
        Title_DrawMenuAndPollInput();
    }
    Title_menuItemStates[Title_MenuItemId_Mono].isEnabled = 0;
    Title_menuItemStates[Title_MenuItemId_Stereo].isEnabled = 0;
    Title_menuItemStates[Title_MenuItemId_Sound].outlinePosition = 64;
    Title_menuItemStates[Title_MenuItemId_Sound].outlineSaturation = 64;
    Title_DrawMenuAndPollInput();
    Title_menuItemStates[Title_MenuItemId_Sound].outlineSaturation = 128;
    Title_DrawMenuAndPollInput();
    Title_menuItemStates[Title_MenuItemId_Sound].outlineSaturation = 192;
    Title_DrawMenu();
    Title_menuItemStates[Title_MenuItemId_Sound].textBlendFactor = 16;
    Title_menuItemStates[Title_MenuItemId_Sound].motion = Title_MenuItemMotion_Stationary;
}

// decomp: TITLE.PRG _nop (src/TITLE/TITLE.PRG/22C.c:4461)
int Title_NewGameChoice(void) { return 0; }

// decomp: TITLE.PRG _initEnvironment (src/TITLE/TITLE.PRG/22C.c:4463)
void Title_InitializeSettings(void)
{
    int monoSound;
    int vibrationOn;

    vibrationOn = Input_settings.vibrationOn;
    monoSound = Input_settings.monoSound;
    memset(&Input_settings, 0, sizeof Input_settings);
    Input_settings.menuFlags = 0x2D8;
    Input_settings.information = 1;
    Input_settings.simpleMap = 3;
    Input_settings.containerEmpty = 1;
    Input_settings.abilityTimingDisplay = 1;
    Input_settings.cursorMemory = 1;
    if (Title_titleScreenVisitCount == 0) {
        Title_ShowPublisherAndDeveloper();
        vibrationOn = 1;
        monoSound = 0;
    }
    Input_settings.monoSound = monoSound != 0;
    Input_settings.vibrationOn = vibrationOn != 0;
    Audio_SetMonoOutput(Input_settings.monoSound);
    Audio_Control_SetDiscMixVolume(0x7F);
    memset(&Audio_Seq_unusedInventoryIndices, 0, sizeof Audio_Seq_unusedInventoryIndices);
    Audio_Seq_unusedStateFlags.difficulty = 1;
    Audio_Seq_unusedStateFlags.puzzleMode = 1;
}

// decomp: TITLE.PRG vs_title_exec (src/TITLE/TITLE.PRG/22C.c:4491)
int Title_Run(void)
{
    static struct {
        u_long dmaTag;
        Render_TileTexturePagePacket tile;
        char reserved14[44];
    } Title_titleTransitionPacketBuffer;
    static int Title_lastButtonActivityFrame;
    static int Title_lastIntroMovieFrame;
    static char Title_titleLoopPadding[8] __attribute__((unused));

    RECT rect;
    int i;
    int menuItem;
    int selectedOption;
    void* menuData;

    if (Boot_isClearSavePending != 0) {
        Boot_isClearSavePending = 0;
        Title_titleScreenVisitCount = 0;
        Title_ShowClearSaveScreen();
    }
    Title_InitializeNewGameData();
    Title_InitializeSettings();
    Movie_Intro_isPlaying = 0;
    ++Title_titleScreenVisitCount;
    menuItem = Save_MemoryCard_AnySaveFileExists();

    do {
        for (i = 7; i >= 0; --i) {
            Title_menuItemStates[i].isEnabled = 0;
        }

        Title_LoadBackgroundTextures();
        Movie_Intro_Start();
        Movie_Intro_Play();
        Title_InitializeTitleDisplay();
        menuData = Title_InitializeMenu(menuItem);
        for (i = 32; i >= 0; i -= 2) {
            Title_BlendCopyright(menuData, i);
        }
        VSync(0);
        setRECT(&rect, 688, 256, 160, 128);
        StoreImage(&rect, menuData);
        VSync(0);
        for (i = 32; i >= 0; i -= 4) {
            Title_BlendMenuBackground(menuData, i);
            Title_DrawMenu();
        }
        Title_ReleaseMenuCapture(menuData);
        for (i = 0; i < 8; ++i) {
            Title_DrawMenuAndPollInput();
        }

        Input_InvalidateControllerConnections();
        Title_lastIntroMovieFrame = VSync(-1);
        Title_lastButtonActivityFrame = VSync(-1);

        while (1) {
            VSync(0);
            Input_UpdateControllers();
            if (Movie_Intro_isPlaying != 0) {
                Title_lastIntroMovieFrame = VSync(-1);
            }
            if ((Input_buttonsState & 0xFFFF) != 0) {
                Title_lastButtonActivityFrame = VSync(-1);
            }
            if (((VSync(-1) - Title_lastButtonActivityFrame) > 1000)
                && ((VSync(-1) - Title_lastIntroMovieFrame) > 100)) {
                selectedOption = Title_MenuItemId_Timeout;
                break;
            }
            if (Input_buttonsState & (PADRright | PADstart)) {
                switch (menuItem) {
                case Title_MenuItemId_NewGame:
                    selectedOption = Title_NewGameChoice();
                    break;
                case Title_MenuItemId_Continue:
                    selectedOption = Title_MenuItemId_Continue;
                    break;
                case Title_MenuItemId_Vibration:
                    Title_PlayMenuSelectSound();
                    Title_ShowVibrationSettings();
                    break;
                case Title_MenuItemId_Sound:
                    Title_PlayMenuSelectSound();
                    Title_ShowSoundSettings();
                    break;
                }

                if ((menuItem < Title_MenuItemId_Vibration)
                    && (selectedOption >= Title_MenuItemId_NewGame)) {
                    break;
                }
                Title_lastButtonActivityFrame = VSync(-1);
            }
            i = 0;
            if (Input_buttonsState & (PADLup | PADLleft)) {
                --i;
            }
            if (Input_buttonsState & (PADselect | PADLright | PADLdown)) {
                ++i;
            }
            if (i != 0) {
                menuItem = (menuItem + i) & 3;
                Title_PlayMenuChangeSound();
                if (i == 1) {
                    Title_InitializeMenuItem((menuItem + 1) & 3, 0x80);
                    for (i = 0; i < 4; ++i) {
                        Title_menuItemStates[i].motion = Title_MenuItemMotion_MovingUp;
                        Title_menuItemStates[i].targetPosition = Title_menuItemStates[i].textPosition - 32;
                    }
                } else {
                    Title_InitializeMenuItem((menuItem - 1) & 3, 0);
                    for (i = 0; i < 4; ++i) {
                        Title_menuItemStates[i].motion = Title_MenuItemMotion_MovingDown;
                        Title_menuItemStates[i].targetPosition = Title_menuItemStates[i].textPosition + 32;
                    }
                }
                for (i = 0; i < 10; ++i) {
                    Title_DrawMenuAndPollInput();
                }
            }
            Title_DrawMenu();
        }
        if (selectedOption >= Title_MenuItemId_NewGame) {
            if (Movie_Intro_isPlaying != 0) {
                Movie_Intro_Stop();
            }
            if (selectedOption == Title_MenuItemId_Continue) {
                Title_PlayMenuSelectSound();
            } else {
                Title_PlayNewGameSounds();
                Title_PrepareExit(selectedOption);
            }
        }

        if (selectedOption == Title_MenuItemId_Timeout) {
            selectedOption = 4 - (Title_titleScreenVisitCount & 1);
            Title_PrepareExit(selectedOption);
        }
        for (i = 64; i > 0; --i) {
            VSync(0);
            Input_UpdateControllers();
            Title_titleTransitionPacketBuffer.dmaTag = vs_getTag(Title_titleTransitionPacketBuffer.tile, Render_LegacyPrimitiveAddress_End);
            Title_titleTransitionPacketBuffer.tile.texturePageCommand =
                vs_getTpage(320, 0, Render_TextureFormat_Direct16Bit, Render_BlendMode_Subtract, Render_Dithering_Off);
            Title_titleTransitionPacketBuffer.tile.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_TileSemiTrans, 8, 8, 8);
            Title_titleTransitionPacketBuffer.tile.position = vs_getXY(0, 0);
            Title_titleTransitionPacketBuffer.tile.size = vs_getWH(640, 480);
            DrawPrim(&Title_titleTransitionPacketBuffer);
            Input_UpdateVibrationPatterns();
        }
        VSync(0);
        if (selectedOption == Title_MenuItemId_Continue) {
            selectedOption = Title_ShowContinueScreen();
            if (selectedOption == Title_MenuItemId_Continue) {
                Title_PrepareExit(Title_MenuItemId_Continue);
            }
        }
    } while (selectedOption < Title_MenuItemId_NewGame);
    return selectedOption;
}

// decomp: TITLE.PRG _initScreen (src/TITLE/TITLE.PRG/22C.c:4658)
void Title_InitializeGameDisplay(int w, int h, int distance, int r, int g, int b)
{
    RECT rect = (RECT) { 0, 0, 1024, 512 };
    Core_Gte_SetGeomOffset(w / 2, h / 2);
    Core_Gte_SetGeomScreen(distance);
    SetDefDrawEnv(Render_legacyDrawEnvironments, 0, 0, w, h - 16);
    SetDefDispEnv(Render_legacyDisplayEnvironments, w, 0, w, h - 16);
    SetDefDrawEnv(Render_legacyDrawEnvironments + 1, w, 0, w, h - 16);
    SetDefDispEnv(Render_legacyDisplayEnvironments + 1, 0, 0, w, h - 16);
    setRECT(&Render_legacyDisplayEnvironments[0].screen, 0, 8, 256, 224);
    setRECT(&Render_legacyDisplayEnvironments[1].screen, 0, 8, 256, 224);
    Render_legacyDrawEnvironments[1].dtd = 0;
    Render_legacyDrawEnvironments[0].dtd = 0;
    Render_legacyDrawEnvironments[1].isbg = 1;
    Render_legacyDrawEnvironments[0].isbg = 1;
    setRGB0(&Render_legacyDrawEnvironments[0], r, g, b);
    setRGB0(&Render_legacyDrawEnvironments[1], r, g, b);
    ClearImage(&rect, 0, 0, 0);
    PutDispEnv(&Render_legacyDisplayEnvironments[Render_frameBuf]);
    PutDrawEnv(&Render_legacyDrawEnvironments[Render_frameBuf]);
}

// decomp: TITLE.PRG _renderScreen (src/TITLE/TITLE.PRG/22C.c:4684)
int Title_PresentGameFrame(u_long* ot)
{
    int _[2] __attribute__((unused));
    int vsync;

    Render_frameBuf = Render_frameBuf == 0;
    vsync = State_UpdatePlayTimeAndLoading(0);
    PutDispEnv(Render_frameBuf + Render_legacyDisplayEnvironments);
    PutDrawEnv(Render_frameBuf + Render_legacyDrawEnvironments);
    DrawOTag(ot);
    return vsync;
}

// decomp: TITLE.PRG _initGameData (src/TITLE/TITLE.PRG/22C.c:4698)
void Title_InitializeNewGameData(void)
{
    int i;
    int j;
    char* v0;

    Title_InitializeGameDisplay(640, 240, 512, 0, 0, 0);
    i = 0x43F;
    v0 = ((char*)&Audio_Seq_unusedStateFlags) + i;
    do {
        *v0-- = 0;
    } while (--i >= 0);

    Core_Memory_CopyAlignedWords(State_learnedActionFlags, Title_initialLearnedActionBits, sizeof Title_initialLearnedActionBits);
    Core_Memory_ZeroAlignedWords(&State_roomVisitProgress, sizeof State_roomVisitProgress);
    Core_Memory_ZeroAlignedWords(&State_playTime, sizeof State_playTime);
    Core_Memory_ZeroAlignedWords(&Audio_Seq_unusedScoredata, sizeof Audio_Seq_unusedScoredata);
    State_currentRoomStateSlot = 0;
    Core_Memory_ZeroAlignedWords(State_recentRoomEnemyStates, sizeof State_recentRoomEnemyStates);
    Core_Memory_ZeroAlignedWords(&Audio_Seq_unusedArtsStatus, sizeof Audio_Seq_unusedArtsStatus);

    for (i = 0; i < 8; ++i) {
        for (j = 0; j < 3; ++j) {
            Audio_Seq_unusedScoredata.bossTimeTrialScores[i][j].time = -8388608; // int24_min
            Audio_Seq_unusedScoredata.bossTimeTrialScores[i][j].round = 0;
            Audio_Seq_unusedScoredata.bossTimeTrialScores[i][j].difficulty = 0;
        }
    }

    for (i = 0; i < 1; ++i) {
        Core_Memory_ZeroAlignedWords((char*)&State_ashleySnapshot + i * sizeof State_ashleySnapshot, 0x50);
    }

    Core_Memory_ZeroAlignedWords(&State_savedEntryDoor, sizeof State_savedEntryDoor);
    Core_Memory_CopyAlignedWords(&Audio_Seq_unusedInventory, Title_newGameInventoryTemplate, sizeof Audio_Seq_unusedInventory);

    State_tickSpeed = 2;
    State_savedEntryDoor.entryTileX = 4;
    State_savedEntryDoor.entryTileZ = 2;
    State_savedEntryDoor.tileLocation.entryDirection = 0;
    State_savedEntryDoor.entryElevation = 2;
}

// decomp: TITLE.PRG _setTitleExitFlags (src/TITLE/TITLE.PRG/22C.c:4741)
void Title_PrepareExit(int cause)
{
    switch (cause) {
    case 0: // New Game
        Audio_Seq_unusedStateFlags.opaqueC = 0;
        Audio_Seq_unusedStateFlags.opaqueD = 0;
        State_savedEntryDoor.destinationZoneId = 1;
        State_savedEntryDoor.destinationRoomId = 0;
        return;
    case 3: // Timeout cutscene1
        Audio_Seq_unusedStateFlags.opaqueC = 0;
        Audio_Seq_unusedStateFlags.opaqueD = 1;
        Audio_Seq_unusedStateFlags.opaqueE = 1;
        State_savedEntryDoor.destinationZoneId = 20;
        State_savedEntryDoor.destinationRoomId = 0;
        return;
    case 4: // Timeout cutscene2
        Audio_Seq_unusedStateFlags.opaqueC = 0;
        Audio_Seq_unusedStateFlags.opaqueD = 1;
        Audio_Seq_unusedStateFlags.opaqueE = 1;
        State_savedEntryDoor.destinationZoneId = 2;
        State_savedEntryDoor.destinationRoomId = 1;
        return;
    case 1: // Continue
        Audio_Seq_unusedStateFlags.opaqueC = 0;
        Audio_Seq_unusedStateFlags.opaqueD = 0;
        return;
    default:
        Audio_Seq_unusedStateFlags.opaqueC = 0;
        Audio_Seq_unusedStateFlags.opaqueD = 0;
        return;
    }
}

