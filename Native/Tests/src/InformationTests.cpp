#include <array>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Core/Text/TextCode.h"
#include "Input/Controller.h"
#include "Menu/Framework/Framework.h"
#include "Menu/TextBox.h"
#include "Render/Display.h"
#include "Render/Font.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"
}

namespace {
struct InformationFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  u_long* saved_ordering = Menu_activeTextBoxOrderingTable;
  int saved_color = Render_fontColor;
  char saved_palette = Render_fontPaletteOffset;
  char saved_table = Render_fontTableSelect;
  unsigned char saved_index = Render_activeTextBoxIndex;
  Menu_TextBoxState saved_box = Menu_textBoxes[7];
  char saved_setting = Input_settings.information;
  int saved_buttons = Input_buttonsState;
  int* saved_rows = Menu_Framework_rowTypeBuf;
  std::array<unsigned char, 0x20010> memory{};
  std::array<u_long, 34> ordering{};
  std::array<char, 96> message{};
  int row_type = 0;

  InformationFixture() {
    memory.fill(0xA5);
    message.fill(static_cast<char>(Core_TextCode_Terminator));
    message[0] = 5;
    message[1] = 0;
    message[2] = 6;
    Render_legacyPrimitiveBuffers[0] = memory.data() + 8;
    Render_frameBuf = 0;
    Menu_activeTextBoxOrderingTable = ordering.data();
    Render_activeTextBoxIndex = 7;
    Menu_Framework_rowTypeBuf = &row_type;
    Input_buttonsState = 0;
    Menu_textBoxes[7] = {};
    Menu_InitInformationTextBox(Menu_InformationBoxHeader_Information);
    ResetPackets();
  }

  ~InformationFixture() {
    Menu_textBoxes[7] = saved_box;
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    Menu_activeTextBoxOrderingTable = saved_ordering;
    Render_fontColor = saved_color;
    Render_fontPaletteOffset = saved_palette;
    Render_fontTableSelect = saved_table;
    Render_activeTextBoxIndex = saved_index;
    Input_settings.information = saved_setting;
    Input_buttonsState = saved_buttons;
    Menu_Framework_rowTypeBuf = saved_rows;
  }

  void ResetPackets() {
    Render_frameScratch.primitiveCursor = memory.data() + 8;
    ClearOTag(ordering.data(), static_cast<int>(ordering.size()));
  }
};
}  // namespace

TEST_CASE("Information messages preserve the original encoded instant prefix",
          "[render][text][information]") {
  InformationFixture fixture;
  Menu_Framework_SetInformationMessage(fixture.message.data());
  const auto* encoded =
      reinterpret_cast<const unsigned char*>(Menu_textBoxes[7].string);
  CHECK(encoded[0] == Core_TextCode_ChunkSize);
  CHECK(encoded[1] == Core_TextCode_NoChunking);
  CHECK(std::memcmp(encoded + 2, fixture.message.data(),
                    fixture.message.size()) == 0);
  const std::array<unsigned char, 3> tail{encoded[98], encoded[99],
                                          encoded[100]};
  const std::array<unsigned char, 3> expected{Core_TextCode_Terminator,
                                              Core_TextCode_Terminator, 0};
  CHECK(tail == expected);
}

TEST_CASE(
    "Information is complete immediately and replaces old text on selection",
    "[render][text][information]") {
  InformationFixture fixture;
  const auto updates = GENERATE(1, 8);
  for (int update = 0; update < updates; ++update) {
    Menu_Framework_SetInformationMessage(fixture.message.data());
    fixture.ResetPackets();
    Render_DrawProgressiveTextBoxText(&Menu_textBoxes[7]);
    CHECK(Menu_textBoxes[7].flags.done);
    CHECK(Menu_textBoxes[7].prevChunksPrinted == 3);
    CHECK(Render_frameScratch.primitiveCursor ==
          fixture.memory.data() + 8 + sizeof(u_long) * 3 * 6);
  }
  fixture.message[0] = static_cast<char>(Core_TextCode_Terminator);
  Menu_Framework_SetInformationMessage(fixture.message.data());
  fixture.ResetPackets();
  Render_DrawProgressiveTextBoxText(&Menu_textBoxes[7]);
  CHECK(Menu_textBoxes[7].flags.done);
  CHECK(Menu_textBoxes[7].prevChunksPrinted == 0);
  CHECK(Render_frameScratch.primitiveCursor == fixture.memory.data() + 8);
  CHECK(fixture.memory.front() == 0xA5);
  CHECK(fixture.memory.back() == 0xA5);
}

TEST_CASE("Information control repair does not accelerate ordinary dialogue",
          "[render][text][information]") {
  InformationFixture fixture;
  Menu_TextBoxState dialogue{};
  dialogue.flags.kind = 2;
  dialogue.flags.speakerTexture = 32;
  dialogue.brightness = 128;
  dialogue.speed = 1;
  // NOLINTNEXTLINE(build/include_what_you_use): C text member, not std::string.
  dialogue.string = fixture.message.data();
  Render_DrawProgressiveTextBoxText(&dialogue);
  CHECK_FALSE(dialogue.flags.done);
  CHECK(dialogue.prevChunksPrinted == 1);
  CHECK(Render_frameScratch.primitiveCursor ==
        fixture.memory.data() + 8 + 6 * sizeof(u_long));
}

TEST_CASE("Information toggles hide normal help but retain required cautions",
          "[render][text][information]") {
  InformationFixture fixture;
  Input_settings.information = 1;
  Menu_Framework_UpdateInformationVisibility(0, 16);
  CHECK(Menu_textBoxes[7].state == 11);
  CHECK(Menu_textBoxes[7].flags.kind == 4);
  Input_settings.information = 0;
  Menu_Framework_UpdateInformationVisibility(0, 16);
  CHECK(Menu_textBoxes[7].state == 0);
  fixture.row_type = 1;
  Menu_Framework_UpdateInformationVisibility(0, 16);
  CHECK(Menu_textBoxes[7].state == 11);
  fixture.row_type = 0;
  Input_settings.information = 1;
  Menu_Framework_UpdateInformationVisibility(0, 16);
  CHECK(Menu_textBoxes[7].state == 11);
}
