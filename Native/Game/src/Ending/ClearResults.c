// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
#include "Ending/ClearResults.h"
#include "Ending/Credits/StaffRoll.h"
#include "Ending/Results.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Render/Images.h"
#include "Render/TextureAnimation.h"
#include "State/GameTime.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/Settings.h"
#include "State/StateFlags.h"

// decomp: D_800DB876 (D_800DB876@ENDING/ENDING.PRG)
u_char Ending_kRankHeadingWidth; /* data: contents loaded in Step 5 */

// decomp: D_800DBA7C (D_800DBA7C@ENDING/ENDING.PRG)
CVECTOR Ending_congratulationsTint[2]; /* data: contents loaded in Step 5 */

// decomp: D_800DBA88 (D_800DBA88@ENDING/ENDING.PRG)
CVECTOR Ending_scoreTint[]; /* data: contents loaded in Step 5 */

// decomp: D_800DBA90 (D_800DBA90@ENDING/ENDING.PRG)
CVECTOR Ending_countingScoreTint[]; /* data: contents loaded in Step 5 */

// decomp: D_800DBA98 (D_800DBA98@ENDING/ENDING.PRG)
CVECTOR Ending_mapCompletionTint[]; /* data: contents loaded in Step 5 */

// decomp: D_800DBAA0 (D_800DBAA0@ENDING/ENDING.PRG)
CVECTOR Ending_countingMapCompletionTint[]; /* data: contents loaded in Step 5 */

// decomp: D_800DBAA8 (D_800DBAA8@ENDING/ENDING.PRG)
CVECTOR Ending_rankHeadingTint[4]; /* data: contents loaded in Step 5 */

// decomp: D_800DBAB0 (D_800DBAB0@ENDING/ENDING.PRG)
CVECTOR Ending_rankLabelTint[]; /* data: contents loaded in Step 5 */

// decomp: _rank (_rank@ENDING/ENDING.PRG)
u_int Ending_riskbreakerRankIndex;

// decomp: _clearCount (_clearCount@ENDING/ENDING.PRG)
int Ending_clearCount;

// decomp: _score (_score@ENDING/ENDING.PRG)
u_int Ending_finalScore;

// decomp: _mapCompletion (_mapCompletion@ENDING/ENDING.PRG)
int Ending_mapCompletion;

// decomp: _onScreenMapCompletion (_onScreenMapCompletion@ENDING/ENDING.PRG)
u_int Ending_displayedMapCompletion;

// decomp: _onScreenScore (_onScreenScore@ENDING/ENDING.PRG)
int Ending_displayedScore;

// decomp: _submenuState (_submenuState@ENDING/ENDING.PRG)
int Ending_resultsLoadPhase;

// decomp: D_800DC20C (D_800DC20C@ENDING/ENDING.PRG)
int Ending_resultsAnimationFrame;

// decomp: D_800DC210 (D_800DC210@ENDING/ENDING.PRG)
int Ending_isLeavingResults;

// decomp: _incrementingScore (_incrementingScore@ENDING/ENDING.PRG)
int Ending_isScoreIncrementing;

// decomp: _incrementingMapCompletion (_incrementingMapCompletion@ENDING/ENDING.PRG)
int Ending_isMapCompletionIncrementing;

// decomp: ENDING.PRG _do_updateClearGameStats (src/ENDING/ENDING.PRG/D4.c:1502)
void Ending_CommitClearProgress(void)
{
    int i;

    if (Audio_Seq_unusedScoredata.completionTimeMinutes == 0) {
        Audio_Seq_unusedScoredata.clearCount = Audio_Seq_unusedStateFlags.clearCount;
        Audio_Seq_unusedScoredata.completionTimeMinutes =
            (State_playTime.t.hours * 60) + State_playTime.t.minutes;
    } else {
        int minutes = (State_playTime.t.hours * 60) + State_playTime.t.minutes;
        if (minutes < Audio_Seq_unusedScoredata.completionTimeMinutes) {
            Audio_Seq_unusedScoredata.completionTimeMinutes = minutes;
            Audio_Seq_unusedScoredata.clearCount = Audio_Seq_unusedStateFlags.clearCount;
        }
    }

    Ending_AwardClearTitles();
    Core_Memory_ZeroAlignedWords(&State_roomVisitProgress, sizeof State_roomVisitProgress);

    for (i = 0x10; i < 0x440; ++i) {
        ((u_char*)&Audio_Seq_unusedStateFlags)[i] = 0;
    }

    if (Audio_Seq_unusedStateFlags.difficulty != 0) {
        Audio_Seq_unusedStateFlags.normalModeCleared = 1;
    } else {
        Audio_Seq_unusedStateFlags.difficulty = 1;
    }

    if (Audio_Seq_unusedStateFlags.clearCount < 99) {
        ++Audio_Seq_unusedStateFlags.clearCount;
    }

    State_savedEntryDoor.destinationZoneId = 1;
    State_savedEntryDoor.destinationRoomId = 0;
}

// decomp: ENDING.PRG _updateClearGameStats (src/ENDING/ENDING.PRG/D4.c:1549)
void Ending_UpdateClearProgress(void) { Ending_CommitClearProgress(); }

// decomp: ENDING.PRG _updateTitles (src/ENDING/ENDING.PRG/D4.c:1570)
void Ending_AwardClearTitles(void)
{
    int i;
    int j;
    u_int count;

    if (Audio_Seq_unusedScoredata.completionTimeMinutes < 600) {
        Audio_Seq_unusedScoredata.titles |= 2;
    }

    for (i = 184; i < 224; ++i) {
        if (!Battle_actionDefinitions[i].unlocked) {
            break;
        }
    }

    if (i == 224) {
        Audio_Seq_unusedScoredata.titles |= 1 << 14;
    }

    for (i = 22; i < 54; ++i) {
        if ((i != 33) && (i != 36) && (i != 38) && (i != 39)
            && !Battle_actionDefinitions[i].unlocked) {
            break;
        }
    }

    if (i == 54) {
        Audio_Seq_unusedScoredata.titles |= 1 << 15;
    }

    if (Input_settings.currentGameSaveCount == 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 16;
    }

    if (Audio_Seq_unusedStateFlags.magicUsed == 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 17;
    }

    if (Audio_Seq_unusedStateFlags.battleAbilitiesUsed == 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 18;
    }

    if (Audio_Seq_unusedStateFlags.breakArtsUsed == 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 19;
    }

    count = 0;

    for (i = 0; i < 16; ++i) {
        int t1 = 1;
        for (j = 0; j < 32; ++j) {
            if (State_roomVisitProgress.roomFlags[i] & Ending_countableRoomBits[i] & (t1 << j)) {
                ++count;
            }
        }
    }

    if (Audio_Seq_unusedScoredata.mapCompletion < count) {
        Audio_Seq_unusedScoredata.mapCompletion = count;
    }

    count = 0;

    for (i = 0; i < 64; ++i) {
        if (Audio_Seq_unusedStateFlags.chestsOpened[i] != 0) {
            ++count;
        }
    }

    if (Audio_Seq_unusedScoredata.openedChestCount < count) {
        Audio_Seq_unusedScoredata.openedChestCount = count;
    }

    if (Audio_Seq_unusedStateFlags.damascusGolemDefeated != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 4;
    }

    if (Audio_Seq_unusedStateFlags.damascusCrabDefeated != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 5;
    }

    if (Audio_Seq_unusedStateFlags.ravanaDefeated != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 6;
    }

    if (Audio_Seq_unusedStateFlags.dragonZombieDefeated != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 7;
    }

    if (Audio_Seq_unusedStateFlags.deathAndOgreZombieDefeated != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 8;
    }

    if (Audio_Seq_unusedStateFlags.asuraDefeated != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 9;
    }

    if (Audio_Seq_unusedStateFlags.goldKeyObtained != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 12;
    }

    if (Audio_Seq_unusedStateFlags.chestKeyObtained != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 13;
    }
}

// decomp: ENDING.PRG func_8006BD78 (src/ENDING/ENDING.PRG/D4.c:1677)
void Ending_ShowClearResults(void)
{
    int temp_s0;

    Ending_resultsLoadPhase = 0;

    do {
        Ending_Credits_BeginFrame();
        temp_s0 = Ending_LoadClearResultsStep();
        Ending_Credits_PresentFrame(*(u_long**)((uintptr_t)Core_ScratchBase() + 0x004) + 0x7FF);
    } while (temp_s0 == 0);

    do {
        Ending_Credits_BeginFrame();
        temp_s0 = Ending_DrawClearResults();
        VSync(0);
        Ending_Credits_PresentFrame(*(u_long**)((uintptr_t)Core_ScratchBase() + 0x004) + 0x7FF);
    } while (temp_s0 == 0);
}

// decomp: ENDING.PRG func_8006BE04 (src/ENDING/ENDING.PRG/D4.c:1698)
int Ending_LoadClearResultsStep(void)
{
    static const Content_DiscFile Ending_resultsDiscFiles[] = {
        { VS_END_DIS_LBA, VS_END_DIS_SIZE },
        { VS_ENDSCR00_DIS_LBA, VS_ENDSCR00_DIS_SIZE },
        { VS_ENDSCR01_DIS_LBA, VS_ENDSCR01_DIS_SIZE },
        { VS_ENDSCR02_DIS_LBA, VS_ENDSCR02_DIS_SIZE },
        { VS_ENDSCR03_DIS_LBA, VS_ENDSCR03_DIS_SIZE },
        { VS_ENDSCR04_DIS_LBA, VS_ENDSCR04_DIS_SIZE },
        { VS_ENDSCR05_DIS_LBA, VS_ENDSCR05_DIS_SIZE },
        { VS_ENDSCR06_DIS_LBA, VS_ENDSCR06_DIS_SIZE },
        { VS_ENDSCR07_DIS_LBA, VS_ENDSCR07_DIS_SIZE },
        { VS_ENDSCR08_DIS_LBA, VS_ENDSCR08_DIS_SIZE },
        { VS_ENDSCR09_DIS_LBA, VS_ENDSCR09_DIS_SIZE },
        { VS_ENDSCR10_DIS_LBA, VS_ENDSCR10_DIS_SIZE },
        { VS_ENDSCR11_DIS_LBA, VS_ENDSCR11_DIS_SIZE },
        { VS_ENDSCR12_DIS_LBA, VS_ENDSCR12_DIS_SIZE },
        { VS_ENDSCR13_DIS_LBA, VS_ENDSCR13_DIS_SIZE },
        { VS_ENDSCR14_DIS_LBA, VS_ENDSCR14_DIS_SIZE },
        { VS_ENDSCR15_DIS_LBA, VS_ENDSCR15_DIS_SIZE },
    };

    extern Content_DiscQueueSlot* Ending_resultsDiscReadSlot;
    extern void* Ending_resultsTimBuffer;

    TIM_IMAGE sp10;
    int i;
    int var_a1;
    int var_v1;

    if (Ending_resultsLoadPhase == 0) {

        Ending_resultsTimBuffer = Core_Memory_AllocateFromHighEnd(Ending_resultsDiscFiles->size);
        Ending_resultsDiscReadSlot = Content_AllocateDiscRequest(Ending_resultsDiscFiles);

        Content_EnqueueDiscRequest(Ending_resultsDiscReadSlot, Ending_resultsTimBuffer);

        ++Ending_resultsLoadPhase;

    } else if (Ending_resultsLoadPhase == 1) {

        if (Ending_resultsDiscReadSlot->state != Content_DiscRequestState_Loaded) {
            return 0;
        }

        for (i = 0; i < 3; ++i) {

            Render_ParseTimImage(Ending_resultsTimBuffer + i * 0x8220, &sp10);

            if (sp10.paddr != NULL) {
                sp10.prect->x = 832 + i * 64;
                sp10.prect->y = 0x100;
                sp10.prect->h = 0xFF;
                LoadImage(sp10.prect, sp10.paddr);
            }

            if (i == 0) {
                if (sp10.caddr != NULL) {
                    sp10.crect->x = 0x300;
                    sp10.crect->y = 0x1FF;
                    sp10.crect->w = 0x80;
                    sp10.crect->h = 1;
                    *sp10.caddr = 0;
                    LoadImage(sp10.crect, sp10.caddr);
                }
            }
        }

        Content_ReleaseDiscRequest(Ending_resultsDiscReadSlot);
        ++Ending_resultsLoadPhase;

    } else if (Ending_resultsLoadPhase == 2) {

        Core_Memory_FreeFromHighEnd(Ending_resultsTimBuffer);

        var_a1 = 0;
        Ending_displayedMapCompletion = 0;
        Ending_displayedScore = 0;
        Ending_resultsAnimationFrame = 0;
        Ending_isLeavingResults = 0;
        Ending_clearCount = Audio_Seq_unusedStateFlags.clearCount;

        for (i = 0; i < 16; ++i) {
            int flag;
            for (flag = 0; flag < 32; ++flag) {
                int v = 1;
                if (State_roomVisitProgress.roomFlags[i] & Ending_countableRoomBits[i] & (v << flag)) {
                    ++var_a1;
                }
            }
        }

        var_v1 = 0;

        for (i = 0; i < 64; ++i) {
            if (Audio_Seq_unusedStateFlags.chestsOpened[i] != 0) {
                ++var_v1;
            }
        }

        if (Audio_Seq_unusedScoredata.mapCompletion < var_a1) {
            Audio_Seq_unusedScoredata.mapCompletion = var_a1;
        }

        if (Audio_Seq_unusedScoredata.openedChestCount < var_v1) {
            Audio_Seq_unusedScoredata.openedChestCount = var_v1;
        }

        Ending_mapCompletion = (Audio_Seq_unusedScoredata.mapCompletion * 100) / 361;

        Ending_CalculateClearScore();
        Ending_DetermineRiskbreakerRank();

        Ending_resultsTimBuffer = Core_Memory_AllocateFromHighEnd(Ending_resultsDiscFiles[Ending_riskbreakerRankIndex + 1].size);
        Ending_resultsDiscReadSlot = Content_AllocateDiscRequest(&Ending_resultsDiscFiles[Ending_riskbreakerRankIndex + 1]);
        Content_EnqueueDiscRequest(Ending_resultsDiscReadSlot, Ending_resultsTimBuffer);
        ++Ending_resultsLoadPhase;

    } else if (Ending_resultsLoadPhase == 3) {

        if (Ending_resultsDiscReadSlot->state != 4) {
            return 0;
        }

        Render_ParseTimImage(Ending_resultsTimBuffer, &sp10);

        if (sp10.paddr != NULL) {
            sp10.prect->x = 0;
            sp10.prect->y = 0x100;
            LoadImage(sp10.prect, sp10.paddr);
        }

        Content_ReleaseDiscRequest(Ending_resultsDiscReadSlot);

        ++Ending_resultsLoadPhase;

    } else {
        Ending_resultsLoadPhase = 0;
        Core_Memory_FreeFromHighEnd(Ending_resultsTimBuffer);

        return 1;
    }

    return 0;
}

// decomp: ENDING.PRG _renderCongratulationsScreen (src/ENDING/ENDING.PRG/D4.c:1844)
int Ending_DrawClearResults(void)
{
    if (Ending_isLeavingResults == 0) {

        if (Ending_resultsAnimationFrame == 0xD0) {
            Audio_PlaySfxCentered(0x7E, 0x77);
            Audio_PlaySfxCentered(0x7E, 0x78);
        }

        Ending_DrawRankBackground(Ending_resultsAnimationFrame);
        Ending_DrawCongratulations(0xA0, 0x28, Ending_resultsAnimationFrame);
        Ending_DrawCountingScore(0xA0, 0x50, Ending_resultsAnimationFrame - 0x20, Ending_isLeavingResults);
        Ending_DrawCountingMapCompletion(0xA0, 0x66, Ending_resultsAnimationFrame - 0x70, Ending_isLeavingResults);
        Ending_DrawRiskbreakerRankHeading(0xA0, 0x8A, Ending_resultsAnimationFrame - 0xC0);
        Ending_DrawRiskbreakerRank(0xA0, 0x9A, Ending_resultsAnimationFrame - 0xD0);

        if (Ending_resultsAnimationFrame >= 0x1001) {
            Ending_resultsAnimationFrame = 0x1000;
        }

        if (Input_buttonsPressed.allButtons != 0) {
            Ending_isLeavingResults = 1;
            Ending_resultsAnimationFrame = 0;
        }

    } else {

        Ending_DrawRankBackground(8 - Ending_resultsAnimationFrame);
        Ending_DrawCongratulations(0xA0, 0x28, 8 - Ending_resultsAnimationFrame);
        Ending_DrawScore(0xA0, 0x50, 8 - Ending_resultsAnimationFrame, 3);
        Ending_DrawMapCompletion(0xA0, 0x66, 8 - Ending_resultsAnimationFrame, 3);
        Ending_DrawRiskbreakerRankHeading(0xA0, 0x8A, 8 - Ending_resultsAnimationFrame);
        Ending_DrawRiskbreakerRank(0xA0, 0x9A, 8 - Ending_resultsAnimationFrame);

        if (Ending_resultsAnimationFrame >= 8) {
            return 1;
        }
    }

    ++Ending_resultsAnimationFrame;
    return 0;
}

// decomp: ENDING.PRG func_8006C3CC (src/ENDING/ENDING.PRG/D4.c:1887)
void Ending_DrawRankBackground(int arg0)
{
    int var_s2;
    int i;
    int temp_s4;
    POLY_FT4* poly;

    if (arg0 < 0) {
        arg0 = 0;
    }

    if (arg0 > 64) {
        arg0 = 64;
    }

    temp_s4 = arg0 * 2;

    if (arg0 <= 0) {
        return;
    }

    poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);
    var_s2 = 64;

    for (i = 0; i < 5; ++i) {
        setPolyFT4(poly);
        setXY4(poly, i * 64, 8, 64 + i * 64, 8, i * 64, 232, 64 + i * 64, 232);
        setUV4(poly, 0, 0, 64, 0, 0, 224, 64, 224);
        setRGB0(poly, temp_s4, temp_s4, temp_s4);
        setClut(poly, 0, 0);
        poly->tpage = (i + 16) | getTPage(3, 0, 0, 0);
        AddPrim(*((void**)((uintptr_t)Core_ScratchBase() + 0x004)) + 0x1FF8, poly++);
        var_s2 += 64;
    }

    *(void**)((uintptr_t)Core_ScratchBase() + 0x000) = poly;
}

// decomp: ENDING.PRG _renderCongratulations (src/ENDING/ENDING.PRG/D4.c:1925)
void Ending_DrawCongratulations(int arg0, int arg1, int arg2)
{
    extern CVECTOR Ending_congratulationsTint[2];

    if (arg2 < 0) {
        arg2 = 0;
    }

    if (arg2 > 64) {
        arg2 = 64;
    }

    if (arg2 > 0) {
        Ending_congratulationsTint[0].cd = arg2;
        Ending_congratulationsTint[1].cd = arg2;
        arg0 = arg0 - ((Ending_resultAtlasRegions[22].w + Ending_resultAtlasRegions[23].w) >> 1);
        Render_DrawTextureFlashFade(arg0, arg1, 0x16, Ending_congratulationsTint);
        Render_DrawTextureFlashFade(arg0 + Ending_resultAtlasRegions[22].w, arg1, 0x17, &Ending_congratulationsTint[1]);
    }
}

// decomp: ENDING.PRG _renderScore (src/ENDING/ENDING.PRG/D4.c:1946)
void Ending_DrawScore(int arg0, int arg1, int arg2, int arg3 __attribute__((unused)))
{
    extern CVECTOR Ending_scoreTint[];

    char sp10[10];
    int i;
    int v0;
    int v1;

    if (arg2 < 0) {
        arg2 = 0;
    }

    if (arg2 > 64) {
        arg2 = 64;
    }

    if (arg2 > 0) {

        Ending_scoreTint[0].cd = arg2;

        sprintf(sp10, "%09d", Ending_finalScore);

        v1 = Ending_resultAtlasRegions[18].w + Ending_resultAtlasRegions[26].w + Ending_resultAtlasRegions[20].w;
        arg0 -= (Ending_resultAtlasRegions[10].w * 2 + v1 + 0x74) >> 1;

        Render_DrawTextureTintFade(arg0, arg1, 0x12, Ending_scoreTint);

        arg0 += Ending_resultAtlasRegions[18].w;

        Render_DrawTextureTintFade(arg0, arg1 + 7, 0x1A, Ending_scoreTint);

        v0 = arg0 + 2;
        arg0 = v0 + Ending_resultAtlasRegions[26].w;

        for (i = 0; i < 9; ++i) {
            Render_DrawTextureTintFade(arg0, arg1 + 3, sp10[i] - 0x30, Ending_scoreTint);
            arg0 += 12;
            if ((i == 2) || (i == 5)) {
                int v0;
                Render_DrawTextureTintFade(arg0, arg1 + 0xE, 0xA, Ending_scoreTint);
                v0 = arg0 + 3;
                arg0 = v0 + Ending_resultAtlasRegions[10].w;
            }
        }

        Render_DrawTextureTintFade(arg0, arg1 + 8, 0x14, Ending_scoreTint);
    }
}

// decomp: ENDING.PRG _renderIncrementalScore (src/ENDING/ENDING.PRG/D4.c:1996)
void Ending_DrawCountingScore(
    int arg0, int arg1, int arg2, int arg3 __attribute__((unused)))
{
    extern CVECTOR Ending_countingScoreTint[];

    char sp18[16];
    int temp_s2;
    int var_s1;
    int i;
    int v0;
    int v1;

    var_s1 = arg2;

    if (var_s1 < 0) {
        var_s1 = 0;
    }

    if (var_s1 > 64) {
        var_s1 = 64;
    }

    if (var_s1 > 0) {
        temp_s2 = arg0 + (var_s1 * 8);

        if (Ending_finalScore != 0 && var_s1 >= 32) {

            if (Ending_displayedScore == 0) {
                Ending_isScoreIncrementing = 1;
                Audio_PlaySfxCentered(0x7E, 0x72);
            }

            if ((Ending_displayedScore == Ending_finalScore) && (Ending_isScoreIncrementing != 0)) {
                Ending_isScoreIncrementing = 0;
                Audio_StopSfx(0x7E, 0x72);
                Audio_PlaySfxCentered(0x7E, 0x73);
            }
        }

        if (var_s1 >= 32) {
            Ending_displayedScore = ((u_int)Ending_finalScore >> 5) * (var_s1 - 32);
        }

        if (var_s1 == 64) {
            Ending_displayedScore = Ending_finalScore;
        }

        sprintf(sp18, "%09d", Ending_displayedScore);

        v1 = Ending_resultAtlasRegions[18].w + Ending_resultAtlasRegions[26].w + Ending_resultAtlasRegions[20].w;
        arg0 -= (Ending_resultAtlasRegions[10].w * 2 + v1 + 0x74) >> 1;

        Render_DrawTextureWipe(arg0, arg1, 0x12, Ending_countingScoreTint, temp_s2);

        arg0 += Ending_resultAtlasRegions[18].w;

        Render_DrawTextureWipe(arg0, arg1 + 7, 0x1A, Ending_countingScoreTint, temp_s2);

        v0 = arg0 + 2;
        arg0 = v0 + Ending_resultAtlasRegions[26].w;

        for (i = 0; i < 9; ++i) {
            Render_DrawTextureWipe(arg0, arg1 + 3, sp18[i] - '0', Ending_countingScoreTint, temp_s2);
            arg0 += 12;
            if ((i == 2) || (i == 5)) {
                Render_DrawTextureWipe(arg0, arg1 + 0xE, 0xA, Ending_countingScoreTint, temp_s2);
                arg0 += 3 + Ending_resultAtlasRegions[10].w;
            }
        }

        Render_DrawTextureWipe(arg0, arg1 + 8, 0x14, Ending_countingScoreTint, temp_s2);
    }
}

// decomp: ENDING.PRG _renderMapCompletion (src/ENDING/ENDING.PRG/D4.c:2070)
void Ending_DrawMapCompletion(
    int arg0, int arg1, int arg2, int arg3 __attribute__((unused)))
{
    extern CVECTOR Ending_mapCompletionTint[];

    char sp10[2];
    int i;
    int v0;

    if (arg2 < 0) {
        arg2 = 0;
    }

    if (arg2 > 64) {
        arg2 = 64;
    }

    if (arg2 > 0) {

        Ending_mapCompletionTint[0].cd = arg2;

        sprintf(sp10, "%03d", Ending_mapCompletion);

        arg0 -= ((Ending_resultAtlasRegions[21].w + Ending_resultAtlasRegions[26].w + Ending_resultAtlasRegions[19].w + 0x26) >> 1);

        Render_DrawTextureTintFade(arg0, arg1, 0x15, Ending_mapCompletionTint);

        arg0 += Ending_resultAtlasRegions[21].w;

        Render_DrawTextureTintFade(arg0, arg1 + 7, 0x1A, Ending_mapCompletionTint);

        v0 = arg0 + 2;
        arg0 = v0 + Ending_resultAtlasRegions[26].w;

        for (i = 0; i < 3; ++i) {
            Render_DrawTextureTintFade(arg0, arg1 + 3, sp10[i] - '0', Ending_mapCompletionTint);
            arg0 += 12;
        }

        Render_DrawTextureTintFade(arg0, arg1 + 8, 0x13, Ending_mapCompletionTint);
    }
}

// decomp: ENDING.PRG _renderIncrementalMapCompletion (src/ENDING/ENDING.PRG/D4.c:2113)
void Ending_DrawCountingMapCompletion(
    int arg0, int arg1, int arg2, int arg3 __attribute__((unused)))
{
    extern CVECTOR Ending_countingMapCompletionTint[];

    char sp18[2];
    int temp_s4;
    int i;
    int v0;

    if (arg2 < 0) {
        arg2 = 0;
    }

    if (arg2 > 64) {
        arg2 = 64;
    }

    if (arg2 <= 0) {
        return;
    }

    temp_s4 = arg0 + arg2 * 8;

    if (Ending_mapCompletion != 0 && arg2 >= 32) {

        if (Ending_displayedMapCompletion == 0) {
            Ending_isMapCompletionIncrementing = 1;
            Audio_PlaySfxCentered(0x7E, 0x72);
        }

        if ((Ending_displayedMapCompletion == Ending_mapCompletion)
            && (Ending_isMapCompletionIncrementing != 0)) {
            Ending_isMapCompletionIncrementing = 0;
            Audio_StopSfx(0x7E, 0x72);
            Audio_PlaySfxCentered(0x7E, 0x73);
        }
    }

    if (arg2 >= 32) {
        if (Ending_mapCompletion >= 32u) {
            Ending_displayedMapCompletion = (u_int)(Ending_mapCompletion * (arg2 - 32)) >> 5;
        } else if (Ending_displayedMapCompletion < Ending_mapCompletion) {
            Ending_displayedMapCompletion += 1;
        }
    }

    sprintf(sp18, "%03d", Ending_displayedMapCompletion);

    arg0 -= ((int)(Ending_resultAtlasRegions[21].w + Ending_resultAtlasRegions[26].w + Ending_resultAtlasRegions[19].w + 0x26) >> 1);

    Render_DrawTextureWipe(arg0, arg1, 0x15, Ending_countingMapCompletionTint, temp_s4);

    arg0 += Ending_resultAtlasRegions[21].w;

    Render_DrawTextureWipe(arg0, arg1 + 7, 0x1A, Ending_countingMapCompletionTint, temp_s4);

    v0 = arg0 + 2;
    arg0 = v0 + Ending_resultAtlasRegions[26].w;

    for (i = 0; i < 3; ++i) {
        Render_DrawTextureWipe(arg0, arg1 + 3, sp18[i] - '0', Ending_countingMapCompletionTint, temp_s4);
        arg0 += 12;
    }

    Render_DrawTextureWipe(arg0, arg1 + 8, 0x13, Ending_countingMapCompletionTint, temp_s4);
}

// decomp: ENDING.PRG _renderRiskbreakerRankHeader (src/ENDING/ENDING.PRG/D4.c:2181)
void Ending_DrawRiskbreakerRankHeading(int x, int y, int timer)
{
    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Ending_rankHeadingTint[0].cd = timer;
        Render_DrawTextureTintFade(x - (Ending_kRankHeadingWidth >> 1), y, 12, Ending_rankHeadingTint);
    }
}

// decomp: ENDING.PRG _renderRiskbreakerRank (src/ENDING/ENDING.PRG/D4.c:2197)
void Ending_DrawRiskbreakerRank(int x, int y, int timer)
{
    extern CVECTOR Ending_rankLabelTint[];

    int xInset;
    int i;

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    xInset = 0;

    if (timer > 0) {

        Ending_rankLabelTint[0].cd = timer;

        for (i = 0; i < Ending_rankLabelRegionIds[Ending_riskbreakerRankIndex][0]; ++i) {
            xInset += Ending_resultAtlasRegions[Ending_rankLabelRegionIds[Ending_riskbreakerRankIndex][i + 1]].w;
        }

        xInset /= 2;
        x -= xInset;

        for (i = 0; i < Ending_rankLabelRegionIds[Ending_riskbreakerRankIndex][0]; ++i) {
            Render_DrawTextureFlashFade(x, y, Ending_rankLabelRegionIds[Ending_riskbreakerRankIndex][i + 1], Ending_rankLabelTint);

            x += Ending_resultAtlasRegions[Ending_rankLabelRegionIds[Ending_riskbreakerRankIndex][i + 1]].w;
        }
    }
}

// decomp: ENDING.PRG _determineRank (src/ENDING/ENDING.PRG/D4.c:2255)
void Ending_DetermineRiskbreakerRank(void)
{
    int unlockedTitles;
    int flag;
    int i;

    for (i = 0, unlockedTitles = 0, flag = 1; i < 16; ++i) {
        if (Audio_Seq_unusedScoredata.titles & (flag << i)) {
            ++unlockedTitles;
        }
    }

    for (i = 0; i < 16; ++i) {
        if (unlockedTitles >= Ending_rankTitleThresholds[i]) {
            if (Ending_finalScore >= Ending_rankScoreThresholds[i]) {
                Ending_riskbreakerRankIndex = i;
                return;
            }
        }
    }
}

// decomp: ENDING.PRG _updateScore (src/ENDING/ENDING.PRG/D4.c:2277)
void Ending_CalculateClearScore(void)
{
    short enemyKillPoints[] = { 20, 20, 40, 80, 100, 60 };
    short weaponAttackPoints[] = { 20, 40, 20, 100, 60, 100, 60, 100, 80, 60 };
    int i;

    Ending_finalScore = 0;

    for (i = 0; i < 6; ++i) {
        Ending_finalScore += Audio_Seq_unusedScoredata.enemyKills[i] * enemyKillPoints[i];
    }

    for (i = 0; i < 10; ++i) {
        Ending_finalScore += Audio_Seq_unusedScoredata.weaponAttacks[i] * weaponAttackPoints[i];
    }

    for (i = 0; i < 8; ++i) {
        if (Audio_Seq_unusedScoredata.bossTimeTrialScores[i][0].time != 0x800000) {
            Ending_finalScore += 20000;
        }
    }

    if (Audio_Seq_unusedScoredata.maxChain < 6) {
        Ending_finalScore += Audio_Seq_unusedScoredata.maxChain * 10;
    } else if (Audio_Seq_unusedScoredata.maxChain < 11) {
        Ending_finalScore += Audio_Seq_unusedScoredata.maxChain * 20;
    } else if (Audio_Seq_unusedScoredata.maxChain < 16) {
        Ending_finalScore += Audio_Seq_unusedScoredata.maxChain * 40;
    } else if (Audio_Seq_unusedScoredata.maxChain < 21) {
        Ending_finalScore += Audio_Seq_unusedScoredata.maxChain * 80;
    } else if (Audio_Seq_unusedScoredata.maxChain < 26) {
        Ending_finalScore += Audio_Seq_unusedScoredata.maxChain * 160;
    } else if (Audio_Seq_unusedScoredata.maxChain < 31) {
        Ending_finalScore += Audio_Seq_unusedScoredata.maxChain * 320;
    } else if (Audio_Seq_unusedScoredata.maxChain < 51) {
        Ending_finalScore += Audio_Seq_unusedScoredata.maxChain * 640;
    } else {
        Ending_finalScore += Audio_Seq_unusedScoredata.maxChain * 1280;
    }

    Ending_finalScore += Ending_clearCount * 100000;

    if ((Ending_clearCount != 0) && (Audio_Seq_unusedScoredata.completionTimeMinutes < 600)) {
        if (Audio_Seq_unusedScoredata.completionTimeMinutes >= 540) {
            Ending_finalScore += Ending_finalScore / 4;
        } else if (Audio_Seq_unusedScoredata.completionTimeMinutes >= 300) {
            Ending_finalScore += Ending_finalScore / 2;
        } else {
            Ending_finalScore *= 2;
        }
    }

    Ending_finalScore += Audio_Seq_unusedScoredata.streakScore;
    Ending_finalScore += Audio_Seq_unusedScoredata.miscScore;

    if (Ending_finalScore > 999999999) {
        Ending_finalScore = 999999999;
    }
}

