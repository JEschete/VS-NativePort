#include "Menu/TextBoxUpdate.hpp"

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu {
namespace {
namespace math = Core::Math;
constexpr std::int32_t kFullScale = 0x10000;
constexpr std::int32_t kOvershootScale = 0x11000;
constexpr std::int32_t kSpringRange = 0xC000;
constexpr std::int32_t kSpringStart = 0x6000;
constexpr std::int32_t kMinimumSpringDuration = 3;
constexpr std::int32_t kQuarterTurn = 1024;
constexpr std::int32_t kSineToScale = 16;
constexpr std::int32_t kColumnPixels = 12;
constexpr std::int32_t kRowPixels = 13;
constexpr std::int32_t kHorizontalPadding = 10;
constexpr std::int32_t kVerticalPadding = 4;
constexpr int kScaleShift = 16;

std::int32_t OpeningScale(std::int32_t elapsed, std::int32_t duration,
                          std::uint8_t style) {
  switch (style) {
    case 0:
    case 1:
      return math::WrappingMultiply(
          math::Sine(math::R3000Quotient(
              math::WrappingMultiply(elapsed, kQuarterTurn), duration)),
          kSineToScale);
    case 2:
      if (elapsed < duration - 1) {
        return math::R3000Quotient(math::WrappingMultiply(elapsed, kFullScale),
                                   duration - 2);
      }
      return elapsed == duration ? kFullScale : kOvershootScale;
    default:
      if (duration >= kMinimumSpringDuration) {
        if (elapsed < duration - 1) {
          return math::WrappingAdd(
              math::R3000Quotient(math::WrappingMultiply(elapsed, kSpringRange),
                                  duration - 2),
              kSpringStart);
        }
        return elapsed == duration ? kFullScale : kOvershootScale;
      }
      return elapsed;
  }
}

std::int16_t Scaled(std::int32_t value, std::int32_t scale) {
  return math::WrapSigned16(static_cast<std::uint32_t>(
      math::WrappingMultiply(value, scale) >> kScaleShift));
}

std::int16_t Interpolate(std::int16_t position, std::int16_t center,
                         std::int32_t scale) {
  const auto total =
      math::WrappingAdd(math::WrappingMultiply(position, scale),
                        math::WrappingMultiply(
                            center, math::WrappingSubtract(kFullScale, scale)));
  return math::WrapSigned16(static_cast<std::uint32_t>(total >> kScaleShift));
}

void DrawBox(TextBoxUpdateHost& host, std::size_t index, TextBoxKind kind,
             std::int32_t state, std::int32_t duration, std::int32_t scale) {
  switch (kind) {
    case TextBoxKind::kPlain:
      if (state < 0) {
        host.BlendText(index, scale);
        return;
      }
      break;
    case TextBoxKind::kInformation:
      host.DrawInformationBackdrop(index);
      if (state < 0) return;
      break;
    case TextBoxKind::kBlendOnly:
      host.BlendText(index, scale);
      return;
    case TextBoxKind::kShapedFrameLegacy:
    case TextBoxKind::kShapedFrame:
      host.DrawFrame(index);
      break;
    case TextBoxKind::kHidden:
      return;
    default:
      break;
  }
  if (state == duration + 1) {
    host.DrawProgressiveText(index);
  } else if (state < 0) {
    host.DrawScaledText(index, scale);
  }
}

void UpdateBox(TextBoxUpdateHost& host, std::size_t index) {
  auto box = host.ReadBox(index);
  std::int32_t state = box.state;
  if (state == 0 || box.flags.frozen) return;
  const auto duration =
      static_cast<std::int32_t>(static_cast<std::uint8_t>(box.open_duration));
  const auto step = host.TickSpeed() >> 1;
  std::int32_t scale = 0;
  if (state < 0) {
    state = math::WrappingSubtract(state, step);
    if (state < -duration) {
      box.state = 0;
      host.WriteBox(index, box);
      return;
    }
    scale = math::R3000Quotient(
        math::WrappingMultiply(
            math::WrappingAdd(math::WrappingAdd(duration, state), 1),
            kFullScale),
        duration);
  } else {
    state = math::WrappingAdd(state, step);
    if (duration < state) state = duration + 1;
    scale = OpeningScale(math::WrappingSubtract(state, 1), duration,
                         box.flags.open_style);
  }
  box.state = math::WrapSigned16(static_cast<std::uint32_t>(state));
  box.frame_x = Interpolate(box.x, box.center_x, scale);
  box.frame_y = Interpolate(box.y, box.center_y, scale);
  box.frame_width =
      Scaled(static_cast<std::uint8_t>(box.chars_per_line) * kColumnPixels +
                 kHorizontalPadding,
             scale);
  box.frame_height = Scaled(
      static_cast<std::uint8_t>(box.line_count) * kRowPixels + kVerticalPadding,
      scale);
  host.WriteBox(index, box);
  // Draw decisions use the full register, not the narrowed state stored above.
  DrawBox(host, index, static_cast<TextBoxKind>(box.flags.kind), state,
          duration, scale);
}
}  // namespace

// decomp: func_800CB83C (src/BATTLE/BATTLE.PRG/5BF94.c:3502,
// original instructions 800CB83C-800CBBCC).
TextBoxUpdateStatus UpdateTextBoxes(TextBoxUpdateHost& host) {
  for (std::size_t index = kTextBoxCount; index-- > 0;) {
    UpdateBox(host, index);
  }
  host.SubmitOrdering();
  auto status = TextBoxUpdateStatus::kComplete;
  const auto buffer = host.OrderingBufferIndex();
  if (buffer < kTextBoxOrderingBufferCount) {
    host.BeginOrderingBuffer(buffer == kTextBoxOrderingBufferCount - 1
                                 ? 0
                                 : static_cast<std::uint8_t>(buffer + 1));
  } else {
    status = TextBoxUpdateStatus::kInvalidOrderingBuffer;
  }
  if (host.TimerVisible()) host.DrawTimer();
  host.DrawOverlay();
  host.AdvanceRandomBits();
  return status;
}
}  // namespace VS::Menu
