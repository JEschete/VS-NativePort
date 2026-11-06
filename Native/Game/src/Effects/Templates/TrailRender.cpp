// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/TrailRender.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/TrailCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;

// A colour curve's sample doubles into the channel, saturating; a channel
// without a curve stays at half intensity.
constexpr std::uint8_t kNeutralChannel = 0x80;
constexpr std::int32_t kSaturatedChannel = 0xFF;
// A channel times a transparency sample, 0x80 leaving it unchanged.
constexpr unsigned kTransparencyShift = 7;
// A segment's normal is a quarter turn from its screen direction. A joint
// whose two normals lie more than 0x7FF apart takes the opposite side of
// their mean.
constexpr std::int32_t kQuarterTurn = 0x400;
constexpr std::int32_t kHalfTurn = 0x800;
constexpr std::uint32_t kJointBias = 0x7FF;
constexpr std::uint32_t kJointSpan = 0xFFF;
// u moves one full collapse's share of the texture's width per collapse step
// (a quarter for five samples, an eighth for nine).
constexpr unsigned kHalfwordBits = 16;

// What every quad of one render shares.
template <std::size_t History>
struct TrailPaint {
  RingColor color{};
  std::array<std::uint8_t, History> transparency{};
  RingTexture texture{};
  std::int32_t half_width{};
  std::int32_t near_clip{};
};

// The ribbon's cross-section at one sample: its screen points either side of
// the sample, their u and their faded colour.
struct TrailEdge {
  std::uint32_t plus{};
  std::uint32_t minus{};
  std::uint8_t u{};
  RingColor color{};
};

[[nodiscard]] std::uint8_t ChannelIntensity(std::uint8_t curve,
                                            std::int32_t distance,
                                            TrailServices& host) {
  if (curve == 0) {
    return kNeutralChannel;
  }
  const std::int32_t doubled =
      math::WrappingMultiply(host.SampleCurve(curve, distance), 2);
  return static_cast<std::uint8_t>(
      doubled <= kSaturatedChannel ? doubled : kSaturatedChannel);
}

[[nodiscard]] std::uint8_t Faded(std::uint8_t channel,
                                 std::uint8_t level) noexcept {
  return static_cast<std::uint8_t>(
      (static_cast<std::uint32_t>(channel) * level) >> kTransparencyShift);
}

[[nodiscard]] std::uint32_t ScreenWord(std::int32_t x,
                                       std::int32_t y) noexcept {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y))
          << kHalfwordBits);
}

[[nodiscard]] std::int32_t ScreenX(std::uint32_t screen) noexcept {
  return math::WrapSigned16(screen);
}

[[nodiscard]] std::int32_t ScreenY(std::uint32_t screen) noexcept {
  return math::WrapSigned16(screen >> kHalfwordBits);
}

// The normal of the segment from one projected sample to the next.
[[nodiscard]] std::int32_t SegmentAngle(std::uint32_t from,
                                        std::uint32_t to) noexcept {
  return math::WrappingSubtract(
      math::Arctangent2(math::WrappingSubtract(ScreenY(to), ScreenY(from)),
                        math::WrappingSubtract(ScreenX(to), ScreenX(from))),
      kQuarterTurn);
}

// The mean of two neighbouring segments' normals.
[[nodiscard]] std::int32_t JointAngle(std::int32_t before,
                                      std::int32_t after) noexcept {
  const std::int32_t mean =
      math::R3000Quotient(math::WrappingAdd(before, after), 2);
  const std::uint32_t apart =
      static_cast<std::uint32_t>(math::WrappingSubtract(after, before)) +
      kJointBias;
  return apart >= kJointSpan ? math::WrappingAdd(mean, kHalfTurn) : mean;
}

template <std::size_t History>
[[nodiscard]] TrailEdge EdgeAt(const TrailPaint<History>& paint,
                               std::uint32_t screen, std::int32_t angle,
                               std::size_t step) {
  const std::int32_t dx = RingFixed(math::Cosine(angle), paint.half_width);
  const std::int32_t dy = RingFixed(math::Sine(angle), paint.half_width);
  const std::int32_t x = ScreenX(screen);
  const std::int32_t y = ScreenY(screen);
  const std::uint8_t level = paint.transparency.at(step);
  const std::int32_t shift = math::R3000Quotient(
      math::WrappingMultiply(paint.texture.w, static_cast<std::int32_t>(step)),
      kTrailFullCollapseOf<History>);
  return {
      .plus = ScreenWord(math::WrappingAdd(x, dx), math::WrappingAdd(y, dy)),
      .minus = ScreenWord(math::WrappingSubtract(x, dx),
                          math::WrappingSubtract(y, dy)),
      .u = static_cast<std::uint8_t>(paint.texture.u + shift),
      .color = {.r = Faded(paint.color.r, level),
                .g = Faded(paint.color.g, level),
                .b = Faded(paint.color.b, level)}};
}

// The quad from one edge to the next, drawn at the mean depth of their
// samples when that lies in front of the far depth and beyond the near clip.
template <std::size_t History>
void SubmitSegment(const TrailEdge& from, const TrailEdge& to,
                   std::span<const RingProjection, 2> samples,
                   const TrailPaint<History>& paint, TrailServices& host) {
  const std::int32_t depth = math::R3000Quotient(
      math::WrappingAdd(samples.front().depth, samples.back().depth), 2);
  if (depth >= kRingFarDepth || depth <= paint.near_clip) {
    return;
  }
  const RingTexture& texture = paint.texture;
  const auto bottom = static_cast<std::uint8_t>(texture.v + texture.h);
  RingQuad quad;
  quad.screens = {from.plus, to.plus, from.minus, to.minus};
  quad.colors = {from.color, to.color, from.color, to.color};
  quad.uvs = {RingUv{.u = from.u, .v = texture.v},
              RingUv{.u = to.u, .v = texture.v},
              RingUv{.u = from.u, .v = bottom}, RingUv{.u = to.u, .v = bottom}};
  quad.clut = texture.clut;
  quad.tpage = texture.tpage;
  host.Submit(quad, depth);
}

// Why the original would have left its history-sized arrays drawing this
// trail: more samples than entries, a newest sample outside them, a collapse
// beyond the transparency samples, or a collapse too short for the inner
// edges.
template <std::size_t History>
[[nodiscard]] std::optional<TrailFault> FaultOf(
    const BasicTrailRecord<History>& trail) noexcept {
  if (trail.sample_count > History) {
    return TrailFault::kHistoryOverflow;
  }
  if (trail.current >= History) {
    return TrailFault::kCursorOutside;
  }
  if (trail.collapse > kTrailFullCollapseOf<History>) {
    return TrailFault::kCollapseOutside;
  }
  if (trail.sample_count > 2 && trail.collapse + 2 < trail.sample_count) {
    return TrailFault::kFadeUnderflow;
  }
  return std::nullopt;
}

template <std::size_t History>
using TrailProjections = std::array<RingProjection, History>;

// The trail's samples projected newest first.
template <std::size_t History>
[[nodiscard]] TrailProjections<History> ProjectHistory(
    const BasicTrailRecord<History>& trail, TrailServices& host) {
  TrailProjections<History> projected{};
  std::size_t at = trail.current;
  for (std::size_t j = 0; j < trail.sample_count; ++j) {
    projected.at(j) = host.Project(trail.samples.at(at));
    at = (at + History - 1) % History;
  }
  return projected;
}

template <std::size_t History>
void DrawTrail(const BasicTrailRecord<History>& trail, std::size_t index,
               const TrailPaint<History>& paint, TrailServices& host) {
  if (trail.sample_count < 2) {
    return;
  }
  if (const std::optional<TrailFault> fault = FaultOf(trail)) {
    host.ReportSkippedTrail(index, *fault);
    return;
  }
  const std::size_t count = trail.sample_count;
  const TrailProjections<History> projected = ProjectHistory(trail, host);
  const std::span<const RingProjection> samples(projected);
  std::array<std::int32_t, History - 1> normals{};
  for (std::size_t j = 0; j + 1 < count; ++j) {
    normals.at(j) =
        SegmentAngle(projected.at(j).screen, projected.at(j + 1).screen);
  }
  TrailEdge edge =
      EdgeAt(paint, projected.front().screen, normals.front(), trail.collapse);
  std::size_t j = 0;
  for (; j + 2 < count; ++j) {
    const TrailEdge next = EdgeAt(paint, projected.at(j + 1).screen,
                                  JointAngle(normals.at(j), normals.at(j + 1)),
                                  trail.collapse - j - 1);
    SubmitSegment(edge, next, samples.subspan(j).first<2>(), paint, host);
    edge = next;
  }
  const TrailEdge last =
      EdgeAt(paint, projected.at(j + 1).screen, normals.at(j), 0);
  SubmitSegment(edge, last, samples.subspan(j).first<2>(), paint, host);
}
template <std::size_t History>
void PaintTrails(const TrailDefinition& definition, TrailScratch& scratch,
                 std::span<BasicTrailRecord<History>> trails,
                 std::int32_t distance, BasicTrailHost<History>& host) {
  scratch.acceleration_min = TrailHalfwords(SampleTrailVector(
      definition.acceleration, definition.curves.acceleration, distance, host));
  const std::array<std::uint8_t, kTrailAxes>& curves = definition.curves.color;
  const RingColor color{.r = ChannelIntensity(curves[0], distance, host),
                        .g = ChannelIntensity(curves[1], distance, host),
                        .b = ChannelIntensity(curves[2], distance, host)};
  if (!host.BeginProjection((scratch.flags & kTrailEffectView) != 0)) {
    return;
  }
  const std::optional<RingTexture> texture = host.AdvanceTexture();
  if (!texture) {
    return;
  }
  const TrailPaint<History> paint{
      .color = color,
      .transparency = host.Transparency(),
      .texture = *texture,
      .half_width = math::R3000Quotient(scratch.acceleration_min.x, 2),
      .near_clip = host.NearClip()};
  for (std::size_t index = 0; index < trails.size(); ++index) {
    DrawTrail(trails[index], index, paint, host);
  }
}
}  // namespace

// decomp: EFFECT VS_TRAIL_RENDER_FUNCTION (src/EFFECT/trails5.h:449).
// The original wrote its quads without bounding the ordering-table index or
// the primitive buffer; TrailHost::Submit owns both. Its first quad of a
// two-sample trail repeated stale code bytes in corners 1 and 3 and left the
// halfwords after the last two texture coordinates unwritten; every quad now
// carries the 0x3E code in each corner and zeros there (EncodeRingPacket).
void TrailRender(const TrailDefinition& definition, TrailScratch& scratch,
                 std::span<TrailRecord> trails, std::int32_t distance,
                 TrailHost& host) {
  PaintTrails<kTrailHistory>(definition, scratch, trails, distance, host);
}

// decomp: EFFECT VS_TRAIL_RENDER_FUNCTION (src/EFFECT/trails9.h:456).
// decomp: PLG049.BIN func_800FAA70 (src/EFFECT/PLG049.BIN/E0.c:6).
// decomp: PLG050.BIN func_800FB694 (src/EFFECT/PLG050.BIN/E0.c:12).
// decomp: PLG057.BIN func_800FC904 (src/EFFECT/PLG057.BIN/2D0.c:20).
// decomp: PLG069.BIN func_800FB57C (src/EFFECT/PLG069.BIN/E0.c:13).
// decomp: PLG135.BIN func_800FB9DC (src/EFFECT/PLG135.BIN/E0.c:18).
// decomp: PLG161.BIN func_800FC564 (src/EFFECT/PLG161.BIN/F8.c:16).
// decomp: PLG165.BIN func_800FAA88 (src/EFFECT/PLG165.BIN/F8.c:14).
// decomp: PLG167.BIN func_800FC894 (src/EFFECT/PLG167.BIN/E0.c:21).
// decomp: PLG168.BIN func_800FAB80 (src/EFFECT/PLG168.BIN/1F0.c:8).
// decomp: PLG169.BIN func_800FAA70 (src/EFFECT/PLG169.BIN/E0.c:14).
// decomp: PLG232.BIN func_800FAA70 (src/EFFECT/PLG232.BIN/E0.c:8).
// The plugin instances are byte-identical; each overlay's renderer-slot entry
// (TrailsLive.cpp) binds this one definition. The same packet and bounds notes
// as TrailRender apply.
void TrailNineRender(const TrailDefinition& definition, TrailScratch& scratch,
                     std::span<TrailNineRecord> trails, std::int32_t distance,
                     TrailNineHost& host) {
  PaintTrails<kTrailNineHistory>(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG057.BIN func_800FEA10 (src/EFFECT/PLG057.BIN/2D0.c:30).
void Plg057::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG073.BIN func_800FC590 (src/EFFECT/PLG073.BIN/F8.c:16).
// decomp: PLG074.BIN func_800FC590 (src/EFFECT/PLG074.BIN/F8.c:17).
void Plg073::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG086.BIN func_800FD3EC (src/EFFECT/PLG086.BIN/F8.c:25).
void Plg086::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG087.BIN func_800FD1C4 (src/EFFECT/PLG087.BIN/1F0.c:27).
void Plg087::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG128.BIN func_800FF54C (src/EFFECT/PLG128.BIN/F8.c:258).
void Plg128::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG134.BIN func_800FAA5C (src/EFFECT/PLG134.BIN/E0.c:14).
void Plg134::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG137.BIN func_800FB580 (src/EFFECT/PLG137.BIN/F8.c:13).
void Plg137::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG141.BIN func_800FAA5C (src/EFFECT/PLG141.BIN/E0.c:8).
void Plg141::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG146.BIN func_800FE8D8 (src/EFFECT/PLG146.BIN/1D8.c:45).
void Plg146::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG148.BIN func_800FAA5C (src/EFFECT/PLG148.BIN/E0.c:8).
void Plg148::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG242.BIN func_800FC4BC (src/EFFECT/PLG242.BIN/F8.c:15).
void Plg242::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG255.BIN func_800FAB54 (src/EFFECT/PLG255.BIN/1D8.c:8).
void Plg255::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG265.BIN func_800FD150 (src/EFFECT/PLG265.BIN/2E8.c:36).
void Plg265::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG266.BIN func_800FDEFC (src/EFFECT/PLG266.BIN/2E8.c:44).
void Plg266::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}

// decomp: PLG291.BIN func_800FAA74 (src/EFFECT/PLG291.BIN/F8.c:14).
void Plg291::TrailRender(const Templates::TrailDefinition& definition,
                         Templates::TrailScratch& scratch,
                         std::span<Templates::TrailRecord> trails,
                         std::int32_t distance, Templates::TrailHost& host) {
  Templates::TrailRender(definition, scratch, trails, distance, host);
}
}  // namespace VS::Effects
