// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Core/String/GameTextLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Score/GazetteRowsLive.hpp"
#include "Menu/Score/GazetteTimesLive.hpp"
#include "Render/EncodedTextLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/StatusEffects.h"
#include "Content/StringTables.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"

// decomp: _clearCount (_clearCount@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_clearCount;

// decomp: _gazetteRows (_gazetteRows@src/MENU/MENU9.PRG/238.c)
Menu_Score_Row Menu_Score_gazetteRows[32];

// decomp: _miscInfo (_miscInfo@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_miscInfo[1646]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _score (_score@src/MENU/MENU9.PRG/238.c)
u_long Menu_Score_score;

// decomp: _statDescriptions (_statDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_statDescriptions[460]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _statHeaders (_statHeaders@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_statHeaders[274]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _timeAttackDescriptions (_timeAttackDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_timeAttackDescriptions[232]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _timeAttackReferenceTimes (_timeAttackReferenceTimes@src/MENU/MENU9.PRG/238.c)
char const* Menu_Score_kTimeAttackReferenceTimes[] = { "$00:25:00", "$00:30:00", "$00:40:00",
    "$00:50:00", "$01:00:00", "$01:15:00", "$01:00:00", "$01:25:00" };

// decomp: _timeAttacks (_timeAttacks@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_timeAttacks[58]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _titleDescriptions (_titleDescriptions@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_titleDescriptions[784]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _titleText (_titleText@src/MENU/MENU9.PRG/238.c)
u_short Menu_Score_titleText[364]; /* contents: disc string table (ModuleData.cpp) */

// decomp: local static of _renderTitleMenu, at file scope so the module reset reaches it
static char Menu_Score_titlesCursorAnimState = 0;

// decomp: local static of _renderBossRushDetails, at file scope so the module reset reaches it
static char Menu_Score_statisticsCursorAnimStep = 0;

// decomp: MENU9.PRG _setTitleRows (src/MENU/MENU9.PRG/238.c:1722)
void Menu_Score_CreateTitleRows(void)
{
    int i;
    Menu_Score_Row* row;
    int title_flag_bit;

    for (i = 0, row = Menu_Score_gazetteRows; i < 32; ++i, ++row) {

        row->x = 60;
        row->rowIndex = i + 1;
        row->selected = 0;
        row->animationState = 0;
        row->disabled = 0;

        if (Audio_Seq_unusedScoredata.titles & ((title_flag_bit = 1) << i)) {
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
                VS::Menu::Framework::SetLiveInformationMessage(row->description);
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
                Menu_Score_titlesCursorAnimState = VS::Menu::Framework::DrawLiveCursor(
                    Menu_Score_titlesCursorAnimState, vs_getXY_2(row->x - 12, y - 8));
            }
        }

        menuItem = VS::Menu::Framework::SetLiveMenuItem(0, row->x, y, 200, 0, row->title);

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
            VS::Menu::Score::RenderLiveScrollableRow(menuItem);
        } else {
            VS::Menu::Framework::DrawLiveMenuItem(menuItem);
        }
    }

    VS::Menu::Framework::LiveMenuItem(0)->state = 0;
}

// decomp: MENU9.PRG _renderTitleNo (src/MENU/MENU9.PRG/238.c:1835)
void Menu_Score_DrawTitleNumber(int x, int y, int titleNo, int dimmed)
{
    char digits[sizeof "00"];
    int i;
    POLY_FT4* poly;

    sprintf(digits, "%02d", titleNo);

    x += 19;
    poly = (POLY_FT4 *)(Render_frameScratch.primitiveCursor);

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

        AddPrim(Render_frameScratch.overlayOrderingTable, poly++);
    }

    Render_frameScratch.primitiveCursor = poly;
}

// decomp: MENU9.PRG _setStatText (src/MENU/MENU9.PRG/238.c:2201)
void Menu_Score_CreateStatisticRows(void)
{
    int i;
    Menu_Score_Row* p;

    for (i = 0, p = Menu_Score_gazetteRows; i < 23; ++i, ++p) {
        p->x = 16;
        p->title = (char *)((void*)&Menu_Score_statHeaders[Menu_Score_statHeaders[i]]);
        p->description = (char *)((void*)&Menu_Score_statDescriptions[Menu_Score_statDescriptions[i]]);
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

    char aligned_value_text[32];
    Menu_Score_Row* row;
    u_short selected_record_index;
    int value_right_edge_x;
    int i;
    Menu_Framework_MenuRow* menuItem;
    char* intBuf;
    Menu_Score_CursorMemory* record_cursor_memory;
    int v1;

    row = Menu_Score_gazetteRows;
    selected_record_index = Battle_unusedMenu9CursorMemory.recordTimePage
              + Battle_unusedMenu9CursorMemory.recordTimeRow;

    for (i = 0; i < 23; ++i, ++row) {
        if (i == selected_record_index) {
            if (row->selected == 0) {
                VS::Menu::Framework::SetLiveInformationMessage(row->description);
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
    record_cursor_memory = &Battle_unusedMenu9CursorMemory;
    intBuf = &aligned_value_text[2];
    value_right_edge_x = 0x30 * 6;

    for (; i < 8; ++i, ++row) {
        int record_row_y = 0xF0 - (animState * 0x17);

        if (record_row_y < ((i * 0x10) + 0x38)) {
            record_row_y = ((i * 0x10) + 0x38);
        }
        if ((row->selected != 0) && (animState == 8)) {
            Menu_Score_statisticsCursorAnimStep = VS::Menu::Framework::DrawLiveCursor(
                Menu_Score_statisticsCursorAnimStep, ((row->x - 0xC) & 0xFFFF) | ((record_row_y - 8) << 0x10));
        }

        menuItem = VS::Menu::Framework::SetLiveMenuItem(0, row->x, record_row_y, 0x8E, 0, row->title);
        menuItem->gradientState = row->animationState;

        if ((i == 0) && (record_cursor_memory->recordTimePage != 0)) {
            menuItem->isScrollable = 1;
        }

        if ((i == 7) && (record_cursor_memory->recordTimePage != 0xF)) {
            menuItem->isScrollable = 2;
        }

        if ((menuItem->isScrollable != 0) && (animState == 8)) {
            VS::Menu::Score::RenderLiveScrollableRow(menuItem);
        } else {
            VS::Menu::Framework::DrawLiveMenuItem(menuItem);
        }

        v1 = record_cursor_memory->recordTimePage + i;

        if (v1 == 0) {
            VS::Core::String::FormatLiveGameInteger(intBuf, Menu_Score_clearCount);
        } else if (v1 == 1) {
            VS::Menu::Score::FormatLiveFastestClearTime(std::span(aligned_value_text).subspan(2),
                static_cast<std::int32_t>(Audio_Seq_unusedScoredata.clearCount),
                static_cast<std::int32_t>(Audio_Seq_unusedScoredata.completionTimeMinutes));
        } else if (v1 == 2) {
            VS::Core::String::FormatLiveGamePercent(intBuf, (Menu_Score_openedChestCount * 100) / 52);
        } else if (v1 == 3) {
            VS::Core::String::FormatLiveGamePercent(intBuf, (Audio_Seq_unusedScoredata.openedChestCount * 100) / 52);
        } else if (v1 == 4) {
            VS::Core::String::FormatLiveGamePercent(intBuf, (Menu_Score_mapCompletion * 100) / 361);
        } else if (v1 == 5) {
            VS::Core::String::FormatLiveGamePercent(intBuf, Audio_Seq_unusedScoredata.mapCompletion * 100 / 361);
        } else if (v1 == 6) {
            VS::Core::String::FormatLiveGameInteger(intBuf, Audio_Seq_unusedScoredata.maxChain);
        } else if (v1 < 13) {
            VS::Core::String::FormatLiveGameInteger(intBuf, Audio_Seq_unusedScoredata.enemyKills[v1 - 7]);
        } else {
            v1 -= 12;
            if (v1 == 10) {
                v1 = 0;
            }
            VS::Core::String::FormatLiveGameInteger(intBuf, Audio_Seq_unusedScoredata.weaponAttacks[v1]);
        }

        v1 = value_right_edge_x - (VS::Render::MeasureLiveEncodedTextLine(intBuf) * 6);
        ++value_right_edge_x;
        --value_right_edge_x;
        aligned_value_text[0] = 0xFA;
        aligned_value_text[1] = v1 - (row->x - 0x72);
        menuItem = VS::Menu::Framework::SetLiveMenuItem(0, row->x + 0x8E, record_row_y, 0x8E, 0, aligned_value_text);
        menuItem->gradientState = 8 - row->animationState;

        if ((i == 0) && (record_cursor_memory->recordTimePage != 0)) {
            menuItem->isScrollable = 1;
        }
        if ((i == 7) && (record_cursor_memory->recordTimePage != 0xF)) {
            menuItem->isScrollable = 2;
        }

        if ((menuItem->isScrollable != 0) && (animState == 8)) {
            VS::Menu::Score::RenderLiveScrollableRow(menuItem);
        } else {
            VS::Menu::Framework::DrawLiveMenuItem(menuItem);
        }
    }

    VS::Menu::Framework::LiveMenuItem(0)->state = 0;
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
}

}  // extern "C"
