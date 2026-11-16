// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/MusicLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Menu/Results/TimeAttackHeadingLive.hpp"
#include "Menu/Results/TimeAttackRatingLive.hpp"
#include "Render/ScreenOverlayLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/TimImageLive.hpp"
#include "State/SceneTimerLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/StatusEffects.h"
#include "Content/DiscLayout.h"
#include "DiscQueue.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/FramebufferEffects.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

// decomp: _disFiles (_disFiles@src/MENU/MENUF.PRG/3B8.c)
Content_DiscFile const Menu_Results_kDisFiles[] = { { VS_RANK_DIS_LBA, VS_RANK_DIS_SIZE },
    { VS_TIME_DIS_LBA, VS_TIME_DIS_SIZE }, { VS_ATTACK_DIS_LBA, VS_ATTACK_DIS_SIZE },
    { VS_IQ_DIS_LBA, VS_IQ_DIS_SIZE }, { VS_ESC_DIS_LBA, VS_ESC_DIS_SIZE } };

// decomp: _newTimeSlot (_newTimeSlot@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_newTimeSlot;

// decomp: _screenState (_screenState@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_screenState;

// decomp: _screenTimer (_screenTimer@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_screenTimer;

// decomp: _timeTrialTime (_timeTrialTime@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_timeTrialTime;

// decomp: local static of _renderTimeAttackStart, at file scope so the module reset reaches it
static CVECTOR Menu_Results_timeAttackStartColors[] = { { 220, 80, 64 }, { 64, 128, 220 }, { 128, 128, 128 } };

// decomp: MENUF.PRG _initTimeAttackEnd (src/MENU/MENUF.PRG/3B8.c:256)
int Menu_Results_InitializeTimeAttackResults(void)
{
    static Content_DiscQueueSlot* Menu_Results_initializeTimeAttackResultsCdSlot;
    static void* Menu_Results_initializeTimeAttackResultsTimData;

    int i;
    int state = Battle_unusedSubmenuState;

    if (state == 0) {

        VS::Render::ReserveLiveTexturePageRange(0x1D, 1, 5);

        Menu_Results_initializeTimeAttackResultsTimData = VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Time].size);
        Menu_Results_initializeTimeAttackResultsCdSlot = VS::Content::AllocateLiveDiscRequest(&Menu_Results_kDisFiles[1]);

        if (Menu_Results_initializeTimeAttackResultsCdSlot != NULL) {
            VS::Content::EnqueueLiveDiscRequest(Menu_Results_initializeTimeAttackResultsCdSlot, Menu_Results_initializeTimeAttackResultsTimData);
        } else {
            Menu_Results_initializeTimeAttackResultsCdSlot = VS::Content::CompleteLiveRefusedDiscRequest(&Menu_Results_kDisFiles[1], Menu_Results_initializeTimeAttackResultsTimData);
        }

        ++Battle_unusedSubmenuState;

    } else if (state == 1) {
        if (Menu_Results_initializeTimeAttackResultsCdSlot->state == Content_DiscRequestState_Loaded) {
            TIM_IMAGE tim;

            VS::Render::ParseLiveTim((u_long *)(Menu_Results_initializeTimeAttackResultsTimData), &tim);

            if (tim.paddr != NULL) {
                tim.prect->x = 832;
                tim.prect->y = 256;
                tim.prect->h = 255;

                LoadImage(tim.prect, tim.paddr);
            }

            if (tim.caddr != NULL) {
                tim.crect->x = 768;
                tim.crect->y = 511;
                tim.crect->w = 160;
                tim.crect->h = 1;
                tim.caddr[0] = 0;

                LoadImage(tim.crect, tim.caddr);
                VS::Core::Memory::CopyLiveAlignedWords(Menu_Results_clutBuf, tim.caddr, sizeof Menu_Results_clutBuf);
            }

            VS::Content::ReleaseLiveDiscRequest(Menu_Results_initializeTimeAttackResultsCdSlot);
            Menu_Results_QueueResultsMusic();

            ++Battle_unusedSubmenuState;
        }
    } else if (VS::Audio::LivePollMusicLoads() == 0) {

        VS::Audio::LivePlayMusicSlot(2, 0x7F, 0);

        Menu_Results_timeTrialTime = (Audio_Seq_unusedStateFlags.timeAttackMins << 0x10)
                       | (Audio_Seq_unusedStateFlags.timeAttackSecs << 8)
                       | Audio_Seq_unusedStateFlags.timeAttackHundredths;

        if (Menu_Results_timeTrialTime == 0) {
            Menu_Results_timeTrialTime = (59 << 16) | (59 << 8) | 99;
        }

        for (i = 0; i < 3; ++i) {
            int record;

            if ((Audio_Seq_unusedScoredata
                        .bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][i]
                        .time)
                < Menu_Results_timeTrialTime) {
                continue;
            }

            for (record = 2; i < record; --record) {
                Audio_Seq_unusedScoredata
                    .bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][record]
                    .time = Audio_Seq_unusedScoredata
                                .bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss]
                                                    [record - 1]
                                .time;
                Audio_Seq_unusedScoredata
                    .bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][record]
                    .round = Audio_Seq_unusedScoredata
                                 .bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss]
                                                     [record - 1]
                                 .round;
                Audio_Seq_unusedScoredata
                    .bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][record]
                    .difficulty =
                    Audio_Seq_unusedScoredata
                        .bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss]
                                            [record - 1]
                        .difficulty;
            }

            Audio_Seq_unusedScoredata.bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][i]
                .time = Menu_Results_timeTrialTime;
            Audio_Seq_unusedScoredata.bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][i]
                .round = Audio_Seq_unusedStateFlags.clearCount;
            Audio_Seq_unusedScoredata.bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][i]
                .difficulty = Audio_Seq_unusedStateFlags.difficulty;

            break;
        }

        Menu_Results_newTimeSlot = i;
        Menu_Results_screenTimer = 0;
        Menu_Results_screenState = 0;
        Menu_Results_elementAnimationState = 0;
        Battle_unusedSubmenuState = 0;

        VS::Core::Memory::FreeLiveFromHighEnd(Menu_Results_initializeTimeAttackResultsTimData);

        return 1;
    }
    return 0;
}

// decomp: MENUF.PRG _initTimeAttackStart (src/MENU/MENUF.PRG/3B8.c:372)
int Menu_Results_InitializeTimeAttackStart(void)
{
    static Content_DiscQueueSlot* Menu_Results_initializeTimeAttackStartCdSlot;
    static void* Menu_Results_initializeTimeAttackStartTimData;

    if (Battle_unusedSubmenuState == 0) {

        VS::Render::ReserveLiveTexturePageRange(0x1D, 2, 5);

        Menu_Results_initializeTimeAttackStartTimData = VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Attack].size);
        Menu_Results_initializeTimeAttackStartCdSlot = VS::Content::AllocateLiveDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Attack]);

        if (Menu_Results_initializeTimeAttackStartCdSlot != NULL) {
            VS::Content::EnqueueLiveDiscRequest(Menu_Results_initializeTimeAttackStartCdSlot, Menu_Results_initializeTimeAttackStartTimData);
        } else {
            Menu_Results_initializeTimeAttackStartCdSlot = VS::Content::CompleteLiveRefusedDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Attack], Menu_Results_initializeTimeAttackStartTimData);
        }

        ++Battle_unusedSubmenuState;
    } else if (Battle_unusedSubmenuState == 1) {
        if (Menu_Results_initializeTimeAttackStartCdSlot->state == Content_DiscRequestState_Loaded) {
            int i;
            TIM_IMAGE tim;

            for (i = 0; i < 2; ++i) {
                VS::Render::ParseLiveTim((u_long *)(((u_char*)(Menu_Results_initializeTimeAttackStartTimData)) + i * 0x8220), &tim);

                if (tim.paddr != NULL) {
                    tim.prect->x = 832 + i * 64;
                    tim.prect->y = 256;
                    tim.prect->h = 255;

                    LoadImage(tim.prect, tim.paddr);
                }

                if (i == 0) {
                    if (tim.caddr != NULL) {
                        tim.crect->x = 768;
                        tim.crect->y = 511;
                        tim.crect->w = 160;
                        tim.crect->h = 1;
                        tim.caddr[0] = 0;

                        LoadImage(tim.crect, tim.caddr);
                    }
                }
            }

            VS::Content::ReleaseLiveDiscRequest(Menu_Results_initializeTimeAttackStartCdSlot);

            ++Battle_unusedSubmenuState;
        }
    } else {
        VS::Core::Memory::FreeLiveFromHighEnd(Menu_Results_initializeTimeAttackStartTimData);

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

// decomp: MENUF.PRG _renderTimeAttackEnd (src/MENU/MENUF.PRG/3B8.c:905)
int Menu_Results_UpdateTimeAttackResults(void)
{
    int palette_pulse_sine = VS::Core::Math::Sine(Menu_Results_elementAnimationState);

    VS::Menu::Results::LivePulseTimeAttackClut(ABS(palette_pulse_sine) / 256);

    Menu_Results_elementAnimationState = (Menu_Results_elementAnimationState + 64) & 0xFFF;

    if (Menu_Results_screenState == 0) {
        if (Menu_Results_screenTimer == 0x30) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x72);
        }

        if (Menu_Results_screenTimer == 0x60) {
            VS::Audio::LiveStopSfx(0x7E, 0x72);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x73);
        }

        VS::Menu::Results::LiveDrawTimeAttackResultsHeading(160, 52, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawCountingTimeAttackTime(16, 164, Menu_Results_screenTimer - 32);
        VS::Menu::Results::LiveDrawTimeAttackRatingWipe(16, 144, Menu_Results_screenTimer - 96);

        if (Menu_Results_newTimeSlot == 0) {
            VS::Menu::Results::LiveDrawNewRecord(70, 184, Menu_Results_screenTimer - 128);
        }

        if ((Menu_Results_screenTimer == 128) && (Menu_Results_newTimeSlot == 0)) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x76);
        }

        if (Menu_Results_screenTimer == 160) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x73);
        }

        if (Menu_Results_screenTimer == 168) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x73);
        }

        if (Menu_Results_screenTimer == 176) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x73);
        }

        VS::Menu::Results::LiveDrawLeaderboardEntry(304, 144, 0, Menu_Results_screenTimer - 160);
        VS::Menu::Results::LiveDrawLeaderboardEntry(304, 144, 1, Menu_Results_screenTimer - 168);
        VS::Menu::Results::LiveDrawLeaderboardEntry(304, 144, 2, Menu_Results_screenTimer - 176);

        ++Menu_Results_screenTimer;

        if ((Input_buttonsPressed.allButtons & PADRright) || (Menu_Results_screenTimer >= 192)) {

            VS::Audio::LiveStopSfx(0x7E, 0);

            Menu_Results_screenTimer = 0;
            ++Menu_Results_screenState;
        }
    } else if (Menu_Results_screenState == 1) {

        VS::Menu::Results::LiveDrawTimeAttackResultsHeading(160, 52, 64);
        VS::Menu::Results::LiveDrawTimeAttackTime(16, 164, 64);
        VS::Menu::Results::LiveDrawTimeAttackRating(16, 144, 64);

        if (Menu_Results_newTimeSlot == 0) {
            VS::Menu::Results::LiveDrawNewRecord(70, 184, 64);
        }

        VS::Menu::Results::LiveDrawLeaderboard(304, 144, 64);

        ++Menu_Results_screenTimer;

        if ((Input_buttonsPressed.allButtons & PADRright) || (Menu_Results_screenTimer >= 450)) {

            if (Input_buttonsPressed.allButtons & PADRright) {
                VS::Audio::LivePlaySfxCentered(0x7E, 5);
            }

            ++Menu_Results_screenState;
            Menu_Results_screenTimer = 7;
        }
    } else if (Menu_Results_screenState == 2) {

        VS::Menu::Results::LiveDrawTimeAttackResultsHeading(160, 52, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawTimeAttackTime(16, 164, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawTimeAttackRating(16, 144, Menu_Results_screenTimer);

        if (Menu_Results_newTimeSlot == 0) {
            VS::Menu::Results::LiveDrawNewRecord(70, 184, Menu_Results_screenTimer);
        }

        VS::Menu::Results::LiveDrawLeaderboard(304, 144, Menu_Results_screenTimer);

        if (Menu_Results_screenTimer > 0) {
            --Menu_Results_screenTimer;
        } else {
            ++Menu_Results_screenState;
        }
    } else {
        VS::Render::RetireLiveTexturePageRange(29, 1, 5);

        return 1;
    }
    return 0;
}

// decomp: MENUF.PRG _renderTimeAttackStart (src/MENU/MENUF.PRG/3B8.c:1014)
int Menu_Results_UpdateTimeAttackStart(void)
{
    static int Menu_Results_timeAttackStartCountdownOffsets[] = { 121, 93, 67, 43, 25, 12, 3, 0 };
    static Render_FramebufferEffectTranslation Menu_Results_kFramebufferEffectParams[3] = { { 0x1029, 0x1029, 0x1000 },
        { 0x107A, 0x107A, 0x1000 } };

    int countdown_interval;
    int x;

    if (Menu_Results_screenState == 0) {
        if ((Menu_Results_screenTimer == ((Menu_Results_screenTimer / 15) * 0xF)) && (Menu_Results_screenTimer < 0x2E)) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x7A);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x7B);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x7C);
        }

        VS::Menu::Results::LiveDrawTimeAttackHeading(160, 48, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawCourseNumber(160, 92, Menu_Results_screenTimer - 15);
        VS::Menu::Results::LiveDrawBossName(160, 120, Menu_Results_screenTimer - 30);
        VS::Menu::Results::LiveDrawBestTime(160, 156, Menu_Results_screenTimer - 45);

        ++Menu_Results_screenTimer;

        if (Input_buttonsPressed.allButtons & PADRright) {
            Menu_Results_screenState = 2;
            Menu_Results_screenTimer = 0;
        } else if (Menu_Results_screenTimer >= 90) {
            Menu_Results_screenState = 1;
            Menu_Results_screenTimer = 8;
        }
    } else if (Menu_Results_screenState == 1) {
        --Menu_Results_screenTimer;

        VS::Menu::Results::LiveDrawTimeAttackHeading(160, 48, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawCourseNumber(160, 92, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawBossName(160, 120, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawBestTime(160, 156, Menu_Results_screenTimer);

        if ((Input_buttonsPressed.allButtons & PADRright) || (Menu_Results_screenTimer == 0)) {
            Menu_Results_screenState = 2;
            Menu_Results_screenTimer = 0;
        }

    } else if (Menu_Results_screenState == 2) {
        VS::State::SetLiveTickSpeed(4);
        VS::Render::SetLiveFramebufferEffectRotation(0);
        VS::Render::SetLiveFramebufferEffectScale(&Menu_Results_kFramebufferEffectParams[0]);
        VS::Render::SetLiveFramebufferEffectTint(&Menu_Results_timeAttackStartColors[1]);
        VS::Render::SetLiveFramebufferEffectOffset((Render_FramebufferEffectTranslation*)(Menu_Results_kFramebufferEffectParams + 2));
        VS::Render::SetLiveFramebufferEffectBlendMode(0);
        VS::Render::SetLiveFramebufferEffectDepth(0U);
        VS::Render::SetLiveFramebufferPreviousFrame(0);
        VS::Render::SetLiveFramebufferEffectEnabled(1);

        ++Menu_Results_screenState;

    } else if (Menu_Results_screenState == 3) {

        countdown_interval = Menu_Results_screenTimer / 15;

        if (Menu_Results_screenTimer == (countdown_interval * 0xF)) {

            switch (countdown_interval) {
            case 0:
            case 1:
            case 2:
                VS::Audio::LivePlaySfxCentered(0x7E, 0x7D);
                VS::Audio::LivePlaySfxCentered(0x7E, 0x7E);
                VS::Audio::LivePlaySfxCentered(0x7E, 0x7F);
                break;

            case 3:
                VS::Audio::LivePlaySfxCentered(0x7E, 0x80);
                VS::Audio::LivePlaySfxCentered(0x7E, 0x81);
                VS::Audio::LivePlaySfxCentered(0x7E, 0x82);
                break;
            }
        }

        if (Menu_Results_screenTimer >= 45) {

            if (Menu_Results_screenTimer == 45) {
                VS::Render::SetLiveFramebufferEffectBlendMode(1);
                VS::Render::SetLiveFramebufferEffectScale(&Menu_Results_kFramebufferEffectParams[1]);
                VS::Render::SetLiveFramebufferEffectTint(&Menu_Results_timeAttackStartColors[0]);
            } else if (Menu_Results_screenTimer == 0x2E) {
                VS::Render::SetLiveFramebufferEffectBlendMode(0);
            } else if (Menu_Results_screenTimer == 0x37) {
                VS::Render::SetLiveFramebufferEffectTint(&Menu_Results_timeAttackStartColors[2]);
            } else if (Menu_Results_screenTimer == 0x3C) {
                VS::Render::SetLiveFramebufferEffectScale(&Menu_Results_kFramebufferEffectParams[0]);
            }

            x = 104 - Menu_Results_screenTimer;

            if (x < 45) {
                x = 45;
            }

            VS::Menu::Results::LiveDrawStartCountdown(160, 120, x);

        } else {
            x = Menu_Results_screenTimer % 15;

            if (x > 7) {
                x = 7;
            }

            x = Menu_Results_timeAttackStartCountdownOffsets[x];

            VS::Menu::Results::LiveDrawStartCountdown(x + 160, 120, Menu_Results_screenTimer);
        }

        ++Menu_Results_screenTimer;

        if (Menu_Results_screenTimer >= 62) {
            ++Menu_Results_screenState;
        }

    } else if (Menu_Results_screenState == 4) {
        VS::State::SetLiveTickSpeed(2);
        VS::Render::SetLiveFramebufferEffectScale(&Menu_Results_kFramebufferEffectParams[2]);
        VS::Render::SetLiveFramebufferEffectEnabled(0);
        VS::Render::RetireLiveTexturePageRange(0x1D, 2, 5);

        return 1;
    }

    return 0;
}

// MENUF.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Results_TimeAttack_ResetMenufStatics(void)
{
    {
        static const CVECTOR initial[] = { { 220, 80, 64 }, { 64, 128, 220 }, { 128, 128, 128 } };
        memcpy((void*)&Menu_Results_timeAttackStartColors, &initial, sizeof Menu_Results_timeAttackStartColors);
    }
}

}  // extern "C"
