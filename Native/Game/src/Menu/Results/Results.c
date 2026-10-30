// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Music.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "CdDrive.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
#include "Field/MapProgress.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/TimeAttack.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/TextureRegionDraw.h"
#include "Render/Textures.h"
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
Render_TextureRegion Menu_Results_disMap[] = {
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

// decomp: local static of _renderCongratulations, at file scope so the module reset reaches it
static CVECTOR Menu_Results_congratulationsColors[] = { { 128, 96, 64 }, { 200, 180, 160 }, { 128, 96, 64 } };

// decomp: local static of _renderIncrementalMapCompletion, at file scope so the module reset reaches it
static CVECTOR Menu_Results_countingMapCompletionColors[] = { { 100, 180, 220 }, { 100, 180, 220 } };

// decomp: local static of _renderIncrementalScore, at file scope so the module reset reaches it
static CVECTOR Menu_Results_countingScoreColors[] = { { 100, 180, 220 }, { 100, 180, 220 } };

// decomp: local static of _renderMapCompletion, at file scope so the module reset reaches it
static CVECTOR Menu_Results_mapCompletionColors[] = { { 100, 180, 220 }, { 100, 180, 220 } };

// decomp: local static of _renderRiskbreakerRank, at file scope so the module reset reaches it
static CVECTOR Menu_Results_riskbreakerRankColors[] = { { 128, 128, 128 }, { 128, 128, 128 } };

// decomp: local static of _renderRiskbreakerRankHeader, at file scope so the module reset reaches it
static CVECTOR Menu_Results_riskbreakerRankHeadingColors[] = { { 200, 80, 20 }, { 200, 80, 20 } };

// decomp: local static of _renderScore, at file scope so the module reset reaches it
static CVECTOR Menu_Results_scoreColors[] = { { 100, 180, 220 }, { 100, 180, 220 } };

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
        Menu_Framework_NoOperation();

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

        Render_ReserveTexturePageRange(0x1D, 3, 5);

        Menu_Results_initializeGameClearTimData = Core_Memory_AllocateFromHighEnd(Menu_Results_kDisFiles[Menu_Results_DisFile_Rank].size);
        Menu_Results_initializeGameClearCdSlot = Content_AllocateDiscRequest(&Menu_Results_kDisFiles[Menu_Results_DisFile_Rank]);

        Content_EnqueueDiscRequest(Menu_Results_initializeGameClearCdSlot, Menu_Results_initializeGameClearTimData);

        ++Battle_unusedSubmenuState;

    } else if (Battle_unusedSubmenuState == 1) {
        if (Menu_Results_initializeGameClearCdSlot->state == Content_DiscRequestState_Loaded) {

            for (i = 0; i < 3; ++i) {
                TIM_IMAGE tim;

                Render_ParseTextureImage(Menu_Results_initializeGameClearTimData + i * 0x8220, &tim);

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

            Content_ReleaseDiscRequest(Menu_Results_initializeGameClearCdSlot);
            Menu_Results_QueueResultsMusic();

            ++Battle_unusedSubmenuState;
        }
    } else if (Audio_PollMusicLoads() == 0) {
        int strength;
        int intelligence;
        int agility;
        int mapCompletion;
        u_int chestsOpened;

        Audio_PlayMusicSlot(2, 0x7F, 0);
        Core_Memory_FreeFromHighEnd(Menu_Results_initializeGameClearTimData);

        Menu_Results_onScreenMapCompletion = 0;
        Menu_Results_onScreenScore = 0;
        Menu_Results_screenTimer = 0;
        Menu_Results_screenState = 0;
        Menu_Results_elementAnimationState = 0;
        Menu_Results_buffReelSelection = (Core_Random_Rand() & 0xF0) | 8;

        strength = Menu_Results_AshleyTotalStrength();
        intelligence = Menu_Results_AshleyTotalIntelligence();
        agility = Menu_Results_AshleyTotalAgility();

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

        Menu_Results_CalculateScore();
        Menu_Results_DetermineRiskbreakerRank();

        return 1;
    }

    return 0;
}

// decomp: MENUF.PRG func_80103748 (src/MENU/MENUF.PRG/3B8.c:436)
void Menu_Results_QueueResultsMusic(void)
{
    if ((Audio_soundState.currentMusicId != 7)
        && (((Audio_soundState.currentMusicId - 31) < 9) || ((Audio_soundState.currentMusicId - 45) < 5))) {
        Audio_LoadMusicSlot(Audio_soundState.currentMusicId + 100, 2);
    } else {
        Audio_LoadMusicSlot(128, 2);
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
            Audio_PlaySfxCentered(0x7E, 0x77);
            Audio_PlaySfxCentered(0x7E, 0x78);
        }

        Menu_Results_DrawCongratulations(160, 40, Menu_Results_screenTimer);
        Menu_Results_DrawCountingScore(160, 80, Menu_Results_screenTimer - 32, Menu_Results_screenState);
        Menu_Results_DrawCountingMapCompletion(160, 102, Menu_Results_screenTimer - 112, Menu_Results_screenState);
        Menu_Results_DrawRiskbreakerRankHeading(160, 138, Menu_Results_screenTimer - 192);
        Menu_Results_DrawRiskbreakerRank(160, 154, Menu_Results_screenTimer - 208);
        Menu_Results_DrawBonusReel(130, 190, Menu_Results_screenTimer - 272, Menu_Results_screenState);
    }

    if (Menu_Results_screenState == 0) {
        Menu_Results_buffReelSelection = (Menu_Results_buffReelSelection + (Menu_Results_elementAnimationState >> 3)) & 0xFF;
        if ((Menu_Results_screenTimer - 272) > 0) {

            if (Menu_Results_elementAnimationState == 8) {
                Audio_PlaySfxCentered(0x7E, 0x74);
                Audio_PlaySfxCentered(0x7E, 0x75);
            }

            if (Menu_Results_elementAnimationState < 48) {
                if (Menu_Results_screenTimer > 288) {
                    ++Menu_Results_elementAnimationState;

                    Menu_Results_DrawPressButtonPrompt(
                        214, 187, Menu_Results_elementAnimationState, Menu_Results_screenTimer);
                }
            } else {
                if ((Input_buttonsPressed.allButtons & PADRright) || Menu_Results_screenTimer > 2072) {

                    if (Input_buttonsPressed.allButtons & PADRright) {
                        Audio_PlaySfxCentered(0x7E, 0x76);
                    }

                    Menu_Results_screenState = 1;
                    Menu_Results_gameClearStateTimer = 0;

                    Audio_FadeSfxVolume(0x7E, 0x74, 0x40, 0x30);
                    Audio_FadeSfxVolume(0x7E, 0x75, 0x40, 0x30);
                    Audio_FadeSfxPitch(0x7E, 0x74, -0x10, 0x30);
                    Audio_FadeSfxPitch(0x7E, 0x75, -0x10, 0x30);
                }

                Menu_Results_DrawPressButtonPrompt(214, 187, 48, Menu_Results_screenTimer);
            }
        } else if (Input_buttonsPressed.allButtons & PADRright) {

            Audio_StopSfx(0x7E, 0);

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
            Menu_Results_DrawPressButtonPrompt(0xD6, 0xBB, prompt_slide_step, Menu_Results_screenTimer);
        }

        updated_reel_phase = Menu_Results_buffReelSelection + (Menu_Results_elementAnimationState >> 3);
        Menu_Results_buffReelSelection = updated_reel_phase & 0xFF;

        if (Menu_Results_elementAnimationState >= 9) {
            --Menu_Results_elementAnimationState;
        }

        updated_reel_phase &= 0xF;

        if ((updated_reel_phase == 8) && (Menu_Results_elementAnimationState == updated_reel_phase)) {

            Audio_StopSfx(0x7E, 0x74);
            Audio_StopSfx(0x7E, 0x75);
            Audio_PlaySfxCentered(0x7E, 0x77);
            Audio_PlaySfxCentered(0x7E, 0x78);

            Menu_Results_screenState = 2;
            Menu_Results_gameClearStateTimer = 0;
        }
    } else if (Menu_Results_screenState == 2) {
        if ((Input_buttonsPressed.allButtons & PADRright) || (Menu_Results_gameClearStateTimer > 450)) {
            Menu_Results_screenState = 3;
            Menu_Results_gameClearStateTimer = 0;
        }
    } else {

        Menu_Results_DrawCongratulations(160, 40, 8 - Menu_Results_gameClearStateTimer);
        Menu_Results_DrawScore(160, 80, 8 - Menu_Results_gameClearStateTimer, 3);
        Menu_Results_DrawMapCompletion(160, 102, 8 - Menu_Results_gameClearStateTimer, 3);
        Menu_Results_DrawRiskbreakerRankHeading(160, 138, 8 - Menu_Results_gameClearStateTimer);
        Menu_Results_DrawRiskbreakerRank(160, 154, 8 - Menu_Results_gameClearStateTimer);
        Menu_Results_DrawBonusReel(130, 190, 8 - Menu_Results_gameClearStateTimer, 3);

        if (Menu_Results_gameClearStateTimer >= 8) {
            int amount = Menu_Results_buffReels[Menu_Results_buffReelIndex][(char)Menu_Results_buffReelSelection / 16].amount;
            switch ((short)(Menu_Results_buffReels[Menu_Results_buffReelIndex][(char)Menu_Results_buffReelSelection / 16].stat
                            - 13)) {
            case 0:
                Menu_Results_ApplyStrengthBonus(amount);
                break;

            case 1:
                Menu_Results_ApplyAgilityBonus(amount);
                break;

            case 2:
                Menu_Results_ApplyIntelligenceBonus(amount);
                break;

            case 3:
                Menu_Results_ApplyHpBonus(amount);
                break;

            case 4:
                Menu_Results_ApplyMpBonus(amount);
                break;
            }

            Render_RetireTexturePageRange(0x1D, 3, 5);
            Menu_Framework_NoOperation();

            return 1;
        }
    }

    ++Menu_Results_gameClearStateTimer;
    ++Menu_Results_screenTimer;
    return 0;
}

// decomp: MENUF.PRG _renderCongratulations (src/MENU/MENUF.PRG/3B8.c:1332)
void Menu_Results_DrawCongratulations(int x, int y, int timer)
{

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_congratulationsColors[0].cd = timer;
        Menu_Results_congratulationsColors[1].cd = timer;
        x -= (Menu_Results_disMap[Menu_Results_DisIndex_Rank0Congratulations].w
                 + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Congratulations + 1].w)
          >> 1;

        Render_DrawTextureFlashFade(Menu_Results_disMap, x, y, Menu_Results_DisIndex_Rank0Congratulations, Menu_Results_congratulationsColors);
        Render_DrawTextureFlashFade(Menu_Results_disMap, x + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Congratulations].w, y,
            Menu_Results_DisIndex_Rank0Congratulations + 1, &Menu_Results_congratulationsColors[1]);
    }
}

// decomp: MENUF.PRG _renderScore (src/MENU/MENUF.PRG/3B8.c:1357)
void Menu_Results_DrawScore(int x, int y, int timer, int arg3 __attribute__((unused)))
{

    char buf[16];
    int i;
    int score_label_and_colon_width;

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_scoreColors[0].cd = timer;

        sprintf(buf, "%09d", Menu_Results_score);

        score_label_and_colon_width = Menu_Results_disMap[Menu_Results_DisIndex_Rank0Score].w + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Colon].w;
        x -= (((Menu_Results_disMap[Menu_Results_DisIndex_Rank0Comma].w * 2)
                  + (score_label_and_colon_width + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Pts].w))
                 + 116)
          >> 1;

        Render_DrawTextureTintFade(Menu_Results_disMap, x, y, Menu_Results_DisIndex_Rank0Score, Menu_Results_scoreColors);

        x += Menu_Results_disMap[Menu_Results_DisIndex_Rank0Score].w;

        Render_DrawTextureTintFade(Menu_Results_disMap, x, y + 7, Menu_Results_DisIndex_Rank0Colon, Menu_Results_scoreColors);

        i = 2;
        x = (x + i) + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Colon].w;

        for (i = 0; i < 9; ++i) {
            Render_DrawTextureTintFade(Menu_Results_disMap, x, y + 3, buf[i] - '0', Menu_Results_scoreColors);

            x += 0xC;

            if ((i == 2) || (i == 5)) {
                Render_DrawTextureTintFade(Menu_Results_disMap, x, y + 14, 10, Menu_Results_scoreColors);

                x += Menu_Results_disMap[Menu_Results_DisIndex_Rank0Comma].w + 3;
            }
        }

        Render_DrawTextureTintFade(Menu_Results_disMap, x, y + 8, Menu_Results_DisIndex_Rank0Pts, Menu_Results_scoreColors);
    }
}

// decomp: MENUF.PRG _renderIncrementalScore (src/MENU/MENUF.PRG/3B8.c:1409)
void Menu_Results_DrawCountingScore(int x, int y, int timer, int arg3 __attribute__((unused)))
{

    char buf[16];
    int wipe_edge_x;
    int i;
    int v;

    if (timer < 0) {
        timer = 0;
    }
    if (timer > 64) {
        timer = 64;
    }
    if (timer <= 0) {
        return;
    }

    wipe_edge_x = x + (timer * 8);

    if ((Menu_Results_score != 0) && (timer >= 32)) {

        if (Menu_Results_onScreenScore == 0) {

            Menu_Results_incrementingScore = 1;

            Audio_PlaySfxCentered(0x7E, 0x72);
        }

        if ((Menu_Results_onScreenScore == Menu_Results_score) && (Menu_Results_incrementingScore != 0)) {

            Menu_Results_incrementingScore = 0;

            Audio_StopSfx(0x7E, 0x72);
            Audio_PlaySfxCentered(0x7E, 0x73);
        }
    }

    if (timer >= 32) {
        Menu_Results_onScreenScore = (Menu_Results_score >> 5) * (timer - 32);
    }

    if (timer == 64) {
        Menu_Results_onScreenScore = Menu_Results_score;
    }

    sprintf(buf, "%09d", Menu_Results_onScreenScore);

    v = Menu_Results_disMap[Menu_Results_DisIndex_Rank0Score].w + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Colon].w
      + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Pts].w;
    x -= (((Menu_Results_disMap[Menu_Results_DisIndex_Rank0Comma].w * 2) + v) + 116) >> 1;

    Render_DrawTextureWipe(Menu_Results_disMap, x, y, Menu_Results_DisIndex_Rank0Score, Menu_Results_countingScoreColors, wipe_edge_x);

    x += Menu_Results_disMap[Menu_Results_DisIndex_Rank0Score].w;

    Render_DrawTextureWipe(Menu_Results_disMap, x, y + 7, Menu_Results_DisIndex_Rank0Colon, Menu_Results_countingScoreColors, wipe_edge_x);

    i = 2;
    x = x + i + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Colon].w;

    for (i = 0; i < 9; ++i) {
        Render_DrawTextureWipe(Menu_Results_disMap, x, y + 3, buf[i] - '0', Menu_Results_countingScoreColors, wipe_edge_x);

        x += 12;

        if ((i == 2) || (i == 5)) {
            Render_DrawTextureWipe(Menu_Results_disMap, x, y + 14, Menu_Results_DisIndex_Rank0Comma, Menu_Results_countingScoreColors, wipe_edge_x);

            x += 3 + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Comma].w;
        }
    }

    Render_DrawTextureWipe(Menu_Results_disMap, x, y + 8, 0x14, Menu_Results_countingScoreColors, wipe_edge_x);
}

// decomp: MENUF.PRG _renderMapCompletion (src/MENU/MENUF.PRG/3B8.c:1486)
void Menu_Results_DrawMapCompletion(int x, int y, int timer, int arg3 __attribute__((unused)))
{

    char buf[4];
    int i;

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_mapCompletionColors[0].cd = timer;

        sprintf(buf, "%03d", Menu_Results_mapCompletion);

        x -= (Menu_Results_disMap[21].w + Menu_Results_disMap[26].w + Menu_Results_disMap[19].w + 0x26) >> 1;
        i = 2;

        Render_DrawTextureTintFade(Menu_Results_disMap, x, y, 0x15, Menu_Results_mapCompletionColors);

        x += Menu_Results_disMap[21].w;

        Render_DrawTextureTintFade(Menu_Results_disMap, x, y + 7, 0x1A, Menu_Results_mapCompletionColors);

        x = x + i + Menu_Results_disMap[26].w;

        for (i = 0; i < 3; ++i) {
            Render_DrawTextureTintFade(Menu_Results_disMap, x, y + 3, buf[i] - '0', Menu_Results_mapCompletionColors);

            x += 0xC;
        }

        Render_DrawTextureTintFade(Menu_Results_disMap, x, y + 8, 0x13, Menu_Results_mapCompletionColors);
    }
}

// decomp: MENUF.PRG _renderIncrementalMapCompletion (src/MENU/MENUF.PRG/3B8.c:1527)
void Menu_Results_DrawCountingMapCompletion(
    int draw_x, int draw_y, int animation_timer, int arg3 __attribute__((unused)))
{

    char buf[8];
    int wipe_edge_x;
    int i;

    if (animation_timer < 0) {
        animation_timer = 0;
    }

    if (animation_timer > 64) {
        animation_timer = 64;
    }

    if (animation_timer <= 0) {
        return;
    }

    wipe_edge_x = draw_x + (animation_timer * 8);

    if ((Menu_Results_mapCompletion != 0) && (animation_timer >= 32)) {

        if (Menu_Results_onScreenMapCompletion == 0) {
            Menu_Results_incrementingMapCompletion = 1;

            Audio_PlaySfxCentered(0x7E, 0x72);
        }

        if ((Menu_Results_onScreenMapCompletion == Menu_Results_mapCompletion)
            && (Menu_Results_incrementingMapCompletion != 0)) {
            Menu_Results_incrementingMapCompletion = 0;

            Audio_StopSfx(0x7E, 0x72);
            Audio_PlaySfxCentered(0x7E, 0x73);
        }
    }

    if (animation_timer >= 32) {
        if (Menu_Results_mapCompletion >= 32) {
            Menu_Results_onScreenMapCompletion = (Menu_Results_mapCompletion * (animation_timer - 32)) >> 5;
        } else if (Menu_Results_onScreenMapCompletion < Menu_Results_mapCompletion) {
            ++Menu_Results_onScreenMapCompletion;
        }
    }

    sprintf(buf, "%03d", Menu_Results_onScreenMapCompletion);

    draw_x -= (Menu_Results_disMap[Menu_Results_DisIndex_Rank0MapCompleted].w + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Colon].w
                + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Percent].w + 38)
         >> 1;

    Render_DrawTextureWipe(Menu_Results_disMap, draw_x, draw_y, Menu_Results_DisIndex_Rank0MapCompleted, Menu_Results_countingMapCompletionColors, wipe_edge_x);

    draw_x += Menu_Results_disMap[Menu_Results_DisIndex_Rank0MapCompleted].w;
    i = 2;

    Render_DrawTextureWipe(Menu_Results_disMap, draw_x, draw_y + 7, Menu_Results_DisIndex_Rank0Colon, Menu_Results_countingMapCompletionColors, wipe_edge_x);
    draw_x = draw_x + i + Menu_Results_disMap[Menu_Results_DisIndex_Rank0Colon].w;

    for (i = 0; i < 3; ++i) {
        Render_DrawTextureWipe(Menu_Results_disMap, draw_x, draw_y + 3, buf[i] - '0', Menu_Results_countingMapCompletionColors, wipe_edge_x);

        draw_x += 12;
    }

    Render_DrawTextureWipe(Menu_Results_disMap, draw_x, draw_y + 8, Menu_Results_DisIndex_Rank0Percent, Menu_Results_countingMapCompletionColors, wipe_edge_x);
}

// decomp: MENUF.PRG _renderRiskbreakerRankHeader (src/MENU/MENUF.PRG/3B8.c:1598)
void Menu_Results_DrawRiskbreakerRankHeading(int x, int y, int timer)
{

    if (timer < 0) {
        timer = 0;
    }

    if (timer > 64) {
        timer = 64;
    }

    if (timer > 0) {
        Menu_Results_riskbreakerRankHeadingColors[0].cd = timer;

        Render_DrawTextureTintFade(Menu_Results_disMap, x - (Menu_Results_disMap[Menu_Results_DisIndex_Rank0RiskbreakerRank].w >> 1), y,
            Menu_Results_DisIndex_Rank0RiskbreakerRank, Menu_Results_riskbreakerRankHeadingColors);
    }
}

// decomp: MENUF.PRG _renderRiskbreakerRank (src/MENU/MENUF.PRG/3B8.c:1618)
void Menu_Results_DrawRiskbreakerRank(int x, int y, int timer)
{

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

        Menu_Results_riskbreakerRankColors[0].cd = timer;

        for (i = 0; i < Menu_Results_riskbreakerRanks[Menu_Results_rank][0]; ++i) {
            xInset += Menu_Results_disMap[Menu_Results_riskbreakerRanks[Menu_Results_rank][i + 1]].w;
        }

        xInset /= 2;
        x -= xInset;

        for (i = 0; i < Menu_Results_riskbreakerRanks[Menu_Results_rank][0]; ++i) {
            Render_DrawTextureFlashFade(Menu_Results_disMap, x, y, Menu_Results_riskbreakerRanks[Menu_Results_rank][i + 1], Menu_Results_riskbreakerRankColors);

            x += Menu_Results_disMap[Menu_Results_riskbreakerRanks[Menu_Results_rank][i + 1]].w;
        }
    }
}

// decomp: MENUF.PRG _renderTextureFadeIn (src/MENU/MENUF.PRG/3B8.c:1877)
void Menu_Results_DrawFadingTexture(int x, int y, int texIndex, int brightness)
{
    Render_TextureRegion* texture;
    POLY_FT4* poly;
    void** scratch;

    if (brightness <= 0) {
        return;
    }

    poly = *(void**)Core_ScratchAddr(0);
    texture = &Menu_Results_disMap[texIndex];

    setPolyFT4(poly);
    setXY4(
        poly, x, y, texture->w + x, y, x, texture->h + y, texture->w + x, texture->h + y);
    setUV4(poly, texture->x, texture->y, texture->x + texture->w, texture->y, texture->x,
        texture->y + texture->h, texture->x + texture->w, texture->y + texture->h);
    setSemiTrans(poly, 1);

    if (brightness < 8) {
        setRGB0(poly, brightness * 16, brightness * 16, brightness * 16);
        poly->tpage = texture->tpage | getTPage(0, Render_BlendMode_Add, 0, 0);
        poly->clut = texture->clut + 1;
    } else {
        setRGB0(poly, 128, 128, 128);
        poly->tpage = texture->tpage;
        poly->clut = texture->clut;
    }

    scratch = (void**)Core_ScratchAddr(0);

    AddPrim(scratch[1] - 0x1C, poly++);

    scratch[0] = poly;
}

// decomp: MENUF.PRG _renderTextureFadeInClut (src/MENU/MENUF.PRG/3B8.c:1914)
void Menu_Results_DrawFadingTextureClut(int x, int y, int texIndex, int brightness, int clut)
{
    Render_TextureRegion* texture;
    POLY_FT4* poly;
    void** scratch;

    if (brightness <= 0) {
        return;
    }

    poly = *(void**)Core_ScratchAddr(0);
    texture = &Menu_Results_disMap[texIndex];

    setPolyFT4(poly);
    setXY4(
        poly, x, y, texture->w + x, y, x, texture->h + y, texture->w + x, texture->h + y);
    setUV4(poly, texture->x, texture->y, texture->x + texture->w, texture->y, texture->x,
        texture->y + texture->h, texture->x + texture->w, texture->y + texture->h);
    setSemiTrans(poly, 1);

    if (brightness < 8) {
        setRGB0(poly, brightness * 16, brightness * 16, brightness * 16);
        poly->tpage = texture->tpage | getTPage(0, Render_BlendMode_Add, 0, 0);
        poly->clut = clut + 1;
    } else {
        setRGB0(poly, 128, 128, 128);
        poly->tpage = texture->tpage;
        poly->clut = clut;
    }

    scratch = (void**)Core_ScratchAddr(0);

    AddPrim(scratch[1] - 0x1C, poly++);

    scratch[0] = poly;
}

// decomp: MENUF.PRG _renderTime (src/MENU/MENUF.PRG/3B8.c:1951)
void Menu_Results_DrawTime(int x, int y, int timer, int time, int isNewRecord)
{
    char buf[16];
    int i;
    int time_digits_clut;

    if (time == 0x800000) {
        sprintf(buf, "------");
    } else {
        sprintf(
            buf, "%02d%02d%02d", (time >> 0x10) & 0xFF, (time >> 8) & 0xFF, time & 0xFF);
    }

    time_digits_clut = getClut(800, 511);

    if (isNewRecord != 0) {
        time_digits_clut = getClut(896, 511);
    }

    for (i = 0; i < 6; ++i) {
        if (buf[i] == '-') {
            Menu_Results_DrawFadingTextureClut(x, y, Menu_Results_DisIndex_Time0Dash, timer, time_digits_clut);

            x += 12;
        } else {
            Menu_Results_DrawFadingTextureClut(x, y - 3, buf[i] - '0', timer, time_digits_clut);

            x += 12;
        }

        if (i == 1) {
            Menu_Results_DrawFadingTextureClut(x, y - 2, Menu_Results_DisIndex_Time0Prime, timer, time_digits_clut);

            x += 6;

        } else if (i == 3) {
            Menu_Results_DrawFadingTextureClut(x, y - 2, Menu_Results_DisIndex_Time0DoublePrime, timer, time_digits_clut);

            x += 6;
        }
    }
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

        Render_DrawTextureTintFade(Menu_Results_disMap, x, y, Menu_Results_DisIndex_Rank0Bonus, Menu_Results_bonusReelColors);

        x += Menu_Results_disMap[11].w;

        Render_DrawTextureTintFade(Menu_Results_disMap, x, y, Menu_Results_DisIndex_Rank0Colon, Menu_Results_bonusReelColors);

        x += 2 + Menu_Results_disMap[26].w;
        reel_base_clip_height = 16;
        p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        area = p[0];

        SetDrawArea(area, &Render_legacyDrawEnvironments[(Render_frameBuf + 1) & 1].clip);
        AddPrim(p[1] - 0x1C, area++);

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

            Menu_Results_DrawFadingTextureClut(x, y - 1, Menu_Results_buffReels[Menu_Results_buffReelIndex][temp].stat,
                timer, getClut(800, 511));

            x += Menu_Results_disMap[Menu_Results_buffReels[Menu_Results_buffReelIndex][temp].stat].w;

            Menu_Results_DrawFadingTextureClut(
                x, y + 4, Menu_Results_DisIndex_Rank0Plus, timer, getClut(800, 511));

            x += Menu_Results_disMap[24].w;

            sprintf(buf, "%d", Menu_Results_buffReels[Menu_Results_buffReelIndex][temp].amount);

            len = strlen(buf);

            for (j = 0; j < len; ++j) {
                Menu_Results_DrawFadingTextureClut(
                    x, y - 1, buf[j] - '0', timer, getClut(800, 511));
                x += 0xC;
            }
        }

        q = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
        area = q[0];

        SetDrawArea(area, &reel_clip_rect);
        AddPrim(q[1] - 0x1C, area++);

        q[0] = area;
    }
}

// decomp: MENUF.PRG _renderPressButtonPrompt (src/MENU/MENUF.PRG/3B8.c:2117)
void Menu_Results_DrawPressButtonPrompt(int x, int y, int slide_timer, int bounce_timer)
{
    POLY_FT4* poly;
    void** p;
    int v1;

    if (slide_timer < 0) {
        return;
    }

    v1 = x + 288;

    if (slide_timer > 48) {
        slide_timer = 48;
    }

    x = (v1 - (slide_timer * 6));

    Render_DrawCompactTextColor(
        "PRESS", vs_getXY_2(x + 18, y), vs_getRGB888(255, 128, 128), NULL);
    Render_DrawCompactTextColor(
        "BUTTON", vs_getXY_2(x + 18, y + 10), vs_getRGB888(255, 128, 128), NULL);

    y += 22;

    if ((bounce_timer & 0xF) < 9) {
        v1 = y - 16;
        y = v1 + ((bounce_timer & 0xF) >> 1);
    } else {
        v1 = y - 10;
        y = v1 - ((bounce_timer & 0xF) >> 2);
    }

    poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);

    setPolyFT4(poly);
    setShadeTex(poly, 1);
    setXY4(poly, x, y - 2, x + 16, y - 2, x, y + 14, x + 16, y + 0xE);
    setUV4(poly, 32, 128, 48, 128, 32, 144, 48, 144);
    setTPage(poly, 0, 0, 768, 0);
    setClut(poly, 944, 223);

    p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);

    AddPrim(p[1] - 0x1C, poly++);

    p[0] = poly;
}

// decomp: MENUF.PRG _renderAnimatedStatWheel (src/MENU/MENUF.PRG/3B8.c:2168)
void Menu_Results_DrawBonusReelTexture(int x, int y, int texId)
{
    int i;
    int brightness;
    void** p = (void**)((uintptr_t)Core_ScratchBase() + 0x000);
    POLY_FT4* poly = p[0];
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

            AddPrim(p[1] - 0x1C, poly++);

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

            AddPrim(p[1] - 0x1C, poly++);

        } else if (y >= 206) {

            brightness = 128 - ((y - 206) * 16);

            if (brightness < 0) {
                brightness = 0;
            }

            setRGB0(poly, brightness, brightness, brightness);
            setSemiTrans(poly, 1);
            setClut(poly, 816, 511);
            poly->tpage = Menu_Results_disMap[texId].tpage | getTPage(0, Render_BlendMode_Add, 0, 0);

            AddPrim(p[1] - 0x1C, poly++);

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

            AddPrim(p[1] - 0x1C, poly++);

        } else {
            setRGB0(poly, 128, 128, 128);
            setSemiTrans(poly, 1);
            setClut(poly, 800, 511);
            poly->tpage = Menu_Results_disMap[texId].tpage;

            AddPrim(p[1] - 0x1C, poly++);
        }
    }

    p[0] = poly;
}

// decomp: MENUF.PRG _determineRank (src/MENU/MENUF.PRG/3B8.c:2774)
void Menu_Results_DetermineRiskbreakerRank(void)
{
    static short Menu_Results_determineRiskbreakerRankTitleThresholds[16] = { 16, 12, 8 };
    static u_int Menu_Results_determineRiskbreakerRankScores[] = { 100000000, 75000000, 60000000, 40000000, 32000000,
        24000000, 16000000, 12000000, 8000000, 5000000, 4000000, 3000000, 2000000,
        1000000, 500000, 0 };

    int unlockedTitles;
    int i;
    int flag;

    for (i = 0, unlockedTitles = 0, flag = 1; i < 16; ++i) {
        if (Audio_Seq_unusedScoredata.titles & (flag << i)) {
            ++unlockedTitles;
        }
    }

    for (i = 0; i < 16; ++i) {
        if (unlockedTitles >= Menu_Results_determineRiskbreakerRankTitleThresholds[i]) {
            if (Menu_Results_score >= Menu_Results_determineRiskbreakerRankScores[i]) {
                Menu_Results_rank = i;
                return;
            }
        }
    }
}

// decomp: MENUF.PRG _calculateScore (src/MENU/MENUF.PRG/3B8.c:2801)
void Menu_Results_CalculateScore(void)
{
    short classPoints[] = { 20, 20, 40, 80, 100, 60 };
    short weaponClassPoints[] = { 20, 40, 20, 100, 60, 100, 60, 100, 80, 60 };
    int i;

    Menu_Results_score = 0;

    for (i = 0; i < 6; ++i) {
        Menu_Results_score += Audio_Seq_unusedScoredata.enemyKills[i] * classPoints[i];
    }

    for (i = 0; i < 10; ++i) {
        Menu_Results_score += Audio_Seq_unusedScoredata.weaponAttacks[i] * weaponClassPoints[i];
    }

    for (i = 0; i < 8; ++i) {
        if ((Audio_Seq_unusedScoredata.bossTimeTrialScores[i][0].time) != 0x800000) {
            Menu_Results_score += 20000;
        }
    }

    if (Audio_Seq_unusedScoredata.maxChain < 6) {
        Menu_Results_score += Audio_Seq_unusedScoredata.maxChain * 10;
    } else if (Audio_Seq_unusedScoredata.maxChain < 11) {
        Menu_Results_score += Audio_Seq_unusedScoredata.maxChain * 20;
    } else if (Audio_Seq_unusedScoredata.maxChain < 16) {
        Menu_Results_score += Audio_Seq_unusedScoredata.maxChain * 40;
    } else if (Audio_Seq_unusedScoredata.maxChain < 21) {
        Menu_Results_score += Audio_Seq_unusedScoredata.maxChain * 80;
    } else if (Audio_Seq_unusedScoredata.maxChain < 26) {
        Menu_Results_score += Audio_Seq_unusedScoredata.maxChain * 160;
    } else if (Audio_Seq_unusedScoredata.maxChain < 31) {
        Menu_Results_score += Audio_Seq_unusedScoredata.maxChain * 320;
    } else if (Audio_Seq_unusedScoredata.maxChain < 51) {
        Menu_Results_score += Audio_Seq_unusedScoredata.maxChain * 640;
    } else {
        Menu_Results_score += Audio_Seq_unusedScoredata.maxChain * 1280;
    }

    Menu_Results_score += Menu_Results_clearCount * 100000;

    if ((Menu_Results_clearCount != 0) && (Audio_Seq_unusedScoredata.completionTimeMinutes < 600)) {
        if (Audio_Seq_unusedScoredata.completionTimeMinutes >= 540) {
            Menu_Results_score = Menu_Results_score + (Menu_Results_score >> 2);
        } else if (Audio_Seq_unusedScoredata.completionTimeMinutes >= 300) {
            Menu_Results_score = Menu_Results_score + (Menu_Results_score >> 1);
        } else {
            Menu_Results_score = Menu_Results_score * 2;
        }
    }

    Menu_Results_score = Menu_Results_score + Audio_Seq_unusedScoredata.streakScore;
    Menu_Results_score = Menu_Results_score + Audio_Seq_unusedScoredata.miscScore;

    if (Menu_Results_score > 999999999) {
        Menu_Results_score = 999999999;
    }
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

// decomp: MENUF.PRG _getTotalStrength (src/MENU/MENUF.PRG/3B8.c:2880)
int Menu_Results_AshleyTotalStrength(void) { return Battle_characterState->stats->totalStrength; }

// decomp: MENUF.PRG _getTotalIntelligence (src/MENU/MENUF.PRG/3B8.c:2882)
int Menu_Results_AshleyTotalIntelligence(void)
{
    return Battle_characterState->stats->totalIntelligence;
}

// decomp: MENUF.PRG _getTotalAgility (src/MENU/MENUF.PRG/3B8.c:2887)
int Menu_Results_AshleyTotalAgility(void) { return Battle_characterState->stats->totalAgility; }

// decomp: MENUF.PRG _raiseMaxStrength (src/MENU/MENUF.PRG/3B8.c:2889)
void Menu_Results_ApplyStrengthBonus(int amount)
{
    Battle_characterState->stats->totalStrength += amount;

    if (Battle_characterState->stats->totalStrength > 999) {
        Battle_characterState->stats->totalStrength = 999;
    }

    Battle_characterState->stats->strength += amount;

    if (Battle_characterState->stats->strength > 999) {
        Battle_characterState->stats->strength = 999;
    }
}

// decomp: MENUF.PRG _raiseMaxIntelligence (src/MENU/MENUF.PRG/3B8.c:2904)
void Menu_Results_ApplyIntelligenceBonus(int amount)
{
    Battle_characterState->stats->totalIntelligence += amount;

    if (Battle_characterState->stats->totalIntelligence > 999) {
        Battle_characterState->stats->totalIntelligence = 999;
    }

    Battle_characterState->stats->intelligence += amount;

    if (Battle_characterState->stats->intelligence > 999) {
        Battle_characterState->stats->intelligence = 999;
    }
}

// decomp: MENUF.PRG _raiseMaxAgility (src/MENU/MENUF.PRG/3B8.c:2919)
void Menu_Results_ApplyAgilityBonus(int amount)
{
    Battle_characterState->stats->totalAgility += amount;

    if (Battle_characterState->stats->totalAgility > 999) {
        Battle_characterState->stats->totalAgility = 999;
    }

    Battle_characterState->stats->agility += amount;

    if (Battle_characterState->stats->agility > 999) {
        Battle_characterState->stats->agility = 999;
    }
}

// decomp: MENUF.PRG _raiseMaxHP (src/MENU/MENUF.PRG/3B8.c:2934)
void Menu_Results_ApplyHpBonus(int amount)
{
    Battle_characterState->stats->maxHp += amount;

    if (Battle_characterState->stats->maxHp > 999) {
        Battle_characterState->stats->maxHp = 999;
    }

    Battle_characterState->stats->currentHp += amount;

    if (Battle_characterState->stats->currentHp > 999) {
        Battle_characterState->stats->currentHp = 999;
    }
}

// decomp: MENUF.PRG _raiseMaxMP (src/MENU/MENUF.PRG/3B8.c:2949)
void Menu_Results_ApplyMpBonus(int amount)
{
    Battle_characterState->stats->maxMp += amount;

    if (Battle_characterState->stats->maxMp > 999) {
        Battle_characterState->stats->maxMp = 999;
    }

    Battle_characterState->stats->currentMp += amount;

    if (Battle_characterState->stats->currentMp > 999) {
        Battle_characterState->stats->currentMp = 999;
    }
}

// MENUF.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Results_ResetMenufStatics(void)
{
    {
        static const CVECTOR initial[] = { { 128, 96, 64 }, { 200, 180, 160 }, { 128, 96, 64 } };
        memcpy((void*)&Menu_Results_congratulationsColors, &initial, sizeof Menu_Results_congratulationsColors);
    }
    {
        static const CVECTOR initial[] = { { 100, 180, 220 }, { 100, 180, 220 } };
        memcpy((void*)&Menu_Results_scoreColors, &initial, sizeof Menu_Results_scoreColors);
    }
    {
        static const CVECTOR initial[] = { { 100, 180, 220 }, { 100, 180, 220 } };
        memcpy((void*)&Menu_Results_countingScoreColors, &initial, sizeof Menu_Results_countingScoreColors);
    }
    {
        static const CVECTOR initial[] = { { 100, 180, 220 }, { 100, 180, 220 } };
        memcpy((void*)&Menu_Results_mapCompletionColors, &initial, sizeof Menu_Results_mapCompletionColors);
    }
    {
        static const CVECTOR initial[] = { { 100, 180, 220 }, { 100, 180, 220 } };
        memcpy((void*)&Menu_Results_countingMapCompletionColors, &initial, sizeof Menu_Results_countingMapCompletionColors);
    }
    {
        static const CVECTOR initial[] = { { 200, 80, 20 }, { 200, 80, 20 } };
        memcpy((void*)&Menu_Results_riskbreakerRankHeadingColors, &initial, sizeof Menu_Results_riskbreakerRankHeadingColors);
    }
    {
        static const CVECTOR initial[] = { { 128, 128, 128 }, { 128, 128, 128 } };
        memcpy((void*)&Menu_Results_riskbreakerRankColors, &initial, sizeof Menu_Results_riskbreakerRankColors);
    }
    {
        static const CVECTOR initial[] = { { 128, 96, 64 }, { 200, 180, 160 } };
        memcpy((void*)&Menu_Results_bonusReelColors, &initial, sizeof Menu_Results_bonusReelColors);
    }
}

