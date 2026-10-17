// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Sfx.h"
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
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Timers.h"

// decomp: D_800EB98C (D_800EB98C@BATTLE/BATTLE.PRG)
u_short Render_kGlyphPixelExpansion[16]; /* data: contents loaded in Step 5 */

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

    prim[0] = (((uintptr_t)(prim + 2) << 8) >> 8) | 0x01000000;
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
            prim[0] = (((uintptr_t)(prim + 5) << 8) >> 8) | 0x04000000;
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
    *nextPrim = ((u_long)Render_frameScratch.primitiveCursor << 8) >> 8;
    Render_frameScratch.primitiveCursor = prim + 1;
}

// decomp: BATTLE.PRG vs_battle_renderTextRaw (src/BATTLE/BATTLE.PRG/5BF94.c:1436)
void Render_DrawCompactText(char const* text, int xy, void* nextPrim)
{
    Render_DrawCompactTextColor(text, xy, vs_getRGB(128, 128, 128), nextPrim);
}

// decomp: BATTLE.PRG _writeStringTerminator (src/BATTLE/BATTLE.PRG/5BF94.c:1443)
void Render_TerminateEncodedString(char* str) { *str = '\0'; }

// decomp: BATTLE.PRG vs_battle_printf (src/BATTLE/BATTLE.PRG/5BF94.c:1445)
char* Render_FormatEncodedText(char* dest, char* src)
{
    int divisor;
    int integer;
    int c;

    c = *src++;

    while (c != '\0') {
        if (c >= Core_TextCode_Control) {
            switch (c) {
            case Core_TextCode_PrintHex:
                c = *src++;
                integer = Battle_stringContext.integers[c % 10];
                if (integer < 0) {
                    integer = -integer;
                    *dest++ = '-';
                }
                if (c < 10) {
                    if (integer == 0) {
                        *dest++ = '0';
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
                c = *src++;
                integer = Battle_stringContext.integers[c % 10];
                if (integer < 0) {
                    integer = -integer;
                    *dest++ = '-';
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
        c = *src++;
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
        if (c < '\f') {
            length += Render_variableFontGlyphWidths[c];
            continue;
        } else if ((c == '\f') || (c == '\0') || (c == '\n')) {
            return length / 6;
        } else if (c == Core_TextCode_Spacing) {
            length += *str++;
        } else if (c == Core_TextCode_PrintDecimal) {
            c = *str++;
            if (c > '9') {
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

    if (glyphId == ' ') {
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
void Render_SetMenuFontStyle(int arg0)
{
    Render_fontTableSelect = 0;
    Render_fontColor = 384;
    Render_fontPaletteOffset = arg0;
}

// decomp: BATTLE.PRG _printVariableWidthFont (src/BATTLE/BATTLE.PRG/5BF94.c:1706)
void Render_DrawProgressiveTextBoxText(Menu_TextBoxState* arg0)
{
    char intBuf[16];
    u_long* sp20 = Menu_activeTextBoxOrderingTable + Render_activeTextBoxIndex * 4;
    int done = 0;
    int sfx = 0;
    int chunksPrinted = 0;
    char* savedString;
    int speed = 1;
    int justify = 0;
    int inInlineString = 0;
    int postContinuation = 0;
    int charsRemaining = arg0->speed;

    int printX;
    int printY;
    int temp_v0;
    int var_a0;
    int var_a2;
    int hexBufPos;
    int decInt;
    int decBufPos;
    char* currentString;
    u_short temp_v1;
    u_int currentChar;
    u_int opcodeParam;

    if (charsRemaining != 0) {

        Render_fontTableSelect = arg0->flags.kind != 4;
        Render_fontColor = arg0->brightness;
        Render_fontPaletteOffset = (1 - (*((char*)arg0) & 1)) * 4;

        if (arg0->flags.kind == 2) {
            Render_fontTableSelect = 0;
            Render_fontPaletteOffset = 0;
        }
        currentString = arg0->string;
        printX = arg0->xIndent;
        printY = arg0->yIndent;

        arg0->flags.done = 0;

        while (charsRemaining > 0) {
            if (postContinuation != 0) {
                --postContinuation;
            }
            currentChar = *currentString;
            ++currentString;
            if (currentChar < 0xEC) {
                if (currentChar < '\f') {
                    printX = Render_DrawVariableWidthGlyph(
                        currentChar, printX, printY, sp20);
                    charsRemaining -= speed;
                    ++chunksPrinted;
                    continue;
                }

                switch (currentChar - 0xE5) {
                case 0: // 0xE5 New page
                    arg0->string = currentString;
                    done = (arg0->speed = 1);
                    arg0->prevChunksPrinted = 0;
                    break;
                case 1: // 0xE6 Continuation arrow
                    if (charsRemaining == 1) {
                        if ((*(&Input_buttonsPressed.ports[0].lowByte)) != 0) {
                            charsRemaining = 0;
                        } else {
                            done =
                                (arg0->flags.arrowAnimFrame + State_tickSpeed) % 24;
                            printX = Render_DrawVariableWidthGlyph(
                                0xBC - (done >> 3), printX, printY, sp20);
                            arg0->flags.arrowAnimFrame = done;
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
                        temp_v1 = arg0->opaque2C;
                        temp_v0 = temp_v1 << 0x10;
                        if (temp_v0 > 0) {
                            arg0->opaque2C = ((temp_v0 >> 0x12) - temp_v1);
                        }
                        arg0->flags.done = 1;
                        if (((*(&Input_buttonsPressed.ports[0].lowByte)) != 0)
                            || (postContinuation != 0)) {
                            arg0->speed = 0xFFFF;
                        }
                        done = 1;
                    }
                    break;
                case 3: // 0xE8 New line
                    printX = arg0->xIndent;
                    if (justify != 0) {
                        printX += ((arg0->charsPerLine * 2)
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
                opcodeParam = *currentString;
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
                                    (((arg0->charsPerLine * 2)
                                         - Render_MeasureEncodedTextLine(currentString))
                                        * 3)
                                    + arg0->xIndent;
                            }
                        } else { // Color
                            Render_fontPaletteOffset = (Render_fontPaletteOffset & 4) | opcodeParam;
                        }
                    }
                    break;
                case 0xFC:
                    if (arg0->opaque2C > 0) {
                        arg0->opaque2C = -opcodeParam;
                    }
                    break;
                case 0xFD: // Print hex
                    hexBufPos = 0;
                    var_a2 = Battle_stringContext.integers[opcodeParam % 10];
                    var_a0 = 0x10000000;
                    if (var_a2 < 0) {
                        var_a2 = -var_a2;
                        intBuf[0] = 0xA7;
                        hexBufPos = 1;
                    }
                    if (opcodeParam < 10) {
                        if (var_a2 == 0) {
                            intBuf[hexBufPos++] = 0;
                        } else {

                            while ((var_a2 / var_a0) == 0) {
                                var_a0 >>= 4;
                            }

                            while (var_a0 != 0) {
                                intBuf[hexBufPos++] = (var_a2 / var_a0) & 0xF;
                                var_a0 >>= 4;
                            }
                        }
                    } else {
                        opcodeParam = (opcodeParam / 10) - 1;
                        var_a0 = 1;
                        while (opcodeParam != 0) {
                            --opcodeParam;
                            var_a0 *= 16;
                        }
                        while (var_a0 != 0) {
                            intBuf[hexBufPos++] = (var_a2 / var_a0) & 0xF;
                            var_a0 >>= 4;
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
                    var_a2 = 1000000000;
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

                            while ((decInt / var_a2) == 0) {
                                var_a2 /= 10;
                            }

                            while (var_a2 != 0) {
                                intBuf[decBufPos++] = (decInt / var_a2) % 10;
                                var_a2 /= 10;
                            }
                        }
                    } else {
                        opcodeParam = (opcodeParam / 10) - 1;
                        var_a2 = 1;
                        while (opcodeParam != 0) {
                            var_a2 *= 10;
                            --opcodeParam;
                        }
                        while (var_a2 != 0) {
                            intBuf[decBufPos++] = (decInt / var_a2) % 10;
                            var_a2 /= 10;
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
            arg0->speed += State_tickSpeed >> 1;
        }

        if ((arg0->prevChunksPrinted != chunksPrinted) && (sfx != 0)) {
            Audio_PlaySfxCentered(0x7E, sfx);
        }

        if (!(arg0->flags.speakerTexture & 0x20)) {
            if (arg0->opaque2C != 0) {
                ++arg0->opaque2C;
            } else {
                Actors_SetActorTextureAnimation(arg0->flags.speakerTexture, 0, 0, 0);
            }
        }
        arg0->prevChunksPrinted = chunksPrinted;
    }
}

// decomp: BATTLE.PRG _printFixedWidthFontChar (src/BATTLE/BATTLE.PRG/5BF94.c:1977) [inline-asm: not yet ported]
int Render_DrawScaledFontChar(int charId, int x, int y, int width, int height, int scale)
{
    VS_UNPORTED("Render::DrawScaledFontChar");
    return 0;
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
        int temp_s6;
        int done;
        int printX = (ctx->frameX << 0x10) + ((ctx->xIndent - ctx->x) * scale);
        int printY = (ctx->frameY << 0x10) + ((ctx->yIndent - ctx->y) * scale);

        Render_fontPaletteOffset = (1 - (*((char*)ctx) & 1)) * 4;
        Render_fontTableSelect = ctx->flags.kind != 4;
        Render_fontColor = ctx->brightness;

        temp_s6 = printX;

        if (ctx->flags.kind == 2) {
            Render_fontTableSelect = 0;
            Render_fontPaletteOffset = 0;
        }

        done = ctx->brightness;
        Render_fontColor = done | (done << 8) | (done << 0x10) | 0x2C000000;
        done = 0;

        while (1) {
            u_int currentChar = *str++;

            if (currentChar < 0xEC) {
                if (currentChar < '\f') {
                    printX = Render_DrawScaledFontChar(
                        currentChar, printX, printY, charWidth, charHeight, scale);
                    continue;
                }

                switch (currentChar - '\f') {
                case 2: // 0xE7 Termination
                    done = 1;
                    break;
                case 3: // 0xE8 New line
                    printX = temp_s6;
                    if (justify != 0) {
                        printX = temp_s6
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
                u_int opcodeParam = *str++;

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
                                printX = temp_s6
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
void Render_BlendTextBoxIntoFramebuffer(Menu_TextBoxState* arg0, int arg1)
{
    RECT rect;
    int height;
    int _y;
    int done = 0;
    u_short* buf;
    int width;
    int x = arg0->x;
    int y = arg0->y;
    int justify = 0;
    char* str = arg0->string;
    u_int printX = arg0->xIndent - x;
    u_int printY;

    _y = y;
    printY = arg0->yIndent - y;
    width = arg0->charsPerLine * 12 + 10;
    height = arg0->lineCount * 13 + 4;

    if ((x < 0) || (y < 0) || ((x + width) >= 320) || ((y + height) >= 224)) {
        return;
    }

    buf = Core_Memory_AllocateFromHighEnd(width * height * 2);

    if ((arg1 >> 0x10) != 0) {
        arg1 = 0x10000;
    }

    setRECT(&rect, x + Render_frameBuf * 320, y, width, height);
    StoreImage(&rect, (void*)buf);

    Render_fontTableSelect = 1;

    while (1) {
        u_int currentChar = *str++;

        if (currentChar < 0xEC) {
            if (currentChar < '\f') {
                if (currentChar != ' ') {
                    currentChar += Render_fontTableSelect * 21 * 9;
                    if (((width - 12) >= printX) && ((height - 12) >= printY)) {
                        Render_BlendGlyphIntoPixels(buf + ((printX + (printY * width))), currentChar,
                            width, arg1);
                    }
                }
                printX += Render_variableFontGlyphWidths[currentChar];
            } else {
                switch (currentChar - '\f') {
                case 2:
                    if (Input_buttonsPressed.allButtons & PADRright) {
                        arg0->speed = 0xFFFF;
                    }
                    done = 1;
                    break;
                case 3:
                    printX = arg0->xIndent - x;
                    if (justify != 0) {
                        printX +=
                            ((arg0->charsPerLine * 2) - Render_MeasureEncodedTextLine(str))
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
            u_int opcodeParam = *str++;
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
                            printX = ((((arg0->charsPerLine * 2)
                                           - Render_MeasureEncodedTextLine(str))
                                          * 3)
                                         + arg0->xIndent)
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

