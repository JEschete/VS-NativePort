// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Feedback.h"
#include "Battle/Targeting.h"
#include "Content/StringTables.h"
#include "Core/Memory/Copy.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Rename.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/UiPrimitives.h"

// decomp: D_80105DB0 (D_80105DB0@src/MENU/MENU8.PRG/21A0.c)
char Menu_Rename_navIconPulseRequest = 0;

// decomp: _1 (_1@src/MENU/MENU8.PRG/21A0.c)
char Menu_Rename_layoutPadding2;

// decomp: _animStep (_animStep@src/MENU/MENU8.PRG/21A0.c)
u_char Menu_Rename_animStep;

// decomp: _charTable (_charTable@src/MENU/MENU8.PRG/21A0.c)
char* Menu_Rename_charTable;

// decomp: local static of _renderRenameMenu, at file scope so the module reset reaches it
static char Menu_Rename_frameControllerActionAnimationCounters[8] = { 0 };

// decomp: local static of _renameMenuInput, at file scope so the module reset reaches it
static char Menu_Rename_nameEditorNameBuffer[24];

// decomp: _nameNavIconAnim (_nameNavIconAnim@src/MENU/MENU8.PRG/21A0.c)
char Menu_Rename_nameNavIconAnim = 0;

// decomp: _renderRenameMenuStrings (_renderRenameMenuStrings@src/MENU/MENU8.PRG/21A0.c)
u_short Menu_Rename_renderRenameMenuStrings[208]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _selectedControllerAction (_selectedControllerAction@src/MENU/MENU8.PRG/21A0.c)
u_char Menu_Rename_selectedControllerAction = 0;

// decomp: _showRenameInterface (_showRenameInterface@src/MENU/MENU8.PRG/21A0.c)
char Menu_Rename_showRenameInterface;

// decomp: _stringBuf (_stringBuf@src/MENU/MENU8.PRG/21A0.c)
char Menu_Rename_stringBuf[20];

// decomp: local static of _confirmationScreen, at file scope so the module reset reaches it
static char Menu_Rename_confirmationCursorState = 0;

// decomp: local static of _renderRenameMenu, at file scope so the module reset reaches it
static char Menu_Rename_frameNamePlaceHolder[] = { Core_TextCode_Terminator, Core_TextCode_Terminator,
        Core_TextCode_Terminator, Core_TextCode_Terminator };

// decomp: weaponId (weaponId@src/MENU/MENU8.PRG/21A0.c)
u_char Menu_Rename_weaponId;

// decomp: MENU8.PRG _insertDeleteHandler (src/MENU/MENU8.PRG/21A0.c:29)
int Menu_Rename_EditNameAtCursor(int bufPos)
{
    int previousPos;
    int i;

    Menu_Rename_navIconPulseRequest = 1;
    previousPos = bufPos;

    if (Input_buttonRepeat & PADRup) {

        Menu_PlayChangeSound();

        for (i = 19; bufPos < i; --i) {
            Menu_Rename_stringBuf[i] = Menu_Rename_stringBuf[i - 1];
        }

        Menu_Rename_stringBuf[bufPos] = Core_TextCode_Space;

    } else if (Input_buttonRepeat & PADRleft) {

        Menu_PlayChangeSound();

        for (i = bufPos; i < 19; ++i) {
            Menu_Rename_stringBuf[i] = Menu_Rename_stringBuf[i + 1];
        }

        Menu_Rename_stringBuf[19] = Core_TextCode_Space;

    } else if ((Input_buttonsState & (PADL1 | PADR1)) != (PADL1 | PADR1)) {

        if (Input_buttonRepeat & PADL1) {
            if (bufPos == 0) {
                if (Input_buttonsPressed.allButtons & PADL1) {
                    bufPos = 19;
                }
            } else {
                --bufPos;
            }
        }

        if (Input_buttonRepeat & PADR1) {
            if (bufPos == 19) {
                if (Input_buttonsPressed.allButtons & PADR1) {
                    bufPos = 0;
                }
            } else {
                ++bufPos;
            }
        }
    }

    if (bufPos != previousPos) {
        Menu_PlayChangeSound();
    }

    return bufPos;
}

// decomp: MENU8.PRG _copyNormalizedString (src/MENU/MENU8.PRG/21A0.c:92)
int Menu_Rename_CopyTrimmedName(char* stringBuf)
{
    int pos;
    int i;

    for (pos = 0, i = 0; i < 20; ++pos, ++i) {
        if (Menu_Rename_stringBuf[i] == Core_TextCode_Terminator) {
            break;
        }
        stringBuf[pos] = Menu_Rename_stringBuf[i];
    }

    stringBuf[pos--] = Core_TextCode_Terminator;

    while (stringBuf[pos] == Core_TextCode_Space) {
        stringBuf[pos--] = Core_TextCode_Terminator;
        if (pos < 0) {
            return 1;
        }
    }

    pos += 2;

    while (pos < 24) {
        stringBuf[pos++] = Core_TextCode_Zero;
    }

    return 0;
}

// decomp: MENU8.PRG _renderRenameMenu (src/MENU/MENU8.PRG/21A0.c:133)
void Menu_Rename_DrawFrame(int xOffset)
{
    static int Menu_Rename_frameBackgroundHexCoords[] = { vs_getXY(216, 72), vs_getXY(142, 180),
        vs_getXY(112, 72), vs_getXY(112, 224), vs_getXY(216, 72), vs_getXY(216, 224),
        vs_getXY(142, 180), vs_getXY(112, 224), vs_getXY(216, 72), vs_getXY(290, 180),
        vs_getXY(216, 224), vs_getXY(320, 224), vs_getXY(216, 72), vs_getXY(320, 72),
        vs_getXY(290, 180), vs_getXY(320, 224) };

    // Seemingly no purpose, perhaps an unimplemented animation
    static char Menu_Rename_unusedFrameCycle = 0;

    u_int* frame_ordering_table;
    int x;
    int y;
    int j;
    int i;
    char* commandString;
    u_long* prim;
    int counter;

    frame_ordering_table = (u_int*)Render_frameScratch.orderingTable - 1;

    Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Cross, (-xOffset + 16), 50, 0);
    Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, (-xOffset + 16), 66, 0);
    Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Triangle, (-xOffset + 16), 82, 0);
    prim = Render_QueueDefaultSprite(
        vs_getWH(16, 16), ((-xOffset + 16) & 0xFFFF) | (34 << 16));
    prim[4] = vs_getUV0Clut(176, 112, 944, 223);

    prim += 6;

    Menu_Rename_frameControllerActionAnimationCounters[Menu_Rename_selectedControllerAction] = 8;
    Menu_Rename_selectedControllerAction = 0;

    for (i = 1; i < 5; ++i) {

        counter = Menu_Rename_frameControllerActionAnimationCounters[i];

        if (Menu_Rename_frameControllerActionAnimationCounters[i] != 0) {
            --Menu_Rename_frameControllerActionAnimationCounters[i];
        }

        x = Menu_Framework_GradientStopColor(8 - counter, 0, 128) | 0x50000000;
        y = Menu_Framework_GradientStopColor(counter, 0, 128);

        for (j = 0; j < 12; ++j) {
            int background_right_inset;

            prim[0] = (*frame_ordering_table & 0xFFFFFF) | 0x06000000;
            prim[1] = vs_getTpage(0, 0, 0, 0, 1);
            prim[2] = x;
            prim[3] = ((-xOffset) & 0xFFFF) | (j + 20 + i * 16) << 16;
            prim[4] = y;

            background_right_inset = j - 96;

            prim[5] = ((-xOffset - background_right_inset) & 0xFFFF) | (j + 20 + i * 16) << 16;
            prim[6] = vs_getTpage(0, 0, 0, 0, 0);

            *frame_ordering_table = LegacyGpu_Handle(prim);

            prim += 7;
        }
    }

    Render_frameScratch.primitiveCursor = prim;

    Render_SetMenuFontStyle(4);

    for (i = 0; i < 4; ++i) {

        commandString = (char*)&Menu_Rename_renderRenameMenuStrings
            [Menu_Rename_renderRenameMenuStrings[Text_Menu8_rename_Index_insert - i]];
        x = 40;

        while ((j = *commandString++) != Core_TextCode_Terminator) {
            Render_DrawVariableWidthGlyph(
                j, x - xOffset, 84 - (i * 16), (u_long *)(((u_char*)(Render_frameScratch.orderingTable)) - 0x14));

            x += 6;
        }
    }

    Menu_Framework_SetMenuItem(29, xOffset + 156, 48, 164, 0, Menu_Rename_frameNamePlaceHolder)->selected = 1;

    for (j = 0; j < 20; ++j) {
        counter = Menu_Rename_stringBuf[j];

        if (counter != Core_TextCode_Space) {

            int name_character_x = (j * 6 + 0xA2);

            Render_DrawVariableWidthGlyph(
                counter, xOffset + name_character_x, 0x30, (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) - 8));
        }
    }

    if (xOffset == 0) {
        u_long* frame_overlay_ordering_table;

        int color = vs_getRGB888(64, 64, 64);
        x = 0x37F9;

        if ((Menu_Rename_navIconPulseRequest != 0) || (Menu_Rename_nameNavIconAnim != 11)) {

            color = vs_getRGB888(128, 128, 128);
            ++Menu_Rename_nameNavIconAnim;
            --x;

            if (Menu_Rename_nameNavIconAnim > 11) {
                Menu_Rename_nameNavIconAnim = 0;
            }
        }

        Menu_Rename_navIconPulseRequest = 0;
        j = (Menu_Rename_nameNavIconAnim >> 2);

        Render_DrawCompactTextColor("1", vs_getXY(152, 51), color, 0);
        Render_DrawCompactTextColor("L", vs_getXY(146, 51), color, 0);
        Render_QueueDefaultSprite(vs_getWH(16, 16), (136 - j) | (48 << 16))[4] =
            (x << 0x10) | vs_getUV(0, 48);
        Render_DrawCompactTextColor("1", vs_getXY(292, 51), color, 0);
        Render_DrawCompactTextColor("R", vs_getXY(286, 51), color, 0);
        Render_QueueDefaultSprite(vs_getWH(16, 16), (j + 296) | (48 << 16))[4] =
            (x << 0x10) | vs_getUV(16, 48);

        frame_overlay_ordering_table = (u_long *)(Render_frameScratch.overlayOrderingTable);
        prim = (u_long *)(Render_frameScratch.primitiveCursor);

        prim[0] = ((u_long)frame_overlay_ordering_table[0] & 0xFFFFFF) | 0x09000000;
        prim[1] = vs_getTpage(0, 0, 0, 0, 1);
        prim[2] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyG4, 0, 0, 0);
        prim[3] = vs_getRGB888(208, 0, 30);
        prim[4] = 0;
        prim[5] = vs_getRGB888(232, 0, 32);
        prim[6] = 0xC0A080;
        prim[7] = vs_getRGB888(232, 0, 46);
        prim[8] = 0;
        prim[9] = vs_getRGB888(0, 1, 30);

        ((u_long*)Render_frameScratch.overlayOrderingTable)[0] = LegacyGpu_Handle(prim);
        Render_frameScratch.primitiveCursor = prim + 10;

        if (++Menu_Rename_unusedFrameCycle >= 12) {
            Menu_Rename_unusedFrameCycle = 0;
        }
    }

    commandString = (char*)(Menu_Rename_renderRenameMenuStrings + Text_Menu8_rename_Offset_charTable);

    for (i = 0; i < 9; ++i) {
        for (j = 0; j < 14; ++j) {

            int character_grid_origin_x = (xOffset + 0x82);

            Render_DrawVariableWidthGlyph(commandString[i * 15 + j],
                character_grid_origin_x + j * 12, (i * 16) + 78, (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) - 8));
        }
    }

    for (j = 0; j < 16; j += 4) {
        prim = (u_long *)(Render_frameScratch.primitiveCursor);

        prim[0] = (*frame_ordering_table & 0xFFFFFF) | 0x09000000;
        prim[1] = vs_getTpage(0, 0, 0, 0, 1);
        prim[2] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyG4, 0, 65, 107);
        prim[3] = xOffset + Menu_Rename_frameBackgroundHexCoords[j];
        prim[4] = vs_getRGB888(0, 5, 51);
        prim[5] = xOffset + Menu_Rename_frameBackgroundHexCoords[j + 1];
        prim[6] = vs_getRGB888(0, 5, 51);
        prim[7] = xOffset + Menu_Rename_frameBackgroundHexCoords[j + 2];
        prim[8] = vs_getRGB888(0, 5, 51);
        prim[9] = xOffset + Menu_Rename_frameBackgroundHexCoords[j + 3];

        *frame_ordering_table = LegacyGpu_Handle(prim);
        Render_frameScratch.primitiveCursor = (u_long*)(prim + 10);
    }
}

// decomp: MENU8.PRG _highlightSelection (src/MENU/MENU8.PRG/21A0.c:318)
void Menu_Rename_DrawSelectionBox(int x, int y, int w, int h)
{
    u_long* selection_box_primitive;
    u_long* selection_box_ordering_table;

    selection_box_ordering_table = (u_long *)(Render_frameScratch.overlayOrderingTable);
    selection_box_primitive = (u_long *)(Render_frameScratch.primitiveCursor);

    selection_box_primitive[0] = (*selection_box_ordering_table & 0xFFFFFF) | 0x09000000;
    selection_box_primitive[1] = vs_getTpage(0, 0, 0, Render_BlendMode_Add, Render_Dithering_On);
    selection_box_primitive[2] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyG4SemiTrans, 128, 160, 192);
    selection_box_primitive[3] = (x | (y << 16));
    selection_box_primitive[4] = 0;
    selection_box_primitive[5] = (x + w) | (y << 16);
    selection_box_primitive[6] = 0;
    selection_box_primitive[7] = x | ((y + h) << 16);
    selection_box_primitive[8] = vs_getRGB888(128, 160, 192);
    selection_box_primitive[9] = (x + w) | ((y + h) << 16);

    *selection_box_ordering_table = LegacyGpu_Handle(selection_box_primitive);
    Render_frameScratch.primitiveCursor = selection_box_primitive + 10;
}

// decomp: MENU8.PRG _highlightCharSelection (src/MENU/MENU8.PRG/21A0.c:346)
int Menu_Rename_HighlightGridCharacter(int cursorState, int column, int row)
{
    int x;
    int y;

    x = column * 12;
    y = row * 16;

    Menu_Rename_DrawSelectionBox(x + 128, y + 76, 10, 18);

    return Menu_Framework_DrawCursor(cursorState, (x + 112) | ((y + 66) << 16));
}

// decomp: MENU8.PRG _confirmationScreen (src/MENU/MENU8.PRG/21A0.c:364)
int Menu_Rename_UpdateConfirmation(int init)
{
    static int Menu_Rename_confirmationLayoutPadding[] __attribute__((unused)) = { 0x01002AFA, 0x00E79CF1 }; // Junk string?

    static char Menu_Rename_confirmationState;
    static char Menu_Rename_confirmationSelectedRow;
    static char Menu_Rename_confirmationLayoutPadding2[14] __attribute__((unused));

    int i;

    if (init != 0) {
        Menu_Rename_confirmationSelectedRow = 0;
        Menu_Rename_confirmationState = 0;
    }

    switch (Menu_Rename_confirmationState) {
    case 0:
    case 1:
    case 2: {
        Menu_Framework_MenuRow* menuItem =
            Menu_Framework_SetMenuItem(Menu_Rename_confirmationState + 20, 320, (Menu_Rename_confirmationState * 16) + 128, 0x7E, 0,
                (char*)&Menu_Rename_renderRenameMenuStrings
                    [Menu_Rename_renderRenameMenuStrings[Menu_Rename_confirmationState + Text_Menu8_rename_Index_optionYes]]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        ++Menu_Rename_confirmationState;
        break;
    }

    case 3:
        Menu_Rename_confirmationState += Menu_Framework_AreMenuRowsIdle();
        break;

    case 4:
        for (i = 0; i < 3; ++i) {
            Menu_Framework_MenuItem(i + 20)->selected = (Menu_Rename_confirmationSelectedRow ^ i) == 0;
        }

        if (Input_buttonsPressed.allButtons & (PADRright | PADRdown)) {

            for (i = 0; i < 3; ++i) {
                Menu_Framework_FlyOutRowRight(i + 20);
            }

            if (Input_buttonsPressed.allButtons & PADRdown) {
                Menu_Rename_confirmationSelectedRow = 1;
            }

            return Menu_Rename_confirmationSelectedRow + 1;
        }

        i = Menu_Rename_confirmationSelectedRow;

        if (Input_buttonRepeat & PADLup) {
            i += 2;
        }

        if (Input_buttonRepeat & PADLdown) {
            ++i;
        }

        if (i >= 3) {
            i -= 3;
        }

        if (i != Menu_Rename_confirmationSelectedRow) {
            Menu_PlayChangeSound();
            Menu_Rename_confirmationSelectedRow = i;
        }

        Menu_Rename_confirmationCursorState = Menu_Framework_DrawMainMenuCursor(Menu_Rename_confirmationCursorState, i + 7);
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _updateMenuSelection (src/MENU/MENU8.PRG/21A0.c:445)
void Menu_Rename_SetCommandHighlight(int row)
{
    int i;

    if (row < 4) {
        Menu_Rename_selectedControllerAction = row + 1;
    }

    for (i = 4; i < 9; ++i) {
        Menu_Framework_MenuItem(i + 29)->selected = i == row;
    }
}

// decomp: MENU8.PRG _renameMenuInput (src/MENU/MENU8.PRG/21A0.c:463)
int Menu_Rename_UpdateNameEditor(int initialize)
{
    enum Menu_Rename_NameEditorState {
        Menu_Rename_NameEditorState_Init,
        Menu_Rename_NameEditorState_State1,
        Menu_Rename_NameEditorState_State2,
        Menu_Rename_NameEditorState_LeaveRenameScreen,
        Menu_Rename_NameEditorState_InitConfirmationScreen,
        Menu_Rename_NameEditorState_ConfirmationScreen,
        Menu_Rename_NameEditorState_ReturnToRenameScreen
    };

    static char Menu_Rename_nameEditorState;
    static char Menu_Rename_nameEditorCursorAnimState;
    static char Menu_Rename_nameEditorCharTableColumn;
    static char Menu_Rename_nameEditorCharTableRow;
    static u_char Menu_Rename_nameEditorCurrentPos;
    static char Menu_Rename_unusedEditorFlag __attribute__((unused));
    int selectedRow;
    int var_a1;
    int var_s1;
    Items_InventoryWeapon* weapon;

    if (initialize != 0) {
        Menu_Rename_nameEditorCharTableColumn = 0;
        Menu_Rename_nameEditorCharTableRow = 0;
        Menu_Rename_nameEditorCurrentPos = 0;
        Menu_Rename_unusedEditorFlag = 0;
        Menu_Rename_nameEditorCursorAnimState = 8;
        Menu_Rename_nameEditorState = Menu_Rename_NameEditorState_Init;

        return 0;
    }

    switch (Menu_Rename_nameEditorState) {
    case Menu_Rename_NameEditorState_Init:
        // _cursorAnimState miused as a delay
        if (Menu_Rename_nameEditorCursorAnimState != 0) {
            --Menu_Rename_nameEditorCursorAnimState;
        } else {
            Menu_Rename_nameEditorState = 2;
        }
        break;

    case 1:
        Menu_Rename_nameEditorCurrentPos = Menu_Rename_EditNameAtCursor(Menu_Rename_nameEditorCurrentPos);

        if (Input_buttonsPressed.allButtons & PADstart) {
            Menu_PlaySelectSound();
            Menu_Rename_SetCommandHighlight(255);

            Menu_Rename_nameEditorState = Menu_Rename_NameEditorState_LeaveRenameScreen;
            break;
        }

        if (Input_buttonRepeat & (1 << 6)) {
            Menu_PlayChangeSound();

            if (Menu_Rename_nameEditorCurrentPos != 0) {

                --Menu_Rename_nameEditorCurrentPos;

                for (var_s1 = Menu_Rename_nameEditorCurrentPos; var_s1 < 19; ++var_s1) {
                    Menu_Rename_stringBuf[var_s1] = Menu_Rename_stringBuf[var_s1 + 1];
                }

                Menu_Rename_stringBuf[19] = 0x8F;
            }
        }

        var_s1 = Menu_Rename_nameEditorCharTableRow;

        if (Input_buttonsPressed.allButtons & (1 << 13)) {

            Menu_PlayChangeSound();
            Menu_Rename_SetCommandHighlight(255);

            if (var_s1 == 3) {
                var_s1 = 1;
            } else {
                var_s1 = 0;
            }

            Menu_Rename_nameEditorCharTableRow = var_s1;
            Menu_Rename_nameEditorState = 2;

            break;
        }

        if (Input_buttonsPressed.allButtons & (1 << 5)) {

            switch (var_s1) {
            case 0:
                Menu_PlaySelectSound();
                Menu_Rename_SetCommandHighlight(255);
                Menu_Rename_nameEditorState = 3;
                break;

            case 1:
                if (Menu_Rename_nameEditorCurrentPos != 0) {

                    --Menu_Rename_nameEditorCurrentPos;

                    for (var_a1 = Menu_Rename_nameEditorCurrentPos; var_a1 < 19; ++var_a1) {
                        Menu_Rename_stringBuf[var_a1] = Menu_Rename_stringBuf[var_a1 + 1];
                    }

                    Menu_Rename_stringBuf[19] = Core_TextCode_Space;
                }

                break;

            case 2:
                for (var_a1 = Menu_Rename_nameEditorCurrentPos; var_a1 < 0x13; ++var_a1) {
                    Menu_Rename_stringBuf[var_a1] = Menu_Rename_stringBuf[var_a1 + 1];
                }

                Menu_Rename_stringBuf[19] = Core_TextCode_Space;
                break;

            case 3:
                for (var_a1 = 0x13; Menu_Rename_nameEditorCurrentPos < var_a1; --var_a1) {
                    Menu_Rename_stringBuf[var_a1] = Menu_Rename_stringBuf[var_a1 - 1];
                }

                Menu_Rename_stringBuf[Menu_Rename_nameEditorCurrentPos] = Core_TextCode_Space;
                break;
            }

            if (Menu_Rename_nameEditorState != 1) {
                break;
            }

            Menu_PlayChangeSound();
        }

        if (Input_buttonRepeat & (1 << 12)) {
            if (var_s1 == 0) {
                if (Input_buttonsPressed.allButtons & (1 << 12)) {
                    var_s1 = 3;
                }
            } else {
                --var_s1;
            }
        }

        if (Input_buttonRepeat & (1 << 14)) {
            if (var_s1 == 3) {
                if (Input_buttonsPressed.allButtons & (1 << 14)) {
                    var_s1 = 0;
                }
            } else {
                ++var_s1;
            }
        }

        if (var_s1 != Menu_Rename_nameEditorCharTableRow) {
            Menu_PlayChangeSound();
            Menu_Rename_nameEditorCharTableRow = var_s1;
        }

        Menu_Rename_SetCommandHighlight(var_s1);

        Menu_Rename_nameEditorCursorAnimState =
            Menu_Framework_DrawCursor(Menu_Rename_nameEditorCursorAnimState, ((var_s1 * 16) + 28) << 16);
        break;

    case 2:
        Menu_Rename_nameEditorCurrentPos = Menu_Rename_EditNameAtCursor(Menu_Rename_nameEditorCurrentPos);

        if (Input_buttonsPressed.allButtons & (1 << 11)) {
            Menu_PlaySelectSound();
            Menu_Rename_nameEditorState = 3;
            break;
        }

        if (Input_buttonsPressed.allButtons & (1 << 5)) {

            Menu_PlayChangeSound();

            var_s1 = Menu_Rename_charTable[Menu_Rename_nameEditorCharTableColumn + (Menu_Rename_nameEditorCharTableRow * 15)];
            Menu_Rename_stringBuf[Menu_Rename_nameEditorCurrentPos] = var_s1;

            if (Menu_Rename_nameEditorCurrentPos < 19) {
                ++Menu_Rename_nameEditorCurrentPos;
            }
        }

        if (Input_buttonRepeat & (1 << 6)) {

            Menu_PlayChangeSound();

            if (Menu_Rename_nameEditorCurrentPos != 0) {
                --Menu_Rename_nameEditorCurrentPos;
                for (var_s1 = Menu_Rename_nameEditorCurrentPos; var_s1 < 19; ++var_s1) {
                    Menu_Rename_stringBuf[var_s1] = Menu_Rename_stringBuf[var_s1 + 1];
                }

                Menu_Rename_stringBuf[19] = Core_TextCode_Space;
            }
        }

        var_a1 = Menu_Rename_nameEditorCharTableColumn + (Menu_Rename_nameEditorCharTableRow * 14);

        if (Input_buttonRepeat & (1 << 12)) {
            if (Menu_Rename_nameEditorCharTableRow != 0) {
                --Menu_Rename_nameEditorCharTableRow;
            } else if (Input_buttonsPressed.allButtons & (1 << 12)) {
                Menu_Rename_nameEditorCharTableRow = 8;
            }
        }

        if (Input_buttonRepeat & (1 << 14)) {
            if (Menu_Rename_nameEditorCharTableRow == 8) {
                if (Input_buttonsPressed.allButtons & (1 << 14)) {
                    Menu_Rename_nameEditorCharTableRow = Menu_Rename_nameEditorState != 2;
                }
            } else {
                ++Menu_Rename_nameEditorCharTableRow;
            }
        }

        if (Input_buttonRepeat & (1 << 15)) {
            if (Menu_Rename_nameEditorCharTableColumn == 0) {
                if (Input_buttonsPressed.allButtons & (1 << 15)) {

                    Menu_PlayChangeSound();

                    if (Menu_Rename_nameEditorCharTableRow == 0) {
                        Menu_Rename_nameEditorCharTableRow = 2;
                    } else {
                        Menu_Rename_nameEditorCharTableRow = 3;
                    }

                    Menu_Rename_nameEditorState = 1;
                    break;
                }
            } else {
                --Menu_Rename_nameEditorCharTableColumn;
            }
        }

        if (Input_buttonRepeat & (1 << 13)) {
            if (Menu_Rename_nameEditorCharTableColumn == 13) {
                if (Input_buttonsPressed.allButtons & (1 << 13)) {
                    Menu_Rename_nameEditorCharTableColumn = 0;
                }
            } else {
                ++Menu_Rename_nameEditorCharTableColumn;
            }
        }

        if (var_a1 != (Menu_Rename_nameEditorCharTableColumn + (Menu_Rename_nameEditorCharTableRow * 14))) {
            Menu_PlayChangeSound();
        }

        break;

    case Menu_Rename_NameEditorState_LeaveRenameScreen:
        Menu_Rename_SetCommandHighlight(0);

        Menu_Rename_showRenameInterface = 0;
        Menu_Rename_nameEditorState = Menu_Rename_NameEditorState_InitConfirmationScreen;
        break;

    case Menu_Rename_NameEditorState_InitConfirmationScreen:
        if (Menu_Rename_animStep != 10) {
            break;
        }

        if (Menu_Rename_CopyTrimmedName(Menu_Rename_nameEditorNameBuffer) != 0) {
            Core_Memory_CopyBytesBackward(Menu_Rename_nameEditorNameBuffer,
                Menu_Framework_itemNames
                    + Audio_Seq_unusedInventory
                        .blades[Audio_Seq_unusedInventory.weapons[Menu_Rename_weaponId].blade - 1]
                        .id,
                0x18);
        }

        Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(1));

        Battle_stringContext.strings[0] = Menu_Rename_nameEditorNameBuffer;

        Render_FormatEncodedText(Menu_Framework_stringBuf,
            (char*)(Menu_Rename_renderRenameMenuStrings + Text_Menu8_rename_Offset_confirmPrompt));

        Menu_Framework_SetInformationMessage(Menu_Framework_stringBuf);
        Menu_Rename_UpdateConfirmation(1);

        Menu_Rename_nameEditorState = Menu_Rename_NameEditorState_ConfirmationScreen;
        break;

    case Menu_Rename_NameEditorState_ConfirmationScreen:
        selectedRow = Menu_Rename_UpdateConfirmation(0);

        if (selectedRow == 0) {
            break;
        }

        Menu_DismissTextBox(7);

        if (selectedRow == 2) {
            Menu_PlayLeaveSound();
            Menu_Rename_showRenameInterface = 1;
            Menu_Rename_nameEditorState = Menu_Rename_NameEditorState_ReturnToRenameScreen;
            break;
        }

        weapon = &Audio_Seq_unusedInventory.weapons[Menu_Rename_weaponId];

        if (selectedRow == 1) {
            Audio_PlaySfxCentered(0x7E, 0xD);
            Core_Memory_CopyBytesBackward(weapon->name, Menu_Rename_nameEditorNameBuffer, sizeof Menu_Rename_nameEditorNameBuffer);

            if (weapon->isEquipped != 0) {
                Items_EquipAshleyWeapon(weapon);
            }
        } else {
            Menu_PlayLeaveSound();

            if (weapon->name[0] == Core_TextCode_Terminator) {
                Core_Memory_CopyBytesBackward(weapon->name,
                    Menu_Framework_itemNames
                        + Audio_Seq_unusedInventory.blades[weapon->blade - 1].id,
                    sizeof weapon->name);
            }
        }

        return 1;

    case Menu_Rename_NameEditorState_ReturnToRenameScreen:
        if (Menu_Rename_animStep == 0) {
            Menu_Rename_nameEditorCharTableColumn = 0;
            Menu_Rename_nameEditorCharTableRow = 0;
            Menu_Rename_nameEditorState = 2;
        }
        break;
    }

    if (Menu_Rename_nameEditorState == 2) {
        Menu_Rename_nameEditorCursorAnimState =
            Menu_Rename_HighlightGridCharacter(Menu_Rename_nameEditorCursorAnimState, Menu_Rename_nameEditorCharTableColumn, Menu_Rename_nameEditorCharTableRow);
    }

    if (((Menu_Rename_nameEditorState - 1)) < 2U) {
        Menu_Rename_DrawSelectionBox((Menu_Rename_nameEditorCurrentPos * 6) + 161, 48, 8, 12);
    }

    return 0;
}

// decomp: MENU8.PRG vs_menu8_execRename (src/MENU/MENU8.PRG/21A0.c:820)
int Menu_Rename_Update(char* state)
{
    int i;
    int currentState;
    char* weapon_name_cursor;
    char* name_padding_cursor;
    int encoded_space;

    currentState = *state;

    switch (*state) {
    case 0:
        Menu_Rename_weaponId = Menu_Framework_stringBuf[1];
        Menu_Rename_charTable = (char*)(Menu_Rename_renderRenameMenuStrings + Text_Menu8_rename_Offset_charTable);

        Menu_Framework_DismissInformationBox();

        encoded_space = Core_TextCode_Space;
        i = 19;
        name_padding_cursor = &Menu_Rename_stringBuf[i];

        for (; i >= 0; --i) {
            *name_padding_cursor-- = encoded_space;
        }

        if (Menu_Framework_stringBuf[0] == 1) {
            weapon_name_cursor = Audio_Seq_unusedInventory.weapons[Menu_Rename_weaponId].name;
            for (i = 0; i < 20; ++i) {
                int c = *weapon_name_cursor++;
                if (c == Core_TextCode_Terminator) {
                    break;
                } else if (c == Core_TextCode_Space) {
                    continue;
                } else if (c == Core_TextCode_Spacing) {
                    ++weapon_name_cursor;
                } else {
                    Menu_Rename_stringBuf[i] = c;
                }
            }
        }

        Menu_Rename_showRenameInterface = 0;
        Menu_Rename_animStep = 10;
        *state = 1;
        break;

    case 1:
        Menu_Rename_UpdateNameEditor(1);

        Menu_Rename_showRenameInterface = 1;
        *state = 2;
        break;

    case 2:
        if (Menu_Rename_UpdateNameEditor(0) != 0) {
            Menu_Framework_FlyOutRowRight(0xA);
            Menu_Framework_FlyOutRowLeft(0);
            Menu_Framework_InitializeInformationBox();

            *state = 3;
        }
        break;

    case 3:
        if (Menu_Rename_animStep == 10) {
            *state = 0;
            Menu_Framework_menuState.currentMenu = 13;
        }

        break;
    }

    if (Menu_Rename_showRenameInterface != 0) {
        if (Menu_Rename_animStep != 0) {
            --Menu_Rename_animStep;
        }
        Menu_Rename_DrawFrame(Menu_Framework_rowAnimationSteps[Menu_Rename_animStep]);
    } else if (Menu_Rename_animStep < 10) {
        ++Menu_Rename_animStep;
        Menu_Rename_DrawFrame(Menu_Rename_animStep << 5);
    }

    return 0;
}

// MENU8.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Rename_ResetMenu8Statics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Rename_navIconPulseRequest, &initial, sizeof Menu_Rename_navIconPulseRequest);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Rename_nameNavIconAnim, &initial, sizeof Menu_Rename_nameNavIconAnim);
    }
    {
        static const u_char initial = 0;
        memcpy((void*)&Menu_Rename_selectedControllerAction, &initial, sizeof Menu_Rename_selectedControllerAction);
    }
    {
        static const char initial[8] = { 0 };
        memcpy((void*)&Menu_Rename_frameControllerActionAnimationCounters, &initial, sizeof Menu_Rename_frameControllerActionAnimationCounters);
    }
    {
        static const char initial[] = { Core_TextCode_Terminator, Core_TextCode_Terminator,
        Core_TextCode_Terminator, Core_TextCode_Terminator };
        memcpy((void*)&Menu_Rename_frameNamePlaceHolder, &initial, sizeof Menu_Rename_frameNamePlaceHolder);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Rename_confirmationCursorState, &initial, sizeof Menu_Rename_confirmationCursorState);
    }
    {
        static const char initial[24] = {};
        memcpy((void*)&Menu_Rename_nameEditorNameBuffer, &initial, sizeof Menu_Rename_nameEditorNameBuffer);
    }
}

}  // extern "C"
