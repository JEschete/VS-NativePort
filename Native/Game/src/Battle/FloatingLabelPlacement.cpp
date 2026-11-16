// Hand-maintained (tools/port/owned.csv).
#include "Battle/FloatingLabelPlacement.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

#include "Battle/FloatingLabelProducers.hpp"
#include "Battle/FloatingLabelQueue.hpp"
#include "Battle/FloatingLabelRendering.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;

// The updater (2842C.c:1757-1969).
constexpr std::int16_t kStepTimerLimit = 20;
constexpr std::uint8_t kLockedLabelMode = 2;
constexpr std::uint8_t kFirstUnanchoredKind = 3;
constexpr std::int16_t kAnchoredLeft = 32;
constexpr std::int16_t kAnchoredRight = 288;
constexpr std::int16_t kAnchoredTop = 40;
constexpr std::int16_t kAnchoredBottom = 200;
constexpr std::int32_t kValueLabelDrop = 8;
constexpr std::uint8_t kSettledFrame = 10;
constexpr std::uint8_t kLabelEndFrame = 31;
constexpr std::uint8_t kTimingEndFrame = 46;
constexpr std::uint8_t kChainEndFrame = 91;
constexpr std::uint8_t kEquipmentEndFrame = 61;
constexpr std::int16_t kStackLift = 10;
constexpr std::int16_t kFinishedWaiting = -1;
constexpr FloatingLabelScreenPoint kTimingOrigin{44, 48};
constexpr FloatingLabelScreenPoint kChainOrigin{44, 64};
constexpr FloatingLabelScreenPoint kEquipmentOrigin{8, 172};

// The placement search (2842C.c:1971-2065).
constexpr std::int32_t kOverlapWidth = 24;
constexpr std::int32_t kOverlapHeight = 10;
constexpr std::int32_t kSearchStep = 8;
constexpr std::int32_t kSearchHeight = 48;
constexpr std::int32_t kSearchWidth = 32;

// The digits and captions (2842C.c:2073-2136, 2210-2310, 2765-2862).
constexpr std::uint8_t kFirstCaptionMode = 3;
constexpr std::uint16_t kNegativeValue = 0x8000;
constexpr std::uint8_t kLastBounceFrame = 15;
constexpr std::size_t kNegativeBounceShift = 4;
constexpr std::int32_t kDigitAdvance = 6;
constexpr std::int32_t kDigitWidth = 8;
constexpr std::int32_t kDigitHalfHeight = 6;
constexpr std::uint32_t kDigitVisibleSpan = 328;
constexpr std::int32_t kDigitAtlasStride = 8;
constexpr std::int32_t kDigitAtlasU = 64;
constexpr std::int32_t kValueAdvance = 3;
constexpr std::uint16_t kDigitPage = 0x0C;
constexpr std::uint16_t kValueDigitPalette = 0x37F4;
constexpr std::uint8_t kRemovalFlag = 1;
constexpr std::uint8_t kSlideFrames = 4;
constexpr std::uint8_t kHoldStartFrame = 6;
constexpr std::uint8_t kBrightStartFrame = 25;
constexpr std::uint8_t kClosingFrame = 27;
constexpr std::int32_t kLastFrame = 30;
constexpr std::int32_t kHeldStep = 4;
constexpr std::int32_t kBrightStep = 5;
constexpr std::uint8_t kRemovalMarkerFrame = 15;
constexpr std::int32_t kLastRemovalMarkerStep = 4;
constexpr std::int32_t kScreenWidth = 320;
constexpr std::int32_t kStatusHalfHeight = 6;
constexpr std::uint8_t kBrightBias = 96;
constexpr std::int32_t kFadeScale = 32;
constexpr std::uint16_t kFadingPage = 0x37;
constexpr std::uint16_t kFadingPalette = 0x373C;
constexpr std::uint16_t kHeldPage = 0x17;
constexpr std::uint16_t kHeldPalette = 0x373D;
constexpr std::int32_t kWordGap = 2;
constexpr std::int32_t kSignWidth = 8;
constexpr std::int32_t kSignTop = 1;
constexpr std::int32_t kSignBottom = 7;
constexpr std::int32_t kSignAtlasU = 120;
constexpr std::uint8_t kSignAtlasV = 208;
constexpr std::int32_t kStatDigitAdvance = 5;
constexpr std::int32_t kStatDigitWidth = 6;
constexpr std::int32_t kStatDigitBottom = 9;
constexpr std::uint8_t kStatDigitHeight = 10;
constexpr std::uint16_t kStatWordPalette = 0x37F5;
constexpr std::uint16_t kStatDigitPalette = 0x37F4;

constexpr std::size_t kBounceFrames = 20;
using Bounce = std::array<std::int16_t, kBounceFrames>;
constexpr Bounce kBounce{{
    0,  -6, -12, -14, -16, -10, -5, -1, 0,  -3,  //
    -6, -7, -8,  -5,  -3,  -1,  0,  -2, -1, 0,   //
}};
constexpr std::array<Render::QuadColor, 4> kValueColors{{
    {128, 128, 128},
    {0, 128, 0},
    {128, 0, 128},
    {0, 128, 128},
}};
constexpr std::array<std::int32_t, kSlideFrames> kStatusSlide{32, 14, 6, 2};
constexpr std::array<std::int32_t, kSlideFrames> kStatusFade{5, 9, 13, 15};
// Indexed by the positive flag: a negative change is orange, a positive blue.
constexpr std::array<Render::QuadColor, 2> kStatChangeColors{{
    {128, 64, 0},
    {0, 64, 128},
}};

std::int16_t Wrap16(std::int32_t value) {
  return math::WrapSigned16(static_cast<std::uint32_t>(value));
}

std::int16_t Add16(std::int16_t left, std::int32_t right) {
  return Wrap16(math::WrappingAdd(left, right));
}

Render::ScreenVertex Vertex(std::int32_t x, std::int32_t y) {
  return {Wrap16(x), Wrap16(y)};
}

// A modulated (shade-textured) quad with axis-aligned corners.
FloatingLabelQuad TintedQuad(FloatingLabelScreenPoint top_left,
                             FloatingLabelScreenPoint bottom_right,
                             const FloatingLabelAtlasRect& atlas,
                             Render::QuadColor tint, std::uint16_t page,
                             std::uint16_t palette) {
  return {.top_left = Vertex(top_left.x, top_left.y),
          .bottom_right = Vertex(bottom_right.x, bottom_right.y),
          .texture = {.u = static_cast<std::uint8_t>(atlas.u),
                      .v = static_cast<std::uint8_t>(atlas.v),
                      .width = static_cast<std::uint8_t>(atlas.width),
                      .height = static_cast<std::uint8_t>(atlas.height),
                      .texture_page = page,
                      .clut = palette},
          .tint = tint,
          .raw_texture = false};
}

// A signed text byte's digit cell, as the original's lb and subtraction.
std::int32_t DigitCell(std::uint8_t text_byte) {
  return static_cast<std::int8_t>(text_byte) - '0';
}

std::uint8_t AddFrames(std::uint8_t frame, std::int32_t half_tick) {
  return static_cast<std::uint8_t>(math::WrappingAdd(frame, half_tick));
}

bool Overlaps(LabelScreenPoint point, LabelScreenPoint placed) {
  return std::abs(point.x - placed.x) < kOverlapWidth &&
         std::abs(point.y - placed.y) < kOverlapHeight;
}

bool AnyOverlap(std::int32_t x, std::int32_t y,
                const std::array<LabelScreenPoint, kPlacedLabelCapacity>& taken,
                std::int32_t count) {
  for (std::int32_t index = 0; index < count; ++index) {
    const auto& placed = taken[static_cast<std::size_t>(index)];
    if (std::abs(y - placed.y) < kOverlapHeight &&
        std::abs(x - placed.x) < kOverlapWidth) {
      return true;
    }
  }
  return false;
}

// The first free offset nearest the anchor: rows downward, then columns
// outward, the right side before the left.
std::optional<LabelScreenPoint> FreeOffset(
    LabelScreenPoint position,
    const std::array<LabelScreenPoint, kPlacedLabelCapacity>& taken,
    std::int32_t count) {
  for (std::int32_t dy = 0; dy < kSearchHeight; dy += kSearchStep) {
    for (std::int32_t dx = 0; dx < kSearchWidth; dx += kSearchStep) {
      const auto y = position.y + dy;
      if (!AnyOverlap(position.x + dx, y, taken, count)) {
        return LabelScreenPoint{Wrap16(dx), Wrap16(dy)};
      }
      if (!AnyOverlap(position.x - dx, y, taken, count)) {
        return LabelScreenPoint{Wrap16(-dx), Wrap16(dy)};
      }
    }
  }
  return std::nullopt;
}

FloatingLabelAppearance Appearance(const FloatingLabelRecord& label) {
  return {.elapsed_frames = label.frame,
          .value = label.value,
          .raw_mode = label.color_flags,
          .text_length = label.text_length};
}

struct ValueDigits {
  std::int32_t width = 0;
  std::size_t color = 0;
  std::int32_t offset = 0;
};

std::optional<ValueDigits> ValueLayout(FloatingLabelFeedbackWorld& world,
                                       FloatingLabelScreenPoint& origin,
                                       const FloatingLabelRecord& label) {
  const bool negative = (label.value & kNegativeValue) != 0;
  if (label.color_flags >= kFirstCaptionMode) {
    const std::int32_t rise = label.frame >> 1;
    origin.y = negative ? math::WrappingAdd(origin.y, rise)
                        : math::WrappingSubtract(origin.y, rise);
    const auto width = DrawValueLabelCaption(world, origin, Appearance(label));
    if (!width) return std::nullopt;
    return ValueDigits{.width = width.value()};
  }
  const auto frame = std::min(label.frame, kLastBounceFrame);
  ValueDigits digits{
      .width = label.text_length * kValueAdvance,
      .color = static_cast<std::size_t>((label.color_flags & 1U) << 1U)};
  if (negative) {
    digits.offset = kBounce[frame + kNegativeBounceShift];
  } else {
    digits.offset = kBounce[frame];
    ++digits.color;
  }
  return digits;
}

struct StatusStep {
  std::int32_t step = 0;
  std::int32_t slide = 0;
};

std::optional<StatusStep> StatusAnimation(const FloatingLabelRecord& label) {
  const bool removal = (label.color_flags & kRemovalFlag) != 0;
  const std::int32_t frame = label.frame;
  if (label.frame < kSlideFrames) {
    return StatusStep{frame, removal ? 0 : kStatusSlide[label.frame]};
  }
  if (label.frame < kHoldStartFrame) return StatusStep{kBrightStep, 0};
  if (label.frame >= kClosingFrame) {
    const auto step = kLastFrame - frame;
    if (step < 0) return std::nullopt;
    return StatusStep{
        step, removal ? 0 : -kStatusSlide[static_cast<std::size_t>(step)]};
  }
  return StatusStep{label.frame >= kBrightStartFrame ? kBrightStep : kHeldStep,
                    0};
}

std::uint8_t StatusChannel(std::uint8_t channel, std::int32_t step) {
  if (step == kBrightStep) {
    return static_cast<std::uint8_t>(channel / 2 + kBrightBias);
  }
  if (step == kHeldStep) return static_cast<std::uint8_t>(channel / 2);
  return static_cast<std::uint8_t>(
      channel * kStatusFade[static_cast<std::size_t>(step)] / kFadeScale);
}

Render::QuadColor StatusTint(Render::QuadColor color, std::int32_t step) {
  return {StatusChannel(color.r, step), StatusChannel(color.g, step),
          StatusChannel(color.b, step)};
}

struct StatusCaptions {
  std::array<FloatingLabelAtlasRect, kStatusLabelCaptions> rects{};
  std::size_t count = 0;
  std::int32_t width = 0;
  Render::QuadColor color{};
};

std::optional<StatusCaptions> StatusParts(FloatingLabelFeedbackWorld& world,
                                          const FloatingLabelRecord& label,
                                          const StatusLabelLayout& layout) {
  if (layout.count > kStatusLabelCaptions) return std::nullopt;
  StatusCaptions parts{.count = layout.count};
  for (std::size_t index = 0; index < parts.count; ++index) {
    const auto rect = world.CaptionRect(layout.captions[index]);
    if (!rect) return std::nullopt;
    parts.rects[index] = rect.value();
    parts.width = math::WrappingAdd(parts.width, rect->width);
  }
  const bool removal = (label.color_flags & kRemovalFlag) != 0;
  const auto color = world.StatusColor(removal ? 0 : layout.color);
  if (!color) return std::nullopt;
  parts.color = color.value();
  return parts;
}

bool DrawStatusCaptions(FloatingLabelFeedbackWorld& world,
                        FloatingLabelScreenPoint origin,
                        const StatusCaptions& parts, std::int32_t step) {
  const bool held = step == kHeldStep;
  const auto tint = StatusTint(parts.color, step);
  for (std::size_t index = 0; index < parts.count; ++index) {
    auto atlas = parts.rects[index];
    const auto right = math::WrappingAdd(origin.x, atlas.width);
    if (origin.x < kScreenWidth && right >= 0) {
      atlas.height = kStatusHalfHeight * 2;
      auto quad = TintedQuad(
          {origin.x, math::WrappingSubtract(origin.y, kStatusHalfHeight)},
          {right, math::WrappingAdd(origin.y, kStatusHalfHeight)}, atlas, tint,
          held ? kHeldPage : kFadingPage, held ? kHeldPalette : kFadingPalette);
      quad.translucent = true;
      if (!world.DrawQuad(quad)) return false;
    }
    origin.x = right;
  }
  return true;
}

bool DrawStatWord(FloatingLabelFeedbackWorld& world,
                  FloatingLabelScreenPoint& origin,
                  const FloatingLabelAtlasRect& atlas, Render::QuadColor tint) {
  const auto right = math::WrappingAdd(origin.x, atlas.width);
  if (!world.DrawEquipmentQuad(
          TintedQuad(origin, {right, math::WrappingAdd(origin.y, atlas.height)},
                     atlas, tint, kDigitPage, kStatWordPalette))) {
    return false;
  }
  origin.x = math::WrappingAdd(right, kWordGap);
  return true;
}

// The updater's per-label state: the record, its index and the screen point.
struct LabelPass {
  FloatingLabelRecord record{};
  std::int32_t index = 0;
  LabelScreenPoint screen{};
  std::int32_t half_tick = 0;
  std::int32_t removed = 0;
};

void RemoveLabel(LabelPass& pass) {
  pass.record.kind = static_cast<std::uint8_t>(FloatingLabelKind::kInactive);
  ++pass.removed;
}

std::uint8_t StripRepeatedLimb(std::uint8_t color_flags) {
  return static_cast<std::uint8_t>(color_flags & kLabelColorMask);
}

void Advance(FloatingLabelRecord& record) {
  AdvanceFloatingLabelQueue({record.position.w, record.frame});
}

// Ages a shown actor label: settled after kSettledFrame, gone at the end.
void AgeShownLabel(LabelPass& pass) {
  auto& record = pass.record;
  record.frame = AddFrames(record.frame, pass.half_tick);
  if (record.frame >= kSettledFrame) record.position.w = kFinishedWaiting;
  if (record.frame >= kLabelEndFrame) RemoveLabel(pass);
}

// Ages a waiting actor label: it moves up the queue after kSettledFrame.
void AgeWaitingLabel(LabelPass& pass) {
  auto& record = pass.record;
  record.frame = AddFrames(record.frame, pass.half_tick);
  if (record.frame >= kSettledFrame) Advance(record);
}

void AgeUntil(LabelPass& pass, std::uint8_t end_frame) {
  pass.record.frame = AddFrames(pass.record.frame, pass.half_tick);
  if (pass.record.frame >= end_frame) RemoveLabel(pass);
}

FloatingLabelScreenPoint Origin(LabelScreenPoint screen) {
  return {screen.x, screen.y};
}

bool DrawPassLeaf(FloatingLabelUpdateWorld& world, const LabelPass& pass,
                  FloatingLabelLeaf leaf, FloatingLabelScreenPoint origin) {
  return world.WriteLabel(pass.index, pass.record) &&
         world.DrawLeaf(leaf, pass.index, origin);
}

bool UpdateValueLabel(FloatingLabelUpdateWorld& world, LabelPass& pass) {
  auto& record = pass.record;
  const bool shown = record.position.w <= 0;
  if (shown) record.color_flags = StripRepeatedLimb(record.color_flags);
  pass.screen.y = Add16(pass.screen.y, kValueLabelDrop);
  if (!PlaceActorValueLabel(pass.screen, record, world.Placed())) return false;
  if (shown) {
    const auto leaf = record.color_flags == kLockedLabelMode
                          ? FloatingLabelLeaf::kLocked
                          : FloatingLabelLeaf::kActorValue;
    if (!DrawPassLeaf(world, pass, leaf, Origin(pass.screen))) return false;
    AgeShownLabel(pass);
    return true;
  }
  if ((record.color_flags & kRepeatedLimbFlag) != 0) {
    record.color_flags = StripRepeatedLimb(record.color_flags);
    Advance(record);
    return true;
  }
  AgeWaitingLabel(pass);
  return true;
}

bool UpdateStatusLabel(FloatingLabelUpdateWorld& world, LabelPass& pass,
                       bool ashley_hidden) {
  auto& record = pass.record;
  if (record.position.w > 0) {
    AgeWaitingLabel(pass);
    return true;
  }
  pass.screen.y = Add16(pass.screen.y, -record.text_length);
  if (record.actor != 0 || !ashley_hidden) {
    const auto leaf = record.color_flags == kLockedLabelMode
                          ? FloatingLabelLeaf::kLocked
                          : FloatingLabelLeaf::kActorStatus;
    if (!DrawPassLeaf(world, pass, leaf, Origin(pass.screen))) return false;
  }
  AgeShownLabel(pass);
  return true;
}

bool UpdateKind(FloatingLabelUpdateWorld& world, LabelPass& pass,
                const FloatingLabelUpdateState& state) {
  switch (pass.record.kind) {
    case static_cast<std::uint8_t>(FloatingLabelKind::kActorValue):
      return UpdateValueLabel(world, pass);
    case static_cast<std::uint8_t>(FloatingLabelKind::kActorStatus):
      return UpdateStatusLabel(world, pass, state.ashley_status_labels_hidden);
    case static_cast<std::uint8_t>(FloatingLabelKind::kWorldLocked):
      if (!DrawPassLeaf(world, pass, FloatingLabelLeaf::kLocked,
                        Origin(pass.screen))) {
        return false;
      }
      AgeUntil(pass, kLabelEndFrame);
      return true;
    case static_cast<std::uint8_t>(FloatingLabelKind::kAbilityTimingResult):
      if (!DrawPassLeaf(world, pass, FloatingLabelLeaf::kAbilityTimingResult,
                        kTimingOrigin)) {
        return false;
      }
      AgeUntil(pass, kTimingEndFrame);
      return true;
    case static_cast<std::uint8_t>(FloatingLabelKind::kChainCount):
      if (!DrawPassLeaf(world, pass, FloatingLabelLeaf::kChainCount,
                        kChainOrigin)) {
        return false;
      }
      AgeUntil(pass, kChainEndFrame);
      return true;
    case kEquipmentStatChangeKind:
      if (pass.record.position.w >= 0) return true;
      if (!DrawPassLeaf(world, pass, FloatingLabelLeaf::kEquipmentStatChange,
                        kEquipmentOrigin)) {
        return false;
      }
      if (!state.room_updates_paused) AgeUntil(pass, kEquipmentEndFrame);
      return true;
    default:
      return true;
  }
}

LabelScreenPoint ClampAnchored(LabelScreenPoint screen) {
  screen.x = std::clamp(screen.x, kAnchoredLeft, kAnchoredRight);
  screen.y = std::clamp(screen.y, kAnchoredTop, kAnchoredBottom);
  return screen;
}

// The label's screen point and visibility; nullopt when a service refuses.
std::optional<std::int32_t> AnchorLabel(FloatingLabelUpdateWorld& world,
                                        LabelPass& pass) {
  const auto& record = pass.record;
  if (record.kind < kFirstUnanchoredKind) {
    LabelPoint bone{};
    if (world.TaggedBonePosition(record.actor, kLabelAnchorTag, 0, bone) != 0) {
      return 0;
    }
    bone.y = std::min<std::int16_t>(bone.y, 0);
    bone.x = Add16(bone.x, record.position.x);
    bone.y = Add16(bone.y, record.position.y);
    bone.z = Add16(bone.z, record.position.z);
    const auto visible = world.Project(bone, pass.screen);
    pass.screen = ClampAnchored(pass.screen);
    return visible;
  }
  if (record.kind == kFirstUnanchoredKind) {
    return world.Project(record.position, pass.screen);
  }
  return 1;
}

// Every equipment label gives up one waiting step; the one that starts
// showing lifts the equipment labels queued before it.
bool StepEquipmentStack(FloatingLabelRecords& records) {
  for (std::int32_t index = 0; index < records.LabelCount(); ++index) {
    auto record = records.Label(index);
    if (!record) return false;
    if (record->kind != kEquipmentStatChangeKind) continue;
    record->position.w = Add16(record->position.w, -1);
    if (!records.WriteLabel(index, record.value())) return false;
    if (record->position.w != kFinishedWaiting) continue;
    for (std::int32_t earlier = 0; earlier < index; ++earlier) {
      auto lifted = records.Label(earlier);
      if (!lifted) return false;
      if (lifted->kind != kEquipmentStatChangeKind) continue;
      lifted->scale_y = Add16(lifted->scale_y, kStackLift);
      if (!records.WriteLabel(earlier, lifted.value())) return false;
    }
  }
  return true;
}
}  // namespace

// decomp: F00719 func_800941FC (src/BATTLE/BATTLE.PRG/2842C.c:1757).
bool UpdateAndDrawFloatingLabels(FloatingLabelUpdateWorld& world,
                                 FloatingLabelUpdateState state) {
  if (state.ability_cue_frames > 0) {
    state.ability_cue_frames = Add16(state.ability_cue_frames, -1);
    if (!world.DrawAbilityTimingCue()) return false;
  }
  const std::int32_t half_tick = state.tick_speed / 2;
  if (!state.room_updates_paused) {
    state.status_step_timer = Add16(state.status_step_timer, half_tick);
  }
  if (state.status_step_timer > kStepTimerLimit) {
    state.status_step_timer = 0;
    if (!StepEquipmentStack(world)) return false;
  }
  world.Placed().SetCount(0);
  LabelPass pass{.half_tick = half_tick};
  for (; pass.index < world.LabelCount(); ++pass.index) {
    const auto record = world.Label(pass.index);
    if (!record) return false;
    pass.record = record.value();
    if (pass.record.kind ==
        static_cast<std::uint8_t>(FloatingLabelKind::kInactive)) {
      continue;
    }
    const auto visible = AnchorLabel(world, pass);
    if (!visible) return false;
    if (visible.value() <= 0) {
      RemoveLabel(pass);
    } else if (!UpdateKind(world, pass, state)) {
      return false;
    }
    if (!world.WriteLabel(pass.index, pass.record)) return false;
  }
  return pass.removed <= 0 || world.CompactLabels(pass.removed);
}

// decomp: F00720 func_80094844 (src/BATTLE/BATTLE.PRG/2842C.c:1971).
bool PlaceActorValueLabel(LabelScreenPoint& position,
                          FloatingLabelRecord& label,
                          PlacedLabelPositions& placed) {
  const std::int32_t count = placed.Count();
  if (count < 0 || count >= kPlacedLabelCapacity) return false;
  std::array<LabelScreenPoint, kPlacedLabelCapacity> taken{};
  for (std::int32_t index = 0; index < count; ++index) {
    const auto point = placed.Position(index);
    if (!point) return false;
    taken[static_cast<std::size_t>(index)] = point.value();
  }
  if (label.scale_x == kLabelUnitScale && label.scale_y == kLabelUnitScale) {
    label.scale_x = 0;
    label.scale_y = 0;
    const auto overlapping = std::ranges::any_of(
        std::span(taken).first(static_cast<std::size_t>(count)),
        [&](LabelScreenPoint other) { return Overlaps(position, other); });
    if (overlapping) {
      if (const auto offset = FreeOffset(position, taken, count)) {
        label.scale_x = offset->x;
        label.scale_y = offset->y;
      }
    }
  }
  position.x = Add16(position.x, label.scale_x);
  position.y = Add16(position.y, label.scale_y);
  if (!placed.SetPosition(count, position)) return false;
  position.x = std::clamp(position.x, kAnchoredLeft, kAnchoredRight);
  position.y = std::min(position.y, kAnchoredBottom);
  placed.SetCount(Wrap16(count + 1));
  return true;
}

// decomp: F00722 func_80094B0C (src/BATTLE/BATTLE.PRG/2842C.c:2073).
bool DrawActorValueLabel(FloatingLabelFeedbackWorld& world,
                         FloatingLabelScreenPoint origin,
                         const FloatingLabelRecord& label) {
  if (label.text_length > kLabelTextBytes) return false;
  auto digits = ValueLayout(world, origin, label);
  if (!digits) return false;
  const auto tint = kValueColors[digits->color];
  auto x = math::WrappingAdd(origin.x, digits->width - kDigitAdvance);
  for (auto index = static_cast<std::int32_t>(label.text_length) - 1;
       index >= 0; --index, x = math::WrappingSubtract(x, kDigitAdvance)) {
    const auto digit_y = math::WrappingAdd(origin.y, digits->offset);
    digits->offset /= 2;
    if (static_cast<std::uint32_t>(math::WrappingAdd(x, kDigitWidth)) >=
        kDigitVisibleSpan) {
      continue;
    }
    const auto cell = DigitCell(label.text[static_cast<std::size_t>(index)]);
    const FloatingLabelAtlasRect atlas{
        Wrap16(cell * kDigitAtlasStride + kDigitAtlasU), 0, kDigitWidth,
        kDigitHalfHeight * 2};
    if (!world.DrawQuad(
            TintedQuad({x, math::WrappingSubtract(digit_y, kDigitHalfHeight)},
                       {math::WrappingAdd(x, kDigitWidth),
                        math::WrappingAdd(digit_y, kDigitHalfHeight)},
                       atlas, tint, kDigitPage, kValueDigitPalette))) {
      return false;
    }
  }
  return true;
}

// decomp: F00725 func_80095258 (src/BATTLE/BATTLE.PRG/2842C.c:2210).
bool DrawActorStatusLabel(FloatingLabelFeedbackWorld& world,
                          FloatingLabelScreenPoint origin,
                          const FloatingLabelRecord& label) {
  const auto layout = world.StatusLayout(label.value);
  if (!layout) return false;
  if (layout->count == 0) return true;
  const auto animation = StatusAnimation(label);
  if (!animation) return false;
  const auto parts = StatusParts(world, label, layout.value());
  if (!parts) return false;
  origin.x = math::WrappingAdd(origin.x, animation->slide);
  if ((label.color_flags & kRemovalFlag) != 0 &&
      label.frame >= kRemovalMarkerFrame) {
    const auto marker_step = std::min<std::int32_t>(
        label.frame - kRemovalMarkerFrame, kLastRemovalMarkerStep);
    if (!DrawStatusRemovalMarker(world, origin, marker_step)) return false;
  }
  origin.x = math::WrappingSubtract(origin.x, parts->width / 2);
  return DrawStatusCaptions(world, origin, parts.value(), animation->step);
}

// decomp: F00738 func_80096768 (src/BATTLE/BATTLE.PRG/2842C.c:2694).
std::optional<std::int32_t> QueueEquipmentStatChange(
    std::int32_t equipment_kind, std::int32_t stat_label,
    std::uint32_t encoded_amount, EquipmentStatChangeSettings settings,
    EquipmentStatChangeQueue& queue) {
  const bool weapon = equipment_kind == kWeaponStatChange;
  if (!(settings.weapon && weapon) && !(settings.armor && !weapon)) return 0;
  // The equipment kind's first-word caption (Battle_kStatChangeFirstWordRects).
  constexpr std::array<std::pair<std::int32_t, std::int32_t>, 7> kFirstWord{
      {{1, 4}, {2, 2}, {3, 3}, {4, 6}, {6, 5}, {241, 1}, {240, 0}}};
  auto first_word = equipment_kind;
  for (const auto& [kind, word] : kFirstWord) {
    if (kind == equipment_kind) first_word = word;
  }
  std::int32_t stacked = 0;
  const std::int32_t count = queue.LabelCount();
  for (std::int32_t index = 0; index < count; ++index) {
    const auto record = queue.Label(index);
    if (!record) return std::nullopt;
    if (record->kind == kEquipmentStatChangeKind ||
        record->kind == kPendingEquipmentStatChangeKind) {
      ++stacked;
    }
  }
  const auto slot = queue.ReserveLabel();
  if (!slot) return std::nullopt;
  auto record = queue.Label(slot.value());
  if (!record) return std::nullopt;
  record->kind = kPendingEquipmentStatChangeKind;
  record->actor = kEquipmentStatChangeActor;
  record->position.w = Wrap16(stacked);
  record->scale_y = 0;
  record->frame = 0;
  record->color_flags = static_cast<std::uint8_t>(first_word);
  record->limb = static_cast<std::uint8_t>(stat_label);
  record->value = EncodeLabelValue(encoded_amount);
  // sprintf("%d") writes at most three digits and the NUL; later text bytes
  // keep the reserved record's contents.
  std::array<char, kLabelTextBytes> text{};
  const auto printed = std::to_chars(text.data(), text.data() + text.size() - 1,
                                     record->value & kLabelAmountMask);
  if (printed.ec != std::errc{}) return std::nullopt;
  const auto length = static_cast<std::size_t>(printed.ptr - text.data());
  for (std::size_t index = 0; index <= length; ++index) {
    record->text[index] = static_cast<std::uint8_t>(text[index]);
  }
  record->text_length = static_cast<std::uint8_t>(length);
  if (!queue.WriteLabel(slot.value(), record.value()) ||
      !queue.CommitLabel(slot.value())) {
    return std::nullopt;
  }
  return 0;
}

// decomp: F00739 func_8009695C (src/BATTLE/BATTLE.PRG/2842C.c:2765).
bool DrawEquipmentStatChange(FloatingLabelFeedbackWorld& world,
                             FloatingLabelScreenPoint origin,
                             const FloatingLabelRecord& label) {
  if (label.text_length > kLabelTextBytes) return false;
  const auto first = world.StatChangeFirstWord(label.color_flags);
  const auto second = world.StatChangeSecondWord(label.limb);
  if (!first || !second) return false;
  origin.y = math::WrappingSubtract(origin.y, label.scale_y);
  const bool positive = (label.value & kNegativeValue) == 0;
  const auto tint = kStatChangeColors[positive ? 1 : 0];
  if (!DrawStatWord(world, origin, first.value(), tint) ||
      !DrawStatWord(world, origin, second.value(), tint)) {
    return false;
  }
  const auto top = math::WrappingSubtract(origin.y, kSignTop);
  const FloatingLabelAtlasRect sign{
      static_cast<std::int16_t>((positive ? kSignWidth : 0) + kSignAtlasU),
      kSignAtlasV, kSignWidth, kSignWidth};
  if (!world.DrawEquipmentQuad(
          TintedQuad({origin.x, top},
                     {math::WrappingAdd(origin.x, kSignWidth),
                      math::WrappingAdd(origin.y, kSignBottom)},
                     sign, tint, kDigitPage, kStatWordPalette))) {
    return false;
  }
  auto x = math::WrappingAdd(origin.x,
                             kWordGap + label.text_length * kStatDigitAdvance);
  for (auto index = static_cast<std::int32_t>(label.text_length) - 1;
       index >= 0; --index, x = math::WrappingSubtract(x, kStatDigitAdvance)) {
    const auto cell = DigitCell(label.text[static_cast<std::size_t>(index)]);
    const FloatingLabelAtlasRect atlas{Wrap16(cell * kStatDigitWidth), 0,
                                       kStatDigitWidth, kStatDigitHeight};
    if (!world.DrawEquipmentQuad(
            TintedQuad({x, top},
                       {math::WrappingAdd(x, kStatDigitWidth),
                        math::WrappingAdd(origin.y, kStatDigitBottom)},
                       atlas, tint, kDigitPage, kStatDigitPalette))) {
      return false;
    }
  }
  return true;
}

// decomp: F00742 func_800971D4 (src/BATTLE/BATTLE.PRG/2842C.c:2881).
bool CommitEquipmentStatChanges(FloatingLabelRecords& records) {
  for (std::int32_t index = 0; index < records.LabelCount(); ++index) {
    auto record = records.Label(index);
    if (!record) return false;
    if (record->kind != kPendingEquipmentStatChangeKind) continue;
    record->kind = kEquipmentStatChangeKind;
    if (!records.WriteLabel(index, record.value())) return false;
  }
  return true;
}
}  // namespace VS::Battle
