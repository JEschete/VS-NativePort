extern "C" {
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C++" {
#include "Render/EncodedTextLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/ScaledFont.hpp"
#include "Render/ScaledFontLive.hpp"
#include "Render/UiPacketsLive.hpp"
}
#include "Actors/Spawning.h"
#include "Battle/Targeting.h"
#include "Boot/OverlayReset.h"
#include "Content/StringTables.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Effects/ScreenEffects.h"
#include "Event/ScriptVm.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"

_Static_assert(sizeof Menu_textBoxOrderingTables == 102 * sizeof(u_long),
               "three textbox ordering buffers");

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
  char* end = VS::Render::FormatLiveEncodedText(output + 1, format);
  int passed = end == output + sizeof expected &&
               memcmp(output + 1, expected, sizeof expected) == 0 &&
               output[0] == 0x55 && output[sizeof expected + 1] == 0x55;
  // A destination too small for the expanded text is refused, left empty and
  // reported.
  char small[4];
  memset(small, 0x55, sizeof small);
  const uint64_t refused = VS::Render::RefusedLiveEncodedTextCount();
  passed =
      passed && VS::Render::FormatLiveEncodedText(small, format) == small &&
      (unsigned char)small[0] == Core_TextCode_Terminator && small[1] == 0x55 &&
      VS::Render::RefusedLiveEncodedTextCount() == refused + 1;
  Battle_stringContext = saved;
  return passed;
}

int Tests_CompactTextAndGaugeLinks(void) {
  u_long storage[64] = {0};
  u_long ordering = 0xFFFFFF;
  const Render_FrameScratch saved = Render_frameScratch;
  const unsigned char savedWidth = Render_compactGlyphWidths[16];
  const unsigned char savedIndex = Render_compactGlyphIndices[33];
  Render_compactGlyphWidths[16] = 0x60;
  Render_compactGlyphIndices[33] = 0;
  // The Live queue reserves its packets inside the current frame's buffer.
  void* const saved_buffer = Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = storage;
  Render_frameScratch.primitiveCursor = storage;
  VS::Render::DrawLiveCompactTextColor("A", 0, 0x808080, &ordering);
  // The glyph is stored first; the draw mode stored after it leads the
  // chain, and the glyph ends it.
  int passed = nextPrim(&ordering) == storage + 5 && storage[6] == 0xE100000C &&
               nextPrim(storage + 5) == storage &&
               (storage[0] & 0xFFFFFF) == 0xFFFFFF &&
               LegacyGpu_WalkList(&ordering) == 3;
  ordering = 0xFFFFFF;
  Render_frameScratch.primitiveCursor = storage;
  VS::Render::DrawLiveStatGauge(0, 32, &ordering, 0);
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = saved_buffer;
  passed = passed && nextPrim(&ordering) == storage && (ordering >> 24) == 0 &&
           LegacyGpu_WalkList(&ordering) == 2;
  Render_compactGlyphWidths[16] = savedWidth;
  Render_compactGlyphIndices[33] = savedIndex;
  Render_frameScratch = saved;
  return passed;
}

int Tests_ScaledDialogueGlyphs(void) {
  unsigned char* memory = (unsigned char*)(malloc(0x20010));
  if (memory == nullptr) return 0;
  memset(memory, 0xA5, 0x20010);
  const Render_FrameScratch savedFrame = Render_frameScratch;
  void* savedBuffer = Render_legacyPrimitiveBuffers[0];
  const int savedFrameIndex = Render_frameBuf;
  u_long* savedOrdering = Menu_activeTextBoxOrderingTable;
  const VS::Render::FontStyle saved_style = VS::Render::LiveFontStyle();
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
  VS::Render::LiveFontStyle() = {
      .table_select = 0, .palette_offset = 0, .color = 0x2C808080};
  Render_variableFontGlyphWidths[5] = 7;
  Render_variableFontGlyphWidths[0x8F] = 6;
  // Box 2's glyphs go in front of the last of its four layers.
  VS::Render::LiveScaledGlyphTarget target(&ordering[11]);
  const VS::Render::ScaledFontStyle style{
      .glyph_widths = std::as_bytes(std::span(Render_variableFontGlyphWidths)),
      .table_select = 0,
      .palette_offset = 0,
      .color = 0x2C808080};
  int next_x = VS::Render::DrawScaledFontChar(5, 10 << 16, 20 << 16, 12 << 16,
                                              12 << 16, 1 << 16, style, target);
  POLY_FT4* glyph = reinterpret_cast<POLY_FT4*>((memory + 8));
  int passed = next_x == 17 << 16 && glyph->x0 == 10 && glyph->y0 == 20 &&
               glyph->x3 == 22 && glyph->y3 == 32 && glyph->u0 == 60 &&
               glyph->v0 == 0 && glyph->u3 == 72 && glyph->v3 == 12 &&
               glyph->clut == 0x37B4 && glyph->tpage == 13 &&
               nextPrim(&ordering[11]) == glyph;
  void* end = Render_frameScratch.primitiveCursor;
  passed = passed &&
           VS::Render::DrawScaledFontChar(0x8F, next_x, 0, 0, 0, 1 << 16, style,
                                          target) == 23 << 16 &&
           Render_frameScratch.primitiveCursor == end;
  // The text box drawers draw only the dialogue text boxes.
  Menu_TextBoxState& box = Menu_textBoxes[2];
  memset(&box, 0, sizeof box);
  char text[] = {5, static_cast<char>(0x8F), 5, static_cast<char>(0xE8),
                 5, static_cast<char>(0xE7)};
  box.flags.kind = 4;
  box.string = text;
  passed = passed && VS::Render::MeasureLiveEncodedTextLine(text) == 3;
  box.speed = 1;
  box.brightness = 128;
  box.frameX = 10;
  box.frameY = 20;
  Render_frameScratch.primitiveCursor = memory + 8;
  ClearOTag(ordering, 34);
  VS::Render::DrawLiveScaledTextBoxText(&box, 1 << 16);
  passed = passed && Render_frameScratch.primitiveCursor == glyph + 3 &&
           glyph[1].x0 == 23 && glyph[2].x0 == 10 && glyph[2].y0 == 33 &&
           memory[0] == 0xA5 && memory[0x2000F] == 0xA5;
  Render_frameScratch.primitiveCursor = memory + 8;
  ClearOTag(ordering, 34);
  box.speed = 100;
  box.opaque2C = 1;
  VS::Render::DrawLiveProgressiveTextBoxText(&box);
  passed = passed && box.flags.done && box.prevChunksPrinted == 4 &&
           Render_frameScratch.primitiveCursor ==
               memory + 8 + 3 * 6 * sizeof(u_long);
  unsigned shape[] = {0x01040004, 0x00FF0000, 0xFFFFFF00, 0x03020100};
  Event_textBoxFrameStyles[0] = reinterpret_cast<u_char*>(shape);
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
           nextPrim(&ordering[9]) != nullptr &&
           nextPrim(&ordering[10]) != nullptr &&
           LegacyGpu_WalkList(ordering) > 34;
  Menu_textBoxes[2] = savedBox;
  Event_textBoxFrameStyles[0] = savedShape;
  Render_frameScratch = savedFrame;
  Render_legacyPrimitiveBuffers[0] = savedBuffer;
  Render_frameBuf = savedFrameIndex;
  Menu_activeTextBoxOrderingTable = savedOrdering;
  VS::Render::LiveFontStyle() = saved_style;
  Render_variableFontGlyphWidths[5] = savedWidth;
  Render_variableFontGlyphWidths[0x8F] = savedSpaceWidth;
  free(memory);
  return passed;
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
  void* const saved_buffer = Render_legacyPrimitiveBuffers[framebuffer & 1];
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
  Render_legacyPrimitiveBuffers[framebuffer & 1] = storage + 1;

  Effects_ScreenDistortion_DrawFeedbackTiles();

  u_long* feedback =
      storage + 1 +
      static_cast<size_t>(kFeedbackSpriteCount) * kSpritePacketWords;
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
  while (packet != nullptr && visited < expected_packets) {
    saw_dialogue |= packet == dialogue;
    ++visited;
    packet = nextPrim(packet);
  }
  passed = passed && saw_dialogue && packet == nullptr &&
           visited == expected_packets;
  Render_frameScratch = saved_frame;
  Render_legacyPrimitiveBuffers[framebuffer & 1] = saved_buffer;
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
  const int saved_framebuffer = Render_frameBuf;
  void* const saved_buffer = Render_legacyPrimitiveBuffers[0];
  const VS::Render::FontStyle saved_style = VS::Render::LiveFontStyle();
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
  row.text[1] = static_cast<char>(0xFF);
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
  Render_frameBuf = 0;
  Render_legacyPrimitiveBuffers[0] = storage + 1;
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
    while (packet != nullptr && count <= expected_packets) {
      ++count;
      packet = nextPrim(packet);
    }
    passed = count == expected_packets && packet == nullptr &&
             Render_frameScratch.primitiveCursor ==
                 storage + 1 + kScrollingPacketWords &&
             storage[0] == guard &&
             storage[1 + kScrollingPacketWords] == guard &&
             memcmp(scene, expected_scene, sizeof scene) == 0;
  }
  Render_frameScratch = saved_frame;
  Render_frameBuf = saved_framebuffer;
  Render_legacyPrimitiveBuffers[0] = saved_buffer;
  VS::Render::LiveFontStyle() = saved_style;
  Render_variableFontGlyphWidths[5] = saved_width;
  Menu_Framework_menuItems = saved_items;
  return passed;
}

int CheckMenuWorkspaceReset(void) {
  enum { kRows = 40, kScratchBytes = 0x124, kGuardBytes = 16 };
  const unsigned char guard = 0xA5;
  const size_t workspace_bytes =
      sizeof(Menu_Framework_MenuRow) * kRows + kScratchBytes;
  const size_t allocation_bytes =
      workspace_bytes + static_cast<size_t>(kGuardBytes) * 2;
  unsigned char* storage = (unsigned char*)(malloc(allocation_bytes));
  if (storage == nullptr) {
    return 0;
  }
  memset(storage, guard, allocation_bytes);
  Menu_Framework_MenuRow* saved_items = Menu_Framework_menuItems;
  const u_short saved_flags = Input_settings.menuFlags;
  const unsigned int saved_memory = Input_settings.cursorMemory;
  const char saved_selection = Menu_Framework_submenuStates[0];
  const int saved_cursor = Menu_Framework_mainMenuCursorAnimation;
  Menu_Framework_menuItems =
      reinterpret_cast<Menu_Framework_MenuRow*>((storage + kGuardBytes));
  Input_settings.menuFlags = 1;
  Input_settings.cursorMemory = 0;
  Menu_Framework_submenuStates[0] = 0;

  int passed = Menu_Framework_UpdateMainMenuSelection(2) == 0 &&
               Menu_Framework_menuItems[kRows - 1].subText == nullptr;
  const unsigned char* tail =
      reinterpret_cast<const unsigned char*>(Menu_Framework_menuItems + kRows);
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

int CheckMenuReloadClosingDelay(void) {
  enum { kRows = 40, kVisits = 3, kClosingTicks = 5 };
  const Menu_Framework_MenuControlState saved_state = Menu_Framework_menuState;
  Menu_Framework_MenuRow* saved_items = Menu_Framework_menuItems;
  u_int* saved_background = Menu_Framework_menuBackgroundBuffer;
  const signed char saved_loaded = Menu_Framework_loadedSubMenu;
  const char saved_shortcut = Menu_Framework_shortcutInvoked;
  const char saved_without_main = Menu_Framework_openedWithoutMainMenu;
  Menu_Framework_MenuRow rows[kRows] = {0};
  // The overlay read is a queue slot, as the menu's own reads are.
  Content_DiscQueueSlot* const request = &Content_discRequestSlots[31];
  const Content_DiscQueueSlot saved_slot = *request;
  int passed = 1;
  Boot_ResetMainMenuStatics();
  Menu_Framework_menuItems = rows;
  Menu_Framework_menuBackgroundBuffer = nullptr;
  Menu_Framework_loadedSubMenu = 0;
  Menu_Framework_shortcutInvoked = 0;
  Menu_Framework_openedWithoutMainMenu = 1;
  Menu_Framework_menuState.cdQueue = request;

  for (int visit = 0; visit < kVisits; ++visit) {
    request->state = Content_DiscRequestState_Loaded;
    Menu_Framework_menuState.loading = 1;
    passed &= Menu_Framework_LoadMenuOverlay(0) == 1 &&
              request->state == Content_DiscRequestState_Free &&
              Menu_Framework_menuState.loading == 0;
    for (int tick = 1; tick <= kClosingTicks; ++tick) {
      passed &= Menu_Framework_LoadMenuOverlay(0) == 1;
      passed &= Menu_Framework_QuitDelay() == (tick == kClosingTicks);
    }
  }

  Boot_ResetMainMenuStatics();
  Menu_Framework_menuState = saved_state;
  Menu_Framework_menuItems = saved_items;
  Menu_Framework_menuBackgroundBuffer = saved_background;
  Menu_Framework_loadedSubMenu = saved_loaded;
  Menu_Framework_shortcutInvoked = saved_shortcut;
  Menu_Framework_openedWithoutMainMenu = saved_without_main;
  *request = saved_slot;
  return passed;
}

int CheckMenuDiscStringsLoaded(void) {
  const unsigned char* format = reinterpret_cast<const unsigned char*>(
      &Menu_Status_statusStrings[Text_Menu4_status_Offset_values]);
  const size_t status_words =
      sizeof Menu_Status_statusStrings / sizeof Menu_Status_statusStrings[0];
  const size_t inventory_words =
      sizeof Menu_Inventory_menuText / sizeof Menu_Inventory_menuText[0];
  const unsigned int condition =
      Menu_Status_statusStrings[Text_Menu4_status_Index_critical];
  return status_words > Text_Menu4_status_Offset_values && condition > 0 &&
         condition < status_words && Menu_Inventory_menuText[0] > 0 &&
         Menu_Inventory_menuText[0] < inventory_words &&
         format[0] == Core_TextCode_PrintString && format[1] == 0 &&
         format[5] == Core_TextCode_PrintString && format[6] == 1 &&
         format[7] == Core_TextCode_Terminator;
}

int CheckStatusPacketLinks(int background_first) {
  enum {
    kOrderingWords = 0x822,
    kPacketWords = 64,
    kWrittenWords = 53,
    kInsertedPackets = 5
  };
  const u_long guard = 0xDEADBEEF;
  const Render_FrameScratch saved_frame = Render_frameScratch;
  const char saved_background = Menu_Status_drawBackgroundFirst;
  u_long ordering[kOrderingWords];
  u_long storage[kPacketWords];
  ClearOTagR(ordering, kOrderingWords);
  for (int index = 0; index < kPacketWords; ++index) {
    storage[index] = guard;
  }
  Render_frameScratch.primitiveCursor = storage + 1;
  Render_frameScratch.orderingTable = ordering + 18;
  Render_frameScratch.overlayOrderingTable = ordering + 2;
  Menu_Status_drawBackgroundFirst = background_first;

  Menu_Status_DrawBackground();
  Menu_Status_DrawLimbConditionIndicator(0, 0, 0);
  Menu_Status_DrawLimbLeaderLine(0, 0, 0, 0);

  int count = 0;
  void* packet = ordering + kOrderingWords - 1;
  const int expected_packets = kOrderingWords + kInsertedPackets;
  while (packet != nullptr && count <= expected_packets) {
    ++count;
    packet = nextPrim(packet);
  }
  int passed =
      packet == nullptr && count == expected_packets &&
      Render_frameScratch.primitiveCursor == storage + 1 + kWrittenWords &&
      storage[0] == guard && storage[1 + kWrittenWords] == guard;
  Menu_Status_drawBackgroundFirst = saved_background;
  Render_frameScratch = saved_frame;
  return passed;
}

int CheckMenuPromptPanelLinks(void) {
  enum { kOrderingWords = 0x822, kPacketWords = 256, kAnimationFrames = 8 };
  const u_long guard = 0xDEADBEEF;
  const Render_FrameScratch saved_frame = Render_frameScratch;
  const char saved_display = Menu_Framework_displayActionCost;
  Menu_Framework_TextHeader saved_headers[2];
  memcpy(saved_headers, Menu_Framework_textHeaders, sizeof saved_headers);
  u_long ordering[kOrderingWords];
  u_long storage[kPacketWords];
  ClearOTagR(ordering, kOrderingWords);
  for (int index = 0; index < kPacketWords; ++index) {
    storage[index] = guard;
  }
  Render_frameScratch.primitiveCursor = storage + 1;
  Render_frameScratch.orderingTable = ordering + 18;
  Render_frameScratch.overlayOrderingTable = ordering + 2;
  Menu_Framework_displayActionCost = 1;
  memset(Menu_Framework_textHeaders, 0, sizeof saved_headers);
  // The typed HUD packets reserve their words in the current frame's buffer.
  void* const saved_buffer = Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = storage + 1;

  Menu_Framework_DrawButtonPromptBackground(20, 160, 80, 3);
  for (int frame = 0; frame < kAnimationFrames; ++frame) {
    Menu_Framework_DrawActionCostPanel();
  }

  int count = 0;
  void* packet = ordering + kOrderingWords - 1;
  while (packet != nullptr && count < kOrderingWords + kPacketWords) {
    ++count;
    packet = nextPrim(packet);
  }
  const u_long* end = (const u_long*)(Render_frameScratch.primitiveCursor);
  int passed = packet == nullptr && count >= kOrderingWords + 5 &&
               nextPrim(ordering + 15) == storage + 15 && end > storage + 1 &&
               end < storage + kPacketWords && storage[0] == guard &&
               *end == guard;
  memcpy(Menu_Framework_textHeaders, saved_headers, sizeof saved_headers);
  Menu_Framework_displayActionCost = saved_display;
  Render_frameScratch = saved_frame;
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = saved_buffer;
  return passed;
}

int CheckDefaultLoadoutNames(void) {
  const unsigned char actor_name[] = {0x0A,
                                      0x36,
                                      0x2B,
                                      0x2F,
                                      0x28,
                                      0x3C,
                                      Core_TextCode_Spacing,
                                      6,
                                      0x1B,
                                      0x2C,
                                      0x32,
                                      0x37,
                                      Core_TextCode_Terminator};
  const unsigned char weapon_name[] = {
      0x0F, 0x24, 0x31, 0x27, 0x24, 0x31, 0x2A, 0x32, Core_TextCode_Terminator};
  return memcmp(Actors_ashleyInitialLoadout.name, actor_name,
                sizeof actor_name) == 0 &&
         memcmp(Actors_ashleyInitialLoadout.weapon.name, weapon_name,
                sizeof weapon_name) == 0;
}

}  // extern "C"
