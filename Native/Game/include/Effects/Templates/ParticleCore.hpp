#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

// The state, sampling and packet rules shared by the particle-set templates
// (decomp src/EFFECT/particleDispatch.h, particleSetup.h, particleRender.h and
// particleLineRender.h). A particle set is up to eight Catmull-Rom ribbons,
// each spanning four control points: the origin stage places point 0, the
// target stage point 3 and the control stage the two inner points; the render
// stages draw each ribbon as eight textured quads or eight lines. Every
// function works on value records; the effect package, the slot state and its
// control points, the emitter scratch, the GTE, the texture animation and the
// frame's ordering table are reached through ParticleSetHost
// (ParticlesLive.hpp binds it).
namespace VS::Effects::Templates {
// Ribbons a set holds (the dispatcher clamps its count to this).
inline constexpr std::size_t kParticleSetCapacity = 8;
inline constexpr std::size_t kParticleControlPoints = 4;
// Bytes of one ribbon's control points (four SVECTOR records).
inline constexpr std::size_t kParticleControlPointBytes = 32;
inline constexpr std::size_t kParticleSegments = 8;
inline constexpr std::size_t kParticleProjections = kParticleSegments + 1;
inline constexpr std::size_t kParticleQuadWords = 15;
inline constexpr std::size_t kParticleLineWords = 6;
// Segments at or beyond this depth are not drawn.
inline constexpr std::int32_t kParticleFarDepth = 0x800;
// Definition flags: bits 0-2 anchor the origin, bits 3-5 select its shape,
// bits 6-8 anchor the target, bits 11-12 select how a box or ring places
// points, bit 17 projects through the effect transform's view and bit 18
// turns origin offsets by the transform's world rotation.
inline constexpr std::uint32_t kParticleAnchorMask = 7;
inline constexpr std::uint32_t kParticleShapeMask = 0x38;
inline constexpr std::uint32_t kParticleTargetMask = 0x1C0;
inline constexpr std::uint32_t kParticlePlacementMask = 0x1800;
inline constexpr std::uint32_t kParticleEffectView = 0x20000;
inline constexpr std::uint32_t kParticleRotateOffsets = 0x40000;
// The definition's 26-bit flags field; the untextured dispatcher masked the
// word it published with this, which keeps the same value.
inline constexpr std::uint32_t kParticleDefinitionFlagsMask = 0x3FFFFFF;
// GPU words: the textured ribbon draws a semi-transparent textured Gouraud
// quad (code 0x3E) between two E1 draw-mode words; the line ribbon a
// semi-transparent flat line (code 0x42) between them.
inline constexpr std::uint32_t kParticleQuadLength = 0x0E000000;
inline constexpr std::uint32_t kParticleLineLength = 0x05000000;
inline constexpr std::uint32_t kParticleLinkMask = 0x00FFFFFF;
inline constexpr std::uint32_t kParticleDrawModeWord = 0xE1000000;
inline constexpr std::uint32_t kParticleRestoreModeWord = 0xE1000200;
inline constexpr std::uint8_t kParticleQuadCode = 0x3E;
inline constexpr std::uint8_t kParticleLineCode = 0x42;

// Renderer slot commands (Effects_EffectRenderer).
enum class ParticleCommand : std::uint32_t {
  kCreate = 1,
  kTick = 2,
  kExpire = 3,
  kStop = 4,
};

// Whether the set's state carries a texture animation: the line ribbons
// allocate only the state prefix (the original's VS_PARTICLE_UNTEXTURED).
enum class ParticleTexturing : std::uint8_t { kTextured, kUntextured };

// The overlay global each origin instance keeps its emission cursor in (the
// original's VS_PARTICLE_CORNER_STATE binding).
enum class ParticleEmissionCounter : std::uint8_t {
  kPlg045,
  kPlg057,
  kPlg065,
  kPlg082,
  kPlg087,
  kPlg114,
  kPlg114Second,
  kPlg146,
  kPlg147,
  // D_800FD088, the address PLG082's cursor also has.
  kPlg152,
  kPlg153,
  kPlg156,
  kPlg168,
  kPlg224,
  kPlg255,
  kPlg263,
  kPlg265,
  kPlg265Second,
  kPlg266,
  kPlg266Second,
  kPlg300,
  kPlg301,
};
inline constexpr std::size_t kParticleEmissionCounters = 22;

// Values of the origin's anchor bits.
enum class ParticleAnchor : std::uint32_t {
  kNone = 0,
  kEffectOrigin = 1,
  kTransform = 2,
};

// Values of the origin's shape bits: a sphere, a box, a four-position ring
// (four values) or a ring with the definition's segment and position counts.
enum class ParticleShape : std::uint32_t {
  kSphere = 0,
  kBox = 0x08,
  kRingA = 0x10,
  kRingB = 0x18,
  kRingC = 0x20,
  kRingD = 0x28,
  kCustomRing = 0x30,
};

// Values of the placement bits: boxes place points on a random face or,
// stepping the emission cursor, on its corners (kCorners); rings place
// points between two positions or on the positions themselves (kCorners).
// kFixed leaves a box's points where they were.
enum class ParticlePlacement : std::uint32_t {
  kRandom = 0,
  kRandomAlternate = 0x800,
  kCorners = 0x1000,
  kFixed = 0x1800,
};

// Values of the target's anchor bits the target stage reads.
enum class ParticleTargetAnchor : std::uint32_t {
  kNone = 0,
  kEffectOrigin = 0x80,
  kTransform = 0xC0,
};

// The definition's depth-bias field (vectorRanges[5][0]).
enum class ParticleDepthBias : std::int16_t {
  kNone = 0,
  kBack = 1,
  kFrontA = 2,
  kFrontB = 3,
  kFrontC = 4,
  kNearer = 5,
};

// The packed create parameter: low byte lifetime, next byte definition.
struct ParticleSetPlan {
  std::uint8_t lifetime{};
  std::uint8_t definition_index{};

  friend bool operator==(const ParticleSetPlan&,
                         const ParticleSetPlan&) = default;
};

// The persistent set record (Effects_ParticleQuadSet without its control
// point buffer and texture animation, which stay with the host).
struct ParticleSetState {
  std::uint8_t lifetime{};
  std::uint8_t definition_index{};
  std::int16_t count{};
  std::int32_t cooldown{};
  std::int32_t age{};

  friend bool operator==(const ParticleSetState&,
                         const ParticleSetState&) = default;
};

// Three axes, as the GTE's matrices and vectors.
inline constexpr std::size_t kParticleAxes = Core::Math::kMatrixOrder;
using ParticleVectorRange = std::array<Core::Math::ScalarSpan, kParticleAxes>;
using ParticlePlaneRange = std::array<Core::Math::ScalarSpan, 2>;

// Curve ids the stages sample at the set's age; a zero colour curve keeps
// half intensity on that channel.
struct ParticleSetCurves {
  std::uint8_t position{};
  std::uint8_t extent{};
  std::uint8_t direction{};
  std::uint8_t direction_jitter{};
  std::uint8_t speed{};
  std::uint8_t force{};
  std::uint8_t acceleration{};
  std::uint8_t attraction_offset{};
  std::uint8_t attraction_jitter{};
  std::uint8_t mass{};
  std::uint8_t gravity_scale{};
  std::uint8_t interval{};
  std::uint8_t inner_extent{};
  std::array<std::uint8_t, kParticleAxes> color{};

  friend bool operator==(const ParticleSetCurves&,
                         const ParticleSetCurves&) = default;
};

// The fields of a particle definition the particle-set templates read. The
// original's vectorRanges rows 0, 1, 2, 4, 6 and 7 are the direction, its
// jitter, the speed jitter (forceMin), the ribbon width (accelerationMin),
// the target offset and the target jitter (attractionOffset and
// attractionJitter); pairRanges rows 0-2 the inner points' radii (massRange
// and gravityScaleRange) and the speed. The curves keep the package's names.
struct ParticleSetDefinition {
  std::uint32_t flags{};
  // The line ribbon's semi-transparency mode (transparency field bits 0-1).
  std::uint8_t blend{};
  ParticleSetCurves curves{};
  ParticleVectorRange position{};
  ParticleVectorRange extent{};
  Core::Math::ScalarSpan inner_extent{};
  Core::Math::ScalarSpan interval{};
  ParticleVectorRange direction{};
  ParticleVectorRange direction_jitter{};
  ParticleVectorRange speed_jitter{};
  ParticleVectorRange width{};
  ParticleVectorRange target_offset{};
  ParticleVectorRange target_jitter{};
  ParticlePlaneRange first_radius{};
  ParticlePlaneRange second_radius{};
  ParticlePlaneRange speed{};
  // vectorRanges[5][2]: ticks between origin and target placements.
  std::int16_t cooldown{};
  // vectorRanges[5][0], compared as a halfword.
  std::int16_t depth_bias{};
  std::uint8_t ring_segments{};
  std::uint8_t ring_positions{};

  friend bool operator==(const ParticleSetDefinition&,
                         const ParticleSetDefinition&) = default;
};

// What one tick reads besides the state: the definition, the effect origin
// (the action request position), the definition's transform world matrix,
// the translation of the target's transform and the renderer's near clip.
struct ParticleTickInputs {
  ParticleSetDefinition definition{};
  Core::Math::Vec3 effect_origin{};
  Core::Math::Transform3 world{};
  Core::Math::Vec3 target_translation{};
  std::int32_t near_clip{};
};

// A halfword vector, as the control points and the scratch's SVECTOR fields
// hold it.
struct ParticlePoint {
  std::int16_t x{};
  std::int16_t y{};
  std::int16_t z{};

  friend bool operator==(const ParticlePoint&, const ParticlePoint&) = default;
};

using ParticleControlSet = std::array<ParticlePoint, kParticleControlPoints>;

// The control points of the set's ribbons; `capacity` ribbons exist (the
// host's buffer), never more than kParticleSetCapacity.
struct ParticleSetPoints {
  std::array<ParticleControlSet, kParticleSetCapacity> sets{};
  std::size_t capacity{};

  friend bool operator==(const ParticleSetPoints&,
                         const ParticleSetPoints&) = default;
};

// The emitter-scratch values the stages write (Effects_ParticleEmitterScratch
// at scratch + 0x1D0). The host loads them before a tick and publishes them
// after it, so fields a tick does not write keep their values.
struct ParticleSetScratch {
  std::uint32_t flags{};
  Core::Math::Vec3 position{};
  Core::Math::Vec3 extent{};
  Core::Math::Vec3 aim_point{};
  Core::Math::Vec3 spawn_point{};
  Core::Math::Vec3 direction_jitter{};
  Core::Math::Vec3 base_direction{};
  // The last ribbon point the render stages interpolated (vector).
  ParticlePoint vector{};
  // forceMin, accelerationMin, attractionOffset and attractionJitter.
  ParticlePoint speed_jitter{};
  ParticlePoint width{};
  ParticlePoint target_offset{};
  ParticlePoint target_jitter{};
  // speedRange, massRange and gravityScaleRange.
  Core::Math::Vec2 speed{};
  Core::Math::Vec2 first_radius{};
  Core::Math::Vec2 second_radius{};
  // aimMatrix's rotation.
  Core::Math::Mat3 aim{};
  // sinAngle0/1 and cosAngle0/1.
  std::array<std::int32_t, 2> sine{};
  std::array<std::int32_t, 2> cosine{};
  std::int32_t ring_segments{};
  std::int32_t ring_positions{};

  friend bool operator==(const ParticleSetScratch&,
                         const ParticleSetScratch&) = default;
};

// Everything one tick works on. `texturing` is the dispatcher's: only a
// textured set has a texture animation to step.
struct ParticleSetFrame {
  ParticleTickInputs inputs{};
  ParticleSetState state{};
  ParticleSetScratch scratch{};
  ParticleSetPoints points{};
  ParticleTexturing texturing = ParticleTexturing::kTextured;
};

// A projected point: the packed screen word (SXY2) and its OTZ depth.
struct ParticleProjection {
  std::uint32_t screen{};
  std::int32_t depth{};

  friend bool operator==(const ParticleProjection&,
                         const ParticleProjection&) = default;
};

// The package texture the animation currently shows.
struct ParticleTexture {
  std::uint16_t clut{};
  std::uint16_t tpage{};
  std::uint8_t u{};
  std::uint8_t v{};
  std::uint8_t w{};
  std::uint8_t h{};

  friend bool operator==(const ParticleTexture&,
                         const ParticleTexture&) = default;
};

struct ParticleColor {
  std::uint8_t r{};
  std::uint8_t g{};
  std::uint8_t b{};

  friend bool operator==(const ParticleColor&, const ParticleColor&) = default;
};

struct ParticleUv {
  std::uint8_t u{};
  std::uint8_t v{};

  friend bool operator==(const ParticleUv&, const ParticleUv&) = default;
};

inline constexpr std::size_t kParticleQuadCorners = 4;

// One textured ribbon quad in GPU corner order (0 and 2 trail, 1 and 3
// lead); every corner takes the same colour.
struct ParticleQuad {
  std::array<std::uint32_t, kParticleQuadCorners> screens{};
  std::array<ParticleUv, kParticleQuadCorners> uvs{};
  ParticleColor color{};
  std::uint16_t clut{};
  std::uint16_t tpage{};

  friend bool operator==(const ParticleQuad&, const ParticleQuad&) = default;
};

// One line ribbon segment from `screens[0]` to `screens[1]`.
struct ParticleLine {
  std::array<std::uint32_t, 2> screens{};
  ParticleColor color{};
  std::uint8_t blend{};

  friend bool operator==(const ParticleLine&, const ParticleLine&) = default;
};

// Services the particle-set templates use. A host refuses by returning
// nothing; ParticlesLive.hpp logs every refusal.
class ParticleSetHost {
 public:
  ParticleSetHost() = default;
  ParticleSetHost(const ParticleSetHost&) = delete;
  ParticleSetHost(ParticleSetHost&&) = delete;
  ParticleSetHost& operator=(const ParticleSetHost&) = delete;
  ParticleSetHost& operator=(ParticleSetHost&&) = delete;
  virtual ~ParticleSetHost() = default;

  // The current package's definition `index`, or nothing when it lacks it.
  [[nodiscard]] virtual std::optional<ParticleSetDefinition> Definition(
      std::uint8_t index) = 0;
  // Allocates the slot state and room for `capacity` ribbons (none when it
  // is not positive) and, for a textured set, starts its texture animation.
  virtual void Create(const ParticleSetState& state, std::int32_t capacity,
                      ParticleTexturing texturing) = 0;
  [[nodiscard]] virtual std::optional<ParticleSetState> State() const = 0;
  virtual void StoreState(const ParticleSetState& state) = 0;
  // Frees the control points (the slot state stays with the slot's owner).
  virtual void Release() = 0;
  // The definition and anchors of one tick, or nothing to stop the set.
  [[nodiscard]] virtual std::optional<ParticleTickInputs> BeginTick(
      std::uint8_t definition_index) = 0;
  [[nodiscard]] virtual ParticleSetPoints LoadPoints() const = 0;
  virtual void StorePoints(const ParticleSetPoints& points) = 0;
  [[nodiscard]] virtual ParticleSetScratch LoadScratch() const = 0;
  virtual void PublishScratch(const ParticleSetScratch& scratch) = 0;
  [[nodiscard]] virtual std::int32_t SampleCurve(std::uint8_t curve,
                                                 std::int32_t distance) = 0;
  [[nodiscard]] virtual std::int32_t Rand() = 0;
  // A draw in [low, high) for either bound order; equal bounds return the
  // bound without drawing (vs_battle_randUniformInt).
  [[nodiscard]] virtual std::int32_t RandomRange(std::int32_t first,
                                                 std::int32_t second) = 0;
  [[nodiscard]] virtual std::int32_t EmissionCounter(
      ParticleEmissionCounter counter) const = 0;
  virtual void StoreEmissionCounter(ParticleEmissionCounter counter,
                                    std::int32_t value) = 0;
  // Loads the effect transform's view (true) or the frame's view matrix.
  virtual void BeginProjection(bool effect_view) = 0;
  [[nodiscard]] virtual ParticleProjection Project(ParticlePoint point) = 0;
  // Steps a textured set's texture animation; nothing when no texture can be
  // drawn. The render stage calls it only for a textured set.
  [[nodiscard]] virtual std::optional<ParticleTexture> AdvanceTexture() = 0;
  virtual void SubmitQuad(const ParticleQuad& quad, std::int32_t depth) = 0;
  // Submits the line, then moves its second point (`screens[1]`) one pixel
  // toward its first, as Effects_StepParticleEndpointTowardStart did after
  // the original linked each line.
  virtual void SubmitLine(const ParticleLine& line, std::int32_t depth) = 0;
};

// One stage of a tick; the instances bind each template to its overlay.
using ParticleStage = void (*)(ParticleSetFrame& frame, ParticleSetHost& host);

// What a dispatch instance binds: its four stages and whether its state
// carries a texture animation.
struct ParticleDispatchOptions {
  ParticleStage origin = nullptr;
  ParticleStage target = nullptr;
  ParticleStage control = nullptr;
  ParticleStage render = nullptr;
  ParticleTexturing texturing = ParticleTexturing::kTextured;
};

// The remainder the R3000 DIV instruction leaves in HI: a zero divisor
// leaves the dividend and -1 leaves 0 (no host trap for INT32_MIN % -1).
[[nodiscard]] constexpr std::int32_t ParticleRemainder(
    std::int32_t dividend, std::int32_t divisor) noexcept {
  if (divisor == 0) {
    return dividend;
  }
  if (divisor == -1) {
    return 0;
  }
  return dividend % divisor;
}

// (left * right) >> 12 over the R3000's wrapping product: a Q12 scale.
[[nodiscard]] constexpr std::int32_t ParticleFixed(
    std::int32_t left, std::int32_t right) noexcept {
  constexpr unsigned kFractionBits = 12;
  return Core::Math::WrappingMultiply(left, right) >> kFractionBits;
}

// value / 2 truncated toward zero, as the original's signed halvings.
[[nodiscard]] constexpr std::int32_t ParticleHalf(std::int32_t value) noexcept {
  return Core::Math::R3000Quotient(value, 2);
}

[[nodiscard]] constexpr ParticleSetPlan PlanParticleSet(
    std::int32_t packed) noexcept {
  constexpr unsigned kByteBits = 8;
  const auto bits = static_cast<std::uint32_t>(packed);
  return {.lifetime = static_cast<std::uint8_t>(bits),
          .definition_index = static_cast<std::uint8_t>(bits >> kByteBits)};
}

// The ribbons a set allocates: the greater interval endpoint, at most
// kParticleSetCapacity (a negative endpoint stays negative).
[[nodiscard]] std::int32_t ParticleSetCapacity(
    Core::Math::ScalarSpan interval) noexcept;

// The halfword count a tick draws from an interval sample, at most
// kParticleSetCapacity.
[[nodiscard]] std::int16_t ParticleSetCount(std::int32_t sample) noexcept;

// The ribbons a stage visits: the set's count, but none when it is negative
// and no more than the host's buffer holds.
[[nodiscard]] std::size_t ParticleRibbons(
    const ParticleSetFrame& frame) noexcept;

// `value` stored into a halfword vector.
[[nodiscard]] constexpr ParticlePoint ParticlePointAt(
    Core::Math::Vec3 value) noexcept {
  return {Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.x)),
          Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.y)),
          Core::Math::WrapSigned16(static_cast<std::uint32_t>(value.z))};
}

[[nodiscard]] constexpr Core::Math::Vec3 ParticleVector(
    ParticlePoint point) noexcept {
  return {point.x, point.y, point.z};
}

// `range` sampled at `curve` and the set's age.
[[nodiscard]] Core::Math::Vec3 SampleParticleRange(
    const ParticleVectorRange& range, std::uint8_t curve, std::int32_t age,
    ParticleSetHost& host);
[[nodiscard]] Core::Math::Vec2 SampleParticlePlane(
    const ParticlePlaneRange& range, std::uint8_t curve, std::int32_t age,
    ParticleSetHost& host);
[[nodiscard]] std::int32_t SampleParticleScalar(Core::Math::ScalarSpan range,
                                                std::uint8_t curve,
                                                std::int32_t age,
                                                ParticleSetHost& host);

// A colour channel from its curve sample: twice the sample, at most 255,
// stored as a byte (a negative product wraps).
[[nodiscard]] std::uint8_t ParticleColorChannel(std::int32_t sample) noexcept;

// The ribbon colour at the set's age: each channel from its curve, or 128
// when the channel has no curve.
[[nodiscard]] ParticleColor SampleParticleColor(
    const ParticleSetDefinition& definition, std::int32_t age,
    ParticleSetHost& host);

// The inner-extent sample both render stages start from; zero becomes one.
[[nodiscard]] std::int32_t SampleRibbonJitter(const ParticleSetFrame& frame,
                                              ParticleSetHost& host);

// The ordering depth of a segment between two projected depths: their
// average, when it lies between the near clip and kParticleFarDepth, moved
// by the definition's depth bias; nothing when the segment is not drawn.
[[nodiscard]] std::optional<std::int32_t> ParticleSegmentDepth(
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): call shape.
    std::int32_t first, std::int32_t second, std::int16_t depth_bias,
    std::int32_t near_clip) noexcept;

// The ribbon point at `phase` (Q12; 0 at point 1, 0x1000 at point 2) of the
// Catmull-Rom curve through `points`, stored as a halfword vector.
[[nodiscard]] ParticlePoint ParticleRibbonPoint(
    const ParticleControlSet& points, std::int32_t phase) noexcept;

// Projects the ribbon's nine points: point 0, the seven interpolated points
// at `phases` (one per inner segment boundary) and point 3. The last
// interpolated point is left in the scratch vector.
void ProjectParticleRibbon(
    const ParticleControlSet& points,
    std::span<const std::int32_t, kParticleSegments - 1> phases,
    ParticleSetScratch& scratch,
    std::span<ParticleProjection, kParticleProjections> projections,
    ParticleSetHost& host);

// Writes the fifteen GPU words of `quad`, linked to the ordering entry
// `link`, into `packet`. The original stored u2v2 and u3v3 as halfwords, so
// the high halfwords of words 10 and 13 keep what the buffer held.
void EncodeParticleQuadPacket(
    const ParticleQuad& quad, std::uint32_t link,
    std::span<std::uint32_t, kParticleQuadWords> packet) noexcept;

// The six GPU words of `line`, linked to the ordering entry `link`.
[[nodiscard]] std::array<std::uint32_t, kParticleLineWords>
EncodeParticleLinePacket(const ParticleLine& line, std::uint32_t link) noexcept;
}  // namespace VS::Effects::Templates
