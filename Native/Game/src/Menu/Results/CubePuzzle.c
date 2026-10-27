// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/StatusEffects.h"
#include "CdDrive.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/Zone.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/Images.h"
#include "Render/TextureAnimation.h"
#include "Render/Textures.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

// decomp: _clutBuf (_clutBuf@src/MENU/MENUF.PRG/3B8.c)
u_short Menu_Results_clutBuf[160];

// decomp: _puzzleParTime (_puzzleParTime@src/MENU/MENUF.PRG/3B8.c)
short Menu_Results_puzzleParTime;

// decomp: _puzzleRankCap (_puzzleRankCap@src/MENU/MENUF.PRG/3B8.c)
short Menu_Results_puzzleRankCap;

// decomp: local static of _renderCubePuzzleRank, at file scope so the module reset reaches it
static CVECTOR Menu_Results_cubePuzzleRankColors[] = { { 128, 128, 128 }, { 128, 128, 128 } };

// decomp: local static of _renderEvolveOrDie, at file scope so the module reset reaches it
static CVECTOR Menu_Results_evolveOrDieColors[] = { { 128, 96, 64 }, { 200, 180, 160 }, { 128, 96, 64 } };

// decomp: local static of _renderFoodchain, at file scope so the module reset reaches it
static CVECTOR Menu_Results_foodChainColors[] = { { 128, 96, 64 }, { 200, 180, 160 }, { 128, 96, 64 } };

// decomp: MENUF.PRG _pulseCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:1294)
void Menu_Results_PulseCubePuzzleClut(int blend_weight)
{
    RECT rect;
    int weighted_red;
    int b;
    int weighted_green;
    int i;
    int r;
    int packed_blue;
    int packed_red_green;

    for (i = 0; i < 32; ++i) {

        b = Menu_Results_clutBuf[128 + i];

        weighted_red = ((Menu_Results_clutBuf[96 + i] & 0x1F) * blend_weight)
               + ((Menu_Results_clutBuf[64 + i] & 0x1F) * (16 - blend_weight));

        r = (weighted_red / 16) & 0x1F;

        weighted_green = ((Menu_Results_clutBuf[96 + i] & 0x3E0) * blend_weight)
               + ((Menu_Results_clutBuf[64 + i] & 0x3E0) * (16 - blend_weight));

        packed_red_green = r | ((weighted_green / 16) & 0x3E0);

        b = ((Menu_Results_clutBuf[96 + i] & 0x7C00) * blend_weight)
          + ((Menu_Results_clutBuf[64 + i] & 0x7C00) * (16 - blend_weight));

        packed_blue = (b / 16) & 0x7C00;

        Menu_Results_clutBuf[128 + i] = (packed_red_green | packed_blue) | (Menu_Results_clutBuf[96 + i] & 0x8000);
    }

    setRECT(&rect, 768, 511, 160, 1);

    LoadImage(&rect, (u_long*)Menu_Results_clutBuf);
}

// decomp: MENUF.PRG _initCubePuzzleStart (src/MENU/MENUF.PRG/3B8.c:2260)
int Menu_Results_InitializeCubePuzzleStart(void)
{
    static Content_DiscQueueSlot* Menu_Results_initializeCubePuzzleStartIqDisCdSlot;
    static void* Menu_Results_initializeCubePuzzleStartIqDisData;

    Field_MapRoom* room;

    struct {
        short zoneId;
        short mapId;
        short par;
        short rankCap;
    }* puzzleData;

    if (Battle_unusedSubmenuState == 0) {

        Render_ReserveTexturePageRange(0x1D, 2, 5);

        Menu_Results_initializeCubePuzzleStartIqDisData = Core_Memory_AllocateFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Iq].size);
        Menu_Results_initializeCubePuzzleStartIqDisCdSlot = Content_AllocateDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Iq]);

        Content_EnqueueDiscRequest(Menu_Results_initializeCubePuzzleStartIqDisCdSlot, Menu_Results_initializeCubePuzzleStartIqDisData);

        ++Battle_unusedSubmenuState;

    } else if (Battle_unusedSubmenuState == 1) {
        if (Menu_Results_initializeCubePuzzleStartIqDisCdSlot->state == Content_DiscRequestState_Loaded) {
            int i;
            TIM_IMAGE tim;
            for (i = 0; i < 2; ++i) {

                Render_ParseTextureImage(Menu_Results_initializeCubePuzzleStartIqDisData + i * 0x8220, &tim);

                if (tim.paddr != NULL) {
                    tim.prect->x = 0x340 + i * 0x40;
                    tim.prect->y = 0x100;
                    tim.prect->h = 0xFF;

                    LoadImage(tim.prect, (u_long*)tim.paddr);
                }

                if (i == 0) {
                    if (tim.caddr != NULL) {
                        tim.crect->x = 0x300;
                        tim.crect->y = 0x1FF;
                        tim.crect->w = 0xA0;
                        tim.crect->h = 1;
                        tim.caddr[0] = 0;

                        LoadImage(tim.crect, tim.caddr);
                        Core_Memory_CopyAlignedWords(Menu_Results_clutBuf, tim.caddr, sizeof Menu_Results_clutBuf);
                    }
                }
            }

            Menu_Results_puzzleParTime = 600;
            Menu_Results_puzzleRankCap = 1;
            room = Field_roomData.minimapScene->rooms;
            puzzleData = Menu_Results_initializeCubePuzzleStartIqDisData + 0x10440;

            for (i = 0; i < 64; ++i, ++puzzleData) {
                if ((puzzleData->zoneId == room->zoneId)
                    && (puzzleData->mapId == room->mapId)) {
                    Menu_Results_puzzleParTime = puzzleData->par;
                    Menu_Results_puzzleRankCap = puzzleData->rankCap;
                    break;
                }
            }

            Content_ReleaseDiscRequest(Menu_Results_initializeCubePuzzleStartIqDisCdSlot);

            ++Battle_unusedSubmenuState;
        }
    } else {
        Core_Memory_FreeFromHighEnd(Menu_Results_initializeCubePuzzleStartIqDisData);

        Menu_Results_onScreenMapCompletion = 0;
        Menu_Results_onScreenScore = 0;
        Menu_Results_screenTimer = 0;
        Menu_Results_screenState = 0;
        Menu_Results_elementAnimationState = 0;
        Menu_Results_buffReelSelection = 0;
        Battle_unusedSubmenuState = 0;

        Menu_Framework_NoOperation();
        return 1;
    }

    return 0;
}

// decomp: MENUF.PRG _initCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:2351)
int Menu_Results_InitializeCubePuzzleResults(void)
{
    if (Audio_Seq_unusedStateFlags.puzzleSetMaxTimer != 0) {
        Audio_Seq_unusedStateFlags.puzzleTimerMins = 59;
        Audio_Seq_unusedStateFlags.puzzleTimerSeconds = 59;
        Audio_Seq_unusedStateFlags.puzzleSetMaxTimer = 0;
        Audio_Seq_unusedStateFlags.puzzleTimerHundredths = 99;
    }

    return Menu_Results_InitializeCubePuzzleStart();
}

// decomp: MENUF.PRG _initCubePuzzleQuit (src/MENU/MENUF.PRG/3B8.c:2363)
int Menu_Results_InitializeCubePuzzleQuit(void)
{
    static Content_DiscQueueSlot* Menu_Results_initializeCubePuzzleQuitEscDisCdSLot;
    static void* Menu_Results_initializeCubePuzzleQuitEscDisData;

    TIM_IMAGE tim;
    int i;

    if (Battle_unusedSubmenuState == 0) {

        Render_ReserveTexturePageRange(0x1D, 1, 5);

        Menu_Results_initializeCubePuzzleQuitEscDisData = Core_Memory_AllocateFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Esc].size);
        Menu_Results_initializeCubePuzzleQuitEscDisCdSLot = Content_AllocateDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Esc]);

        Content_EnqueueDiscRequest(Menu_Results_initializeCubePuzzleQuitEscDisCdSLot, Menu_Results_initializeCubePuzzleQuitEscDisData);

        ++Battle_unusedSubmenuState;

    } else if (Battle_unusedSubmenuState == 1) {
        if (Menu_Results_initializeCubePuzzleQuitEscDisCdSLot->state == Content_DiscRequestState_Loaded) {
            for (i = 0; i <= 0; ++i) {

                Render_ParseTextureImage(Menu_Results_initializeCubePuzzleQuitEscDisData + i * 0x8220, &tim);

                if (tim.paddr != NULL) {
                    tim.prect->x = 832 + i * 64;
                    tim.prect->y = 256;
                    tim.prect->h = 255;

                    LoadImage(tim.prect, (u_long*)tim.paddr);
                }

                if ((i == 0) && (tim.caddr != NULL)) {
                    tim.crect->x = 768;
                    tim.crect->y = 511;
                    tim.crect->w = 160;
                    tim.crect->h = 1;
                    *tim.caddr = 0;

                    LoadImage(tim.crect, tim.caddr);
                    Core_Memory_CopyAlignedWords(Menu_Results_clutBuf, tim.caddr, sizeof Menu_Results_clutBuf);
                }
            }

            Content_ReleaseDiscRequest(Menu_Results_initializeCubePuzzleQuitEscDisCdSLot);

            ++Battle_unusedSubmenuState;
        }
    } else {
        Core_Memory_FreeFromHighEnd(Menu_Results_initializeCubePuzzleQuitEscDisData);

        Menu_Results_onScreenMapCompletion = 0;
        Menu_Results_onScreenScore = 0;
        Menu_Results_screenTimer = 0;
        Menu_Results_screenState = 0;
        Menu_Results_elementAnimationState = 0;
        Menu_Results_buffReelSelection = 0;
        Battle_unusedSubmenuState = 0;
        return 1;
    }

    return 0;
}

// decomp: MENUF.PRG _renderCubePuzzleStart (src/MENU/MENUF.PRG/3B8.c:2432)
int Menu_Results_UpdateCubePuzzleStart(void)
{
    if (Menu_Results_screenState != 0) {
        return 0;
    }

    if (Menu_Results_screenTimer == 0) {
        Audio_PlaySfxCentered(0x7E, 0x7D);
    }

    if (Menu_Results_screenTimer == 30) {
        Audio_PlaySfxCentered(0x7E, 0x7D);
    }

    Menu_Results_DrawEvolveOrDie(160, 64, Menu_Results_screenTimer);
    Menu_Results_DrawAverageTimeHeading(128, 116, Menu_Results_screenTimer - 15);
    Menu_Results_DrawCubePuzzleParTime(192, 144, Menu_Results_screenTimer - 30);
    Menu_Results_DrawPressButtonPrompt(214, 187, Menu_Results_screenTimer - 30, Menu_Results_screenTimer);

    if (Menu_Results_screenTimer < 32767) {
        ++Menu_Results_screenTimer;
    }

    if (Input_buttonsPressed.allButtons & PADRright) {
        if (Menu_Results_screenTimer >= 78) {

            Audio_StopSfx(126, 0);
            Render_RetireTexturePageRange(29, 2, 5);

            return 1;
        }

        Audio_StopSfx(126, 0);

        Menu_Results_screenTimer = 78;
    }

    return 0;
}

// decomp: MENUF.PRG _renderCubePuzzleEnd (src/MENU/MENUF.PRG/3B8.c:2477)
int Menu_Results_UpdateCubePuzzleResults(void)
{
    static int Menu_Results_cubePuzzleResultsPuzzleModeRankPoints[] = { 100, 200, 400, 600, 800, 1000, 2000, 3000,
        4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };

    int rank_index_bias;

    Menu_Results_PulseCubePuzzleClut((Core_Gte_Sin(Menu_Results_elementAnimationState) / 512) + 8);

    Menu_Results_elementAnimationState = (Menu_Results_elementAnimationState + 64) & 0xFFF;

    if (Menu_Results_screenState == 0) {
        int rank = ((Audio_Seq_unusedStateFlags.puzzleTimerMins * 6000)
                    + (Audio_Seq_unusedStateFlags.puzzleTimerSeconds * 100)
                    + Audio_Seq_unusedStateFlags.puzzleTimerHundredths);
        rank -= (Menu_Results_puzzleParTime * 100);

        if (rank <= 0) {
            rank = 15 - Menu_Results_puzzleRankCap;
        } else {
            --rank;
            rank = rank / 200;
            rank_index_bias = 1;
            rank = 15 - (Menu_Results_puzzleRankCap + (rank + rank_index_bias));

            if (rank >= 16) {
                rank = 15;
            }

            if (rank < 0) {
                rank = 0;
            }
        }

        if (Menu_Results_screenTimer == 16) {
            Audio_PlaySfxCentered(0x7E, 0x72);
        }

        if (Menu_Results_screenTimer == 64) {
            Audio_StopSfx(0x7E, 0x72);
            Audio_PlaySfxCentered(0x7E, 0x73);
        }

        if (Menu_Results_screenTimer == 67) {
            Audio_PlaySfxCentered(0x7E, 0x7D);
        }

        if (Menu_Results_screenTimer == 75) {
            Audio_PlaySfxCentered(0x7E, 0x7D);
        }

        if (Menu_Results_screenTimer == 90) {
            Audio_PlaySfxCentered(0x7E, 0x7D);
        }

        Menu_Results_DrawCountingCubePuzzleTime(88, 64, Menu_Results_screenTimer);
        Menu_Results_DrawFoodChain(160, 100, Menu_Results_screenTimer - 75);
        Menu_Results_DrawCubePuzzleRank(160, 124, Menu_Results_screenTimer - 90, rank);
        Menu_Results_DrawPressButtonPrompt(214, 187, Menu_Results_screenTimer - 90, Menu_Results_screenTimer);

        if (Menu_Results_screenTimer < 32767) {
            ++Menu_Results_screenTimer;
        }

        if (Input_buttonsPressed.allButtons & PADRright) {
            if (Menu_Results_screenTimer >= 138) {

                Audio_StopSfx(0x7E, 0);
                Render_RetireTexturePageRange(0x1D, 2, 5);

                Audio_Seq_unusedScoredata.miscScore += Menu_Results_cubePuzzleResultsPuzzleModeRankPoints[rank];

                Menu_Framework_NoOperation();

                return 1;
            }

            Audio_StopSfx(0x7E, 0);

            Menu_Results_screenTimer = 138;
        }
    }

    return 0;
}

// decomp: MENUF.PRG _renderCubePuzzleQuit (src/MENU/MENUF.PRG/3B8.c:2566)
int Menu_Results_UpdateCubePuzzleQuit(void)
{
    Menu_Results_PulseCubePuzzleClut((Core_Gte_Sin(Menu_Results_elementAnimationState) / 512) + 8);

    Menu_Results_elementAnimationState = (Menu_Results_elementAnimationState + 64) & 0xFFF;

    if (Menu_Results_screenState != 0) {
        return 0;
    }

    if (Menu_Results_screenTimer == 0) {
        Audio_PlaySfxCentered(0x7E, 0x7A);
    }

    if (Menu_Results_screenTimer == 15) {
        Audio_PlaySfxCentered(0x7E, 0x7A);
    }

    Menu_Results_DrawGivingUp(64, Menu_Results_screenTimer);
    Menu_Results_DrawChicken(100, Menu_Results_screenTimer - 15);
    Menu_Results_DrawPressButtonPrompt(214, 187, Menu_Results_screenTimer - 15, Menu_Results_screenTimer);

    if (Menu_Results_screenTimer < 0x7FFF) {
        ++Menu_Results_screenTimer;
    }

    if (Input_buttonsPressed.allButtons & PADRright) {
        if (Menu_Results_screenTimer > 62) {

            Audio_StopSfx(0x7E, 0);
            Render_RetireTexturePageRange(0x1D, 1, 5);
            Menu_Framework_NoOperation();

            return 1;
        }

        Menu_Results_screenTimer = 63;

        Audio_StopSfx(0x7E, 0);
    }

    return 0;
}

// decomp: MENUF.PRG _renderGivingUp (src/MENU/MENUF.PRG/3B8.c:2610)
void Menu_Results_DrawGivingUp(int draw_y, int fade_timer)
{
    if (fade_timer < 0) {
        fade_timer = 0;
    }
    if (fade_timer > 64) {
        fade_timer = 64;
    }
    if (fade_timer > 0) {
        Menu_Results_DrawFadingTexture(0, draw_y, Menu_Results_DisIndex_Esc0GivingUp, fade_timer);
        Menu_Results_DrawFadingTexture(160, draw_y, Menu_Results_DisIndex_Esc0GivingUp + 1, fade_timer);
    }
}

// decomp: MENUF.PRG _renderChicken (src/MENU/MENUF.PRG/3B8.c:2624)
void Menu_Results_DrawChicken(int draw_y, int fade_timer)
{
    if (fade_timer < 0) {
        fade_timer = 0;
    }

    if (fade_timer > 64) {
        fade_timer = 64;
    }

    if (fade_timer > 0) {
        Menu_Results_DrawFadingTexture(0, draw_y, Menu_Results_DisIndex_Esc0Chicken, fade_timer);
        Menu_Results_DrawFadingTexture(0xA0, draw_y, Menu_Results_DisIndex_Esc0Chicken + 1, fade_timer);
    }
}

// decomp: MENUF.PRG _renderEvolveOrDie (src/MENU/MENUF.PRG/3B8.c:2640)
void Menu_Results_DrawEvolveOrDie(int x, int y, int step)
{

    if (step < 0) {
        step = 0;
    }

    if (step > 64) {
        step = 64;
    }

    if (step > 0) {
        Menu_Results_evolveOrDieColors[0].cd = step;
        Menu_Results_evolveOrDieColors[1].cd = step;
        x -= (Menu_Results_disMap[Menu_Results_DisIndex_Iq0EvolveOrDie].w + Menu_Results_disMap[Menu_Results_DisIndex_Iq0EvolveOrDie + 1].w)
          >> 1;

        Render_DrawTextureFlashFade(x, y, Menu_Results_DisIndex_Iq0EvolveOrDie, Menu_Results_evolveOrDieColors);
        Render_DrawTextureFlashFade(x + Menu_Results_disMap[Menu_Results_DisIndex_Iq0EvolveOrDie].w, y,
            Menu_Results_DisIndex_Iq0EvolveOrDie + 1, &Menu_Results_evolveOrDieColors[1]);
    }
}

// decomp: MENUF.PRG _renderFoodchain (src/MENU/MENUF.PRG/3B8.c:2664)
void Menu_Results_DrawFoodChain(int x, int y, int timer)
{

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_foodChainColors[0].cd = timer;
        Menu_Results_foodChainColors[1].cd = timer;
        x -= (Menu_Results_disMap[Menu_Results_DisIndex_Iq0Foodchain].w + Menu_Results_disMap[Menu_Results_DisIndex_Iq0Foodchain + 1].w) >> 1;

        Render_DrawTextureFlashFade(x, y, Menu_Results_DisIndex_Iq0Foodchain, Menu_Results_foodChainColors);
        Render_DrawTextureFlashFade(
            x + Menu_Results_disMap[Menu_Results_DisIndex_Iq0Foodchain].w, y, Menu_Results_DisIndex_Iq0Foodchain + 1, &Menu_Results_foodChainColors[1]);
    }
}

// decomp: MENUF.PRG _renderAverageTimeHeader (src/MENU/MENUF.PRG/3B8.c:2687)
void Menu_Results_DrawAverageTimeHeading(int x, int y, int timer)
{
    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_DrawFadingTexture(x - (Menu_Results_disMap[Menu_Results_DisIndex_Iq0AverageTime].w >> 1), y,
            Menu_Results_DisIndex_Iq0AverageTime, timer);
    }
}

// decomp: MENUF.PRG _renderCubePuzzleTime (src/MENU/MENUF.PRG/3B8.c:2699)
void Menu_Results_DrawCubePuzzleParTime(int x, int y, int timer)
{
    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        int seconds_shifted_for_packing = (Menu_Results_puzzleParTime % 60) << 0x10;

        Menu_Results_DrawTime(x, y, timer, ((Menu_Results_puzzleParTime / 60) << 0x10) | (seconds_shifted_for_packing >> 8), 0);
    }
}

// decomp: MENUF.PRG _renderCubePuzzleRank (src/MENU/MENUF.PRG/3B8.c:2712)
void Menu_Results_DrawCubePuzzleRank(int x, int y, int timer, int rank)
{

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_cubePuzzleRankColors[0].cd = timer;

        Render_DrawTextureFlashFade(x - (Menu_Results_disMap[rank + Menu_Results_DisIndex_Iq1GameDesigner].w >> 1), y,
            rank + Menu_Results_DisIndex_Iq1GameDesigner, Menu_Results_cubePuzzleRankColors);
    }
}

// decomp: MENUF.PRG _renderCubePuzzleIncrementalTime (src/MENU/MENUF.PRG/3B8.c:2732)
void Menu_Results_DrawCountingCubePuzzleTime(int x, int y, int timer)
{
    int temp_a3;
    int time;
    int display_minutes;
    int* time_accumulator_address;

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer <= 0) {
        return;
    }

    Menu_Results_DrawFadingTexture(x, y, 72, timer);

    x += Menu_Results_disMap[72].w;

    if (timer >= 16) {
        temp_a3 = Audio_Seq_unusedStateFlags.puzzleTimerSeconds;
        display_minutes = Audio_Seq_unusedStateFlags.puzzleTimerMins;
        temp_a3 = ((display_minutes * 6000) + (temp_a3 * 100))
                + Audio_Seq_unusedStateFlags.puzzleTimerHundredths;
        time = temp_a3;
        time = ((*(time_accumulator_address = &time)) * (timer - 16)) / 48;
        display_minutes = time / 6000;
        time = time % 6000;
        temp_a3 = time / 100;
        time = time % 100;
        time = (display_minutes << 0x10) | (temp_a3 << 8) | time;
    } else {
        time = 0;
    }

    Menu_Results_DrawTime(x, y + 2, timer, time, 0);
}

// MENUF.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Results_CubePuzzle_ResetMenufStatics(void)
{
    {
        static const CVECTOR initial[] = { { 128, 96, 64 }, { 200, 180, 160 }, { 128, 96, 64 } };
        memcpy((void*)&Menu_Results_evolveOrDieColors, &initial, sizeof Menu_Results_evolveOrDieColors);
    }
    {
        static const CVECTOR initial[] = { { 128, 96, 64 }, { 200, 180, 160 }, { 128, 96, 64 } };
        memcpy((void*)&Menu_Results_foodChainColors, &initial, sizeof Menu_Results_foodChainColors);
    }
    {
        static const CVECTOR initial[] = { { 128, 128, 128 }, { 128, 128, 128 } };
        memcpy((void*)&Menu_Results_cubePuzzleRankColors, &initial, sizeof Menu_Results_cubePuzzleRankColors);
    }
}

