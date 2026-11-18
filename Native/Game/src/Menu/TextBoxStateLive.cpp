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
#include "Audio/Seq/Channel.h"
#include "Core/Diagnostics.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/Sounds.hpp"
#include "Menu/TextBox.h"

namespace VS::Menu {
namespace {
using LiveTextBoxes = std::array<TextBox, kTextBoxCount>;
using LiveAnchorPresets = std::array<TextAnchorPreset, kTextAnchorPresetCount>;

static_assert(std::size(Menu_textBoxes) == kTextBoxCount);
static_assert(std::size(Menu_textBoxSelector.order) == kTextBoxCount);
// The script's 4-bit preset reaches past D_800EC284's nine pairs into the
// following module data, which the native table holds as the same bytes.
static_assert(std::size(Menu_textBoxAnchors) >= kTextAnchorPresetCount);

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
    const auto& pair = std::span(Menu_textBoxAnchors)[i];
    presets[i] = {
        .primary = TextAnchor::FromWord(std::bit_cast<std::uint32_t>(pair[0])),
        .secondary =
            TextAnchor::FromWord(std::bit_cast<std::uint32_t>(pair[1])),
    };
  }
  return presets;
}
}  // namespace

TextBox ReadLiveTextBox(std::size_t index) {
  const Menu_TextBoxState& live = std::span(Menu_textBoxes)[index];
  return {
      .flags = TextBoxFlags::FromWord(std::bit_cast<std::uint32_t>(live.flags)),
      .opaque4 = live.opaque4,
      .opaque6 = live.opaque6,
      .text = live.string,
      .speed = live.speed,
      .previous_chunks = live.prevChunksPrinted,
      .x_indent = live.xIndent,
      .y_indent = live.yIndent,
      .x = live.x,
      .y = live.y,
      .center_x = live.centerX,
      .center_y = live.centerY,
      .state = live.state,
      .brightness = live.brightness,
      .reserved1f = live.reserved1F,
      .chars_per_line = static_cast<std::int8_t>(live.charsPerLine),
      .line_count = static_cast<std::int8_t>(live.lineCount),
      .open_duration = static_cast<std::int8_t>(live.openDuration),
      .reserved23 = static_cast<std::int8_t>(live.reserved23),
      .frame_x = live.frameX,
      .frame_y = live.frameY,
      .frame_width = live.frameWidth,
      .frame_height = live.frameHeight,
      .speaker_pause = live.opaque2C,
      .speaker_animation = live.opaque2E,
  };
}

void WriteLiveTextBox(std::size_t index, const TextBox& box) {
  Menu_TextBoxState& live = std::span(Menu_textBoxes)[index];
  live.flags = std::bit_cast<Battle_TextBoxFlags>(box.flags.Word());
  live.opaque4 = box.opaque4;
  live.opaque6 = box.opaque6;
  // The box only borrows the caller's (mutable, original-signature) string.
  live.string =  // NOLINT(build/include_what_you_use)
      const_cast<char*>(box.text);
  live.speed = box.speed;
  live.prevChunksPrinted = box.previous_chunks;
  live.xIndent = box.x_indent;
  live.yIndent = box.y_indent;
  live.x = box.x;
  live.y = box.y;
  live.centerX = box.center_x;
  live.centerY = box.center_y;
  live.state = box.state;
  live.brightness = box.brightness;
  live.reserved1F = box.reserved1f;
  live.charsPerLine = static_cast<char>(box.chars_per_line);
  live.lineCount = static_cast<char>(box.line_count);
  live.openDuration = static_cast<char>(box.open_duration);
  live.reserved23 = static_cast<char>(box.reserved23);
  live.frameX = box.frame_x;
  live.frameY = box.frame_y;
  live.frameWidth = box.frame_width;
  live.frameHeight = box.frame_height;
  live.opaque2C = box.speaker_pause;
  live.opaque2E = box.speaker_animation;
}

TextBoxSelector ReadLiveTextBoxSelector() {
  TextBoxSelector selector{
      .selected = Menu_textBoxSelector.selectedIndex,
      .count = Menu_textBoxSelector.count,
  };
  std::ranges::copy(Menu_textBoxSelector.order, selector.order.begin());
  return selector;
}

void WriteLiveTextBoxSelector(const TextBoxSelector& selector) {
  std::ranges::copy(selector.order,
                    std::span(Menu_textBoxSelector.order).begin());
  Menu_textBoxSelector.selectedIndex = selector.selected;
  Menu_textBoxSelector.count = selector.count;
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
int SelectLiveTextBoxChoice(u_char box_mask) {
  LiveTextBoxHost host;
  LiveTextBoxes boxes = ReadLiveTextBoxes();
  TextBoxSelector selector = ReadLiveTextBoxSelector();
  const TextBoxSelectionInput input{
      .pressed =
          static_cast<std::uint32_t>(VS::Input::buttonsPressed.allButtons),
      .default_choice =
          static_cast<std::uint8_t>(Audio_Seq_unusedStateFlags.opaque11),
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
void InitLiveInformationTextBox(enum Menu_InformationBoxHeader header) {
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
Menu_TextBoxState* LiveTextBox(int id) {
  const auto index = LiveIndex(id, "TextBox");
  if (!index.has_value()) return nullptr;
  return &std::span(Menu_textBoxes)[*index];
}

// decomp: F01410 func_800CCE10 (src/BATTLE/BATTLE.PRG/5BF94.c:4546).
Menu_TextAnchorWord LiveTextBoxAnchor(int anchor_preset_index,
                                      int orientation_flags,
                                      int text_column_count,
                                      int text_line_count) {
  const LiveAnchorPresets presets = ReadLiveAnchorPresets();
  const auto anchor = TextBoxAnchor(presets, {.preset = anchor_preset_index,
                                              .orientation = orientation_flags,
                                              .columns = text_column_count,
                                              .lines = text_line_count});
  if (!anchor.has_value()) {
    VS::Core::Warnf("menu", "TextBoxAnchor refused preset %d (presets 0-15)",
                    anchor_preset_index);
    return std::bit_cast<Menu_TextAnchorWord>(std::uint32_t{0});
  }
  return std::bit_cast<Menu_TextAnchorWord>(anchor->Word());
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
