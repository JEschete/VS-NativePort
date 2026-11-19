// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Menu/Results/Results.h"
#include "Render/Images.hpp"

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
VS::Render::TextureRegion Menu_Results_disMap[127] = {
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

