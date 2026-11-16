// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/MusicLive.hpp"
#include "Audio/SfxLive.hpp"
#include "Content/CdDriveLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Results/GameClearScoreLive.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/ResultsRankLive.hpp"
#include "Menu/Results/ResultsStatsLive.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "Render/TimImageLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "Battle/StatusEffects.h"
#include "DiscQueue.h"
#include "Field/MapProgress.h"
#include "Input/ControllerLiveState.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/Display.h"
#include "Render/Images.h"
#include "Render/Render.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

// decomp: _buffReelIndex (_buffReelIndex@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_buffReelIndex;

// decomp: _buffReelSelection (_buffReelSelection@src/MENU/MENUF.PRG/3B8.c)
u_int Menu_Results_buffReelSelection;

// decomp: _buffReels (_buffReels@src/MENU/MENUF.PRG/3B8.c)
Menu_Results_BuffReel Menu_Results_buffReels[][16] = {
    { { Menu_Results_BuffReelStat_Hp, 5 }, { Menu_Results_BuffReelStat_Hp, 3 }, { Menu_Results_BuffReelStat_Str, 2 },
        { Menu_Results_BuffReelStat_Agi, 1 }, { Menu_Results_BuffReelStat_Hp, 4 }, { Menu_Results_BuffReelStat_Int, 1 },
        { Menu_Results_BuffReelStat_Mp, 2 }, { Menu_Results_BuffReelStat_Hp, 3 }, { Menu_Results_BuffReelStat_Str, 2 },
        { Menu_Results_BuffReelStat_Mp, 1 }, { Menu_Results_BuffReelStat_Hp, 4 }, { Menu_Results_BuffReelStat_Hp, 4 },
        { Menu_Results_BuffReelStat_Str, 3 }, { Menu_Results_BuffReelStat_Hp, 3 }, { Menu_Results_BuffReelStat_Mp, 2 },
        { Menu_Results_BuffReelStat_Str, 1 } },
    { { Menu_Results_BuffReelStat_Mp, 1 }, { Menu_Results_BuffReelStat_Hp, 4 }, { Menu_Results_BuffReelStat_Int, 1 },
        { Menu_Results_BuffReelStat_Hp, 3 }, { Menu_Results_BuffReelStat_Hp, 5 }, { Menu_Results_BuffReelStat_Hp, 4 },
        { Menu_Results_BuffReelStat_Int, 2 }, { Menu_Results_BuffReelStat_Str, 1 }, { Menu_Results_BuffReelStat_Hp, 4 },
        { Menu_Results_BuffReelStat_Mp, 2 }, { Menu_Results_BuffReelStat_Agi, 1 }, { Menu_Results_BuffReelStat_Int, 3 },
        { Menu_Results_BuffReelStat_Hp, 4 }, { Menu_Results_BuffReelStat_Int, 2 }, { Menu_Results_BuffReelStat_Hp, 3 },
        { Menu_Results_BuffReelStat_Hp, 3 } },
    { { Menu_Results_BuffReelStat_Hp, 4 }, { Menu_Results_BuffReelStat_Int, 1 }, { Menu_Results_BuffReelStat_Agi, 2 },
        { Menu_Results_BuffReelStat_Hp, 4 }, { Menu_Results_BuffReelStat_Mp, 2 }, { Menu_Results_BuffReelStat_Agi, 1 },
        { Menu_Results_BuffReelStat_Mp, 1 }, { Menu_Results_BuffReelStat_Hp, 3 }, { Menu_Results_BuffReelStat_Agi, 3 },
        { Menu_Results_BuffReelStat_Hp, 4 }, { Menu_Results_BuffReelStat_Hp, 3 }, { Menu_Results_BuffReelStat_Hp, 5 },
        { Menu_Results_BuffReelStat_Agi, 2 }, { Menu_Results_BuffReelStat_Mp, 2 }, { Menu_Results_BuffReelStat_Hp, 3 },
        { Menu_Results_BuffReelStat_Str, 1 } }
};

// decomp: _clearCount (_clearCount@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_clearCount;

// decomp: _disMap (_disMap@src/MENU/MENUF.PRG/3B8.c)
Render_TextureRegion Menu_Results_disMap[127] = {
    { 0, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 14, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 28, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 42, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 56, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 70, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 84, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 98, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 112, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 126, 0, 14, 20, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 240, 16, 4, 8, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 148, 4, 60, 16, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 6, 24, 154, 16, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 4, 44, 38, 18, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 50, 44, 44, 18, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 98, 44, 38, 18, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 144, 44, 30, 18, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 182, 44, 34, 18, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 0, 64, 66, 22, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 104, 70, 12, 12, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 118, 70, 28, 14, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 0, 88, 170, 22, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 8, 118, 136, 24, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 8, 150, 134, 24, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 200, 82, 10, 10, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 216, 82, 8, 6, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 232, 82, 6, 12, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 30, 12, 66, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 98, 12, 70, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 170, 12, 76, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 70, 36, 72, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 144, 36, 102, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 78, 60, 70, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 152, 60, 94, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 38, 84, 80, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 118, 84, 122, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 46, 108, 118, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 166, 108, 74, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 78, 132, 94, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 172, 132, 74, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 82, 156, 86, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 170, 156, 76, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 60, 180, 96, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 158, 180, 88, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 120, 204, 54, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 178, 204, 68, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 56, 228, 114, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 172, 228, 72, 24, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 110, 12, 26, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 138, 12, 54, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 194, 12, 52, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 64, 36, 88, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 182, 36, 64, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 90, 60, 90, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 184, 60, 62, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 82, 84, 66, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 148, 84, 98, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 42, 108, 102, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 144, 108, 102, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 50, 132, 116, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 168, 132, 78, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 74, 156, 92, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 166, 156, 80, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 58, 180, 68, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 130, 180, 52, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 184, 180, 62, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 106, 204, 78, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 188, 204, 58, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 106, 228, 140, 24, getTPage(0, 0, 960, 256), getClut(768, 511) },
    { 146, 2, 88, 14, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 0, 24, 144, 24, getTPage(0, 0, 832, 256), getClut(864, 511) },
    { 0, 48, 144, 24, getTPage(0, 0, 832, 256), getClut(864, 511) },
    { 176, 78, 56, 18, getTPage(0, 0, 832, 256), getClut(800, 511) },
    { 144, 16, 8, 8, getTPage(0, 0, 832, 256), getClut(800, 511) },
    { 152, 16, 8, 8, getTPage(0, 0, 832, 256), getClut(800, 511) },
    { 192, 18, 32, 20, getTPage(0, 0, 832, 256), getClut(800, 511) },
    { 180, 38, 44, 20, getTPage(0, 0, 832, 256), getClut(800, 511) },
    { 184, 58, 40, 20, getTPage(0, 0, 832, 256), getClut(800, 511) },
    { 2, 72, 116, 20, getTPage(0, 0, 832, 256), getClut(832, 511) },
    { 0, 96, 70, 20, getTPage(0, 0, 832, 256), getClut(832, 511) },
    { 72, 96, 68, 20, getTPage(0, 0, 832, 256), getClut(832, 511) },
    { 122, 72, 46, 20, getTPage(0, 0, 832, 256), getClut(832, 511) },
    { 144, 96, 100, 20, getTPage(0, 0, 832, 256), getClut(832, 511) },
    { 238, 4, 12, 16, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 166, 20, 76, 18, getTPage(0, 0, 832, 256), getClut(832, 511) },
    { 4, 32, 140, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 4, 56, 114, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 4, 80, 204, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 4, 104, 198, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 4, 128, 232, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 4, 152, 204, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 4, 176, 114, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 122, 56, 100, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 4, 200, 176, 24, getTPage(0, 0, 832, 256), getClut(896, 511) },
    { 4, 227, 212, 28, getTPage(0, 0, 832, 256), getClut(864, 511) },
    { 146, 180, 100, 18, getTPage(0, 0, 832, 256), getClut(800, 511) },
    { 158, 14, 86, 106, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 50, 14, 92, 106, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 16, 14, 30, 106, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 30, 132, 204, 106, getTPage(0, 0, 896, 256), getClut(768, 511) },
    { 4, 227, 106, 28, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 110, 227, 106, 28, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 4, 120, 111, 30, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 115, 120, 111, 30, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 4, 150, 160, 24, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 8, 176, 170, 24, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 8, 200, 116, 24, getTPage(0, 0, 832, 256), getClut(768, 511) },
    { 8, 8, 170, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 28, 134, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 102, 48, 118, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 48, 94, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 142, 28, 88, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 68, 142, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 88, 144, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 108, 128, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 128, 98, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 136, 108, 104, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 148, 156, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 168, 174, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 106, 128, 88, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 188, 154, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 208, 156, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 8, 228, 194, 20, getTPage(0, 0, 896, 256), getClut(896, 511) },
    { 0, 0, 160, 24, getTPage(0, 0, 832, 256), getClut(864, 511) },
    { 0, 24, 160, 24, getTPage(0, 0, 832, 256), getClut(864, 511) },
    { 0, 48, 160, 24, getTPage(0, 0, 832, 256), getClut(832, 511) },
    { 0, 72, 160, 24, getTPage(0, 0, 832, 256), getClut(832, 511) },
};

// decomp: _elementAnimationState (_elementAnimationState@src/MENU/MENUF.PRG/3B8.c)
u_int Menu_Results_elementAnimationState;

// decomp: _incrementingMapCompletion (_incrementingMapCompletion@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_incrementingMapCompletion;

// decomp: _incrementingScore (_incrementingScore@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_incrementingScore;

// decomp: _mapCompletion (_mapCompletion@src/MENU/MENUF.PRG/3B8.c)
u_int Menu_Results_mapCompletion;

// decomp: _onScreenMapCompletion (_onScreenMapCompletion@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_onScreenMapCompletion;

// decomp: _onScreenScore (_onScreenScore@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_onScreenScore;

// decomp: _rank (_rank@src/MENU/MENUF.PRG/3B8.c)
int Menu_Results_rank;

// decomp: _riskbreakerRanks (_riskbreakerRanks@src/MENU/MENUF.PRG/3B8.c)
u_char Menu_Results_riskbreakerRanks[][4] = { { 3, Menu_Results_DisIndex_Rank1Grand, Menu_Results_DisIndex_Rank1Master,
                                             Menu_Results_DisIndex_Rank1Breaker },
    { 2, Menu_Results_DisIndex_Rank1Grand, Menu_Results_DisIndex_Rank1Paladin },
    { 2, Menu_Results_DisIndex_Rank1Radiant, Menu_Results_DisIndex_Rank1Knight },
    { 2, Menu_Results_DisIndex_Rank1Raging, Menu_Results_DisIndex_Rank1Berserker },
    { 2, Menu_Results_DisIndex_Rank2Dragon, Menu_Results_DisIndex_Rank2Slayer },
    { 2, Menu_Results_DisIndex_Rank1Couragous, Menu_Results_DisIndex_Rank1Adventurer },
    { 2, Menu_Results_DisIndex_Rank1Master, Menu_Results_DisIndex_Rank1Gladiator },
    { 2, Menu_Results_DisIndex_Rank1Blade, Menu_Results_DisIndex_Rank1Master },
    { 2, Menu_Results_DisIndex_Rank2Mystic, Menu_Results_DisIndex_Rank2Wanderer }, { 1, Menu_Results_DisIndex_Rank1Paladin },
    { 1, Menu_Results_DisIndex_Rank2Spectrebane }, { 1, Menu_Results_DisIndex_Rank2Destroyer },
    { 1, Menu_Results_DisIndex_Rank1Berserker }, { 1, Menu_Results_DisIndex_Rank1DareDevil },
    { 1, Menu_Results_DisIndex_Rank1Gladiator }, { 2, Menu_Results_DisIndex_Rank2Normal, Menu_Results_DisIndex_Rank2Agent } };

// decomp: _score (_score@src/MENU/MENUF.PRG/3B8.c)
u_int Menu_Results_score;

// decomp: local static of _renderStatWheel, at file scope so the module reset reaches it
static CVECTOR Menu_Results_bonusReelColors[] = { { 128, 96, 64 }, { 200, 180, 160 } };

// decomp: MENUF.PRG vs_menuF_exec (src/MENU/MENUF.PRG/3B8.c:23)
int Menu_Results_UpdateScreen(u_char* state)
{
    switch (*state) {
    case 0:
        *state = 1;
        // Fallthrough
    case 1:
        *state += (Menu_Results_InitializeScreen() != 0);
        break;
    case 2:
        if (Menu_Results_UpdatePresentation() != 0) {
            *state = 0;
            return 1;
        }
        break;
    }
    return 0;
}

// decomp: MENUF.PRG _initMenu (src/MENU/MENUF.PRG/3B8.c:59)
int Menu_Results_InitializeScreen(void)
{
    int ret;

    switch (Audio_Seq_unusedStateFlags.menuFSubmenu) {
    case 0:
        ret = Menu_Results_InitializeGameClear();
        break;

    case 1:
        ret = Menu_Results_InitializeTimeAttackStart();
        break;

    case 2:
        ret = Menu_Results_InitializeTimeAttackResults();
        break;

    case 3:
        Menu_Results_ApplyHealingScoreBonus();
        VS::Menu::Framework::NoOperation();

        ret = 1;
        break;

    case 4:
        ret = Menu_Results_InitializeCubePuzzleStart();
        break;

    case 5:
        ret = Menu_Results_InitializeCubePuzzleResults();
        break;

    case 6:
        ret = Menu_Results_InitializeCubePuzzleQuit();
        break;
    }

    return ret;
}

// decomp: MENUF.PRG _initCongratulationsScreen (src/MENU/MENUF.PRG/3B8.c:118)
int Menu_Results_InitializeGameClear(void)
{
    static Content_DiscQueueSlot* Menu_Results_initializeGameClearCdSlot;
    static void* Menu_Results_initializeGameClearTimData;

    int i;

    if (Battle_unusedSubmenuState == 0) {

        VS::Render::ReserveLiveTexturePageRange(0x1D, 3, 5);

        Menu_Results_initializeGameClearTimData = VS::Core::Memory::AllocateLiveFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Rank].size);
        Menu_Results_initializeGameClearCdSlot = VS::Content::AllocateLiveDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Rank]);

        if (Menu_Results_initializeGameClearCdSlot != NULL) {
            VS::Content::EnqueueLiveDiscRequest(Menu_Results_initializeGameClearCdSlot, Menu_Results_initializeGameClearTimData);
        } else {
            Menu_Results_initializeGameClearCdSlot = VS::Content::CompleteLiveRefusedDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Rank], Menu_Results_initializeGameClearTimData);
        }

        ++Battle_unusedSubmenuState;

    } else if (Battle_unusedSubmenuState == 1) {
        if (Menu_Results_initializeGameClearCdSlot->state == Content_DiscRequestState_Loaded) {

            for (i = 0; i < 3; ++i) {
                TIM_IMAGE tim;

                VS::Render::ParseLiveTim((u_long *)(((u_char*)(Menu_Results_initializeGameClearTimData)) + i * 0x8220), &tim);

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
                        tim.crect->w = 128;
                        tim.crect->h = 1;
                        tim.caddr[0] = 0;

                        LoadImage(tim.crect, tim.caddr);
                    }
                }
            }

            VS::Content::ReleaseLiveDiscRequest(Menu_Results_initializeGameClearCdSlot);
            Menu_Results_QueueResultsMusic();

            ++Battle_unusedSubmenuState;
        }
    } else if (VS::Audio::LivePollMusicLoads() == 0) {
        int strength;
        int intelligence;
        int agility;
        int mapCompletion;
        u_int chestsOpened;

        VS::Audio::LivePlayMusicSlot(2, 0x7F, 0);
        VS::Core::Memory::FreeLiveFromHighEnd(Menu_Results_initializeGameClearTimData);

        Menu_Results_onScreenMapCompletion = 0;
        Menu_Results_onScreenScore = 0;
        Menu_Results_screenTimer = 0;
        Menu_Results_screenState = 0;
        Menu_Results_elementAnimationState = 0;
        Menu_Results_buffReelSelection = (Core_Random_Rand() & 0xF0) | 8;

        strength = VS::Menu::Results::AshleyLiveTotalStrength();
        intelligence = VS::Menu::Results::AshleyLiveTotalIntelligence();
        agility = VS::Menu::Results::AshleyLiveTotalAgility();

        if ((intelligence >= strength) && (agility >= strength)) {
            Menu_Results_buffReelIndex = 0;
        } else if (strength >= intelligence) {
            if (agility >= intelligence) {
                Menu_Results_buffReelIndex = 1;
            } else {
                Menu_Results_buffReelIndex = 2;
            }
        } else {
            Menu_Results_buffReelIndex = 2;
        }

        Battle_unusedSubmenuState = 0;

        Menu_Results_ApplyHealingScoreBonus();

        Menu_Results_clearCount = Audio_Seq_unusedStateFlags.clearCount;

        if (State_roomVisitProgress.roomFlags[1] & 0x800000) {
            State_roomVisitProgress.roomFlags[1] |= 0x400000;
        }

        mapCompletion = 0;

        for (i = 0; i < 16; ++i) {
            int flag;
            for (flag = 0; flag < 32; ++flag) {
                int v = 1;
                if (State_roomVisitProgress.roomFlags[i] & Field_countableRoomMask[i]
                    & (v << flag)) {
                    ++mapCompletion;
                }
            }
        }

        chestsOpened = 0;

        for (i = 0; i < 64; ++i) {
            if (Audio_Seq_unusedStateFlags.chestsOpened[i] != 0) {
                ++chestsOpened;
            }
        }

        if (Audio_Seq_unusedScoredata.mapCompletion < mapCompletion) {
            Audio_Seq_unusedScoredata.mapCompletion = mapCompletion;
        }
        if (Audio_Seq_unusedScoredata.openedChestCount < chestsOpened) {
            Audio_Seq_unusedScoredata.openedChestCount = chestsOpened;
        }

        Menu_Results_mapCompletion = (mapCompletion * 100) / 361;

        VS::Menu::Results::CalculateLiveScore();
        VS::Menu::Results::DetermineLiveRiskbreakerRank();

        return 1;
    }

    return 0;
}

// decomp: MENUF.PRG func_80103748 (src/MENU/MENUF.PRG/3B8.c:436)
void Menu_Results_QueueResultsMusic(void)
{
    if ((Audio_soundState.currentMusicId != 7)
        && (((Audio_soundState.currentMusicId - 31) < 9) || ((Audio_soundState.currentMusicId - 45) < 5))) {
        VS::Audio::LiveLoadMusicSlot(Audio_soundState.currentMusicId + 100, 2);
    } else {
        VS::Audio::LiveLoadMusicSlot(128, 2);
    }
}

// decomp: MENUF.PRG _execMenu (src/MENU/MENUF.PRG/3B8.c:453)
int Menu_Results_UpdatePresentation(void)
{
    int ret;

    switch (Audio_Seq_unusedStateFlags.menuFSubmenu) {
    case 0:
        ret = Menu_Results_UpdateGameClear();
        break;

    case 1:
        ret = Menu_Results_UpdateTimeAttackStart();
        break;

    case 2:
        ret = Menu_Results_UpdateTimeAttackResults();
        break;

    case 3:
        ret = 1;
        break;

    case 4:
        ret = Menu_Results_UpdateCubePuzzleStart();
        break;

    case 5:
        ret = Menu_Results_UpdateCubePuzzleResults();
        break;

    case 6:
        ret = Menu_Results_UpdateCubePuzzleQuit();
        break;
    }

    // BUG: potential garbage
    return ret;
}

// decomp: MENUF.PRG _renderCongratulationsScreen (src/MENU/MENUF.PRG/3B8.c:755)
int Menu_Results_UpdateGameClear(void)
{
    static int Menu_Results_gameClearStateTimer;

    if (Menu_Results_screenState != 3) {
        if (Menu_Results_screenTimer == 208) {
            VS::Audio::LivePlaySfxCentered(0x7E, 0x77);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x78);
        }

        VS::Menu::Results::LiveDrawCongratulations(160, 40, Menu_Results_screenTimer);
        VS::Menu::Results::LiveDrawCountingScore(160, 80, Menu_Results_screenTimer - 32, Menu_Results_screenState);
        VS::Menu::Results::LiveDrawCountingMapCompletion(160, 102, Menu_Results_screenTimer - 112, Menu_Results_screenState);
        VS::Menu::Results::LiveDrawRiskbreakerRankHeading(160, 138, Menu_Results_screenTimer - 192);
        VS::Menu::Results::LiveDrawRiskbreakerRank(160, 154, Menu_Results_screenTimer - 208);
        Menu_Results_DrawBonusReel(130, 190, Menu_Results_screenTimer - 272, Menu_Results_screenState);
    }

    if (Menu_Results_screenState == 0) {
        Menu_Results_buffReelSelection = (Menu_Results_buffReelSelection + (Menu_Results_elementAnimationState >> 3)) & 0xFF;
        if ((Menu_Results_screenTimer - 272) > 0) {

            if (Menu_Results_elementAnimationState == 8) {
                VS::Audio::LivePlaySfxCentered(0x7E, 0x74);
                VS::Audio::LivePlaySfxCentered(0x7E, 0x75);
            }

            if (Menu_Results_elementAnimationState < 48) {
                if (Menu_Results_screenTimer > 288) {
                    ++Menu_Results_elementAnimationState;

                    VS::Menu::Results::LiveDrawPressButtonPrompt(
                        214, 187, Menu_Results_elementAnimationState, Menu_Results_screenTimer);
                }
            } else {
                if ((Input_buttonsPressed.allButtons & PADRright) || Menu_Results_screenTimer > 2072) {

                    if (Input_buttonsPressed.allButtons & PADRright) {
                        VS::Audio::LivePlaySfxCentered(0x7E, 0x76);
                    }

                    Menu_Results_screenState = 1;
                    Menu_Results_gameClearStateTimer = 0;

                    VS::Audio::LiveFadeSfxVolume(0x7E, 0x74, 0x40, 0x30);
                    VS::Audio::LiveFadeSfxVolume(0x7E, 0x75, 0x40, 0x30);
                    VS::Audio::LiveFadeSfxPitch(0x7E, 0x74, -0x10, 0x30);
                    VS::Audio::LiveFadeSfxPitch(0x7E, 0x75, -0x10, 0x30);
                }

                VS::Menu::Results::LiveDrawPressButtonPrompt(214, 187, 48, Menu_Results_screenTimer);
            }
        } else if (Input_buttonsPressed.allButtons & PADRright) {

            VS::Audio::LiveStopSfx(0x7E, 0);

            Menu_Results_screenTimer = 272;
            Menu_Results_incrementingScore = 0;
            Menu_Results_incrementingMapCompletion = 0;
            Menu_Results_onScreenMapCompletion = Menu_Results_mapCompletion;
            Menu_Results_onScreenScore = Menu_Results_score;
        }
    } else if (Menu_Results_screenState == 1) {
        int updated_reel_phase;
        int prompt_slide_step = 48 - Menu_Results_gameClearStateTimer;

        if (prompt_slide_step > 0) {
            VS::Menu::Results::LiveDrawPressButtonPrompt(0xD6, 0xBB, prompt_slide_step, Menu_Results_screenTimer);
        }

        updated_reel_phase = Menu_Results_buffReelSelection + (Menu_Results_elementAnimationState >> 3);
        Menu_Results_buffReelSelection = updated_reel_phase & 0xFF;

        if (Menu_Results_elementAnimationState >= 9) {
            --Menu_Results_elementAnimationState;
        }

        updated_reel_phase &= 0xF;

        if ((updated_reel_phase == 8) && (Menu_Results_elementAnimationState == updated_reel_phase)) {

            VS::Audio::LiveStopSfx(0x7E, 0x74);
            VS::Audio::LiveStopSfx(0x7E, 0x75);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x77);
            VS::Audio::LivePlaySfxCentered(0x7E, 0x78);

            Menu_Results_screenState = 2;
            Menu_Results_gameClearStateTimer = 0;
        }
    } else if (Menu_Results_screenState == 2) {
        if ((Input_buttonsPressed.allButtons & PADRright) || (Menu_Results_gameClearStateTimer > 450)) {
            Menu_Results_screenState = 3;
            Menu_Results_gameClearStateTimer = 0;
        }
    } else {

        VS::Menu::Results::LiveDrawCongratulations(160, 40, 8 - Menu_Results_gameClearStateTimer);
        VS::Menu::Results::LiveDrawScore(160, 80, 8 - Menu_Results_gameClearStateTimer, 3);
        VS::Menu::Results::LiveDrawMapCompletion(160, 102, 8 - Menu_Results_gameClearStateTimer, 3);
        VS::Menu::Results::LiveDrawRiskbreakerRankHeading(160, 138, 8 - Menu_Results_gameClearStateTimer);
        VS::Menu::Results::LiveDrawRiskbreakerRank(160, 154, 8 - Menu_Results_gameClearStateTimer);
        Menu_Results_DrawBonusReel(130, 190, 8 - Menu_Results_gameClearStateTimer, 3);

        if (Menu_Results_gameClearStateTimer >= 8) {
            int amount = Menu_Results_buffReels[Menu_Results_buffReelIndex][(char)Menu_Results_buffReelSelection / 16].amount;
            switch ((short)(Menu_Results_buffReels[Menu_Results_buffReelIndex][(char)Menu_Results_buffReelSelection / 16].stat
                            - 13)) {
            case 0:
                VS::Menu::Results::ApplyLiveStrengthBonus(amount);
                break;

            case 1:
                VS::Menu::Results::ApplyLiveAgilityBonus(amount);
                break;

            case 2:
                VS::Menu::Results::ApplyLiveIntelligenceBonus(amount);
                break;

            case 3:
                VS::Menu::Results::ApplyLiveHpBonus(amount);
                break;

            case 4:
                VS::Menu::Results::ApplyLiveMpBonus(amount);
                break;
            }

            VS::Render::RetireLiveTexturePageRange(0x1D, 3, 5);
            VS::Menu::Framework::NoOperation();

            return 1;
        }
    }

    ++Menu_Results_gameClearStateTimer;
    ++Menu_Results_screenTimer;
    return 0;
}

// decomp: MENUF.PRG _renderStatWheel (src/MENU/MENUF.PRG/3B8.c:1996)
void Menu_Results_DrawBonusReel(int x, int y, int timer, int results_screen_state)
{

    RECT reel_clip_rect;
    char buf[8];
    DR_AREA* area;
    int i;
    int j;
    void** p;
    void** q;
    int reel_base_clip_height;
    int reel_entry_index;
    int len;
    int temp;

    if (timer < 0) {
        timer = 0;
    }
    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {

        Menu_Results_bonusReelColors[0].cd = timer;
        x -= (Menu_Results_disMap[11].w + Menu_Results_disMap[26].w + Menu_Results_disMap[14].w + Menu_Results_disMap[24].w + 0xE) >> 1;

        VS::Render::DrawLiveTextureTintFade(Menu_Results_disMap, x, y, Menu_Results_DisIndex_Rank0Bonus, Menu_Results_bonusReelColors);

        x += Menu_Results_disMap[11].w;

        VS::Render::DrawLiveTextureTintFade(Menu_Results_disMap, x, y, Menu_Results_DisIndex_Rank0Colon, Menu_Results_bonusReelColors);

        x += 2 + Menu_Results_disMap[26].w;
        reel_base_clip_height = 16;
        p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        area = (DR_AREA *)(p[0]);

        SetDrawArea(area, &Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip);
        AddPrim(((u_char*)(p[1])) - 0x1C, area++);

        p[0] = area;

        if (Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip.x >= 320) {
            reel_clip_rect.x = 320;
        } else {
            reel_clip_rect.x = 0;
        }

        reel_clip_rect.y = y - (Menu_Results_elementAnimationState >> 3);
        reel_clip_rect.w = 320;
        reel_clip_rect.h = ((Menu_Results_elementAnimationState >> 3) * 2) + reel_base_clip_height;

        if (results_screen_state < 2) {
            int reel_origin_x = x;
            int reel_row_offset;
            y += 8 + (Menu_Results_buffReelSelection & 0xF);

            for (i = 0; i < 3; ++i) {
                x = reel_origin_x;
                reel_row_offset = i - 1;
                reel_entry_index = (Menu_Results_buffReelSelection / 16);
                reel_entry_index += reel_row_offset;
                temp = reel_entry_index & 0xF;

                Menu_Results_DrawBonusReelTexture(
                    x, y - 1, Menu_Results_buffReels[Menu_Results_buffReelIndex][reel_entry_index & 0xF].stat);

                x += Menu_Results_disMap[Menu_Results_buffReels[Menu_Results_buffReelIndex][reel_entry_index & 0xF].stat].w;

                Menu_Results_DrawBonusReelTexture(x, y + 4, Menu_Results_DisIndex_Rank0Plus);

                x += Menu_Results_disMap[24].w;

                sprintf(buf, "%d", Menu_Results_buffReels[Menu_Results_buffReelIndex][reel_entry_index & 0xF].amount);

                len = strlen(buf);

                for (j = 0; j < len; ++j) {
                    Menu_Results_DrawBonusReelTexture(x, y - 1, buf[j] - 0x30);
                    x += 0xC;
                }

                y -= 16;
            }

        } else {
            temp = (char)Menu_Results_buffReelSelection / 16;

            VS::Menu::Results::LiveDrawFadingTextureClut(x, y - 1, Menu_Results_buffReels[Menu_Results_buffReelIndex][temp].stat,
                timer, getClut(800, 511));

            x += Menu_Results_disMap[Menu_Results_buffReels[Menu_Results_buffReelIndex][temp].stat].w;

            VS::Menu::Results::LiveDrawFadingTextureClut(
                x, y + 4, Menu_Results_DisIndex_Rank0Plus, timer, getClut(800, 511));

            x += Menu_Results_disMap[24].w;

            sprintf(buf, "%d", Menu_Results_buffReels[Menu_Results_buffReelIndex][temp].amount);

            len = strlen(buf);

            for (j = 0; j < len; ++j) {
                VS::Menu::Results::LiveDrawFadingTextureClut(
                    x, y - 1, buf[j] - '0', timer, getClut(800, 511));
                x += 0xC;
            }
        }

        q = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        area = (DR_AREA *)(q[0]);

        SetDrawArea(area, &reel_clip_rect);
        AddPrim(((u_char*)(q[1])) - 0x1C, area++);

        q[0] = area;
    }
}

// decomp: MENUF.PRG _renderAnimatedStatWheel (src/MENU/MENUF.PRG/3B8.c:2168)
void Menu_Results_DrawBonusReelTexture(int x, int y, int texId)
{
    int i;
    int brightness;
    void** p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    POLY_FT4* poly = (POLY_FT4 *)(p[0]);
    int texY = Menu_Results_disMap[texId].y;

    for (i = 0; i < Menu_Results_disMap[texId].h; ++i, ++y, ++texY) {

        setPolyFT4(poly);
        setXY4(
            poly, x, y, Menu_Results_disMap[texId].w + x, y, x, y + 1, Menu_Results_disMap[texId].w + x, y + 1);
        setUV4(poly, Menu_Results_disMap[texId].x, texY, Menu_Results_disMap[texId].x + Menu_Results_disMap[texId].w, texY,
            Menu_Results_disMap[texId].x, texY + 1, Menu_Results_disMap[texId].x + Menu_Results_disMap[texId].w, texY + 1);

        if (y < 190) {
            brightness = 128 - ((189 - y) * 16);

            if (brightness < 0) {
                brightness = 0;
            }

            setRGB0(poly, brightness, brightness, brightness);
            setSemiTrans(poly, 1);
            setClut(poly, 816, 511);
            poly->tpage = Menu_Results_disMap[texId].tpage | getTPage(0, Render_BlendMode_Add, 0, 0);

            AddPrim(((u_char*)(p[1])) - 0x1C, poly++);

            setPolyFT4(poly);
            setXY4(poly, x, y, Menu_Results_disMap[texId].w + x, y, x, y + 1, Menu_Results_disMap[texId].w + x,
                y + 1);
            setUV4(poly, Menu_Results_disMap[texId].x, texY, Menu_Results_disMap[texId].x + Menu_Results_disMap[texId].w,
                texY, Menu_Results_disMap[texId].x, texY + 1, Menu_Results_disMap[texId].x + Menu_Results_disMap[texId].w,
                texY + 1);

            setRGB0(poly, brightness, brightness, brightness);
            setSemiTrans(poly, 1);
            setClut(poly, 816, 511);
            poly->tpage =
                Menu_Results_disMap[texId].tpage | getTPage(0, Render_BlendMode_Subtract, 0, 0);

            AddPrim(((u_char*)(p[1])) - 0x1C, poly++);

        } else if (y >= 206) {

            brightness = 128 - ((y - 206) * 16);

            if (brightness < 0) {
                brightness = 0;
            }

            setRGB0(poly, brightness, brightness, brightness);
            setSemiTrans(poly, 1);
            setClut(poly, 816, 511);
            poly->tpage = Menu_Results_disMap[texId].tpage | getTPage(0, Render_BlendMode_Add, 0, 0);

            AddPrim(((u_char*)(p[1])) - 0x1C, poly++);

            setPolyFT4(poly);
            setXY4(poly, x, y, Menu_Results_disMap[texId].w + x, y, x, y + 1, Menu_Results_disMap[texId].w + x,
                y + 1);
            setUV4(poly, Menu_Results_disMap[texId].x, texY, Menu_Results_disMap[texId].x + Menu_Results_disMap[texId].w,
                texY, Menu_Results_disMap[texId].x, texY + 1, Menu_Results_disMap[texId].x + Menu_Results_disMap[texId].w,
                texY + 1);
            setRGB0(poly, brightness, brightness, brightness);
            setSemiTrans(poly, 1);
            setClut(poly, 816, 511);
            poly->tpage =
                Menu_Results_disMap[texId].tpage | getTPage(0, Render_BlendMode_Subtract, 0, 0);

            AddPrim(((u_char*)(p[1])) - 0x1C, poly++);

        } else {
            setRGB0(poly, 128, 128, 128);
            setSemiTrans(poly, 1);
            setClut(poly, 800, 511);
            poly->tpage = Menu_Results_disMap[texId].tpage;

            AddPrim(((u_char*)(p[1])) - 0x1C, poly++);
        }
    }

    p[0] = poly;
}

// decomp: MENUF.PRG _addHealBonus (src/MENU/MENUF.PRG/3B8.c:2865)
void Menu_Results_ApplyHealingScoreBonus(void)
{
    int score_before_healing_penalty = Audio_Seq_unusedScoredata.miscScore + 100000;

    Audio_Seq_unusedScoredata.miscScore = score_before_healing_penalty - (Audio_Seq_unusedScoredata.healCount * 100);

    if (Audio_Seq_unusedScoredata.miscScore > 999999999) {
        Audio_Seq_unusedScoredata.miscScore = 999999999;
    }

    Audio_Seq_unusedScoredata.healCount = 0;
}

// MENUF.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Results_ResetMenufStatics(void)
{
    {
        static const CVECTOR initial[] = { { 128, 96, 64 }, { 200, 180, 160 } };
        memcpy((void*)&Menu_Results_bonusReelColors, &initial, sizeof Menu_Results_bonusReelColors);
    }
}

}  // extern "C"
