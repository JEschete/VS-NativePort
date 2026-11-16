// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/SfxLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Results/CubePuzzleTextLive.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/TimImageLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/StatusEffects.h"
#include "DiscQueue.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/Zone.h"
#include "Input/ControllerLiveState.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

// decomp: _clutBuf (_clutBuf@src/MENU/MENUF.PRG/3B8.c)
u_short Menu_Results_clutBuf[160];

// decomp: _puzzleParTime (_puzzleParTime@src/MENU/MENUF.PRG/3B8.c)
short Menu_Results_puzzleParTime;

// decomp: _puzzleRankCap (_puzzleRankCap@src/MENU/MENUF.PRG/3B8.c)
short Menu_Results_puzzleRankCap;

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

        VS::Render::ReserveLiveTexturePageRange(0x1D, 2, 5);

        Menu_Results_initializeCubePuzzleStartIqDisData = VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Iq].size);
        Menu_Results_initializeCubePuzzleStartIqDisCdSlot = VS::Content::AllocateLiveDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Iq]);

        if (Menu_Results_initializeCubePuzzleStartIqDisCdSlot != NULL) {
            VS::Content::EnqueueLiveDiscRequest(Menu_Results_initializeCubePuzzleStartIqDisCdSlot, Menu_Results_initializeCubePuzzleStartIqDisData);
        } else {
            Menu_Results_initializeCubePuzzleStartIqDisCdSlot = VS::Content::CompleteLiveRefusedDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Iq], Menu_Results_initializeCubePuzzleStartIqDisData);
        }

        ++Battle_unusedSubmenuState;

    } else if (Battle_unusedSubmenuState == 1) {
        if (Menu_Results_initializeCubePuzzleStartIqDisCdSlot->state == Content_DiscRequestState_Loaded) {
            int i;
            TIM_IMAGE tim;
            for (i = 0; i < 2; ++i) {

                VS::Render::ParseLiveTim((u_long *)(((u_char*)(Menu_Results_initializeCubePuzzleStartIqDisData)) + i * 0x8220), &tim);

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
                        VS::Core::Memory::CopyLiveAlignedWords(Menu_Results_clutBuf, tim.caddr, sizeof Menu_Results_clutBuf);
                    }
                }
            }

            Menu_Results_puzzleParTime = 600;
            Menu_Results_puzzleRankCap = 1;
            room = Field_roomData.minimapScene->rooms;
            puzzleData = (decltype(puzzleData))(((u_char*)(Menu_Results_initializeCubePuzzleStartIqDisData)) + 0x10440);

            for (i = 0; i < 64; ++i, ++puzzleData) {
                if ((puzzleData->zoneId == room->zoneId)
                    && (puzzleData->mapId == room->mapId)) {
                    Menu_Results_puzzleParTime = puzzleData->par;
                    Menu_Results_puzzleRankCap = puzzleData->rankCap;
                    break;
                }
            }

            VS::Content::ReleaseLiveDiscRequest(Menu_Results_initializeCubePuzzleStartIqDisCdSlot);

            ++Battle_unusedSubmenuState;
        }
    } else {
        VS::Core::Memory::FreeLiveFromHighEnd(Menu_Results_initializeCubePuzzleStartIqDisData);

        Menu_Results_onScreenMapCompletion = 0;
        Menu_Results_onScreenScore = 0;
        Menu_Results_screenTimer = 0;
        Menu_Results_screenState = 0;
        Menu_Results_elementAnimationState = 0;
        Menu_Results_buffReelSelection = 0;
        Battle_unusedSubmenuState = 0;

        VS::Menu::Framework::NoOperation();
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

        VS::Render::ReserveLiveTexturePageRange(0x1D, 1, 5);

        Menu_Results_initializeCubePuzzleQuitEscDisData = VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Esc].size);
        Menu_Results_initializeCubePuzzleQuitEscDisCdSLot = VS::Content::AllocateLiveDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Esc]);

        if (Menu_Results_initializeCubePuzzleQuitEscDisCdSLot != NULL) {
            VS::Content::EnqueueLiveDiscRequest(Menu_Results_initializeCubePuzzleQuitEscDisCdSLot, Menu_Results_initializeCubePuzzleQuitEscDisData);
        } else {
            Menu_Results_initializeCubePuzzleQuitEscDisCdSLot = VS::Content::CompleteLiveRefusedDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Esc], Menu_Results_initializeCubePuzzleQuitEscDisData);
        }

        ++Battle_unusedSubmenuState;

    } else if (Battle_unusedSubmenuState == 1) {
        if (Menu_Results_initializeCubePuzzleQuitEscDisCdSLot->state == Content_DiscRequestState_Loaded) {
            for (i = 0; i <= 0; ++i) {

                VS::Render::ParseLiveTim((u_long *)(((u_char*)(Menu_Results_initializeCubePuzzleQuitEscDisData)) + i * 0x8220), &tim);

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
                    VS::Core::Memory::CopyLiveAlignedWords(Menu_Results_clutBuf, tim.caddr, sizeof Menu_Results_clutBuf);
                }
            }

            VS::Content::ReleaseLiveDiscRequest(Menu_Results_initializeCubePuzzleQuitEscDisCdSLot);

            ++Battle_unusedSubmenuState;
        }
    } else {
        VS::Core::Memory::FreeLiveFromHighEnd(Menu_Results_initializeCubePuzzleQuitEscDisData);

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
        VS::Audio::LivePlaySfxCentered(0x7E, 0x7D);
    }

    if (Menu_Results_screenTimer == 30) {
        VS::Audio::LivePlaySfxCentered(0x7E, 0x7D);
    }

    VS::Menu::Results::LiveDrawEvolveOrDie(160, 64, Menu_Results_screenTimer);
    VS::Menu::Results::LiveDrawAverageTimeHeading(128, 116, Menu_Results_screenTimer - 15);
    VS::Menu::Results::LiveDrawCubePuzzleParTime(192, 144, Menu_Results_screenTimer - 30);
    VS::Menu::Results::LiveDrawPressButtonPrompt(214, 187, Menu_Results_screenTimer - 30, Menu_Results_screenTimer);

    if (Menu_Results_screenTimer < 32767) {
        ++Menu_Results_screenTimer;
    }

    if (Input_buttonsPressed.allButtons & PADRright) {
        if (Menu_Results_screenTimer >= 78) {

            VS::Audio::LiveStopSfx(126, 0);
            VS::Render::RetireLiveTexturePageRange(29, 2, 5);

            return 1;
        }

        VS::Audio::LiveStopSfx(126, 0);

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

    Menu_Results_PulseCubePuzzleClut((VS::Core::Math::Sine(Menu_Results_elementAnimationState) / 512) + 8);

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
            VS::Audio::LivePlaySfxCentered(0x7E, 0x72);
        }

        if (Menu_Results_screenTimer == 64) {
            VS::Audio::LiveStopSfx(0x7E, 0x72);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x73);
        }

        if (Menu_Results_screenTimer == 67) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x7D);
        }

        if (Menu_Results_screenTimer == 75) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x7D);
        }

        if (Menu_Results_screenTimer == 90) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x7D);
        }

        VS::Menu::Results::LiveDrawCountingCubePuzzleTime(88, 64, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawFoodChain(160, 100, Menu_Results_screenTimer - 75);
        VS::Menu::Results::LiveDrawCubePuzzleRank(160, 124, Menu_Results_screenTimer - 90, rank);
        VS::Menu::Results::LiveDrawPressButtonPrompt(214, 187, Menu_Results_screenTimer - 90, Menu_Results_screenTimer);

        if (Menu_Results_screenTimer < 32767) {
            ++Menu_Results_screenTimer;
        }

        if (Input_buttonsPressed.allButtons & PADRright) {
            if (Menu_Results_screenTimer >= 138) {

                VS::Audio::LiveStopSfx(0x7E, 0);
                VS::Render::RetireLiveTexturePageRange(0x1D, 2, 5);

                Audio_Seq_unusedScoredata.miscScore += Menu_Results_cubePuzzleResultsPuzzleModeRankPoints[rank];

                VS::Menu::Framework::NoOperation();

                return 1;
            }

            VS::Audio::LiveStopSfx(0x7E, 0);

            Menu_Results_screenTimer = 138;
        }
    }

    return 0;
}

// decomp: MENUF.PRG _renderCubePuzzleQuit (src/MENU/MENUF.PRG/3B8.c:2566)
int Menu_Results_UpdateCubePuzzleQuit(void)
{
    Menu_Results_PulseCubePuzzleClut((VS::Core::Math::Sine(Menu_Results_elementAnimationState) / 512) + 8);

    Menu_Results_elementAnimationState = (Menu_Results_elementAnimationState + 64) & 0xFFF;

    if (Menu_Results_screenState != 0) {
        return 0;
    }

    if (Menu_Results_screenTimer == 0) {
        VS::Audio::LivePlaySfxCentered(0x7E, 0x7A);
    }

    if (Menu_Results_screenTimer == 15) {
        VS::Audio::LivePlaySfxCentered(0x7E, 0x7A);
    }

    VS::Menu::Results::LiveDrawGivingUp(64, Menu_Results_screenTimer);
    VS::Menu::Results::LiveDrawChicken(100, Menu_Results_screenTimer - 15);
    VS::Menu::Results::LiveDrawPressButtonPrompt(214, 187, Menu_Results_screenTimer - 15, Menu_Results_screenTimer);

    if (Menu_Results_screenTimer < 0x7FFF) {
        ++Menu_Results_screenTimer;
    }

    if (Input_buttonsPressed.allButtons & PADRright) {
        if (Menu_Results_screenTimer > 62) {

            VS::Audio::LiveStopSfx(0x7E, 0);
            VS::Render::RetireLiveTexturePageRange(0x1D, 1, 5);
            VS::Menu::Framework::NoOperation();

            return 1;
        }

        Menu_Results_screenTimer = 63;

        VS::Audio::LiveStopSfx(0x7E, 0);
    }

    return 0;
}

}  // extern "C"
