// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Music.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Battle/Feedback.h"
#include "Battle/TextBoxes.h"
#include "Boot/Lifecycle.h"
#include "CdDrive.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Core/Random.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Event/ScriptAudio.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/RoomRendering.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Files.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/TextureAnimation.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "Save/MenuAssets.h"
#include "Save/NativeMenu.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"
#include "Title/TitleMenu.h"

// decomp: _0 (_0@src/MENU/MENU7.PRG/260.c)
char Menu_Files_layoutPadding1[4];

// decomp: _1 (_1@src/MENU/MENU7.PRG/260.c)
char Menu_Files_layoutPadding2[1];

// decomp: _2 (_2@src/MENU/MENU7.PRG/260.c)
char Menu_Files_layoutPadding3[8];

// decomp: _backupMainSetting (_backupMainSetting@src/MENU/MENU7.PRG/260.c)
char Menu_Files_backupMainSetting;

// decomp: _containerDataEmpty (_containerDataEmpty@src/MENU/MENU7.PRG/260.c)
char Menu_Files_containerDataEmpty;

// decomp: _containerItemCapacities (_containerItemCapacities@src/MENU/MENU7.PRG/260.c)
u_short Menu_Files_containerItemCapacities[] = { 32, 64, 64, 32, 64, 192, 256, 2 };

// decomp: _containerStrings (_containerStrings@src/MENU/MENU7.PRG/260.c)
u_short Menu_Files_containerStrings[895]; /* contents: disc string table (ModuleData.c) */

// decomp: _cursorFileOpSaturation (_cursorFileOpSaturation@src/MENU/MENU7.PRG/260.c)
signed char Menu_Files_cursorFileOpSaturation[] = { 0, -56, -112, -104, -96, -88, -80, -72,
    -64, -56, -48, -40, -32, -24, -16, -8 };

// decomp: _dataNotSaved (_dataNotSaved@src/MENU/MENU7.PRG/260.c)
char Menu_Files_dataNotSaved;

// decomp: local static of _drawHPMP, at file scope so the module reset reaches it
static char Menu_Files_hpMpSummaryDigitDivisors[] = { 0, 1, 10, 100 };

// decomp: _fileMenuElements (_fileMenuElements@src/MENU/MENU7.PRG/260.c)
Menu_Files_FileMenuElement Menu_Files_fileMenuElements[10];

// decomp: _fileMenuScreenFade (_fileMenuScreenFade@src/MENU/MENU7.PRG/260.c)
char Menu_Files_fileMenuScreenFade;

// decomp: _fileProgressCounter (_fileProgressCounter@src/MENU/MENU7.PRG/260.c)
int Menu_Files_fileProgressCounter;

// decomp: _isSaving (_isSaving@src/MENU/MENU7.PRG/260.c)
char Menu_Files_isSaving;

// decomp: _memoryCardMessage (_memoryCardMessage@src/MENU/MENU7.PRG/260.c)
char* Menu_Files_memoryCardMessage;

// decomp: _menuElementStops (_menuElementStops@src/MENU/MENU7.PRG/260.c)
char Menu_Files_menuElementStops[] = { 0, 1, 2, 4, 8, 16, 32, 56, 80, 104, 128, 152, 176,
    200, 224, 248, 255, 255, 255, 255 };

// decomp: _menuItemTextClut (_menuItemTextClut@src/MENU/MENU7.PRG/260.c)
u_short Menu_Files_menuItemTextClut[][16] = {
    { vs_getRGB5551(0, 0, 0, 0), vs_getRGB5551(1, 1, 1, 1), vs_getRGB5551(1, 1, 3, 1),
        vs_getRGB5551(3, 1, 0, 1), vs_getRGB5551(1, 0, 6, 1), vs_getRGB5551(6, 1, 1, 1),
        vs_getRGB5551(11, 2, 0, 1), vs_getRGB5551(16, 5, 0, 1),
        vs_getRGB5551(19, 8, 0, 1), vs_getRGB5551(23, 10, 0, 1),
        vs_getRGB5551(24, 10, 0, 1), vs_getRGB5551(24, 11, 0, 1),
        vs_getRGB5551(26, 11, 0, 1), vs_getRGB5551(25, 11, 3, 1),
        vs_getRGB5551(27, 14, 0, 1), vs_getRGB5551(27, 13, 2, 1) },
    { vs_getRGB5551(0, 0, 0, 0), vs_getRGB5551(1, 1, 1, 1), vs_getRGB5551(1, 1, 3, 1),
        vs_getRGB5551(3, 1, 0, 1), vs_getRGB5551(1, 0, 6, 1), vs_getRGB5551(6, 1, 1, 1),
        vs_getRGB5551(11, 2, 0, 1), vs_getRGB5551(16, 5, 0, 1),
        vs_getRGB5551(19, 8, 0, 1), vs_getRGB5551(23, 10, 0, 1),
        vs_getRGB5551(24, 10, 0, 1), vs_getRGB5551(24, 11, 0, 1),
        vs_getRGB5551(26, 11, 0, 1), vs_getRGB5551(25, 11, 3, 1),
        vs_getRGB5551(27, 14, 0, 1), vs_getRGB5551(27, 13, 2, 1) },
    { vs_getRGB5551(0, 0, 0, 0), vs_getRGB5551(0, 0, 2, 1), vs_getRGB5551(0, 0, 4, 1),
        vs_getRGB5551(0, 0, 6, 1), vs_getRGB5551(0, 0, 8, 1), vs_getRGB5551(0, 0, 10, 1),
        vs_getRGB5551(0, 0, 12, 1), vs_getRGB5551(0, 0, 14, 1),
        vs_getRGB5551(0, 0, 15, 1), vs_getRGB5551(0, 0, 17, 1),
        vs_getRGB5551(0, 0, 20, 1), vs_getRGB5551(0, 0, 22, 1),
        vs_getRGB5551(0, 0, 24, 1), vs_getRGB5551(0, 0, 23, 1),
        vs_getRGB5551(0, 0, 29, 1), vs_getRGB5551(0, 0, 31, 1) },
    { vs_getRGB5551(0, 0, 0, 0), vs_getRGB5551(1, 1, 3, 1), vs_getRGB5551(2, 2, 7, 1),
        vs_getRGB5551(4, 3, 2, 1), vs_getRGB5551(5, 4, 7, 1), vs_getRGB5551(8, 6, 6, 1),
        vs_getRGB5551(13, 10, 9, 1), vs_getRGB5551(19, 14, 10, 1),
        vs_getRGB5551(23, 18, 12, 1), vs_getRGB5551(27, 21, 14, 1),
        vs_getRGB5551(29, 22, 15, 1), vs_getRGB5551(31, 23, 13, 1),
        vs_getRGB5551(31, 24, 16, 1), vs_getRGB5551(29, 25, 21, 1),
        vs_getRGB5551(31, 26, 18, 1), vs_getRGB5551(31, 27, 22, 1) }
};

// decomp: local static of _drawPlayTime, at file scope so the module reset reaches it
static char Menu_Files_playTime[] = "00:00:00:00";

// decomp: _primBuf (_primBuf@src/MENU/MENU7.PRG/260.c)
Menu_Files_PrimitiveBuffer Menu_Files_primBuf;

// decomp: _selectCursorColor (_selectCursorColor@src/MENU/MENU7.PRG/260.c)
u_char Menu_Files_selectCursorColor;

// decomp: _selectCursorXy (_selectCursorXy@src/MENU/MENU7.PRG/260.c)
int Menu_Files_selectCursorXy;

// decomp: _textTable (_textTable@src/MENU/MENU7.PRG/260.c)
u_short* Menu_Files_textTable;

// decomp: local static of _printString, at file scope so the module reset reaches it
static char Menu_Files_textArrowState = 0;

// decomp: local static of _intepolateMenuItemBgColour, at file scope so the module reset reaches it
static int Menu_Files_elementBackgroundColorColors1[] = { vs_getRGB888(0x40, 0x30, 0x66),
        vs_getRGB888(0x40, 0x38, 0x20) };

// decomp: local static of _intepolateMenuItemBgColour, at file scope so the module reset reaches it
static int Menu_Files_elementBackgroundColorColors2[] = {
        vs_getRGB888(0x08, 0x08, 0x20),
        vs_getRGB888(0x10, 0x10, 0x08),
    };

// decomp: local static of _promptYesNo, at file scope so the module reset reaches it
static char Menu_Files_containerConfirmationCursorAnimation = 0;

// decomp: _frameBuf (_frameBuf@src/TITLE/TITLE.PRG/22C.c)
char Menu_Files_frameBuf;

// decomp: MENU7.PRG _drawSaveInfoUI (src/MENU/MENU7.PRG/260.c:1222)
void Menu_Files_DrawSummaryLabel(int xy, enum Menu_Files_FileMenuUiId id)
{
    static int Menu_Files_summaryLabelSaveInfoUVClut[] = { vs_getUV0Clut(248, 8, 832, 223),
        vs_getUV0Clut(242, 0, 832, 223), vs_getUV0Clut(224, 120, 848, 223),
        vs_getUV0Clut(128, 56, 848, 223), vs_getUV0Clut(224, 112, 848, 223),
        vs_getUV0Clut(104, 56, 848, 223), vs_getUV0Clut(170, 76, 864, 223),
        vs_getUV0Clut(144, 0, 864, 223), vs_getUV0Clut(160, 0, 864, 223),
        vs_getUV0Clut(32, 9, 832, 223) };
    static int Menu_Files_summaryLabelSaveInfoWh[] = { vs_getXY(3, 8), vs_getXY(13, 8), vs_getXY(17, 8),
        vs_getXY(22, 8), vs_getXY(26, 8), vs_getXY(19, 8), vs_getXY(10, 10),
        vs_getXY(16, 9), vs_getXY(18, 9), vs_getXY(5, 7) };
    Render_DrawImmediateSprite(xy, Menu_Files_summaryLabelSaveInfoUVClut[id], Menu_Files_summaryLabelSaveInfoWh[id], 0xC);
}

// decomp: MENU7.PRG _renderInteger (src/MENU/MENU7.PRG/260.c:1239)
void Menu_Files_DrawInteger(int xy, u_int value, u_int placeDivisor)
{
    do {
        Render_DrawImmediateSprite(xy, vs_getUV0Clut(((value / placeDivisor) * 6), 0, 832, 223),
            vs_getWH(6, 10), getTPage(0, 0, 768, 0));
        value = value % placeDivisor;
        placeDivisor /= 10;
        xy += 5;
    } while (placeDivisor != 0);
}

// decomp: MENU7.PRG _countDigits (src/MENU/MENU7.PRG/260.c:1253)
int Menu_Files_DecimalDigitCount(int val)
{
    int i;

    i = 0;
    do {
        val /= 10;
        ++i;
    } while (val != 0);
    return i;
}

// decomp: MENU7.PRG _drawHPMP (src/MENU/MENU7.PRG/260.c:1270)
void Menu_Files_DrawHpMpSummary(int xy, enum Menu_Files_StatType stat, u_int currentValue, u_int maxValue)
{

    int currentValueFactor;
    int maxValueDigits;
    u_int rgb1;
    u_int currentValueDigits;
    u_int rgb0;
    int wh;
    u_int placeDivisor;
    int x1y1;
    int red;
    int new_var4;

    if (maxValue != 0) {
        currentValueDigits = (((currentValue * 64) + maxValue) - 1) / maxValue;
    } else {
        currentValueDigits = 0;
    }

    rgb0 = ((207 << 16) | (224 << 8)) | 45;
    if (stat != Menu_Files_StatType_HP) {
        rgb0 = ((159 << 16) | (120 << 8)) | 220;
    }

    red = (rgb0 & 0xFF);
    currentValueFactor = 64 - currentValueDigits;
    rgb1 =
        ((((currentValueDigits * 255) + (red * currentValueFactor)) >> 6)
            | ((((currentValueDigits * 240) + (((rgb0 >> 8) & 0xFF) * currentValueFactor))
                   >> 6)
                << 8))
        | ((((2 * (currentValueDigits * 79)) + ((rgb0 >> 16) * currentValueFactor)) >> 6)
            << 16);

    Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.tilePoly, Render_LegacyPrimitiveAddress_Null);
    Menu_Files_primBuf.prim.tilePoly.tileTpage.texturePageCommand =
        vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_On);
    Menu_Files_primBuf.prim.tilePoly.tileTpage.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 40, 64);
    wh = vs_getWH(66, 5);
    x1y1 = (currentValueDigits + vs_getXY(0, 9));
    Menu_Files_primBuf.prim.tilePoly.tileTpage.position = xy++ + vs_getXY(0, 8);
    Menu_Files_primBuf.prim.tilePoly.tileTpage.size = wh;
    Menu_Files_primBuf.prim.tilePoly.polyG4.colorZeroAndCode = vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4, rgb1);
    Menu_Files_primBuf.prim.tilePoly.polyG4.positionZero = xy + vs_getXY(0, 9);
    Menu_Files_primBuf.prim.tilePoly.polyG4.colorOne = rgb0;
    Menu_Files_primBuf.prim.tilePoly.polyG4.positionOne = xy + x1y1;
    Menu_Files_primBuf.prim.tilePoly.polyG4.colorTwo = rgb1;
    Menu_Files_primBuf.prim.tilePoly.polyG4.positionTwo = xy + vs_getXY(0, 12);
    Menu_Files_primBuf.prim.tilePoly.polyG4.colorThree = rgb0;
    Menu_Files_primBuf.prim.tilePoly.polyG4.positionThree = vs_getXY(0, 12);
    Menu_Files_primBuf.prim.tilePoly.polyG4.positionThree =
        xy + (currentValueDigits + Menu_Files_primBuf.prim.tilePoly.polyG4.positionThree);
    DrawPrim(&Menu_Files_primBuf);

    currentValueDigits = Menu_Files_DecimalDigitCount(currentValue);
    maxValueDigits = Menu_Files_DecimalDigitCount(maxValue);
    Menu_Files_DrawSummaryLabel(xy - 1, stat + Menu_Files_FileMenuUiId_Hp);
    wh = currentValueDigits * 6;
    new_var4 = 55;
    xy = (xy - (wh - new_var4)) - (maxValueDigits * 5);
    placeDivisor = Menu_Files_hpMpSummaryDigitDivisors[currentValueDigits];
    do {
        Render_DrawImmediateSprite(xy - vs_getXY(0, 1),
            (((currentValue / placeDivisor) << 3) + 64) | (getClut(832, 223) << 16),
            vs_getWH(7, 12), getTPage(0, 0, 768, 0));
        currentValue = currentValue % placeDivisor;
        placeDivisor /= 10;
        xy += 6;
    } while (placeDivisor != 0);
    Menu_Files_DrawSummaryLabel(xy + 1, Menu_Files_FileMenuUiId_Dot);
    Menu_Files_DrawInteger(xy + 6, maxValue, Menu_Files_hpMpSummaryDigitDivisors[maxValueDigits]);
}

// decomp: MENU7.PRG _fileProcessingAnim (src/MENU/MENU7.PRG/260.c:1353)
void Menu_Files_DrawTransferProgress(int x, int y)
{
    char* new_var __attribute__((unused));
    int gradientColor1;
    int gradientColor2;
    u_int leftEdge;
    u_int i;

    leftEdge = x - 48;
    new_var = &Menu_Files_isSaving;

    if (leftEdge < 64) {
        leftEdge = 64;
    }

    gradientColor1 = 0;
    gradientColor2 = vs_getRGB888(224, 255, 0);

    if (Menu_Files_isSaving != 0) {
        gradientColor2 = vs_getRGB888(255, 0, 127);
    }

    for (i = 0; i < 2; ++i) {
        Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.polyG4Tpage, Render_LegacyPrimitiveAddress_Null);
        Menu_Files_primBuf.prim.polyG4Tpage.colorZeroAndCode =
            vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4SemiTrans, gradientColor1);
        Menu_Files_primBuf.prim.polyG4Tpage.positionZero = leftEdge | y;
        Menu_Files_primBuf.prim.polyG4Tpage.positionOne = x | y;
        Menu_Files_primBuf.prim.polyG4Tpage.positionTwo = leftEdge | (y + vs_getXY(0, 32));
        Menu_Files_primBuf.prim.polyG4Tpage.texturePage =
            vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_On);
        Menu_Files_primBuf.prim.polyG4Tpage.colorOne = gradientColor2;
        Menu_Files_primBuf.prim.polyG4Tpage.colorTwo = gradientColor1;
        Menu_Files_primBuf.prim.polyG4Tpage.colorThree = gradientColor2;
        Menu_Files_primBuf.prim.polyG4Tpage.positionThree = x | (y + vs_getXY(0, 32));
        DrawPrim(&Menu_Files_primBuf);
        leftEdge = x;
        x += 16;
        gradientColor1 = gradientColor2;
        gradientColor2 = 0;
    }
}

// decomp: MENU7.PRG _fileProcessingCompleteAnim (src/MENU/MENU7.PRG/260.c:1400)
void Menu_Files_DrawTransferComplete(int colour, int y)
{
    int yOfst;
    int colour0;
    u_int i;
    int colour1 = 0;

    if (Menu_Files_isSaving != 0) {
        colour0 = colour + (colour << 15);
    } else {
        colour0 = (colour - (colour >> 3)) + (colour << 8);
    }

    Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.tileTpage, Render_LegacyPrimitiveAddress_Null);
    Menu_Files_primBuf.prim.tileTpage.texturePageCommand =
        vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_On);
    Menu_Files_primBuf.prim.tileTpage.colorCommand = vs_getRGB0Raw(Render_LegacyPrimitiveCode_TileSemiTrans, colour0);
    Menu_Files_primBuf.prim.tileTpage.position = y | 64;
    Menu_Files_primBuf.prim.tileTpage.size = vs_getWH(256, 32);
    DrawPrim(&Menu_Files_primBuf);

    for (i = 0; i < 2; ++i) {
        Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.polyG4, Render_LegacyPrimitiveAddress_Null);
        yOfst = y + vs_getXY(0, -8);
        Menu_Files_primBuf.prim.polyG4.colorZeroAndCode = vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4SemiTrans, colour1);
        Menu_Files_primBuf.prim.polyG4.positionZero = yOfst | 64;
        Menu_Files_primBuf.prim.polyG4.colorOne = colour1;
        Menu_Files_primBuf.prim.polyG4.positionOne = yOfst | 320;
        Menu_Files_primBuf.prim.polyG4.colorTwo = colour0;
        Menu_Files_primBuf.prim.polyG4.positionTwo = y | 64;
        Menu_Files_primBuf.prim.polyG4.colorThree = colour0;
        Menu_Files_primBuf.prim.polyG4.positionThree = y | 320;
        DrawPrim(&Menu_Files_primBuf);
        y += vs_getXY(0, 40);
        colour1 = colour0;
        colour0 = 0;
    }
}

// decomp: MENU7.PRG _initFileMenu (src/MENU/MENU7.PRG/260.c:1447)
void Menu_Files_ResetElements(void)
{
    Menu_Files_memoryCardMessage = 0;
    Menu_Files_selectCursorColor = 0;
    Menu_Files_selectCursorXy = 0;
    Menu_Files_fileProgressCounter = 0;
    Menu_Files_fileMenuScreenFade = 0;
    Title_transferProgressTarget = 0x180;
    memset(&Menu_Files_fileMenuElements, 0, sizeof Menu_Files_fileMenuElements);
}

// decomp: MENU7.PRG _initFileMenuElement (src/MENU/MENU7.PRG/260.c:1463)
Menu_Files_FileMenuElement* Menu_Files_InitializeElement(int id, int xy, int wh, char* text)
{
    Menu_Files_FileMenuElement* element;
    int i;
    u_int c;

    element = &Menu_Files_fileMenuElements[id];
    memset(element, 0, sizeof *element);
    element->state = 1;
    element->slotId = -1;
    *(int*)&element->x = xy;
    *(int*)&element->w = wh;

    if (text != 0) {
        for (i = 0; i < 32;) {
            c = *text++;
            if (c == Core_TextCode_Spacing) {
                element->text[i++] = c;
                do {
                    c = *(text++);
                } while (0);
            } else {
                if (c == Core_TextCode_Terminator) {
                    element->text[i] = 0xFF;
                    return element;
                }
                if (c >= Core_TextCode_NonPrinting) {
                    continue;
                }
            }
            element->text[i++] = c;
        }
    } else {
        element->text[0] = 0xFF;
    }
    return element;
}

// decomp: MENU7.PRG _clearFileMenuElement (src/MENU/MENU7.PRG/260.c:1504)
void Menu_Files_ClearElement(int id)
{
    memset(&Menu_Files_fileMenuElements[id], 0, sizeof Menu_Files_fileMenuElements[id]);
}

// decomp: MENU7.PRG _fileMenuElementsActive (src/MENU/MENU7.PRG/260.c:1512)
int Menu_Files_AreAnimationsComplete(void)
{
    int i;

    for (i = 0; i < 10 && Menu_Files_fileMenuElements[i].state < Menu_Files_ElementState_AnimateX; ++i)
        ;
    return i == 10;
}

// decomp: MENU7.PRG _printCharacter (src/MENU/MENU7.PRG/260.c:1524)
int Menu_Files_DrawGlyph(u_int c, int x, int y, int clut)
{
    static int Menu_Files_glyphWidths[] = { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 12, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 12, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 12, 12, 12, 12, 12,
        12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 8, 5, 7, 8, 7, 7, 7, 6, 6, 6, 7, 7, 7, 9,
        7, 7, 7, 7, 6, 7, 7, 6, 8, 7, 8, 8, 9, 8, 8, 4, 7, 5, 7, 7, 6, 8, 7, 7, 7, 6, 8,
        6, 8, 8, 7, 6, 7, 6, 8, 8, 7, 7, 8, 7, 5, 6, 7, 6, 7, 7, 7, 8, 12, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 12, 7, 7, 7, 7, 7, 8, 8, 8, 8,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 6, 12, 12, 12, 12,
        12, 12, 8, 12, 6, 5, 4, 12, 12, 12, 12, 4, 6, 6, 12, 12, 6, 6, 3, 3, 3, 4, 6, 12,
        6, 6, 6, 12, 5, 6, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        12, 12, 12 };

    if ((c >> 8) == 14) {
        return x + (char)c;
    }
    if (c != Core_TextCode_Space) {
        Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.sprt, Render_LegacyPrimitiveAddress_Null);
        Menu_Files_primBuf.prim.sprt.texturePageCommand =
            vs_getTpage(832, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off);
        Menu_Files_primBuf.prim.sprt.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_SpriteSemiTrans, 128, 128, 128);
        Menu_Files_primBuf.prim.sprt.position = (u_short)x | y << 16;
        Menu_Files_primBuf.prim.sprt.size = vs_getWH(12, 12);
        Menu_Files_primBuf.prim.sprt.uvPalette =
            vs_getUV0Clut((c % 21) * 12, (c / 21) * 12, clut * 16 + 896, 222);
        DrawPrim(&Menu_Files_primBuf);
    }
    return x + Menu_Files_glyphWidths[c];
}

// decomp: MENU7.PRG _findCurrentSave (src/MENU/MENU7.PRG/260.c:1572)
int Menu_Files_FindCurrentDataSaveStep(int init)
{
    static char Menu_Files_findCurrentDataSaveStepState;
    static char Menu_Files_findCurrentDataSaveStepSubState;

    int port;
    int currentSave;
    int event;
    int realPort;

    if (init != 0) {
        Menu_Files_findCurrentDataSaveStepState = 0;
        Menu_Files_findCurrentDataSaveStepSubState = 0;
        return 0;
    }

    realPort = Menu_Files_findCurrentDataSaveStepState >> 1;
    port = realPort + 1;

    if ((Menu_Files_findCurrentDataSaveStepState & 1) == 0) {
        Menu_Framework_SetInformationMessage(
            (char*)(Menu_Files_textTable + Menu_Files_textTable[Text_Menu_MCMAN_BIN_Index_accessing0 - 1 + port]));
        Menu_Files_memoryCardMessage =
            (char*)(Menu_Files_textTable + Menu_Files_textTable[Text_Menu_MCMAN_BIN_Index_accessing0 - 1 + port]);
        Save_MemoryCard_AdvanceCardReadiness(realPort + 1);
        ++Menu_Files_findCurrentDataSaveStepState;
    } else {
        event = Save_MemoryCard_AdvanceCardReadiness(0) & 3;

        if (event != Save_CardReadinessResult_Pending) {
            if (event == Save_CardReadinessResult_Ready) {
                if (Save_MemoryCard_ScanSaveDirectory(realPort + 1) == 0) {
                    currentSave = Save_MemoryCard_FindMatchingSaveSlot();
                    if (currentSave != 0) {
                        return currentSave + (realPort * 16);
                    }
                }
            } else if (event == Save_CardReadinessResult_Timeout) {
                if (Menu_Files_findCurrentDataSaveStepState != 1) {
                    if (Menu_Files_findCurrentDataSaveStepSubState != 0) {
                        return Save_FindSaveResult_Timeout;
                    }
                } else {
                    Menu_Files_findCurrentDataSaveStepSubState = Menu_Files_findCurrentDataSaveStepState;
                }
            }
            ++Menu_Files_findCurrentDataSaveStepState;
        }
    }
    return Menu_Files_findCurrentDataSaveStepState != 4 ? Save_FindSaveResult_Pending : Save_FindSaveResult_NotFound;
}

// decomp: MENU7.PRG maskedHandler (src/MENU/MENU7.PRG/260.c:1640)
enum Save_MaskedHandlerResult Menu_Files_CheckCardStep(int portMask)
{
    static char Menu_Files_checkCardStepMask;
    static char Menu_Files_checkCardStepLayoutPadding[5] __attribute__((unused));

    int event;

    if (portMask != 0) {
        Save_MemoryCard_AdvanceCardReadiness(portMask & 3);
        Menu_Files_checkCardStepMask = (portMask >> 4);
        return Save_MaskedHandlerResult_Pending;
    }
    event = Save_MemoryCard_AdvanceCardReadiness(0);
    if (event != Save_CardReadinessResult_Pending) {
        switch (event & Menu_Files_checkCardStepMask) {
        case Save_CardReadinessResult_Ready:
            return Save_MaskedHandlerResult_Complete;
        case Save_CardReadinessResult_Timeout:
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_insertError);
            break;
        case Save_CardReadinessResult_Unformatted:
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_noData);
            break;
        default:
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_removed);
            break;
        }
        return Save_MaskedHandlerResult_Error;
    }
    return Save_MaskedHandlerResult_Pending;
}

// decomp: MENU7.PRG _printString (src/MENU/MENU7.PRG/260.c:1675)
void Menu_Files_DrawText(char* text, int x, int y, int clut)
{

    char tempArrowState;
    u_int c;
    int startX = x;
    int nextX = x;

    while (1) {
        c = *text++;
        if (c < Core_TextCode_Control) {
            if (c < Core_TextCode_NonPrinting) {
                nextX = Menu_Files_DrawGlyph(c, nextX, y, clut);
            } else if (c == Core_TextCode_Confirm) {
                tempArrowState = (Menu_Files_textArrowState + 1) % 12;
                c = Core_TextCode_Arrow - (tempArrowState >> 2);
                Menu_Files_textArrowState = tempArrowState;
                nextX = Menu_Files_DrawGlyph(c, nextX, y, clut);
            } else if (c != Core_TextCode_Terminator) {
                if (c == Core_TextCode_Newline) {
                    y += 13;
                    nextX = startX;
                }
            } else {
                break;
            }
        } else {
            u_int control = *text++;
            if (c == Core_TextCode_Spacing) {
                if (control >= 0xF0) {
                    nextX -= 256 - control;
                } else {
                    nextX += control;
                }
            }
        }
    }
}

// decomp: MENU7.PRG _intepolateMenuItemBgColour (src/MENU/MENU7.PRG/260.c:1738)
u_int Menu_Files_ElementBackgroundColor(u_int outerFactor, u_int innerFactor)
{

    u_int color1;
    u_int color2;

    if (innerFactor < 9) {
        color1 = Render_BlendRgb(
            vs_getRGB888(0, 65, 107), vs_getRGB888(25, 130, 108), innerFactor);
        color2 =
            Render_BlendRgb(vs_getRGB888(0, 5, 51), vs_getRGB888(1, 40, 38), innerFactor);
    } else {
        color1 = Menu_Files_elementBackgroundColorColors1[((innerFactor >> 3) - 2)];
        color2 = Menu_Files_elementBackgroundColorColors2[((innerFactor >> 3) - 2)];
    }
    return Render_BlendRgb(color1, color2, outerFactor);
}

// decomp: MENU7.PRG _drawFileMenuElement (src/MENU/MENU7.PRG/260.c:1765)
void Menu_Files_DrawElement(Menu_Files_FileMenuElement* element)
{
    u_long clut[130];
    Save_CardFileInfoRecord* saveInfo;
    int xy;
    int var_a3;
    int location;
    int x;
    int y;
    int var1;
    int var2;
    int uvClut;
    int slotState;
    void* locationClut;
    int new_var;

    if (element->selected != 0) {
        element->outertextBlendFactor = 8;
    } else if (element->outertextBlendFactor != 0) {
        --element->outertextBlendFactor;
    }
    if (element->slotUnavailable != 0) {
        element->outertextBlendFactor = 0;
    }
    if ((element->slotId >= 5) || ((element->y - 72) < 0x51U)) {
        y = element->innertextBlendFactor;
        var1 = Menu_Files_ElementBackgroundColor(8 - element->outertextBlendFactor, y);
        var2 = Menu_Files_ElementBackgroundColor(element->outertextBlendFactor, y);
        if (y & 7) {
            element->innertextBlendFactor = y + 1;
        }
        y = element->y << 16;

        Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.tilePoly, Render_LegacyPrimitiveAddress_Null);
        Menu_Files_primBuf.prim.tilePoly.tileTpage.texturePageCommand =
            vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_On);
        Menu_Files_primBuf.prim.tilePoly.tileTpage.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0);
        Menu_Files_primBuf.prim.tilePoly.tileTpage.position =
            ((element->x + 2) & 0xFFFF) | (element->y + 2) << 16;
        Menu_Files_primBuf.prim.tilePoly.tileTpage.size = element->w | (element->h << 0x10);
        Menu_Files_primBuf.prim.tilePoly.polyG4.colorZeroAndCode = vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4, var1);
        Menu_Files_primBuf.prim.tilePoly.polyG4.positionZero = (u_short)element->x | y;
        Menu_Files_primBuf.prim.tilePoly.polyG4.colorOne = var2;
        Menu_Files_primBuf.prim.tilePoly.polyG4.positionOne = ((element->x + element->w) & 0xFFFF) | y;
        Menu_Files_primBuf.prim.tilePoly.polyG4.colorTwo = var1;
        Menu_Files_primBuf.prim.tilePoly.polyG4.positionTwo =
            (u_short)element->x | ((element->y + element->h) << 0x10);
        Menu_Files_primBuf.prim.tilePoly.polyG4.colorThree = var2;
        Menu_Files_primBuf.prim.tilePoly.polyG4.positionThree =
            ((element->x + element->w) & 0xFFFF) | ((element->y + element->h) << 0x10);
        DrawPrim(&Menu_Files_primBuf);

        x = element->x + 6;
        for (var1 = 0; var1 < 32; ++var1) {
            var2 = element->text[var1];
            if (var2 == Core_TextCode_Spacing) {
                x += element->text[++var1];
            } else if (var2 != 0xFF) {
                x = Menu_Files_DrawGlyph(var2, x, element->y, 0);
            } else {
                break;
            }
        }
        var2 = element->slotId;
        if (var2 < 5) {
            Render_ReadPackedRectangle(vs_getXY(768, 227), clut, vs_getWH(256, 1));
            saveInfo = &Title_saveFileSummaries[var2];
            location = element->saveLocation - 1;
            var1 = saveInfo->summary.base.slotState;
            if (saveInfo->summary.base.slotState < 3) {
                if (saveInfo->summary.base.slotState == 0) {
                    location = -2;
                } else if (saveInfo->summary.base.slotState == 2) {
                    location = -4;
                } else if (Menu_Files_isSaving != 0) {
                    location = -1;
                } else {
                    location = -5;
                }
            } else if (location >= 48) {
                location = -3;
            }
            if (location >= 32u) {
                locationClut = Save_MemoryCard_uiResources->locationPalettes[1];
            } else {
                locationClut = Save_MemoryCard_uiResources->locationPalettes[0];
            }
            Render_UploadPackedRectangle(vs_getXY(768, 227), locationClut, vs_getWH(256, 1));
            if (location < 0) {
                uvClut = (~location << 13) | vs_getUV0Clut(64, 0, 768, 227);
                xy = (element->x - 64) | y;
                Render_DrawImmediateSprite(xy, uvClut, vs_getWH(64, 32),
                    ((8 - element->outertextBlendFactor) << 19) | 0x9C);
            } else {
                int v0;
                uvClut = ((location & 8) * 8) | ((location & 7) << 0xD)
                       | vs_getUV0Clut(0, 0, 768, 227);
                xy = (element->x - 64) | y;
                new_var = (((location & 0x30) * 4) + 832) & 0x3FF;
                v0 = new_var >> 6;
                var_a3 = (((8 - element->outertextBlendFactor) << 19) | 0x90);
                Render_DrawImmediateSprite(xy, uvClut, vs_getWH(64, 32), v0 | var_a3);
            }
            Render_UploadPackedRectangle(vs_getXY(768, 227), clut, vs_getWH(256, 1));
            Menu_Files_DrawSummaryLabel((element->x - 22) | y, Menu_Files_FileMenuUiId_Number);
            Menu_Files_DrawInteger((element->x - 9) | y, var2 + 1, 0xAU);
            slotState = saveInfo->summary.base.slotState;
            if (slotState == Save_CardFileSlotStatus_Unavailable) {
                Menu_Files_DrawText((char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_inUse),
                    element->x + 6, element->y + 10, 3);
            } else if (slotState == Save_CardFileSlotStatus_Available) {
                if (Menu_Files_isSaving == 0) {
                    Menu_Files_DrawText((char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_empty),
                        element->x + 6, element->y + 10, 3);
                } else {
                    Menu_Files_DrawText((char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_new),
                        element->x + 6, element->y + 10, 0);
                }
            } else {
                y += 4 << 16;
                Menu_Files_DrawText(
                    (char*)&Menu_Files_textTable[Menu_Files_textTable[element->saveLocation
                                                  + Text_Menu_MCMAN_BIN_Index_saveLocations]],
                    element->x + 6, element->y + 4, 0);
                Menu_Files_DrawSummaryLabel(y | 172, Menu_Files_FileMenuUiId_Map);
                Menu_Files_DrawSummaryLabel(y | 189, Menu_Files_FileMenuUiId_Colon);
                var1 = saveInfo->summary.stats.mapCompletion;

                if (var1 == 100) {
                    Menu_Files_DrawInteger(y | 192, 100, 100);
                    Menu_Files_DrawSummaryLabel((y + 0xFFFF0000) | 207, Menu_Files_FileMenuUiId_Percent);
                } else {
                    Menu_Files_DrawInteger(y | 192, var1, 10);
                    Menu_Files_DrawSummaryLabel((y + 0xFFFF0000) | 202, Menu_Files_FileMenuUiId_Percent);
                }
                Menu_Files_DrawSummaryLabel(y | 217, Menu_Files_FileMenuUiId_Save);
                Menu_Files_DrawSummaryLabel(y | 239, Menu_Files_FileMenuUiId_Colon);
                Menu_Files_DrawInteger(y | 242, saveInfo->summary.stats.cumulativeSaveCount, 1000);
                Menu_Files_DrawSummaryLabel(y | 267, Menu_Files_FileMenuUiId_Clear);
                Menu_Files_DrawSummaryLabel(y | 293, Menu_Files_FileMenuUiId_Colon);
                Menu_Files_DrawInteger(y | 296, saveInfo->summary.stats.clearCount, 10);
                y += 13 << 16;
                if (saveInfo->summary.stats.clearCount != 0) {
                    Render_DrawImmediateSprite(y | 69, vs_getUV0Clut(240, 16, 912, 223), vs_getWH(16, 16),
                        getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, 768, 0));
                }
                Menu_Files_DrawSummaryLabel(y | 240, Menu_Files_FileMenuUiId_Time);
                var1 = saveInfo->summary.stats.gameTime.t.hours;
                if (var1 == 100) {
                    Menu_Files_DrawInteger(y | 263, var1, 100);
                } else {
                    Menu_Files_DrawInteger(y | 268, var1, 10);
                }
                Menu_Files_DrawSummaryLabel(y | 279, Menu_Files_FileMenuUiId_Colon);
                Menu_Files_DrawInteger(y | 282, saveInfo->summary.stats.gameTime.t.minutes, 10);
                Menu_Files_DrawSummaryLabel(y | 293, Menu_Files_FileMenuUiId_Colon);
                Menu_Files_DrawInteger(y | 296, saveInfo->summary.stats.gameTime.t.seconds, 10);
                Menu_Files_DrawHpMpSummary(y | 88, Menu_Files_StatType_HP, saveInfo->summary.stats.currentHP,
                    saveInfo->summary.stats.maxHP);
                Menu_Files_DrawHpMpSummary(y | 158, Menu_Files_StatType_MP, saveInfo->summary.stats.currentMP,
                    saveInfo->summary.stats.maxMP);
                y -= 17 << 16;
            }
            if ((element->selected != 0) && (Menu_Files_fileProgressCounter != 0)) {
                if (Menu_Files_fileProgressCounter < 0) {
                    signed char* p = (Menu_Files_cursorFileOpSaturation + Menu_Files_fileProgressCounter++);
                    Menu_Files_DrawTransferComplete(-p[17], y);
                } else {
                    int new_var3 = 0x140;
                    Menu_Files_DrawTransferProgress(
                        Title_transferProgressPosition
                            + (((Menu_Files_fileProgressCounter - Title_previousTransferProgress)
                                   * ((Title_saveTransferErrorOffset * 0x14)
                                       - (Title_transferProgressPosition - new_var3)))
                                / Title_transferProgressTarget),
                        y);
                }
            }
        }
    }
}

// decomp: MENU7.PRG _drawFileMenu (src/MENU/MENU7.PRG/260.c:1951)
void Menu_Files_DrawFrame(int framebuf)
{
    int _[2];
    Menu_Files_FileMenuElement* element;
    int j;
    int i;
    u_int x;
    int state;
    int new_var;

    element = Menu_Files_fileMenuElements;
    x = 0;
    if (framebuf != 0) {
        x = 320;
    }
    Render_DrawImmediateSprite(vs_getXY(256, 176), vs_getUV0Clut(0, 176, 768, 227), vs_getWH(64, 64),
        getTPage(Render_TextureFormat_Palette8Bit, Render_BlendMode_Average, 768, 256));
    Render_DrawImmediateSprite(vs_getXY(0, 176), vs_getUV0Clut(0, 176, 768, 227), vs_getWH(256, 64),
        getTPage(Render_TextureFormat_Palette8Bit, Render_BlendMode_Average, 640, 256));

    for (i = 0; i < 10; ++i, ++element) {
        state = element->state;
        if (state == Menu_Files_ElementState_AnimateX) {
            if (element->x < element->targetPosition) {
                element->x += 32;
                if (element->x >= element->targetPosition) {
                    element->x = element->targetPosition;
                    element->state = Menu_Files_ElementState_Static;
                }
            } else {

                for (j = 1; j < 16; ++j) {
                    if ((element->targetPosition + Menu_Files_menuElementStops[j]) >= element->x) {
                        break;
                    }
                }
                element->x = element->targetPosition + Menu_Files_menuElementStops[j - 1];
                if (element->w < (320 - element->x)) {
                    element->w = 320 - element->x;
                }
                if (element->x == element->targetPosition) {
                    element->state = Menu_Files_ElementState_Static;
                }
            }
        }
        if (state == Menu_Files_ElementState_AnimateY) {
            if (element->y > element->targetPosition) {
                for (j = 1; j < 16; ++j) {
                    if ((element->targetPosition + Menu_Files_menuElementStops[j]) >= element->y) {
                        break;
                    }
                }
                element->y = (element->targetPosition + Menu_Files_menuElementStops[j - 1]);
            }
            if (element->y == element->targetPosition) {
                element->state = Menu_Files_ElementState_Static;
            }
        }
        if (state == Menu_Files_ElementState_AnimateNegX) {
            if (element->x < element->targetPosition) {
                for (j = 1; j < 16; ++j) {
                    if (element->x >= -Menu_Files_menuElementStops[j]) {
                        break;
                    }
                }
                element->x = -Menu_Files_menuElementStops[j - 1];
                if (element->x == 0) {
                    element->state = Menu_Files_ElementState_Static;
                }
            } else {
                element->x = element->x - 32;
                if (element->targetPosition >= element->x) {
                    element->x = element->targetPosition;
                    element->state = Menu_Files_ElementState_Static;
                }
            }
        }
        if (state != Menu_Files_ElementState_Inactive) {
            Menu_Files_DrawElement(element);
        }
    }

    for (i = 1; i < 4; ++i) {
        new_var = 256;
        Render_DrawImmediateSprite(vs_getXY(0, 187 - i), (187 - i) << 8, vs_getWH(256, 1),
            (x >> 6) | (((4 - i) << 21) | new_var));
        Render_DrawImmediateSprite(vs_getXY(256, 187 - i), (187 - i) << 8, vs_getWH(64, 1),
            (((int)(x + new_var)) >> 6) | (((4 - i) << 21) | new_var));
    }
    Render_DrawImmediateSprite(vs_getXY(10, 181), vs_getUV0Clut(64, 145, 848, 223), vs_getWH(33, 7),
        getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, 768, 0)); // "Caution"
    DrawSync(0);
    Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.tile, Render_LegacyPrimitiveAddress_Null);
    Menu_Files_primBuf.prim.tile.colorAndCode = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0);
    Menu_Files_primBuf.prim.tile.position = vs_getXY(0, 187);
    Menu_Files_primBuf.prim.tile.size = vs_getWH(320, 52);
    DrawPrim(&Menu_Files_primBuf); // Message dialog background
    if (Menu_Files_selectCursorXy != 0) {
        Render_DrawImmediateSprite(Menu_Files_selectCursorXy, vs_getUV0Clut(32, 48, 896, 223), vs_getWH(16, 16),
            (Menu_Files_cursorFileOpSaturation[Menu_Files_selectCursorColor] << 16)
                | getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, 768, 0));
        Menu_Files_selectCursorColor = (Menu_Files_selectCursorColor + 1) & 0xF;
        if (Input_buttonsPressed.allButtons & PADRup) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_INVALID);
        }
    } else {
        Menu_Files_selectCursorColor = 0;
    }
    if (Menu_Files_memoryCardMessage != 0) {
        Menu_Files_DrawText(Menu_Files_memoryCardMessage, 16, 192, 0);
    }
    Save_NativeDrawFileOverlay(0);
    if (Menu_Files_fileMenuScreenFade != 0) {
        Menu_Files_primBuf.tag = vs_getTag(Menu_Files_primBuf.prim.tileTpage, Render_LegacyPrimitiveAddress_Null);
        Menu_Files_primBuf.prim.tileTpage.texturePageCommand =
            vs_getTpage(0, 0, Render_TextureFormat_Direct16Bit, Render_BlendMode_Subtract, Render_Dithering_Off);
        Menu_Files_primBuf.prim.tileTpage.position = vs_getXY(0, 0);
        Menu_Files_primBuf.prim.tileTpage.size = vs_getWH(320, 224);
        Menu_Files_primBuf.prim.tileTpage.colorCommand =
            vs_getRGB0Raw(Render_LegacyPrimitiveCode_TileSemiTrans, Menu_Files_fileMenuScreenFade * vs_getRGB888(8, 8, 8));
        DrawPrim(&Menu_Files_primBuf);
    }
}

// decomp: MENU7.PRG _drawFileMenuBg (src/MENU/MENU7.PRG/260.c:2078)
void Menu_Files_DrawBackground(void)
{
    Render_DrawImmediateSprite(vs_getXY(256, 0), vs_getUV0Clut(0, 0, 768, 227), vs_getWH(64, 176),
        getTPage(Render_TextureFormat_Palette8Bit, Render_BlendMode_Average, 768, 256));
    Render_DrawImmediateSprite(vs_getXY(0, 0), vs_getUV0Clut(0, 0, 768, 227), vs_getWH(256, 176),
        getTPage(Render_TextureFormat_Palette8Bit, Render_BlendMode_Average, 640, 256));
}

// decomp: MENU7.PRG _promptConfirm (src/MENU/MENU7.PRG/260.c:2092)
int Menu_Files_UpdateConfirmation(int arg0)
{
    enum Menu_Files_ConfirmationState {
        Menu_Files_ConfirmationState_InitYes = 0,
        Menu_Files_ConfirmationState_InitNo = 1,
        Menu_Files_ConfirmationState_WaitForAnimation = 2,
        Menu_Files_ConfirmationState_HandleInput = 3,
    };

    static char Menu_Files_confirmationState;
    static char Menu_Files_confirmationCancelled;
    static char Menu_Files_confirmationLayoutPadding[2] __attribute__((unused));

    Menu_Files_FileMenuElement* temp_v0;
    int i;

    if (arg0 != 0) {
        Menu_Files_confirmationCancelled = 1;
        Menu_Files_confirmationState = Menu_Files_ConfirmationState_InitYes;
        return 0;
    }

    switch (Menu_Files_confirmationState) {
    case Menu_Files_ConfirmationState_InitYes:
    case Menu_Files_ConfirmationState_InitNo:
        temp_v0 = Menu_Files_InitializeElement(Menu_Files_confirmationState + 3,
            vs_getXY(-126 & 0xFFFF, Menu_Files_confirmationState * 16 + 18), vs_getWH(126, 12),
            (char*)&Menu_Files_textTable[Menu_Files_textTable[Menu_Files_confirmationState + Text_Menu_MCMAN_BIN_Index_yesIndent]]);
        temp_v0->state = 4;
        temp_v0->targetPosition = 0;
        ++Menu_Files_confirmationState;
        break;
    case Menu_Files_ConfirmationState_WaitForAnimation:
        Menu_Files_confirmationState += Menu_Files_AreAnimationsComplete();
        break;
    case Menu_Files_ConfirmationState_HandleInput:
        Menu_Files_fileMenuElements[Menu_Files_confirmationCancelled + 3].selected = 1;
        Menu_Files_fileMenuElements[4 - Menu_Files_confirmationCancelled].selected = 0;
        if (Input_buttonsPressed.allButtons & (PADRdown | PADRright)) {
            Menu_Files_selectCursorXy = 0;
            for (i = 3; i < 5; ++i) {
                Menu_Files_fileMenuElements[i].state = 4;
                Menu_Files_fileMenuElements[i].targetPosition = -126;
            }
            if (Input_buttonsPressed.allButtons & PADRright) {
                if (Menu_Files_confirmationCancelled == 0) {
                    Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
                    return 1;
                }
            }
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            return -1;
        }
        if (Input_buttonRepeat & (PADLup | PADLdown)) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            Menu_Files_confirmationCancelled = 1 - Menu_Files_confirmationCancelled;
        }
        Menu_Files_selectCursorXy = (Menu_Files_confirmationCancelled * 16 + 10) << 16;
        break;
    }
    return 0;
}

// decomp: MENU7.PRG _promptOverwrite (src/MENU/MENU7.PRG/260.c:2161)
int Menu_Files_UpdateOverwritePrompt(int arg0)
{
    static int Menu_Files_overwritePromptConfirmed;
    static char Menu_Files_overwritePromptState;
    static char Menu_Files_overwritePromptInitialized;

    int temp_v0;

    if (arg0 != 0) {
        Menu_Files_overwritePromptInitialized = 1;
        Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_overwritePrompt);
        Menu_Files_UpdateConfirmation(1);
        Menu_Files_overwritePromptState = 0;
        return 0;
    }
    switch (Menu_Files_overwritePromptState) {
    case 0:
        temp_v0 = Menu_Files_UpdateConfirmation(0);
        Menu_Files_overwritePromptConfirmed = temp_v0;
        if (temp_v0 != 0) {
            Menu_Files_overwritePromptState = 1;
        }
        break;
    case 1:
        if (Menu_Files_AreAnimationsComplete() != 0) {
            return Menu_Files_overwritePromptConfirmed;
        }
        break;
    }
    return 0;
}

// decomp: MENU7.PRG _promptFormat (src/MENU/MENU7.PRG/260.c:2199)
int Menu_Files_UpdateFormatPrompt(int initPort)
{
    enum Menu_Files_FormatPromptState { Menu_Files_FormatPromptState_PromptConfirm = 0, Menu_Files_FormatPromptState_InitEvents = 1, Menu_Files_FormatPromptState_HandleEvents = 2, Menu_Files_FormatPromptState_Format = 3 };

    static char Menu_Files_formatPromptState;
    static char Menu_Files_formatPromptPort;
    static char Menu_Files_formatPromptLayoutPadding[8] __attribute__((unused));

    int val;

    if (initPort != 0) {
        Menu_Files_formatPromptPort = initPort;
        Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_formatPrompt);
        Menu_Files_UpdateConfirmation(1);
        Menu_Files_formatPromptState = Menu_Files_FormatPromptState_PromptConfirm;
        return 0;
    }

    switch (Menu_Files_formatPromptState) {
    case Menu_Files_FormatPromptState_PromptConfirm:
        val = Menu_Files_UpdateConfirmation(0);

        if (val != 0) {
            if (val < 0) {
                Menu_Files_memoryCardMessage =
                    (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_formatCancelled);
                return -1;
            }

            Menu_Files_formatPromptState = Menu_Files_FormatPromptState_InitEvents;
        }
        return 0;
    case Menu_Files_FormatPromptState_InitEvents:
        if (Menu_Files_AreAnimationsComplete() != 0) {
            Save_MemoryCard_AdvanceCardReadiness(Menu_Files_formatPromptPort);
            Menu_Files_formatPromptState = Menu_Files_FormatPromptState_HandleEvents;
        }
        return 0;
    case Menu_Files_FormatPromptState_HandleEvents:
        val = Save_MemoryCard_AdvanceCardReadiness(0);

        if (val != 0) {
            if (val == Save_CardReadinessResult_Unformatted) {
                Menu_Files_formatPromptState = Menu_Files_FormatPromptState_Format;
                Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_formatting);
            } else {
                if (val == Save_CardReadinessResult_Timeout) {
                    Menu_Files_memoryCardMessage =
                        (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_insertError);
                } else {
                    Menu_Files_memoryCardMessage =
                        (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_removed);
                }
                return -1;
            }
        }
        return 0;
    case Menu_Files_FormatPromptState_Format:
        if (_card_format((Menu_Files_formatPromptPort - 1) * 16) == 0) {
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_formatFailed);
            return -1;
        }
        return 1;
    default:
        return 0;
    }
}

// decomp: MENU7.PRG _showSaveFilesMenu (src/MENU/MENU7.PRG/260.c:2277)
int Menu_Files_UpdateSaveFiles(int initPort)
{
    enum Menu_Files_SaveFilesState {
        Menu_Files_SaveFilesState_Init = 0,
        Menu_Files_SaveFilesState_HandleInput = 1,
        Menu_Files_SaveFilesState_SlotSelected = 2,
        Menu_Files_SaveFilesState_OverwritePrompt = 3,
        Menu_Files_SaveFilesState_InitOverwrite = 4,
        Menu_Files_SaveFilesState_Save = 5,
        Menu_Files_SaveFilesState_Validate = 6,
        Menu_Files_SaveFilesState_LeaveWithTimer = 7,
        Menu_Files_SaveFilesState_Leave = 8,
        Menu_Files_SaveFilesState_Format = 9,
        Menu_Files_SaveFilesState_LeaveError = 10
    };

    static State_Settings Menu_Files_saveFilesSettingsBackup;
    static int Menu_Files_saveFilesSaveSuccessful;
    static char Menu_Files_saveFilesState;
    static char Menu_Files_saveFilesPort;
    static char Menu_Files_saveFilesPage;
    static char Menu_Files_saveFilesSlot;
    static char Menu_Files_saveFilesLeaveTimer;

    Menu_Files_FileMenuElement* element;
    int val;
    int saveId;
    int i;

    if (initPort != 0) {
        Menu_Files_saveFilesPort = initPort;
        Menu_Files_saveFilesSlot = Save_MemoryCard_FindMatchingSaveSlot();
        if (Menu_Files_saveFilesSlot) {
            --Menu_Files_saveFilesSlot;
        }
        Menu_Files_saveFilesPage = 0;
        if (Menu_Files_saveFilesSlot == 4) {
            Menu_Files_saveFilesPage = 2;
            Menu_Files_saveFilesSlot = 2;
        } else if (Menu_Files_saveFilesSlot != 0) {
            Menu_Files_saveFilesPage = Menu_Files_saveFilesSlot - 1;
            Menu_Files_saveFilesSlot = 1;
        }
        Menu_Files_saveFilesSaveSuccessful = -1;
        Menu_Files_saveFilesLeaveTimer = 0;
        Menu_Files_saveFilesState = Menu_Files_SaveFilesState_Init;
        return 0;
    }
    switch (Menu_Files_saveFilesState) {
    case Menu_Files_SaveFilesState_Init:
        for (i = 0; i < 5; ++i) {
            element = Menu_Files_InitializeElement(
                i + 5, vs_getXY(64, i * 40 + 72), vs_getWH(256, 32), 0);
            element->slotId = i;
            element->slotUnavailable =
                Title_saveFileSummaries[i].summary.base.slotState == Save_CardFileSlotStatus_Unavailable;
            element->saveLocation = Title_saveFileSummaries[i].summary.stats.saveLocation;
        }
        Menu_Files_saveFilesState = Menu_Files_SaveFilesState_HandleInput;
        /* fallthrough */
    case Menu_Files_SaveFilesState_HandleInput:
        Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_selectFile);
        if (Input_buttonsPressed.allButtons & PADRdown) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            Menu_Files_selectCursorXy = 0;
            Menu_Files_saveFilesState = Menu_Files_SaveFilesState_Leave;
        } else {
            for (i = 0; i < 5; ++i) {
                Menu_Files_fileMenuElements[i + 5].y = ((i - Menu_Files_saveFilesPage) * 40) + 72;
                Menu_Files_fileMenuElements[i + 5].selected = 0;
            }
            val = Menu_Files_saveFilesSlot + Menu_Files_saveFilesPage;
            if (Input_buttonsPressed.allButtons & PADRright) {
                if (Title_saveFileSummaries[val].summary.base.slotState != Save_CardFileSlotStatus_Unavailable) {
                    Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
                    Menu_Files_fileMenuElements[val + 5].selected = 1;
                    Menu_Files_selectCursorXy = 0;
                    Menu_Files_CheckCardStep(Menu_Files_saveFilesPort + Save_EventMask_All);
                    Menu_Files_saveFilesState = Menu_Files_SaveFilesState_SlotSelected;
                    break;
                } else {
                    Menu_PlayInvalidSound();
                }
            }
            if (Input_buttonRepeat & PADLup) {
                if (Menu_Files_saveFilesSlot == 0) {
                    if (Menu_Files_saveFilesPage != 0) {
                        --Menu_Files_saveFilesPage;
                    }
                } else {
                    --Menu_Files_saveFilesSlot;
                }
            }
            if (Input_buttonRepeat & PADLdown) {
                if (Menu_Files_saveFilesSlot == 2) {
                    if (Menu_Files_saveFilesPage < 2) {
                        ++Menu_Files_saveFilesPage;
                    }
                } else {
                    ++Menu_Files_saveFilesSlot;
                }
            }
            if (val != (Menu_Files_saveFilesSlot + Menu_Files_saveFilesPage)) {
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            }
            Menu_Files_selectCursorXy = vs_getXY(24, Menu_Files_saveFilesSlot * 40 + 62);
            Menu_Files_fileMenuElements[val + 5].selected = 1;
        }
        break;
    case Menu_Files_SaveFilesState_SlotSelected:
        val = Menu_Files_CheckCardStep(0);
        if (val != Save_MaskedHandlerResult_Pending) {
            if (Menu_Files_memoryCardMessage == (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_noData)) {
                Menu_Files_UpdateFormatPrompt(Menu_Files_saveFilesPort);
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_Format;
            } else if (val > Save_MaskedHandlerResult_Error) {
                val = Menu_Files_saveFilesSlot + Menu_Files_saveFilesPage;
                if (Title_saveFileSummaries[val].summary.base.slotState == Save_CardFileSlotStatus_Available) {
                    if (Save_MemoryCard_CreateSaveFile(Menu_Files_saveFilesPort, val + 1) != 0) {
                        Menu_Files_saveFilesState = Menu_Files_SaveFilesState_LeaveWithTimer;
                        Menu_Files_memoryCardMessage =
                            (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_saveFailed);
                    } else {
                        Menu_Files_saveFilesState = Menu_Files_SaveFilesState_Save;
                    }
                } else {
                    Menu_Files_UpdateOverwritePrompt(1);
                    Menu_Files_saveFilesState = Menu_Files_SaveFilesState_OverwritePrompt;
                }
            } else {
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_LeaveWithTimer;
            }
        }
        break;
    case Menu_Files_SaveFilesState_OverwritePrompt:
        val = Menu_Files_UpdateOverwritePrompt(0);
        if (val != 0) {
            if (val == 1) {
                Menu_Files_CheckCardStep(Menu_Files_saveFilesPort + Save_EventMask_All);
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_InitOverwrite;
            } else {
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_HandleInput;
            }
        }
        break;
    case Menu_Files_SaveFilesState_InitOverwrite:
        val = Menu_Files_CheckCardStep(0);
        if (val != Save_MaskedHandlerResult_Pending) {
            if (val > Save_MaskedHandlerResult_Error) {
                Menu_Files_dataNotSaved |= (Save_MemoryCard_FindMatchingSaveSlot() == (Menu_Files_saveFilesSlot + Menu_Files_saveFilesPage + 1));
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_Save;
            } else {
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_LeaveWithTimer;
            }
        }
        break;
    case Menu_Files_SaveFilesState_Save:
        val = Menu_Files_saveFilesSlot + Menu_Files_saveFilesPage;
        Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_saving);
        Core_Memory_CopyBytesBackward(&Menu_Files_saveFilesSettingsBackup, &Input_settings, sizeof Menu_Files_saveFilesSettingsBackup);
        Core_Memory_CopyBytesBackward(
            (Save_GameSaveImage*)Title_memoryCardUiResource + 2, (Save_GameSaveImage*)Title_memoryCardUiResource + 1, sizeof(Save_GameSaveImage));
        if (Menu_Files_containerDataEmpty != 0) {
            Menu_Files_backupMainSetting = (*(u_int*)&Input_settings >> 4) & 1;
            *(int*)&Input_settings |= 0x10;
        }
        Save_MemoryCard_PackageGameSave(val);
        Save_MemoryCard_WriteSaveStep((val + 1) | ((Menu_Files_saveFilesPort - 1) << 0x10));
        Menu_Files_saveFilesState = Menu_Files_SaveFilesState_Validate;
        break;
    case Menu_Files_SaveFilesState_Validate:
        val = Save_MemoryCard_WriteSaveStep(0);
        ++Menu_Files_fileProgressCounter;
        if (val != 0) {
            Audio_Seq_unusedStateFlags.doorEntered = 0;
            saveId = Menu_Files_saveFilesSlot + Menu_Files_saveFilesPage;
            if (val < 0) {
                if (Menu_Files_containerDataEmpty != 0) {
                    char v = Menu_Files_backupMainSetting & 1;
                    *(int*)&Input_settings =
                        (*(int*)&Input_settings & ~0x10) | (v * 0x10);
                }
                memset(&Title_saveFileSummaries[saveId], 0, sizeof Title_saveFileSummaries[saveId]);
                Title_saveFileSummaries[saveId].summary.base.slotState = Save_CardFileSlotStatus_Temporary;
                Menu_Files_fileMenuElements[saveId + 5].saveLocation = 0;
                Core_Memory_CopyBytesBackward((Save_GameSaveImage*)Title_memoryCardUiResource + 1, (Save_GameSaveImage*)Title_memoryCardUiResource + 2,
                    sizeof(Save_GameSaveImage));
                Menu_Files_fileProgressCounter = 0;
                Core_Memory_CopyBytesBackward(&Input_settings, &Menu_Files_saveFilesSettingsBackup, sizeof Input_settings);
                Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_saveFailed);
            } else {
                Menu_Files_dataNotSaved = 0;
                Menu_Files_fileProgressCounter = -16;
                Core_Memory_CopyBytesBackward(&Title_saveFileSummaries[saveId],
                    Title_memoryCardUiResource + sizeof Title_saveFileSummaries[saveId] * 3,
                    sizeof Title_saveFileSummaries[saveId]);
                Save_MemoryCard_DecodeSaveBytes(Title_saveFileSummaries[saveId].key,
                    &Title_saveFileSummaries[saveId].summary.base.slotState,
                    sizeof Title_saveFileSummaries[saveId] - sizeof(int));
                Menu_Files_fileMenuElements[saveId + 5].saveLocation =
                    Title_saveFileSummaries[saveId].summary.stats.saveLocation;
                Audio_PlaySfxCentered(0x7E, VS_SFX_FILEOPCOMPLETE);
                Menu_Files_saveFilesSaveSuccessful = 1;
                Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_saved);
            }
            Menu_Files_saveFilesState = Menu_Files_SaveFilesState_LeaveWithTimer;
        }
        break;
    case Menu_Files_SaveFilesState_LeaveWithTimer:
        if (Menu_Files_saveFilesSaveSuccessful == 1) {
            ++Menu_Files_saveFilesLeaveTimer;
        }
        if (Input_buttonsPressed.ports[0].lowByte == 0) {
            if (Menu_Files_saveFilesLeaveTimer != 150) {
                break;
            }
        }
        Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
        Menu_Files_saveFilesState = Menu_Files_SaveFilesState_Leave;
        break;
    case Menu_Files_SaveFilesState_Leave:
        if (Menu_Files_fileProgressCounter == 0) {
            for (i = 5; i < 10; ++i) {
                Menu_Files_ClearElement(i);
            }
            return Menu_Files_saveFilesSaveSuccessful;
        }
        break;
    case Menu_Files_SaveFilesState_Format:
        val = Menu_Files_UpdateFormatPrompt(0);
        if (val != 0) {
            if (val < 0) {
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_LeaveError;
            } else if (Save_MemoryCard_CreateSaveFile(Menu_Files_saveFilesPort, Menu_Files_saveFilesPage + Menu_Files_saveFilesSlot + 1) != 0) {
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_LeaveWithTimer;
                Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_saveFailed);
            } else {
                Menu_Files_saveFilesState = Menu_Files_SaveFilesState_Save;
            }
        }
        break;
    case Menu_Files_SaveFilesState_LeaveError:
        if (Input_buttonsPressed.ports[0].lowByte != 0) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            for (i = 5; i < 10; ++i) {
                Menu_Files_ClearElement(i);
            }
            return -1;
        }
        break;
    }
    return 0;
}

// decomp: MENU7.PRG _selectSaveMemoryCard (src/MENU/MENU7.PRG/260.c:2537)
int Menu_Files_UpdateSaveCard(int initPort)
{
    enum Menu_Files_SaveCardState {
        Menu_Files_SaveCardState_Init = 0,
        Menu_Files_SaveCardState_WaitForUnselectedAnimation = 1,
        Menu_Files_SaveCardState_WaitForAnimation = 2,
        Menu_Files_SaveCardState_HandleEvents = 3,
        Menu_Files_SaveCardState_AnimateLeave = 4,
        Menu_Files_SaveCardState_Leave = 5,
        Menu_Files_SaveCardState_InitSaveMenu = 6,
        Menu_Files_SaveCardState_ShowSaveMenu = 7
    };

    static char Menu_Files_saveCardState;
    static u_char Menu_Files_saveCardPort;

    int i, j;

    if (initPort != 0) {
        Menu_Files_saveCardPort = initPort;
        Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_checking);
        Menu_Files_saveCardState = Menu_Files_SaveCardState_Init;
        return 0;
    }

    switch (Menu_Files_saveCardState) {
    case Menu_Files_SaveCardState_Init:
        Menu_Files_fileMenuElements[Menu_Files_saveCardPort].state = Menu_Files_ElementState_AnimateX;
        Menu_Files_fileMenuElements[Menu_Files_saveCardPort].targetPosition = 180;
        Menu_Files_fileMenuElements[Menu_Files_saveCardPort].innertextBlendFactor = 1;
        Menu_Files_fileMenuElements[3 - Menu_Files_saveCardPort].state = Menu_Files_ElementState_AnimateX;
        Menu_Files_fileMenuElements[3 - Menu_Files_saveCardPort].targetPosition = 320;
        Menu_Files_saveCardState = Menu_Files_SaveCardState_WaitForUnselectedAnimation;
        break;
    case Menu_Files_SaveCardState_WaitForUnselectedAnimation:
        if (Menu_Files_AreAnimationsComplete() == 0) {
            break;
        }
        if (Menu_Files_saveCardPort == 2) {
            Menu_Files_fileMenuElements[2].state = Menu_Files_ElementState_AnimateY;
            Menu_Files_fileMenuElements[2].targetPosition = 50;
        }
        Menu_Files_saveCardState = Menu_Files_SaveCardState_WaitForAnimation;
        break;
    case Menu_Files_SaveCardState_WaitForAnimation:
        if (Menu_Files_AreAnimationsComplete() != 0) {
            Save_MemoryCard_AdvanceCardReadiness(Menu_Files_saveCardPort);
            Menu_Files_saveCardState = Menu_Files_SaveCardState_HandleEvents;
        }
        break;
    case Menu_Files_SaveCardState_HandleEvents:
        i = Save_MemoryCard_AdvanceCardReadiness(0) & 3;
        if (i == 0) {
            break;
        }
        switch (i) {
        case Save_CardReadinessResult_Ready:
            Menu_Files_saveCardState = Menu_Files_SaveCardState_InitSaveMenu;
            break;
        case Save_CardReadinessResult_Timeout:
            Menu_Files_saveCardState = Menu_Files_SaveCardState_AnimateLeave;
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_insertError);
            break;
        case Save_CardReadinessResult_Unformatted:
            memset(Title_saveFileSummaries, 0, sizeof *Title_saveFileSummaries * 5);
            for (i = 14; i >= 0; --i) {
                Title_cardDirectoryEntries[i] = 0;
            }
            j = Save_CardFileSlotStatus_Available;
            for (i = 4; i >= 0; --i) {
                Title_saveFileSummaries[i].summary.base.slotState = j;
            }
            Menu_Files_UpdateSaveFiles(Menu_Files_saveCardPort);
            Menu_Files_saveCardState = Menu_Files_SaveCardState_ShowSaveMenu;
            break;
        }
        break;
    case Menu_Files_SaveCardState_AnimateLeave:
        if (Input_buttonsPressed.ports[0].lowByte == 0) {
            break;
        }
        Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
        for (i = 1; i < 3; ++i) {
            Menu_Files_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
            Menu_Files_fileMenuElements[i].targetPosition = 320;
        }
        Menu_Files_saveCardState = Menu_Files_SaveCardState_Leave;
        break;
    case Menu_Files_SaveCardState_Leave:
        if (Menu_Files_AreAnimationsComplete() != 0) {
            return -1;
        }
        break;
    case Menu_Files_SaveCardState_InitSaveMenu:
        if (Save_MemoryCard_ScanSaveDirectory(Menu_Files_saveCardPort) != 0) {
            Menu_Files_saveCardState = Menu_Files_SaveCardState_AnimateLeave;
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_loadfailed);
            break;
        }
        for (i = 0; i < 5; ++i) {
            if (Title_saveFileSummaries[i].summary.base.slotState != Save_CardFileSlotStatus_Unavailable) {
                break;
            }
        }
        if (i == 5) {
            Menu_Files_saveCardState = Menu_Files_SaveCardState_AnimateLeave;
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_cardFull);
            break;
        }
        Menu_Files_UpdateSaveFiles(Menu_Files_saveCardPort);
        Menu_Files_saveCardState = Menu_Files_SaveCardState_ShowSaveMenu;
        break;
    case Menu_Files_SaveCardState_ShowSaveMenu:
        i = Menu_Files_UpdateSaveFiles(0);
        if (i == 0) {
            break;
        }
        if (i < 0) {
            for (i = 1; i < 3; ++i) {
                Menu_Files_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
                Menu_Files_fileMenuElements[i].targetPosition = 320;
            }
            Menu_Files_saveCardState = Menu_Files_SaveCardState_Leave;
            break;
        }
        return 1;
    }
    return 0;
}

// decomp: MENU7.PRG _showLoadFilesMenu (src/MENU/MENU7.PRG/260.c:2878)
int Menu_Files_UpdateLoadFiles(int initPort)
{
    enum Menu_Files_LoadFilesState {
        Menu_Files_LoadFilesState_Init = 0,
        Menu_Files_LoadFilesState_HandleInput = 1,
        Menu_Files_LoadFilesState_StartLoad = 2,
        Menu_Files_LoadFilesState_ApplyLoad = 3,
        Menu_Files_LoadFilesState_Loaded = 4,
        Menu_Files_LoadFilesState_Exiting = 5
    };

    static State_PlayTime Menu_Files_loadFilesBackupTime;
    static int Menu_Files_loadFilesFileLoaded;
    static char Menu_Files_loadFilesState;
    static char Menu_Files_loadFilesPort;
    static char Menu_Files_loadFilesPage;
    static char Menu_Files_loadFilesSlot;
    static char Menu_Files_loadFilesLeaveTimer;
    static char Menu_Files_loadFilesCompleteTimer;

    Menu_Files_FileMenuElement* element;
    int currentSlot;
    int i;

    if (initPort != 0) {
        Menu_Files_loadFilesPort = initPort;
        Menu_Files_loadFilesSlot = Save_MemoryCard_NewestSaveSlot();
        Menu_Files_loadFilesPage = 0;
        if (Menu_Files_loadFilesSlot == 4) {
            Menu_Files_loadFilesPage = 2;
            Menu_Files_loadFilesSlot = 2;
        } else if (Menu_Files_loadFilesSlot != 0) {
            Menu_Files_loadFilesPage = Menu_Files_loadFilesSlot - 1;
            Menu_Files_loadFilesSlot = 1;
        }
        Menu_Files_loadFilesFileLoaded = -1;
        Menu_Files_loadFilesCompleteTimer = 0;
        Menu_Files_loadFilesState = Menu_Files_LoadFilesState_Init;
        return 0;
    }
    switch (Menu_Files_loadFilesState) {
    case Menu_Files_LoadFilesState_Init:
        for (i = 0; i < 5; ++i) {
            element = Menu_Files_InitializeElement(
                i + 5, ((72 + i * 40) << 16) | 64, vs_getWH(256, 32), 0);
            element->slotId = i;
            element->slotUnavailable =
                Title_saveFileSummaries[i].summary.base.slotState < Save_CardFileSlotStatus_InUse;
            element->saveLocation = Title_saveFileSummaries[i].summary.stats.saveLocation;
        }
        Menu_Files_loadFilesState = Menu_Files_LoadFilesState_HandleInput;
        Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_selectFile);
        // fallthrough
    case Menu_Files_LoadFilesState_HandleInput:
        if (Input_buttonsPressed.allButtons & PADRdown) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            for (i = 5; i < 10; ++i) {
                Menu_Files_ClearElement(i);
            }
            Menu_Files_selectCursorXy = 0;
            return -1;
        }
        for (i = 0; i < 5; ++i) {
            Menu_Files_fileMenuElements[i + 5].y = (((i - Menu_Files_loadFilesPage) * 40) + 72);
            Menu_Files_fileMenuElements[i + 5].selected = 0;
        }

        currentSlot = Menu_Files_loadFilesSlot + Menu_Files_loadFilesPage;

        if (Input_buttonsPressed.allButtons & PADRright) {
            if (Title_saveFileSummaries[currentSlot].summary.base.slotState >= Save_CardFileSlotStatus_InUse) {
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
                Menu_Files_fileMenuElements[currentSlot + 5].selected = 1;
                Menu_Files_selectCursorXy = 0;
                Menu_Files_CheckCardStep(Menu_Files_loadFilesPort + Save_EventMask_All);
                Menu_Files_loadFilesState = Menu_Files_LoadFilesState_StartLoad;
                break;
            }
            Audio_PlaySfxCentered(0x7E, VS_SFX_INVALID);
        }
        if (Input_buttonRepeat & PADLup) {
            if (Menu_Files_loadFilesSlot == 0) {
                if (Menu_Files_loadFilesPage != 0) {
                    --Menu_Files_loadFilesPage;
                }
            } else {
                --Menu_Files_loadFilesSlot;
            }
        }
        if (Input_buttonRepeat & PADLdown) {
            if (Menu_Files_loadFilesSlot == 2) {
                if (Menu_Files_loadFilesPage < 2) {
                    ++Menu_Files_loadFilesPage;
                }
            } else {
                ++Menu_Files_loadFilesSlot;
            }
        }
        if (currentSlot != (Menu_Files_loadFilesSlot + Menu_Files_loadFilesPage)) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
        }
        Menu_Files_selectCursorXy = ((Menu_Files_loadFilesSlot * 40 + 62) << 16) | 24;
        Menu_Files_fileMenuElements[currentSlot + 5].selected = 1;
        break;
    case Menu_Files_LoadFilesState_StartLoad:
        currentSlot = Menu_Files_CheckCardStep(0);
        if (currentSlot != Save_MaskedHandlerResult_Pending) {
            if (currentSlot >= Save_MaskedHandlerResult_Pending) {
                int new_var;
                Save_MemoryCard_ReadSaveStep(((Menu_Files_loadFilesSlot + Menu_Files_loadFilesPage) + 1) | (new_var = ((Menu_Files_loadFilesPort - 1) << 16) | 256));
                Menu_Files_loadFilesState = Menu_Files_LoadFilesState_ApplyLoad;
                Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_loading);
            } else {
                Menu_Files_loadFilesState = Menu_Files_LoadFilesState_Loaded;
            }
        }
        break;
    case Menu_Files_LoadFilesState_ApplyLoad:
        currentSlot = Save_MemoryCard_ReadSaveStep(0);
        ++Menu_Files_fileProgressCounter;
        if (currentSlot != 0) {
            Menu_Files_fileProgressCounter = 0;
            do {
                Menu_Files_loadFilesLeaveTimer = 0;
                if (currentSlot < 0) {
                    Menu_Files_memoryCardMessage =
                        (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_loadfailed);
                    break;
                }
                switch (Save_MemoryCard_ValidateAndApplyLoadedSave(1)) {
                case 0:
                    Menu_Files_fileProgressCounter = -16;
                    Audio_PlaySfxCentered(0x7E, VS_SFX_FILEOPCOMPLETE);
                    Menu_Files_loadFilesLeaveTimer = 16;
                    Menu_Files_loadFilesFileLoaded = 1;
                    Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_loaded);
                    break;
                case 1:
                    Menu_Files_memoryCardMessage =
                        (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_fileCorruptDescription);
                    break;
                }
            } while (0);

            Menu_Files_loadFilesBackupTime.t = State_playTime.t;
            Menu_Files_loadFilesState = Menu_Files_LoadFilesState_Loaded;
        }
        break;
    case Menu_Files_LoadFilesState_Loaded:
        if (Menu_Files_loadFilesLeaveTimer != 0) {
            --Menu_Files_loadFilesLeaveTimer;
        }
        if (Menu_Files_loadFilesFileLoaded == 1) {
            ++Menu_Files_loadFilesCompleteTimer;
        }
        if ((Input_buttonsPressed.ports[0].lowByte != 0) || (Menu_Files_loadFilesCompleteTimer == 150)) {
            if (Menu_Files_loadFilesFileLoaded < 0) {
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            }
            Menu_Files_loadFilesState = Menu_Files_LoadFilesState_Exiting;
        }
        break;
    case Menu_Files_LoadFilesState_Exiting:
        if (Menu_Files_loadFilesLeaveTimer != 0) {
            --Menu_Files_loadFilesLeaveTimer;
            break;
        }
        if (Menu_Files_loadFilesFileLoaded == 1) {
            State_playTime.t = Menu_Files_loadFilesBackupTime.t;
        } else {
            for (i = 5; i < 10; ++i) {
                Menu_Files_ClearElement(i);
            }
        }
        return Menu_Files_loadFilesFileLoaded;
    }
    return 0;
}

// decomp: MENU7.PRG _selectLoadMemoryCard (src/MENU/MENU7.PRG/260.c:3063)
long Menu_Files_UpdateLoadCard(int initPort)
{
    enum Menu_Files_LoadCardState {
        Menu_Files_LoadCardState_Init = 0,
        Menu_Files_LoadCardState_WaitForUnselectedAnimation = 1,
        Menu_Files_LoadCardState_PollEventsInit = 2,
        Menu_Files_LoadCardState_PollEvents = 3,
        Menu_Files_LoadCardState_Leave = 4,
        Menu_Files_LoadCardState_ReturnNotSelected = 5,
        Menu_Files_LoadCardState_PollSuccess = 6,
        Menu_Files_LoadCardState_SelectFile = 7
    };

    static char Menu_Files_loadCardState;
    static u_char Menu_Files_loadCardPort;

    int i;

    if (initPort != 0) {
        Menu_Files_loadCardPort = initPort;
        Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_checking);
        Menu_Files_loadCardState = Menu_Files_LoadCardState_Init;
        return 0;
    }
    switch (Menu_Files_loadCardState) {
    case Menu_Files_LoadCardState_Init:
        Menu_Files_fileMenuElements[Menu_Files_loadCardPort].state = Menu_Files_ElementState_AnimateX;
        Menu_Files_fileMenuElements[Menu_Files_loadCardPort].targetPosition = 180;
        Menu_Files_fileMenuElements[Menu_Files_loadCardPort].innertextBlendFactor = 1;
        Menu_Files_fileMenuElements[3 - Menu_Files_loadCardPort].state = Menu_Files_ElementState_AnimateX;
        Menu_Files_fileMenuElements[3 - Menu_Files_loadCardPort].targetPosition = 320;
        Menu_Files_loadCardState = Menu_Files_LoadCardState_WaitForUnselectedAnimation;
        break;

    case Menu_Files_LoadCardState_WaitForUnselectedAnimation:
        if (Menu_Files_AreAnimationsComplete() == 0) {
            break;
        }
        if (Menu_Files_loadCardPort == 2) {
            Menu_Files_fileMenuElements[2].state = Menu_Files_ElementState_AnimateY;
            Menu_Files_fileMenuElements[2].targetPosition = 50;
        }
        Menu_Files_loadCardState = Menu_Files_LoadCardState_PollEventsInit;
        break;

    case Menu_Files_LoadCardState_PollEventsInit:
        if (Menu_Files_AreAnimationsComplete() == 0) {
            break;
        }
        Menu_Files_CheckCardStep(Menu_Files_loadCardPort + Save_EventMask_IgnoreNoEvent);
        Menu_Files_loadCardState = Menu_Files_LoadCardState_PollEvents;
        break;

    case Menu_Files_LoadCardState_PollEvents:
        i = Menu_Files_CheckCardStep(0);
        if (i != Save_MaskedHandlerResult_Pending) {
            Menu_Files_loadCardState = i + Menu_Files_LoadCardState_Leave + 1;
        }
        break;

    case Menu_Files_LoadCardState_Leave:
        if ((Input_buttonsPressed.ports[0].lowByte) == 0) {
            break;
        }
        Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
        for (i = 1; i < 3; ++i) {
            Menu_Files_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
            Menu_Files_fileMenuElements[i].targetPosition = 320;
        }
        Menu_Files_loadCardState = Menu_Files_LoadCardState_ReturnNotSelected;
        break;

    case Menu_Files_LoadCardState_ReturnNotSelected:
        if (Menu_Files_AreAnimationsComplete() != 0) {
            return -1;
        }
        break;

    case Menu_Files_LoadCardState_PollSuccess:
        if (Save_MemoryCard_ScanSaveDirectory(Menu_Files_loadCardPort) != 0) {
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_loadfailed);
            Menu_Files_loadCardState = Menu_Files_LoadCardState_Leave;
            break;
        }
        for (i = 0; i < 5; ++i) {
            if (Title_saveFileSummaries[i].summary.base.slotState > Save_CardFileSlotStatus_Available) {
                break;
            }
        }

        if (i == 5) {
            Menu_Files_memoryCardMessage = (char*)(Menu_Files_textTable + Text_Menu_MCMAN_BIN_Offset_noData);
            Menu_Files_loadCardState = Menu_Files_LoadCardState_Leave;
        } else {
            Menu_Files_UpdateLoadFiles(Menu_Files_loadCardPort);
            Menu_Files_loadCardState = 7;
        }
        break;

    case Menu_Files_LoadCardState_SelectFile:
        i = Menu_Files_UpdateLoadFiles(0);
        if (i == 0) {
            break;
        }
        if (i < 0) {
            for (i = 1; i < 3; ++i) {
                Menu_Files_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
                Menu_Files_fileMenuElements[i].targetPosition = 320;
            }
            Menu_Files_loadCardState = Menu_Files_LoadCardState_ReturnNotSelected;
            break;
        }
        return 1;
    }
    return 0;
}

// decomp: MENU7.PRG _initGameOver (src/MENU/MENU7.PRG/260.c:3395)
int Menu_Files_LoadGameOverAssetsStep(int arg0)
{

    static Content_DiscQueueSlot* Menu_Files_loadGameOverAssetsStepQueueSlot;
    static u_int* Menu_Files_loadGameOverAssetsStepGameOverBin;
    static int Menu_Files_loadGameOverAssetsStepState;
    static char Menu_Files_loadGameOverAssetsStepLayoutPadding[8] __attribute__((unused));

    Content_DiscFile cdFile;

    if (arg0 != 0) {
        cdFile.lba = VS_GAMEOVER_BIN_LBA;
        cdFile.size = VS_GAMEOVER_BIN_SIZE;
        Menu_Files_loadGameOverAssetsStepQueueSlot = Content_AllocateDiscRequest(&cdFile);
        Menu_Files_loadGameOverAssetsStepGameOverBin = Core_Memory_AllocateFromHighEnd(VS_GAMEOVER_BIN_SIZE);
        Content_EnqueueDiscRequest(Menu_Files_loadGameOverAssetsStepQueueSlot, Menu_Files_loadGameOverAssetsStepGameOverBin);
        Menu_Files_loadGameOverAssetsStepState = 0;
        return 0;
    }
    switch (Menu_Files_loadGameOverAssetsStepState) {
    case 0:
        if (Menu_Files_loadGameOverAssetsStepQueueSlot->state == 4) {
            Content_ReleaseDiscRequest(Menu_Files_loadGameOverAssetsStepQueueSlot);
            Render_UploadImageRectangle(vs_getXY(832, 256), Menu_Files_loadGameOverAssetsStepGameOverBin, vs_getWH(24, 128));
            Render_UploadImageRectangle(vs_getXY(832, 384), Menu_Files_menuItemTextClut, vs_getWH(48, 1));
            Menu_Files_loadGameOverAssetsStepState = 1;
        }
        return 0;
    case 1:
        Core_Memory_FreeFromHighEnd(Menu_Files_loadGameOverAssetsStepGameOverBin);
        return 1;
    default:
        return 0;
    }
}

// decomp: MENU7.PRG _setMenuItemClut (src/MENU/MENU7.PRG/260.c:3434)
void Menu_Files_BlendGameOverTextClut(
    short* clut, int textBlendFactor, u_short* clut0, u_short* clut1)
{
    int r0;
    int r1;
    int g0;
    int g1;
    int b0;
    int b1;
    int rdiff;
    int gdiff;
    int bdiff;
    int i;

    clut[0] = 0;

    for (i = 1; i < 16; ++i) {
        r0 = clut0[i];
        r1 = clut1[i];
        g0 = r0 & 0x3E0;
        b0 = r0 & 0x7C00;
        r0 &= 0x1F;
        g1 = r1 & 0x3E0;
        b1 = r1 & 0x7C00;
        r1 &= 0x1F;
        rdiff = (r1 - r0) * textBlendFactor;
        gdiff = (g1 - g0) * textBlendFactor;
        bdiff = (b1 - b0) * textBlendFactor;
        clut[i] = ((((r0 * 16 + rdiff) & 0x1F0) | ((g0 * 16 + gdiff) & 0x3E00)
                       | ((b0 * 16 + bdiff) & 0x7C000))
                      >> 4)
                | 0x8000;
    }
}

// decomp: MENU7.PRG _displayGameOverScreen (src/MENU/MENU7.PRG/260.c:3475)
int Menu_Files_UpdateGameOverChoices(int init)
{
    static short Menu_Files_gameOverChoicesClut[2][16];
    static int Menu_Files_gameOverChoicesColor1;
    static int Menu_Files_gameOverChoicesColor2;
    static int Menu_Files_gameOverChoicesColor3;
    static int Menu_Files_gameOverChoicesBlendFactor1;
    static int Menu_Files_gameOverChoicesBlendFactor2;
    static int Menu_Files_gameOverChoicesSelectedOption;
    static char Menu_Files_gameOverChoicesLayoutPadding[8];

    u_long* prim;
    u_long* nextPrim = Render_frameScratch.overlayOrderingTable;

    if (init != 0) {
        Menu_Files_gameOverChoicesColor1 = 0;
        Menu_Files_gameOverChoicesColor2 = 0;
        Menu_Files_gameOverChoicesColor3 = 0;
        Menu_Files_gameOverChoicesBlendFactor1 = 0;
        Menu_Files_gameOverChoicesBlendFactor2 = 0;
        Menu_Files_gameOverChoicesSelectedOption = 0;
        return 0;
    }
    if (Menu_Files_gameOverChoicesColor1 < 0) {
        prim = Render_frameScratch.primitiveCursor;
        prim[0] = (*nextPrim & 0xFFFFFF) | 0x04000000;
        prim[1] = vs_getTpage(0, 0, Render_TextureFormat_Direct16Bit, Render_BlendMode_Subtract, Render_Dithering_Off);
        prim[3] = vs_getXY(0, 0);
        prim[4] = vs_getWH(320, 240);
        prim[2] = vs_getRGB0Raw(Render_LegacyPrimitiveCode_TileSemiTrans, (Menu_Files_gameOverChoicesColor1 + 128) * 0x020202);
        *nextPrim = LegacyGpu_Handle(prim);
        Render_frameScratch.primitiveCursor = prim + 5;
        Menu_Files_gameOverChoicesColor1 += 8;
        if (Menu_Files_gameOverChoicesColor1 == 0) {
            SetDispMask(0);
            return Menu_Files_gameOverChoicesSelectedOption + 1;
        }
        return 0;
    }
    if (Menu_Files_gameOverChoicesColor1 < 128) {
        Menu_Files_gameOverChoicesColor1 += 8;
    } else {
        if (Input_buttonsPressed.allButtons & (PADstart | PADRright)) {
            Menu_PlaySelectSound();
            Audio_FadeMusicSlotVolume(1, 0, 60);
            Menu_Files_gameOverChoicesColor1 = -128;
            return 0;
        }
        if (Input_buttonsPressed.allButtons & (PADselect | PADLup | PADLdown)) {
            Menu_PlayChangeSound();
            Menu_Files_gameOverChoicesSelectedOption = 1 - Menu_Files_gameOverChoicesSelectedOption;
        }
        if (Menu_Files_gameOverChoicesSelectedOption == 0) {
            if (Menu_Files_gameOverChoicesBlendFactor1 < 16) {
                Menu_Files_gameOverChoicesColor2 = Menu_Files_gameOverChoicesBlendFactor1 * 16;
                Menu_Files_gameOverChoicesBlendFactor1 += 4;
                if (Menu_Files_gameOverChoicesBlendFactor1 > 16) {
                    Menu_Files_gameOverChoicesBlendFactor1 = 16;
                }
            } else {
                if (Menu_Files_gameOverChoicesColor2 >= 144) {
                    Menu_Files_gameOverChoicesColor2 -= 16;
                } else {
                    Menu_Files_gameOverChoicesColor2 = 128;
                }
                if (Menu_Files_gameOverChoicesColor3 < 16) {
                    Menu_Files_gameOverChoicesColor3 = 0;
                } else {
                    Menu_Files_gameOverChoicesColor3 -= 16;
                }
            }
            if (Menu_Files_gameOverChoicesBlendFactor2 != 0) {
                --Menu_Files_gameOverChoicesBlendFactor2;
            }
        } else {
            if (Menu_Files_gameOverChoicesBlendFactor2 < 16) {
                Menu_Files_gameOverChoicesColor3 = Menu_Files_gameOverChoicesBlendFactor2 * 16;
                Menu_Files_gameOverChoicesBlendFactor2 += 4;
                if (Menu_Files_gameOverChoicesBlendFactor2 > 16) {
                    Menu_Files_gameOverChoicesBlendFactor2 = 16;
                }
            } else {
                if (Menu_Files_gameOverChoicesColor3 >= 144) {
                    Menu_Files_gameOverChoicesColor3 -= 16;
                } else {
                    Menu_Files_gameOverChoicesColor3 = 128;
                }
                if (Menu_Files_gameOverChoicesColor2 < 16) {
                    Menu_Files_gameOverChoicesColor2 = 0;
                } else {
                    Menu_Files_gameOverChoicesColor2 -= 16;
                }
            }
            if (Menu_Files_gameOverChoicesBlendFactor1 != 0) {
                --Menu_Files_gameOverChoicesBlendFactor1;
            }
        }
    }

    Menu_Files_BlendGameOverTextClut(Menu_Files_gameOverChoicesClut[0], Menu_Files_gameOverChoicesBlendFactor1, Menu_Files_menuItemTextClut[0], Menu_Files_menuItemTextClut[3]);
    Menu_Files_BlendGameOverTextClut(Menu_Files_gameOverChoicesClut[1], Menu_Files_gameOverChoicesBlendFactor2, Menu_Files_menuItemTextClut[0], Menu_Files_menuItemTextClut[3]);
    Render_UploadImageRectangle(vs_getXY(832, 384), &Menu_Files_gameOverChoicesClut, vs_getWH(32, 1));

    prim = Render_QueueSprite(Menu_Files_gameOverChoicesColor1, vs_getXY(112, 128), vs_getWH(96, 32), nextPrim);
    prim[1] = vs_getTpage(832, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);
    prim[4] = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 52);

    prim = Render_QueueSprite(Menu_Files_gameOverChoicesColor1, vs_getXY(112, 160), vs_getWH(96, 32), nextPrim);
    prim[1] = vs_getTpage(832, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);
    prim[4] = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 32, 53);

    prim =
        Render_QueueSprite(Menu_Files_gameOverChoicesColor2 | 256, vs_getXY(112, 128), vs_getWH(96, 32), nextPrim);
    prim[1] = vs_getTpage(832, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);
    prim[4] = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 64, 54);

    prim =
        Render_QueueSprite(Menu_Files_gameOverChoicesColor3 | 256, vs_getXY(112, 160), vs_getWH(96, 32), nextPrim);
    prim[1] = vs_getTpage(832, 256, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);
    prim[4] = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 96, 54);

    return 0;
}

// decomp: MENU7.PRG vs_menu7_gameOver (src/MENU/MENU7.PRG/260.c:3603)
int Menu_Files_UpdateGameOver(u_char* state)
{
    enum Menu_Files_GameOverState {
        Menu_Files_GameOverState_Init,
        Menu_Files_GameOverState_LoadAssets,
        Menu_Files_GameOverState_DisplayGameOver,
        Menu_Files_GameOverState_InitFileMenu,
        Menu_Files_GameOverState_DisplayFileMenu,
        Menu_Files_GameOverState_SelectFile,
        Menu_Files_GameOverState_RestartIntro,
        Menu_Files_GameOverState_ReturnToTitle
    };

    static u_short Menu_Files_gameOverClutBuf[256];
    static State_Settings Menu_Files_gameOverSettingsBackup;
    static int Menu_Files_gameOverResetDelay;
    static int Menu_Files_gameOverDifficulty;

    RECT rect;
    int option;

    switch (*state) {
    case Menu_Files_GameOverState_Init:
        Render_RetireTexturePageRange(0x1F, 1, 6);
        Field_SetFieldRenderingEnabled(0);
        Menu_Files_gameOverSettingsBackup = Input_settings;
        Menu_Files_gameOverDifficulty = Audio_Seq_unusedStateFlags.difficulty;
        Menu_Files_LoadGameOverAssetsStep(1);
        *state = Menu_Files_GameOverState_LoadAssets;
        break;
    case Menu_Files_GameOverState_LoadAssets:
        if (Menu_Files_LoadGameOverAssetsStep(0) == 0) {
            break;
        }
        *state = Menu_Files_GameOverState_DisplayGameOver;
        Menu_Files_UpdateGameOverChoices(1);
        // Fallthrough
    case Menu_Files_GameOverState_DisplayGameOver:
        option = Menu_Files_UpdateGameOverChoices(0);
        if (option == 0) {
            break;
        }
        Menu_Files_gameOverResetDelay = 16;
        if (option != 2) {
            option = Audio_Seq_unusedStateFlags.introState;
            if (((option - 1) < 2U) && (Audio_Seq_unusedStateFlags.clearCount == 0)) {
                Save_MemoryCard_LoadIntroSnapshotStep(option);
                *state = Menu_Files_GameOverState_RestartIntro;
            } else {
                *state = Menu_Files_GameOverState_InitFileMenu;
            }
        } else {
            *state = Menu_Files_GameOverState_ReturnToTitle;
        }
        break;
    case Menu_Files_GameOverState_InitFileMenu:
        Save_MemoryCard_InitializeMenuResourcesStep(1);
        Menu_Files_ResetElements();
        setRECT(&rect, 640, 511, 256, 1);
        StoreImage(&rect, (u_long*)Menu_Files_gameOverClutBuf);
        Render_LoadClutColors(Menu_Files_gameOverClutBuf, 3, 0, 256);
        *state = Menu_Files_GameOverState_DisplayFileMenu;
        break;
    case Menu_Files_GameOverState_DisplayFileMenu:
        if (Save_MemoryCard_InitializeMenuResourcesStep(0) != 0) {
            Menu_Files_UpdateDataLoadMenu(2);
            *state = Menu_Files_GameOverState_SelectFile;
            Menu_Files_DrawBackground();
            Menu_Files_DrawFrame(Render_frameBuf);
        }
        break;
    case Menu_Files_GameOverState_SelectFile:
        SetDispMask(1);
        option = Menu_Files_UpdateDataLoadMenu(0);
        if (option != 0) {
            Save_MemoryCard_ReleaseMenuResources();
            SetDispMask(0);
            if (option < 0) {
                Menu_Files_gameOverResetDelay = 1;
                *state = Menu_Files_GameOverState_ReturnToTitle;
                break;
            }
            Audio_FreeMusicSlot(1);
            Audio_Seq_unusedStateFlags.gameOver = 0;
            Boot_EnterBattle();
        } else {
            Menu_Files_DrawBackground();
            Menu_Files_DrawFrame(Render_frameBuf);
        }
        break;
    case Menu_Files_GameOverState_RestartIntro:
        if (Save_MemoryCard_LoadIntroSnapshotStep(0) != 0) {
            Input_settings = Menu_Files_gameOverSettingsBackup;
            Audio_Seq_unusedStateFlags.difficulty = Menu_Files_gameOverDifficulty;
            Audio_FreeMusicSlot(1);
            Audio_SetMonoOutput(Input_settings.monoSound);
            Audio_Seq_unusedStateFlags.gameOver = 0;
            Boot_EnterBattle();
        }
        break;
    case Menu_Files_GameOverState_ReturnToTitle:
        if (Menu_Files_gameOverResetDelay != 0) {
            --Menu_Files_gameOverResetDelay;
        } else {
            Audio_FreeMusicSlot(1);
            Boot_ResetToTitle();
        }
        break;
    }
    Audio_Seq_unusedStateFlags.gameOver = 1;
    return 0;
}

// decomp: MENU7.PRG _promptYesNo (src/MENU/MENU7.PRG/260.c:3727)
int Menu_Files_UpdateContainerConfirmation(int initParams)
{
    enum Menu_Files_ContainerConfirmationState { Menu_Files_ContainerConfirmationState_Init, Menu_Files_ContainerConfirmationState_AnimWait, Menu_Files_ContainerConfirmationState_HandleInput, Menu_Files_ContainerConfirmationState_ReturnSelection };

    static int Menu_Files_containerConfirmationSelectedOption;
    static char Menu_Files_containerConfirmationState;
    static char Menu_Files_containerConfirmationCancelWithMenuButton;
    static char Menu_Files_containerConfirmationLayoutPadding[34] __attribute__((unused));

    Menu_Framework_MenuRow* menuItem;
    u_int buttons;

    if (initParams != 0) {
        menuItem = Menu_Framework_SetMenuItem(0x1E, 320, 146, 0x7E, 0,
            (char*)&Menu_Files_containerStrings[Text_Menu7_container_Offset_yesOption]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        Menu_Files_containerConfirmationState = Menu_Files_ContainerConfirmationState_Init;
        Menu_Files_containerConfirmationSelectedOption = (initParams - 1) & 1;
        Menu_Files_containerConfirmationCancelWithMenuButton = initParams >> 2;
        return 0;
    }
    switch (Menu_Files_containerConfirmationState) {
    case Menu_Files_ContainerConfirmationState_Init:
        menuItem = Menu_Framework_SetMenuItem(0x1F, 320, 162, 0x7E, 0,
            (char*)&Menu_Files_containerStrings[Text_Menu7_container_Offset_noOption]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        Menu_Files_containerConfirmationState = Menu_Files_ContainerConfirmationState_AnimWait;
        break;
    case Menu_Files_ContainerConfirmationState_AnimWait:
        Menu_Files_containerConfirmationState += Menu_Framework_AreMenuRowsIdle();
        break;
    case Menu_Files_ContainerConfirmationState_HandleInput:
        Menu_Framework_MenuItem(30 + Menu_Files_containerConfirmationSelectedOption)->selected = 1;
        Menu_Framework_MenuItem(31 - Menu_Files_containerConfirmationSelectedOption)->selected = 0;
        buttons = Input_buttonsPressed.allButtons;
        if (Menu_Files_containerConfirmationCancelWithMenuButton == 0) {
            if (buttons & PADRup) {
                buttons -= PADRup;
                Menu_PlayInvalidSound();
            }
        }
        if (buttons & (PADRup | PADRright | PADRdown)) {
            if ((Menu_Files_containerConfirmationSelectedOption != 0) || (buttons & (PADRup | PADRdown))) {
                Menu_PlayLeaveSound();
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Files_containerConfirmationSelectedOption = -1;
            } else {
                Menu_PlaySelectSound();
                Menu_Framework_ClearMenuExcept(0);
                Menu_Files_containerConfirmationSelectedOption = 1;
            }
            Menu_Files_containerConfirmationState = Menu_Files_ContainerConfirmationState_ReturnSelection;
        } else {
            Menu_Files_containerConfirmationCursorAnimation = Menu_Framework_DrawMainMenuCursor(Menu_Files_containerConfirmationCursorAnimation, Menu_Files_containerConfirmationSelectedOption + 8);
            if (Input_buttonRepeat & (PADLup | PADLdown)) {
                Menu_PlayChangeSound();
                Menu_Files_containerConfirmationSelectedOption = 1 - Menu_Files_containerConfirmationSelectedOption;
            }
        }
        break;
    case Menu_Files_ContainerConfirmationState_ReturnSelection:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Files_containerConfirmationSelectedOption;
        }
        break;
    }
    return 0;
}

// decomp: MENU7.PRG _getContainerIndexOffset (src/MENU/MENU7.PRG/260.c:3804)
void* Menu_Files_ContainerIndexSection(int section, Items_ContainerIndices* indices)
{
    int i = 0;
    int offset = 0;

    if (section > 0) {
        do {
            offset += Menu_Files_containerItemCapacities[i++];
        } while (i < section);
    }
    return (u_short*)indices + offset;
}

// decomp: MENU7.PRG _copyContainer (src/MENU/MENU7.PRG/260.c:3821)
void Menu_Files_CopySelectedContainerItems(Items_ContainerData* target, Items_ContainerData* source,
    Items_ContainerIndices* toCopy)
{
    int i;
    int j;
    Items_ContainerIndices* targetIndices;
    int sourceIndex;

    targetIndices = Core_Memory_AllocateFromHighEnd(sizeof *targetIndices);
    Core_Memory_ZeroWordsBackward(targetIndices, sizeof *targetIndices);
    Core_Memory_ZeroWordsBackward(target, sizeof *target);

    for (i = 0; i < 7; ++i) {
        u_short* sourceOffset = Menu_Files_ContainerIndexSection(i, toCopy);
        u_short* targetOffset = Menu_Files_ContainerIndexSection(i, targetIndices);
        for (j = 0; j < Menu_Files_containerItemCapacities[i]; ++j) {
            if (sourceOffset[j] != 0) {
                targetOffset[sourceOffset[j] - 1] = j + 1;
            }
        }
    }

    for (i = 0; i < 32; ++i) {
        sourceIndex = toCopy->weapons[i];
        if (sourceIndex != 0) {
            Items_InventoryWeapon* weapon = &target->weapons[i];
            Core_Memory_CopyHalfwords(
                weapon, &source->weapons[sourceIndex - 1], sizeof *weapon);
            weapon->index = i + 1;
            weapon->blade = targetIndices->blades[weapon->blade - 1];
            weapon->grip = targetIndices->grips[weapon->grip - 1];

            for (j = 0; j < 4; ++j) {
                if (weapon->gems[j] != 0) {
                    weapon->gems[j] = targetIndices->gems[weapon->gems[j] - 1];
                }
            }
        }
    }

    for (i = 0; i < 64; ++i) {
        sourceIndex = toCopy->blades[i];
        if (sourceIndex != 0) {
            Items_InventoryBlade* blade = &target->blades[i];
            Core_Memory_CopyHalfwords(blade, &source->blades[sourceIndex - 1], sizeof *blade);
            blade->index = i + 1;
            if (blade->assembledWeaponIndex != 0) {
                blade->assembledWeaponIndex =
                    targetIndices->weapons[blade->assembledWeaponIndex - 1];
            }
        }
    }

    for (i = 0; i < 64; ++i) {
        sourceIndex = toCopy->grips[i];
        if (sourceIndex != 0) {
            Items_InventoryGrip* grip = &target->grips[i];
            Core_Memory_CopyHalfwords(grip, &source->grips[sourceIndex - 1], sizeof *grip);
            grip->index = i + 1;
            if (grip->assembledWeaponIndex != 0) {
                grip->assembledWeaponIndex =
                    targetIndices->weapons[grip->assembledWeaponIndex - 1];
            }
        }
    }

    for (i = 0; i < 32; ++i) {
        sourceIndex = toCopy->shields[i];
        if (sourceIndex != 0) {
            Items_InventoryShield* shield = &target->shields[i];
            Core_Memory_CopyHalfwords(
                shield, &source->shields[sourceIndex - 1], sizeof *shield);
            shield->index = i + 1;
            for (j = 0; j < 4; ++j) {
                if (shield->gems[j] != 0) {
                    shield->gems[j] = targetIndices->gems[shield->gems[j] - 1];
                }
            }
        }
    }

    for (i = 0; i < 64; ++i) {
        sourceIndex = toCopy->armor[i];
        if (sourceIndex != 0) {
            Items_InventoryArmor* armor = &target->armor[i];
            Core_Memory_CopyHalfwords(armor, &source->armor[sourceIndex - 1], sizeof *armor);
            armor->index = i + 1;
        }
    }

    for (i = 0; i < 192; ++i) {
        sourceIndex = toCopy->gems[i];
        if (sourceIndex != 0) {
            Items_InventoryGem* gem = &target->gems[i];
            Core_Memory_CopyHalfwords(gem, &source->gems[sourceIndex - 1], sizeof *gem);
            gem->index = i + 1;
            if (gem->setItemIndex != 0) {
                if (gem->setItemIndex & 0x80) {
                    gem->setItemIndex =
                        targetIndices->shields[(gem->setItemIndex & 0xFFFF) - 0x81]
                        | 0x80;
                } else {
                    gem->setItemIndex = targetIndices->weapons[gem->setItemIndex - 1];
                }
            }
        }
    }

    for (i = 0; i < 256; ++i) {
        sourceIndex = toCopy->misc[i];
        if (sourceIndex != 0) {
            Items_InventoryMisc* item = &target->misc[i];
            Core_Memory_CopyBytesBackward(item, &source->misc[sourceIndex - 1], sizeof *item);
            item->index = i + 1;
        }
    }

    Core_Memory_FreeFromHighEnd(targetIndices);
}

// decomp: MENU7.PRG _drawPlayTime (src/MENU/MENU7.PRG/260.c:4157)
void Menu_Files_DrawPlayTime(void)
{

    char time[4];
    int digits;
    int i;

    *(int*)&time = State_playTime.all;
    if (time[3] >= 100) {
        *(int*)&time = 0;
        time[3] = 0;
        Render_DrawCompactText("1", 0xC800D8, Render_frameScratch.orderingTable - 0x14);
    } else {
        time[0] = ((((time[0] << 14) + Core_Random_NextInterfaceRandomBits(15)) * 5u) >> 14) / 3;
    }
    for (i = 0; i < 4; ++i) {
        digits = Core_Math_SplitDecimalDigit(time[i]);
        Menu_Files_playTime[9 - i * 3] = (digits >> 4) + 48;
        Menu_Files_playTime[10 - i * 3] = (digits & 0xF) + 48;
    }
    Render_DrawCompactText(Menu_Files_playTime, 0xC800E0, Render_frameScratch.orderingTable - 0x14);
    Render_DrawCompactText("PLAY    TIME", 0xBC00E0, Render_frameScratch.orderingTable - 0x14);
}

// decomp: MENU7.PRG vs_menu7_dataMenu (src/MENU/MENU7.PRG/260.c:4187)
int Menu_Files_UpdateDataMenu(u_char* state)
{
    enum Menu_Files_DataMenuState {
        Menu_Files_DataMenuState_None = 0,
        Menu_Files_DataMenuState_Init = 3,
        Menu_Files_DataMenuState_LoadWait,
        Menu_Files_DataMenuState_InitSaveMenu = 7,
        Menu_Files_DataMenuState_ShowSaveMenu,
        Menu_Files_DataMenuState_InitLoadMenu,
        Menu_Files_DataMenuState_HandleInput
    };

    char* menuStrings[4];
    int rowTypes[2];
    RECT rect;
    int row;

    if (*state < 11) {
        Menu_Files_DrawPlayTime();
    }

    switch (*state) {
    case Menu_Files_DataMenuState_Init:
        Save_MemoryCard_InitializeMenuResourcesStep(1);
        *state = Menu_Files_DataMenuState_LoadWait;
        break;

    case Menu_Files_DataMenuState_LoadWait:
        if ((Menu_Framework_AreMenuRowsIdle() == 0) || (Save_MemoryCard_InitializeMenuResourcesStep(0) == 0)) {
            break;
        }
    // Fallthrough
    case 5:
        for (row = 0; row < 2; ++row) {
            menuStrings[row * 2] = (char*)(&Menu_Files_textTable[Menu_Files_textTable[row * 2]]);
            menuStrings[row * 2 + 1] =
                (char*)(&Menu_Files_textTable[Text_Menu_MCMAN_BIN_Offset_saveDisabled]);
            rowTypes[row] = 0;
        }

        Menu_saveLocationId = Field_CurrentSavePointId();
        if (!Menu_saveLocationId || (Audio_Seq_unusedStateFlags.opaqueA7 != 0)) {
            rowTypes[0] |= 1;
        }
        row = Input_settings.cursorMemory;
        if ((*state) != Menu_Files_DataMenuState_LoadWait) {
            Input_settings.cursorMemory = 1;
        }

        Menu_Framework_CreateMenuRows(2, 0x143, menuStrings, rowTypes);
        Input_settings.cursorMemory = row;
        Field_SetFieldRenderingEnabled(0);
        *state = 6;
        break;

    case 6:
        row = Menu_Framework_GetSelectedRow() + 1;
        if (row != 0) {
            if (row > 0) {
                Menu_Framework_ClearMenuExcept(6);
                Menu_Files_ResetElements();
                switch (row) {
                case 1:
                    *state = Menu_Files_DataMenuState_InitSaveMenu;
                    break;

                case 2:
                    *state = Menu_Files_DataMenuState_InitLoadMenu;
                    break;
                }

            } else {
                Field_SetFieldRenderingEnabled(1);
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                if (row == -2) {
                    *state = 12;
                } else {
                    *state = 11;
                }
            }
        } else {
            if (((Menu_saveLocationId != 0) && (Audio_Seq_unusedStateFlags.opaqueA7 != 0))
                && (Menu_Framework_HighlightedRow() == 0)) {
                Menu_InitInformationTextBox(1);
                Menu_textBoxes[7].state = 11;
            } else {
                Menu_textBoxes[7].state = 0;
            }
        }
        break;

    case Menu_Files_DataMenuState_InitSaveMenu:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Files_UpdateDataSaveMenu(1);
            *state = Menu_Files_DataMenuState_ShowSaveMenu;
        }
        break;

    case Menu_Files_DataMenuState_ShowSaveMenu:
        if (Menu_Files_UpdateDataSaveMenu(0) != 0) {
            *state = 5;
        }
        Menu_Files_DrawFrame(Render_frameBuf);
        break;

    case Menu_Files_DataMenuState_InitLoadMenu:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Files_UpdateDataLoadMenu(1);
            *state = Menu_Files_DataMenuState_HandleInput;
        }
        break;

    case Menu_Files_DataMenuState_HandleInput:
        row = Menu_Files_UpdateDataLoadMenu(0);
        if (row != 0) {
            if (row < 0) {
                *state = 5;
                break;
            }
            setRECT(&rect, 0, 0, 640, 240);
            ClearImage(&rect, 0, 0, 0);
            Save_MemoryCard_ReleaseMenuResources();
            *state = 0;
            Boot_EnterBattle();
            break;
        }
        Menu_Files_DrawFrame(Render_frameBuf);
        break;

    case 11:
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetBackgroundFade(0);
        *state = 13;
        break;

    case 12:
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetBackgroundFade(0);
        *state = 14;
        break;

    case 13:
    case 14:
        if (Menu_Framework_backgroundFadeStep != 0) {
            break;
        }
        Save_MemoryCard_ReleaseMenuResources();
        if (*state == 14) {
            Menu_Framework_menuState.currentMenu = 7;
        }
        *state = Menu_Files_DataMenuState_None;
        return 1;
    }

    return 0;
}

// decomp: TITLE.PRG _findCurrentSave (src/TITLE/TITLE.PRG/22C.c:1427)
int Menu_Files_FindCurrentSaveStep(int init)
{
    static char Menu_Files_findCurrentSaveStepState;
    static char Menu_Files_findCurrentSaveStepSubState;

    int port;
    int currentSave;
    int event;
    int realPort;

    if (init != 0) {
        Menu_Files_findCurrentSaveStepState = 0;
        Menu_Files_findCurrentSaveStepSubState = 0;
        return 0;
    }

    realPort = Menu_Files_findCurrentSaveStepState >> 1;
    port = realPort + 1;

    if ((Menu_Files_findCurrentSaveStepState & 1) == 0) {
        Title_memoryCardMessage =
            (char*)(Title_saveMenuText + Title_saveMenuText[Text_Menu_MCMAN_BIN_Index_accessing0 - 1 + port]);
        Title_SaveCard_AdvanceCardReadiness(realPort + 1);
        ++Menu_Files_findCurrentSaveStepState;
    } else {
        event = Title_SaveCard_AdvanceCardReadiness(0) & 3;

        if (event != Save_CardReadinessResult_Pending) {
            if (event == Save_CardReadinessResult_Ready) {
                if (Title_SaveCard_ScanSaveDirectory(realPort + 1) == 0) {
                    currentSave = Title_SaveCard_FindMatchingSaveSlot();
                    if (currentSave != 0) {
                        return currentSave + (realPort * 16);
                    }
                }
            } else if (event == Save_CardReadinessResult_Timeout) {
                if (Menu_Files_findCurrentSaveStepState != 1) {
                    if (Menu_Files_findCurrentSaveStepSubState != 0) {
                        return Save_FindSaveResult_Timeout;
                    }
                } else {
                    Menu_Files_findCurrentSaveStepSubState = Menu_Files_findCurrentSaveStepState;
                }
            }
            ++Menu_Files_findCurrentSaveStepState;
        }
    }
    return Menu_Files_findCurrentSaveStepState != 4 ? Save_FindSaveResult_Pending : Save_FindSaveResult_NotFound;
}

// decomp: TITLE.PRG _initSaveScreen (src/TITLE/TITLE.PRG/22C.c:3074)
void Menu_Files_InitializeDisplay(void)
{
    DISPENV disp;
    DRAWENV draw;
    RECT rect;

    Menu_Files_frameBuf = 0;
    SetDefDispEnv(&disp, 320, 0, 320, 240);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    disp.screen.y = 8;
    disp.screen.h = 224;
    PutDispEnv(&disp);
    PutDrawEnv(&draw);
    setRECT(&rect, 0, 0, 640, 240);
    ClearImage(&rect, 0, 0, 0);
    SetDispMask(1);
}

// decomp: TITLE.PRG _saveScreenSwapBuf (src/TITLE/TITLE.PRG/22C.c:3093)
void Menu_Files_SwapBuffers(void)
{
    DISPENV disp;
    DRAWENV draw;
    int offset;

    Menu_Files_frameBuf = (1 - Menu_Files_frameBuf);
    offset = Menu_Files_frameBuf * 320;
    SetDefDispEnv(&disp, 320 - offset, 0, 320, 240);
    SetDefDrawEnv(&draw, offset, 0, 320, 240);
    disp.screen.y = 8;
    disp.screen.h = 224;
    PutDispEnv(&disp);
    PutDrawEnv(&draw);
}

// decomp: TITLE.PRG _saveScreenConfirmation (src/TITLE/TITLE.PRG/22C.c:3226)
int Menu_Files_UpdateClearSaveReminder(int init)
{
    enum Title_CardSelectionPhase {
        Title_CardSelectionPhase_InitializeSlotOne = 0,
        Title_CardSelectionPhase_InitializeSlotTwo = 1,
        Title_CardSelectionPhase_WaitForSlotAnimation = 2,
        Title_CardSelectionPhase_HandleInput = 3,
        Title_CardSelectionPhase_ReturnSelection = 4
    };

    static char Menu_Files_clearSaveReminderState;
    static u_char Menu_Files_clearSaveReminderConfirm;
    static char Title_clearSaveConfirmationPadding[5] __attribute__((unused));

    Menu_Files_FileMenuElement* elem;
    int i;

    if (init != 0) {
        Menu_Files_clearSaveReminderConfirm = 1;
        Menu_Files_clearSaveReminderState = Title_CardSelectionPhase_InitializeSlotOne;
        return 0;
    }

    switch (Menu_Files_clearSaveReminderState) {
    case Title_CardSelectionPhase_InitializeSlotOne:
    case Title_CardSelectionPhase_InitializeSlotTwo:
        elem = Title_SaveFiles_InitializeElement(Menu_Files_clearSaveReminderState + 1, vs_getXY(320, Menu_Files_clearSaveReminderState * 16 + 146),
            vs_getWH(126, 12),
            (char*)&Title_saveMenuText[Title_saveMenuText[Menu_Files_clearSaveReminderState + Text_Menu_MCMAN_BIN_Index_yesOption]]);
        elem->state = Menu_Files_ElementState_AnimateX;
        elem->targetPosition = 194;
        ++Menu_Files_clearSaveReminderState;
        break;
    case Title_CardSelectionPhase_WaitForSlotAnimation:
        Menu_Files_clearSaveReminderState += Title_SaveFiles_AreAnimationsComplete();
        break;
    case Title_CardSelectionPhase_HandleInput:
        Title_fileMenuElements[Menu_Files_clearSaveReminderConfirm].selected = 1;
        Title_fileMenuElements[3 - Menu_Files_clearSaveReminderConfirm].selected = 0;
        if (Input_buttonsPressed.allButtons & PADRdown) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_INVALID);
        }
        if (Input_buttonsPressed.allButtons & PADRright) {
            Menu_Files_clearSaveReminderState = Title_CardSelectionPhase_ReturnSelection;
        }
        if (Menu_Files_clearSaveReminderState == Title_CardSelectionPhase_ReturnSelection) {
            if (Menu_Files_clearSaveReminderConfirm == 1) {
                Title_PlayMenuSelectSound();
            } else {
                Title_PlayMenuLeaveSound();
            }
            Title_selectionCursorPosition = 0;
            for (i = 1; i < 3; ++i) {
                Title_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
                Title_fileMenuElements[i].targetPosition = 320;
            }
            break;
        }
        if (Input_buttonRepeat & (PADLup | PADLdown)) {
            Title_PlayMenuChangeSound();
            Menu_Files_clearSaveReminderConfirm = 3 - Menu_Files_clearSaveReminderConfirm;
        }
        Title_selectionCursorPosition = vs_getXY(180, (Menu_Files_clearSaveReminderConfirm + 7) * 16 + 10);
        break;
    case Title_CardSelectionPhase_ReturnSelection:
        if (Title_SaveFiles_AreAnimationsComplete() != 0) {
            return Menu_Files_clearSaveReminderConfirm;
        }
        break;
    }
    return 0;
}

// decomp: TITLE.PRG _saveScreenExitScreen (src/TITLE/TITLE.PRG/22C.c:3299)
void Menu_Files_DrawExitFade(void)
{
    Title_SaveFiles_DrawImmediateSprite(vs_getXY(256, 0), 0, vs_getWH(64, 224),
        getTPage(Render_TextureFormat_Direct16Bit, Render_BlendMode_Subtract, (1 - Menu_Files_frameBuf) * 320 + 256, 0));
    Title_SaveFiles_DrawImmediateSprite(0, 0, vs_getWH(256, 224),
        getTPage(Render_TextureFormat_Direct16Bit, Render_BlendMode_Subtract, (1 - Menu_Files_frameBuf) * 320, 0));
    Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.tile, Render_LegacyPrimitiveAddress_Null);
    Title_fileMenuPrimitiveBuffer.prim.tile.colorAndCode = vs_getRGB0(Render_LegacyPrimitiveCode_TileSemiTrans, 8, 8, 8);
    Title_fileMenuPrimitiveBuffer.prim.tile.position = 0;
    Title_fileMenuPrimitiveBuffer.prim.tile.size = vs_getWH(320, 224);
    DrawPrim(&Title_fileMenuPrimitiveBuffer);
}

// MENU7.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Files_ResetMenu7Statics(void)
{
    {
        static const u_short initial[] = { 32, 64, 64, 32, 64, 192, 256, 2 };
        memcpy((void*)&Menu_Files_containerItemCapacities, &initial, sizeof Menu_Files_containerItemCapacities);
    }
    {
        static const signed char initial[] = { 0, -56, -112, -104, -96, -88, -80, -72,
    -64, -56, -48, -40, -32, -24, -16, -8 };
        memcpy((void*)&Menu_Files_cursorFileOpSaturation, &initial, sizeof Menu_Files_cursorFileOpSaturation);
    }
    {
        static const char initial[] = { 0, 1, 2, 4, 8, 16, 32, 56, 80, 104, 128, 152, 176,
    200, 224, 248, 255, 255, 255, 255 };
        memcpy((void*)&Menu_Files_menuElementStops, &initial, sizeof Menu_Files_menuElementStops);
    }
    {
        static const char initial[] = { 0, 1, 10, 100 };
        memcpy((void*)&Menu_Files_hpMpSummaryDigitDivisors, &initial, sizeof Menu_Files_hpMpSummaryDigitDivisors);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Files_textArrowState, &initial, sizeof Menu_Files_textArrowState);
    }
    {
        static const int initial[] = { vs_getRGB888(0x40, 0x30, 0x66),
        vs_getRGB888(0x40, 0x38, 0x20) };
        memcpy((void*)&Menu_Files_elementBackgroundColorColors1, &initial, sizeof Menu_Files_elementBackgroundColorColors1);
    }
    {
        static const int initial[] = {
        vs_getRGB888(0x08, 0x08, 0x20),
        vs_getRGB888(0x10, 0x10, 0x08),
    };
        memcpy((void*)&Menu_Files_elementBackgroundColorColors2, &initial, sizeof Menu_Files_elementBackgroundColorColors2);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Files_containerConfirmationCursorAnimation, &initial, sizeof Menu_Files_containerConfirmationCursorAnimation);
    }
    {
        static const char initial[] = "00:00:00:00";
        memcpy((void*)&Menu_Files_playTime, &initial, sizeof Menu_Files_playTime);
    }
}

