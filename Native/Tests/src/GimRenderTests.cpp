#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <array>
#include <cstddef>

#include "Actors/Spawning.h"
#include "Battle/TargetingState.h"
#include "Boot/OverlayReset.h"
#include "Content/DiscLayout.h"
#include "Content/StringTables.h"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Text/TextCode.hpp"
#include "DiscQueue.h"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffects.h"
#include "Event/ScriptVm.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Status/ActorStatus.h"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLive.hpp"
#include "Menu/TextBox.h"
#include "Menu/TextBoxFramesLive.hpp"
#include "Render/Display.h"
#include "Render/EncodedTextLive.hpp"
#include "Render/Frame.h"
#include "Render/GameFontLive.hpp"
#include "Render/Render.h"
#include "Render/ScaledFont.hpp"
#include "Render/ScaledFontLive.hpp"
#include "Render/Text/CompactFont.h"
#include "Render/Text/VariableFont.h"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"

static_assert(sizeof VS::Menu::textBoxOrderingTables == 102 * sizeof(u_long),
              "three textbox ordering buffers");

int Tests_EncodedTextFormatting(void) {
  const Menu_StringContext saved = Battle_stringContext;
  char nested[] = {10, 0, VS::Core::TextCode::kPrintDecimal, 1,
                   VS::Core::TextCode::kTerminator};
  char format[] = {
      VS::Core::TextCode::kPrintDecimal, 0, 0x8F,
      VS::Core::TextCode::kPrintString,  0, 0x8F,
      VS::Core::TextCode::kPrintHex,     2, VS::Core::TextCode::kTerminator,
      VS::Core::TextCode::kPrintString,  0};
  const unsigned char expected[] = {
      0xA7, 1, 0, 5, 0x8F, 10, 0, 7, 0x8F, 0, VS::Core::TextCode::kTerminator};
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
      static_cast<unsigned char>(small[0]) == VS::Core::TextCode::kTerminator &&
      small[1] == 0x55 &&
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

static bool DrawTrackedFrameFixture(int index, const void* shape,
                                    std::size_t shape_bytes) {
  constexpr std::size_t kFrameArenaBytes = 4096;
  std::array<std::byte, kFrameArenaBytes> frame_arena{};
  VS::Core::Memory::ScopedLiveGameHeap frame_heap(frame_arena);
  void* const source = VS::Core::Memory::AllocateLiveFromHighEnd(shape_bytes);
  if (source == nullptr) {
    return false;
  }
  memcpy(source, shape, shape_bytes);
  Event_textBoxFrameStyles[0] = static_cast<u_char*>(source);
  VS::Menu::DrawLiveTextBoxFrame(index);
  VS::Core::Memory::FreeLiveFromHighEnd(source);
  return true;
}

int Tests_ScaledDialogueGlyphs(void) {
  unsigned char* memory = static_cast<unsigned char*>(malloc(0x20010));
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
  memset(&Menu_textBoxes[2], 0, sizeof Menu_textBoxes[2]);
  Menu_textBoxes[2].frameX = 10;
  Menu_textBoxes[2].frameY = 20;
  Menu_textBoxes[2].frameWidth = 64;
  Menu_textBoxes[2].frameHeight = 64;
  Menu_textBoxes[2].brightness = 128;
  Render_frameScratch.primitiveCursor = memory + 8;
  ClearOTag(ordering, 34);
  passed = DrawTrackedFrameFixture(2, shape, sizeof shape) && passed;
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
  void* const saved_ordering = Render_legacyOrderingTables[framebuffer & 1];
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
  Render_legacyOrderingTables[framebuffer & 1] = ordering;

  VS::Effects::ScreenDistortion::DrawLiveFeedbackTiles();

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
  Render_legacyOrderingTables[framebuffer & 1] = saved_ordering;
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
    kScrollingPackets = 38,
    kFrameTableWords = 0x822
  };
  const u_long guard = 0xDEADBEEF;
  const Render_FrameScratch saved_frame = Render_frameScratch;
  const int saved_framebuffer = Render_frameBuf;
  void* const saved_buffer = Render_legacyPrimitiveBuffers[0];
  void* const saved_table = Render_legacyOrderingTables[0];
  const VS::Render::FontStyle saved_style = VS::Render::LiveFontStyle();
  const char saved_width = Render_variableFontGlyphWidths[5];
  Menu_Framework_MenuRow* saved_items = VS::Menu::Framework::menuItems;
  Menu_Framework_MenuRow rows[40] = {0};
  // The Live menu links only into the current frame's ordering table, which
  // holds the overlay and scene tables as in the game loop.
  u_long frame_table[kFrameTableWords];
  u_long* const overlay = frame_table;
  u_long* const scene = frame_table + kOrderingWords;
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
  memcpy(expected_scene, scene, sizeof expected_scene);
  for (int index = 0; index < kPacketWords; ++index) {
    storage[index] = guard;
  }
  Render_frameScratch.overlayOrderingTable = overlay + 4;
  Render_frameScratch.orderingTable = scene + 4;
  Render_frameScratch.primitiveCursor = storage + 1;
  Render_frameBuf = 0;
  Render_legacyPrimitiveBuffers[0] = storage + 1;
  Render_legacyOrderingTables[0] = frame_table;
  Render_variableFontGlyphWidths[5] = 7;
  VS::Menu::Framework::menuItems = rows;

  if (scrolling) {
    VS::Menu::Framework::DrawLiveScrollableRow(&row);
  } else {
    VS::Menu::Framework::DrawLiveMenuItem(&row);
  }

  u_long* glyph = storage + 1;
  u_long* shadow = glyph + 6;
  u_long* gradient = shadow + 4;
  int passed =
      nextPrim(overlay + 6) == gradient && nextPrim(gradient) == glyph &&
      nextPrim(glyph) == overlay + 5 && nextPrim(overlay + 7) == shadow &&
      Render_frameScratch.primitiveCursor == storage + 1 + kRowPacketWords &&
      storage[0] == guard && storage[1 + kRowPacketWords] == guard &&
      memcmp(scene, expected_scene, sizeof expected_scene) == 0;
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
             memcmp(scene, expected_scene, sizeof expected_scene) == 0;
  }
  Render_frameScratch = saved_frame;
  Render_frameBuf = saved_framebuffer;
  Render_legacyPrimitiveBuffers[0] = saved_buffer;
  Render_legacyOrderingTables[0] = saved_table;
  VS::Render::LiveFontStyle() = saved_style;
  Render_variableFontGlyphWidths[5] = saved_width;
  VS::Menu::Framework::menuItems = saved_items;
  return passed;
}

int CheckMenuWorkspaceReset(void) {
  enum {
    kRows = 40,
    kScratchBytes = 0x124,
    kStringBytes = 0x60,
    kGuardBytes = 16
  };
  const unsigned char guard = 0xA5;
  const size_t workspace_bytes =
      sizeof(Menu_Framework_MenuRow) * kRows + kScratchBytes;
  const size_t allocation_bytes =
      workspace_bytes + static_cast<size_t>(kGuardBytes) * 2;
  unsigned char* storage =
      static_cast<unsigned char*>(malloc(allocation_bytes));
  if (storage == nullptr) {
    return 0;
  }
  memset(storage, guard, allocation_bytes);
  Menu_Framework_MenuRow* saved_items = VS::Menu::Framework::menuItems;
  char* const saved_strings = VS::Menu::Framework::stringBuf;
  int* const saved_types = VS::Menu::Framework::rowTypeBuf;
  const u_short saved_flags = VS::Input::settings.menuFlags;
  const unsigned int saved_memory = VS::Input::settings.cursorMemory;
  const char saved_selection = VS::Menu::Framework::submenuStates[0];
  const int saved_cursor = VS::Menu::Framework::mainMenuCursorAnimation;
  VS::Menu::Framework::menuItems =
      reinterpret_cast<Menu_Framework_MenuRow*>((storage + kGuardBytes));
  // The workspace layout the menu's allocation sets: the string buffer, then
  // the row types, after the rows.
  VS::Menu::Framework::stringBuf =
      reinterpret_cast<char*>(VS::Menu::Framework::menuItems + kRows);
  VS::Menu::Framework::rowTypeBuf =
      reinterpret_cast<int*>(VS::Menu::Framework::stringBuf + kStringBytes);
  VS::Input::settings.menuFlags = 1;
  VS::Input::settings.cursorMemory = 0;
  VS::Menu::Framework::submenuStates[0] = 0;

  int passed = VS::Menu::Framework::UpdateLiveMainMenuSelection(2) == 0 &&
               VS::Menu::Framework::menuItems[kRows - 1].subText == nullptr;
  const unsigned char* tail = reinterpret_cast<const unsigned char*>(
      VS::Menu::Framework::menuItems + kRows);
  for (int index = 0; index < kScratchBytes; ++index) {
    passed = passed && tail[index] == 0;
  }
  for (int index = 0; index < kGuardBytes; ++index) {
    passed = passed && storage[index] == guard &&
             storage[kGuardBytes + workspace_bytes + index] == guard;
  }
  VS::Menu::Framework::menuItems = saved_items;
  VS::Menu::Framework::stringBuf = saved_strings;
  VS::Menu::Framework::rowTypeBuf = saved_types;
  VS::Input::settings.menuFlags = saved_flags;
  VS::Input::settings.cursorMemory = saved_memory;
  VS::Menu::Framework::submenuStates[0] = saved_selection;
  VS::Menu::Framework::mainMenuCursorAnimation = saved_cursor;
  free(storage);
  return passed;
}

int CheckMenuReloadClosingDelay(void) {
  enum { kRows = 40, kVisits = 3, kClosingTicks = 5 };
  const Menu_Framework_MenuControlState saved_state =
      VS::Menu::Framework::menuState;
  Menu_Framework_MenuRow* saved_items = VS::Menu::Framework::menuItems;
  u_int* saved_background = VS::Menu::Framework::menuBackgroundBuffer;
  const signed char saved_loaded = VS::Menu::Framework::loadedSubMenu;
  const char saved_shortcut = VS::Menu::Framework::shortcutInvoked;
  const char saved_without_main = VS::Menu::Framework::openedWithoutMainMenu;
  Menu_Framework_MenuRow rows[kRows] = {0};
  // The overlay read is a queue slot, as the menu's own reads are.
  Content_DiscQueueSlot* const request = &Content_discRequestSlots[31];
  const Content_DiscQueueSlot saved_slot = *request;
  int passed = 1;
  Boot_ResetMainMenuStatics();
  VS::Menu::Framework::ResetLiveMainMenuStatics();
  VS::Menu::Framework::menuItems = rows;
  VS::Menu::Framework::menuBackgroundBuffer = nullptr;
  VS::Menu::Framework::loadedSubMenu = 0;
  VS::Menu::Framework::shortcutInvoked = 0;
  VS::Menu::Framework::openedWithoutMainMenu = 1;
  VS::Menu::Framework::menuState.cdQueue = request;

  for (int visit = 0; visit < kVisits; ++visit) {
    request->state = Content_DiscRequestState_Loaded;
    request->cdFile = {VS_MAINMENU_PRG_LBA, VS_MAINMENU_PRG_SIZE};
    VS::Menu::Framework::menuState.loading = 1;
    passed &= VS::Menu::Framework::LoadLiveMenuOverlay(0) == 1 &&
              request->state == Content_DiscRequestState_Free &&
              VS::Menu::Framework::menuState.loading == 0;
    for (int tick = 1; tick <= kClosingTicks; ++tick) {
      passed &= VS::Menu::Framework::LoadLiveMenuOverlay(0) == 1;
      passed &= VS::Menu::Framework::LiveQuitDelay() == (tick == kClosingTicks);
    }
  }

  Boot_ResetMainMenuStatics();
  VS::Menu::Framework::ResetLiveMainMenuStatics();
  VS::Menu::Framework::menuState = saved_state;
  VS::Menu::Framework::menuItems = saved_items;
  VS::Menu::Framework::menuBackgroundBuffer = saved_background;
  VS::Menu::Framework::loadedSubMenu = saved_loaded;
  VS::Menu::Framework::shortcutInvoked = saved_shortcut;
  VS::Menu::Framework::openedWithoutMainMenu = saved_without_main;
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
         format[0] == VS::Core::TextCode::kPrintString && format[1] == 0 &&
         format[5] == VS::Core::TextCode::kPrintString && format[6] == 1 &&
         format[7] == VS::Core::TextCode::kTerminator;
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
  void* const saved_buffer = Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  void* const saved_table = Render_legacyOrderingTables[Render_frameBuf & 1];
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = storage + 1;
  Render_legacyOrderingTables[Render_frameBuf & 1] = ordering;

  VS::Menu::Status::LiveDrawBackground();
  VS::Menu::Status::LiveDrawLimbConditionIndicator(0, 0, 0);
  VS::Menu::Status::LiveDrawLimbLeaderLine(0, 0, 0, 0);

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
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = saved_buffer;
  Render_legacyOrderingTables[Render_frameBuf & 1] = saved_table;
  Render_frameScratch = saved_frame;
  return passed;
}

int CheckMenuPromptPanelLinks(void) {
  enum { kOrderingWords = 0x822, kPacketWords = 256, kAnimationFrames = 8 };
  const u_long guard = 0xDEADBEEF;
  const Render_FrameScratch saved_frame = Render_frameScratch;
  const char saved_display = VS::Menu::Framework::displayActionCost;
  Menu_Framework_TextHeader saved_headers[2];
  memcpy(saved_headers, VS::Menu::Framework::textHeaders, sizeof saved_headers);
  u_long ordering[kOrderingWords];
  u_long storage[kPacketWords];
  ClearOTagR(ordering, kOrderingWords);
  for (int index = 0; index < kPacketWords; ++index) {
    storage[index] = guard;
  }
  Render_frameScratch.primitiveCursor = storage + 1;
  Render_frameScratch.orderingTable = ordering + 18;
  Render_frameScratch.overlayOrderingTable = ordering + 2;
  VS::Menu::Framework::displayActionCost = 1;
  memset(VS::Menu::Framework::textHeaders, 0, sizeof saved_headers);
  // The typed HUD packets reserve their words in the current frame's buffer
  // and link only into the current frame's ordering table.
  void* const saved_buffer = Render_legacyPrimitiveBuffers[Render_frameBuf & 1];
  void* const saved_table = Render_legacyOrderingTables[Render_frameBuf & 1];
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = storage + 1;
  Render_legacyOrderingTables[Render_frameBuf & 1] = ordering;

  VS::Menu::Framework::DrawLiveButtonPromptBackground(20, 160, 80, 3);
  for (int frame = 0; frame < kAnimationFrames; ++frame) {
    VS::Menu::Framework::DrawLiveActionCostPanel();
  }

  int count = 0;
  void* packet = ordering + kOrderingWords - 1;
  while (packet != nullptr && count < kOrderingWords + kPacketWords) {
    ++count;
    packet = nextPrim(packet);
  }
  const u_long* end =
      static_cast<const u_long*>(Render_frameScratch.primitiveCursor);
  int passed = packet == nullptr && count >= kOrderingWords + 5 &&
               nextPrim(ordering + 15) == storage + 15 && end > storage + 1 &&
               end < storage + kPacketWords && storage[0] == guard &&
               *end == guard;
  memcpy(VS::Menu::Framework::textHeaders, saved_headers, sizeof saved_headers);
  VS::Menu::Framework::displayActionCost = saved_display;
  Render_frameScratch = saved_frame;
  Render_legacyPrimitiveBuffers[Render_frameBuf & 1] = saved_buffer;
  Render_legacyOrderingTables[Render_frameBuf & 1] = saved_table;
  return passed;
}

int CheckDefaultLoadoutNames(void) {
  const unsigned char actor_name[] = {0x0A,
                                      0x36,
                                      0x2B,
                                      0x2F,
                                      0x28,
                                      0x3C,
                                      VS::Core::TextCode::kSpacing,
                                      6,
                                      0x1B,
                                      0x2C,
                                      0x32,
                                      0x37,
                                      VS::Core::TextCode::kTerminator};
  const unsigned char weapon_name[] = {
      0x0F, 0x24, 0x31,
      0x27, 0x24, 0x31,
      0x2A, 0x32, VS::Core::TextCode::kTerminator};
  return memcmp(Actors_ashleyInitialLoadout.name, actor_name,
                sizeof actor_name) == 0 &&
         memcmp(Actors_ashleyInitialLoadout.weapon.name, weapon_name,
                sizeof weapon_name) == 0;
}
