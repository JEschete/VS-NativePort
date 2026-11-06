// Hand-maintained (tools/port/owned.csv).
#include "Effects/Templates/ParticleOrigin.hpp"

#include <cstddef>
#include <cstdint>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"
#include "Effects/Templates/ParticleCore.hpp"

namespace VS::Effects::Templates {
namespace {
namespace math = Core::Math;
using math::Vec3;

constexpr std::int32_t kBoxFaces = 6;
constexpr std::uint32_t kCornerMask = 7;
constexpr std::int32_t kCornerX = 1;
constexpr std::int32_t kCornerY = 2;
constexpr std::int32_t kCornerZ = 4;
constexpr std::int32_t kFourPositionRing = 4;
constexpr std::int32_t kFullTurn = 0x1000;
// Ring positions start an eighth of a turn round.
constexpr std::int32_t kRingAngleOffset = 0x200;
// A ring point between two positions takes a Q12 weight from a 15-bit draw.
constexpr unsigned kRingWeightShift = 3;

// The value Core_Random_Rand() % 6 picks: the box face a point lands on.
enum class BoxFace : std::int32_t {
  kPositiveX = 0,
  kNegativeX = 1,
  kPositiveY = 2,
  kNegativeY = 3,
  kPositiveZ = 4,
  kNegativeZ = 5,
};

// A ring position in the extent's x-z plane.
struct RingPosition {
  std::int32_t x{};
  std::int32_t z{};
};

[[nodiscard]] std::int16_t Halfword(std::int32_t value) noexcept {
  return math::WrapSigned16(static_cast<std::uint32_t>(value));
}

// `extent` or its negation.
[[nodiscard]] std::int16_t Side(std::int32_t extent, bool positive) noexcept {
  return Halfword(positive ? extent : math::WrappingNegate(extent));
}

// A draw across the box: [-extent, extent) either way round.
[[nodiscard]] std::int16_t Across(std::int32_t extent, ParticleSetHost& host) {
  return Halfword(host.RandomRange(extent, math::WrappingNegate(extent)));
}

// Turns the offset by the transform's world rotation when the definition
// asks, then stores it, moved to the sampled position, as the ribbon's first
// point. decomp: particleSetup.h:96-105 (ApplyRotMatrix with the translation
// cleared, then the position added as halfwords).
void PlaceOrigin(ParticleSetFrame& frame, std::size_t ribbon,
                 ParticlePoint offset) {
  ParticleSetScratch& scratch = frame.scratch;
  if ((scratch.flags & kParticleRotateOffsets) != 0) {
    scratch.aim_point = math::RotateTranslate(
        {.rotation = frame.inputs.world.rotation, .translation = {}},
        ParticleVector(offset));
    offset = ParticlePointAt(scratch.aim_point);
  }
  frame.points.sets.at(ribbon)[0] = ParticlePointAt(
      math::AddVectors(ParticleVector(offset), scratch.position));
}

// decomp: particleSetup.h:77-107, a point on the extent's ellipsoid at two
// random angles.
void PlaceOnSphere(ParticleSetFrame& frame, ParticleSetHost& host) {
  ParticleSetScratch& scratch = frame.scratch;
  const std::size_t ribbons = ParticleRibbons(frame);
  for (std::size_t ribbon = 0; ribbon < ribbons; ++ribbon) {
    std::int32_t angle = host.Rand();
    scratch.sine[0] = math::Sine(angle);
    scratch.cosine[0] = math::Cosine(angle);
    angle = host.Rand();
    scratch.sine[1] = math::Sine(angle);
    scratch.cosine[1] = math::Cosine(angle);
    const Vec3& extent = scratch.extent;
    const ParticlePoint offset{
        Halfword(ParticleFixed(ParticleFixed(extent.x, scratch.cosine[0]),
                               scratch.cosine[1])),
        Halfword(ParticleFixed(ParticleFixed(extent.y, scratch.cosine[0]),
                               scratch.sine[1])),
        Halfword(ParticleFixed(extent.z, scratch.sine[0]))};
    PlaceOrigin(frame, ribbon, offset);
  }
}

// decomp: particleSetup.h:117-156, the face `face` of the box, drawing the
// two other axes across it. Another value leaves `point` as it was.
[[nodiscard]] ParticlePoint OnBoxFace(std::int32_t face, ParticlePoint point,
                                      const Vec3& extent,
                                      ParticleSetHost& host) {
  const auto side = static_cast<BoxFace>(face);
  switch (side) {
    case BoxFace::kPositiveX:
    case BoxFace::kNegativeX:
      point.x = Side(extent.x, side == BoxFace::kPositiveX);
      point.y = Across(extent.y, host);
      point.z = Across(extent.z, host);
      break;
    case BoxFace::kPositiveY:
    case BoxFace::kNegativeY:
      point.y = Side(extent.y, side == BoxFace::kPositiveY);
      point.x = Across(extent.x, host);
      point.z = Across(extent.z, host);
      break;
    case BoxFace::kPositiveZ:
    case BoxFace::kNegativeZ:
      point.z = Side(extent.z, side == BoxFace::kPositiveZ);
      point.x = Across(extent.x, host);
      point.y = Across(extent.y, host);
      break;
    default:
      break;
  }
  return point;
}

// decomp: particleSetup.h:159-181, the corner whose bits 0-2 select the
// positive x, y and z sides.
[[nodiscard]] ParticlePoint OnBoxCorner(std::int32_t corner,
                                        const Vec3& extent) noexcept {
  return {Side(extent.x, (corner & kCornerX) != 0),
          Side(extent.y, (corner & kCornerY) != 0),
          Side(extent.z, (corner & kCornerZ) != 0)};
}

// decomp: particleSetup.h:109-195. A point keeps its previous offset for the
// fixed placement; the corners step the overlay's emission cursor, which is
// stored back after the loop as the original's register copy was.
void PlaceOnBox(ParticleEmissionCounter counter, ParticleSetFrame& frame,
                ParticleSetHost& host) {
  const auto placement = static_cast<ParticlePlacement>(frame.scratch.flags &
                                                        kParticlePlacementMask);
  std::int32_t corner = host.EmissionCounter(counter);
  const std::size_t ribbons = ParticleRibbons(frame);
  for (std::size_t ribbon = 0; ribbon < ribbons; ++ribbon) {
    ParticlePoint offset = frame.points.sets.at(ribbon)[0];
    switch (placement) {
      case ParticlePlacement::kRandom:
      case ParticlePlacement::kRandomAlternate:
        offset = OnBoxFace(ParticleRemainder(host.Rand(), kBoxFaces), offset,
                           frame.scratch.extent, host);
        break;
      case ParticlePlacement::kCorners:
        corner = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(math::WrappingAdd(corner, 1)) &
            kCornerMask);
        offset = OnBoxCorner(corner, frame.scratch.extent);
        break;
      case ParticlePlacement::kFixed:
        break;
    }
    PlaceOrigin(frame, ribbon, offset);
  }
  host.StoreEmissionCounter(counter, corner);
}

// decomp: particleSetup.h:197-220, the ring's segment and position counts:
// four each, or the definition's (a zero segment count is one and a zero
// position count takes the segment count). False for the unused shape.
[[nodiscard]] bool CountRing(ParticleShape shape,
                             const ParticleSetDefinition& definition,
                             ParticleSetScratch& scratch) noexcept {
  switch (shape) {
    case ParticleShape::kRingA:
    case ParticleShape::kRingB:
    case ParticleShape::kRingC:
    case ParticleShape::kRingD:
      scratch.ring_segments = kFourPositionRing;
      scratch.ring_positions = kFourPositionRing;
      return true;
    case ParticleShape::kCustomRing:
      scratch.ring_segments =
          definition.ring_segments != 0 ? definition.ring_segments : 1;
      scratch.ring_positions = definition.ring_positions != 0
                                   ? definition.ring_positions
                                   : scratch.ring_segments;
      return true;
    case ParticleShape::kSphere:
    case ParticleShape::kBox:
    default:
      return false;
  }
}

// The ring position `index` steps of `step` round, scaled by the extent.
[[nodiscard]] RingPosition RingPositionAt(std::int32_t index, std::int32_t step,
                                          const Vec3& extent) noexcept {
  const std::int32_t angle =
      math::WrappingAdd(math::WrappingMultiply(index, step), kRingAngleOffset);
  return {.x = ParticleFixed(math::Cosine(angle), extent.x),
          .z = ParticleFixed(math::Sine(angle), extent.z)};
}

// A Q12 `weight` of the way from `start` to `end`.
[[nodiscard]] std::int16_t Between(std::int32_t start, std::int32_t end,
                                   std::int32_t weight) noexcept {
  return Halfword(math::WrappingAdd(
      ParticleFixed(math::WrappingSubtract(end, start), weight), start));
}

// decomp: particleSetup.h:223-259. Each point advances the emission cursor
// one ring position and lands on it (corners) or at a random weight between
// the cursor's first position and it.
void PlaceOnRing(ParticleEmissionCounter counter, ParticleSetFrame& frame,
                 ParticleSetHost& host) {
  ParticleSetScratch& scratch = frame.scratch;
  const bool corners =
      static_cast<ParticlePlacement>(scratch.flags & kParticlePlacementMask) ==
      ParticlePlacement::kCorners;
  const std::int32_t step =
      math::R3000Quotient(kFullTurn, scratch.ring_segments);
  std::int32_t position = host.EmissionCounter(counter);
  RingPosition start{};
  if (!corners) {
    position = ParticleRemainder(position, scratch.ring_positions);
    start = RingPositionAt(position, step, scratch.extent);
  }
  const std::size_t ribbons = ParticleRibbons(frame);
  for (std::size_t ribbon = 0; ribbon < ribbons; ++ribbon) {
    position = ParticleRemainder(math::WrappingAdd(position, 1),
                                 scratch.ring_positions);
    const RingPosition end = RingPositionAt(position, step, scratch.extent);
    ParticlePoint offset{.x = Halfword(end.x), .y = 0, .z = Halfword(end.z)};
    if (!corners) {
      const std::int32_t weight = host.Rand() >> kRingWeightShift;
      offset.x = Between(start.x, end.x, weight);
      offset.z = Between(start.z, end.z, weight);
    }
    PlaceOrigin(frame, ribbon, offset);
  }
  host.StoreEmissionCounter(counter, position);
}
}  // namespace

// decomp: EFFECT VS_PARTICLE_ORIGIN_FUNCTION (src/EFFECT/particleSetup.h:36).
void ParticleOrigin(ParticleEmissionCounter counter, ParticleSetFrame& frame,
                    ParticleSetHost& host) {
  const ParticleSetDefinition& definition = frame.inputs.definition;
  ParticleSetScratch& scratch = frame.scratch;
  const std::int32_t age = frame.state.age;
  scratch.position = SampleParticleRange(definition.position,
                                         definition.curves.position, age, host);
  switch (static_cast<ParticleAnchor>(scratch.flags & kParticleAnchorMask)) {
    case ParticleAnchor::kEffectOrigin:
      scratch.position =
          math::AddVectors(scratch.position, frame.inputs.effect_origin);
      break;
    case ParticleAnchor::kTransform:
      scratch.position =
          math::AddVectors(scratch.position, frame.inputs.world.translation);
      break;
    case ParticleAnchor::kNone:
    default:
      break;
  }
  scratch.extent = SampleParticleRange(definition.extent,
                                       definition.curves.extent, age, host);
  const auto shape =
      static_cast<ParticleShape>(scratch.flags & kParticleShapeMask);
  if (shape == ParticleShape::kSphere) {
    PlaceOnSphere(frame, host);
  } else if (shape == ParticleShape::kBox) {
    PlaceOnBox(counter, frame, host);
  } else if (CountRing(shape, definition, scratch)) {
    PlaceOnRing(counter, frame, host);
  }
}
}  // namespace VS::Effects::Templates

namespace VS::Effects {
// decomp: PLG045.BIN func_800F9910 (src/EFFECT/PLG045.BIN/110.c:1).
void Plg045::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg045, frame,
                            host);
}

// decomp: PLG057.BIN func_800F9AD0 (src/EFFECT/PLG057.BIN/2D0.c:2).
void Plg057::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg057, frame,
                            host);
}

// decomp: PLG065.BIN func_800FC340 (src/EFFECT/PLG065.BIN/F8.c:19).
// decomp: PLG065.BIN func_800FC340 (src/EFFECT/PLG065.BIN/F8.c:35).
void Plg065::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg065, frame,
                            host);
}

// decomp: PLG082.BIN func_800F9928 (src/EFFECT/PLG082.BIN/128.c:8).
void Plg082::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg082, frame,
                            host);
}

// decomp: PLG087.BIN func_800FE168 (src/EFFECT/PLG087.BIN/1F0.c:31).
// decomp: PLG087.BIN func_800FE168 (src/EFFECT/PLG087.BIN/1F0.c:47).
void Plg087::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg087, frame,
                            host);
}

// decomp: PLG114.BIN func_800F9A08 (src/EFFECT/PLG114.BIN/208.c:2).
void Plg114::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg114, frame,
                            host);
}

// decomp: PLG114.BIN func_800FB6AC (src/EFFECT/PLG114.BIN/208.c:14).
// decomp: PLG114.BIN func_800FB6AC (src/EFFECT/PLG114.BIN/208.c:30).
void Plg114::ParticleOrigin2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg114Second,
                            frame, host);
}

// decomp: PLG146.BIN func_800FB544 (src/EFFECT/PLG146.BIN/1D8.c:10).
// decomp: PLG146.BIN func_800FB544 (src/EFFECT/PLG146.BIN/1D8.c:26).
void Plg146::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg146, frame,
                            host);
}

// decomp: PLG147.BIN func_800FA51C (src/EFFECT/PLG147.BIN/F8.c:6).
// decomp: PLG147.BIN func_800FA51C (src/EFFECT/PLG147.BIN/F8.c:22).
void Plg147::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg147, frame,
                            host);
}

// decomp: PLG152.BIN func_800FA478 (src/EFFECT/PLG152.BIN/128.c:11).
void Plg152::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg152, frame,
                            host);
}

// decomp: PLG153.BIN func_800FB040 (src/EFFECT/PLG153.BIN/110.c:12).
void Plg153::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg153, frame,
                            host);
}

// decomp: PLG156.BIN func_800F9928 (src/EFFECT/PLG156.BIN/128.c:2).
void Plg156::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg156, frame,
                            host);
}

// decomp: PLG168.BIN func_800FBB10 (src/EFFECT/PLG168.BIN/1F0.c:12).
// decomp: PLG168.BIN func_800FBB10 (src/EFFECT/PLG168.BIN/1F0.c:28).
void Plg168::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg168, frame,
                            host);
}

// decomp: PLG224.BIN func_800F98F8 (src/EFFECT/PLG224.BIN/F8.c:2).
// decomp: PLG224.BIN func_800F98F8 (src/EFFECT/PLG224.BIN/F8.c:18).
void Plg224::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg224, frame,
                            host);
}

// decomp: PLG255.BIN func_800FBAF8 (src/EFFECT/PLG255.BIN/1D8.c:12).
// decomp: PLG255.BIN func_800FBAF8 (src/EFFECT/PLG255.BIN/1D8.c:28).
void Plg255::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg255, frame,
                            host);
}

// decomp: PLG263.BIN func_800FC00C (src/EFFECT/PLG263.BIN/110.c:344).
void Plg263::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg263, frame,
                            host);
}

// decomp: PLG265.BIN func_800FA9E0 (src/EFFECT/PLG265.BIN/2E8.c:6).
// decomp: PLG265.BIN func_800FA9E0 (src/EFFECT/PLG265.BIN/2E8.c:22).
void Plg265::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg265, frame,
                            host);
}

// decomp: PLG265.BIN func_800FE0F4 (src/EFFECT/PLG265.BIN/2E8.c:40).
void Plg265::ParticleOrigin2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg265Second,
                            frame, host);
}

// decomp: PLG266.BIN func_800F9AE8 (src/EFFECT/PLG266.BIN/2E8.c:2).
void Plg266::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg266, frame,
                            host);
}

// decomp: PLG266.BIN func_800FB78C (src/EFFECT/PLG266.BIN/2E8.c:14).
// decomp: PLG266.BIN func_800FB78C (src/EFFECT/PLG266.BIN/2E8.c:30).
void Plg266::ParticleOrigin2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg266Second,
                            frame, host);
}

// decomp: PLG300.BIN func_800FA584 (src/EFFECT/PLG300.BIN/110.c:12).
void Plg300::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg300, frame,
                            host);
}

// decomp: PLG301.BIN func_800F9910 (src/EFFECT/PLG301.BIN/110.c:8).
void Plg301::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg301, frame,
                            host);
}
}  // namespace VS::Effects
