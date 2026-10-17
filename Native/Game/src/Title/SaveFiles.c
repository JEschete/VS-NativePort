// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Feedback.h"
#include "Content/StringTables.h"
#include "Core/Memory/Copy.h"
#include "Core/Text/TextCode.h"
#include "Event/ScriptAudio.h"
#include "Field/Doors.h"
#include "Field/Mpd.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Menu/Files.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/TextureAnimation.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "Save/MemoryCard/Resources.h"
#include "State/Ashley.h"
#include "State/GameTime.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
#include "Title/SaveCard.h"
#include "Title/SaveFiles.h"

// decomp: _fontTable (_fontTable@TITLE/TITLE.PRG)
Title_SaveMenuFontRecord Title_saveMenuFontAtlas[1]; /* contents: module image (ModuleData.c) */

// decomp: _debugFont (_debugFont@TITLE/TITLE.PRG)
u_long Title_debugFontAtlas[4928]; /* contents: module image (ModuleData.c) */

// decomp: _uiTable (_uiTable@TITLE/TITLE.PRG)
Title_SaveMenuUiRecord Title_saveMenuUiAtlas; /* contents: module image (ModuleData.c) */

// decomp: _2 (_2@src/TITLE/TITLE.PRG/22C.c)
char Title_fileMenuPrimitivePadding[8];

// decomp: _3 (_3@src/TITLE/TITLE.PRG/22C.c)
char Title_saveScreenPadding;

// decomp: _backupMainSetting (_backupMainSetting@src/TITLE/TITLE.PRG/22C.c)
char Title_savedMainMenuSetting;

// decomp: _containerDataEmpty (_containerDataEmpty@src/TITLE/TITLE.PRG/22C.c)
char Title_isContainerDataEmpty;

// decomp: _cursorFileOpSaturation (_cursorFileOpSaturation@src/TITLE/TITLE.PRG/22C.c)
signed char Title_kFileOperationCursorSaturation[] = { 0, -56, -112, -104, -96, -88, -80, -72,
    -64, -56, -48, -40, -32, -24, -16, -8 };

// decomp: _dataNotSaved (_dataNotSaved@src/TITLE/TITLE.PRG/22C.c)
char Title_hasUnsavedData;

// decomp: _fileMenuElements (_fileMenuElements@src/TITLE/TITLE.PRG/22C.c)
Menu_Files_FileMenuElement Title_fileMenuElements[10];

// decomp: _fileMenuScreenFade (_fileMenuScreenFade@src/TITLE/TITLE.PRG/22C.c)
char Title_fileMenuScreenFade;

// decomp: _filePreviousProgressCounter (_filePreviousProgressCounter@src/TITLE/TITLE.PRG/22C.c)
u_short Title_previousTransferProgress;

// decomp: _fileProgressCounter (_fileProgressCounter@src/TITLE/TITLE.PRG/22C.c)
int Title_transferProgressCounter;

// decomp: _fileProgressPosition (_fileProgressPosition@src/TITLE/TITLE.PRG/22C.c)
u_short Title_transferProgressPosition;

// decomp: _fileProgressTarget (_fileProgressTarget@src/TITLE/TITLE.PRG/22C.c)
u_short Title_transferProgressTarget;

// decomp: _isSaving (_isSaving@src/TITLE/TITLE.PRG/22C.c)
char Title_isSaving;

// decomp: _memoryCardMessage (_memoryCardMessage@src/TITLE/TITLE.PRG/22C.c)
char* Title_memoryCardMessage;

// decomp: _menuElementStops (_menuElementStops@src/TITLE/TITLE.PRG/22C.c)
char Title_kFileMenuAnimationStops[] = { 0, 1, 2, 4, 8, 16, 32, 56, 80, 104, 128, 152, 176,
    200, 224, 248, 255, 255, 255, 255 };

// decomp: _primBuf (_primBuf@src/TITLE/TITLE.PRG/22C.c)
Menu_Files_PrimitiveBuffer Title_fileMenuPrimitiveBuffer;

// decomp: _saveFileInfo (_saveFileInfo@src/TITLE/TITLE.PRG/22C.c)
Save_CardFileInfoRecord* Title_saveFileSummaries;

// decomp: _saveMenuBg_clut (_saveMenuBg_clut@TITLE/TITLE.PRG)
Title_SaveMenuBackgroundRecord Title_saveMenuBackground; /* contents: module image (ModuleData.c) */

// decomp: _selectCursorColor (_selectCursorColor@src/TITLE/TITLE.PRG/22C.c)
u_char Title_selectionCursorColor;

// decomp: _selectCursorXy (_selectCursorXy@src/TITLE/TITLE.PRG/22C.c)
int Title_selectionCursorPosition;

// decomp: _spmcimg (_spmcimg@src/TITLE/TITLE.PRG/22C.c)
char* Title_memoryCardUiResource;

// decomp: _textTable (_textTable@src/TITLE/TITLE.PRG/22C.c)
u_short* Title_saveMenuText;

// decomp: TITLE.PRG _drawSprt (src/TITLE/TITLE.PRG/22C.c:1082)
void Title_SaveFiles_DrawImmediateSprite(int xy, int uvClut, int wh, int tpage)
{
    Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.sprt, Render_LegacyPrimitiveAddress_Null);
    Title_fileMenuPrimitiveBuffer.prim.sprt.texturePageCommand = vs_getTpageRaw(tpage & 0x9FF);
    Title_fileMenuPrimitiveBuffer.prim.sprt.colorCommand =
        vs_getRGB0Raw(Render_LegacyPrimitiveCode_Sprite, ((0x80 - (tpage >> 16)) * 0x010101));
    Title_fileMenuPrimitiveBuffer.prim.sprt.position = xy;
    Title_fileMenuPrimitiveBuffer.prim.sprt.uvPalette = uvClut;
    Title_fileMenuPrimitiveBuffer.prim.sprt.size = wh;
    DrawPrim(&Title_fileMenuPrimitiveBuffer);
}

// decomp: TITLE.PRG _drawSaveInfoUI (src/TITLE/TITLE.PRG/22C.c:1113)
void Title_SaveFiles_DrawSummaryLabel(int xy, enum Menu_Files_FileMenuUiId id)
{
    static int Title_saveSummaryUvClutWords[] = { vs_getUV0Clut(248, 8, 832, 223),
        vs_getUV0Clut(242, 0, 832, 223), vs_getUV0Clut(224, 120, 848, 223),
        vs_getUV0Clut(128, 56, 848, 223), vs_getUV0Clut(224, 112, 848, 223),
        vs_getUV0Clut(104, 56, 848, 223), vs_getUV0Clut(170, 76, 864, 223),
        vs_getUV0Clut(144, 0, 864, 223), vs_getUV0Clut(160, 0, 864, 223),
        vs_getUV0Clut(32, 9, 832, 223) };
    static int Title_kSaveSummaryGlyphSizes[] = { vs_getXY(3, 8), vs_getXY(13, 8), vs_getXY(17, 8),
        vs_getXY(22, 8), vs_getXY(26, 8), vs_getXY(19, 8), vs_getXY(10, 10),
        vs_getXY(16, 9), vs_getXY(18, 9), vs_getXY(5, 7) };

    Title_SaveFiles_DrawImmediateSprite(xy, Title_saveSummaryUvClutWords[id], Title_kSaveSummaryGlyphSizes[id],
        getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, 768, 0));
}

// decomp: TITLE.PRG _drawHPMP (src/TITLE/TITLE.PRG/22C.c:1154)
void Title_SaveFiles_DrawHpMpSummary(int xy, enum Menu_Files_StatType stat, u_int currentValue, u_int maxValue)
{
    static char Title_kHpMpDigitDivisors[] = { 0, 1, 10, 100 };
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

    Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.tilePoly, Render_LegacyPrimitiveAddress_Null);
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.tileTpage.texturePageCommand =
        vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_On);
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.tileTpage.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 40, 64);
    wh = vs_getWH(66, 5);
    x1y1 = (currentValueDigits + vs_getXY(0, 9));
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.tileTpage.position = xy++ + vs_getXY(0, 8);
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.tileTpage.size = wh;
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.colorZeroAndCode = vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4, rgb1);
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionZero = xy + vs_getXY(0, 9);
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.colorOne = rgb0;
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionOne = xy + x1y1;
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.colorTwo = rgb1;
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionTwo = xy + vs_getXY(0, 12);
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.colorThree = rgb0;
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionThree = vs_getXY(0, 12);
    Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionThree =
        xy + (currentValueDigits + Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionThree);
    DrawPrim(&Title_fileMenuPrimitiveBuffer);

    currentValueDigits = Menu_Files_DecimalDigitCount(currentValue);
    maxValueDigits = Menu_Files_DecimalDigitCount(maxValue);
    Title_SaveFiles_DrawSummaryLabel(xy - 1, stat + Menu_Files_FileMenuUiId_Hp);
    wh = currentValueDigits * 6;
    new_var4 = 55;
    xy = (xy - (wh - new_var4)) - (maxValueDigits * 5);
    placeDivisor = Title_kHpMpDigitDivisors[currentValueDigits];
    do {
        Title_SaveFiles_DrawImmediateSprite(xy - vs_getXY(0, 1),
            (((currentValue / placeDivisor) << 3) + 64) | (getClut(832, 223) << 16),
            vs_getWH(7, 12), getTPage(0, 0, 768, 0));
        currentValue = currentValue % placeDivisor;
        placeDivisor /= 10;
        xy += 6;
    } while (placeDivisor != 0);
    Title_SaveFiles_DrawSummaryLabel(xy + 1, Menu_Files_FileMenuUiId_Dot);
    Menu_Files_DrawInteger(xy + 6, maxValue, Title_kHpMpDigitDivisors[maxValueDigits]);
}

// decomp: TITLE.PRG _fileProcessingAnim (src/TITLE/TITLE.PRG/22C.c:1233)
void Title_SaveFiles_DrawTransferProgress(int x, int y)
{
    char* new_var __attribute__((unused));
    int gradientColor1;
    int gradientColor2;
    u_int leftEdge;
    u_int i;

    leftEdge = x - 48;
    new_var = &Title_isSaving;

    if (leftEdge < 64) {
        leftEdge = 64;
    }

    gradientColor1 = 0;
    gradientColor2 = vs_getRGB888(224, 255, 0);

    if (Title_isSaving != 0) {
        gradientColor2 = vs_getRGB888(255, 0, 127);
    }

    for (i = 0; i < 2; ++i) {
        Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage, Render_LegacyPrimitiveAddress_Null);
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.colorZeroAndCode =
            vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4SemiTrans, gradientColor1);
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.positionZero = leftEdge | y;
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.positionOne = x | y;
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.positionTwo = leftEdge | (y + vs_getXY(0, 32));
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.texturePage =
            vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_On);
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.colorOne = gradientColor2;
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.colorTwo = gradientColor1;
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.colorThree = gradientColor2;
        Title_fileMenuPrimitiveBuffer.prim.polyG4Tpage.positionThree = x | (y + vs_getXY(0, 32));
        DrawPrim(&Title_fileMenuPrimitiveBuffer);
        leftEdge = x;
        x += 16;
        gradientColor1 = gradientColor2;
        gradientColor2 = 0;
    }
}

// decomp: TITLE.PRG _fileProcessingCompleteAnim (src/TITLE/TITLE.PRG/22C.c:1277)
void Title_SaveFiles_DrawTransferComplete(int colour, int y)
{
    int yOfst;
    int colour0;
    u_int i;
    int colour1 = 0;

    if (Title_isSaving != 0) {
        colour0 = colour + (colour << 15);
    } else {
        colour0 = (colour - (colour >> 3)) + (colour << 8);
    }

    Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.tileTpage, Render_LegacyPrimitiveAddress_Null);
    Title_fileMenuPrimitiveBuffer.prim.tileTpage.texturePageCommand =
        vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_On);
    Title_fileMenuPrimitiveBuffer.prim.tileTpage.colorCommand = vs_getRGB0Raw(Render_LegacyPrimitiveCode_TileSemiTrans, colour0);
    Title_fileMenuPrimitiveBuffer.prim.tileTpage.position = y | 64;
    Title_fileMenuPrimitiveBuffer.prim.tileTpage.size = vs_getWH(256, 32);
    DrawPrim(&Title_fileMenuPrimitiveBuffer);

    for (i = 0; i < 2; ++i) {
        Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.polyG4, Render_LegacyPrimitiveAddress_Null);
        yOfst = y + vs_getXY(0, -8);
        Title_fileMenuPrimitiveBuffer.prim.polyG4.colorZeroAndCode = vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4SemiTrans, colour1);
        Title_fileMenuPrimitiveBuffer.prim.polyG4.positionZero = yOfst | 64;
        Title_fileMenuPrimitiveBuffer.prim.polyG4.colorOne = colour1;
        Title_fileMenuPrimitiveBuffer.prim.polyG4.positionOne = yOfst | 320;
        Title_fileMenuPrimitiveBuffer.prim.polyG4.colorTwo = colour0;
        Title_fileMenuPrimitiveBuffer.prim.polyG4.positionTwo = y | 64;
        Title_fileMenuPrimitiveBuffer.prim.polyG4.colorThree = colour0;
        Title_fileMenuPrimitiveBuffer.prim.polyG4.positionThree = y | 320;
        DrawPrim(&Title_fileMenuPrimitiveBuffer);
        y += vs_getXY(0, 40);
        colour1 = colour0;
        colour0 = 0;
    }
}

// decomp: TITLE.PRG _initFileMenu (src/TITLE/TITLE.PRG/22C.c:1321)
void Title_SaveFiles_ResetElements(void)
{
    Title_memoryCardMessage = NULL;
    Title_selectionCursorColor = 0;
    Title_selectionCursorPosition = 0;
    Title_transferProgressCounter = 0;
    Title_fileMenuScreenFade = 0;
    Title_transferProgressTarget = 384;
    memset(Title_fileMenuElements, 0, sizeof Title_fileMenuElements);
}

// decomp: TITLE.PRG _initFileMenuElement (src/TITLE/TITLE.PRG/22C.c:1332)
Menu_Files_FileMenuElement* Title_SaveFiles_InitializeElement(int id, int xy, int wh, char* text)
{
    Menu_Files_FileMenuElement* element;
    int i;
    u_int c;

    element = &Title_fileMenuElements[id];
    memset(element, 0, sizeof *element);
    element->state = 1;
    element->slotId = -1;
    *(int*)&element->x = xy;
    *(int*)&element->w = wh;

    if (text != NULL) {
        for (i = 0; i < 32;) {
            c = *text++;
            if (c == Core_TextCode_Spacing) {
                element->text[i++] = c;
                do {
                    c = *(text++);
                } while (0);
            } else {
                if (c == Core_TextCode_Terminator) {
                    element->text[i] = Core_TextCode_PrintString;
                    return element;
                }
                if (c >= Core_TextCode_NonPrinting) {
                    continue;
                }
            }
            element->text[i++] = c;
        }
    } else {
        element->text[0] = Core_TextCode_PrintString;
    }
    return element;
}

// decomp: TITLE.PRG _clearFileMenuElement (src/TITLE/TITLE.PRG/22C.c:1370)
void Title_SaveFiles_ClearElement(int id)
{
    memset(&Title_fileMenuElements[id], 0, sizeof Title_fileMenuElements[id]);
}

// decomp: TITLE.PRG _fileMenuElementsActive (src/TITLE/TITLE.PRG/22C.c:1375)
int Title_SaveFiles_AreAnimationsComplete(void)
{
    int i;

    for (i = 0; i < 10 && Title_fileMenuElements[i].state < Menu_Files_ElementState_AnimateX; ++i)
        ;
    return i == 10;
}

// decomp: TITLE.PRG _printCharacter (src/TITLE/TITLE.PRG/22C.c:1384)
int Title_SaveFiles_DrawGlyph(u_int c, int x, int y, int clut)
{
    static int Title_kSaveMenuGlyphWidths[] = { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
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
        Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.sprt, Render_LegacyPrimitiveAddress_Null);
        Title_fileMenuPrimitiveBuffer.prim.sprt.texturePageCommand =
            vs_getTpage(832, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off);
        Title_fileMenuPrimitiveBuffer.prim.sprt.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_SpriteSemiTrans, 128, 128, 128);
        Title_fileMenuPrimitiveBuffer.prim.sprt.position = (u_short)x | y << 16;
        Title_fileMenuPrimitiveBuffer.prim.sprt.size = vs_getWH(12, 12);
        Title_fileMenuPrimitiveBuffer.prim.sprt.uvPalette =
            vs_getUV0Clut((c % 21) * 12, (c / 21) * 12, clut * 16 + 896, 222);
        DrawPrim(&Title_fileMenuPrimitiveBuffer);
    }
    return x + Title_kSaveMenuGlyphWidths[c];
}

// decomp: TITLE.PRG _memcardMaskedHandler (src/TITLE/TITLE.PRG/22C.c:1488)
int Title_SaveFiles_CheckCardStep(int portMask)
{
    static char Title_cardEventResultMask;
    static char Title_cardEventMaskPadding __attribute__((unused));

    int event;

    if (portMask != 0) {
        Title_SaveCard_AdvanceCardReadiness(portMask & 3);
        Title_cardEventResultMask = (portMask >> 4);
        return 0;
    }
    event = Title_SaveCard_AdvanceCardReadiness(0);
    if (event != Save_CardReadinessResult_Pending) {
        switch (event & Title_cardEventResultMask) {
        case Save_CardReadinessResult_Ready:
            return 1;
        case Save_CardReadinessResult_Timeout:
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_insertError);
            break;
        case Save_CardReadinessResult_Unformatted:
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_noData);
            break;
        default:
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_removed);
            break;
        }
        return -1;
    }
    return 0;
}

// decomp: TITLE.PRG _drawFileMenuElement (src/TITLE/TITLE.PRG/22C.c:1601)
void Title_SaveFiles_DrawElement(Menu_Files_FileMenuElement* element)
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
    u_short* locationClut;
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

        Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.tilePoly, Render_LegacyPrimitiveAddress_Null);
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.tileTpage.texturePageCommand =
            vs_getTpage(0, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_On);
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.tileTpage.colorCommand = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0);
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.tileTpage.position =
            ((element->x + 2) & 0xFFFF) | (element->y + 2) << 16;
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.tileTpage.size = element->w | (element->h << 0x10);
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.colorZeroAndCode = vs_getRGB0Raw(Render_LegacyPrimitiveCode_PolyG4, var1);
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionZero = (u_short)element->x | y;
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.colorOne = var2;
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionOne = ((element->x + element->w) & 0xFFFF) | y;
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.colorTwo = var1;
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionTwo =
            (u_short)element->x | ((element->y + element->h) << 0x10);
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.colorThree = var2;
        Title_fileMenuPrimitiveBuffer.prim.tilePoly.polyG4.positionThree =
            ((element->x + element->w) & 0xFFFF) | ((element->y + element->h) << 0x10);
        DrawPrim(&Title_fileMenuPrimitiveBuffer);

        x = element->x + 6;
        for (var1 = 0; var1 < 32; ++var1) {
            var2 = element->text[var1];
            if (var2 == Core_TextCode_Spacing) {
                x += element->text[++var1];
            } else if (var2 != 0xFF) {
                x = Title_SaveFiles_DrawGlyph(var2, x, element->y, 0);
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
                } else if (Title_isSaving != 0) {
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
                Title_SaveFiles_DrawImmediateSprite(xy, uvClut, vs_getWH(64, 32),
                    ((8 - element->outertextBlendFactor) << 19) | 0x9C);
            } else {
                int v0;
                uvClut = ((location & 8) * 8) | ((location & 7) << 0xD)
                       | vs_getUV0Clut(0, 0, 768, 227);
                xy = (element->x - 64) | y;
                new_var = (((location & 0x30) * 4) + 832) & 0x3FF;
                v0 = new_var >> 6;
                var_a3 = (((8 - element->outertextBlendFactor) << 19) | 0x90);
                Title_SaveFiles_DrawImmediateSprite(xy, uvClut, vs_getWH(64, 32), v0 | var_a3);
            }
            Render_UploadPackedRectangle(vs_getXY(768, 227), clut, vs_getWH(256, 1));
            Title_SaveFiles_DrawSummaryLabel((element->x - 22) | y, Menu_Files_FileMenuUiId_Number);
            Menu_Files_DrawInteger((element->x - 9) | y, var2 + 1, 0xAU);
            slotState = saveInfo->summary.base.slotState;
            if (slotState == Save_CardFileSlotStatus_Unavailable) {
                Menu_Files_DrawText((char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_inUse),
                    element->x + 6, element->y + 10, 3);
            } else if (slotState == Save_CardFileSlotStatus_Available) {
                if (Title_isSaving == 0) {
                    Menu_Files_DrawText((char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_empty),
                        element->x + 6, element->y + 10, 3);
                } else {
                    Menu_Files_DrawText((char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_new),
                        element->x + 6, element->y + 10, 0);
                }
            } else {
                y += 4 << 16;
                Menu_Files_DrawText(
                    (char*)&Title_saveMenuText[Title_saveMenuText[element->saveLocation
                                                  + Text_Menu_MCMAN_BIN_Index_saveLocations]],
                    element->x + 6, element->y + 4, 0);
                Title_SaveFiles_DrawSummaryLabel(y | 172, Menu_Files_FileMenuUiId_Map);
                Title_SaveFiles_DrawSummaryLabel(y | 189, Menu_Files_FileMenuUiId_Colon);
                var1 = saveInfo->summary.stats.mapCompletion;

                if (var1 == 100) {
                    Menu_Files_DrawInteger(y | 192, 100, 100);
                    Title_SaveFiles_DrawSummaryLabel((y + 0xFFFF0000) | 207, Menu_Files_FileMenuUiId_Percent);
                } else {
                    Menu_Files_DrawInteger(y | 192, var1, 10);
                    Title_SaveFiles_DrawSummaryLabel((y + 0xFFFF0000) | 202, Menu_Files_FileMenuUiId_Percent);
                }
                Title_SaveFiles_DrawSummaryLabel(y | 217, Menu_Files_FileMenuUiId_Save);
                Title_SaveFiles_DrawSummaryLabel(y | 239, Menu_Files_FileMenuUiId_Colon);
                Menu_Files_DrawInteger(y | 242, saveInfo->summary.stats.cumulativeSaveCount, 1000);
                Title_SaveFiles_DrawSummaryLabel(y | 267, Menu_Files_FileMenuUiId_Clear);
                Title_SaveFiles_DrawSummaryLabel(y | 293, Menu_Files_FileMenuUiId_Colon);
                Menu_Files_DrawInteger(y | 296, saveInfo->summary.stats.clearCount, 10);
                y += 13 << 16;
                if (saveInfo->summary.stats.clearCount != 0) {
                    Title_SaveFiles_DrawImmediateSprite(y | 69, vs_getUV0Clut(240, 16, 912, 223), vs_getWH(16, 16),
                        getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, 768, 0));
                }
                Title_SaveFiles_DrawSummaryLabel(y | 240, Menu_Files_FileMenuUiId_Time);
                var1 = saveInfo->summary.stats.gameTime.t.hours;
                if (var1 == 100) {
                    Menu_Files_DrawInteger(y | 263, var1, 100);
                } else {
                    Menu_Files_DrawInteger(y | 268, var1, 10);
                }
                Title_SaveFiles_DrawSummaryLabel(y | 279, Menu_Files_FileMenuUiId_Colon);
                Menu_Files_DrawInteger(y | 282, saveInfo->summary.stats.gameTime.t.minutes, 10);
                Title_SaveFiles_DrawSummaryLabel(y | 293, Menu_Files_FileMenuUiId_Colon);
                Menu_Files_DrawInteger(y | 296, saveInfo->summary.stats.gameTime.t.seconds, 10);
                Title_SaveFiles_DrawHpMpSummary(y | 88, Menu_Files_StatType_HP, saveInfo->summary.stats.currentHP,
                    saveInfo->summary.stats.maxHP);
                Title_SaveFiles_DrawHpMpSummary(y | 158, Menu_Files_StatType_MP, saveInfo->summary.stats.currentMP,
                    saveInfo->summary.stats.maxMP);
                y -= 17 << 16;
            }
            if ((element->selected != 0) && (Title_transferProgressCounter != 0)) {
                if (Title_transferProgressCounter < 0) {
                    signed char* p = (Title_kFileOperationCursorSaturation + Title_transferProgressCounter++);
                    Title_SaveFiles_DrawTransferComplete(-p[17], y);
                } else {
                    int new_var3 = 0x140;
                    Title_SaveFiles_DrawTransferProgress(
                        Title_transferProgressPosition
                            + (((Title_transferProgressCounter - Title_previousTransferProgress)
                                   * ((Title_saveTransferErrorOffset * 0x14)
                                       - (Title_transferProgressPosition - new_var3)))
                                / Title_transferProgressTarget),
                        y);
                }
            }
        }
    }
}

// decomp: TITLE.PRG _drawFileMenu (src/TITLE/TITLE.PRG/22C.c:1784)
void Title_SaveFiles_DrawFrame(int framebuf)
{
    int _[2];
    Menu_Files_FileMenuElement* element;
    int j;
    int i;
    u_int x;
    int state;
    int new_var;

    element = Title_fileMenuElements;
    x = 0;
    if (framebuf != 0) {
        x = 320;
    }
    Title_SaveFiles_DrawImmediateSprite(vs_getXY(256, 176), vs_getUV0Clut(0, 176, 768, 227), vs_getWH(64, 64),
        getTPage(Render_TextureFormat_Palette8Bit, Render_BlendMode_Average, 768, 256));
    Title_SaveFiles_DrawImmediateSprite(vs_getXY(0, 176), vs_getUV0Clut(0, 176, 768, 227), vs_getWH(256, 64),
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
                    if ((element->targetPosition + Title_kFileMenuAnimationStops[j]) >= element->x) {
                        break;
                    }
                }
                element->x = element->targetPosition + Title_kFileMenuAnimationStops[j - 1];
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
                    if ((element->targetPosition + Title_kFileMenuAnimationStops[j]) >= element->y) {
                        break;
                    }
                }
                element->y = (element->targetPosition + Title_kFileMenuAnimationStops[j - 1]);
            }
            if (element->y == element->targetPosition) {
                element->state = Menu_Files_ElementState_Static;
            }
        }
        if (state == Menu_Files_ElementState_AnimateNegX) {
            if (element->x < element->targetPosition) {
                for (j = 1; j < 16; ++j) {
                    if (element->x >= -Title_kFileMenuAnimationStops[j]) {
                        break;
                    }
                }
                element->x = -Title_kFileMenuAnimationStops[j - 1];
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
            Title_SaveFiles_DrawElement(element);
        }
    }

    for (i = 1; i < 4; ++i) {
        new_var = 256;
        Title_SaveFiles_DrawImmediateSprite(vs_getXY(0, 187 - i), (187 - i) << 8, vs_getWH(256, 1),
            (x >> 6) | (((4 - i) << 21) | new_var));
        Title_SaveFiles_DrawImmediateSprite(vs_getXY(256, 187 - i), (187 - i) << 8, vs_getWH(64, 1),
            (((int)(x + new_var)) >> 6) | (((4 - i) << 21) | new_var));
    }
    Title_SaveFiles_DrawImmediateSprite(vs_getXY(10, 181), vs_getUV0Clut(64, 145, 848, 223), vs_getWH(33, 7),
        getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, 768, 0)); // "Caution"
    DrawSync(0);
    Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.tile, Render_LegacyPrimitiveAddress_Null);
    Title_fileMenuPrimitiveBuffer.prim.tile.colorAndCode = vs_getRGB0(Render_LegacyPrimitiveCode_Tile, 0, 0, 0);
    Title_fileMenuPrimitiveBuffer.prim.tile.position = vs_getXY(0, 187);
    Title_fileMenuPrimitiveBuffer.prim.tile.size = vs_getWH(320, 52);
    DrawPrim(&Title_fileMenuPrimitiveBuffer); // Message dialog background
    if (Title_selectionCursorPosition != 0) {
        Title_SaveFiles_DrawImmediateSprite(Title_selectionCursorPosition, vs_getUV0Clut(32, 48, 896, 223), vs_getWH(16, 16),
            (Title_kFileOperationCursorSaturation[Title_selectionCursorColor] << 16)
                | getTPage(Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, 768, 0));
        Title_selectionCursorColor = (Title_selectionCursorColor + 1) & 0xF;
        if (Input_buttonsPressed.allButtons & PADRup) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_INVALID);
        }
    } else {
        Title_selectionCursorColor = 0;
    }
    if (Title_memoryCardMessage != NULL) {
        Menu_Files_DrawText(Title_memoryCardMessage, 16, 192, 0);
    }
    if (Title_fileMenuScreenFade != 0) {
        Title_fileMenuPrimitiveBuffer.tag = vs_getTag(Title_fileMenuPrimitiveBuffer.prim.tileTpage, Render_LegacyPrimitiveAddress_Null);
        Title_fileMenuPrimitiveBuffer.prim.tileTpage.texturePageCommand =
            vs_getTpage(0, 0, Render_TextureFormat_Direct16Bit, Render_BlendMode_Subtract, Render_Dithering_Off);
        Title_fileMenuPrimitiveBuffer.prim.tileTpage.position = vs_getXY(0, 0);
        Title_fileMenuPrimitiveBuffer.prim.tileTpage.size = vs_getWH(320, 224);
        Title_fileMenuPrimitiveBuffer.prim.tileTpage.colorCommand =
            vs_getRGB0Raw(Render_LegacyPrimitiveCode_TileSemiTrans, Title_fileMenuScreenFade * vs_getRGB888(8, 8, 8));
        DrawPrim(&Title_fileMenuPrimitiveBuffer);
    }
}

// decomp: TITLE.PRG _showLoadFilesMenu (src/TITLE/TITLE.PRG/22C.c:1916)
int Title_SaveFiles_UpdateLoadFiles(int initPort)
{
    enum Menu_Files_LoadFilesState {
        Menu_Files_LoadFilesState_Init = 0,
        Menu_Files_LoadFilesState_HandleInput = 1,
        Menu_Files_LoadFilesState_StartLoad = 2,
        Menu_Files_LoadFilesState_ApplyLoad = 3,
        Menu_Files_LoadFilesState_Loaded = 4,
        Menu_Files_LoadFilesState_Exiting = 5
    };

    static State_PlayTime Title_loadMenuGameTimeBackup;
    static int Title_hasLoadedSaveFile;
    static char Title_loadMenuPhase;
    static char Title_loadMenuCardSlot;
    static char Title_loadMenuPageIndex;
    static char Title_loadMenuSelectedSlot;
    static char Title_loadMenuExitTimerFrames;
    static char Title_loadCompleteTimerFrames;

    Menu_Files_FileMenuElement* element;
    int currentSlot;
    int i;

    if (initPort != 0) {
        Title_loadMenuCardSlot = initPort;
        Title_loadMenuSelectedSlot = Title_SaveCard_NewestSaveSlot();
        Title_loadMenuPageIndex = 0;
        if (Title_loadMenuSelectedSlot == 4) {
            Title_loadMenuPageIndex = 2;
            Title_loadMenuSelectedSlot = 2;
        } else if (Title_loadMenuSelectedSlot != 0) {
            Title_loadMenuPageIndex = Title_loadMenuSelectedSlot - 1;
            Title_loadMenuSelectedSlot = 1;
        }
        Title_hasLoadedSaveFile = -1;
        Title_loadCompleteTimerFrames = 0;
        Title_loadMenuPhase = Menu_Files_LoadFilesState_Init;
        return 0;
    }
    switch (Title_loadMenuPhase) {
    case Menu_Files_LoadFilesState_Init:
        for (i = 0; i < 5; ++i) {
            element = Title_SaveFiles_InitializeElement(
                i + 5, ((72 + i * 40) << 16) | 64, vs_getWH(256, 32), 0);
            element->slotId = i;
            element->slotUnavailable =
                Title_saveFileSummaries[i].summary.base.slotState < Save_CardFileSlotStatus_InUse;
            element->saveLocation = Title_saveFileSummaries[i].summary.stats.saveLocation;
        }
        Title_loadMenuPhase = Menu_Files_LoadFilesState_HandleInput;
        Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_selectFile);
        // fallthrough
    case Menu_Files_LoadFilesState_HandleInput:
        if (Input_buttonsPressed.allButtons & PADRdown) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            for (i = 5; i < 10; ++i) {
                Title_SaveFiles_ClearElement(i);
            }
            Title_selectionCursorPosition = 0;
            return -1;
        }
        for (i = 0; i < 5; ++i) {
            Title_fileMenuElements[i + 5].y = (((i - Title_loadMenuPageIndex) * 40) + 72);
            Title_fileMenuElements[i + 5].selected = 0;
        }

        currentSlot = Title_loadMenuSelectedSlot + Title_loadMenuPageIndex;

        if (Input_buttonsPressed.allButtons & PADRright) {
            if (Title_saveFileSummaries[currentSlot].summary.base.slotState >= Save_CardFileSlotStatus_InUse) {
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
                Title_fileMenuElements[currentSlot + 5].selected = 1;
                Title_selectionCursorPosition = 0;
                Title_SaveFiles_CheckCardStep(Title_loadMenuCardSlot + Save_EventMask_All);
                Title_loadMenuPhase = Menu_Files_LoadFilesState_StartLoad;
                break;
            }
            Audio_PlaySfxCentered(0x7E, VS_SFX_INVALID);
        }
        if (Input_buttonRepeat & PADLup) {
            if (Title_loadMenuSelectedSlot == 0) {
                if (Title_loadMenuPageIndex != 0) {
                    --Title_loadMenuPageIndex;
                }
            } else {
                --Title_loadMenuSelectedSlot;
            }
        }
        if (Input_buttonRepeat & PADLdown) {
            if (Title_loadMenuSelectedSlot == 2) {
                if (Title_loadMenuPageIndex < 2) {
                    ++Title_loadMenuPageIndex;
                }
            } else {
                ++Title_loadMenuSelectedSlot;
            }
        }
        if (currentSlot != (Title_loadMenuSelectedSlot + Title_loadMenuPageIndex)) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
        }
        Title_selectionCursorPosition = ((Title_loadMenuSelectedSlot * 40 + 62) << 16) | 24;
        Title_fileMenuElements[currentSlot + 5].selected = 1;
        break;
    case Menu_Files_LoadFilesState_StartLoad:
        currentSlot = Title_SaveFiles_CheckCardStep(0);
        if (currentSlot != Title_CardPollResult_Pending) {
            if (currentSlot >= Title_CardPollResult_Pending) {
                int new_var;
                Title_SaveCard_ReadSaveStep(
                    ((Title_loadMenuSelectedSlot + Title_loadMenuPageIndex) + 1) | (new_var = ((Title_loadMenuCardSlot - 1) << 16) | 256));
                Title_loadMenuPhase = Menu_Files_LoadFilesState_ApplyLoad;
                Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_loading);
            } else {
                Title_loadMenuPhase = Menu_Files_LoadFilesState_Loaded;
            }
        }
        break;
    case Menu_Files_LoadFilesState_ApplyLoad:
        currentSlot = Title_SaveCard_ReadSaveStep(0);
        ++Title_transferProgressCounter;
        if (currentSlot != 0) {
            Title_transferProgressCounter = 0;
            do {
                Title_loadMenuExitTimerFrames = 0;
                if (currentSlot < 0) {
                    Title_memoryCardMessage =
                        (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_loadfailed);
                    break;
                }
                switch (Title_SaveCard_ValidateAndApplyLoadedSave(1)) {
                case 0:
                    Title_transferProgressCounter = -16;
                    Audio_PlaySfxCentered(0x7E, VS_SFX_FILEOPCOMPLETE);
                    Title_loadMenuExitTimerFrames = 16;
                    Title_hasLoadedSaveFile = 1;
                    Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_loaded);
                    break;
                case 1:
                    Title_memoryCardMessage =
                        (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_fileCorruptDescription);
                    break;
                }
            } while (0);

            Title_loadMenuGameTimeBackup.t = State_playTime.t;
            Title_loadMenuPhase = Menu_Files_LoadFilesState_Loaded;
        }
        break;
    case Menu_Files_LoadFilesState_Loaded:
        if (Title_loadMenuExitTimerFrames != 0) {
            --Title_loadMenuExitTimerFrames;
        }
        if (Title_hasLoadedSaveFile == 1) {
            ++Title_loadCompleteTimerFrames;
        }
        if ((Input_buttonsPressed.ports[0].lowByte != 0) || (Title_loadCompleteTimerFrames == 150)) {
            if (Title_hasLoadedSaveFile < 0) {
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            }
            Title_loadMenuPhase = Menu_Files_LoadFilesState_Exiting;
        }
        break;
    case Menu_Files_LoadFilesState_Exiting:
        if (Title_loadMenuExitTimerFrames != 0) {
            --Title_loadMenuExitTimerFrames;
            break;
        }
        if (Title_hasLoadedSaveFile == 1) {
            State_playTime.t = Title_loadMenuGameTimeBackup.t;
        } else {
            for (i = 5; i < 10; ++i) {
                Title_SaveFiles_ClearElement(i);
            }
        }
        return Title_hasLoadedSaveFile;
    }
    return 0;
}

// decomp: TITLE.PRG _selectLoadMemoryCard (src/TITLE/TITLE.PRG/22C.c:2096)
long Title_SaveFiles_UpdateLoadCard(int initPort)
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

    static char Title_loadCardSelectionPhase;
    static u_char Title_loadCardSelection;

    int i;

    if (initPort != 0) {
        Title_loadCardSelection = initPort;
        Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_checking);
        Title_loadCardSelectionPhase = Menu_Files_LoadCardState_Init;
        return 0;
    }
    switch (Title_loadCardSelectionPhase) {
    case Menu_Files_LoadCardState_Init:
        Title_fileMenuElements[Title_loadCardSelection].state = Menu_Files_ElementState_AnimateX;
        Title_fileMenuElements[Title_loadCardSelection].targetPosition = 180;
        Title_fileMenuElements[Title_loadCardSelection].innertextBlendFactor = 1;
        Title_fileMenuElements[3 - Title_loadCardSelection].state = Menu_Files_ElementState_AnimateX;
        Title_fileMenuElements[3 - Title_loadCardSelection].targetPosition = 320;
        Title_loadCardSelectionPhase = Menu_Files_LoadCardState_WaitForUnselectedAnimation;
        break;

    case Menu_Files_LoadCardState_WaitForUnselectedAnimation:
        if (Title_SaveFiles_AreAnimationsComplete() == 0) {
            break;
        }
        if (Title_loadCardSelection == 2) {
            Title_fileMenuElements[2].state = Menu_Files_ElementState_AnimateY;
            Title_fileMenuElements[2].targetPosition = 50;
        }
        Title_loadCardSelectionPhase = Menu_Files_LoadCardState_PollEventsInit;
        break;

    case Menu_Files_LoadCardState_PollEventsInit:
        if (Title_SaveFiles_AreAnimationsComplete() == 0) {
            break;
        }
        Title_SaveFiles_CheckCardStep(Title_loadCardSelection + Save_EventMask_IgnoreNoEvent);
        Title_loadCardSelectionPhase = Menu_Files_LoadCardState_PollEvents;
        break;

    case Menu_Files_LoadCardState_PollEvents:
        i = Title_SaveFiles_CheckCardStep(0);
        if (i != Title_CardPollResult_Pending) {
            Title_loadCardSelectionPhase = i + Menu_Files_LoadCardState_Leave + 1;
        }
        break;

    case Menu_Files_LoadCardState_Leave:
        if ((Input_buttonsPressed.ports[0].lowByte) == 0) {
            break;
        }
        Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
        for (i = 1; i < 3; ++i) {
            Title_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
            Title_fileMenuElements[i].targetPosition = 320;
        }
        Title_loadCardSelectionPhase = Menu_Files_LoadCardState_ReturnNotSelected;
        break;

    case Menu_Files_LoadCardState_ReturnNotSelected:
        if (Title_SaveFiles_AreAnimationsComplete() != 0) {
            return -1;
        }
        break;

    case Menu_Files_LoadCardState_PollSuccess:
        if (Title_SaveCard_ScanSaveDirectory(Title_loadCardSelection) != 0) {
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_loadfailed);
            Title_loadCardSelectionPhase = Menu_Files_LoadCardState_Leave;
            break;
        }
        for (i = 0; i < 5; ++i) {
            if (Title_saveFileSummaries[i].summary.base.slotState > Save_CardFileSlotStatus_Available) {
                break;
            }
        }

        if (i == 5) {
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_noData);
            Title_loadCardSelectionPhase = Menu_Files_LoadCardState_Leave;
        } else {
            Title_SaveFiles_UpdateLoadFiles(Title_loadCardSelection);
            Title_loadCardSelectionPhase = 7;
        }
        break;

    case Menu_Files_LoadCardState_SelectFile:
        i = Title_SaveFiles_UpdateLoadFiles(0);
        if (i == 0) {
            break;
        }
        if (i < 0) {
            for (i = 1; i < 3; ++i) {
                Title_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
                Title_fileMenuElements[i].targetPosition = 320;
            }
            Title_loadCardSelectionPhase = Menu_Files_LoadCardState_ReturnNotSelected;
            break;
        }
        return 1;
    }
    return 0;
}

// decomp: TITLE.PRG _promptConfirm (src/TITLE/TITLE.PRG/22C.c:2330)
int Title_SaveFiles_UpdateConfirmation(int arg0)
{
    enum Menu_Files_ConfirmationState {
        Menu_Files_ConfirmationState_InitYes = 0,
        Menu_Files_ConfirmationState_InitNo = 1,
        Menu_Files_ConfirmationState_WaitForAnimation = 2,
        Menu_Files_ConfirmationState_HandleInput = 3,
    };

    static char Title_confirmationPhase;
    static char Title_confirmationWasCancelled;
    static char Title_confirmationPadding[2] __attribute__((unused));

    Menu_Files_FileMenuElement* temp_v0;
    int i;

    if (arg0 != 0) {
        Title_confirmationWasCancelled = 1;
        Title_confirmationPhase = Menu_Files_ConfirmationState_InitYes;
        return 0;
    }

    switch (Title_confirmationPhase) {
    case Menu_Files_ConfirmationState_InitYes:
    case Menu_Files_ConfirmationState_InitNo:
        temp_v0 = Title_SaveFiles_InitializeElement(Title_confirmationPhase + 3,
            vs_getXY(-126 & 0xFFFF, Title_confirmationPhase * 16 + 18), vs_getWH(126, 12),
            (char*)&Title_saveMenuText[Title_saveMenuText[Title_confirmationPhase + Text_Menu_MCMAN_BIN_Index_yesIndent]]);
        temp_v0->state = 4;
        temp_v0->targetPosition = 0;
        ++Title_confirmationPhase;
        break;
    case Menu_Files_ConfirmationState_WaitForAnimation:
        Title_confirmationPhase += Title_SaveFiles_AreAnimationsComplete();
        break;
    case Menu_Files_ConfirmationState_HandleInput:
        Title_fileMenuElements[Title_confirmationWasCancelled + 3].selected = 1;
        Title_fileMenuElements[4 - Title_confirmationWasCancelled].selected = 0;
        if (Input_buttonsPressed.allButtons & (PADRdown | PADRright)) {
            Title_selectionCursorPosition = 0;
            for (i = 3; i < 5; ++i) {
                Title_fileMenuElements[i].state = 4;
                Title_fileMenuElements[i].targetPosition = -126;
            }
            if (Input_buttonsPressed.allButtons & PADRright) {
                if (Title_confirmationWasCancelled == 0) {
                    Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
                    return 1;
                }
            }
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            return -1;
        }
        if (Input_buttonRepeat & (PADLup | PADLdown)) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            Title_confirmationWasCancelled = 1 - Title_confirmationWasCancelled;
        }
        Title_selectionCursorPosition = (Title_confirmationWasCancelled * 16 + 10) << 16;
        break;
    }
    return 0;
}

// decomp: TITLE.PRG _promptOverwrite (src/TITLE/TITLE.PRG/22C.c:2393)
int Title_SaveFiles_UpdateOverwritePrompt(int init)
{
    static int Title_overwriteWasConfirmed;
    static char Title_overwritePromptPhase;
    static char Title_isOverwritePromptInitialized;

    if (init != 0) {
        Title_isOverwritePromptInitialized = 1;
        Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_overwritePrompt);
        Title_SaveFiles_UpdateConfirmation(1);
        Title_overwritePromptPhase = 0;
        return 0;
    }
    switch (Title_overwritePromptPhase) {
    case 0:
        Title_overwriteWasConfirmed = Title_SaveFiles_UpdateConfirmation(0);
        if (Title_overwriteWasConfirmed != 0) {
            Title_overwritePromptPhase = 1;
        }
        return 0;
    case 1:
        if (Title_SaveFiles_AreAnimationsComplete() != 0) {
            return Title_overwriteWasConfirmed;
        }
        return 0;
    default:
        return 0;
    }
}

// decomp: TITLE.PRG _promptFormat (src/TITLE/TITLE.PRG/22C.c:2423)
int Title_SaveFiles_UpdateFormatPrompt(int initPort)
{
    enum Menu_Files_FormatPromptState { Menu_Files_FormatPromptState_PromptConfirm = 0, Menu_Files_FormatPromptState_InitEvents = 1, Menu_Files_FormatPromptState_HandleEvents = 2, Menu_Files_FormatPromptState_Format = 3 };

    static char Title_formatPromptPhase;
    static char Title_formatPromptCardSlot;
    static int Title_formatPromptPadding __attribute__((unused));

    int val;

    if (initPort != 0) {
        Title_formatPromptCardSlot = initPort;
        Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_formatPrompt);
        Title_SaveFiles_UpdateConfirmation(1);
        Title_formatPromptPhase = Menu_Files_FormatPromptState_PromptConfirm;
        return 0;
    }

    switch (Title_formatPromptPhase) {
    case Menu_Files_FormatPromptState_PromptConfirm:
        val = Title_SaveFiles_UpdateConfirmation(0);

        if (val != 0) {
            if (val < 0) {
                Title_memoryCardMessage =
                    (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_formatCancelled);
                return -1;
            }

            Title_formatPromptPhase = Menu_Files_FormatPromptState_InitEvents;
        }
        return 0;
    case Menu_Files_FormatPromptState_InitEvents:
        if (Title_SaveFiles_AreAnimationsComplete() != 0) {
            Title_SaveCard_AdvanceCardReadiness(Title_formatPromptCardSlot);
            Title_formatPromptPhase = Menu_Files_FormatPromptState_HandleEvents;
        }
        return 0;
    case Menu_Files_FormatPromptState_HandleEvents:
        val = Title_SaveCard_AdvanceCardReadiness(0);

        if (val != 0) {
            if (val == Save_CardReadinessResult_Unformatted) {
                Title_formatPromptPhase = Menu_Files_FormatPromptState_Format;
                Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_formatting);
            } else {
                if (val == Save_CardReadinessResult_Timeout) {
                    Title_memoryCardMessage =
                        (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_insertError);
                } else {
                    Title_memoryCardMessage =
                        (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_removed);
                }
                return -1;
            }
        }
        return 0;
    case Menu_Files_FormatPromptState_Format:
        if (_card_format((Title_formatPromptCardSlot - 1) * 16) == 0) {
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_formatFailed);
            return -1;
        }
        return 1;
    default:
        return 0;
    }
}

// decomp: TITLE.PRG _showSaveFilesMenu (src/TITLE/TITLE.PRG/22C.c:2495)
int Title_SaveFiles_UpdateSaveFiles(int initPort)
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

    static State_Settings Title_saveMenuSettingsBackup;
    static int Title_wasSaveSuccessful;
    static char Title_saveMenuPhase;
    static char Title_saveMenuCardSlot;
    static char Title_saveMenuPageIndex;
    static char Title_saveMenuFileSlot;
    static char Title_saveMenuExitTimerFrames;

    Menu_Files_FileMenuElement* element;
    int val;
    int saveId;
    int i;

    if (initPort != 0) {
        Title_saveMenuCardSlot = initPort;
        Title_saveMenuFileSlot = Title_SaveCard_FindMatchingSaveSlot();
        if (Title_saveMenuFileSlot) {
            --Title_saveMenuFileSlot;
        }
        Title_saveMenuPageIndex = 0;
        if (Title_saveMenuFileSlot == 4) {
            Title_saveMenuPageIndex = 2;
            Title_saveMenuFileSlot = 2;
        } else if (Title_saveMenuFileSlot != 0) {
            Title_saveMenuPageIndex = Title_saveMenuFileSlot - 1;
            Title_saveMenuFileSlot = 1;
        }
        Title_wasSaveSuccessful = -1;
        Title_saveMenuExitTimerFrames = 0;
        Title_saveMenuPhase = Menu_Files_SaveFilesState_Init;
        return 0;
    }
    switch (Title_saveMenuPhase) {
    case Menu_Files_SaveFilesState_Init:
        for (i = 0; i < 5; ++i) {
            element = Title_SaveFiles_InitializeElement(
                i + 5, vs_getXY(64, i * 40 + 72), vs_getWH(256, 32), 0);
            element->slotId = i;
            element->slotUnavailable =
                Title_saveFileSummaries[i].summary.base.slotState == Save_CardFileSlotStatus_Unavailable;
            element->saveLocation = Title_saveFileSummaries[i].summary.stats.saveLocation;
        }
        Title_saveMenuPhase = Menu_Files_SaveFilesState_HandleInput;
        /* fallthrough */
    case Menu_Files_SaveFilesState_HandleInput:
        Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_selectFile);
        if (Input_buttonsPressed.allButtons & PADRdown) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            Title_selectionCursorPosition = 0;
            Title_saveMenuPhase = Menu_Files_SaveFilesState_Leave;
        } else {
            for (i = 0; i < 5; ++i) {
                Title_fileMenuElements[i + 5].y = ((i - Title_saveMenuPageIndex) * 40) + 72;
                Title_fileMenuElements[i + 5].selected = 0;
            }
            val = Title_saveMenuFileSlot + Title_saveMenuPageIndex;
            if (Input_buttonsPressed.allButtons & PADRright) {
                if (Title_saveFileSummaries[val].summary.base.slotState != Save_CardFileSlotStatus_Unavailable) {
                    Audio_PlaySfxCentered(0x7E, VS_SFX_MENUSELECT);
                    Title_fileMenuElements[val + 5].selected = 1;
                    Title_selectionCursorPosition = 0;
                    Title_SaveFiles_CheckCardStep(Title_saveMenuCardSlot + Save_EventMask_All);
                    Title_saveMenuPhase = Menu_Files_SaveFilesState_SlotSelected;
                    break;
                } else {
                    Audio_PlaySfxCentered(0x7E, VS_SFX_INVALID);
                }
            }
            if (Input_buttonRepeat & PADLup) {
                if (Title_saveMenuFileSlot == 0) {
                    if (Title_saveMenuPageIndex != 0) {
                        --Title_saveMenuPageIndex;
                    }
                } else {
                    --Title_saveMenuFileSlot;
                }
            }
            if (Input_buttonRepeat & PADLdown) {
                if (Title_saveMenuFileSlot == 2) {
                    if (Title_saveMenuPageIndex < 2) {
                        ++Title_saveMenuPageIndex;
                    }
                } else {
                    ++Title_saveMenuFileSlot;
                }
            }
            if (val != (Title_saveMenuFileSlot + Title_saveMenuPageIndex)) {
                Audio_PlaySfxCentered(0x7E, VS_SFX_MENUCHANGE);
            }
            Title_selectionCursorPosition = vs_getXY(24, Title_saveMenuFileSlot * 40 + 62);
            Title_fileMenuElements[val + 5].selected = 1;
        }
        break;
    case Menu_Files_SaveFilesState_SlotSelected:
        val = Title_SaveFiles_CheckCardStep(0);
        if (val != Title_CardPollResult_Pending) {
            if (Title_memoryCardMessage == (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_noData)) {
                Title_SaveFiles_UpdateFormatPrompt(Title_saveMenuCardSlot);
                Title_saveMenuPhase = Menu_Files_SaveFilesState_Format;
            } else if (val > Title_CardPollResult_Error) {
                val = Title_saveMenuFileSlot + Title_saveMenuPageIndex;
                if (Title_saveFileSummaries[val].summary.base.slotState == Save_CardFileSlotStatus_Available) {
                    if (Save_MemoryCard_CreateSaveFile(Title_saveMenuCardSlot, val + 1) != 0) {
                        Title_saveMenuPhase = Menu_Files_SaveFilesState_LeaveWithTimer;
                        Title_memoryCardMessage =
                            (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_saveFailed);
                    } else {
                        Title_saveMenuPhase = Menu_Files_SaveFilesState_Save;
                    }
                } else {
                    Title_SaveFiles_UpdateOverwritePrompt(1);
                    Title_saveMenuPhase = Menu_Files_SaveFilesState_OverwritePrompt;
                }
            } else {
                Title_saveMenuPhase = Menu_Files_SaveFilesState_LeaveWithTimer;
            }
        }
        break;
    case Menu_Files_SaveFilesState_OverwritePrompt:
        val = Title_SaveFiles_UpdateOverwritePrompt(0);
        if (val != 0) {
            if (val == 1) {
                Title_SaveFiles_CheckCardStep(Title_saveMenuCardSlot + Save_EventMask_All);
                Title_saveMenuPhase = Menu_Files_SaveFilesState_InitOverwrite;
            } else {
                Title_saveMenuPhase = Menu_Files_SaveFilesState_HandleInput;
            }
        }
        break;
    case Menu_Files_SaveFilesState_InitOverwrite:
        val = Title_SaveFiles_CheckCardStep(0);
        if (val != Title_CardPollResult_Pending) {
            if (val > Title_CardPollResult_Error) {
                Title_hasUnsavedData |=
                    (Title_SaveCard_FindMatchingSaveSlot() == (Title_saveMenuFileSlot + Title_saveMenuPageIndex + 1));
                Title_saveMenuPhase = Menu_Files_SaveFilesState_Save;
            } else {
                Title_saveMenuPhase = Menu_Files_SaveFilesState_LeaveWithTimer;
            }
        }
        break;
    case Menu_Files_SaveFilesState_Save:
        val = Title_saveMenuFileSlot + Title_saveMenuPageIndex;
        Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_saving);
        Core_Memory_CopyBytesBackward(&Title_saveMenuSettingsBackup, &Input_settings, sizeof Title_saveMenuSettingsBackup);
        Core_Memory_CopyBytesBackward(
            (Save_GameSaveImage*)Title_memoryCardUiResource + 2, (Save_GameSaveImage*)Title_memoryCardUiResource + 1, sizeof(Save_GameSaveImage));
        if (Title_isContainerDataEmpty != 0) {
            Title_savedMainMenuSetting = (*(u_int*)&Input_settings >> 4) & 1;
            *(int*)&Input_settings |= 0x10;
        }
        Save_MemoryCard_PackageClearSave(val);
        Title_SaveCard_WriteSaveStep((val + 1) | ((Title_saveMenuCardSlot - 1) << 0x10));
        Title_saveMenuPhase = Menu_Files_SaveFilesState_Validate;
        break;
    case Menu_Files_SaveFilesState_Validate:
        val = Title_SaveCard_WriteSaveStep(0);
        ++Title_transferProgressCounter;
        if (val != 0) {
            Audio_Seq_unusedStateFlags.doorEntered = 0;
            saveId = Title_saveMenuFileSlot + Title_saveMenuPageIndex;
            if (val < 0) {
                if (Title_isContainerDataEmpty != 0) {
                    char v = Title_savedMainMenuSetting & 1;
                    *(int*)&Input_settings =
                        (*(int*)&Input_settings & ~0x10) | (v * 0x10);
                }
                memset(&Title_saveFileSummaries[saveId], 0, sizeof Title_saveFileSummaries[saveId]);
                Title_saveFileSummaries[saveId].summary.base.slotState = Save_CardFileSlotStatus_Temporary;
                Title_fileMenuElements[saveId + 5].saveLocation = 0;
                Core_Memory_CopyBytesBackward((Save_GameSaveImage*)Title_memoryCardUiResource + 1, (Save_GameSaveImage*)Title_memoryCardUiResource + 2,
                    sizeof(Save_GameSaveImage));
                Title_transferProgressCounter = 0;
                Core_Memory_CopyBytesBackward(&Input_settings, &Title_saveMenuSettingsBackup, sizeof Input_settings);
                Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_saveFailed);
            } else {
                Title_hasUnsavedData = 0;
                Title_transferProgressCounter = -16;
                Core_Memory_CopyBytesBackward(&Title_saveFileSummaries[saveId],
                    Title_memoryCardUiResource + sizeof Title_saveFileSummaries[saveId] * 3,
                    sizeof Title_saveFileSummaries[saveId]);
                Save_MemoryCard_DecodeSaveBytes(Title_saveFileSummaries[saveId].key,
                    &Title_saveFileSummaries[saveId].summary.base.slotState,
                    sizeof Title_saveFileSummaries[saveId] - sizeof(int));
                Title_fileMenuElements[saveId + 5].saveLocation =
                    Title_saveFileSummaries[saveId].summary.stats.saveLocation;
                Audio_PlaySfxCentered(0x7E, VS_SFX_FILEOPCOMPLETE);
                Title_wasSaveSuccessful = 1;
                Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_saved);
            }
            Title_saveMenuPhase = Menu_Files_SaveFilesState_LeaveWithTimer;
        }
        break;
    case Menu_Files_SaveFilesState_LeaveWithTimer:
        if (Title_wasSaveSuccessful == 1) {
            ++Title_saveMenuExitTimerFrames;
        }
        if (Input_buttonsPressed.ports[0].lowByte == 0) {
            if (Title_saveMenuExitTimerFrames != 150) {
                break;
            }
        }
        Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
        Title_saveMenuPhase = Menu_Files_SaveFilesState_Leave;
        break;
    case Menu_Files_SaveFilesState_Leave:
        if (Title_transferProgressCounter == 0) {
            for (i = 5; i < 10; ++i) {
                Title_SaveFiles_ClearElement(i);
            }
            return Title_wasSaveSuccessful;
        }
        break;
    case Menu_Files_SaveFilesState_Format:
        val = Title_SaveFiles_UpdateFormatPrompt(0);
        if (val != 0) {
            if (val < 0) {
                Title_saveMenuPhase = Menu_Files_SaveFilesState_LeaveError;
            } else if (Save_MemoryCard_CreateSaveFile(Title_saveMenuCardSlot, Title_saveMenuPageIndex + Title_saveMenuFileSlot + 1) != 0) {
                Title_saveMenuPhase = Menu_Files_SaveFilesState_LeaveWithTimer;
                Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_saveFailed);
            } else {
                Title_saveMenuPhase = Menu_Files_SaveFilesState_Save;
            }
        }
        break;
    case Menu_Files_SaveFilesState_LeaveError:
        if (Input_buttonsPressed.ports[0].lowByte != 0) {
            Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
            for (i = 5; i < 10; ++i) {
                Title_SaveFiles_ClearElement(i);
            }
            return -1;
        }
        break;
    }
    return 0;
}

// decomp: TITLE.PRG _selectSaveMemoryCard (src/TITLE/TITLE.PRG/22C.c:2750)
int Title_SaveFiles_UpdateSaveCard(int initPort)
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

    static char Title_saveCardSelectionPhase;
    static u_char Title_saveCardSelection;

    int i, j;

    if (initPort != 0) {
        Title_saveCardSelection = initPort;
        Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_checking);
        Title_saveCardSelectionPhase = Menu_Files_SaveCardState_Init;
        return 0;
    }

    switch (Title_saveCardSelectionPhase) {
    case Menu_Files_SaveCardState_Init:
        Title_fileMenuElements[Title_saveCardSelection].state = Menu_Files_ElementState_AnimateX;
        Title_fileMenuElements[Title_saveCardSelection].targetPosition = 180;
        Title_fileMenuElements[Title_saveCardSelection].innertextBlendFactor = 1;
        Title_fileMenuElements[3 - Title_saveCardSelection].state = Menu_Files_ElementState_AnimateX;
        Title_fileMenuElements[3 - Title_saveCardSelection].targetPosition = 320;
        Title_saveCardSelectionPhase = Menu_Files_SaveCardState_WaitForUnselectedAnimation;
        break;
    case Menu_Files_SaveCardState_WaitForUnselectedAnimation:
        if (Title_SaveFiles_AreAnimationsComplete() == 0) {
            break;
        }
        if (Title_saveCardSelection == 2) {
            Title_fileMenuElements[2].state = Menu_Files_ElementState_AnimateY;
            Title_fileMenuElements[2].targetPosition = 50;
        }
        Title_saveCardSelectionPhase = Menu_Files_SaveCardState_WaitForAnimation;
        break;
    case Menu_Files_SaveCardState_WaitForAnimation:
        if (Title_SaveFiles_AreAnimationsComplete() != 0) {
            Title_SaveCard_AdvanceCardReadiness(Title_saveCardSelection);
            Title_saveCardSelectionPhase = Menu_Files_SaveCardState_HandleEvents;
        }
        break;
    case Menu_Files_SaveCardState_HandleEvents:
        i = Title_SaveCard_AdvanceCardReadiness(0) & 3;
        if (i == 0) {
            break;
        }
        switch (i) {
        case Save_CardReadinessResult_Ready:
            Title_saveCardSelectionPhase = Menu_Files_SaveCardState_InitSaveMenu;
            break;
        case Save_CardReadinessResult_Timeout:
            Title_saveCardSelectionPhase = Menu_Files_SaveCardState_AnimateLeave;
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_insertError);
            break;
        case Save_CardReadinessResult_Unformatted:
            memset(Title_saveFileSummaries, 0, sizeof *Title_saveFileSummaries * 5);
            for (i = 14; i >= 0; --i) {
                Title_cardDirectoryEntries[i] = NULL;
            }
            j = Save_CardFileSlotStatus_Available;
            for (i = 4; i >= 0; --i) {
                Title_saveFileSummaries[i].summary.base.slotState = j;
            }
            Title_SaveFiles_UpdateSaveFiles(Title_saveCardSelection);
            Title_saveCardSelectionPhase = Menu_Files_SaveCardState_ShowSaveMenu;
            break;
        }
        break;
    case Menu_Files_SaveCardState_AnimateLeave:
        if (Input_buttonsPressed.ports[0].lowByte == 0) {
            break;
        }
        Audio_PlaySfxCentered(0x7E, VS_SFX_MENULEAVE);
        for (i = 1; i < 3; ++i) {
            Title_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
            Title_fileMenuElements[i].targetPosition = 320;
        }
        Title_saveCardSelectionPhase = Menu_Files_SaveCardState_Leave;
        break;
    case Menu_Files_SaveCardState_Leave:
        if (Title_SaveFiles_AreAnimationsComplete() != 0) {
            return -1;
        }
        break;
    case Menu_Files_SaveCardState_InitSaveMenu:
        if (Title_SaveCard_ScanSaveDirectory(Title_saveCardSelection) != 0) {
            Title_saveCardSelectionPhase = Menu_Files_SaveCardState_AnimateLeave;
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_loadfailed);
            break;
        }
        for (i = 0; i < 5; ++i) {
            if (Title_saveFileSummaries[i].summary.base.slotState != Save_CardFileSlotStatus_Unavailable) {
                break;
            }
        }
        if (i == 5) {
            Title_saveCardSelectionPhase = Menu_Files_SaveCardState_AnimateLeave;
            Title_memoryCardMessage = (char*)(Title_saveMenuText + Text_Menu_MCMAN_BIN_Offset_cardFull);
            break;
        }
        Title_SaveFiles_UpdateSaveFiles(Title_saveCardSelection);
        Title_saveCardSelectionPhase = Menu_Files_SaveCardState_ShowSaveMenu;
        break;
    case Menu_Files_SaveCardState_ShowSaveMenu:
        i = Title_SaveFiles_UpdateSaveFiles(0);
        if (i == 0) {
            break;
        }
        if (i < 0) {
            for (i = 1; i < 3; ++i) {
                Title_fileMenuElements[i].state = Menu_Files_ElementState_AnimateX;
                Title_fileMenuElements[i].targetPosition = 320;
            }
            Title_saveCardSelectionPhase = Menu_Files_SaveCardState_Leave;
            break;
        }
        return 1;
    }
    return 0;
}

