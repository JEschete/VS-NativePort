// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Sfx.h"
#include "Battle/Feedback.h"
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Core/Memory/Heap.h"
#include "Core/Text/TextCode.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/ScaledFont.h"
#include "Render/Sprites.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

// decomp: D_800EB98C (D_800EB98C@BATTLE/BATTLE.PRG)
u_short Render_kGlyphPixelExpansion[16]; /* contents: module image (ModuleData.c) */

// decomp: D_800F4CB8 (D_800F4CB8@BATTLE/BATTLE.PRG)
char Render_fontPaletteOffset;

// decomp: _fontTable (_fontTable@BATTLE/BATTLE.PRG)
char Render_fontTableSelect;

// decomp: _fontBrightness (_fontBrightness@BATTLE/BATTLE.PRG)
int Render_fontColor;

// decomp: D_800F4E80 (D_800F4E80@BATTLE/BATTLE.PRG)
u_char Render_activeTextBoxIndex;

// decomp: BATTLE.PRG vs_battle_renderTextRawColor (src/BATTLE/BATTLE.PRG/5BF94.c:1345)
void Render_DrawCompactTextColor(char const* text, int x, int color, u_long* nextPrim)
{
    u_long* prim = Render_frameScratch.primitiveCursor;
    int y = x >> 16;
    u_char c;

    x = (short)x;

    if (nextPrim == NULL) {
        nextPrim = Render_frameScratch.overlayOrderingTable;
    }

    prim[0] = (LegacyGpu_Handle(prim + 2)) | 0x01000000;
    prim[1] = _get_mode(0, 0, getTPage(0, 0, 768, 0));
    prim += 2;

    while ((c = *text++) != 0) {
        u_int width;
        u_int index;

        c -= ' ';

        if (c >= '[' - ' ') {
            if (c == '[' - ' ') {
                --x;
            }
        } else if (c - ('#' - ' ') < 2u) {
            u_char const* scan;
            u_char ch;

            for (width = 0, scan = text; (ch = *scan++) != 0;) {
                ch -= ' ';

                if (ch > '[' - ' ') {
                    continue;
                }

                width += (Render_compactGlyphWidths[ch >> 1] >> ((ch << 2) & 4)) & 0xF;
                ch -= 'A' - ' ';

                if (ch <= 'Z' - 'A') {
                    index = *scan - 'A';

                    if (index <= 'Z' - 'A') {
                        index += ch * 26;
                        width -= (Render_compactGlyphLeftTrim[index >> 2] >> ((index & 3) * 2)) & 3;
                    }
                }
            }

            x -= width >> (c - ('#' - ' '));
        } else {
            width = (Render_compactGlyphWidths[c >> 1] >> ((c << 2) & 4)) & 0xF;

            if (width == 0) {
                continue;
            }

            index = Render_compactGlyphIndices[c];
            prim[0] = (LegacyGpu_Handle(prim + 5)) | 0x04000000;
            prim[1] = vs_getRGB0Raw(Render_LegacyPrimitiveCode_Sprite, color);
            prim[2] = vs_getXY_2(x, y);
            prim[3] = vs_getUV0Clut(index & 0xFE, (index & 1) * 10 + 76, 864, 223);
            prim[4] = vs_getWH(width, 10);

            if (c == '*' - ' ') {
                prim[2] = vs_getXY_2(x - 1, y - 1);
                prim[3] = vs_getUV0Clut(232, 75, 864, 223);
                prim[4] = vs_getWH(width, 11);
            }

            prim += 5;
            x += width;
            c -= 'A' - ' ';

            if (c <= 'Z' - 'A') {
                index = *text - 'A';

                if (index <= 'Z' - 'A') {
                    index += c * 26;
                    x -= (Render_compactGlyphLeftTrim[index >> 2] >> ((index & 3) * 2)) & 3;
                }
            }
        }
    }

    *prim = *nextPrim & 0xFFFFFF;
    *nextPrim = LegacyGpu_Handle(Render_frameScratch.primitiveCursor);
    Render_frameScratch.primitiveCursor = prim + 1;
}

// decomp: BATTLE.PRG vs_battle_renderTextRaw (src/BATTLE/BATTLE.PRG/5BF94.c:1436)
void Render_DrawCompactText(char const* text, int xy, void* nextPrim)
{
    Render_DrawCompactTextColor(text, xy, vs_getRGB(128, 128, 128), nextPrim);
}

// decomp: BATTLE.PRG _writeStringTerminator (src/BATTLE/BATTLE.PRG/5BF94.c:1443)
void Render_TerminateEncodedString(char* str) { *str = Core_TextCode_Terminator; }

// decomp: BATTLE.PRG vs_battle_printf (src/BATTLE/BATTLE.PRG/5BF94.c:1445)
char* Render_FormatEncodedText(char* dest, char* src)
{
    int divisor;
    int integer;
    int c;

    c = (u_char)*src++;

    while (c != Core_TextCode_Terminator) {
        if (c >= Core_TextCode_Control) {
            switch (c) {
            case Core_TextCode_PrintHex:
                c = (u_char)*src++;
                integer = Battle_stringContext.integers[c % 10];
                if (integer < 0) {
                    integer = -integer;
                    *dest++ = 0xA7;
                }
                if (c < 10) {
                    if (integer == 0) {
                        *dest++ = 0;
                    } else {
                        for (divisor = 0x10000000; (integer / divisor) == 0;
                            divisor >>= 4)
                            ;
                        while (divisor != 0) {
                            *dest++ = (integer / divisor) & 0xF;
                            divisor >>= 4;
                        }
                    }
                } else {
                    c = (c / 10) - 1;
                    divisor = 1;
                    while (c != 0) {
                        --c;
                        divisor *= 16;
                    }
                    while (divisor != 0) {
                        *dest++ = (integer / divisor) & 0xF;
                        divisor >>= 4;
                    }
                }
                break;
            case Core_TextCode_PrintDecimal:
                c = (u_char)*src++;
                integer = Battle_stringContext.integers[c % 10];
                if (integer < 0) {
                    integer = -integer;
                    *dest++ = 0xA7;
                }
                if (c < 10) {
                    if (integer == 0) {
                        *dest++ = 0;
                    } else {
                        for (divisor = 1000000000; (integer / divisor) == 0;
                            divisor /= 10)
                            ;
                        while (divisor != 0) {
                            *dest++ = (integer / divisor) % 10;
                            divisor /= 10;
                        }
                    }
                } else {
                    c = (c / 10) - 1;
                    for (divisor = 1; c != 0; --c) {
                        divisor *= 10;
                    }
                    while (divisor != 0) {
                        *dest++ = (integer / divisor) % 10;
                        divisor /= 10;
                    }
                }
                break;
            case Core_TextCode_PrintString:
                dest = Render_FormatEncodedText(
                    dest, Battle_stringContext.strings[(u_char)*src++]);
                break;
            default:
                *dest++ = c;
                *dest++ = *src++;
            }
        } else {
            *dest++ = c;
        }
        c = (u_char)*src++;
    }
    *dest = c;
    return dest;
}

// decomp: BATTLE.PRG vs_battle_getTextLineLength (src/BATTLE/BATTLE.PRG/5BF94.c:1654)
int Render_MeasureEncodedTextLine(char* str)
{
    int length = 0;

    while (1) {
        u_char c = *str++;
        if (c < 0xE5) {
            length += Render_variableFontGlyphWidths[c];
            continue;
        } else if ((c == 0xE5) || (c == 0xE7) || (c == 0xE8)) {
            return length / 6;
        } else if (c == Core_TextCode_Spacing) {
            length += (u_char)*str++;
        } else if (c == Core_TextCode_PrintDecimal) {
            c = *str++;
            if (c > 9) {
                length += (c / 10) * 6;
            }
        } else if (c >= Core_TextCode_ChunkSize) {
            ++str;
        }
    }
}

// decomp: BATTLE.PRG vs_battle_printVariableWidthFontChar (src/BATTLE/BATTLE.PRG/5BF94.c:1678)
int Render_DrawVariableWidthGlyph(u_int glyphId, int x, int y, u_long* ot)
{
    int glyphRow;
    u_int glyphCol;
    u_long* sprite;

    if (glyphId == 0x8F) {
        return x + 6;
    }

    glyphId += Render_fontTableSelect * 0xBD;
    sprite =
        Render_QueueSprite(Render_fontColor, (x & 0xFFFF) | (y << 0x10), 0xC000C, ot + 3);
    sprite[1] = 0xE100002D;
    glyphRow = glyphId / 21;
    glyphCol = glyphId % 21;
    sprite[4] = ((glyphCol * 0xC) | (glyphRow * 0xC00)
                 | ((((((Render_fontPaletteOffset * 0x10) + 0x340) >> 4) & 0x3F) | 0x3780) << 0x10));
    return x + Render_variableFontGlyphWidths[glyphId];
}

// decomp: BATTLE.PRG vs_battle_setFontStyle (src/BATTLE/BATTLE.PRG/5BF94.c:1699)
void Render_SetMenuFontStyle(int palette_offset)
{
    Render_fontTableSelect = 0;
    Render_fontColor = 384;
    Render_fontPaletteOffset = palette_offset;
}

// decomp: BATTLE.PRG _printVariableWidthFont (src/BATTLE/BATTLE.PRG/5BF94.c:1706)
void Render_DrawProgressiveTextBoxText(Menu_TextBoxState* text_box)
{
    char intBuf[16];
    u_long* ordering_table = Menu_activeTextBoxOrderingTable + Render_activeTextBoxIndex * 4;
    int done = 0;
    int sfx = 0;
    int chunksPrinted = 0;
    char* savedString;
    int speed = 1;
    int justify = 0;
    int inInlineString = 0;
    int postContinuation = 0;
    int charsRemaining = text_box->speed;

    int printX;
    int printY;
    int signed_animation_counter_word;
    int hexadecimal_divisor;
    int hex_value_or_decimal_divisor;
    int hexBufPos;
    int decInt;
    int decBufPos;
    char* currentString;
    u_short animation_counter_snapshot;
    u_int currentChar;
    u_int opcodeParam;

    if (charsRemaining != 0) {

        Render_fontTableSelect = text_box->flags.kind != 4;
        Render_fontColor = text_box->brightness;
        Render_fontPaletteOffset = (1 - (*((char*)text_box) & 1)) * 4;

        if (text_box->flags.kind == 2) {
            Render_fontTableSelect = 0;
            Render_fontPaletteOffset = 0;
        }
        currentString = text_box->string;
        printX = text_box->xIndent;
        printY = text_box->yIndent;

        text_box->flags.done = 0;

        while (charsRemaining > 0) {
            if (postContinuation != 0) {
                --postContinuation;
            }
            currentChar = (u_char)*currentString;
            ++currentString;
            if (currentChar < 0xEC) {
                if (currentChar < 0xE5) {
                    printX = Render_DrawVariableWidthGlyph(
                        currentChar, printX, printY, ordering_table);
                    charsRemaining -= speed;
                    ++chunksPrinted;
                    continue;
                }

                switch (currentChar - 0xE5) {
                case 0: // 0xE5 New page
                    text_box->string = currentString;
                    done = (text_box->speed = 1);
                    text_box->prevChunksPrinted = 0;
                    break;
                case 1: // 0xE6 Continuation arrow
                    if (charsRemaining == 1) {
                        if ((*(&Input_buttonsPressed.ports[0].lowByte)) != 0) {
                            charsRemaining = 0;
                        } else {
                            done =
                                (text_box->flags.arrowAnimFrame + State_tickSpeed) % 24;
                            printX = Render_DrawVariableWidthGlyph(
                                0xBC - (done >> 3), printX, printY, ordering_table);
                            text_box->flags.arrowAnimFrame = done;
                        }
                        done = 1;
                    } else {
                        --charsRemaining;
                        postContinuation = 2;
                    }
                    break;
                case 2: // 0xE7 Termination
                    if (inInlineString != 0) {
                        currentString = savedString;
                        inInlineString = 0;
                    } else {
                        animation_counter_snapshot = text_box->opaque2C;
                        signed_animation_counter_word = animation_counter_snapshot << 0x10;
                        if (signed_animation_counter_word > 0) {
                            text_box->opaque2C = ((signed_animation_counter_word >> 0x12) - animation_counter_snapshot);
                        }
                        text_box->flags.done = 1;
                        if (((*(&Input_buttonsPressed.ports[0].lowByte)) != 0)
                            || (postContinuation != 0)) {
                            text_box->speed = 0xFFFF;
                        }
                        done = 1;
                    }
                    break;
                case 3: // 0xE8 New line
                    printX = text_box->xIndent;
                    if (justify != 0) {
                        printX += ((text_box->charsPerLine * 2)
                                      - Render_MeasureEncodedTextLine(currentString))
                                * 3;
                    }
                    printY += 13;
                    break;
                }
                if (done != 0) {
                    break;
                }
            } else {
                opcodeParam = (u_char)*currentString;
                ++currentString;

                switch (currentChar) {
                case 0xF8: // Character buffer chunking size
                    speed = opcodeParam;
                    break;
                case 0xF9: // Play sfx
                    sfx = opcodeParam;
                    break;
                case 0xFA: // Offset x
                    currentChar = 256;
                    if (opcodeParam >= 240) {
                        printX = printX - currentChar + opcodeParam;
                    } else {
                        printX += opcodeParam;
                    }
                    break;
                case 0xFB: // Manipulate font
                    if (opcodeParam & 0xF8) { // Offset y
                        printY += opcodeParam >> 3;
                    } else {
                        if (opcodeParam == 6) { // Font table 0
                            Render_fontTableSelect = 0;
                        } else if (opcodeParam == 5) { // Font table 1
                            Render_fontTableSelect = 1;
                        } else if (opcodeParam == 4) { // Justify
                            justify ^= 1;
                            if (justify != 0) {
                                printX =
                                    (((text_box->charsPerLine * 2)
                                         - Render_MeasureEncodedTextLine(currentString))
                                        * 3)
                                    + text_box->xIndent;
                            }
                        } else { // Color
                            Render_fontPaletteOffset = (Render_fontPaletteOffset & 4) | opcodeParam;
                        }
                    }
                    break;
                case 0xFC:
                    if (text_box->opaque2C > 0) {
                        text_box->opaque2C = -opcodeParam;
                    }
                    break;
                case 0xFD: // Print hex
                    hexBufPos = 0;
                    hex_value_or_decimal_divisor = Battle_stringContext.integers[opcodeParam % 10];
                    hexadecimal_divisor = 0x10000000;
                    if (hex_value_or_decimal_divisor < 0) {
                        hex_value_or_decimal_divisor = -hex_value_or_decimal_divisor;
                        intBuf[0] = 0xA7;
                        hexBufPos = 1;
                    }
                    if (opcodeParam < 10) {
                        if (hex_value_or_decimal_divisor == 0) {
                            intBuf[hexBufPos++] = 0;
                        } else {

                            while ((hex_value_or_decimal_divisor / hexadecimal_divisor) == 0) {
                                hexadecimal_divisor >>= 4;
                            }

                            while (hexadecimal_divisor != 0) {
                                intBuf[hexBufPos++] = (hex_value_or_decimal_divisor / hexadecimal_divisor) & 0xF;
                                hexadecimal_divisor >>= 4;
                            }
                        }
                    } else {
                        opcodeParam = (opcodeParam / 10) - 1;
                        hexadecimal_divisor = 1;
                        while (opcodeParam != 0) {
                            --opcodeParam;
                            hexadecimal_divisor *= 16;
                        }
                        while (hexadecimal_divisor != 0) {
                            intBuf[hexBufPos++] = (hex_value_or_decimal_divisor / hexadecimal_divisor) & 0xF;
                            hexadecimal_divisor >>= 4;
                        }
                    }
                    intBuf[hexBufPos] = 0xE7;
                    savedString = currentString;
                    currentString = intBuf;
                    inInlineString = 1;
                    break;
                case 0xFE: // Print decimal
                    decBufPos = 0;
                    decInt = Battle_stringContext.integers[opcodeParam % 10];
                    hex_value_or_decimal_divisor = 1000000000;
                    if (decInt < 0) {
                        decInt = -decInt;
                        intBuf[0] = 0xA7; // -
                        decBufPos = 1;
                    }
                    if (opcodeParam < 10) {
                        if (decInt == 0) {
                            intBuf[decBufPos] = 0;
                            ++decBufPos;
                        } else {

                            while ((decInt / hex_value_or_decimal_divisor) == 0) {
                                hex_value_or_decimal_divisor /= 10;
                            }

                            while (hex_value_or_decimal_divisor != 0) {
                                intBuf[decBufPos++] = (decInt / hex_value_or_decimal_divisor) % 10;
                                hex_value_or_decimal_divisor /= 10;
                            }
                        }
                    } else {
                        opcodeParam = (opcodeParam / 10) - 1;
                        hex_value_or_decimal_divisor = 1;
                        while (opcodeParam != 0) {
                            hex_value_or_decimal_divisor *= 10;
                            --opcodeParam;
                        }
                        while (hex_value_or_decimal_divisor != 0) {
                            intBuf[decBufPos++] = (decInt / hex_value_or_decimal_divisor) % 10;
                            hex_value_or_decimal_divisor /= 10;
                        }
                    }
                    intBuf[decBufPos] = 0xE7;
                    savedString = currentString;
                    currentString = intBuf;
                    inInlineString = 1;
                    break;
                case 0xFF: // Print string reference
                    savedString = currentString;
                    currentString = (char*)Battle_stringContext.strings[opcodeParam];
                    inInlineString = 1;
                    break;
                }
            }
        }

        if (charsRemaining <= 0) {
            text_box->speed += State_tickSpeed >> 1;
        }

        if ((text_box->prevChunksPrinted != chunksPrinted) && (sfx != 0)) {
            Audio_PlaySfxCentered(0x7E, sfx);
        }

        if (!(text_box->flags.speakerTexture & 0x20)) {
            if (text_box->opaque2C != 0) {
                ++text_box->opaque2C;
            } else {
                Actors_SetActorTextureAnimation(text_box->flags.speakerTexture, 0, 0, 0);
            }
        }
        text_box->prevChunksPrinted = chunksPrinted;
    }
}

// decomp: BATTLE.PRG _printFixedWidthFont (src/BATTLE/BATTLE.PRG/5BF94.c:2096)
void Render_DrawScaledTextBoxText(Menu_TextBoxState* ctx, int scale)
{
    int justify = 0;
    char* str = ctx->string;
    int charWidth = scale * 12;
    int charHeight = charWidth;
    int lineHeight = scale * 13;

    if (ctx->speed != 0) {
        int line_origin_x;
        int done;
        int printX = (ctx->frameX << 0x10) + ((ctx->xIndent - ctx->x) * scale);
        int printY = (ctx->frameY << 0x10) + ((ctx->yIndent - ctx->y) * scale);

        Render_fontPaletteOffset = (1 - (*((char*)ctx) & 1)) * 4;
        Render_fontTableSelect = ctx->flags.kind != 4;
        Render_fontColor = ctx->brightness;

        line_origin_x = printX;

        if (ctx->flags.kind == 2) {
            Render_fontTableSelect = 0;
            Render_fontPaletteOffset = 0;
        }

        done = ctx->brightness;
        Render_fontColor = done | (done << 8) | (done << 0x10) | 0x2C000000;
        done = 0;

        while (1) {
            u_int currentChar = (u_char)*str++;

            if (currentChar < 0xEC) {
                if (currentChar < 0xE5) {
                    printX = Render_DrawScaledFontChar(
                        currentChar, printX, printY, charWidth, charHeight, scale);
                    continue;
                }

                switch (currentChar - 0xE5) {
                case 2: // 0xE7 Termination
                    done = 1;
                    break;
                case 3: // 0xE8 New line
                    printX = line_origin_x;
                    if (justify != 0) {
                        printX = line_origin_x
                               + (((ctx->charsPerLine * 2)
                                      - Render_MeasureEncodedTextLine(str))
                                   * (charWidth >> 2));
                    }
                    printY += lineHeight;
                    break;
                }
                if (done != 0) {
                    break;
                }
            } else {
                u_int opcodeParam = (u_char)*str++;

                switch (currentChar - 0xF8) {
                case 2: // Offset x
                    if (opcodeParam >= 0xF0) {
                        printX -= ((256 - opcodeParam) * charWidth) / 12;
                    } else {
                        printX += (opcodeParam * charWidth) / 12;
                    }
                    break;
                case 3: // Manipulate font
                    if (opcodeParam & 0xF8) { // Offset y
                        printY += (opcodeParam >> 3) * scale;
                    } else {
                        if (opcodeParam == 6) { // Font table 0
                            Render_fontTableSelect = 0;
                        } else if (opcodeParam == 5) { // Font table 1
                            Render_fontTableSelect = 1;
                        } else if (opcodeParam == 4) { // Justify
                            justify ^= 1;
                            if (justify != 0) {
                                printX = line_origin_x
                                       + (((ctx->charsPerLine * 2)
                                              - Render_MeasureEncodedTextLine(str))
                                           * (charWidth >> 2));
                            }
                        } else { // Color
                            Render_fontPaletteOffset = (Render_fontPaletteOffset & 4) | opcodeParam;
                        }
                    }
                    break;
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_800C7EBC (src/BATTLE/BATTLE.PRG/5BF94.c:2192)
void Render_BlendGlyphIntoPixels(u_short* destination, u_int glyph, int stride, u_int alpha)
{
    RECT rect;
    u_short pixels[36];
    u_short packed;
    int row;
    int column;
    u_short* output;
    u_int inverse = 0x10000 - alpha;

    setRECT(&rect, (glyph % 21) * 3 + 832, (glyph / 21) * 12, 3, 12);
    StoreImage(&rect, (u_long*)pixels);

    for (row = 0; row < 12; ++row, destination += stride) {
        for (column = 0, output = destination; column < 12; ++column, ++output) {
            u_short source;
            u_int index;

            if (column & 3) {
                packed >>= 4;
            } else {
                packed = pixels[(column >> 2) + row * 3];
            }

            index = packed & 15;
            source = index;

            if (index == 0) {
                continue;
            }

            source = Render_kGlyphPixelExpansion[source];
            *output =
                (((((*output & 31) * inverse) + ((source & 31) * alpha)) >> 16) & 31)
                | (((((*output & 0x3E0) * inverse) + ((source & 0x3E0) * alpha)) >> 16)
                    & 0x3E0)
                | (((((*output & 0x7C00) * inverse) + ((source & 0x7C00) * alpha)) >> 16)
                    & 0x7C00);
        }
    }
}

// decomp: BATTLE.PRG _renderTextImmediate (src/BATTLE/BATTLE.PRG/5BF94.c:2236)
void Render_BlendTextBoxIntoFramebuffer(Menu_TextBoxState* text_box, int glyph_alpha)
{
    RECT rect;
    int height;
    int _y;
    int done = 0;
    u_short* buf;
    int width;
    int x = text_box->x;
    int y = text_box->y;
    int justify = 0;
    char* str = text_box->string;
    u_int printX = text_box->xIndent - x;
    u_int printY;

    _y = y;
    printY = text_box->yIndent - y;
    width = text_box->charsPerLine * 12 + 10;
    height = text_box->lineCount * 13 + 4;

    if ((x < 0) || (y < 0) || ((x + width) >= 320) || ((y + height) >= 224)) {
        return;
    }

    buf = Core_Memory_AllocateFromHighEnd(width * height * 2);

    if ((glyph_alpha >> 0x10) != 0) {
        glyph_alpha = 0x10000;
    }

    setRECT(&rect, x + Render_frameBuf * 320, y, width, height);
    StoreImage(&rect, (void*)buf);

    Render_fontTableSelect = 1;

    while (1) {
        u_int currentChar = (u_char)*str++;

        if (currentChar < 0xEC) {
            if (currentChar < Core_TextCode_NonPrinting) {
                if (currentChar != Core_TextCode_Space) {
                    currentChar += Render_fontTableSelect * 21 * 9;
                    if (((width - 12) >= printX) && ((height - 12) >= printY)) {
                        Render_BlendGlyphIntoPixels(buf + ((printX + (printY * width))), currentChar,
                            width, glyph_alpha);
                    }
                }
                printX += Render_variableFontGlyphWidths[currentChar];
            } else {
                switch (currentChar - Core_TextCode_NonPrinting) {
                case 2:
                    if (Input_buttonsPressed.allButtons & PADRright) {
                        text_box->speed = 0xFFFF;
                    }
                    done = 1;
                    break;
                case 3:
                    printX = text_box->xIndent - x;
                    if (justify != 0) {
                        printX +=
                            ((text_box->charsPerLine * 2) - Render_MeasureEncodedTextLine(str))
                            * 3;
                    }
                    printY += 13;
                    break;
                }
                if (done != 0) {
                    break;
                }
            }
        } else {
            u_int opcodeParam = (u_char)*str++;
            switch (currentChar - 0xF8) {
            case 2:
                currentChar = 256;
                if (opcodeParam >= 0xF0) {
                    printX = printX - currentChar + opcodeParam;
                } else {
                    printX += opcodeParam;
                }
                break;
            case 3:
                if (opcodeParam & 0xF8) {
                    printY += opcodeParam >> 3;
                } else {
                    if (opcodeParam == 6) {
                        Render_fontTableSelect = 0;
                    } else if (opcodeParam == 5) {
                        Render_fontTableSelect = 1;
                    } else if (opcodeParam == 4) {
                        justify ^= 1;
                        if (justify != 0) {
                            printX = ((((text_box->charsPerLine * 2)
                                           - Render_MeasureEncodedTextLine(str))
                                          * 3)
                                         + text_box->xIndent)
                                   - x;
                        }
                    }
                }
                break;
            }
        }
    }
    Render_UploadImageRectangle(
        (x + (Render_frameBuf * 320)) | (_y << 0x10), buf, width | (height << 0x10));
    Core_Memory_FreeFromHighEnd(buf);
}

