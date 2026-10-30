// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Music.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/StatusEffects.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/FramebufferEffects.h"
#include "Render/Images.h"
#include "Render/TextureRegionDraw.h"
#include "Render/Textures.h"
#include "State/Progress.h"
#include "State/StateFlags.h"
#include "State/Timers.h"

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

// decomp: local static of _renderTimeAttackHeader, at file scope so the module reset reaches it
static CVECTOR Menu_Results_timeAttackHeadingColors[] = { { 128, 96, 64 }, { 200, 180, 160 }, { 128, 96, 64 } };

// decomp: local static of _renderTimeAttackRatingWipe, at file scope so the module reset reaches it
static CVECTOR Menu_Results_timeAttackRatingWipeColors[] = { { 128, 128, 128 }, { 128, 128, 128 } };

// decomp: local static of _renderTimeAttackResultsHeader, at file scope so the module reset reaches it
static CVECTOR Menu_Results_timeAttackResultsHeadingColors[] = { { 128, 128, 128 }, { 128, 128, 128 } };

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

        Render_ReserveTexturePageRange(0x1D, 1, 5);

        Menu_Results_initializeTimeAttackResultsTimData = Core_Memory_AllocateFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Time].size);
        Menu_Results_initializeTimeAttackResultsCdSlot = Content_AllocateDiscRequest(&Menu_Results_kDisFiles[1]);

        Content_EnqueueDiscRequest(Menu_Results_initializeTimeAttackResultsCdSlot, Menu_Results_initializeTimeAttackResultsTimData);

        ++Battle_unusedSubmenuState;

    } else if (state == 1) {
        if (Menu_Results_initializeTimeAttackResultsCdSlot->state == Content_DiscRequestState_Loaded) {
            TIM_IMAGE tim;

            Render_ParseTextureImage(Menu_Results_initializeTimeAttackResultsTimData, &tim);

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
                Core_Memory_CopyAlignedWords(Menu_Results_clutBuf, tim.caddr, sizeof Menu_Results_clutBuf);
            }

            Content_ReleaseDiscRequest(Menu_Results_initializeTimeAttackResultsCdSlot);
            Menu_Results_QueueResultsMusic();

            ++Battle_unusedSubmenuState;
        }
    } else if (Audio_PollMusicLoads() == 0) {

        Audio_PlayMusicSlot(2, 0x7F, 0);

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

        Core_Memory_FreeFromHighEnd(Menu_Results_initializeTimeAttackResultsTimData);

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

        Render_ReserveTexturePageRange(0x1D, 2, 5);

        Menu_Results_initializeTimeAttackStartTimData = Core_Memory_AllocateFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Attack].size);
        Menu_Results_initializeTimeAttackStartCdSlot = Content_AllocateDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Attack]);

        Content_EnqueueDiscRequest(Menu_Results_initializeTimeAttackStartCdSlot, Menu_Results_initializeTimeAttackStartTimData);

        ++Battle_unusedSubmenuState;
    } else if (Battle_unusedSubmenuState == 1) {
        if (Menu_Results_initializeTimeAttackStartCdSlot->state == Content_DiscRequestState_Loaded) {
            int i;
            TIM_IMAGE tim;

            for (i = 0; i < 2; ++i) {
                Render_ParseTextureImage(Menu_Results_initializeTimeAttackStartTimData + i * 0x8220, &tim);

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

            Content_ReleaseDiscRequest(Menu_Results_initializeTimeAttackStartCdSlot);

            ++Battle_unusedSubmenuState;
        }
    } else {
        Core_Memory_FreeFromHighEnd(Menu_Results_initializeTimeAttackStartTimData);

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
    int palette_pulse_sine = Core_Gte_Sin(Menu_Results_elementAnimationState);

    Menu_Results_PulseTimeAttackClut(ABS(palette_pulse_sine) / 256);

    Menu_Results_elementAnimationState = (Menu_Results_elementAnimationState + 64) & 0xFFF;

    if (Menu_Results_screenState == 0) {
        if (Menu_Results_screenTimer == 0x30) {
            Audio_PlaySfxCentered(0x7E, 0x72);
        }

        if (Menu_Results_screenTimer == 0x60) {
            Audio_StopSfx(0x7E, 0x72);
            Audio_PlaySfxCentered(0x7E, 0x73);
        }

        Menu_Results_DrawTimeAttackResultsHeading(160, 52, Menu_Results_screenTimer);
        Menu_Results_DrawCountingTimeAttackTime(16, 164, Menu_Results_screenTimer - 32);
        Menu_Results_DrawTimeAttackRatingWipe(16, 144, Menu_Results_screenTimer - 96);

        if (Menu_Results_newTimeSlot == 0) {
            Menu_Results_DrawNewRecord(70, 184, Menu_Results_screenTimer - 128);
        }

        if ((Menu_Results_screenTimer == 128) && (Menu_Results_newTimeSlot == 0)) {
            Audio_PlaySfxCentered(0x7E, 0x76);
        }

        if (Menu_Results_screenTimer == 160) {
            Audio_PlaySfxCentered(0x7E, 0x73);
        }

        if (Menu_Results_screenTimer == 168) {
            Audio_PlaySfxCentered(0x7E, 0x73);
        }

        if (Menu_Results_screenTimer == 176) {
            Audio_PlaySfxCentered(0x7E, 0x73);
        }

        Menu_Results_DrawLeaderboardEntry(304, 144, 0, Menu_Results_screenTimer - 160);
        Menu_Results_DrawLeaderboardEntry(304, 144, 1, Menu_Results_screenTimer - 168);
        Menu_Results_DrawLeaderboardEntry(304, 144, 2, Menu_Results_screenTimer - 176);

        ++Menu_Results_screenTimer;

        if ((Input_buttonsPressed.allButtons & PADRright) || (Menu_Results_screenTimer >= 192)) {

            Audio_StopSfx(0x7E, 0);

            Menu_Results_screenTimer = 0;
            ++Menu_Results_screenState;
        }
    } else if (Menu_Results_screenState == 1) {

        Menu_Results_DrawTimeAttackResultsHeading(160, 52, 64);
        Menu_Results_DrawTimeAttackTime(16, 164, 64);
        Menu_Results_DrawTimeAttackRating(16, 144, 64);

        if (Menu_Results_newTimeSlot == 0) {
            Menu_Results_DrawNewRecord(70, 184, 64);
        }

        Menu_Results_DrawLeaderboard(304, 144, 64);

        ++Menu_Results_screenTimer;

        if ((Input_buttonsPressed.allButtons & PADRright) || (Menu_Results_screenTimer >= 450)) {

            if (Input_buttonsPressed.allButtons & PADRright) {
                Audio_PlaySfxCentered(0x7E, 5);
            }

            ++Menu_Results_screenState;
            Menu_Results_screenTimer = 7;
        }
    } else if (Menu_Results_screenState == 2) {

        Menu_Results_DrawTimeAttackResultsHeading(160, 52, Menu_Results_screenTimer);
        Menu_Results_DrawTimeAttackTime(16, 164, Menu_Results_screenTimer);
        Menu_Results_DrawTimeAttackRating(16, 144, Menu_Results_screenTimer);

        if (Menu_Results_newTimeSlot == 0) {
            Menu_Results_DrawNewRecord(70, 184, Menu_Results_screenTimer);
        }

        Menu_Results_DrawLeaderboard(304, 144, Menu_Results_screenTimer);

        if (Menu_Results_screenTimer > 0) {
            --Menu_Results_screenTimer;
        } else {
            ++Menu_Results_screenState;
        }
    } else {
        Render_RetireTexturePageRange(29, 1, 5);

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
            Audio_PlaySfxCentered(0x7E, 0x7A);
            Audio_PlaySfxCentered(0x7E, 0x7B);
            Audio_PlaySfxCentered(0x7E, 0x7C);
        }

        Menu_Results_DrawTimeAttackHeading(160, 48, Menu_Results_screenTimer);
        Menu_Results_DrawCourseNumber(160, 92, Menu_Results_screenTimer - 15);
        Menu_Results_DrawBossName(160, 120, Menu_Results_screenTimer - 30);
        Menu_Results_DrawBestTime(160, 156, Menu_Results_screenTimer - 45);

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

        Menu_Results_DrawTimeAttackHeading(160, 48, Menu_Results_screenTimer);
        Menu_Results_DrawCourseNumber(160, 92, Menu_Results_screenTimer);
        Menu_Results_DrawBossName(160, 120, Menu_Results_screenTimer);
        Menu_Results_DrawBestTime(160, 156, Menu_Results_screenTimer);

        if ((Input_buttonsPressed.allButtons & PADRright) || (Menu_Results_screenTimer == 0)) {
            Menu_Results_screenState = 2;
            Menu_Results_screenTimer = 0;
        }

    } else if (Menu_Results_screenState == 2) {
        State_SetTickSpeed(4);
        Render_SetFramebufferEffectRotation(0);
        Render_SetFramebufferEffectScale(&Menu_Results_kFramebufferEffectParams[0]);
        Render_SetFramebufferEffectTint(&Menu_Results_timeAttackStartColors[1]);
        Render_SetFramebufferEffectOffset((Render_FramebufferEffectTranslation*)(Menu_Results_kFramebufferEffectParams + 2));
        Render_SetFramebufferEffectBlendMode(0);
        Render_SetFramebufferEffectDepth(0U);
        Render_SetFramebufferPreviousFrame(0);
        Render_SetFramebufferEffectEnabled(1);

        ++Menu_Results_screenState;

    } else if (Menu_Results_screenState == 3) {

        countdown_interval = Menu_Results_screenTimer / 15;

        if (Menu_Results_screenTimer == (countdown_interval * 0xF)) {

            switch (countdown_interval) {
            case 0:
            case 1:
            case 2:
                Audio_PlaySfxCentered(0x7E, 0x7D);
                Audio_PlaySfxCentered(0x7E, 0x7E);
                Audio_PlaySfxCentered(0x7E, 0x7F);
                break;

            case 3:
                Audio_PlaySfxCentered(0x7E, 0x80);
                Audio_PlaySfxCentered(0x7E, 0x81);
                Audio_PlaySfxCentered(0x7E, 0x82);
                break;
            }
        }

        if (Menu_Results_screenTimer >= 45) {

            if (Menu_Results_screenTimer == 45) {
                Render_SetFramebufferEffectBlendMode(1);
                Render_SetFramebufferEffectScale(&Menu_Results_kFramebufferEffectParams[1]);
                Render_SetFramebufferEffectTint(&Menu_Results_timeAttackStartColors[0]);
            } else if (Menu_Results_screenTimer == 0x2E) {
                Render_SetFramebufferEffectBlendMode(0);
            } else if (Menu_Results_screenTimer == 0x37) {
                Render_SetFramebufferEffectTint(&Menu_Results_timeAttackStartColors[2]);
            } else if (Menu_Results_screenTimer == 0x3C) {
                Render_SetFramebufferEffectScale(&Menu_Results_kFramebufferEffectParams[0]);
            }

            x = 104 - Menu_Results_screenTimer;

            if (x < 45) {
                x = 45;
            }

            Menu_Results_DrawStartCountdown(160, 120, x);

        } else {
            x = Menu_Results_screenTimer % 15;

            if (x > 7) {
                x = 7;
            }

            x = Menu_Results_timeAttackStartCountdownOffsets[x];

            Menu_Results_DrawStartCountdown(x + 160, 120, Menu_Results_screenTimer);
        }

        ++Menu_Results_screenTimer;

        if (Menu_Results_screenTimer >= 62) {
            ++Menu_Results_screenState;
        }

    } else if (Menu_Results_screenState == 4) {
        State_SetTickSpeed(2);
        Render_SetFramebufferEffectScale(&Menu_Results_kFramebufferEffectParams[2]);
        Render_SetFramebufferEffectEnabled(0);
        Render_RetireTexturePageRange(0x1D, 2, 5);

        return 1;
    }

    return 0;
}

// decomp: MENUF.PRG _renderTimeAttackHeader (src/MENU/MENUF.PRG/3B8.c:1146)
void Menu_Results_DrawTimeAttackHeading(int x, int y, int timer)
{

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 0x40) {
        timer = 0x40;
    }

    if (timer > 0) {

        Menu_Results_timeAttackHeadingColors[0].cd = timer;
        Menu_Results_timeAttackHeadingColors[1].cd = timer;
        x -= (Menu_Results_disMap[Menu_Results_DisIndex_Attack0TimeAttack].w
                 + Menu_Results_disMap[Menu_Results_DisIndex_Attack0TimeAttack + 1].w)
          >> 1;

        Render_DrawTextureFlashFade(Menu_Results_disMap, x, y, 100, Menu_Results_timeAttackHeadingColors);
        Render_DrawTextureFlashFade(Menu_Results_disMap, x + Menu_Results_disMap[Menu_Results_DisIndex_Attack0TimeAttack].w, y,
            Menu_Results_DisIndex_Attack0TimeAttack + 1, &Menu_Results_timeAttackHeadingColors[1]);
    }
}

// decomp: MENUF.PRG _renderCourse (src/MENU/MENUF.PRG/3B8.c:1175)
void Menu_Results_DrawCourseNumber(int x, int y, int timer)
{
    int course_number_base_x;

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        x -= (Menu_Results_disMap[Menu_Results_DisIndex_Attack0Course].w + 24) >> 1;

        Menu_Results_DrawFadingTexture(x, y, Menu_Results_DisIndex_Attack0Course, timer);

        course_number_base_x = x + 12;

        Menu_Results_DrawFadingTextureClut(course_number_base_x + Menu_Results_disMap[Menu_Results_DisIndex_Attack0Course].w, y - 1,
            Audio_Seq_unusedStateFlags.timeAttackBoss + 1, timer, 0x7FF4);
    }
}

// decomp: MENUF.PRG _renderBossName (src/MENU/MENUF.PRG/3B8.c:1195)
void Menu_Results_DrawBossName(int x, int y, int timer)
{
    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        if (Audio_Seq_unusedStateFlags.timeAttackBoss != 6) {

            Menu_Results_DrawFadingTexture(x
                                     - (Menu_Results_disMap[Audio_Seq_unusedStateFlags.timeAttackBoss
                                                + Menu_Results_DisIndex_Attack0Minotaur]
                                             .w
                                         >> 1),
                y, Audio_Seq_unusedStateFlags.timeAttackBoss + Menu_Results_DisIndex_Attack0Minotaur, timer);

        } else {
            x -= (Menu_Results_disMap[Menu_Results_DisIndex_Attack0Death].w + Menu_Results_disMap[Menu_Results_DisIndex_Attack0OgreZombie].w)
              >> 1;

            Menu_Results_DrawFadingTexture(x, y, Menu_Results_DisIndex_Attack0Death, timer);

            x += Menu_Results_disMap[Menu_Results_DisIndex_Attack0Death].w;

            Menu_Results_DrawFadingTexture(x, y, Menu_Results_DisIndex_Attack0OgreZombie, timer);
        }
    }
}

// decomp: MENUF.PRG _renderBestTimeHeader (src/MENU/MENUF.PRG/3B8.c:1226)
void Menu_Results_DrawBestTime(int x, int y, int timer)
{
    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        int best_time_base_x;
        x -= (Menu_Results_disMap[Menu_Results_DisIndex_Attack0BestTime].w + 96) >> 1;

        Menu_Results_DrawFadingTexture(x, y, Menu_Results_DisIndex_Attack0BestTime, timer);

        x++;
        best_time_base_x = x + 11;

        Menu_Results_DrawTime(best_time_base_x + Menu_Results_disMap[Menu_Results_DisIndex_Attack0BestTime].w, y + 2, timer,
            Audio_Seq_unusedScoredata.bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][0]
                .time,
            0);
    }
}

// decomp: MENUF.PRG _renderStartCountdown (src/MENU/MENUF.PRG/3B8.c:1248)
void Menu_Results_DrawStartCountdown(int x, int y, int timer)
{
    int countdown_texture_id = (timer / 15) + Menu_Results_DisIndex_Attack0Three;

    Menu_Results_DrawFadingTexture(
        x - (Menu_Results_disMap[countdown_texture_id].w / 2), y - (Menu_Results_disMap[countdown_texture_id].h / 2), countdown_texture_id, timer % 15);
}

// decomp: MENUF.PRG _pulseTimeAttackEnd (src/MENU/MENUF.PRG/3B8.c:1256)
void Menu_Results_PulseTimeAttackClut(int blend_weight)
{
    RECT rect;
    int weighted_red;
    int r;
    int g;
    int b;
    int i;
    int packed_blue;
    int packed_red_green;

    for (i = 0; i < 32; ++i) {

        b = Menu_Results_clutBuf[128 + i];

        weighted_red = ((Menu_Results_clutBuf[32 + i] & 0x1F) * blend_weight)
               + ((Menu_Results_clutBuf[96 + i] & 0x1F) * (16 - blend_weight));

        r = (weighted_red / 16) & 0x1F;

        g = ((Menu_Results_clutBuf[32 + i] & 0x3E0) * blend_weight)
          + ((Menu_Results_clutBuf[96 + i] & 0x3E0) * (16 - blend_weight));

        packed_red_green = r | ((g / 16) & 0x3E0);

        b = ((Menu_Results_clutBuf[32 + i] & 0x7C00) * blend_weight)
          + ((Menu_Results_clutBuf[96 + i] & 0x7C00) * (16 - blend_weight));

        packed_blue = (b / 16) & 0x7C00;

        Menu_Results_clutBuf[128 + i] = (packed_red_green | packed_blue) | (Menu_Results_clutBuf[32 + i] & 0x8000);
    }

    setRECT(&rect, 768, 511, 160, 1);

    LoadImage(&rect, (u_long*)Menu_Results_clutBuf);
}

// decomp: MENUF.PRG _renderTimeAttackResultsHeader (src/MENU/MENUF.PRG/3B8.c:1654)
void Menu_Results_DrawTimeAttackResultsHeading(int x, int y, int timer)
{

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_timeAttackResultsHeadingColors[0].cd = timer;
        x -= (Menu_Results_disMap[Menu_Results_DisIndex_Time0TimeAttackResults].w
                 + Menu_Results_disMap[Menu_Results_DisIndex_Time0TimeAttackResults + 1].w)
          >> 1;

        Render_DrawTextureFlashFade(Menu_Results_disMap, x, y, Menu_Results_DisIndex_Time0TimeAttackResults, Menu_Results_timeAttackResultsHeadingColors);
        Render_DrawTextureFlashFade(Menu_Results_disMap, x + Menu_Results_disMap[Menu_Results_DisIndex_Time0TimeAttackResults].w, y,
            Menu_Results_DisIndex_Time0TimeAttackResults + 1, Menu_Results_timeAttackResultsHeadingColors);
    }
}

// decomp: MENUF.PRG _renderTimeAttackRating (src/MENU/MENUF.PRG/3B8.c:1678)
void Menu_Results_DrawTimeAttackRating(int x, int y, int fade_timer)
{
    int rating_texture_offset;

    if (fade_timer < 0) {
        fade_timer = 0;
    }
    if (fade_timer > 64) {
        fade_timer = 64;
    }

    if (fade_timer <= 0) {
        return;
    }

    if (Menu_Results_timeTrialTime < 7680) {
        rating_texture_offset = 0;
    } else if (Menu_Results_timeTrialTime <= 65535) {
        rating_texture_offset = 1;
    } else if (Menu_Results_timeTrialTime <= 73215) {
        rating_texture_offset = 2;
    } else if (Menu_Results_timeTrialTime <= 131071) {
        rating_texture_offset = 3;
    } else {
        rating_texture_offset = 4;
    }

    Menu_Results_DrawFadingTexture(x, y, rating_texture_offset + Menu_Results_DisIndex_Time0Excellent, fade_timer);
}

// decomp: MENUF.PRG _renderTimeAttackRatingWipe (src/MENU/MENUF.PRG/3B8.c:1708)
void Menu_Results_DrawTimeAttackRatingWipe(int x, int y, int arg2)
{

    int wipe_edge_x;

    if (arg2 < 0) {
        arg2 = 0;
    }

    if (arg2 > 64) {
        arg2 = 64;
    }

    if (arg2 <= 0) {
        return;
    }

    wipe_edge_x = x + (arg2 * 8);

    if (Menu_Results_timeTrialTime < 7680) {
        arg2 = 0;
    } else if (Menu_Results_timeTrialTime <= 65535) {
        arg2 = 1;
    } else if (Menu_Results_timeTrialTime <= 73215) {
        arg2 = 2;
    } else if (Menu_Results_timeTrialTime <= 131071) {
        arg2 = 3;
    } else {
        arg2 = 4;
    }

    Render_DrawTextureWipe(Menu_Results_disMap, x, y, arg2 + Menu_Results_DisIndex_Time0Excellent, Menu_Results_timeAttackRatingWipeColors, wipe_edge_x);
}

// decomp: MENUF.PRG _renderHeaderAndTime (src/MENU/MENUF.PRG/3B8.c:1743)
void Menu_Results_DrawTimeAttackTime(int draw_x, int draw_y, int fade_timer)
{
    if (fade_timer < 0) {
        fade_timer = 0;
    }

    if (fade_timer > 64) {
        fade_timer = 64;
    }

    if (fade_timer > 0) {
        Menu_Results_DrawFadingTexture(draw_x, draw_y, Menu_Results_DisIndex_Time0Time, fade_timer);

        draw_x += Menu_Results_disMap[Menu_Results_DisIndex_Time0Time].w;

        Menu_Results_DrawTime(draw_x, draw_y + 2, fade_timer, Menu_Results_timeTrialTime, 0);
    }
}

// decomp: MENUF.PRG _renderHeaderAndIncrementalTime (src/MENU/MENUF.PRG/3B8.c:1762)
void Menu_Results_DrawCountingTimeAttackTime(int x, int y, int timer)
{
    int total_centiseconds;
    int var_a3;
    int* time_accumulator_address;

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_DrawFadingTexture(x, y, Menu_Results_DisIndex_Time0Time, timer);

        x += Menu_Results_disMap[Menu_Results_DisIndex_Time0Time].w;

        if (timer >= 16) {
            int mins = (Menu_Results_timeTrialTime >> 0x10) & 0xFF;
            int secs = (Menu_Results_timeTrialTime >> 8) & 0xFF;
            int ms = Menu_Results_timeTrialTime & 0xFF;
            total_centiseconds = ((mins * 6000) + (secs * 100)) + ms;
            var_a3 = total_centiseconds;
            var_a3 = ((*(time_accumulator_address = &var_a3)) * (timer - 16)) / 48;
            mins = var_a3 / 6000;
            var_a3 = var_a3 % 6000;
            secs = var_a3 / 100;
            var_a3 = var_a3 % 100;
            var_a3 = (mins << 16) | (secs << 8) | var_a3;
        } else {
            var_a3 = 0;
        }

        Menu_Results_DrawTime(x, y + 2, timer, var_a3, 0);
    }
}

// decomp: MENUF.PRG _renderNewRecord (src/MENU/MENUF.PRG/3B8.c:1801)
void Menu_Results_DrawNewRecord(int x, int y, int timer)
{
    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_DrawFadingTexture(x, y, Menu_Results_DisIndex_Time0NewRecord, timer);
    }
}

// decomp: MENUF.PRG _renderAllStoredTimes (src/MENU/MENUF.PRG/3B8.c:1816)
void Menu_Results_DrawLeaderboard(int x, int y, int timer)
{
    int isNewRecord;
    int i;
    int leaderboard_clut;

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    for (i = 0; i < 3; ++i) {

        leaderboard_clut = getClut(800, 511);
        isNewRecord = Menu_Results_newTimeSlot == i;

        if (isNewRecord != 0) {
            leaderboard_clut = getClut(896, 511);
        }

        Menu_Results_DrawTime(x - 84, y + i * 20 + 2, timer,
            Audio_Seq_unusedScoredata
                .bossTimeTrialScores[0][Audio_Seq_unusedStateFlags.timeAttackBoss * 3 + i]
                .time,
            isNewRecord);
        Menu_Results_DrawFadingTextureClut((x - Menu_Results_disMap[Menu_Results_DisIndex_Time0First + i].w) - 88, y + i * 20,
            Menu_Results_DisIndex_Time0First + i, timer, leaderboard_clut);
    }
}

// decomp: MENUF.PRG _renderStoredTime (src/MENU/MENUF.PRG/3B8.c:1849)
void Menu_Results_DrawLeaderboardEntry(int x, int y, int slot, int timer)
{
    int isCurrentTime;
    int clut;

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    isCurrentTime = Menu_Results_newTimeSlot == slot;
    clut = getClut(800, 511);

    if (isCurrentTime != 0) {
        clut = getClut(896, 511);
    }

    Menu_Results_DrawTime(x - 84, y + (slot * 20) + 2, timer,
        Audio_Seq_unusedScoredata.bossTimeTrialScores[Audio_Seq_unusedStateFlags.timeAttackBoss][slot]
            .time,
        isCurrentTime);
    Menu_Results_DrawFadingTextureClut((x - Menu_Results_disMap[slot + Menu_Results_DisIndex_Time0First].w) - 88,
        y + (slot * 20), slot + Menu_Results_DisIndex_Time0First, timer, clut);
}

// MENUF.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Results_TimeAttack_ResetMenufStatics(void)
{
    {
        static const CVECTOR initial[] = { { 220, 80, 64 }, { 64, 128, 220 }, { 128, 128, 128 } };
        memcpy((void*)&Menu_Results_timeAttackStartColors, &initial, sizeof Menu_Results_timeAttackStartColors);
    }
    {
        static const CVECTOR initial[] = { { 128, 96, 64 }, { 200, 180, 160 }, { 128, 96, 64 } };
        memcpy((void*)&Menu_Results_timeAttackHeadingColors, &initial, sizeof Menu_Results_timeAttackHeadingColors);
    }
    {
        static const CVECTOR initial[] = { { 128, 128, 128 }, { 128, 128, 128 } };
        memcpy((void*)&Menu_Results_timeAttackResultsHeadingColors, &initial, sizeof Menu_Results_timeAttackResultsHeadingColors);
    }
    {
        static const CVECTOR initial[] = { { 128, 128, 128 }, { 128, 128, 128 } };
        memcpy((void*)&Menu_Results_timeAttackRatingWipeColors, &initial, sizeof Menu_Results_timeAttackRatingWipeColors);
    }
}

