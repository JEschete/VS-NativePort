#include "Battle/SphereTargetLabels.hpp"

#include <utility>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;

constexpr int kRightward = 1;
constexpr int kLeftward = -1;

// The link is 16 segments; positions along it are in 1/16 steps of its length.
constexpr int kLinkSegments = 16;
constexpr int kLinkFractionBits = 4;
constexpr int kLinkHalfFractionBits = kLinkFractionBits + 1;
constexpr int kLinkRound = 1 << kLinkFractionBits;
constexpr int kLinkGrayScale = 192;
constexpr int kLinkBlueScale = 255;
constexpr int kLinkBlueFloor = 32;
constexpr int kGreenShift = 8;
constexpr int kBlueShift = 16;

constexpr int kLabelOrderingSlot = 2;
constexpr int kLabelGradientSteps = 8;
constexpr int kLabelFadeRowScale = 8;
constexpr int kLabelBrightness = 0x80;
constexpr std::uint32_t kLabelTextColor = 0x404040;
constexpr std::uint32_t kSelectedLabelTextColor = 0x808080;
constexpr int kRightLabelTextInset = 6;
constexpr int kLeftLabelTextInset = 0x42;
constexpr int kLeftLabelShadowWiden = 2;
constexpr int kLabelShadowOffset = 2;
constexpr int kLinkRowOffset = 4;
constexpr int kLeftLabelRingInset = kTargetLabelWidth - 1;
// The selected label's pulse: its head trails the ring phase by two segments
// and, once the phase reaches 18, a second pulse runs 16 phases behind.
constexpr int kPulseHeadLag = 2;
constexpr int kSecondPulsePhase = 18;
constexpr int kSecondPulseOffset = 14;
constexpr int kPulsePhaseMask = 0x1F;

constexpr std::int16_t kRightColumnRestX = 0xF8;
constexpr int kLeftColumnRestX = -4;
constexpr int kLabelExitStep = 0x20;
constexpr int kLeftRestLead = 4;
constexpr int kVisibleLeftX = -126;
constexpr int kVisibleRightX = 320;

constexpr int kFirstPositionTargetShift = 4;
static_assert(kFirstPositionTargetId == 1 << kFirstPositionTargetShift);

constexpr std::uint32_t kPacketLinkMask = 0xFFFFFF;
constexpr int kPacketLengthShift = 24;
constexpr std::uint32_t kShadowTileCommand = 0x60000000;
constexpr std::uint32_t kDitherDrawMode = 0xE1000200;
constexpr std::uint32_t kGouraudQuadCommand = 0x38000000;
constexpr std::uint32_t kClearDrawMode = 0xE1000000;
constexpr int kCoordinateBits = 16;
constexpr std::uint32_t kCoordinateMask = 0xFFFFU;

LabelScreenPoint ScreenPoint(std::int32_t x, std::int32_t y) noexcept {
  return {.x = math::WrapSigned16(static_cast<std::uint32_t>(x)),
          .y = math::WrapSigned16(static_cast<std::uint32_t>(y))};
}

std::uint32_t PackedPoint(std::int32_t x, std::int32_t y) noexcept {
  return (static_cast<std::uint32_t>(x) & kCoordinateMask) |
         (static_cast<std::uint32_t>(y) << kCoordinateBits);
}

std::uint32_t PackedPoint(LabelScreenPoint point) noexcept {
  return PackedPoint(point.x, point.y);
}

// Gray rising to 192 and blue from 32 to 255 as the segment nears the head.
std::uint32_t LinkSegmentColor(int level) noexcept {
  const auto gray =
      static_cast<std::uint32_t>((level * kLinkGrayScale) >> kLinkFractionBits);
  const auto blue = static_cast<std::uint32_t>(
      (level * kLinkBlueScale + (kLinkSegments - level) * kLinkBlueFloor) >>
      kLinkFractionBits);
  return gray | (gray << kGreenShift) | (blue << kBlueShift);
}

// The shared body of func_800C16FC and func_800C1A40; `direction` is the
// screen x direction from the anchor towards the ring's column. The inputs are
// halfwords, so every intermediate fits a word without wrapping.
void DrawTargetLabelLink(LabelScreenPoint anchor, LabelScreenPoint ring,
                         int pulse_head, int direction,
                         TargetLabelLinkCanvas& canvas) {
  const std::int32_t anchor_x = anchor.x;
  std::int32_t anchor_y = anchor.y;
  const std::int32_t ring_x = ring.x;
  const std::int32_t ring_y = ring.y;

  std::int32_t rise = anchor_y - ring_y;
  std::int32_t sign_y = 1;
  if (rise < 0) {
    sign_y = -1;
    rise = -rise;
  }
  if ((rise & 1) != 0) anchor_y -= sign_y;
  const std::int32_t half_rise = rise >> 1;
  const std::int32_t elbow_x = anchor_x + direction * half_rise;
  std::int32_t run = direction * (ring_x - elbow_x);
  std::int32_t sign_x = 1;
  if (run < 0) {
    sign_x = -1;
    run = -run;
  }
  const std::int32_t length = half_rise * 2 + run;

  // Points on the diagonal, `distance` sixteenths from the ring end.
  const auto diagonal_x = [&](std::int32_t distance) {
    return anchor_x + direction * (distance >> kLinkHalfFractionBits);
  };
  const auto horizontal_x = [&](std::int32_t position) {
    return ring_x - direction * position * sign_x;
  };

  std::int32_t segment_start = 0;
  std::int32_t segment_end = length;
  for (int segment = 0; segment < kLinkSegments;
       ++segment, segment_start += length, segment_end += length) {
    if (pulse_head < segment) continue;
    const int level = segment - pulse_head + kLinkSegments;
    if (level < 0) continue;
    const std::uint32_t command_color =
        kTargetLabelLinkCommand | LinkSegmentColor(level);

    const std::int32_t start = segment_start >> kLinkFractionBits;
    if (start < run) {
      const std::int32_t end = segment_end >> kLinkFractionBits;
      if (end < run) {
        canvas.DrawLinkLine(
            {.command_color = command_color,
             .start = ScreenPoint(horizontal_x(start), ring_y),
             .end = ScreenPoint(horizontal_x(end - 1), ring_y)});
        continue;
      }
      canvas.DrawLinkLine(
          {.command_color = command_color,
           .start = ScreenPoint(horizontal_x(start), ring_y),
           .end = ScreenPoint(elbow_x + direction * sign_x, ring_y)});
      const std::int32_t remaining = length * (kLinkSegments - 1 - segment);
      if (ring_y != anchor_y - (remaining >> kLinkFractionBits) * sign_y) {
        canvas.DrawLinkLine(
            {.command_color = command_color,
             .start = ScreenPoint(elbow_x, ring_y),
             .end = ScreenPoint(
                 diagonal_x(remaining),
                 anchor_y - ((remaining + kLinkRound) >> kLinkFractionBits) *
                                sign_y)});
      }
      continue;
    }
    const std::int32_t far = length * (kLinkSegments - segment);
    const std::int32_t near = length * (kLinkSegments - 1 - segment);
    canvas.DrawLinkLine(
        {.command_color = command_color,
         .start = ScreenPoint(diagonal_x(far),
                              anchor_y - (far >> kLinkFractionBits) * sign_y),
         .end = ScreenPoint(
             diagonal_x(near),
             anchor_y - ((near + kLinkRound) >> kLinkFractionBits) * sign_y)});
  }
  canvas.SetLinkDrawMode(kTargetLabelLinkDrawMode);
}

void SlideLeft(SphereTargetLabel& label,
               const LabelSlideSteps& slide_steps) noexcept {
  const std::int32_t x = label.screen_x;
  if (x < kTargetLabelColumnSplitX) {
    label.screen_x = math::WrapSigned16(
        static_cast<std::uint32_t>(math::WrappingSubtract(x, kLabelExitStep)));
    return;
  }
  std::size_t step = 0;
  while (step < slide_steps.size() &&
         slide_steps[step] + kRightColumnRestX < x) {
    ++step;
  }
  if (step != 0) {
    label.screen_x =
        static_cast<std::int16_t>(slide_steps[step - 1] + kRightColumnRestX);
    return;
  }
  label.screen_x = kRightColumnRestX;
  label.state = kTargetLabelSettled;
}

void SlideRight(SphereTargetLabel& label,
                const LabelSlideSteps& slide_steps) noexcept {
  const std::int32_t x = label.screen_x;
  if (x >= kTargetLabelColumnSplitX) {
    label.screen_x = math::WrapSigned16(
        static_cast<std::uint32_t>(math::WrappingAdd(x, kLabelExitStep)));
    return;
  }
  std::size_t step = 0;
  while (step < slide_steps.size() &&
         x + kLeftRestLead < -static_cast<std::int32_t>(slide_steps[step])) {
    ++step;
  }
  std::int32_t lead = 0;
  if (step != 0) {
    lead = -static_cast<std::int32_t>(slide_steps[step - 1]);
  } else {
    label.state = kTargetLabelSettled;
  }
  label.screen_x = static_cast<std::int16_t>(lead + kLeftColumnRestX);
}
}  // namespace

LabelBoxPacket BuildTargetLabelBoxPacket(const TargetLabelBox& box,
                                         std::uint32_t next_link) noexcept {
  constexpr auto kBodyWords =
      static_cast<std::uint32_t>(kTargetLabelBoxPacketWords - 1);
  const std::int32_t left = box.position.x;
  const std::int32_t right = left + kTargetLabelWidth;
  const std::int32_t top = box.position.y;
  const std::int32_t bottom = top + kTargetLabelHeight;
  return {(next_link & kPacketLinkMask) | (kBodyWords << kPacketLengthShift),
          kShadowTileCommand,
          PackedPoint(box.shadow_position),
          PackedPoint(box.shadow_width, kTargetLabelHeight),
          kDitherDrawMode,
          kGouraudQuadCommand | box.left_color,
          PackedPoint(left, top),
          box.right_color,
          PackedPoint(right, top),
          box.left_color,
          PackedPoint(left, bottom),
          box.right_color,
          PackedPoint(right, bottom),
          kClearDrawMode};
}

// decomp: BATTLE.PRG func_800C16FC (src/BATTLE/BATTLE.PRG/58578.c:477),
// F01298, 0x800C16FC-0x800C1A3C.
void DrawRightTargetLabelLink(LabelScreenPoint anchor, LabelScreenPoint ring,
                              int pulse_head, TargetLabelLinkCanvas& canvas) {
  DrawTargetLabelLink(anchor, ring, pulse_head, kRightward, canvas);
}

// decomp: BATTLE.PRG func_800C1A40 (src/BATTLE/BATTLE.PRG/58578.c:561),
// F01299, 0x800C1A40-0x800C1D80.
void DrawLeftTargetLabelLink(LabelScreenPoint anchor, LabelScreenPoint ring,
                             int pulse_head, TargetLabelLinkCanvas& canvas) {
  DrawTargetLabelLink(anchor, ring, pulse_head, kLeftward, canvas);
}

// decomp: BATTLE.PRG func_800C1D84 (src/BATTLE/BATTLE.PRG/58578.c:645),
// F01300, 0x800C1D84-0x800C1DC0.
bool AreTargetLabelsSettled(
    std::span<const SphereTargetLabel, kSphereTargetLabelCount>
        labels) noexcept {
  for (const SphereTargetLabel& label : labels) {
    if (label.state >= kTargetLabelSlidingLeft) return false;
  }
  return true;
}

// decomp: BATTLE.PRG func_800C1DC4 (src/BATTLE/BATTLE.PRG/58578.c:656),
// F01301, 0x800C1DC4-0x800C20B0.
void DrawSphereTargetLabel(SphereTargetLabel& label,
                           std::uint8_t column_top_offset,
                           SphereTargetLabelCanvas& canvas) {
  const bool selected = label.depth != 0;
  const int overlay_slot = kLabelOrderingSlot - label.depth;
  const int gradient = selected ? kLabelGradientSteps : label.gradient;
  const int color_index = label.fade * kLabelFadeRowScale;
  std::uint32_t left_color = canvas.GradientStopColor(
      kLabelGradientSteps - gradient, color_index, kLabelBrightness);
  std::uint32_t right_color =
      canvas.GradientStopColor(gradient, color_index, kLabelBrightness);
  const bool left_column = label.screen_x < kTargetLabelColumnSplitX;
  if (left_column) std::swap(left_color, right_color);

  const std::int32_t x = label.screen_x;
  std::int32_t y = label.offset_y + column_top_offset;
  canvas.DrawText(
      {.position = ScreenPoint(
           x + (left_column ? kLeftLabelTextInset : kRightLabelTextInset),
           y - 1),
       .color = selected ? kSelectedLabelTextColor : kLabelTextColor,
       .overlay_slot = overlay_slot});
  if (gradient != 0) label.gradient = static_cast<std::uint8_t>(gradient - 1);

  const int shadow_widen = left_column ? kLeftLabelShadowWiden : 0;
  canvas.DrawBox(
      {.shadow_position = ScreenPoint(x + kLabelShadowOffset - shadow_widen,
                                      y + kLabelShadowOffset),
       .shadow_width =
           static_cast<std::int16_t>(kTargetLabelWidth + shadow_widen),
       .position = ScreenPoint(x, y),
       .left_color = left_color,
       .right_color = right_color,
       .overlay_slot = overlay_slot});
  if (!selected) return;

  y += kLinkRowOffset;
  canvas.BeginProjection();
  const LabelScreenPoint anchor = canvas.Project(label.world_position);
  const int pulse_head = label.pulse_phase - kPulseHeadLag;
  const bool second_pulse = label.pulse_phase >= kSecondPulsePhase;
  const int second_head =
      (label.pulse_phase + kSecondPulseOffset) & kPulsePhaseMask;
  if (left_column) {
    const LabelScreenPoint ring = ScreenPoint(x + kLeftLabelRingInset, y);
    DrawLeftTargetLabelLink(anchor, ring, pulse_head, canvas);
    if (second_pulse) {
      DrawLeftTargetLabelLink(anchor, ring, second_head, canvas);
    }
    return;
  }
  const LabelScreenPoint ring = ScreenPoint(x, y);
  DrawRightTargetLabelLink(anchor, ring, pulse_head, canvas);
  if (second_pulse) {
    DrawRightTargetLabelLink(anchor, ring, second_head, canvas);
  }
}

// decomp: BATTLE.PRG func_800C20B4 (src/BATTLE/BATTLE.PRG/58578.c:750),
// F01302, 0x800C20B4-0x800C2250.
void UpdateSphereTargetLabels(
    std::span<SphereTargetLabel, kSphereTargetLabelCount> labels,
    const LabelSlideSteps& slide_steps, SphereTargetLabelDrawer& drawer) {
  for (std::size_t index = 0; index < labels.size(); ++index) {
    SphereTargetLabel& label = labels[index];
    if (label.state == kTargetLabelHidden) continue;
    if (label.state == kTargetLabelSlidingLeft) {
      SlideLeft(label, slide_steps);
    } else if (label.state == kTargetLabelSlidingRight) {
      SlideRight(label, slide_steps);
    }
    if (label.screen_x >= kVisibleLeftX && label.screen_x < kVisibleRightX) {
      drawer.DrawLabel(index, label);
    } else {
      label.state = kTargetLabelHidden;
    }
  }
}

// decomp: BATTLE.PRG func_800C4650 (src/BATTLE/BATTLE.PRG/5BE50.c:65),
// F01310, 0x800C4650-0x800C4704.
void UpdateSphereTargetHighlights(std::span<const ActionTargetPreview> targets,
                                  SphereTargetHighlightHost& host) {
  for (const ActionTargetPreview& target : targets) {
    if ((target.actor_id >> kFirstPositionTargetShift) != 0) continue;
    if (host.IsInSecondaryVolume(target.position)) {
      host.EnablePalettePulse(target.actor_id, target.limb);
    } else {
      host.DisablePalettePulse(target.actor_id, target.limb);
    }
  }
}
}  // namespace VS::Battle
