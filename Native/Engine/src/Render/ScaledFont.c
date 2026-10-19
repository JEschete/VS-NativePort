// Hand-maintained (tools/port/owned.csv).
#include "Render/ScaledFont.h"

#include <stdint.h>

#include "Actors/TextureAnimation.h"
#include "Audio/Sfx.h"
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Core/Memory/Heap.h"
#include "Core/Text/TextCode.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Legacy/Psx.h"
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

// decomp: BATTLE.PRG _printFixedWidthFontChar
// (src/BATTLE/BATTLE.PRG/5BF94.c:1977).
int Render_DrawScaledFontChar(int char_id, int x, int y, int width, int height,
                              int scale) {
  enum {
    kPrimitiveBufferBytes = 0x20000,
    kSpaceGlyph = 0x8F,
    kSpaceAdvance = 6,
    kGlyphsPerTable = 189,
    kTextBoxCount = 8,
    kPositionFractionBits = 16,
    kGlyphsPerRow = 21,
    kGlyphCellSize = 12,
    kPaletteBaseCode = 0x3780,
    kFirstPaletteIndex = 0x34,
    kPaletteIndexMask = 0x3F,
    kFontTexturePage = 13,
    kTextBoxOrderStride = 4,
    kTextForegroundLayer = 3
  };
  if (char_id == kSpaceGlyph) return x + kSpaceAdvance * scale;
  const unsigned glyph =
      (unsigned)char_id +
      (unsigned char)Render_fontTableSelect * kGlyphsPerTable;
  if (glyph >= sizeof Render_variableFontGlyphWidths ||
      (unsigned)Render_activeTextBoxIndex >= kTextBoxCount ||
      Menu_activeTextBoxOrderingTable == NULL) {
    return x;
  }
  const int next_x =
      x + (unsigned char)Render_variableFontGlyphWidths[glyph] * scale;
  const uintptr_t base =
      (uintptr_t)Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  const uintptr_t cursor = (uintptr_t)Render_frameScratch.primitiveCursor;
  if (base == 0 || cursor < base ||
      cursor - base > kPrimitiveBufferBytes - sizeof(POLY_FT4)) {
    return next_x;
  }
  POLY_FT4* primitive = (void*)cursor;
  memset(primitive, 0, sizeof *primitive);
  setPolyFT4(primitive);
  memcpy(&primitive->r0, &Render_fontColor, sizeof Render_fontColor);
  setXY4(primitive, x >> kPositionFractionBits, y >> kPositionFractionBits,
         (x + width) >> kPositionFractionBits, y >> kPositionFractionBits,
         x >> kPositionFractionBits, (y + height) >> kPositionFractionBits,
         (x + width) >> kPositionFractionBits,
         (y + height) >> kPositionFractionBits);
  const unsigned column = glyph % kGlyphsPerRow * kGlyphCellSize;
  const unsigned row = glyph / kGlyphsPerRow * kGlyphCellSize;
  setUV4(primitive, column, row, column + kGlyphCellSize, row, column,
         row + kGlyphCellSize, column + kGlyphCellSize, row + kGlyphCellSize);
  primitive->clut =
      (uint16_t)(kPaletteBaseCode | (((unsigned char)Render_fontPaletteOffset +
                                      kFirstPaletteIndex) &
                                     kPaletteIndexMask));
  primitive->tpage = kFontTexturePage;
  AddPrim(Menu_activeTextBoxOrderingTable +
              (size_t)Render_activeTextBoxIndex * kTextBoxOrderStride +
              kTextForegroundLayer,
          primitive);
  Render_frameScratch.primitiveCursor = primitive + 1;
  return next_x;
}
