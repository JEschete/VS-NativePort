#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/Seq/Channel.h"
#include "Menu/TextBox.h"
#include "Menu/TextBoxState.hpp"
#include "Menu/TextBoxStateLive.hpp"

namespace {
namespace m = VS::Menu;

struct SpeakerCall {
  std::uint8_t speaker = 0;
  std::int32_t timeline = 0;
  std::int32_t command = 0;
  friend bool operator==(const SpeakerCall&, const SpeakerCall&) = default;
};

class RecordingHost final : public m::TextBoxHost {
 public:
  std::vector<SpeakerCall> speakers;
  int selects = 0;
  int changes = 0;

  void AnimateSpeaker(std::uint8_t speaker, std::int32_t timeline,
                      std::int32_t command) override {
    speakers.push_back({speaker, timeline, command});
  }
  void PlaySelectSound() override { ++selects; }
  void PlayChangeSound() override { ++changes; }
};

constexpr std::uint32_t kUp = 0x1000;
constexpr std::uint32_t kDown = 0x4000;
constexpr std::uint32_t kCross = 0x40;

std::array<m::TextBox, m::kTextBoxCount> SilentBoxes() {
  std::array<m::TextBox, m::kTextBoxCount> boxes{};
  for (std::size_t i = 0; i < boxes.size(); ++i) {
    boxes[i].flags.speaker_texture = m::kNoTextBoxSpeaker;
    boxes[i].opaque6 = static_cast<std::int16_t>(i);
  }
  return boxes;
}

// Three choices at y 50, 10 and 30 in boxes 0-2.
std::array<m::TextBox, m::kTextBoxCount> ChoiceBoxes() {
  auto boxes = SilentBoxes();
  boxes[0].y = 50;
  boxes[1].y = 10;
  boxes[2].y = 30;
  return boxes;
}
}  // namespace

TEST_CASE("Text box flags keep their packed word", "[text-box-state]") {
  constexpr std::uint32_t kWord = 0xE5A3C7B6;
  const auto flags = m::TextBoxFlags::FromWord(kWord);
  CHECK(flags == m::TextBoxFlags{.kind = 6,
                                 .open_style = 3,
                                 .alignment = 2,
                                 .frame_shape = 7,
                                 .frame_part = 0xC,
                                 .arrow_frame = 0xA3,
                                 .speaker_texture = 0x25,
                                 .done = true,
                                 .frozen = true});
  CHECK(flags.Word() == kWord);
}

TEST_CASE("Text anchors keep their packed word", "[text-box-state]") {
  const m::TextAnchor anchor{
      .x = -1, .y = -2048, .alignment = 3, .frame_part = 15};
  CHECK(anchor.Word() == 0x3F800FFFU);
  CHECK(m::TextAnchor::FromWord(0xFF800FFFU) == anchor);
}

TEST_CASE("F01329 starts text with the speaker talking",
          "[text-box-state][F01329]") {
  RecordingHost host;
  m::TextBox box{.speed = 0xFFFF, .previous_chunks = 9};
  box.flags.speaker_texture = 2;
  box.speaker_animation = 6;
  box.opaque4 = 4;
  const char* text = "abc";
  CHECK(m::SetTextBoxText(box, host, text) == 0);
  CHECK(box.text == text);
  CHECK((box.speed == 1 && box.opaque4 == 0 && box.previous_chunks == 0));
  CHECK(box.speaker_pause == 1);
  CHECK(host.speakers == std::vector<SpeakerCall>{{2, 7, 1}});
}

TEST_CASE("F01329 polls printing until the speed finishes",
          "[text-box-state][F01329]") {
  RecordingHost host;
  m::TextBox box{.speed = 1};
  CHECK(m::SetTextBoxText(box, host, nullptr) == -1);
  box.speed = 0xFFFF;
  CHECK(m::SetTextBoxText(box, host, nullptr) == 0);
  box.speed = 0;
  CHECK(m::SetTextBoxText(box, host, nullptr) == 0);
  CHECK(host.speakers.empty());
}

TEST_CASE("F01329 leaves a box without a speaker silent",
          "[text-box-state][F01329]") {
  RecordingHost host;
  m::TextBox box{};
  box.flags.speaker_texture = m::kNoTextBoxSpeaker | 3;
  box.speaker_pause = 5;
  m::StartTextBoxText(box, host, "x");
  CHECK(host.speakers.empty());
  CHECK(box.speaker_pause == 5);
  CHECK(m::TextBoxTextPending(box));
}

TEST_CASE("F01330 orders the choices by y and highlights the default",
          "[text-box-state][F01330]") {
  RecordingHost host;
  auto boxes = ChoiceBoxes();
  m::TextBoxSelector selector{};
  const auto choice = m::SelectTextBoxChoice(
      boxes, selector, host, 0b111, {.pressed = kCross, .default_choice = 2});
  REQUIRE(choice.has_value());
  CHECK(choice == 0);
  CHECK(selector ==
        m::TextBoxSelector{.order = {1, 2, 0}, .selected = 1, .count = 3});
  CHECK((host.changes == 1 && host.selects == 0));
  CHECK(boxes[2].brightness == m::kSelectedChoiceBrightness);
  CHECK(boxes[1].brightness == m::kOtherChoiceBrightness);
}

TEST_CASE("F01330 moves with wrapping and draws the highlight last",
          "[text-box-state][F01330]") {
  RecordingHost host;
  auto boxes = ChoiceBoxes();
  m::TextBoxSelector selector{};
  CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0b111, {}) == 0);
  CHECK(host.changes == 0);
  CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0b111,
                               {.pressed = kUp}) == 0);
  CHECK(selector ==
        m::TextBoxSelector{.order = {0, 1, 2}, .selected = 2, .count = 3});
  // The bottom choice's contents (box 0, y 50) now draw from box 2.
  CHECK((boxes[2].y == 50 && boxes[2].opaque6 == 0 &&
         boxes[2].brightness == m::kSelectedChoiceBrightness &&
         boxes[0].brightness == m::kOtherChoiceBrightness));
  CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0b111,
                               {.pressed = kDown}) == 0);
  CHECK((selector.selected == 0 && host.changes == 2));
}

TEST_CASE("F01330 confirms one-based and restarts on the next call",
          "[text-box-state][F01330]") {
  RecordingHost host;
  auto boxes = ChoiceBoxes();
  m::TextBoxSelector selector{};
  CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0b111,
                               {.default_choice = 3}) == 0);
  CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0b111,
                               {.pressed = kCross | kDown}) == 3);
  CHECK(selector.count == 0);
  CHECK(host.selects == 1);
  // A default past the choices (and 0) falls back to the first.
  CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0b11,
                               {.default_choice = 9}) == 0);
  CHECK(selector.count == 2);
  CHECK(selector.selected == 0);
}

TEST_CASE("F01330 keeps the original tie and sentinel ordering",
          "[text-box-state][F01330]") {
  RecordingHost host;
  auto boxes = SilentBoxes();
  boxes[1].y = 0x7FFF;
  boxes[2].y = 5;
  m::TextBoxSelector selector{};
  CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0b110, {}) == 0);
  // A chosen box at y 0x7FFF loses to the sentinel: box 0, outside the
  // mask, is listed instead, as in the original.
  CHECK(selector.order[0] == 2);
  CHECK(selector.order[1] == 0);
  CHECK(boxes[1].brightness == 0);
}

TEST_CASE("F01330 never confirms an empty choice", "[text-box-state][F01330]") {
  RecordingHost host;
  auto boxes = ChoiceBoxes();
  const auto before = boxes;
  m::TextBoxSelector selector{.order = {9, 9}, .selected = 4};
  for (int frame = 0; frame < 3; ++frame) {
    CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0,
                                 {.pressed = kCross, .default_choice = 2}) ==
          0);
  }
  CHECK(selector.count == 0);
  CHECK(selector.selected == 0);
  CHECK(boxes == before);
  CHECK(host.selects == 0);
  CHECK(host.changes == 0);
}

TEST_CASE("F01330 refuses a corrupt selector before changing it",
          "[text-box-state][F01330]") {
  RecordingHost host;
  auto boxes = ChoiceBoxes();
  const auto before = boxes;
  m::TextBoxSelector selector{.order = {0, 8}, .selected = 0, .count = 2};
  const auto kept = selector;
  CHECK_FALSE(
      m::SelectTextBoxChoice(boxes, selector, host, 0b11, {.pressed = kDown})
          .has_value());
  selector.count = 9;
  CHECK_FALSE(
      m::SelectTextBoxChoice(boxes, selector, host, 0b11, {}).has_value());
  selector.count = kept.count;
  CHECK(selector == kept);
  CHECK(boxes == before);
  CHECK(host.changes == 0);
  selector.count = 9;
  CHECK(m::SelectTextBoxChoice(boxes, selector, host, 0b11,
                               {.pressed = kCross}) == 1);
}

TEST_CASE("F01411 opens a box and keeps an opening box's state",
          "[text-box-state][F01411]") {
  m::TextBox box{.text = "kept", .previous_chunks = 3, .state = 5};
  box.frame_width = 40;
  box.speaker_pause = 2;
  m::InitTextBox(box, {.flags = static_cast<std::int32_t>(0xABCD0123U),
                       .x = 0x12345,
                       .y = -2,
                       .chars_per_line = 0x1FF,
                       .line_count = 3,
                       .center_x = 7,
                       .center_y = 8});
  CHECK(box.flags.Word() == 0x20000123U);
  CHECK(box.open_duration == 10);
  CHECK(box.state == 5);
  CHECK(box.x == 0x2345);
  CHECK(box.x_indent == 0x234A);
  CHECK(box.y_indent == 1);
  CHECK(box.chars_per_line == -1);
  CHECK(box.brightness == m::kSelectedChoiceBrightness);
  CHECK((box.frame_width == 40 && box.previous_chunks == 3 &&
         box.speaker_pause == 2 && box.text != nullptr));
}

TEST_CASE("F01411 restarts closed boxes and rewrites the frame kinds",
          "[text-box-state][F01411]") {
  m::TextBox box{.state = -3};
  m::InitTextBox(box, {.flags = 0x0F11});
  CHECK(box.state == 1);
  CHECK((box.flags.kind == 7 && box.flags.frame_shape == 0));
  CHECK(box.open_duration == 20);
  m::InitTextBox(box, {.flags = 0x0035});
  CHECK((box.flags.kind == 7 && box.flags.frame_shape == 2));
  CHECK(box.open_duration == 8);
}

TEST_CASE("F01411 lays out the information kind at the screen bottom",
          "[text-box-state][F01411]") {
  m::TextBox box{};
  m::InitTextBox(box, {.flags = 0x0024, .line_count = 4});
  CHECK(box.state == 1);
  CHECK(box.open_duration == 10);
  CHECK((box.chars_per_line == 0x18 && box.center_x == 0xA0 &&
         box.center_y == 0xF0 && box.x_indent == 0x10 &&
         box.y_indent == 0xF2 - 52));
}

TEST_CASE("F01343 opens the information box with its header",
          "[text-box-state][F01343]") {
  auto boxes = SilentBoxes();
  m::InitInformationTextBox(boxes, 2);
  const m::TextBox& box = boxes[m::kInformationTextBox];
  CHECK(box.flags.kind == 4);
  CHECK(box.flags.frame_shape == 2);
  CHECK(box.flags.speaker_texture == m::kNoTextBoxSpeaker);
  CHECK(box.y == 1);
  CHECK(box.line_count == 4);
  CHECK(box.y_indent == 0xF2 - 52);
  CHECK(box.state == 1);
  CHECK(boxes[0] == SilentBoxes()[0]);
}

TEST_CASE("F01412 closes asynchronously and always stops the speaker",
          "[text-box-state][F01412]") {
  RecordingHost host;
  m::TextBox box{.state = 4};
  box.flags.speaker_texture = 3;
  box.flags.open_style = 2;
  CHECK(m::DismissTextBox(box, host));
  CHECK(box.state == -1);
  CHECK(box.open_duration == 8);
  CHECK(box.flags.open_style == 0);
  box.state = -6;
  box.open_duration = 1;
  CHECK(m::DismissTextBox(box, host));
  CHECK((box.state == -6 && box.open_duration == 1));
  box.state = 0;
  CHECK_FALSE(m::DismissTextBox(box, host));
  CHECK(host.speakers == std::vector<SpeakerCall>(3, {3, 0, 0}));
}

TEST_CASE("F01378 dismisses every box and reports when all are closed",
          "[text-box-state][F01378]") {
  RecordingHost host;
  auto boxes = SilentBoxes();
  CHECK(m::DismissAllTextBoxes(boxes, host));
  boxes[2].state = 3;
  boxes[6].state = -1;
  boxes[5].flags.speaker_texture = 1;
  CHECK_FALSE(m::DismissAllTextBoxes(boxes, host));
  CHECK(boxes[2].state == -1);
  CHECK(boxes[6].state == -1);
  CHECK(host.speakers.size() == 1);
  // Reopen after the asynchronous close finished.
  boxes[2].state = 0;
  boxes[6].state = 0;
  CHECK(m::DismissAllTextBoxes(boxes, host));
}

TEST_CASE("F01409 refuses ids outside the eight boxes",
          "[text-box-state][F01409]") {
  CHECK(m::TextBoxIndex(0) == 0U);
  CHECK(m::TextBoxIndex(7) == 7U);
  CHECK_FALSE(m::TextBoxIndex(8).has_value());
  CHECK_FALSE(m::TextBoxIndex(-1).has_value());
}

TEST_CASE("F01410 picks, mirrors and centres the anchor per facing",
          "[text-box-state][F01410]") {
  const std::array<m::TextAnchorPreset, 2> presets{{
      {.primary = {.x = 100, .y = -20, .alignment = 1, .frame_part = 3},
       .secondary = {.x = -50, .y = 40, .alignment = 2, .frame_part = 5}},
      {.primary = {.x = -2048}},
  }};
  CHECK(m::TextBoxAnchor(presets, {0, 0, 4, 2}) ==
        m::TextAnchor{.x = 71, .y = -35, .alignment = 1, .frame_part = 3});
  CHECK(m::TextBoxAnchor(presets, {0, 4, 4, 2}) ==
        m::TextAnchor{.x = 21, .y = 25, .alignment = 0, .frame_part = 5});
  CHECK(m::TextBoxAnchor(presets, {0, 8 | 3, 4, 2}) ==
        m::TextAnchor{.x = -79, .y = 25, .alignment = 2, .frame_part = 5});
  CHECK(m::TextBoxAnchor(presets, {0, 12, 4, 2}) ==
        m::TextAnchor{.x = -129, .y = -35, .alignment = 3, .frame_part = 3});
  CHECK(m::TextBoxAnchor(presets, {1, 0, 0, -1}) ==
        m::TextAnchor{.x = 2043, .y = 5});
  CHECK_FALSE(m::TextBoxAnchor(presets, {2, 0, 0, 0}).has_value());
  CHECK_FALSE(m::TextBoxAnchor(presets, {-1, 0, 0, 0}).has_value());
}

namespace {
struct LiveTextBoxFixture {
  std::array<Menu_TextBoxState, m::kTextBoxCount> saved_boxes =
      std::to_array(Menu_textBoxes);
  Menu_Framework_TextBoxSelector saved_selector = Menu_textBoxSelector;
  decltype(Audio_Seq_unusedStateFlags.opaque11) saved_default_choice =
      Audio_Seq_unusedStateFlags.opaque11;
  std::array<std::array<Menu_TextAnchorWord, 2>, m::kTextAnchorPresetCount>
      saved_anchors{};

  LiveTextBoxFixture() {
    for (std::size_t i = 0; i < saved_anchors.size(); ++i) {
      std::ranges::copy(std::span(Menu_textBoxAnchors)[i],
                        saved_anchors[i].begin());
    }
    Menu_textBoxSelector = {};
    Audio_Seq_unusedStateFlags.opaque11 = 0;
    for (std::size_t i = 0; i < m::kTextBoxCount; ++i) {
      m::TextBox box{.state = 0};
      box.flags.speaker_texture = m::kNoTextBoxSpeaker;
      m::WriteLiveTextBox(i, box);
    }
  }
  ~LiveTextBoxFixture() {
    std::ranges::copy(saved_boxes, std::span(Menu_textBoxes).begin());
    Menu_textBoxSelector = saved_selector;
    Audio_Seq_unusedStateFlags.opaque11 = saved_default_choice;
    for (std::size_t i = 0; i < saved_anchors.size(); ++i) {
      std::ranges::copy(saved_anchors[i],
                        std::begin(std::span(Menu_textBoxAnchors)[i]));
    }
  }
  LiveTextBoxFixture(const LiveTextBoxFixture&) = delete;
  LiveTextBoxFixture& operator=(const LiveTextBoxFixture&) = delete;
};
}  // namespace

TEST_CASE("Text box Live views round-trip every field",
          "[text-box-state][F01409]") {
  const LiveTextBoxFixture fixture;
  char text[] = "live";
  const m::TextBox box{.flags = m::TextBoxFlags::FromWord(0xA5C3B2E1U),
                       .opaque4 = -4,
                       .opaque6 = 6,
                       .text = text,
                       .speed = 0xFFFF,
                       .previous_chunks = 7,
                       .x_indent = -8,
                       .y_indent = 9,
                       .x = -10,
                       .y = 11,
                       .center_x = 12,
                       .center_y = -13,
                       .state = -14,
                       .brightness = 0xFE,
                       .reserved1f = 1,
                       .chars_per_line = -2,
                       .line_count = 3,
                       .open_duration = 4,
                       .reserved23 = -5,
                       .frame_x = 6,
                       .frame_y = 7,
                       .frame_width = 8,
                       .frame_height = 9,
                       .speaker_pause = 10,
                       .speaker_animation = -11};
  m::WriteLiveTextBox(3, box);
  CHECK(m::ReadLiveTextBox(3) == box);
  CHECK(std::bit_cast<std::uint32_t>(Menu_textBoxes[3].flags) == 0xA5C3B2E1U);
  CHECK(Menu_textBoxes[3].string ==  // NOLINT(build/include_what_you_use)
        text);
  CHECK(m::LiveTextBox(3) == &Menu_textBoxes[3]);
  CHECK(m::LiveTextBox(8) == nullptr);
  CHECK(m::LiveTextBox(-1) == nullptr);
}

TEST_CASE("Text box Live lifecycle opens, selects and dismisses the globals",
          "[text-box-state][F01411][F01412][F01378][F01330][F01343]") {
  const LiveTextBoxFixture fixture;
  m::InitLiveTextBox(0, 0x10, 0, 40, 10, 2, 0, 0);
  m::InitLiveTextBox(1, 0x10, 0, 20, 10, 2, 0, 0);
  m::InitLiveInformationTextBox(Menu_InformationBoxHeader_Check);
  CHECK(Menu_textBoxes[7].flags.frameShape == 3);
  CHECK(Menu_textBoxes[0].openDuration == 20);
  CHECK(m::SelectLiveTextBoxChoice(0b11) == 0);
  CHECK(Menu_textBoxSelector.count == 2);
  CHECK(Menu_textBoxSelector.order[0] == 1);
  CHECK(m::DismissLiveTextBox(0) == -1);
  CHECK(Menu_textBoxes[0].state == -1);
  CHECK(m::DismissAllLiveTextBoxes() == 0);
  CHECK(m::DismissLiveTextBox(9) == 0);
}

TEST_CASE("Text box Live text and anchors refuse out-of-range indexes",
          "[text-box-state][F01329][F01410]") {
  const LiveTextBoxFixture fixture;
  char text[] = "page";
  CHECK(m::SetLiveTextBoxText(2, text) == 0);
  CHECK(Menu_textBoxes[2].string ==  // NOLINT(build/include_what_you_use)
        text);
  CHECK(m::SetLiveTextBoxText(2, nullptr) == -1);
  CHECK(m::SetLiveTextBoxText(8, text) == 0);
  Menu_textBoxAnchors[15][1] =
      std::bit_cast<Menu_TextAnchorWord>(m::TextAnchor{.x = 30}.Word());
  const auto word = m::LiveTextBoxAnchor(15, 8, 0, 0);
  CHECK(std::bit_cast<std::uint32_t>(word) ==
        m::TextAnchor{.x = 25, .y = -2}.Word());
  CHECK(std::bit_cast<std::uint32_t>(m::LiveTextBoxAnchor(16, 0, 0, 0)) == 0);
  m::InitLiveTextBox(8, 0, 0, 0, 0, 0, 0, 0);
  CHECK(Menu_textBoxes[7].state == 0);
}
