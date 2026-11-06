// Hand-maintained (tools/port/owned.csv).
#include "Event/ScriptDialogCommands.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Render/EncodedText.hpp"

namespace VS::Event {
namespace {
using Core::Math::SquareRootInteger;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;

// Operand offsets after the opcode byte.
constexpr std::size_t kFirstOperand = 1;
constexpr std::size_t kSecondOperand = 2;
constexpr std::size_t kThirdOperand = 3;
constexpr std::size_t kFourthOperand = 4;
constexpr std::size_t kFifthOperand = 5;
constexpr std::size_t kSixthOperand = 6;
constexpr std::size_t kSeventhOperand = 7;
constexpr std::size_t kEighthOperand = 8;
constexpr std::size_t kTenthOperand = 10;

// A box byte that packs two 4-bit fields: the low one is the box the
// command sets up, the high one a centring box or a speaker animation.
constexpr unsigned kNibbleShift = 4;
constexpr unsigned kNibbleMask = 0xF;

// Script screen coordinates: x in 2-pixel units, both offset by 32 pixels.
constexpr int kScriptXScale = 2;
constexpr int kScriptScreenOffset = 32;

// The screen area dialog boxes are clamped into.
constexpr int kScreenMargin = 4;
constexpr int kScreenRight = 320;
constexpr int kScreenBottom = 216;

// An actor's heading, in 4096ths of a turn, rounded to the side it faces.
constexpr int kQuarterTurn = 1024;
constexpr int kQuadrantMask = 0xC00;
constexpr unsigned kQuadrantShift = 10;
constexpr int kOrientationStep = 4;

// Speaker references at and past this one name no actor.
constexpr std::uint16_t kNonActorSpeaker = 0x2000;

// String integer operands: a 10-bit flag id and a 2-bit format.
constexpr int kFlagIdMask = 0x3FF;
constexpr unsigned kFormatShift = 6;
constexpr unsigned kByteBits = 8;
enum class IntegerFormat : std::uint8_t {
  kUnsignedByte = 0,
  kSignedByte = 1,
  kUnsignedHalf = 2,
  kSignedHalf = 3,
};

// A transition byte: the box in its top 3 bits, the ticks in the rest.
constexpr unsigned kTransitionBoxShift = 5;
constexpr unsigned kTransitionTicksMask = 0x1F;

// Refuses an instruction shorter than its operands.
[[nodiscard]] bool Fits(ScriptCommand command, std::size_t length,
                        ScriptDialogHost& host) {
  if (command.size() >= length) {
    return true;
  }
  host.Refuse(ScriptDialogRefusal::kShortCommand,
              static_cast<int>(command.size()));
  return false;
}

// Refuses a box id past the text boxes.
[[nodiscard]] bool ValidBox(unsigned box, ScriptDialogHost& host) {
  if (box < static_cast<unsigned>(kTextBoxCount)) {
    return true;
  }
  host.Refuse(ScriptDialogRefusal::kTextBox, static_cast<int>(box));
  return false;
}

[[nodiscard]] std::uint8_t Byte(ScriptCommand command, std::size_t offset) {
  return OperandByte(command, offset);
}

[[nodiscard]] std::uint16_t Reference(ScriptCommand command,
                                      std::size_t offset) {
  return static_cast<std::uint16_t>(OperandHalf(command, offset));
}

// The layout an init command sets: centred on the box itself when the
// command names it twice, else on the open box it names.
[[nodiscard]] std::optional<TextBoxLayout> InitLayout(ScriptCommand command,
                                                      ScriptDialogHost& host) {
  const std::uint8_t boxes = Byte(command, kFirstOperand);
  const unsigned box = boxes & kNibbleMask;
  const unsigned centring_box = boxes >> kNibbleShift;
  const std::uint8_t left = Byte(command, kFourthOperand);
  const std::uint8_t top = Byte(command, kFifthOperand);
  TextBoxLayout layout{
      .flags = Reference(command, kSecondOperand),
      .x = (left * kScriptXScale) - kScriptScreenOffset,
      .y = top - kScriptScreenOffset,
      .chars_per_line = Byte(command, kSixthOperand),
      .line_count = Byte(command, kSeventhOperand),
  };
  if (!ValidBox(box, host)) {
    return std::nullopt;
  }
  if (box == centring_box) {
    layout.center_x = static_cast<std::int16_t>(
        layout.x + (DialogBoxWidth(layout.chars_per_line) >> 1));
    layout.center_y = static_cast<std::int16_t>(
        layout.y + (DialogBoxHeight(layout.line_count) >> 1));
    return layout;
  }
  if (!ValidBox(centring_box, host)) {
    return std::nullopt;
  }
  const TextBoxFrame frame = host.Frame(static_cast<int>(centring_box));
  layout.center_x = static_cast<std::int16_t>(
      frame.x + (DialogBoxWidth(frame.chars_per_line) >> 1));
  layout.center_y = static_cast<std::int16_t>(
      frame.y + (DialogBoxHeight(frame.line_count) >> 1));
  return layout;
}

// Clamps one axis of a box into [kScreenMargin, limit - size]: the clamped
// start, and how far it moved, as the original's 16-bit locals.
struct AxisClamp {
  std::int16_t start;
  std::int16_t moved;
};
[[nodiscard]] AxisClamp ClampAxis(std::int16_t start, int size, int limit) {
  if (start < kScreenMargin) {
    return {.start = kScreenMargin,
            .moved = static_cast<std::int16_t>(kScreenMargin - start)};
  }
  if (start + size > limit) {
    const auto clamped = static_cast<std::int16_t>(limit - size);
    return {.start = clamped,
            .moved = static_cast<std::int16_t>(clamped - start)};
  }
  return {.start = start, .moved = 0};
}

[[nodiscard]] int ReadDialogInteger(IntegerFormat format, int flag,
                                    ScriptDialogHost& host) {
  switch (format) {
    case IntegerFormat::kUnsignedByte:
      return host.ScriptFlag(flag);
    case IntegerFormat::kSignedByte:
      return static_cast<std::int8_t>(host.ScriptFlag(flag));
    case IntegerFormat::kUnsignedHalf:
    case IntegerFormat::kSignedHalf:
      break;
  }
  const unsigned low = host.ScriptFlag(flag);
  const unsigned high = host.ScriptFlag(flag + 1);
  const auto half = static_cast<std::uint16_t>(low | (high << kByteBits));
  if (format == IntegerFormat::kSignedHalf) {
    return static_cast<std::int16_t>(half);
  }
  return half;
}

}  // namespace

// decomp: vs_battle_script_showTextBox (src/BATTLE/BATTLE.PRG/4A0A8.c:707).
std::expected<ScriptCommand, DialogStringError> ResolveDialogString(
    ScriptCommand table, std::size_t index) {
  constexpr std::size_t kHalfwordBytes = 2;
  if (table.size() < kHalfwordBytes) {
    return std::unexpected(DialogStringError::kTable);
  }
  const std::size_t entries = OperandHalf(table, 0);
  if (entries == 0 || entries > table.size() / kHalfwordBytes) {
    return std::unexpected(DialogStringError::kTable);
  }
  if (index >= entries) {
    return std::unexpected(DialogStringError::kIndex);
  }
  const std::size_t offset =
      OperandHalf(table, index * kHalfwordBytes) * kHalfwordBytes;
  if (offset < entries * kHalfwordBytes || offset >= table.size()) {
    return std::unexpected(DialogStringError::kOffset);
  }
  std::size_t end = table.size();
  for (std::size_t entry = 0; entry < entries; ++entry) {
    const std::size_t next =
        OperandHalf(table, entry * kHalfwordBytes) * kHalfwordBytes;
    if (next > offset && next < end) {
      end = next;
    }
  }
  const auto text = table.subspan(offset, end - offset);
  for (std::size_t cursor = 0; cursor < text.size(); ++cursor) {
    const auto code = text[cursor];
    if (code == Render::kTextTerminator) {
      return text.first(cursor + 1);
    }
    if (code >= Render::kTextFirstParameterized) {
      ++cursor;
      if (cursor >= text.size() ||
          (code == Render::kTextPrintString &&
           text[cursor] >= Render::kEncodedTextArgumentCount)) {
        return std::unexpected(DialogStringError::kControl);
      }
    }
  }
  return std::unexpected(DialogStringError::kTerminator);
}

DialogPlacement CalcDialogAnchorPlacement(DialogOrigin origin,
                                          const DialogAnchor& anchor,
                                          int columns, int lines) {
  const auto x = static_cast<std::int16_t>(origin.x + anchor.x);
  const auto y = static_cast<std::int16_t>(origin.y + anchor.y);
  const AxisClamp horizontal =
      ClampAxis(x, DialogBoxWidth(columns), kScreenRight);
  const AxisClamp vertical =
      ClampAxis(y, DialogBoxHeight(lines), kScreenBottom);
  DialogPlacement placement{.anchor = anchor,
                            .screen_x = horizontal.start,
                            .screen_y = vertical.start};
  placement.fits = horizontal.moved == 0 && vertical.moved == 0;
  if (!placement.fits) {
    placement.clamp_distance = SquareRootInteger(
        WrappingAdd(WrappingMultiply(horizontal.moved, horizontal.moved),
                    WrappingMultiply(vertical.moved, vertical.moved)));
  }
  return placement;
}

ScriptStep ScriptInitDialogBox(ScriptCommand command, ScriptDialogHost& host) {
  if (!Fits(command, kInitDialogBoxLength, host)) {
    return ScriptStep::kAdvance;
  }
  const auto layout = InitLayout(command, host);
  if (!layout) {
    return ScriptStep::kAdvance;
  }
  const int box = static_cast<int>(Byte(command, kFirstOperand) & kNibbleMask);
  host.InitTextBox(box, *layout);
  host.SetSpeakerAnimation(box, Byte(command, kTenthOperand));
  const std::int16_t speaker = OperandHalf(command, kEighthOperand);
  if (speaker < static_cast<std::int16_t>(kNonActorSpeaker)) {
    host.SetSpeakerTexture(
        box, host.ResolveActor(static_cast<std::uint16_t>(speaker)) &
                 kSpeakerTextureMask);
  }
  return ScriptStep::kAdvance;
}

ScriptStep ScriptShowDialogText(ScriptCommand command, ScriptDialogHost& host) {
  if (!Fits(command, kShowDialogTextLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t box = Byte(command, kFirstOperand);
  if (!ValidBox(box, host)) {
    return ScriptStep::kAdvance;
  }
  if (!host.AwaitingClose(box)) {
    host.ShowDialogString(box, Byte(command, kSecondOperand));
    if (Byte(command, kThirdOperand) == 0) {
      host.SetAwaitingClose(box, true);
      return ScriptStep::kHold;
    }
    return ScriptStep::kAdvance;
  }
  const int result = host.PollTextBox(box);
  if (result < 0) {
    return ScriptStep::kHold;
  }
  if (result > 0) {
    host.SetTextBoxSelection(result);
  }
  host.SetAwaitingClose(box, false);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptDismissDialogBox(ScriptCommand command,
                                  ScriptDialogHost& host) {
  if (Fits(command, kDismissDialogBoxLength, host)) {
    const std::uint8_t box = Byte(command, kFirstOperand);
    if (ValidBox(box, host)) {
      static_cast<void>(host.DismissTextBox(box));
    }
  }
  return ScriptStep::kAdvance;
}

ScriptStep ScriptInitActorDialogBox(ScriptCommand command,
                                    ScriptDialogHost& host) {
  if (!Fits(command, kInitActorDialogBoxLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t box = Byte(command, kFirstOperand);
  if (!ValidBox(box, host)) {
    return ScriptStep::kAdvance;
  }
  const DialogOrigin origin = host.ActorDialogOrigin(
      host.ResolveActor(Reference(command, kSecondOperand)));
  const int quadrant =
      ((origin.heading - kQuarterTurn) & kQuadrantMask) >> kQuadrantShift;
  const int preset =
      static_cast<int>(Byte(command, kFifthOperand) & kNibbleMask);
  const int columns = Byte(command, kSixthOperand);
  const int lines = Byte(command, kSeventhOperand);

  std::array<DialogPlacement, kDialogPlacementOrder.size()> placements{};
  std::size_t best = placements.size();
  for (std::size_t side = 0; side < placements.size(); ++side) {
    const int orientation =
        (quadrant ^ kDialogPlacementOrder[side]) * kOrientationStep;
    placements[side] = CalcDialogAnchorPlacement(
        origin, host.TextBoxAnchor(preset, orientation, columns, lines),
        columns, lines);
    if (placements[side].fits) {
      best = side;
      break;
    }
  }
  if (best == placements.size()) {
    best = 0;
    for (std::size_t side = 1; side < placements.size(); ++side) {
      if (placements[best].clamp_distance > placements[side].clamp_distance) {
        best = side;
      }
    }
  }

  const DialogPlacement& placement = placements[best];
  host.InitTextBox(
      box, TextBoxLayout{
               .flags = Reference(command, kFourthOperand),
               .x = placement.screen_x,
               .y = placement.screen_y,
               .chars_per_line = columns,
               .line_count = lines,
               .center_x = placement.screen_x + (DialogBoxWidth(columns) >> 1),
               .center_y = placement.screen_y + (DialogBoxHeight(lines) >> 1),
           });
  host.SetSpeakerTexture(box, kActorDialogSpeakerTexture);
  host.SetAnchorStyle(box, placement.anchor.alignment,
                      placement.anchor.frame_part);
  host.SetSpeakerAnimation(box, Byte(command, kEighthOperand));
  return ScriptStep::kAdvance;
}

ScriptStep ScriptWaitForDialogDismissal(ScriptCommand command,
                                        ScriptDialogHost& host) {
  if (!Fits(command, kWaitForDialogDismissalLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t box = Byte(command, kFirstOperand);
  if (!ValidBox(box, host)) {
    return ScriptStep::kAdvance;
  }
  return host.DismissTextBox(box) != 0 ? ScriptStep::kHold
                                       : ScriptStep::kAdvance;
}

ScriptStep ScriptSetDialogInteger(ScriptCommand command,
                                  ScriptDialogHost& host) {
  if (!Fits(command, kSetDialogIntegerLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t slot = Byte(command, kFirstOperand);
  if (slot >= kDialogIntegerCount) {
    host.Refuse(ScriptDialogRefusal::kDialogInteger, slot);
    return ScriptStep::kAdvance;
  }
  const int flag = OperandHalf(command, kSecondOperand) & kFlagIdMask;
  const auto format =
      static_cast<IntegerFormat>(Byte(command, kThirdOperand) >> kFormatShift);
  host.SetDialogInteger(slot, ReadDialogInteger(format, flag, host));
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetDialogTransitionDuration(ScriptCommand command,
                                             ScriptDialogHost& host) {
  if (Fits(command, kSetDialogTransitionDurationLength, host)) {
    const std::uint8_t transition = Byte(command, kFirstOperand);
    host.SetOpenDuration(transition >> kTransitionBoxShift,
                         static_cast<int>(transition & kTransitionTicksMask));
  }
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSelectDialogChoice(ScriptCommand command,
                                    ScriptDialogHost& host) {
  if (!Fits(command, kSelectDialogChoiceLength, host) || host.SkipActive()) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t box_mask = Byte(command, kFirstOperand);
  if (!host.SelectionActive()) {
    static_cast<void>(host.SelectTextBoxChoice(box_mask));
    host.SetSelectionActive(true);
    return ScriptStep::kHold;
  }
  const auto choice =
      static_cast<std::int16_t>(host.SelectTextBoxChoice(box_mask));
  if (choice == 0) {
    return ScriptStep::kHold;
  }
  host.SetScriptFlag(kDialogChoiceFlag, choice);
  host.SetSelectionActive(false);
  return ScriptStep::kAdvance;
}

ScriptStep ScriptSetDialogSpeaker(ScriptCommand command,
                                  ScriptDialogHost& host) {
  if (!Fits(command, kSetDialogSpeakerLength, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint8_t boxes = Byte(command, kFirstOperand);
  const unsigned box = boxes & kNibbleMask;
  if (!ValidBox(box, host)) {
    return ScriptStep::kAdvance;
  }
  const std::uint16_t speaker = Reference(command, kSecondOperand);
  const int texture = speaker < kNonActorSpeaker
                          ? host.ResolveActor(speaker) & kSpeakerTextureMask
                          : kNoSpeakerTexture;
  host.SetSpeakerTexture(static_cast<int>(box), texture);
  host.SetSpeakerAnimation(static_cast<int>(box), boxes >> kNibbleShift);
  return ScriptStep::kAdvance;
}

}  // namespace VS::Event
