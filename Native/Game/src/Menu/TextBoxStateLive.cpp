// Hand-maintained (tools/port/owned.csv).
#include "Menu/TextBoxStateLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

#include "Actors/TextureAnimationLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBox.hpp"
#include "State/StateFlags.hpp"

namespace VS::Menu {
namespace {
using LiveTextBoxes = std::array<TextBox, kTextBoxCount>;
using LiveAnchorPresets = std::array<TextAnchorPreset, kTextAnchorPresetCount>;

static_assert(std::size(textBoxes) == kTextBoxCount);
static_assert(std::size(textBoxSelector.order) == kTextBoxCount);
// The script's 4-bit preset reaches past D_800EC284's nine pairs into the
// following module data, which the native table holds as the same bytes.
static_assert(std::size(textBoxAnchors) == kTextAnchorPresetCount);

class LiveTextBoxHost final : public TextBoxHost {
 public:
  void AnimateSpeaker(std::uint8_t speaker, std::int32_t timeline,
                      std::int32_t command) override {
    VS::Actors::SetLiveActorTextureAnimation(speaker, 0, timeline, command);
  }
  void PlaySelectSound() override { VS::Menu::PlaySelectSound(); }
  void PlayChangeSound() override { VS::Menu::PlayChangeSound(); }
};

std::optional<std::size_t> LiveIndex(int id, const char* entry) {
  const auto index = TextBoxIndex(id);
  if (!index.has_value()) {
    VS::Core::Warnf("menu", "%s refused text box %d (boxes 0-7)", entry, id);
  }
  return index;
}

LiveTextBoxes ReadLiveTextBoxes() {
  LiveTextBoxes boxes{};
  for (std::size_t i = 0; i < kTextBoxCount; ++i) boxes[i] = ReadLiveTextBox(i);
  return boxes;
}

void WriteLiveTextBoxes(const LiveTextBoxes& boxes) {
  for (std::size_t i = 0; i < kTextBoxCount; ++i) WriteLiveTextBox(i, boxes[i]);
}

LiveAnchorPresets ReadLiveAnchorPresets() {
  LiveAnchorPresets presets{};
  for (std::size_t i = 0; i < kTextAnchorPresetCount; ++i) {
    const auto& pair = std::span(textBoxAnchors)[i];
    presets[i] = {
        .primary = TextAnchor::FromWord(pair[0]),
        .secondary = TextAnchor::FromWord(pair[1]),
    };
  }
  return presets;
}
}  // namespace

TextBox ReadLiveTextBox(std::size_t index) {
  const TextBoxState& live = std::span(textBoxes)[index];
  return {
      .flags = TextBoxFlags::FromWord(std::bit_cast<std::uint32_t>(live.flags)),
      .opaque4 = live.opaque4,
      .opaque6 = live.opaque6,
      .text = live.text,
      .speed = live.speed,
      .previous_chunks = live.previous_chunks,
      .x_indent = live.x_indent,
      .y_indent = live.y_indent,
      .x = live.x,
      .y = live.y,
      .center_x = live.center_x,
      .center_y = live.center_y,
      .state = live.state,
      .brightness = live.brightness,
      .reserved1f = live.reserved1f,
      .chars_per_line = live.chars_per_line,
      .line_count = live.line_count,
      .open_duration = live.open_duration,
      .reserved23 = live.reserved23,
      .frame_x = live.frame_x,
      .frame_y = live.frame_y,
      .frame_width = live.frame_width,
      .frame_height = live.frame_height,
      .speaker_pause = live.speaker_pause,
      .speaker_animation = live.speaker_animation,
  };
}

void WriteLiveTextBox(std::size_t index, const TextBox& box) {
  TextBoxState& live = std::span(textBoxes)[index];
  live.flags = std::bit_cast<PackedTextBoxFlags>(box.flags.Word());
  live.opaque4 = box.opaque4;
  live.opaque6 = box.opaque6;
  // The box only borrows the caller's (mutable, original-signature) string.
  live.text = const_cast<char*>(box.text);
  live.speed = box.speed;
  live.previous_chunks = box.previous_chunks;
  live.x_indent = box.x_indent;
  live.y_indent = box.y_indent;
  live.x = box.x;
  live.y = box.y;
  live.center_x = box.center_x;
  live.center_y = box.center_y;
  live.state = box.state;
  live.brightness = box.brightness;
  live.reserved1f = box.reserved1f;
  live.chars_per_line = box.chars_per_line;
  live.line_count = box.line_count;
  live.open_duration = box.open_duration;
  live.reserved23 = box.reserved23;
  live.frame_x = box.frame_x;
  live.frame_y = box.frame_y;
  live.frame_width = box.frame_width;
  live.frame_height = box.frame_height;
  live.speaker_pause = box.speaker_pause;
  live.speaker_animation = box.speaker_animation;
}

TextBoxSelector ReadLiveTextBoxSelector() {
  TextBoxSelector selector{
      .selected = textBoxSelector.selected_index,
      .count = textBoxSelector.count,
  };
  std::ranges::copy(textBoxSelector.order, selector.order.begin());
  return selector;
}

void WriteLiveTextBoxSelector(const TextBoxSelector& selector) {
  std::ranges::copy(selector.order, std::span(textBoxSelector.order).begin());
  textBoxSelector.selected_index = selector.selected;
  textBoxSelector.count = selector.count;
}

// decomp: F01329 vs_battle_setTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:1536).
int SetLiveTextBoxText(int box_id, char* text) {
  const auto index = LiveIndex(box_id, "SetTextBoxText");
  if (!index.has_value()) return 0;
  LiveTextBoxHost host;
  TextBox box = ReadLiveTextBox(*index);
  const int result = SetTextBoxText(box, host, text);
  WriteLiveTextBox(*index, box);
  return result;
}

// decomp: F01330 vs_battle_selectTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:1559).
int SelectLiveTextBoxChoice(std::uint8_t box_mask) {
  LiveTextBoxHost host;
  LiveTextBoxes boxes = ReadLiveTextBoxes();
  TextBoxSelector selector = ReadLiveTextBoxSelector();
  const TextBoxSelectionInput input{
      .pressed = static_cast<std::uint32_t>(VS::Input::buttonsPressed),
      .default_choice =
          static_cast<std::uint8_t>(VS::State::stateFlags.opaque_11),
  };
  const auto choice =
      SelectTextBoxChoice(boxes, selector, host, box_mask, input);
  if (!choice.has_value()) {
    VS::Core::Warnf(
        "menu",
        "SelectTextBoxChoice refused a corrupt selector (count %u), "
        "restarting the selection",
        static_cast<unsigned>(selector.count));
    selector.count = 0;
    WriteLiveTextBoxSelector(selector);
    return 0;
  }
  WriteLiveTextBoxes(boxes);
  WriteLiveTextBoxSelector(selector);
  return *choice;
}

// decomp: F01343 vs_battle_initInformationTextBox
// (src/BATTLE/BATTLE.PRG/5BF94.c:2653).
void InitLiveInformationTextBox(InformationHeader header) {
  TextBox box = ReadLiveTextBox(kInformationTextBox);
  InitTextBox(box, InformationTextBoxLayout(static_cast<std::int32_t>(header)));
  WriteLiveTextBox(kInformationTextBox, box);
}

// decomp: F01378 _dismissAllTextBoxes (src/BATTLE/BATTLE.PRG/5BF94.c:3757).
int DismissAllLiveTextBoxes() {
  LiveTextBoxHost host;
  LiveTextBoxes boxes = ReadLiveTextBoxes();
  const bool all_closed = DismissAllTextBoxes(boxes, host);
  WriteLiveTextBoxes(boxes);
  return all_closed ? 1 : 0;
}

// decomp: F01409 vs_battle_getTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:4544).
TextBoxState* LiveTextBox(int id) {
  const auto index = LiveIndex(id, "TextBox");
  if (!index.has_value()) return nullptr;
  return &std::span(textBoxes)[*index];
}

// decomp: F01410 func_800CCE10 (src/BATTLE/BATTLE.PRG/5BF94.c:4546).
TextAnchor LiveTextBoxAnchor(int anchor_preset_index, int orientation_flags,
                             int text_column_count, int text_line_count) {
  const LiveAnchorPresets presets = ReadLiveAnchorPresets();
  const auto anchor = TextBoxAnchor(presets, {.preset = anchor_preset_index,
                                              .orientation = orientation_flags,
                                              .columns = text_column_count,
                                              .lines = text_line_count});
  if (!anchor.has_value()) {
    VS::Core::Warnf("menu", "TextBoxAnchor refused preset %d (presets 0-15)",
                    anchor_preset_index);
    return {};
  }
  // The original returns the packed word: 12-bit x and y.
  return TextAnchor::FromWord(anchor->Word());
}

// decomp: F01411 vs_battle_initTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:4567).
void InitLiveTextBox(int id, int flags, int x, int y, int chars_per_line,
                     int line_count, int center_x, int center_y) {
  const auto index = LiveIndex(id, "InitTextBox");
  if (!index.has_value()) return;
  TextBox box = ReadLiveTextBox(*index);
  InitTextBox(box, {.flags = flags,
                    .x = x,
                    .y = y,
                    .chars_per_line = chars_per_line,
                    .line_count = line_count,
                    .center_x = center_x,
                    .center_y = center_y});
  WriteLiveTextBox(*index, box);
}

// decomp: F01412 vs_battle_dismissTextBox (src/BATTLE/BATTLE.PRG/5BF94.c:4613).
int DismissLiveTextBox(int id) {
  const auto index = LiveIndex(id, "DismissTextBox");
  if (!index.has_value()) return 0;
  LiveTextBoxHost host;
  TextBox box = ReadLiveTextBox(*index);
  const bool visible = DismissTextBox(box, host);
  WriteLiveTextBox(*index, box);
  return visible ? -1 : 0;
}
}  // namespace VS::Menu
