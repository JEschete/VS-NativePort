// Hand-maintained (tools/port/owned.csv).
#include "Render/ScreenOverlay.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace VS::Render {
namespace {
namespace Math = VS::Core::Math;

// The strips are placed about the field frame's centre and span lines -112 to
// 111 from it.
constexpr std::int32_t kEffectCenterX = 160;
constexpr std::int32_t kEffectCenterY = 112;
constexpr std::int16_t kEffectTop = -112;
constexpr std::int16_t kEffectBottom = 111;
// The second frame buffer starts this many VRAM columns in.
constexpr std::int32_t kFrameBufferColumns = 320;
constexpr std::uint8_t kStripRightTexel = 64;
constexpr std::uint8_t kLastStripRightTexel = 63;
constexpr std::int32_t kAngleMask = 0xFFF;
constexpr std::int32_t kHalfTurn = 0x800;

// getTPage(mode, blend, x, 0): mode 2 is 15-bit direct colour.
constexpr std::uint32_t kDirectColorMode = 2;
constexpr std::uint32_t kTextureModeShift = 7;
constexpr std::uint32_t kBlendMask = 3;
constexpr std::uint32_t kBlendShift = 5;
constexpr std::uint32_t kVramColumnMask = 0x3FF;
constexpr std::uint32_t kTexturePageColumnShift = 6;
constexpr std::uint32_t kAdditiveBlend = 1;
constexpr std::uint32_t kSubtractiveBlend = 2;
constexpr std::uint16_t kAdditivePage{kAdditiveBlend << kBlendShift};
constexpr std::uint16_t kSubtractivePage{kSubtractiveBlend << kBlendShift};

// Field_screenTransitionEffect: 3 and codes past the table draw nothing.
constexpr std::array kTransitionStyles{
    ScreenTransitionStyle::kFadeToBlack, ScreenTransitionStyle::kFadeToWhite,
    ScreenTransitionStyle::kWipe,        ScreenTransitionStyle::kNone,
    ScreenTransitionStyle::kFadeToBlack, ScreenTransitionStyle::kFadeToBlack,
};

// Field_screenTransitionStep values the draw steps the alpha for.
constexpr std::uint32_t kCoverStep = 1;
constexpr std::uint32_t kCoverAgainStep = 2;
constexpr std::uint32_t kCoverThenHoldStep = 5;
constexpr std::uint32_t kHeldStep = 6;
constexpr std::uint32_t kUncoverStep = 4;
constexpr std::uint32_t kUncoverAgainStep = 7;

// The wipe sweeps 560 pixels across a band 720 wide and 400 tall about the
// screen centre; the gradient trails 160 pixels behind its front.
constexpr std::int32_t kWipeCenterX = 160;
constexpr std::int32_t kWipeCenterY = 120;
constexpr std::int32_t kWipeTravel = 560;
constexpr std::int16_t kWipeReach = 360;
constexpr std::int16_t kWipeHalfHeight = 200;
constexpr std::int32_t kWipeGradientInset = 200;

[[nodiscard]] std::int16_t Halfword(std::int32_t value) noexcept {
  return Math::WrapSigned16(static_cast<std::uint32_t>(value));
}

[[nodiscard]] ScreenVertex Place(ScreenTransformer& transformer, std::int16_t x,
                                 std::int16_t y) {
  const Math::Vec3 point = transformer.Transform(x, y);
  return {Halfword(point.x), Halfword(point.y)};
}

[[nodiscard]] bool ShowsPreviousFrame(
    const FramebufferEffectSettings& settings) noexcept {
  return (settings.roll & kAngleMask) == 0 && settings.uses_previous_frame != 0;
}

[[nodiscard]] std::array<ScreenVertex, kQuadCornerCount> StripCorners(
    std::int32_t left, ScreenTransformer& transformer) {
  const std::int16_t near_x = Halfword(left - kEffectCenterX);
  const std::int16_t far_x = Halfword(near_x + kFramebufferEffectStripWidth);
  return {Place(transformer, near_x, kEffectTop),
          Place(transformer, far_x, kEffectTop),
          Place(transformer, near_x, kEffectBottom),
          Place(transformer, far_x, kEffectBottom)};
}

[[nodiscard]] bool IsUncovering(std::uint32_t step) noexcept {
  return step == kUncoverStep || step == kUncoverAgainStep;
}
}  // namespace

// decomp: BATTLE.PRG func_8007D360 (src/BATTLE/BATTLE.PRG/146C.c:9258).
// The original also cleared the unread scale z and three unread tint bytes.
void ResetFramebufferEffect(FramebufferEffect& effect) noexcept {
  effect.phase = FramebufferEffectPhase::kOff;
  effect.settings = FramebufferEffectSettings{};
}

// decomp: BATTLE.PRG func_8007D3F8 (src/BATTLE/BATTLE.PRG/146C.c:9278).
void InitializeFramebufferEffect(FramebufferEffect& effect,
                                 FramebufferEffectPackets& packets) {
  packets.Forget();
  ResetFramebufferEffect(effect);
}

// decomp: BATTLE.PRG func_8007D41C (src/BATTLE/BATTLE.PRG/146C.c:9285).
PerFramebufferEffectSlot<FramebufferEffectSlotTemplate>
BuildFramebufferEffectTemplates() noexcept {
  PerFramebufferEffectSlot<FramebufferEffectSlotTemplate> templates{};
  for (std::size_t slot = 0; slot < templates.size(); ++slot) {
    const auto left =
        static_cast<std::int32_t>(slot) * kFramebufferEffectStripWidth;
    const bool last_strip =
        slot % kFramebufferEffectStripCount == kFramebufferEffectStripCount - 1;
    templates[slot] = {Halfword(left),
                       last_strip ? kLastStripRightTexel : kStripRightTexel,
                       FramebufferEffectTexturePage(0, left)};
  }
  return templates;
}

std::uint16_t FramebufferEffectTexturePage(std::int32_t blend_mode,
                                           std::int32_t x) noexcept {
  const std::uint32_t blend =
      static_cast<std::uint32_t>(blend_mode) & kBlendMask;
  const std::uint32_t column = static_cast<std::uint32_t>(x) & kVramColumnMask;
  return static_cast<std::uint16_t>((kDirectColorMode << kTextureModeShift) |
                                    (blend << kBlendShift) |
                                    (column >> kTexturePageColumnShift));
}

bool FramebufferEffectIsTransformed(
    const FramebufferEffectSettings& settings) noexcept {
  return (settings.roll & kAngleMask) != 0 ||
         settings.scale.x != kFramebufferEffectUnitScale ||
         settings.scale.y != settings.scale.x;
}

// 146C.c:9324-9330: RotMatrix, TransMatrix, ScaleMatrix. The original passed
// an unset stack word as the translation's z; only the x and y of each corner
// are used, so it is 0 here.
Math::Transform3 FramebufferEffectTransform(
    const FramebufferEffectSettings& settings) noexcept {
  const Math::Mat3 rotation = Math::BuildRotationXyz({0, 0, settings.roll});
  return {Math::ScaleMatrixColumns(rotation,
                                   {settings.scale.x, settings.scale.y, 0}),
          {Math::WrappingAdd(settings.offset.x, kEffectCenterX),
           Math::WrappingAdd(settings.offset.y, kEffectCenterY), 0}};
}

FramebufferEffectFrame PlanFramebufferEffectFrame(
    const FramebufferEffectSettings& settings, std::size_t frame_buffer,
    ScreenTransformer& transformer) {
  const std::size_t buffer = frame_buffer & 1U;
  const std::size_t source =
      ShowsPreviousFrame(settings) ? (buffer ^ 1U) : buffer;
  FramebufferEffectFrame frame;
  frame.layout = FramebufferEffectIsTransformed(settings)
                     ? FramebufferEffectLayout::kTransformed
                     : FramebufferEffectLayout::kFlat;
  frame.tint = settings.tint;
  frame.depth = settings.depth;
  for (std::size_t strip = 0; strip < kFramebufferEffectStripCount; ++strip) {
    const auto left =
        static_cast<std::int32_t>(strip) * kFramebufferEffectStripWidth;
    FramebufferEffectStrip& out = frame.strips[strip];
    out.slot = (buffer * kFramebufferEffectStripCount) + strip;
    out.texture_page = FramebufferEffectTexturePage(
        settings.blend_mode,
        left + (static_cast<std::int32_t>(source) * kFrameBufferColumns));
    if (frame.layout == FramebufferEffectLayout::kTransformed) {
      out.corners = StripCorners(left, transformer);
    } else {
      out.origin = {Halfword(Math::WrappingAdd(settings.offset.x, left)),
                    Halfword(settings.offset.y)};
    }
  }
  return frame;
}

// 146C.c:9399-9414: 1 becomes 2; 3 and 4 count up and 5 is 0 with the block
// freed.
FramebufferEffectAdvance AdvanceFramebufferEffectPhase(
    FramebufferEffectPhase phase) noexcept {
  switch (phase) {
    case FramebufferEffectPhase::kStarting:
      return {FramebufferEffectPhase::kDrawing, false};
    case FramebufferEffectPhase::kStopping:
      return {FramebufferEffectPhase::kReleasing, false};
    case FramebufferEffectPhase::kReleasing:
      return {FramebufferEffectPhase::kOff, true};
    case FramebufferEffectPhase::kOff:
    case FramebufferEffectPhase::kDrawing:
      break;
  }
  return {phase, false};
}

// decomp: BATTLE.PRG func_8007D734 (src/BATTLE/BATTLE.PRG/146C.c:9311).
// The original wrote through a null block when enabling found no memory.
FramebufferEffectDraw DrawFramebufferEffect(FramebufferEffect& effect,
                                            FramebufferEffectPackets& packets,
                                            ScreenTransformer& transformer,
                                            std::size_t frame_buffer) {
  auto drawn = FramebufferEffectDraw::kHidden;
  if (effect.phase == FramebufferEffectPhase::kDrawing) {
    transformer.LoadTransform(FramebufferEffectTransform(effect.settings));
    if (packets.Ready()) {
      packets.Submit(PlanFramebufferEffectFrame(effect.settings, frame_buffer,
                                                transformer));
      drawn = FramebufferEffectDraw::kDrawn;
    } else {
      drawn = FramebufferEffectDraw::kNoPackets;
    }
  }
  const FramebufferEffectAdvance advance =
      AdvanceFramebufferEffectPhase(effect.phase);
  effect.phase = advance.phase;
  if (advance.release_packets) {
    packets.Release();
  }
  return drawn;
}

// decomp: BATTLE.PRG func_8007DD50 (src/BATTLE/BATTLE.PRG/146C.c:9417). The
// original started the effect before allocating and then drew through the
// null block; without packets the phase is left as it was.
bool SetFramebufferEffectEnabled(FramebufferEffect& effect,
                                 FramebufferEffectPackets& packets,
                                 bool enabled) {
  if (!enabled) {
    effect.phase = FramebufferEffectPhase::kStopping;
    return true;
  }
  if (!packets.Ready() && !packets.Acquire(effect.settings.tint)) {
    return false;
  }
  effect.phase = FramebufferEffectPhase::kStarting;
  return true;
}

// decomp: BATTLE.PRG func_8007DDAC (src/BATTLE/BATTLE.PRG/146C.c:9430).
void SetFramebufferEffectRoll(FramebufferEffectSettings& settings,
                              std::int32_t angle) noexcept {
  settings.roll = Halfword(angle);
}

// decomp: BATTLE.PRG func_8007DDB8 (src/BATTLE/BATTLE.PRG/146C.c:9432).
void SetFramebufferEffectScale(FramebufferEffectSettings& settings,
                               FramebufferEffectPair scale) noexcept {
  settings.scale = scale;
}

// decomp: BATTLE.PRG func_8007DDD4 (src/BATTLE/BATTLE.PRG/146C.c:9438).
void SetFramebufferEffectTint(FramebufferEffectSettings& settings,
                              FramebufferEffectTint tint) noexcept {
  settings.tint = tint;
}

// decomp: BATTLE.PRG func_8007DDF8 (src/BATTLE/BATTLE.PRG/146C.c:9440).
// The original also copied two words the effect never reads.
void SetFramebufferEffectOffset(FramebufferEffectSettings& settings,
                                FramebufferEffectPair offset) noexcept {
  settings.offset = offset;
}

// decomp: BATTLE.PRG func_8007DE2C (src/BATTLE/BATTLE.PRG/146C.c:9442).
void SetFramebufferEffectBlendMode(FramebufferEffectSettings& settings,
                                   std::int32_t blend_mode) noexcept {
  settings.blend_mode = blend_mode;
}

// decomp: BATTLE.PRG func_8007DE38 (src/BATTLE/BATTLE.PRG/146C.c:9444).
void SetFramebufferEffectParameter(FramebufferEffectSettings& settings,
                                   std::int32_t parameter) noexcept {
  settings.parameter = parameter;
}

// decomp: BATTLE.PRG func_8007DE44 (src/BATTLE/BATTLE.PRG/146C.c:9446).
bool SetFramebufferEffectDepth(FramebufferEffectSettings& settings,
                               std::uint32_t depth) noexcept {
  if (depth >= kFramebufferEffectDepthLimit) {
    return false;
  }
  settings.depth = depth;
  return true;
}

// decomp: BATTLE.PRG func_8007DE5C (src/BATTLE/BATTLE.PRG/146C.c:9453).
void SetFramebufferEffectPreviousFrame(
    FramebufferEffectSettings& settings,
    std::int32_t uses_previous_frame) noexcept {
  settings.uses_previous_frame = uses_previous_frame;
}

// decomp: BATTLE.PRG func_8007DE68 (src/BATTLE/BATTLE.PRG/146C.c:9455).
std::int32_t FramebufferEffectStateCode(
    const FramebufferEffect& effect) noexcept {
  return static_cast<std::int32_t>(effect.phase);
}

// decomp: BATTLE.PRG func_8007DE78 (src/BATTLE/BATTLE.PRG/146C.c:9457).
std::int16_t ReadFramebufferEffectRoll(
    const FramebufferEffectSettings& settings) noexcept {
  return settings.roll;
}

// decomp: BATTLE.PRG func_8007DE88 (src/BATTLE/BATTLE.PRG/146C.c:9459).
FramebufferEffectPair ReadFramebufferEffectScale(
    const FramebufferEffectSettings& settings) noexcept {
  return {settings.scale.x, settings.scale.x};
}

// decomp: BATTLE.PRG func_8007DEA8 (src/BATTLE/BATTLE.PRG/146C.c:9466).
FramebufferEffectTint ReadFramebufferEffectTint(
    const FramebufferEffectSettings& settings) noexcept {
  return settings.tint;
}

// decomp: BATTLE.PRG func_8007DECC (src/BATTLE/BATTLE.PRG/146C.c:9468).
FramebufferEffectPair ReadFramebufferEffectOffset(
    const FramebufferEffectSettings& settings) noexcept {
  return settings.offset;
}

// decomp: BATTLE.PRG func_8007DF00 (src/BATTLE/BATTLE.PRG/146C.c:9470).
std::int32_t ReadFramebufferEffectBlendMode(
    const FramebufferEffectSettings& settings) noexcept {
  return settings.blend_mode;
}

// decomp: BATTLE.PRG func_8007DF10 (src/BATTLE/BATTLE.PRG/146C.c:9472).
std::int32_t ReadFramebufferEffectParameter(
    const FramebufferEffectSettings& settings) noexcept {
  return settings.parameter;
}

// decomp: BATTLE.PRG func_8007DF20 (src/BATTLE/BATTLE.PRG/146C.c:9474).
std::uint32_t ReadFramebufferEffectDepth(
    const FramebufferEffectSettings& settings) noexcept {
  return settings.depth;
}

// decomp: BATTLE.PRG func_8007DF30 (src/BATTLE/BATTLE.PRG/146C.c:9476).
std::int32_t ReadFramebufferEffectPreviousFrame(
    const FramebufferEffectSettings& settings) noexcept {
  return settings.uses_previous_frame;
}

// --- Room transition --------------------------------------------------------

ScreenTransitionStyle ScreenTransitionStyleOf(std::int32_t effect) noexcept {
  const auto index = static_cast<std::uint32_t>(effect);
  return index < kTransitionStyles.size() ? kTransitionStyles[index]
                                          : ScreenTransitionStyle::kNone;
}

// 146C.c:2237-2270: white is opaque once fully faded in.
ScreenTransitionFade PlanScreenTransitionFade(ScreenTransitionStyle style,
                                              std::int32_t alpha) noexcept {
  const auto level = static_cast<std::uint8_t>(alpha);
  if (style == ScreenTransitionStyle::kFadeToWhite) {
    return {level, alpha != kScreenTransitionOpaque, kAdditivePage};
  }
  return {level, true, kSubtractivePage};
}

std::array<std::int16_t, kScreenTransitionWipeColumns - 1>
ScreenTransitionWipeEdges(std::int32_t alpha) noexcept {
  const std::int32_t travel =
      Math::WrappingMultiply(alpha, kWipeTravel) / kScreenTransitionOpaque;
  return {Halfword(Math::WrappingSubtract(travel, 2 * kWipeReach) / 2),
          Halfword(Math::WrappingSubtract(travel, kWipeReach)),
          Halfword(Math::WrappingSubtract(travel, kWipeGradientInset))};
}

// 146C.c:2273-2284. The angle is read as a halfword, so only its low 16 bits
// count.
Math::Transform3 ScreenTransitionWipeTransform(
    const ScreenTransition& transition) noexcept {
  const std::int32_t turn = IsUncovering(transition.step) ? kHalfTurn : 0;
  const std::int16_t angle =
      Halfword(Math::WrappingAdd(transition.wipe_angle, turn));
  return {Math::BuildRotationXyz({0, 0, angle}),
          {kWipeCenterX, kWipeCenterY, 0}};
}

ScreenTransitionWipe PlanScreenTransitionWipe(
    const ScreenTransition& transition, ScreenTransformer& transformer) {
  const auto edges = ScreenTransitionWipeEdges(transition.alpha);
  const std::array<std::int16_t, kScreenTransitionWipeColumns> columns{
      static_cast<std::int16_t>(-kWipeReach), edges[0], edges[1], edges[2]};
  ScreenTransitionWipe wipe;
  wipe.texture_page = kSubtractivePage;
  for (std::size_t column = 0; column < columns.size(); ++column) {
    wipe.columns[column].top =
        Place(transformer, columns[column],
              static_cast<std::int16_t>(-kWipeHalfHeight));
    wipe.columns[column].bottom =
        Place(transformer, columns[column], kWipeHalfHeight);
  }
  return wipe;
}

// 146C.c:2368-2386. Covering adds the speed until it would reach 255 and
// holds there; uncovering subtracts it until it would reach 0 and ends.
void AdvanceScreenTransition(ScreenTransition& transition) noexcept {
  const std::uint32_t step = transition.step;
  if (step == kCoverStep || step == kCoverAgainStep ||
      step == kCoverThenHoldStep) {
    if (transition.alpha >=
        Math::WrappingSubtract(kScreenTransitionOpaque, transition.speed)) {
      if (step == kCoverThenHoldStep) {
        transition.step = kHeldStep;
      }
      transition.alpha = kScreenTransitionOpaque;
    } else {
      transition.alpha = Math::WrappingAdd(transition.alpha, transition.speed);
    }
  } else if (IsUncovering(step)) {
    if (transition.alpha <= transition.speed) {
      transition.alpha = 0;
      transition.step = 0;
    } else {
      transition.alpha =
          Math::WrappingSubtract(transition.alpha, transition.speed);
    }
  }
}

// decomp: BATTLE.PRG _drawScreenTransition (src/BATTLE/BATTLE.PRG/146C.c:2223).
void DrawScreenTransition(ScreenTransition& transition,
                          ScreenTransitionTarget& target,
                          ScreenTransformer& transformer) {
  if (transition.alpha != 0) {
    const ScreenTransitionStyle style =
        ScreenTransitionStyleOf(transition.effect);
    switch (style) {
      case ScreenTransitionStyle::kFadeToBlack:
      case ScreenTransitionStyle::kFadeToWhite:
        target.DrawFade(PlanScreenTransitionFade(style, transition.alpha));
        break;
      case ScreenTransitionStyle::kWipe:
        transformer.LoadTransform(ScreenTransitionWipeTransform(transition));
        target.DrawWipe(PlanScreenTransitionWipe(transition, transformer));
        break;
      case ScreenTransitionStyle::kNone:
        break;
    }
  }
  AdvanceScreenTransition(transition);
}
}  // namespace VS::Render
