// Hand-maintained (tools/port/owned.csv).
#include "Menu/TextBoxState.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu {
namespace {
namespace math = VS::Core::Math;

constexpr std::uint32_t kKindMask = 0xF;
// vs_battle_initTextBox keeps only the low halfword of its flags.
constexpr std::uint32_t kLayoutFlagsMask = 0xFFFF;
constexpr std::uint32_t kTwoBitMask = 0x3;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::uint32_t kSpeakerMask = 0x3F;
constexpr int kOpenStyleShift = 4;
constexpr int kAlignmentShift = 6;
constexpr int kFrameShapeShift = 8;
constexpr int kFramePartShift = 12;
constexpr int kArrowFrameShift = 16;
constexpr int kSpeakerShift = 24;
constexpr int kDoneShift = 30;
constexpr int kFrozenShift = 31;

constexpr std::uint32_t kAnchorFieldMask = 0xFFF;
constexpr std::uint32_t kAnchorSignBit = 0x800;
constexpr int kAnchorYShift = 12;
constexpr int kAnchorAlignmentShift = 24;
constexpr int kAnchorFramePartShift = 26;
constexpr int kAnchorFacingShift = 2;
// Anchor alignment bit flipped by a mirrored facing.
constexpr std::uint8_t kMirroredAlignment = 2;
// Pixels per text column/line and the frame margins (5BF94.c:4559-4560).
constexpr std::int32_t kAnchorColumnWidth = 12;
constexpr std::int32_t kAnchorColumnMargin = 10;
constexpr std::int32_t kAnchorLineHeight = 13;
constexpr std::int32_t kAnchorLineMargin = 4;

// Face, shoulder and trigger buttons confirm a choice (5BF94.c:1601).
constexpr std::uint32_t kConfirmButtons = 0xFF;
constexpr std::uint32_t kUpButton = 0x1000;
constexpr std::uint32_t kDownButton = 0x4000;
// The y the sort gives boxes that are not, or no longer, candidates.
constexpr std::int16_t kUnsortedY = 0x7FFF;

// Packed open durations (in half ticks) per open style: 10, 20, 10, 8.
constexpr std::uint32_t kOpenDurations = 0x45A5;
constexpr int kOpenDurationBits = 4;
constexpr std::int8_t kDismissDuration = 8;
constexpr std::uint16_t kTextStartSpeed = 1;
// The indents from the box origin and the information box's fixed layout.
constexpr std::int32_t kTextXIndent = 5;
constexpr std::int32_t kTextYIndent = 3;
constexpr std::int8_t kInformationCharsPerLine = 0x18;
constexpr std::int16_t kInformationCenterX = 0xA0;
constexpr std::int16_t kInformationCenterY = 0xF0;
constexpr std::int16_t kInformationXIndent = 0x10;
constexpr std::int32_t kInformationBottom = 0xF2;
constexpr std::int32_t kInformationLineHeight = 13;
constexpr std::int32_t kInformationLineCount = 4;
constexpr std::uint8_t kSecondShapedFrame = 2;
constexpr int kInformationHeaderShift = 8;

std::int16_t SignExtendAnchorField(std::uint32_t field) {
  const auto value = static_cast<std::int32_t>(field & kAnchorFieldMask);
  return static_cast<std::int16_t>(
      (field & kAnchorSignBit) != 0 ? value - (kAnchorSignBit << 1) : value);
}

bool SelectorIsUsable(const TextBoxSelector& selector) {
  if (selector.count > kTextBoxCount) return false;
  const auto used = std::span(selector.order).first(selector.count);
  return std::ranges::all_of(
      used, [](std::uint8_t index) { return index < kTextBoxCount; });
}

// The first call: candidates in ascending y, then the default highlight.
std::uint32_t StartSelection(TextBoxes boxes, TextBoxSelector& selector,
                             std::uint8_t box_mask,
                             std::uint8_t default_choice) {
  std::array<std::int16_t, kTextBoxCount> y_positions{};
  std::uint32_t count = 0;
  for (std::size_t i = 0; i < kTextBoxCount; ++i) {
    y_positions[i] = kUnsortedY;
    if (((box_mask >> i) & 1U) != 0) {
      ++count;
      y_positions[i] = boxes[i].y;
    }
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    std::size_t sorted = 0;
    for (std::size_t j = 0; j < kTextBoxCount; ++j) {
      if (y_positions[sorted] > y_positions[j]) sorted = j;
    }
    selector.order[i] = static_cast<std::uint8_t>(sorted);
    y_positions[sorted] = kUnsortedY;
  }
  // The original compares the one-based choice less one unsigned, so 0
  // (no default) and choices past the boxes both highlight the first.
  std::uint32_t selected = static_cast<std::uint32_t>(default_choice) - 1U;
  if (selected >= count) selected = 0;
  selector.selected = 0;
  selector.count = static_cast<std::uint8_t>(count);
  return selected;
}

// Highlights `selected`, moves its box's contents to the selection's highest
// box index (drawn over the others) and dims the rest.
void ApplySelection(TextBoxes boxes, TextBoxSelector& selector,
                    TextBoxHost& host, std::uint32_t selected,
                    std::uint32_t count) {
  if (selected != selector.selected) {
    host.PlayChangeSound();
    selector.selected = static_cast<std::uint8_t>(selected);
  }
  if (count == 0) {
    // The original self-swaps the stale order[0] box: nothing changes.
    return;
  }
  std::uint32_t top = selected;
  for (std::uint32_t i = 0; i < count; ++i) {
    if (selector.order[top] < selector.order[i]) top = i;
  }
  const std::uint8_t previous = selector.order[selected];
  selector.order[selected] = selector.order[top];
  selector.order[top] = previous;
  std::swap(boxes[previous], boxes[selector.order[selected]]);
  for (std::uint32_t i = 0; i < count; ++i) {
    boxes[selector.order[i]].brightness =
        i == selected ? kSelectedChoiceBrightness : kOtherChoiceBrightness;
  }
}
}  // namespace

TextBoxFlags TextBoxFlags::FromWord(std::uint32_t word) {
  return {
      .kind = static_cast<std::uint8_t>(word & kKindMask),
      .open_style =
          static_cast<std::uint8_t>((word >> kOpenStyleShift) & kTwoBitMask),
      .alignment =
          static_cast<std::uint8_t>((word >> kAlignmentShift) & kTwoBitMask),
      .frame_shape =
          static_cast<std::uint8_t>((word >> kFrameShapeShift) & kKindMask),
      .frame_part =
          static_cast<std::uint8_t>((word >> kFramePartShift) & kKindMask),
      .arrow_frame =
          static_cast<std::uint8_t>((word >> kArrowFrameShift) & kByteMask),
      .speaker_texture =
          static_cast<std::uint8_t>((word >> kSpeakerShift) & kSpeakerMask),
      .done = ((word >> kDoneShift) & 1U) != 0,
      .frozen = ((word >> kFrozenShift) & 1U) != 0,
  };
}

std::uint32_t TextBoxFlags::Word() const {
  return (kind & kKindMask) | ((open_style & kTwoBitMask) << kOpenStyleShift) |
         ((alignment & kTwoBitMask) << kAlignmentShift) |
         ((frame_shape & kKindMask) << kFrameShapeShift) |
         ((frame_part & kKindMask) << kFramePartShift) |
         (static_cast<std::uint32_t>(arrow_frame) << kArrowFrameShift) |
         ((speaker_texture & kSpeakerMask) << kSpeakerShift) |
         (static_cast<std::uint32_t>(done) << kDoneShift) |
         (static_cast<std::uint32_t>(frozen) << kFrozenShift);
}

bool TextBoxFlags::HasSpeaker() const {
  return (speaker_texture & kNoTextBoxSpeaker) == 0;
}

TextAnchor TextAnchor::FromWord(std::uint32_t word) {
  return {
      .x = SignExtendAnchorField(word),
      .y = SignExtendAnchorField(word >> kAnchorYShift),
      .alignment = static_cast<std::uint8_t>((word >> kAnchorAlignmentShift) &
                                             kTwoBitMask),
      .frame_part = static_cast<std::uint8_t>((word >> kAnchorFramePartShift) &
                                              kKindMask),
  };
}

std::uint32_t TextAnchor::Word() const {
  return (static_cast<std::uint32_t>(x) & kAnchorFieldMask) |
         ((static_cast<std::uint32_t>(y) & kAnchorFieldMask) << kAnchorYShift) |
         ((alignment & kTwoBitMask) << kAnchorAlignmentShift) |
         ((frame_part & kKindMask) << kAnchorFramePartShift);
}

// decomp: BATTLE.PRG vs_battle_setTextBox (F01329, 5BF94.c:1536,
// 0x800C6BF0-0x800C6C8C): the non-null branch.
void StartTextBoxText(TextBox& box, TextBoxHost& host, const char* text) {
  box.text = text;
  box.speed = kTextStartSpeed;
  box.opaque4 = 0;
  box.previous_chunks = 0;
  if (box.flags.HasSpeaker()) {
    host.AnimateSpeaker(box.flags.speaker_texture,
                        static_cast<std::int32_t>(box.speaker_animation) + 1,
                        1);
    box.speaker_pause = 1;
  }
}

// decomp: BATTLE.PRG vs_battle_setTextBox (F01329, 5BF94.c:1553): the null
// branch's poll.
bool TextBoxTextPending(const TextBox& box) {
  return box.speed != kTextBoxFinishedSpeed && box.speed != 0;
}

// decomp: BATTLE.PRG vs_battle_setTextBox (F01329, 5BF94.c:1536,
// 0x800C6BF0-0x800C6C8C).
int SetTextBoxText(TextBox& box, TextBoxHost& host, const char* text) {
  if (text != nullptr) {
    StartTextBoxText(box, host, text);
    return 0;
  }
  return TextBoxTextPending(box) ? -1 : 0;
}

// decomp: BATTLE.PRG vs_battle_selectTextBox (F01330, 5BF94.c:1559,
// 0x800C6C8C-0x800C7010). Deviation: a corrupt selector (count past eight or
// a used order entry past the boxes) is refused before anything changes;
// the original indexed past the order array and the boxes.
std::optional<int> SelectTextBoxChoice(TextBoxes boxes,
                                       TextBoxSelector& selector,
                                       TextBoxHost& host, std::uint8_t box_mask,
                                       const TextBoxSelectionInput& input) {
  std::uint32_t selected = 0;
  std::uint32_t count = 0;
  if (selector.count == 0) {
    selected = StartSelection(boxes, selector, box_mask, input.default_choice);
    count = selector.count;
  } else {
    selected = selector.selected;
    count = selector.count;
    if ((input.pressed & kConfirmButtons) != 0) {
      host.PlaySelectSound();
      selector.count = 0;
      return static_cast<int>(selected) + 1;
    }
    if (!SelectorIsUsable(selector)) return std::nullopt;
    if ((input.pressed & kUpButton) != 0) selected = selected - 1U + count;
    if ((input.pressed & kDownButton) != 0) ++selected;
    selected %= count;
  }
  ApplySelection(boxes, selector, host, selected, count);
  return 0;
}

TextBoxLayout InformationTextBoxLayout(std::int32_t header) {
  return {
      .flags = static_cast<std::int32_t>(
          (static_cast<std::uint32_t>(header) << kInformationHeaderShift) |
          static_cast<std::uint32_t>(TextBoxKind::kInformation)),
      .x = 0,
      .y = 1,
      .chars_per_line = 0,
      .line_count = kInformationLineCount,
      .center_x = 0,
      .center_y = 0,
  };
}

// decomp: BATTLE.PRG vs_battle_initInformationTextBox (F01343, 5BF94.c:2653,
// 0x800C8E04-0x800C8E48).
void InitInformationTextBox(TextBoxes boxes, std::int32_t header) {
  InitTextBox(boxes[kInformationTextBox], InformationTextBoxLayout(header));
}

// decomp: BATTLE.PRG _dismissAllTextBoxes (F01378, 5BF94.c:3757,
// 0x800CB268-0x800CB2B8): every box is dismissed, none skipped.
bool DismissAllTextBoxes(TextBoxes boxes, TextBoxHost& host) {
  bool all_closed = true;
  for (TextBox& box : boxes) {
    if (DismissTextBox(box, host)) all_closed = false;
  }
  return all_closed;
}

// decomp: BATTLE.PRG vs_battle_getTextBox (F01409, 5BF94.c:4544,
// 0x800CCDF4-0x800CCE10). Deviation: ids outside the boxes are refused.
std::optional<std::size_t> TextBoxIndex(std::int32_t id) {
  if (id < 0 || static_cast<std::size_t>(id) >= kTextBoxCount) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(id);
}

// decomp: BATTLE.PRG func_800CCE10 (F01410, 5BF94.c:4546,
// 0x800CCE10-0x800CCF08). Deviations: a preset outside `presets` is refused
// (the original read past D_800EC284), and the result's top two bits are 0
// rather than the caller's stale register bits.
std::optional<TextAnchor> TextBoxAnchor(
    std::span<const TextAnchorPreset> presets,
    const TextAnchorRequest& request) {
  const auto [preset, orientation, columns, lines] = request;
  if (preset < 0 || static_cast<std::size_t>(preset) >= presets.size()) {
    return std::nullopt;
  }
  const std::int32_t facing =
      (orientation >> kAnchorFacingShift) & static_cast<std::int32_t>(3);
  const TextAnchorPreset& pair = presets[static_cast<std::size_t>(preset)];
  const TextAnchor& source =
      facing == 1 || facing == 2 ? pair.secondary : pair.primary;
  std::int32_t offset_x = source.x;
  std::uint8_t alignment = source.alignment & kTwoBitMask;
  if ((facing & 1) != 0) {
    offset_x = -offset_x;
    alignment ^= kMirroredAlignment;
  }
  const std::int32_t width =
      math::WrappingAdd(math::WrappingMultiply(columns, kAnchorColumnWidth),
                        kAnchorColumnMargin) >>
      1;
  const std::int32_t height =
      math::WrappingAdd(math::WrappingMultiply(lines, kAnchorLineHeight),
                        kAnchorLineMargin) >>
      1;
  return TextAnchor{
      .x = SignExtendAnchorField(
          static_cast<std::uint32_t>(math::WrappingSubtract(offset_x, width))),
      .y = SignExtendAnchorField(
          static_cast<std::uint32_t>(math::WrappingSubtract(source.y, height))),
      .alignment = alignment,
      .frame_part = static_cast<std::uint8_t>(source.frame_part & kKindMask),
  };
}

// decomp: BATTLE.PRG vs_battle_initTextBox (F01411, 5BF94.c:4567,
// 0x800CCF08-0x800CD064). The text, printed chunks, frame rectangle and
// speaker pause are left as they were.
void InitTextBox(TextBox& box, const TextBoxLayout& layout) {
  box.flags = TextBoxFlags::FromWord(
      (static_cast<std::uint32_t>(layout.flags) & kLayoutFlagsMask) |
      (static_cast<std::uint32_t>(kNoTextBoxSpeaker) << kSpeakerShift));
  box.x = math::WrapSigned16(static_cast<std::uint32_t>(layout.x));
  box.y = math::WrapSigned16(static_cast<std::uint32_t>(layout.y));
  box.opaque4 = 0;
  box.opaque6 = 0;
  box.speed = 0;
  box.x_indent = math::WrapSigned16(
      static_cast<std::uint32_t>(math::WrappingAdd(layout.x, kTextXIndent)));
  box.y_indent = math::WrapSigned16(
      static_cast<std::uint32_t>(math::WrappingAdd(layout.y, kTextYIndent)));
  box.speaker_animation = 0;
  box.brightness = kSelectedChoiceBrightness;
  box.chars_per_line = static_cast<std::int8_t>(layout.chars_per_line);
  box.line_count = static_cast<std::int8_t>(layout.line_count);
  box.center_x =
      math::WrapSigned16(static_cast<std::uint32_t>(layout.center_x));
  box.center_y =
      math::WrapSigned16(static_cast<std::uint32_t>(layout.center_y));
  box.open_duration = static_cast<std::int8_t>(
      ((kOpenDurations >> (box.flags.open_style * kOpenDurationBits)) &
       kKindMask) *
      2);
  if (box.state <= 0) box.state = 1;
  switch (static_cast<TextBoxKind>(box.flags.kind)) {
    case TextBoxKind::kShapedFrameAlias:
      box.flags.kind = static_cast<std::uint8_t>(TextBoxKind::kShapedFrame);
      box.flags.frame_shape = 0;
      break;
    case TextBoxKind::kInformation:
      box.chars_per_line = kInformationCharsPerLine;
      box.center_x = kInformationCenterX;
      box.center_y = kInformationCenterY;
      box.x_indent = kInformationXIndent;
      box.y_indent =
          math::WrapSigned16(static_cast<std::uint32_t>(math::WrappingSubtract(
              kInformationBottom,
              math::WrappingMultiply(layout.line_count,
                                     kInformationLineHeight))));
      break;
    case TextBoxKind::kSecondShapedFrameAlias:
      box.flags.kind = static_cast<std::uint8_t>(TextBoxKind::kShapedFrame);
      box.flags.frame_shape = kSecondShapedFrame;
      break;
    default:
      break;
  }
}

// decomp: BATTLE.PRG vs_battle_dismissTextBox (F01412, 5BF94.c:4613,
// 0x800CD064-0x800CD0FC). The speaker's animation stops even for a closed
// box.
bool DismissTextBox(TextBox& box, TextBoxHost& host) {
  if (box.flags.HasSpeaker()) {
    host.AnimateSpeaker(box.flags.speaker_texture, 0, 0);
  }
  if (box.state == 0) return false;
  if (box.state > 0) {
    box.open_duration = kDismissDuration;
    box.state = -1;
    box.flags.open_style = 0;
  }
  return true;
}
}  // namespace VS::Menu
