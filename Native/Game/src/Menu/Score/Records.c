// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/StatusEffects.h"
#include "Content/StringTables.h"
#include "Core/String/GameText.h"
#include "Core/Text/TextCode.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Font.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

// decomp: _clearCount (_clearCount@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_clearCount;

// decomp: _gazetteRows (_gazetteRows@src/MENU/MENU9.PRG/238.c)
Menu_Score_Row Menu_Score_gazetteRows[32];

// decomp: _miscInfo (_miscInfo@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_miscInfo[1646]; /* contents: disc string table (ModuleData.c) */

// decomp: _score (_score@src/MENU/MENU9.PRG/238.c)
u_long Menu_Score_score;

// decomp: _statDescriptions (_statDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_statDescriptions[460]; /* contents: disc string table (ModuleData.c) */

// decomp: _statHeaders (_statHeaders@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_statHeaders[274]; /* contents: disc string table (ModuleData.c) */

// decomp: _timeAttackDescriptions (_timeAttackDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_timeAttackDescriptions[232]; /* contents: disc string table (ModuleData.c) */

// decomp: _timeAttackReferenceTimes (_timeAttackReferenceTimes@src/MENU/MENU9.PRG/238.c)
char const* Menu_Score_kTimeAttackReferenceTimes[] = { "$00:25:00", "$00:30:00", "$00:40:00",
    "$00:50:00", "$01:00:00", "$01:15:00", "$01:00:00", "$01:25:00" };

// decomp: _timeAttacks (_timeAttacks@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_timeAttacks[58]; /* contents: disc string table (ModuleData.c) */

// decomp: _titleDescriptions (_titleDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_titleDescriptions[784]; /* contents: disc string table (ModuleData.c) */

// decomp: _titleText (_titleText@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_titleText[364]; /* contents: disc string table (ModuleData.c) */

// decomp: local static of _renderTitleMenu, at file scope so the module reset reaches it
static char Menu_Score_titlesCursorAnimState = 0;

// decomp: local static of _renderBossRushDetails, at file scope so the module reset reaches it
static char Menu_Score_statisticsCursorAnimStep = 0;

// decomp: local static of _renderBossRushRecords, at file scope so the module reset reaches it
static char Menu_Score_timeTrialRecordsCursorAnimStep = 0;

// decomp: MENU9.PRG _setTitleRows (src/MENU/MENU9.PRG/238.c:1722)
void Menu_Score_CreateTitleRows(void)
{
    int i;
    Menu_Score_Row* row;
    int new_var;

    for (i = 0, row = Menu_Score_gazetteRows; i < 32; ++i, ++row) {

        row->x = 60;
        row->rowIndex = i + 1;
        row->selected = 0;
        row->animationState = 0;
        row->disabled = 0;

        if (Audio_Seq_unusedScoredata.titles & ((new_var = 1) << i)) {
            row->opaque1 = 1;
            row->title = (char*)&Menu_Score_titleText[Menu_Score_titleText[i]];
            row->description = (char*)&Menu_Score_titleDescriptions[Menu_Score_titleDescriptions[i]];
        } else {
            row->opaque1 = 0;
            row->title = (char*)&Menu_Score_miscInfo[Menu_Score_miscInfo[Text_Menu9_miscInfo_Index_noneClear]];
            row->description =
                (char*)&Menu_Score_miscInfo[Menu_Score_miscInfo[Text_Menu9_miscInfo_Index_conditionsNotMet]];
        }
    }
}

// decomp: MENU9.PRG _renderTitleMenu (src/MENU/MENU9.PRG/238.c:1756)
void Menu_Score_RenderTitles(int animState)
{

    int i;
    Menu_Framework_MenuRow* menuItem;

    Menu_Score_Row* row = Menu_Score_gazetteRows;
    int selectedRow =
        Battle_unusedMenu9CursorMemory.titlePage + Battle_unusedMenu9CursorMemory.titleRow;

    for (i = 0; i < 32; ++i, ++row) {
        if (i == selectedRow) {
            if (row->selected == 0) {
                Menu_Framework_SetInformationMessage(row->description);
            }
            row->selected = 1;
            if (row->opaque1 != 0) {
                row->animationState = 8;
            }
        } else {
            row->selected = 0;
            if (row->animationState != 0) {
                --row->animationState;
            }
        }
    }

    row = &Menu_Score_gazetteRows[Battle_unusedMenu9CursorMemory.titlePage];

    for (i = 0; i < 8; ++i, ++row) {

        int y = (animState * 23) - 16;

        if ((56 + i * 16) < y) {
            y = (56 + i * 16);
        }

        if (row->selected != 0) {
            if (animState == 8) {
                Menu_Score_titlesCursorAnimState = Menu_Framework_DrawCursor(
                    Menu_Score_titlesCursorAnimState, vs_getXY_2(row->x - 12, y - 8));
            }
        }

        menuItem = Menu_Framework_SetMenuItem(0, row->x, y, 200, 0, row->title);

        if (row->opaque1 == 0) {
            menuItem->fontColor = 3;
        }
        menuItem->gradientState = row->animationState;

        if ((i == 0) && (Battle_unusedMenu9CursorMemory.titlePage != 0)) {
            menuItem->isScrollable = Menu_Framework_MenuItemScroll_Up;
        }

        if ((i == 7) && (Battle_unusedMenu9CursorMemory.titlePage != 24)) {
            menuItem->isScrollable = Menu_Framework_MenuItemScroll_Down;
        }

        Menu_Score_DrawTitleNumber(row->x - 14, y, row->rowIndex, menuItem->isScrollable & 1);

        if ((menuItem->isScrollable != Menu_Framework_MenuItemScroll_None) && (animState == 8)) {
            Menu_Score_RenderScrollableRow(menuItem);
        } else {
            Menu_Framework_DrawMenuItem(menuItem);
        }
    }

    Menu_Framework_MenuItem(0)->state = 0;
}

// decomp: MENU9.PRG _renderTitleNo (src/MENU/MENU9.PRG/238.c:1835)
void Menu_Score_DrawTitleNumber(int x, int y, int titleNo, int dimmed)
{
    char digits[2];
    int i;
    POLY_FT4* poly;

    sprintf(digits, "%02d", titleNo);

    x += 19;
    poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);

    for (i = 1; i >= 0; --i, x -= 5) {

        setPolyFT4(poly);
        setXY4(poly, x, y - 4, x + 6, y - 4, x, y + 6, x + 6, y + 6);
        setcode(poly, (getcode(poly) | 0x02) & 0xFE);

        setUV4(poly, (digits[i] - 48) * 6, 0, ((digits[i] - 48) * 6) + 6, 0,
            (digits[i] - 48) * 6, 10, ((digits[i] - 48) * 6) + 6, 10);

        if (dimmed != 0) {
            setTPage(poly, 0, 1, 768, 0);
            setRGB0(poly, 32, 32, 32);
            setClut(poly, 880, 223);
        } else {
            setTPage(poly, 0, 0, 768, 0);
            setRGB0(poly, 128, 128, 128);
            setClut(poly, 832, 223);
        }

        AddPrim(*(void**)((uintptr_t)Core_ScratchBase() + 0x008), poly++);
    }

    *(void**)((uintptr_t)Core_ScratchBase() + 0x000) = poly;
}

// decomp: MENU9.PRG _setStatText (src/MENU/MENU9.PRG/238.c:2201)
void Menu_Score_CreateStatisticRows(void)
{
    int i;
    Menu_Score_Row* p;

    for (i = 0, p = Menu_Score_gazetteRows; i < 23; ++i, ++p) {
        p->x = 16;
        p->title = (void*)&Menu_Score_statHeaders[Menu_Score_statHeaders[i]];
        p->description = (void*)&Menu_Score_statDescriptions[Menu_Score_statDescriptions[i]];
        p->animationState = 0;
        p->rowIndex = i + 1;
        p->selected = 0;
        p->disabled = 0;
        p->opaque1 = 1;
    }
}

// decomp: MENU9.PRG _renderBossRushDetails (src/MENU/MENU9.PRG/238.c:2226)
void Menu_Score_RenderStatistics(int animState)
{

    char sp18[32];
    Menu_Score_Row* row;
    u_short temp_v1_4;
    int s5;
    int i;
    Menu_Framework_MenuRow* menuItem;
    char* intBuf;
    Menu_Score_CursorMemory* new_var;
    int v1;

    row = Menu_Score_gazetteRows;
    temp_v1_4 = Battle_unusedMenu9CursorMemory.recordTimePage
              + Battle_unusedMenu9CursorMemory.recordTimeRow;

    for (i = 0; i < 23; ++i, ++row) {
        if (i == temp_v1_4) {
            if (row->selected == 0) {
                Menu_Framework_SetInformationMessage(row->description);
            }
            row->selected = 1;
            row->animationState = 8;
        } else {
            row->selected = 0;
            if (row->animationState != 0) {
                --row->animationState;
            }
        }
    }

    row = &Menu_Score_gazetteRows[Battle_unusedMenu9CursorMemory.recordTimePage];
    i = 0;
    new_var = &Battle_unusedMenu9CursorMemory;
    intBuf = &sp18[2];
    s5 = 0x30 * 6;

    for (; i < 8; ++i, ++row) {
        int var_s4 = 0xF0 - (animState * 0x17);

        if (var_s4 < ((i * 0x10) + 0x38)) {
            var_s4 = ((i * 0x10) + 0x38);
        }
        if ((row->selected != 0) && (animState == 8)) {
            Menu_Score_statisticsCursorAnimStep = Menu_Framework_DrawCursor(
                Menu_Score_statisticsCursorAnimStep, ((row->x - 0xC) & 0xFFFF) | ((var_s4 - 8) << 0x10));
        }

        menuItem = Menu_Framework_SetMenuItem(0, row->x, var_s4, 0x8E, 0, row->title);
        menuItem->gradientState = row->animationState;

        if ((i == 0) && (new_var->recordTimePage != 0)) {
            menuItem->isScrollable = 1;
        }

        if ((i == 7) && (new_var->recordTimePage != 0xF)) {
            menuItem->isScrollable = 2;
        }

        if ((menuItem->isScrollable != 0) && (animState == 8)) {
            Menu_Score_RenderScrollableRow(menuItem);
        } else {
            Menu_Framework_DrawMenuItem(menuItem);
        }

        v1 = new_var->recordTimePage + i;

        if (v1 == 0) {
            Core_String_FormatGameInteger(intBuf, Menu_Score_clearCount);
        } else if (v1 == 1) {
            Menu_Score_FormatFastestClearTime(intBuf, Audio_Seq_unusedScoredata.clearCount,
                Audio_Seq_unusedScoredata.completionTimeMinutes);
        } else if (v1 == 2) {
            Core_String_FormatGamePercent(intBuf, (Menu_Score_openedChestCount * 100) / 52);
        } else if (v1 == 3) {
            Core_String_FormatGamePercent(intBuf, (Audio_Seq_unusedScoredata.openedChestCount * 100) / 52);
        } else if (v1 == 4) {
            Core_String_FormatGamePercent(intBuf, (Menu_Score_mapCompletion * 100) / 361);
        } else if (v1 == 5) {
            Core_String_FormatGamePercent(intBuf, Audio_Seq_unusedScoredata.mapCompletion * 100 / 361);
        } else if (v1 == 6) {
            Core_String_FormatGameInteger(intBuf, Audio_Seq_unusedScoredata.maxChain);
        } else if (v1 < 13) {
            Core_String_FormatGameInteger(intBuf, Audio_Seq_unusedScoredata.enemyKills[v1 - 7]);
        } else {
            v1 -= 12;
            if (v1 == 10) {
                v1 = 0;
            }
            Core_String_FormatGameInteger(intBuf, Audio_Seq_unusedScoredata.weaponAttacks[v1]);
        }

        v1 = s5 - (Render_MeasureEncodedTextLine(intBuf) * 6);
        ++s5;
        --s5;
        sp18[0] = 0xFA;
        sp18[1] = v1 - (row->x - 0x72);
        menuItem = Menu_Framework_SetMenuItem(0, row->x + 0x8E, var_s4, 0x8E, 0, sp18);
        menuItem->gradientState = 8 - row->animationState;

        if ((i == 0) && (new_var->recordTimePage != 0)) {
            menuItem->isScrollable = 1;
        }
        if ((i == 7) && (new_var->recordTimePage != 0xF)) {
            menuItem->isScrollable = 2;
        }

        if ((menuItem->isScrollable != 0) && (animState == 8)) {
            Menu_Score_RenderScrollableRow(menuItem);
        } else {
            Menu_Framework_DrawMenuItem(menuItem);
        }
    }

    Menu_Framework_MenuItem(0)->state = 0;
}

// decomp: MENU9.PRG _printFastestClearTime (src/MENU/MENU9.PRG/238.c:2400)
void Menu_Score_FormatFastestClearTime(char* buf, int rounds, int totalSeconds)
{
    int len;
    int i;
    int minutes;
    int seconds;
    char* str;

    if (Menu_Score_clearCount == 0) {
        Core_String_CopyGameText(
            (u_char*)buf, (u_char*)&Menu_Score_miscInfo[Menu_Score_miscInfo[Text_Menu9_miscInfo_Index_noneClear]]);
        return;
    }

    minutes = totalSeconds / 60;

    if (minutes >= 100) {
        minutes = 99;
        seconds = 59;
    } else {
        seconds = totalSeconds % 60;
    }

    *buf = Core_TextCode_UpperR;
    str = buf + 1;

    sprintf(str, "%d", rounds + 1);
    len = strlen(str);

    for (i = 0; i < len; ++i) {
        str[i] -= '0';
    }

    str = &str[len];
    *str++ = Core_TextCode_Spacing;
    *str++ = 12;

    sprintf(str, "%02d", minutes);
    len = strlen(str);

    for (i = 0; i < len; ++i) {
        str[i] -= '0';
    }

    str = &str[len];
    *str++ = Core_TextCode_Colon;

    sprintf(str, "%02d", seconds);
    len = strlen(str);

    for (i = 0; i < len; ++i) {
        str[i] -= '0';
    }

    str += len;
    *str = Core_TextCode_Terminator;
}

// decomp: MENU9.PRG _printBossRushMenuRow (src/MENU/MENU9.PRG/238.c:2461)
void Menu_Score_CreateTimeTrialRows(void)
{
    int i;
    Menu_Score_Row* row;

    for (i = 0, row = Menu_Score_gazetteRows; i < 8; ++i, ++row) {
        row->x = 194;
        if ((Audio_Seq_unusedScoredata.bossTimeTrialScores[i][0].time) == 0x800000) {
            row->disabled = 1;
            row->title = (char*)&Menu_Score_miscInfo[Menu_Score_miscInfo[Text_Menu9_miscInfo_Index_noneBossRush]];
            row->description =
                (char*)&Menu_Score_miscInfo[Menu_Score_miscInfo[Text_Menu9_miscInfo_Index_noneBossRushDesc]];
        } else {
            row->disabled = 0;
            row->title = (char*)&Menu_Score_timeAttacks[Menu_Score_timeAttacks[i]];
            row->description =
                (char*)&Menu_Score_timeAttackDescriptions[Menu_Score_timeAttackDescriptions[i]];
        }
        row->selected = 0;
        row->animationState = i + 1;
        row->opaque1 = 1;
    }
}

// decomp: MENU9.PRG _leaveBossRushMenu (src/MENU/MENU9.PRG/238.c:2488)
void Menu_Score_LeaveTimeTrialMenu(void)
{
    int i;
    Menu_Score_Row* p = Menu_Score_gazetteRows;

    for (i = 0; i < 8; ++i, ++p) {
        if (p->animationState == 0) {
            Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(i);
            menuItem->state = 2;
            menuItem->targetPosition0 = 320;
        } else {
            p->animationState = -1;
        }
    }
}

// decomp: MENU9.PRG _renderBossRushRecords (src/MENU/MENU9.PRG/238.c:2507)
void Menu_Score_RenderTimeTrialRecords(int selected)
{

    char* difficulty[] __attribute__((unused)) = { "EASY", "NORMAL" };
    int difficultyColors[]
        __attribute__((unused)) = { vs_getRGB(0, 128, 128), vs_getRGB(128, 128, 0) };

    char sp28[16];
    int sp38;
    Menu_Score_Row* sp3C;
    Menu_Score_Row* var_s1;
    int var_fp;
    int i;
    int j;
    int var_s7;
    int time;
    Menu_Framework_MenuRow* menuItem;
    void** p;
    void** r;
    void** q;

    var_s1 = Menu_Score_gazetteRows;
    var_s7 = 0;
    sp38 = Battle_unusedMenu9CursorMemory.gazettePage;

    for (i = 0; i < 8; ++i, ++var_s1) {
        if (var_s1->animationState != 0) {
            if (!(--var_s1->animationState)) {
                menuItem = Menu_Framework_SetMenuItem(
                    i, 0x140, 0x32 + i * 0x10, 0x88, 0, var_s1->title);
                menuItem->unselectable = var_s1->disabled;
                menuItem->state = 2;
                menuItem->targetPosition0 = 0xC2;
            }
        } else if (Menu_Framework_MenuItem(i)->state == 1) {
            ++var_s7;
        }
    }

    var_s1 = Menu_Score_gazetteRows;

    for (i = 0; i < 8; ++var_s1, ++i) {

        if ((var_s7 != 8) || (selected != var_s7)) {
            continue;
        }

        menuItem = Menu_Framework_MenuItem(i);

        if (i == sp38) {

            if (var_s1->selected == 0) {
                Menu_Framework_SetInformationMessage(var_s1->description);
            }

            var_s1->selected = 1;
            menuItem->selected = 1;
            var_fp = (((sp38 * 0x10) + 0x2A) << 0x10) | 0xB6;

            Menu_Score_timeTrialRecordsCursorAnimStep = Menu_Framework_DrawCursor(Menu_Score_timeTrialRecordsCursorAnimStep, var_fp);

            sp3C = var_s1;

        } else {
            var_s1->selected = 0;
            menuItem->selected = 0;
        }
    }

    p = ((void**)((uintptr_t)Core_ScratchBase() + 0x000));
    Render_DrawCompactTextColor(
        "PLAYER TIME", vs_getXY((selected * 0x10) - 0x78, 0x20), 0x808080, p[1] + 0x1C);
    Menu_Score_DrawGradientPanel((selected * 0x10) - 0x80, 0x20, 0x60, 0xC, 1);
    Render_DrawCompactTextColor(
        "1ST", vs_getXY((selected * 0x10) - 0x68, 0x32), 0x808080, p[1] + 0x1C);
    Render_DrawCompactTextColor(
        "2ND", vs_getXY((selected * 0x10) - 0x68, 0x50), 0x808080, p[1] + 0x1C);
    Render_DrawCompactTextColor(
        "3RD", vs_getXY((selected * 0x10) - 0x68, 0x6E), 0x808080, p[1] + 0x1C);

    for (j = 0; j < 3; ++j) {

        time = Audio_Seq_unusedScoredata.bossTimeTrialScores[sp38][j].time;

        if (time != 0x800000) {
            sprintf(
                sp28, "$%02d:%02d:%02d", time >> 0x10, (time >> 8) & 0xFF, time & 0xFF);
            q = ((void**)((uintptr_t)Core_ScratchBase() + 0x000));
            Render_DrawCompactTextColor(sp28,
                vs_getXY_2((selected * 16) - 40, 50 + j * 30), vs_getRGB(128, 128, 128),
                q[1] + 0x1C);
            sprintf(sp28, "ROUND    %d",
                Audio_Seq_unusedScoredata.bossTimeTrialScores[sp38][j].round + 1);
            Render_DrawCompactTextColor(sp28,
                vs_getXY_2((selected * 16) - 60, 62 + j * 30), vs_getRGB(128, 128, 128),
                q[1] + 0x1C);
        } else {
            q = ((void**)((uintptr_t)Core_ScratchBase() + 0x000));
            Render_DrawCompactTextColor("$--:--:--",
                vs_getXY_2((selected * 16) - 40, 50 + j * 30), vs_getRGB(80, 80, 80),
                q[1] + 0x1C);
        }
    }

    Menu_Score_DrawGradientPanel((selected * 0x10) - 0x70, 0x32, 0x80, 0x54, 2);
    r = ((void**)((uintptr_t)Core_ScratchBase() + 0x000));
    Render_DrawCompactTextColor("REFERENCE TIME", vs_getXY(selected * 16 - 0x78, 0x8C),
        vs_getRGB(128, 128, 128), r[1] + 0x1C);
    Menu_Score_DrawGradientPanel((selected * 0x10) - 0x80, 0x8C, 0x70, 0xC, 1);

    if ((var_s7 == 8) && (selected == var_s7)) {
        if (sp3C->disabled != 0) {
            Render_DrawCompactTextColor(
                "$--:--:--", vs_getXY(88, 158), vs_getRGB(80, 80, 80), r[1] + 0x1C);
        } else {
            Render_DrawCompactTextColor(Menu_Score_kTimeAttackReferenceTimes[sp38],
                vs_getXY(88, 158), vs_getRGB(128, 128, 128), r[1] + 0x1C);
        }
    }
    Menu_Score_DrawGradientPanel((selected * 0x10) - 0x70, 0x9E, 0x80, 0xC, 2);
}

// decomp: MENU9.PRG _determineCharacterRank (src/MENU/MENU9.PRG/238.c:2633)
void Menu_Score_DetermineRiskbreakerRank(void)
{
    static short Menu_Score_determineRiskbreakerRankCharacterRankTitleThresholds[16] = { 16, 12, 8 };
    static u_int Menu_Score_determineRiskbreakerRankCharacterRankPointThresholds[] = { 100000000, 75000000, 60000000,
        40000000, 32000000, 24000000, 16000000, 12000000, 8000000, 5000000, 4000000,
        3000000, 2000000, 1000000, 500000, 0 };

    int i;
    int titleCount;
    int flag;

    for (i = 0, titleCount = 0, flag = 1; i < 16; ++i) {
        if (Audio_Seq_unusedScoredata.titles & (flag << i)) {
            ++titleCount;
        }
    }

    for (i = 0; i < 16; ++i) {
        if ((titleCount >= Menu_Score_determineRiskbreakerRankCharacterRankTitleThresholds[i])
            && (Menu_Score_score >= Menu_Score_determineRiskbreakerRankCharacterRankPointThresholds[i])) {
            Menu_Score_characterRank = i;
            return;
        }
    }
}

// decomp: MENU9.PRG _setTitleFlags (src/MENU/MENU9.PRG/238.c:2662)
void Menu_Score_AwardTitles(void)
{
    int i;

    if (Menu_Score_clearCount != 0) {
        Audio_Seq_unusedScoredata.titles |= 1;
    }

    if (Audio_Seq_unusedScoredata.openedChestCount >= 52) {
        Audio_Seq_unusedScoredata.titles |= 1 << 2;
    }

    if (Audio_Seq_unusedScoredata.mapCompletion > 360) {
        Audio_Seq_unusedScoredata.titles |= 1 << 3;
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

    for (i = 0; i < 8; ++i) {
        if (Audio_Seq_unusedScoredata.bossTimeTrialScores[i][0].time == 0x800000) {
            break;
        }
    }

    if (i == 8) {
        Audio_Seq_unusedScoredata.titles |= 1 << 10;
    }

    if (Audio_Seq_unusedScoredata.maxChain >= 30) {
        Audio_Seq_unusedScoredata.titles |= 1 << 11;
    }

    if (Audio_Seq_unusedStateFlags.goldKeyObtained != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 12;
    }

    if (Audio_Seq_unusedStateFlags.chestKeyObtained != 0) {
        Audio_Seq_unusedScoredata.titles |= 1 << 13;
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
        if (((i != 33) && (i != 36) && (i != 38) && (i != 39)
                && !Battle_actionDefinitions[i].unlocked)) {
            break;
        }
    }

    if (i == 54) {
        Audio_Seq_unusedScoredata.titles |= 1 << 15;
    }

    for (i = 0; i < 6; ++i) {
        if (Audio_Seq_unusedScoredata.enemyKills[i] < 5000) {
            break;
        }
    }

    if (i == 6) {
        Audio_Seq_unusedScoredata.titles |= 1 << 20;
    }

    for (i = 0; i < 10; ++i) {
        if (Audio_Seq_unusedScoredata.weaponAttacks[i] < 5000) {
            break;
        }
    }

    if (i == 10) {
        Audio_Seq_unusedScoredata.titles |= 1 << 21;
    }

    for (i = 0; i < 9; ++i) {
        if (Audio_Seq_unusedScoredata.weaponAttacks[i + 1] >= 500) {
            Audio_Seq_unusedScoredata.titles |= 1 << (i + 22);
        }
    }

    if (Audio_Seq_unusedScoredata.weaponAttacks[0] >= 500) {
        Audio_Seq_unusedScoredata.titles |= 1 << 31;
    }
}

// decomp: MENU9.PRG _calculateScore (src/MENU/MENU9.PRG/238.c:2842)
void Menu_Score_CalculateScore(void)
{
    short classPoints[6] = { 20, 20, 40, 80, 100, 60 };
    short weaponClassPoints[10] = { 20, 40, 20, 100, 60, 100, 60, 100, 80, 60 };
    int i;

    Menu_Score_score = 0;

    for (i = 0; i < 6; ++i) {
        Menu_Score_score += Audio_Seq_unusedScoredata.enemyKills[i] * classPoints[i];
    }

    for (i = 0; i < 10; ++i) {
        Menu_Score_score += Audio_Seq_unusedScoredata.weaponAttacks[i] * weaponClassPoints[i];
    }

    for (i = 0; i < 8; ++i) {
        if (Audio_Seq_unusedScoredata.bossTimeTrialScores[i][0].time != 0x800000) {
            Menu_Score_score += 20000;
        }
    }

    if (Audio_Seq_unusedScoredata.maxChain < 6) {
        Menu_Score_score += Audio_Seq_unusedScoredata.maxChain * 10;
    } else if (Audio_Seq_unusedScoredata.maxChain < 11) {
        Menu_Score_score += Audio_Seq_unusedScoredata.maxChain * 20;
    } else if (Audio_Seq_unusedScoredata.maxChain < 16) {
        Menu_Score_score += Audio_Seq_unusedScoredata.maxChain * 40;
    } else if (Audio_Seq_unusedScoredata.maxChain < 21) {
        Menu_Score_score += Audio_Seq_unusedScoredata.maxChain * 80;
    } else if (Audio_Seq_unusedScoredata.maxChain < 26) {
        Menu_Score_score += Audio_Seq_unusedScoredata.maxChain * 160;
    } else if (Audio_Seq_unusedScoredata.maxChain < 31) {
        Menu_Score_score += Audio_Seq_unusedScoredata.maxChain * 320;
    } else if (Audio_Seq_unusedScoredata.maxChain < 51) {
        Menu_Score_score += Audio_Seq_unusedScoredata.maxChain * 640;
    } else {
        Menu_Score_score += Audio_Seq_unusedScoredata.maxChain * 1280;
    }

    Menu_Score_score += Menu_Score_clearCount * 100000;

    if ((Menu_Score_clearCount != 0) && (Audio_Seq_unusedScoredata.completionTimeMinutes < 600)) {
        if (Audio_Seq_unusedScoredata.completionTimeMinutes >= 540) {
            Menu_Score_score += Menu_Score_score / 4;
        } else if (Audio_Seq_unusedScoredata.completionTimeMinutes >= 300) {
            Menu_Score_score += Menu_Score_score / 2;
        } else {
            Menu_Score_score *= 2;
        }
    }

    Menu_Score_score += Audio_Seq_unusedScoredata.streakScore;
    Menu_Score_score += Audio_Seq_unusedScoredata.miscScore;
    if (Menu_Score_score > 999999999) {
        Menu_Score_score = 999999999;
    }
}

// MENU9.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Score_Records_ResetMenu9Statics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Score_titlesCursorAnimState, &initial, sizeof Menu_Score_titlesCursorAnimState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Score_statisticsCursorAnimStep, &initial, sizeof Menu_Score_statisticsCursorAnimStep);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Score_timeTrialRecordsCursorAnimStep, &initial, sizeof Menu_Score_timeTrialRecordsCursorAnimStep);
    }
}

