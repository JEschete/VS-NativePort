// Hand-maintained (tools/port/owned.csv).
#include "Render/ScaledFont.h"
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
#include "Render/Frame.h"
#include "Render/LegacyPackets.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Timers.h"


// decomp: BATTLE.PRG _printFixedWidthFontChar (src/BATTLE/BATTLE.PRG/5BF94.c:1977).
int Render_DrawScaledFontChar(int charId, int x, int y, int width, int height, int scale)
{
    if (charId == 0x8F) return x + 6 * scale;
    const unsigned glyph = (unsigned)charId + (unsigned char)Render_fontTableSelect * 189u;
    if (glyph >= sizeof Render_variableFontGlyphWidths || Render_activeTextBoxIndex >= 8 ||
        Menu_activeTextBoxOrderingTable == NULL) return x;
    const int nextX = x + (unsigned char)Render_variableFontGlyphWidths[glyph] * scale;
    const uintptr_t base = (uintptr_t)Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
    const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
    if (base == 0 || cursor < base || cursor - base > 0x20000 - sizeof(POLY_FT4)) return nextX;
    POLY_FT4* primitive = (void*)cursor;
    memset(primitive, 0, sizeof *primitive);
    setPolyFT4(primitive);
    memcpy(&primitive->r0, &Render_fontColor, sizeof Render_fontColor);
    setXY4(primitive, x >> 16, y >> 16, (x + width) >> 16, y >> 16,
           x >> 16, (y + height) >> 16, (x + width) >> 16, (y + height) >> 16);
    const unsigned column = glyph % 21 * 12;
    const unsigned row = glyph / 21 * 12;
    setUV4(primitive, column, row, column + 12, row, column, row + 12, column + 12, row + 12);
    primitive->clut = (unsigned short)(0x3780 | (((unsigned char)Render_fontPaletteOffset + 0x34) & 0x3F));
    primitive->tpage = 13;
    AddPrim(Menu_activeTextBoxOrderingTable + Render_activeTextBoxIndex * 4 + 3, primitive);
    Render_frameScratch.primitiveCursor = primitive + 1;
    return nextX;
}
