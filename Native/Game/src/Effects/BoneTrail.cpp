// Hand-maintained (tools/port/owned.csv).
#include "Effects/BoneTrail.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/EffectRuntime.hpp"

namespace VS::Effects {
namespace {
namespace math = Core::Math;
using Templates::RingColor;
using Templates::RingQuad;
using Templates::RingQuadUvs;
using Templates::RingUv;

constexpr unsigned kByteBits = 8;
constexpr unsigned kHalfwordBits = 16;
constexpr std::uint32_t kLowByte = 0xFF;
constexpr std::uint32_t kLowHalfword = 0xFFFF;
constexpr std::int32_t kWeightSteps = 2;

// Segments are drawn at average depths 17..2047.
constexpr std::int32_t kNearTrailDepth = 17;
constexpr std::int32_t kFarTrailDepth = 0x800;

// A textured trail draws a 31x7 texel cell of the 4-bit page at (512, 256)
// through the CLUT at (768, 240), adding or subtracting its shade.
constexpr std::uint16_t kTrailClut = 0x3C30;
constexpr std::uint16_t kAdditiveTrailPage = 0x38;
constexpr std::uint16_t kSubtractiveTrailPage = 0x58;
// Cells run three to a column, 10 texels apart, columns 40 texels apart;
// the halfword sums carry a u past 255 into v, as the original did.
constexpr std::uint32_t kTrailTextureOrigin = 0x4868;
constexpr std::uint32_t kTexturesPerColumn = 3;
constexpr std::uint32_t kTextureRowStep = 0xA00;
constexpr std::uint32_t kTextureColumnStep = 40;
constexpr std::array kTextureCornerOffsets = {0U, 0x700U, 0x1FU, 0x71FU};
// The additive shade is the colour times the weight over 8; the distortion
// shade the colour times the weight less 128.
constexpr std::int32_t kAdditiveShadeDivisor = 8;
constexpr std::int32_t kDistortionShadeBias = 128;

// The frame on display is sampled from at most x 320 (the display width),
// in 64-pixel 15-bit pages; buffer 1's pages start 5 pages right.
constexpr std::int32_t kDisplayWidth = 320;
constexpr std::uint32_t kPageMask = 0x3F;
constexpr std::int32_t kPageWidth = 0x40;
constexpr std::int32_t kSecondBufferPage = 5;
constexpr std::uint32_t kDirectColorPage = 0x100;
constexpr std::int32_t kFirstTexel = 1;
constexpr std::int32_t kLastTexel = 254;

// From the fourth sample a trail draws; from the ninth all five spans.
constexpr std::int32_t kDrawAge = 4;
constexpr std::int32_t kFullTrailAge = 9;
constexpr std::int32_t kFullTrailSpans = 5;
// Lagrange phases in 1/4096ths of a span: the newest sample, halfway to the
// one before, and the two points of each span.
constexpr std::int32_t kNewestPhase = -0x1000;
constexpr std::int32_t kHalfSpanPhase = -0x800;
constexpr std::array<std::int32_t, 2> kSpanPhases = {0, 0x800};

[[nodiscard]] constexpr bool InsideTrailDepth(std::int32_t depth) noexcept {
  return depth >= kNearTrailDepth && depth < kFarTrailDepth;
}

[[nodiscard]] constexpr std::uint8_t AdditiveShade(
    std::uint8_t channel, std::int32_t weight) noexcept {
  return static_cast<std::uint8_t>(math::WrappingMultiply(channel, weight) /
                                   kAdditiveShadeDivisor);
}

[[nodiscard]] constexpr std::uint8_t DistortionShade(
    std::uint8_t channel, std::int32_t weight) noexcept {
  return static_cast<std::uint8_t>(math::WrappingSubtract(
      math::WrappingMultiply(channel, weight), kDistortionShadeBias));
}

template <typename Shade>
[[nodiscard]] constexpr RingColor ShadeColor(RingColor color,
                                             std::int32_t weight,
                                             Shade shade) noexcept {
  return {.r = shade(color.r, weight),
          .g = shade(color.g, weight),
          .b = shade(color.b, weight)};
}

// The base corners (0 and 1) take the base weight, the tip corners the tip's.
template <typename Shade>
[[nodiscard]] constexpr std::array<RingColor, kTrailCorners> ShadeCorners(
    RingColor color, TrailSegmentWeights weights, Shade shade) noexcept {
  const RingColor base = ShadeColor(color, weights.base, shade);
  const RingColor tip = ShadeColor(color, weights.tip, shade);
  return {base, base, tip, tip};
}

[[nodiscard]] constexpr RingQuadUvs TrailTextureUvs(
    std::uint8_t texture) noexcept {
  const std::uint32_t cell =
      kTrailTextureOrigin + ((texture % kTexturesPerColumn) * kTextureRowStep) +
      ((texture / kTexturesPerColumn) * kTextureColumnStep);
  RingQuadUvs uvs{};
  for (std::size_t corner = 0; corner < kTrailCorners; ++corner) {
    const std::uint32_t uv =
        (cell + kTextureCornerOffsets.at(corner)) & kLowHalfword;
    uvs.at(corner) = {.u = static_cast<std::uint8_t>(uv & kLowByte),
                      .v = static_cast<std::uint8_t>(uv >> kByteBits)};
  }
  return uvs;
}

[[nodiscard]] constexpr std::uint8_t ClampTexel(std::int32_t texel) noexcept {
  return static_cast<std::uint8_t>(std::clamp(texel, kFirstTexel, kLastTexel));
}

[[nodiscard]] constexpr std::int32_t ScreenX(std::uint32_t screen) noexcept {
  return static_cast<std::int16_t>(screen & kLowHalfword);
}

[[nodiscard]] constexpr std::int32_t ScreenY(std::uint32_t screen) noexcept {
  return static_cast<std::int16_t>(screen >> kHalfwordBits);
}

[[nodiscard]] constexpr math::Vec3 Wide(TrailPoint point) noexcept {
  return {.x = point.x, .y = point.y, .z = point.z};
}

[[nodiscard]] constexpr TrailPoint Narrow(math::Vec3 point) noexcept {
  return {.x = static_cast<std::int16_t>(point.x),
          .y = static_cast<std::int16_t>(point.y),
          .z = static_cast<std::int16_t>(point.z)};
}

// The spline through the four samples from `newest` back, on one side of the
// bone, at `phase`.
[[nodiscard]] TrailPoint SplinePoint(const BoneTrail& trail,
                                     std::uint32_t newest, std::int32_t phase,
                                     TrailPoint TrailEdge::* side) noexcept {
  std::array<math::Vec3, math::kLagrangePoints> points{};
  for (std::size_t back = 0; back < points.size(); ++back) {
    const std::uint32_t entry =
        (newest - static_cast<std::uint32_t>(back)) % kTrailHistory;
    points.at(back) = Wide(trail.history.at(entry).*side);
  }
  return Narrow(math::InterpolateLagrangeVector(points, phase));
}

[[nodiscard]] TrailEdge SplineEdge(const BoneTrail& trail, std::uint32_t newest,
                                   std::int32_t phase) noexcept {
  return {.base = SplinePoint(trail, newest, phase, &TrailEdge::base),
          .tip = SplinePoint(trail, newest, phase, &TrailEdge::tip)};
}

// The segments of one tick: from the newest sample to halfway to the one
// before, then two per span back through the history.
void DrawTrail(BoneTrail& trail, BoneTrailHost& host) {
  const std::int32_t spans =
      trail.age < kFullTrailAge ? trail.age - (kDrawAge - 1) : kFullTrailSpans;
  host.LoadView();
  std::int32_t weight = (spans + 1) * kWeightSteps;
  const auto draw_to = [&](std::uint32_t newest, std::int32_t phase) {
    trail.current = SplineEdge(trail, newest, phase);
    host.DrawSegment(trail, {.base = weight / kWeightSteps,
                             .tip = (weight - 1) / kWeightSteps});
    --weight;
    trail.previous = trail.current;
  };
  trail.previous = SplineEdge(trail, trail.newest, kNewestPhase);
  draw_to(trail.newest, kHalfSpanPhase);
  for (std::int32_t span = 0; span < spans; ++span) {
    for (const std::int32_t phase : kSpanPhases) {
      draw_to(trail.newest - static_cast<std::uint32_t>(span), phase);
    }
  }
}

bool StartTrail(std::uint32_t parameter, BoneTrailHost& host) {
  BoneTrail* trail = host.Allocate();
  if (trail == nullptr) {
    host.Refuse(RendererCommand::kStart, BoneTrailRefusal::kNoRoom);
    return false;
  }
  trail->config = BoneTrailConfig::Unpack(parameter);
  trail->newest = 0;
  trail->age = 0;
  return true;
}

bool TickTrail(BoneTrailHost& host) {
  BoneTrail* trail = host.Trail();
  if (trail == nullptr) {
    host.Refuse(RendererCommand::kUpdate, BoneTrailRefusal::kNoTrail);
    return false;
  }
  trail->newest = (trail->newest + 1) % kTrailHistory;
  TrailEdge& edge = trail->history.at(trail->newest);
  host.SampleBone(trail->config.actor, trail->config.bone,
                  Actors::BoneEndpoint::kBase, edge.base);
  host.SampleBone(trail->config.actor, trail->config.bone,
                  Actors::BoneEndpoint::kTip, edge.tip);
  trail->age = math::WrappingAdd(trail->age, 1);
  if (trail->age >= kDrawAge) {
    DrawTrail(*trail, host);
  }
  // An endless trail keeps its life; a spent one wraps to 255 and lasts.
  if (trail->config.life != kEndlessTrailLife) {
    trail->config.life = static_cast<std::uint8_t>(trail->config.life - 1);
  }
  return trail->config.life != 0;
}

bool FadeTrail(BoneTrailHost& host) {
  BoneTrail* trail = host.Trail();
  if (trail == nullptr) {
    host.Refuse(RendererCommand::kFade, BoneTrailRefusal::kNoTrail);
    return false;
  }
  trail->config.life = kFadingTrailLife;
  return true;
}
}  // namespace

std::optional<RingColor> TrailPaletteColor(const TrailPalettes& palettes,
                                           TrailPalette palette,
                                           std::uint8_t index) noexcept {
  std::size_t entry = index;
  if (palette == TrailPalette::kAdditive) {
    if (entry < palettes.additive.size()) {
      return palettes.additive.at(entry);
    }
    entry -= palettes.additive.size();
  }
  if (entry < palettes.distortion.size()) {
    return palettes.distortion.at(entry);
  }
  return std::nullopt;
}

// decomp: BATTLE.PRG func_800CF92C (src/BATTLE/BATTLE.PRG/5BF94.c:5714).
RingUv ClampTrailTexturePoint(std::int32_t u_offset, std::int32_t v_offset,
                              std::uint32_t screen) noexcept {
  return {.u = ClampTexel(math::WrappingAdd(ScreenX(screen), u_offset)),
          .v = ClampTexel(math::WrappingAdd(ScreenY(screen), v_offset))};
}

// decomp: BATTLE.PRG func_800CF988 (src/BATTLE/BATTLE.PRG/5BF94.c:5738).
void MapTrailFramebufferTexture(RingQuad& frame_quad, std::int32_t u_offset,
                                std::int32_t v_offset, std::uint16_t flags,
                                std::uint32_t draw_buffer) {
  std::int32_t left = kDisplayWidth;
  for (const std::uint32_t screen : frame_quad.screens) {
    left = std::min(left, ScreenX(screen));
  }
  left =
      static_cast<std::int32_t>(static_cast<std::uint32_t>(left) & ~kPageMask);
  const std::int32_t page = left / kPageWidth;
  const std::int32_t displayed =
      draw_buffer != 0 ? page : page + kSecondBufferPage;
  frame_quad.tpage = static_cast<std::uint16_t>(
      (static_cast<std::uint32_t>(displayed) | flags | kDirectColorPage) &
      kLowHalfword);
  const std::int32_t u = math::WrappingSubtract(u_offset, left);
  for (std::size_t corner = 0; corner < kTrailCorners; ++corner) {
    frame_quad.uvs.at(corner) =
        ClampTrailTexturePoint(u, v_offset, frame_quad.screens.at(corner));
  }
}

// decomp: BATTLE.PRG func_800CDCBC (src/BATTLE/BATTLE.PRG/5BF94.c:4898).
TrailSegmentDraw DrawTrailSegment(const BoneTrail& trail,
                                  TrailSegmentWeights weights,
                                  TrailSegmentHost& host) {
  const TrailProjection projection =
      host.Project({trail.previous.base, trail.current.base, trail.previous.tip,
                    trail.current.tip});
  if (!InsideTrailDepth(projection.depth)) {
    return TrailSegmentDraw::kOutsideDepth;
  }
  const bool distortion = trail.config.texture == kDistortionTrailTexture;
  const std::optional<RingColor> color = TrailPaletteColor(
      host.Palettes(),
      distortion ? TrailPalette::kDistortion : TrailPalette::kAdditive,
      trail.config.color);
  if (!color) {
    return TrailSegmentDraw::kUnknownColor;
  }
  RingQuad quad{.screens = projection.screens};
  if (distortion) {
    quad.colors = ShadeCorners(*color, weights, DistortionShade);
    MapTrailFramebufferTexture(quad, weights.base, weights.base, 0,
                               host.DrawBuffer());
    host.Submit(quad, kDistortionQuadCode, projection.depth);
    return TrailSegmentDraw::kDrawn;
  }
  quad.colors = ShadeCorners(*color, weights, AdditiveShade);
  quad.uvs = TrailTextureUvs(trail.config.texture);
  quad.clut = kTrailClut;
  quad.tpage = trail.config.color == kSubtractiveTrailColor
                   ? kSubtractiveTrailPage
                   : kAdditiveTrailPage;
  host.Submit(quad, kTrailQuadCode, projection.depth);
  return TrailSegmentDraw::kDrawn;
}

// decomp: BATTLE.PRG func_800CE174 (src/BATTLE/BATTLE.PRG/5BF94.c:4958).
bool UpdateBoneTrail(RendererCommand command, std::uint32_t parameter,
                     BoneTrailHost& host) {
  switch (command) {
    case RendererCommand::kStart:
      return StartTrail(parameter, host);
    case RendererCommand::kUpdate:
      return TickTrail(host);
    case RendererCommand::kFade:
      return FadeTrail(host);
    case RendererCommand::kStop:
      host.Release();
      return false;
  }
  return true;
}

// decomp: BATTLE.PRG func_800CF7A8 (src/BATTLE/BATTLE.PRG/5BF94.c:5666).
BoneTrailRequest MakeBoneTrailRequest(std::int32_t actor, std::int32_t bone,
                                      std::int32_t color,
                                      std::int32_t texture) noexcept {
  constexpr std::int32_t kNibble = 0xF;
  const BoneTrailConfig config{
      .actor = static_cast<std::uint8_t>(actor),
      .bone = static_cast<std::uint8_t>(bone),
      .color = static_cast<std::uint8_t>(color & kNibble),
      .texture = static_cast<std::uint8_t>(texture & kNibble),
      .life = kEndlessTrailLife};
  return {.effect_id = kBoneTrailEffectId,
          .target_count = 1,
          .other_count = 0,
          .source_type = kPositionEndpoint,
          .target_type = kPositionEndpoint,
          .parameter = config.Pack()};
}

// decomp: BATTLE.PRG func_8006C480 (src/BATTLE/BATTLE.PRG/146C.c:1923).
BoneTrailRequest MakeWeaponTrailRequest(std::int32_t actor,
                                        std::int32_t bone) noexcept {
  return MakeBoneTrailRequest(actor, bone, 0, 0);
}

// decomp: BATTLE.PRG func_800CF830 (src/BATTLE/BATTLE.PRG/5BF94.c:5681).
void FadeBoneTrails(BoneTrailChains& chains) {
  for (Effects_EffectInstance* instance = chains.FirstInstance();
       instance != nullptr; instance = chains.NextInstance(*instance)) {
    for (Effects_RendererSlot* slot = chains.FirstRenderer(*instance);
         slot != nullptr; slot = chains.NextRenderer(*slot)) {
      if (chains.RunsBoneTrail(*slot)) {
        chains.Fade(*slot);
      }
    }
  }
}

// decomp: BATTLE.PRG func_8006C4A4 (src/BATTLE/BATTLE.PRG/146C.c:1925).
void FadeWeaponTrails(BoneTrailChains& chains) { FadeBoneTrails(chains); }
}  // namespace VS::Effects
