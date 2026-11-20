#include <array>
#include <cstring>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Core/Text/TextCode.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Settings.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Legacy::ClearOTag;

namespace {
struct InformationFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  int saved_framebuffer = VS::Render::frameBuf;
  std::uint32_t* saved_ordering = VS::Menu::activeTextBoxOrderingTable;
  VS::Render::FontStyle saved_style = VS::Render::LiveFontStyle();
  VS::Menu::TextBoxState saved_box = VS::Menu::textBoxes[7];
  std::uint8_t saved_setting = VS::Input::settings.information;
  int saved_buttons = VS::Input::buttonsState;
  int* saved_rows = VS::Menu::Framework::rowTypeBuf;
  VS::Menu::Framework::MenuRow* saved_items = VS::Menu::Framework::menuItems;
  std::array<unsigned char, 0x20010> memory{};
  std::array<std::uint32_t, 34> ordering{};
  std::array<char, 96> message{};
  // The row types live in the open menu's workspace, beside its rows.
  std::array<VS::Menu::Framework::MenuRow, VS::Menu::Framework::kRowCount>
      rows{};
  std::array<int, VS::Menu::Framework::kRowTypeCount> row_types{};

  InformationFixture() {
    memory.fill(0xA5);
    message.fill(static_cast<char>(VS::Core::TextCode::kTerminator));
    message[0] = 5;
    message[1] = 0;
    message[2] = 6;
    VS::Render::legacyPrimitiveBuffers[0] = memory.data() + 8;
    VS::Render::frameBuf = 0;
    VS::Menu::activeTextBoxOrderingTable = ordering.data();
    VS::Menu::Framework::menuItems = rows.data();
    VS::Menu::Framework::rowTypeBuf = row_types.data();
    VS::Input::buttonsState = 0;
    VS::Menu::textBoxes[7] = {};
    VS::Menu::InitLiveInformationTextBox(
        VS::Menu::InformationHeader::kInformation);
    ResetPackets();
  }

  ~InformationFixture() {
    VS::Menu::textBoxes[7] = saved_box;
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
    VS::Render::frameBuf = saved_framebuffer;
    VS::Menu::activeTextBoxOrderingTable = saved_ordering;
    VS::Render::LiveFontStyle() = saved_style;
    VS::Input::settings.information = saved_setting;
    VS::Input::buttonsState = saved_buttons;
    VS::Menu::Framework::rowTypeBuf = saved_rows;
    VS::Menu::Framework::menuItems = saved_items;
  }

  void ResetPackets() {
    VS::Render::FrameScratchpad().primitive_cursor = memory.data() + 8;
    ClearOTag(ordering.data(), static_cast<int>(ordering.size()));
  }
};
}  // namespace

TEST_CASE("Information messages preserve the original encoded instant prefix",
          "[render][text][information]") {
  InformationFixture fixture;
  VS::Menu::Framework::SetLiveInformationMessage(fixture.message.data());
  const auto* encoded =
      reinterpret_cast<const unsigned char*>(VS::Menu::textBoxes[7].text);
  CHECK(encoded[0] == VS::Core::TextCode::kChunkSize);
  CHECK(encoded[1] == VS::Core::TextCode::kNoChunking);
  CHECK(std::memcmp(encoded + 2, fixture.message.data(),
                    fixture.message.size()) == 0);
  const std::array<unsigned char, 3> tail{encoded[98], encoded[99],
                                          encoded[100]};
  const std::array<unsigned char, 3> expected{
      VS::Core::TextCode::kTerminator, VS::Core::TextCode::kTerminator, 0};
  CHECK(tail == expected);
}

TEST_CASE(
    "Information is complete immediately and replaces old text on selection",
    "[render][text][information]") {
  InformationFixture fixture;
  const auto updates = GENERATE(1, 8);
  for (int update = 0; update < updates; ++update) {
    VS::Menu::Framework::SetLiveInformationMessage(fixture.message.data());
    fixture.ResetPackets();
    VS::Render::DrawLiveProgressiveTextBoxText(&VS::Menu::textBoxes[7]);
    CHECK(VS::Menu::textBoxes[7].flags.done);
    CHECK(VS::Menu::textBoxes[7].previous_chunks == 3);
    CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
          fixture.memory.data() + 8 + sizeof(std::uint32_t) * 3 * 6);
  }
  fixture.message[0] = static_cast<char>(VS::Core::TextCode::kTerminator);
  VS::Menu::Framework::SetLiveInformationMessage(fixture.message.data());
  fixture.ResetPackets();
  VS::Render::DrawLiveProgressiveTextBoxText(&VS::Menu::textBoxes[7]);
  CHECK(VS::Menu::textBoxes[7].flags.done);
  CHECK(VS::Menu::textBoxes[7].previous_chunks == 0);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.memory.data() + 8);
  CHECK(fixture.memory.front() == 0xA5);
  CHECK(fixture.memory.back() == 0xA5);
}

TEST_CASE("Information control repair does not accelerate ordinary dialogue",
          "[render][text][information]") {
  InformationFixture fixture;
  // The text box drawers draw only the dialogue text boxes.
  VS::Menu::TextBoxState& dialogue = VS::Menu::textBoxes[7];
  dialogue = {};
  dialogue.flags.kind = 2;
  dialogue.flags.speaker_texture = 32;
  dialogue.brightness = 128;
  dialogue.speed = 1;
  // NOLINTNEXTLINE(build/include_what_you_use): C text member, not std::string.
  dialogue.text = fixture.message.data();
  VS::Render::DrawLiveProgressiveTextBoxText(&dialogue);
  CHECK_FALSE(dialogue.flags.done);
  CHECK(dialogue.previous_chunks == 1);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        fixture.memory.data() + 8 + 6 * sizeof(std::uint32_t));
}

TEST_CASE("Information toggles hide normal help but retain required cautions",
          "[render][text][information]") {
  InformationFixture fixture;
  VS::Input::settings.information = 1;
  VS::Menu::Framework::UpdateLiveInformationVisibility(0, 16);
  CHECK(VS::Menu::textBoxes[7].state == 11);
  CHECK(VS::Menu::textBoxes[7].flags.kind == 4);
  VS::Input::settings.information = 0;
  VS::Menu::Framework::UpdateLiveInformationVisibility(0, 16);
  CHECK(VS::Menu::textBoxes[7].state == 0);
  fixture.row_types[0] = 1;
  VS::Menu::Framework::UpdateLiveInformationVisibility(0, 16);
  CHECK(VS::Menu::textBoxes[7].state == 11);
  fixture.row_types[0] = 0;
  VS::Input::settings.information = 1;
  VS::Menu::Framework::UpdateLiveInformationVisibility(0, 16);
  CHECK(VS::Menu::textBoxes[7].state == 11);
}
