#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Actors/Spawning.hpp"
#include "Battle/TargetingState.hpp"
#include "Boot/OverlayReset.hpp"
#include "Content/DiscLayout.h"
#include "Content/DiscQueue.hpp"
#include "Content/StringTables.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Text/TextCode.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Settings.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/Inventory.hpp"
#include "Menu/Status/ActorStatus.hpp"
#include "Menu/Status/LimbControllersLive.hpp"
#include "Menu/Status/TerminalControllersLive.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxFramesLive.hpp"
#include "Render/Display.hpp"
#include "Render/EncodedTextLive.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/ScaledFont.hpp"
#include "Render/ScaledFontLive.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/Text/VariableFont.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Legacy::AddPrim;
using VS::Legacy::ClearOTag;
using VS::Legacy::ClearOTagR;
using VS::Legacy::GpuWalkList;
using VS::Legacy::nextPrim;
using VS::Legacy::POLY_FT4;
using VS::Legacy::setlen;

namespace menu4_status = VS::Content::text::menu4_prg::status;

static_assert(sizeof VS::Menu::textBoxOrderingTables ==
                  102 * sizeof(std::uint32_t),
              "three textbox ordering buffers");

int Tests_EncodedTextFormatting(void) {
  const VS::Menu::StringContext saved = VS::Battle::stringContext;
  std::uint8_t nested[] = {10, 0, VS::Core::TextCode::kPrintDecimal, 1,
                           VS::Core::TextCode::kTerminator};
  const std::uint8_t format[] = {
      VS::Core::TextCode::kPrintDecimal, 0, 0x8F,
      VS::Core::TextCode::kPrintString,  0, 0x8F,
      VS::Core::TextCode::kPrintHex,     2, VS::Core::TextCode::kTerminator,
      VS::Core::TextCode::kPrintString,  0};
  const unsigned char expected[] = {
      0xA7, 1, 0, 5, 0x8F, 10, 0, 7, 0x8F, 0, VS::Core::TextCode::kTerminator};
  char output[64];
  memset(output, 0x55, sizeof output);
  VS::Battle::stringContext.integers[0] = -105;
  VS::Battle::stringContext.integers[1] = 7;
  VS::Battle::stringContext.integers[2] = 0;
  VS::Battle::stringContext.strings[0] = reinterpret_cast<char*>(nested);
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
  VS::Battle::stringContext = saved;
  return passed;
}

int Tests_CompactTextAndGaugeLinks(void) {
  std::uint32_t storage[64] = {0};
  std::uint32_t ordering = 0xFFFFFF;
  const VS::Render::FrameScratch saved = VS::Render::FrameScratchpad();
  const unsigned char saved_width = VS::Render::compactGlyphWidths[16];
  const unsigned char saved_index = VS::Render::compactGlyphIndices[33];
  VS::Render::compactGlyphWidths[16] = 0x60;
  VS::Render::compactGlyphIndices[33] = 0;
  // The Live queue reserves its packets inside the current frame's buffer.
  void* const saved_buffer =
      VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1];
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] = storage;
  VS::Render::FrameScratchpad().primitive_cursor = storage;
  VS::Render::DrawLiveCompactTextColor("A", 0, 0x808080, &ordering);
  // The glyph is stored first; the draw mode stored after it leads the
  // chain, and the glyph ends it.
  int passed = nextPrim(&ordering) == storage + 5 && storage[6] == 0xE100000C &&
               nextPrim(storage + 5) == storage &&
               (storage[0] & 0xFFFFFF) == 0xFFFFFF &&
               GpuWalkList(&ordering) == 3;
  ordering = 0xFFFFFF;
  VS::Render::FrameScratchpad().primitive_cursor = storage;
  VS::Render::DrawLiveStatGauge(0, 32, &ordering, 0);
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] = saved_buffer;
  passed = passed && nextPrim(&ordering) == storage && (ordering >> 24) == 0 &&
           GpuWalkList(&ordering) == 2;
  VS::Render::compactGlyphWidths[16] = saved_width;
  VS::Render::compactGlyphIndices[33] = saved_index;
  VS::Render::FrameScratchpad() = saved;
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
  VS::Event::textBoxFrameStyles[0] = static_cast<std::uint8_t*>(source);
  VS::Menu::DrawLiveTextBoxFrame(index);
  VS::Core::Memory::FreeLiveFromHighEnd(source);
  return true;
}

int Tests_ScaledDialogueGlyphs(void) {
  unsigned char* memory = static_cast<unsigned char*>(malloc(0x20010));
  if (memory == nullptr) return 0;
  memset(memory, 0xA5, 0x20010);
  const VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  const int saved_frame_index = VS::Render::frameBuf;
  std::uint32_t* saved_ordering = VS::Menu::activeTextBoxOrderingTable;
  const VS::Render::FontStyle saved_style = VS::Render::LiveFontStyle();
  const std::uint8_t saved_width = VS::Render::variableFontGlyphWidths[5];
  const std::uint8_t saved_space_width =
      VS::Render::variableFontGlyphWidths[0x8F];
  const VS::Menu::TextBoxState saved_box = VS::Menu::textBoxes[2];
  std::uint8_t* saved_shape = VS::Event::textBoxFrameStyles[0];
  std::uint32_t ordering[34];
  ClearOTag(ordering, 34);
  VS::Menu::activeTextBoxOrderingTable = ordering;
  VS::Render::legacyPrimitiveBuffers[0] = memory + 8;
  VS::Render::frameBuf = 0;
  VS::Render::FrameScratchpad().primitive_cursor = memory + 8;
  VS::Render::LiveFontStyle() = {
      .table_select = 0, .palette_offset = 0, .color = 0x2C808080};
  VS::Render::variableFontGlyphWidths[5] = 7;
  VS::Render::variableFontGlyphWidths[0x8F] = 6;
  // Box 2's glyphs go in front of the last of its four layers.
  VS::Render::LiveScaledGlyphTarget target(&ordering[11]);
  const VS::Render::ScaledFontStyle style{
      .glyph_widths =
          std::as_bytes(std::span(VS::Render::variableFontGlyphWidths)),
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
  void* end = VS::Render::FrameScratchpad().primitive_cursor;
  passed = passed &&
           VS::Render::DrawScaledFontChar(0x8F, next_x, 0, 0, 0, 1 << 16, style,
                                          target) == 23 << 16 &&
           VS::Render::FrameScratchpad().primitive_cursor == end;
  // The text box drawers draw only the dialogue text boxes.
  VS::Menu::TextBoxState& box = VS::Menu::textBoxes[2];
  memset(&box, 0, sizeof box);
  char text[] = {5, static_cast<char>(0x8F), 5, static_cast<char>(0xE8),
                 5, static_cast<char>(0xE7)};
  box.flags.kind = 4;
  box.text = text;
  passed = passed && VS::Render::MeasureLiveEncodedTextLine(text) == 3;
  box.speed = 1;
  box.brightness = 128;
  box.frame_x = 10;
  box.frame_y = 20;
  VS::Render::FrameScratchpad().primitive_cursor = memory + 8;
  ClearOTag(ordering, 34);
  VS::Render::DrawLiveScaledTextBoxText(&box, 1 << 16);
  passed = passed &&
           VS::Render::FrameScratchpad().primitive_cursor == glyph + 3 &&
           glyph[1].x0 == 23 && glyph[2].x0 == 10 && glyph[2].y0 == 33 &&
           memory[0] == 0xA5 && memory[0x2000F] == 0xA5;
  VS::Render::FrameScratchpad().primitive_cursor = memory + 8;
  ClearOTag(ordering, 34);
  box.speed = 100;
  box.speaker_pause = 1;
  VS::Render::DrawLiveProgressiveTextBoxText(&box);
  passed = passed && box.flags.done && box.previous_chunks == 4 &&
           VS::Render::FrameScratchpad().primitive_cursor ==
               memory + 8 + std::size_t{3} * 6 * sizeof(std::uint32_t);
  unsigned shape[] = {0x01040004, 0x00FF0000, 0xFFFFFF00, 0x03020100};
  memset(&VS::Menu::textBoxes[2], 0, sizeof VS::Menu::textBoxes[2]);
  VS::Menu::textBoxes[2].frame_x = 10;
  VS::Menu::textBoxes[2].frame_y = 20;
  VS::Menu::textBoxes[2].frame_width = 64;
  VS::Menu::textBoxes[2].frame_height = 64;
  VS::Menu::textBoxes[2].brightness = 128;
  VS::Render::FrameScratchpad().primitive_cursor = memory + 8;
  ClearOTag(ordering, 34);
  passed = DrawTrackedFrameFixture(2, shape, sizeof shape) && passed;
  passed = passed && (ordering[9] >> 24) == 0 && (ordering[10] >> 24) == 0 &&
           nextPrim(&ordering[9]) != nullptr &&
           nextPrim(&ordering[10]) != nullptr && GpuWalkList(ordering) > 34;
  VS::Menu::textBoxes[2] = saved_box;
  VS::Event::textBoxFrameStyles[0] = saved_shape;
  VS::Render::FrameScratchpad() = saved_frame;
  VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
  VS::Render::frameBuf = saved_frame_index;
  VS::Menu::activeTextBoxOrderingTable = saved_ordering;
  VS::Render::LiveFontStyle() = saved_style;
  VS::Render::variableFontGlyphWidths[5] = saved_width;
  VS::Render::variableFontGlyphWidths[0x8F] = saved_space_width;
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
  const std::uint32_t guard = 0xDEADBEEF;
  std::uint32_t ordering[kOrderingEntries];
  std::uint32_t storage[kPacketWords + 2];
  std::uint32_t dialogue[] = {0, 0x60000000, 0, 0x00010001};
  const VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  const int saved_framebuffer = VS::Render::frameBuf;
  void* const saved_buffer =
      VS::Render::legacyPrimitiveBuffers[framebuffer & 1];
  void* const saved_ordering =
      VS::Render::legacyOrderingTables[framebuffer & 1];
  VS::Effects::LiveScreenEffectOverlay* saved_effect =
      VS::Effects::screenEffectOverlay;
  VS::Effects::LiveScreenEffectOverlay effect = {};
  effect.feedback_layer = feedback_layer;
  VS::Effects::screenEffectOverlay = &effect;
  VS::Render::frameBuf = framebuffer;
  ClearOTagR(ordering, kOrderingEntries);
  setlen(dialogue, 3);
  AddPrim(ordering + kTextOrderingEntry, dialogue);
  for (int index = 0; index < kPacketWords + 2; ++index) {
    storage[index] = guard;
  }
  VS::Render::FrameScratchpad().ordering_table = ordering + kSceneOrderingEntry;
  VS::Render::FrameScratchpad().primitive_cursor = storage + 1;
  VS::Render::legacyPrimitiveBuffers[framebuffer & 1] = storage + 1;
  VS::Render::legacyOrderingTables[framebuffer & 1] = ordering;

  VS::Effects::ScreenDistortion::DrawLiveFeedbackTiles();

  std::uint32_t* feedback =
      storage + 1 +
      static_cast<size_t>(kFeedbackSpriteCount) * kSpritePacketWords;
  std::uint32_t* restore = feedback + kFeedbackPacketWords;
  const int feedback_entry = kSceneOrderingEntry + feedback_layer - 3;
  int passed = nextPrim(ordering + feedback_entry) == feedback &&
               nextPrim(feedback) == ordering + feedback_entry - 1 &&
               nextPrim(ordering + kSceneOrderingEntry) == restore &&
               nextPrim(restore) == feedback - kSpritePacketWords &&
               VS::Render::FrameScratchpad().primitive_cursor ==
                   storage + 1 + kPacketWords &&
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
  VS::Render::FrameScratchpad() = saved_frame;
  VS::Render::legacyPrimitiveBuffers[framebuffer & 1] = saved_buffer;
  VS::Render::legacyOrderingTables[framebuffer & 1] = saved_ordering;
  VS::Render::frameBuf = saved_framebuffer;
  VS::Effects::screenEffectOverlay = saved_effect;
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
  const std::uint32_t guard = 0xDEADBEEF;
  const VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  const int saved_framebuffer = VS::Render::frameBuf;
  void* const saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  void* const saved_table = VS::Render::legacyOrderingTables[0];
  const VS::Render::FontStyle saved_style = VS::Render::LiveFontStyle();
  const std::uint8_t saved_width = VS::Render::variableFontGlyphWidths[5];
  VS::Menu::Framework::MenuRow* saved_items = VS::Menu::Framework::menuItems;
  VS::Menu::Framework::MenuRow rows[40] = {};
  // The Live menu links only into the current frame's ordering table, which
  // holds the overlay and scene tables as in the game loop.
  std::uint32_t frame_table[kFrameTableWords];
  std::uint32_t* const overlay = frame_table;
  std::uint32_t* const scene = frame_table + kOrderingWords;
  std::uint32_t expected_scene[kOrderingWords];
  std::uint32_t storage[kPacketWords];
  VS::Menu::Framework::MenuRow row = {};
  row.state = VS::Menu::Framework::RowState::kIdle;
  row.x = 20;
  row.y = 30;
  row.w = 64;
  row.text[0] = 5;
  row.text[1] = 0xFF;
  row.is_scrollable = 1;
  ClearOTagR(overlay, kOrderingWords);
  ClearOTagR(scene, kOrderingWords);
  memcpy(expected_scene, scene, sizeof expected_scene);
  for (int index = 0; index < kPacketWords; ++index) {
    storage[index] = guard;
  }
  VS::Render::FrameScratchpad().overlay_ordering_table = overlay + 4;
  VS::Render::FrameScratchpad().ordering_table = scene + 4;
  VS::Render::FrameScratchpad().primitive_cursor = storage + 1;
  VS::Render::frameBuf = 0;
  VS::Render::legacyPrimitiveBuffers[0] = storage + 1;
  VS::Render::legacyOrderingTables[0] = frame_table;
  VS::Render::variableFontGlyphWidths[5] = 7;
  VS::Menu::Framework::menuItems = rows;

  if (scrolling) {
    VS::Menu::Framework::DrawLiveScrollableRow(&row);
  } else {
    VS::Menu::Framework::DrawLiveMenuItem(&row);
  }

  std::uint32_t* glyph = storage + 1;
  std::uint32_t* shadow = glyph + 6;
  std::uint32_t* gradient = shadow + 4;
  int passed = nextPrim(overlay + 6) == gradient &&
               nextPrim(gradient) == glyph && nextPrim(glyph) == overlay + 5 &&
               nextPrim(overlay + 7) == shadow &&
               VS::Render::FrameScratchpad().primitive_cursor ==
                   storage + 1 + kRowPacketWords &&
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
             VS::Render::FrameScratchpad().primitive_cursor ==
                 storage + 1 + kScrollingPacketWords &&
             storage[0] == guard &&
             storage[1 + kScrollingPacketWords] == guard &&
             memcmp(scene, expected_scene, sizeof expected_scene) == 0;
  }
  VS::Render::FrameScratchpad() = saved_frame;
  VS::Render::frameBuf = saved_framebuffer;
  VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
  VS::Render::legacyOrderingTables[0] = saved_table;
  VS::Render::LiveFontStyle() = saved_style;
  VS::Render::variableFontGlyphWidths[5] = saved_width;
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
      sizeof(VS::Menu::Framework::MenuRow) * kRows + kScratchBytes;
  const size_t allocation_bytes =
      workspace_bytes + static_cast<size_t>(kGuardBytes) * 2;
  unsigned char* storage =
      static_cast<unsigned char*>(malloc(allocation_bytes));
  if (storage == nullptr) {
    return 0;
  }
  memset(storage, guard, allocation_bytes);
  VS::Menu::Framework::MenuRow* saved_items = VS::Menu::Framework::menuItems;
  char* const saved_strings = VS::Menu::Framework::stringBuf;
  int* const saved_types = VS::Menu::Framework::rowTypeBuf;
  const std::uint16_t saved_flags = VS::Input::settings.menu_flags;
  const unsigned int saved_memory = VS::Input::settings.cursor_memory;
  const char saved_selection = VS::Menu::Framework::submenuStates[0];
  const int saved_cursor = VS::Menu::Framework::mainMenuCursorAnimation;
  VS::Menu::Framework::menuItems =
      reinterpret_cast<VS::Menu::Framework::MenuRow*>((storage + kGuardBytes));
  // The workspace layout the menu's allocation sets: the string buffer, then
  // the row types, after the rows.
  VS::Menu::Framework::stringBuf =
      reinterpret_cast<char*>(VS::Menu::Framework::menuItems + kRows);
  VS::Menu::Framework::rowTypeBuf =
      reinterpret_cast<int*>(VS::Menu::Framework::stringBuf + kStringBytes);
  VS::Input::settings.menu_flags = 1;
  VS::Input::settings.cursor_memory = 0;
  VS::Menu::Framework::submenuStates[0] = 0;

  int passed = VS::Menu::Framework::UpdateLiveMainMenuSelection(2) == 0 &&
               VS::Menu::Framework::menuItems[kRows - 1].sub_text == nullptr;
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
  VS::Input::settings.menu_flags = saved_flags;
  VS::Input::settings.cursor_memory = saved_memory;
  VS::Menu::Framework::submenuStates[0] = saved_selection;
  VS::Menu::Framework::mainMenuCursorAnimation = saved_cursor;
  free(storage);
  return passed;
}

int CheckMenuReloadClosingDelay(void) {
  enum { kRows = 40, kVisits = 3, kClosingTicks = 5 };
  const VS::Menu::Framework::LiveMenuControl saved_state =
      VS::Menu::Framework::menuState;
  VS::Menu::Framework::MenuRow* saved_items = VS::Menu::Framework::menuItems;
  std::uint32_t* saved_background = VS::Menu::Framework::menuBackgroundBuffer;
  const signed char saved_loaded = VS::Menu::Framework::loadedSubMenu;
  const char saved_shortcut = VS::Menu::Framework::shortcutInvoked;
  const char saved_without_main = VS::Menu::Framework::openedWithoutMainMenu;
  VS::Menu::Framework::MenuRow rows[kRows] = {};
  // The overlay read is a queue slot, as the menu's own reads are.
  VS::Content::DiscQueueSlot* const request =
      &VS::Content::discRequestSlots[31];
  const VS::Content::DiscQueueSlot saved_slot = *request;
  int passed = 1;
  VS::Boot::ResetMainMenuStatics();
  VS::Menu::Framework::ResetLiveMainMenuStatics();
  VS::Menu::Framework::menuItems = rows;
  VS::Menu::Framework::menuBackgroundBuffer = nullptr;
  VS::Menu::Framework::loadedSubMenu = 0;
  VS::Menu::Framework::shortcutInvoked = 0;
  VS::Menu::Framework::openedWithoutMainMenu = 1;
  VS::Menu::Framework::menuState.cd_queue = request;

  for (int visit = 0; visit < kVisits; ++visit) {
    request->state = VS::Content::DiscRequestState::kLoaded;
    request->file = {VS_MAINMENU_PRG_LBA, VS_MAINMENU_PRG_SIZE};
    VS::Menu::Framework::menuState.control.loading = 1;
    passed &= VS::Menu::Framework::LoadLiveMenuOverlay(0) == 1 &&
              request->state == VS::Content::DiscRequestState::kFree &&
              VS::Menu::Framework::menuState.control.loading == 0;
    for (int tick = 1; tick <= kClosingTicks; ++tick) {
      passed &= VS::Menu::Framework::LoadLiveMenuOverlay(0) == 1;
      passed &= VS::Menu::Framework::LiveQuitDelay() == (tick == kClosingTicks);
    }
  }

  VS::Boot::ResetMainMenuStatics();
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
      &VS::Menu::Status::statusStrings[menu4_status::offset::kValues]);
  const size_t status_words = sizeof VS::Menu::Status::statusStrings /
                              sizeof VS::Menu::Status::statusStrings[0];
  const size_t inventory_words = sizeof VS::Menu::Inventory::menuText /
                                 sizeof VS::Menu::Inventory::menuText[0];
  const unsigned int condition =
      VS::Menu::Status::statusStrings[menu4_status::index::kCritical];
  return status_words > menu4_status::offset::kValues && condition > 0 &&
         condition < status_words && VS::Menu::Inventory::menuText[0] > 0 &&
         VS::Menu::Inventory::menuText[0] < inventory_words &&
         format[0] == VS::Core::TextCode::kPrintString && format[1] == 0 &&
         format[5] == VS::Core::TextCode::kPrintString && format[6] == 1 &&
         format[7] == VS::Core::TextCode::kTerminator;
}

// Points the frame scratchpad at guard-filled packet storage (packets start
// one word in) and a cleared reverse ordering table, with the scene and
// overlay tables at the menu's entries.
static void PrepareMenuPacketCapture(std::span<std::uint32_t> ordering,
                                     std::span<std::uint32_t> storage,
                                     std::uint32_t guard) {
  constexpr std::size_t kSceneEntry = 18;
  constexpr std::size_t kOverlayEntry = 2;
  ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  std::ranges::fill(storage, guard);
  VS::Render::FrameScratch& frame = VS::Render::FrameScratchpad();
  frame.primitive_cursor = storage.data() + 1;
  frame.ordering_table = ordering.data() + kSceneEntry;
  frame.overlay_ordering_table = ordering.data() + kOverlayEntry;
}

int CheckStatusPacketLinks(int background_first) {
  enum {
    kOrderingWords = 0x822,
    kPacketWords = 64,
    kWrittenWords = 53,
    kInsertedPackets = 5
  };
  const std::uint32_t guard = 0xDEADBEEF;
  const VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  const std::int32_t saved_background = VS::Menu::Status::drawBackgroundFirst;
  std::uint32_t ordering[kOrderingWords];
  std::uint32_t storage[kPacketWords];
  PrepareMenuPacketCapture(ordering, storage, guard);
  VS::Menu::Status::drawBackgroundFirst = background_first;
  void* const saved_buffer =
      VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1];
  void* const saved_table =
      VS::Render::legacyOrderingTables[VS::Render::frameBuf & 1];
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] = storage + 1;
  VS::Render::legacyOrderingTables[VS::Render::frameBuf & 1] = ordering;

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
  int passed = packet == nullptr && count == expected_packets &&
               VS::Render::FrameScratchpad().primitive_cursor ==
                   storage + 1 + kWrittenWords &&
               storage[0] == guard && storage[1 + kWrittenWords] == guard;
  VS::Menu::Status::drawBackgroundFirst = saved_background;
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] = saved_buffer;
  VS::Render::legacyOrderingTables[VS::Render::frameBuf & 1] = saved_table;
  VS::Render::FrameScratchpad() = saved_frame;
  return passed;
}

int CheckMenuPromptPanelLinks(void) {
  enum { kOrderingWords = 0x822, kPacketWords = 256, kAnimationFrames = 8 };
  const std::uint32_t guard = 0xDEADBEEF;
  const VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  const char saved_display = VS::Menu::Framework::displayActionCost;
  const auto saved_headers = VS::Menu::Framework::textHeaders;
  std::uint32_t ordering[kOrderingWords];
  std::uint32_t storage[kPacketWords];
  PrepareMenuPacketCapture(ordering, storage, guard);
  VS::Menu::Framework::displayActionCost = 1;
  VS::Menu::Framework::textHeaders = {};
  // The typed HUD packets reserve their words in the current frame's buffer
  // and link only into the current frame's ordering table.
  void* const saved_buffer =
      VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1];
  void* const saved_table =
      VS::Render::legacyOrderingTables[VS::Render::frameBuf & 1];
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] = storage + 1;
  VS::Render::legacyOrderingTables[VS::Render::frameBuf & 1] = ordering;

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
  const std::uint32_t* end = static_cast<const std::uint32_t*>(
      VS::Render::FrameScratchpad().primitive_cursor);
  int passed = packet == nullptr && count >= kOrderingWords + 5 &&
               nextPrim(ordering + 15) == storage + 15 && end > storage + 1 &&
               end < storage + kPacketWords && storage[0] == guard &&
               *end == guard;
  VS::Menu::Framework::textHeaders = saved_headers;
  VS::Menu::Framework::displayActionCost = saved_display;
  VS::Render::FrameScratchpad() = saved_frame;
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] = saved_buffer;
  VS::Render::legacyOrderingTables[VS::Render::frameBuf & 1] = saved_table;
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
  return memcmp(VS::Actors::kAshleyInitialLoadout.name.data(), actor_name,
                sizeof actor_name) == 0 &&
         memcmp(VS::Actors::kAshleyInitialLoadout.weapon.name.data(),
                weapon_name, sizeof weapon_name) == 0;
}
