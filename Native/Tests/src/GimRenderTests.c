#include <stdlib.h>
#include <string.h>

#include "Battle/Targeting.h"
#include "Core/Text/TextCode.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Effects/ScreenEffects.h"
#include "Event/ScriptVm.h"
#include "Input/Controller.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Frame.h"
#include "Render/Images.h"
#include "Render/Render.h"
#include "Render/ScaledFont.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"

_Static_assert(sizeof Menu_textBoxOrderingTables == 102 * sizeof(u_long),
               "three textbox ordering buffers");
_Static_assert(sizeof(Event_SplashLayerView) == sizeof(Render_GimLayer),
               "event and renderer GIM layer stride");
_Static_assert(offsetof(Render_GimLayer, brightness) == 3,
               "GIM brightness remains in the first packed word");
_Static_assert(offsetof(Event_SplashLayerView, offsetX) ==
                   offsetof(Render_GimLayer, offsetX),
               "GIM offset view");
_Static_assert(offsetof(Event_SplashLayerView, scaleFixed12) ==
                   offsetof(Render_GimLayer, scaleAndOrder),
               "GIM scale view");

int Tests_EncodedTextFormatting(void) {
  const Menu_StringContext saved = Battle_stringContext;
  char nested[] = {10, 0, Core_TextCode_PrintDecimal, 1,
                   Core_TextCode_Terminator};
  char format[] = {Core_TextCode_PrintDecimal, 0, 0x8F,
                   Core_TextCode_PrintString,  0, 0x8F,
                   Core_TextCode_PrintHex,     2, Core_TextCode_Terminator,
                   Core_TextCode_PrintString,  0};
  const unsigned char expected[] = {
      0xA7, 1, 0, 5, 0x8F, 10, 0, 7, 0x8F, 0, Core_TextCode_Terminator};
  char output[64];
  memset(output, 0x55, sizeof output);
  Battle_stringContext.integers[0] = -105;
  Battle_stringContext.integers[1] = 7;
  Battle_stringContext.integers[2] = 0;
  Battle_stringContext.strings[0] = nested;
  char* end = Render_FormatEncodedText(output + 1, format);
  int passed = end == output + sizeof expected &&
               memcmp(output + 1, expected, sizeof expected) == 0 &&
               output[0] == 0x55 && output[sizeof expected + 1] == 0x55;
  Render_TerminateEncodedString(output);
  passed = passed && (unsigned char)output[0] == Core_TextCode_Terminator;
  Battle_stringContext = saved;
  return passed;
}

int Tests_GimEventLayerLayout(void) {
  Render_GimLayer layers[3] = {0};
  Event_SplashLayerView* eventLayers = (void*)layers;
  unsigned short file = 7;
  for (unsigned index = 0; index < 3; ++index) {
    layers[index].fileData = &file;
    eventLayers[index].isVisible = 1;
    eventLayers[index].isLoadPending = 1;
    eventLayers[index].renderMode = index;
    eventLayers[index].brightness = 128 + index;
    eventLayers[index].gridAndImageId = (13 << 16) | (16 << 8) | 5;
    eventLayers[index].offsetX = 20 + index;
    eventLayers[index].offsetY = -30 - (int)index;
    eventLayers[index].scaleFixed12 = 4096;
  }
  for (unsigned index = 0; index < 3; ++index) {
    if (!layers[index].drawEnabled || !layers[index].loading ||
        layers[index].drawRoutine != index ||
        (unsigned char)layers[index].brightness != 128 + index ||
        layers[index].columns != 5 || layers[index].rows != 16 ||
        layers[index].imageId != 13 || layers[index].offsetX != 20 + index ||
        layers[index].offsetY != -30 - (int)index ||
        layers[index].scaleAndOrder != 4096 ||
        layers[index].fileData != &file ||
        eventLayers[index].nativePointer10 != &file) {
      return 0;
    }
  }
  return 1;
}

int Tests_CompactTextAndGaugeLinks(void) {
  u_long storage[64] = {0};
  u_long ordering = 0xFFFFFF;
  const Render_FrameScratch saved = Render_frameScratch;
  const unsigned char savedWidth = Render_compactGlyphWidths[16];
  const unsigned char savedIndex = Render_compactGlyphIndices[33];
  Render_compactGlyphWidths[16] = 0x60;
  Render_compactGlyphIndices[33] = 0;
  Render_frameScratch.primitiveCursor = storage;
  Render_DrawCompactTextColor("A", 0, 0x808080, &ordering);
  int passed = nextPrim(&ordering) == storage &&
               nextPrim(storage) == storage + 2 &&
               nextPrim(storage + 2) == storage + 7 &&
               LegacyGpu_WalkList(&ordering) == 4;
  ordering = 0xFFFFFF;
  Render_frameScratch.primitiveCursor = storage;
  Render_DrawStatGauge(0, 32, &ordering, 0);
  passed = passed && nextPrim(&ordering) == storage && (ordering >> 24) == 0 &&
           LegacyGpu_WalkList(&ordering) == 2;
  Render_compactGlyphWidths[16] = savedWidth;
  Render_compactGlyphIndices[33] = savedIndex;
  Render_frameScratch = saved;
  return passed;
}

int Tests_ScaledDialogueGlyphs(void) {
  unsigned char* memory = malloc(0x20010);
  if (memory == NULL) return 0;
  memset(memory, 0xA5, 0x20010);
  const Render_FrameScratch savedFrame = Render_frameScratch;
  void* savedBuffer = Render_legacyPrimitiveBuffers[0];
  const int savedFrameIndex = Render_frameBuf;
  u_long* savedOrdering = Menu_activeTextBoxOrderingTable;
  const int savedColor = Render_fontColor;
  const char savedPalette = Render_fontPaletteOffset,
             savedTable = Render_fontTableSelect;
  const unsigned char savedIndex = Render_activeTextBoxIndex;
  const char savedWidth = Render_variableFontGlyphWidths[5];
  const char savedSpaceWidth = Render_variableFontGlyphWidths[0x8F];
  const Menu_TextBoxState savedBox = Menu_textBoxes[2];
  u_char* savedShape = Event_textBoxFrameStyles[0];
  u_long ordering[34];
  ClearOTag(ordering, 34);
  Menu_activeTextBoxOrderingTable = ordering;
  Render_legacyPrimitiveBuffers[0] = memory + 8;
  Render_frameBuf = 0;
  Render_frameScratch.primitiveCursor = memory + 8;
  Render_fontColor = 0x2C808080;
  Render_fontPaletteOffset = Render_fontTableSelect = 0;
  Render_activeTextBoxIndex = 2;
  Render_variableFontGlyphWidths[5] = 7;
  Render_variableFontGlyphWidths[0x8F] = 6;
  int nextX = Render_DrawScaledFontChar(5, 10 << 16, 20 << 16, 12 << 16,
                                        12 << 16, 1 << 16);
  POLY_FT4* glyph = (void*)(memory + 8);
  int passed = nextX == 17 << 16 && glyph->x0 == 10 && glyph->y0 == 20 &&
               glyph->x3 == 22 && glyph->y3 == 32 && glyph->u0 == 60 &&
               glyph->v0 == 0 && glyph->u3 == 72 && glyph->v3 == 12 &&
               glyph->clut == 0x37B4 && glyph->tpage == 13 &&
               nextPrim(&ordering[11]) == glyph;
  void* end = Render_frameScratch.primitiveCursor;
  passed =
      passed &&
      Render_DrawScaledFontChar(0x8F, nextX, 0, 0, 0, 1 << 16) == 23 << 16 &&
      Render_frameScratch.primitiveCursor == end;
  Menu_TextBoxState box = {0};
  char text[] = {5, (char)0x8F, 5, (char)0xE8, 5, (char)0xE7};
  box.flags.kind = 4;
  box.string = text;
  passed = passed && Render_MeasureEncodedTextLine(text) == 3;
  box.speed = 1;
  box.brightness = 128;
  box.frameX = 10;
  box.frameY = 20;
  Render_frameScratch.primitiveCursor = memory + 8;
  ClearOTag(ordering, 34);
  Render_DrawScaledTextBoxText(&box, 1 << 16);
  passed = passed && Render_frameScratch.primitiveCursor == glyph + 3 &&
           glyph[1].x0 == 23 && glyph[2].x0 == 10 && glyph[2].y0 == 33 &&
           memory[0] == 0xA5 && memory[0x2000F] == 0xA5;
  Render_frameScratch.primitiveCursor = memory + 8;
  ClearOTag(ordering, 34);
  box.speed = 100;
  box.opaque2C = 1;
  Render_DrawProgressiveTextBoxText(&box);
  passed = passed && box.flags.done && box.prevChunksPrinted == 4 &&
           Render_frameScratch.primitiveCursor ==
               memory + 8 + 3 * 6 * sizeof(u_long);
  unsigned shape[] = {0x01040004, 0x00FF0000, 0xFFFFFF00, 0x03020100};
  Event_textBoxFrameStyles[0] = (u_char*)shape;
  memset(&Menu_textBoxes[2], 0, sizeof Menu_textBoxes[2]);
  Menu_textBoxes[2].frameX = 10;
  Menu_textBoxes[2].frameY = 20;
  Menu_textBoxes[2].frameWidth = 64;
  Menu_textBoxes[2].frameHeight = 64;
  Menu_textBoxes[2].brightness = 128;
  Render_frameScratch.primitiveCursor = memory + 8;
  ClearOTag(ordering, 34);
  Menu_DrawTextBoxFrame(2);
  passed = passed && (ordering[9] >> 24) == 0 && (ordering[10] >> 24) == 0 &&
           nextPrim(&ordering[9]) != NULL && nextPrim(&ordering[10]) != NULL &&
           LegacyGpu_WalkList(ordering) > 34;
  Menu_textBoxes[2] = savedBox;
  Event_textBoxFrameStyles[0] = savedShape;
  Render_frameScratch = savedFrame;
  Render_legacyPrimitiveBuffers[0] = savedBuffer;
  Render_frameBuf = savedFrameIndex;
  Menu_activeTextBoxOrderingTable = savedOrdering;
  Render_fontColor = savedColor;
  Render_fontPaletteOffset = savedPalette;
  Render_fontTableSelect = savedTable;
  Render_activeTextBoxIndex = savedIndex;
  Render_variableFontGlyphWidths[5] = savedWidth;
  Render_variableFontGlyphWidths[0x8F] = savedSpaceWidth;
  free(memory);
  return passed;
}

// F01393 / BATTLE func_800CBBCC, 5BF94.c:4056: preserve fixed-point rounding
// and native packet links.
static short Coordinate(int index, int count, int scale, int offset,
                        int vertical) {
  int value = (index * 2 - count) * scale * (vertical ? 15 : 1);
  int divisor = vertical ? 8192 : 128;
  int rounded = value / divisor - (value % divisor < 0);
  return (short)(rounded + offset + (vertical ? 120 : 160));
}

static u_long Position(short x, short y) {
  return (u_long)(u_short)x | ((u_long)(u_short)y << 16);
}

#define CHECK_GIM(condition) \
  do {                       \
    if (!(condition)) {      \
      failedLine = __LINE__; \
      goto cleanup;          \
    }                        \
  } while (0)

int Tests_GimTilePackets(int columns, int rows, int scale, int offsetX,
                         int offsetY, int routine, int brightness,
                         int framebuffer) {
  enum { MaxPacketWords = 250 * 11 + 12 };
  const u_long guard = 0xDEADBEEF;
  const int clut = 0x12340000;
  Render_GimLayer image = {0};
  unsigned char savedScratch[1024];
  unsigned char expectedScratch[1024];
  u_long storage[MaxPacketWords + 2];
  u_long* expectedPackets[252];
  u_long* packets = storage + 1;
  u_long* expectedEnd = packets;
  u_long* previous = NULL;
  u_long ot = 0xFFFFFF;
  int savedFrameBuf = Render_frameBuf;
  char savedBlend = Render_gimBlendSelect;
  int failedLine = 0;
  int blend = routine == 1 && brightness != 128;
  int drawBrightness = routine == 2 ? 128 : brightness;
  int packetCount = 0;
  int i;
  int j;

  image.columns = (u_char)columns;
  image.rows = (u_char)rows;
  image.offsetX = (short)offsetX;
  image.offsetY = (short)offsetY;
  image.scaleAndOrder = (short)scale;
  image.brightness = (char)brightness;
  image.vramPage = 18;
  image.textureMode = 1;
  for (i = 0; i < 250; ++i) {
    image.tileTable[i + 4] = (u_short)(i + 1);
  }
  for (i = 0; i < MaxPacketWords + 2; ++i) {
    storage[i] = guard;
  }

  memcpy(savedScratch, Core_ScratchBase(), sizeof savedScratch);
  memset(Core_ScratchBase(), 0xA5, sizeof savedScratch);
  Render_frameScratch.primitiveCursor = packets;
  memcpy(expectedScratch, Core_ScratchBase(), sizeof expectedScratch);
  Render_frameBuf = framebuffer;
  Render_gimBlendSelect = 0;

  switch (routine) {
    case 0:
      Render_DrawGimTiles(&image, clut, &ot);
      break;
    case 1:
      Render_DrawGimFramebufferFade(&image, clut, &ot);
      break;
    case 2:
      Render_DrawGimScanlineWipe(&image, clut, &ot);
      break;
    case 3:
      Render_DrawGimCrossfade(&image, clut, &ot);
      break;
  }

  if (columns * rows <= 250) {
    for (j = 0; j < rows; ++j) {
      short top = Coordinate(j, rows, scale & 0x3FFF, offsetY, 1);
      short bottom = Coordinate(j + 1, rows, scale & 0x3FFF, offsetY, 1);
      if (top >= 240 || bottom < 0) {
        continue;
      }
      for (i = 0; i < columns; ++i) {
        short left = Coordinate(i, columns, scale & 0x3FFF, offsetX, 0);
        short right = Coordinate(i + 1, columns, scale & 0x3FFF, offsetX, 0);
        int tile = i + j * columns + 1;
        int u = (tile * 64) & 0xC0;
        int v = ((tile * 64) & 0x1F00) * 15;
        int tpage = getTPage(1, 1, (tile >> 2) & 0x1C0, 256);
        int sprite = (scale & 0x3FFF) == ONE;
        int words = sprite ? 6 : 11;
        u_long* p = expectedEnd;
        if (left >= 320 || right < 0) {
          continue;
        }
        CHECK_GIM(p[0] == ((u_long)(words - 1) << 24 |
                           (previous ? LegacyGpu_Handle(previous) : 0xFFFFFF)));
        CHECK_GIM(p[1] == (u_long)(tpage | _get_mode(0, 0, 0)));
        CHECK_GIM(p[2] ==
                  ((((u_long)(sprite ? 0x64 : 0x2C) + blend * 2) << 24) |
                   (u_long)drawBrightness * 0x010101));
        CHECK_GIM(p[3] == Position(left, top));
        CHECK_GIM(p[4] == (u_long)(u | v | clut));
        if (sprite) {
          CHECK_GIM(p[5] == (64 | (15 << 16)));
        } else {
          int u1 = u + 64;
          u1 -= u1 >> 8;
          CHECK_GIM(p[5] == Position(right, top));
          CHECK_GIM(p[6] == (u_long)(u1 | v | (tpage << 16)));
          CHECK_GIM(p[7] == Position(left, bottom));
          CHECK_GIM(p[8] == (u_long)(u | (v + 0xF00)));
          CHECK_GIM(p[9] == Position(right, bottom));
          CHECK_GIM(p[10] == (u_long)(u1 | (v + 0xF00)));
        }
        previous = p;
        expectedPackets[packetCount++] = p;
        expectedEnd += words;
      }
    }
  }

  if (blend) {
    int fb = framebuffer == 0 ? 320 : 0;
    for (i = 0; i < 2; ++i) {
      u_long* p = expectedEnd;
      CHECK_GIM(p[0] == (0x05000000 |
                         (previous ? LegacyGpu_Handle(previous) : 0xFFFFFF)));
      CHECK_GIM(p[1] == (0xE1000120 | ((fb + i * 256) >> 6)));
      CHECK_GIM(p[2] == (0x64000000 | (u_long)(128 - brightness) * 0x010101));
      CHECK_GIM(p[3] == (u_long)(i * 256));
      CHECK_GIM(p[4] == 0);
      CHECK_GIM(p[5] == (u_long)((i ? 64 : 256) | (240 << 16)));
      previous = p;
      expectedPackets[packetCount++] = p;
      expectedEnd += 6;
    }
  }

  CHECK_GIM(Render_frameScratch.primitiveCursor == expectedEnd);
  CHECK_GIM(previous ? LegacyGpu_Pointer(ot) == previous : ot == 0xFFFFFF);
  for (i = packetCount - 1; i >= 0; --i) {
    CHECK_GIM(previous == expectedPackets[i]);
    previous = nextPrim(previous);
  }
  CHECK_GIM(previous == NULL);
  CHECK_GIM(storage[0] == guard);
  for (u_long* p = expectedEnd; p < storage + MaxPacketWords + 2; ++p) {
    CHECK_GIM(*p == guard);
  }
  CHECK_GIM((u_char)image.brightness == brightness);
  Render_frameScratch.primitiveCursor = packets;
  CHECK_GIM(
      memcmp(Core_ScratchBase(), expectedScratch, sizeof expectedScratch) == 0);

cleanup:
  memcpy(Core_ScratchBase(), savedScratch, sizeof savedScratch);
  Render_frameBuf = savedFrameBuf;
  Render_gimBlendSelect = savedBlend;
  return failedLine;
}

int CheckFeedbackEffectDialogueLinks(int framebuffer, int feedback_layer) {
  enum {
    kOrderingEntries = 0x822,
    kSceneOrderingEntry = 18,
    kTextOrderingEntry = 12,
    kFeedbackSpriteCount = 7 * (5 * 3 + 10 * 3),
    kSpritePacketWords = 6,
    kFeedbackPacketWords = 8,
    kRestorePacketWords = 5,
    kPacketWords = kFeedbackSpriteCount * kSpritePacketWords +
                   kFeedbackPacketWords + kRestorePacketWords
  };
  const u_long guard = 0xDEADBEEF;
  u_long ordering[kOrderingEntries];
  u_long storage[kPacketWords + 2];
  u_long dialogue[] = {0, 0x60000000, 0, 0x00010001};
  const Render_FrameScratch saved_frame = Render_frameScratch;
  const int saved_framebuffer = Render_frameBuf;
  Effects_ScreenEffectOverlay* saved_effect = Effects_screenEffectOverlay;
  Effects_ScreenEffectOverlay effect = {0};
  effect.feedbackLayer = feedback_layer;
  Effects_screenEffectOverlay = &effect;
  Render_frameBuf = framebuffer;
  ClearOTagR(ordering, kOrderingEntries);
  setlen(dialogue, 3);
  AddPrim(ordering + kTextOrderingEntry, dialogue);
  for (int index = 0; index < kPacketWords + 2; ++index) {
    storage[index] = guard;
  }
  Render_frameScratch.orderingTable = ordering + kSceneOrderingEntry;
  Render_frameScratch.primitiveCursor = storage + 1;

  Effects_ScreenDistortion_DrawFeedbackTiles();

  u_long* feedback =
      storage + 1 + (size_t)kFeedbackSpriteCount * kSpritePacketWords;
  u_long* restore = feedback + kFeedbackPacketWords;
  const int feedback_entry = kSceneOrderingEntry + feedback_layer - 3;
  int passed =
      nextPrim(ordering + feedback_entry) == feedback &&
      nextPrim(feedback) == ordering + feedback_entry - 1 &&
      nextPrim(ordering + kSceneOrderingEntry) == restore &&
      nextPrim(restore) == feedback - kSpritePacketWords &&
      Render_frameScratch.primitiveCursor == storage + 1 + kPacketWords &&
      storage[0] == guard && storage[kPacketWords + 1] == guard;
  int saw_dialogue = 0;
  int visited = 0;
  void* packet = ordering + kOrderingEntries - 1;
  const int expected_packets = kOrderingEntries + kFeedbackSpriteCount + 3;
  while (packet != NULL && visited < expected_packets) {
    saw_dialogue |= packet == dialogue;
    ++visited;
    packet = nextPrim(packet);
  }
  passed =
      passed && saw_dialogue && packet == NULL && visited == expected_packets;
  Render_frameScratch = saved_frame;
  Render_frameBuf = saved_framebuffer;
  Effects_screenEffectOverlay = saved_effect;
  return passed;
}

int CheckMenuRowNativeLinks(int scrolling) {
  enum {
    kOrderingWords = 16,
    kPacketWords = 256,
    kRowPacketWords = 21,
    kScrollingPacketWords = 238,
    kScrollingPackets = 38
  };
  const u_long guard = 0xDEADBEEF;
  const Render_FrameScratch saved_frame = Render_frameScratch;
  const int saved_color = Render_fontColor;
  const char saved_palette = Render_fontPaletteOffset;
  const char saved_table = Render_fontTableSelect;
  const char saved_width = Render_variableFontGlyphWidths[5];
  Menu_Framework_MenuRow* saved_items = Menu_Framework_menuItems;
  Menu_Framework_MenuRow rows[40] = {0};
  u_long overlay[kOrderingWords];
  u_long scene[kOrderingWords];
  u_long expected_scene[kOrderingWords];
  u_long storage[kPacketWords];
  Menu_Framework_MenuRow row = {0};
  row.state = 1;
  row.x = 20;
  row.y = 30;
  row.w = 64;
  row.text[0] = 5;
  row.text[1] = (char)0xFF;
  row.isScrollable = 1;
  ClearOTagR(overlay, kOrderingWords);
  ClearOTagR(scene, kOrderingWords);
  memcpy(expected_scene, scene, sizeof scene);
  for (int index = 0; index < kPacketWords; ++index) {
    storage[index] = guard;
  }
  Render_frameScratch.overlayOrderingTable = overlay + 4;
  Render_frameScratch.orderingTable = scene + 4;
  Render_frameScratch.primitiveCursor = storage + 1;
  Render_variableFontGlyphWidths[5] = 7;
  Menu_Framework_menuItems = rows;

  if (scrolling) {
    Menu_Framework_DrawScrollableRow(&row);
  } else {
    Menu_Framework_DrawMenuItem(&row);
  }

  u_long* glyph = storage + 1;
  u_long* shadow = glyph + 6;
  u_long* gradient = shadow + 4;
  int passed =
      nextPrim(overlay + 6) == gradient && nextPrim(gradient) == glyph &&
      nextPrim(glyph) == overlay + 5 && nextPrim(overlay + 7) == shadow &&
      Render_frameScratch.primitiveCursor == storage + 1 + kRowPacketWords &&
      storage[0] == guard && storage[1 + kRowPacketWords] == guard &&
      memcmp(scene, expected_scene, sizeof scene) == 0;
  if (scrolling) {
    int count = 0;
    void* packet = overlay + kOrderingWords - 1;
    const int expected_packets = kOrderingWords + kScrollingPackets;
    while (packet != NULL && count <= expected_packets) {
      ++count;
      packet = nextPrim(packet);
    }
    passed = count == expected_packets && packet == NULL &&
             Render_frameScratch.primitiveCursor ==
                 storage + 1 + kScrollingPacketWords &&
             storage[0] == guard &&
             storage[1 + kScrollingPacketWords] == guard &&
             memcmp(scene, expected_scene, sizeof scene) == 0;
  }
  Render_frameScratch = saved_frame;
  Render_fontColor = saved_color;
  Render_fontPaletteOffset = saved_palette;
  Render_fontTableSelect = saved_table;
  Render_variableFontGlyphWidths[5] = saved_width;
  Menu_Framework_menuItems = saved_items;
  return passed;
}

int CheckMenuWorkspaceReset(void) {
  enum { kRows = 40, kScratchBytes = 0x124, kGuardBytes = 16 };
  const unsigned char guard = 0xA5;
  const size_t workspace_bytes =
      sizeof(Menu_Framework_MenuRow) * kRows + kScratchBytes;
  const size_t allocation_bytes = workspace_bytes + (size_t)kGuardBytes * 2;
  unsigned char* storage = malloc(allocation_bytes);
  if (storage == NULL) {
    return 0;
  }
  memset(storage, guard, allocation_bytes);
  Menu_Framework_MenuRow* saved_items = Menu_Framework_menuItems;
  const u_short saved_flags = Input_settings.menuFlags;
  const unsigned int saved_memory = Input_settings.cursorMemory;
  const char saved_selection = Menu_Framework_submenuStates[0];
  const int saved_cursor = Menu_Framework_mainMenuCursorAnimation;
  Menu_Framework_menuItems = (void*)(storage + kGuardBytes);
  Input_settings.menuFlags = 1;
  Input_settings.cursorMemory = 0;
  Menu_Framework_submenuStates[0] = 0;

  int passed = Menu_Framework_UpdateMainMenuSelection(2) == 0 &&
               Menu_Framework_menuItems[kRows - 1].subText == NULL;
  const unsigned char* tail = (const void*)(Menu_Framework_menuItems + kRows);
  for (int index = 0; index < kScratchBytes; ++index) {
    passed = passed && tail[index] == 0;
  }
  for (int index = 0; index < kGuardBytes; ++index) {
    passed = passed && storage[index] == guard &&
             storage[kGuardBytes + workspace_bytes + index] == guard;
  }
  Menu_Framework_menuItems = saved_items;
  Input_settings.menuFlags = saved_flags;
  Input_settings.cursorMemory = saved_memory;
  Menu_Framework_submenuStates[0] = saved_selection;
  Menu_Framework_mainMenuCursorAnimation = saved_cursor;
  free(storage);
  return passed;
}

#undef CHECK_GIM
