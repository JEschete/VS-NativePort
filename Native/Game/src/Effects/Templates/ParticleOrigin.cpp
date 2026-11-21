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

// The value VS::Core::NextBiosRand() % 6 picks: the box face a point lands on.
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
// point. decomp: src/EFFECT/particleSetup.h:96-105 (ApplyRotMatrix with the
// translation cleared, then the position added as halfwords).
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

// decomp: src/EFFECT/particleSetup.h:77-107, a point on the extent's ellipsoid
// at two random angles.
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

// decomp: src/EFFECT/particleSetup.h:117-156, the face `face` of the box,
// drawing the two other axes across it. Another value leaves `point` as it was.
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

// decomp: src/EFFECT/particleSetup.h:159-181, the corner whose bits 0-2 select
// the positive x, y and z sides.
[[nodiscard]] ParticlePoint OnBoxCorner(std::int32_t corner,
                                        const Vec3& extent) noexcept {
  return {Side(extent.x, (corner & kCornerX) != 0),
          Side(extent.y, (corner & kCornerY) != 0),
          Side(extent.z, (corner & kCornerZ) != 0)};
}

// decomp: src/EFFECT/particleSetup.h:109-195. A point keeps its previous offset
// for the fixed placement; the corners step the overlay's emission cursor,
// which is stored back after the loop as the original's register copy was.
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

// decomp: src/EFFECT/particleSetup.h:37-219, the ring's segment and position
// counts: four each, or the definition's (a zero segment count is one and a
// zero position count takes the segment count). False for the unused shape.
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

// decomp: src/EFFECT/particleSetup.h:37-219. Each point advances the emission
// cursor one ring position and lands on it (corners) or at a random weight
// between the cursor's first position and it.
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

// decomp: EFFECT VS_PARTICLE_ORIGIN_FUNCTION (src/EFFECT/particleSetup.h:37).
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
// decomp: PLG045.BIN vs_effect_lightningBolt_0_origin
// (src/EFFECT/particleSetup.h:37; was func_800F9910).
void Plg045::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg045, frame,
                            host);
}

// decomp: PLG057.BIN vs_effect_thunderBurstLv1_0_origin
// (src/EFFECT/particleSetup.h:37; was func_800F9AD0).
void Plg057::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg057, frame,
                            host);
}

// decomp: PLG065.BIN vs_effect_gaeaStrikeLv1_3_origin
// (src/EFFECT/particleSetup.h:37; was func_800FC340). decomp: PLG065.BIN
// vs_effect_gaeaStrikeLv1_3_origin (src/EFFECT/particleSetup.h:37; was
// func_800FC340).
void Plg065::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg065, frame,
                            host);
}

// decomp: PLG082.BIN vs_effect_thunderbolt_0_origin
// (src/EFFECT/particleSetup.h:37; was func_800F9928).
void Plg082::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg082, frame,
                            host);
}

// decomp: PLG087.BIN vs_effect_apocalypse_4_origin
// (src/EFFECT/particleSetup.h:37; was func_800FE168). decomp: PLG087.BIN
// vs_effect_apocalypse_4_origin (src/EFFECT/particleSetup.h:37; was
// func_800FE168).
void Plg087::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg087, frame,
                            host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_0_origin
// (src/EFFECT/particleSetup.h:37; was func_800F9A08).
void Plg114::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg114, frame,
                            host);
}

// decomp: PLG114.BIN vs_effect_numbingNeedle_1_origin
// (src/EFFECT/particleSetup.h:37; was func_800FB6AC). decomp: PLG114.BIN
// vs_effect_numbingNeedle_1_origin (src/EFFECT/particleSetup.h:37; was
// func_800FB6AC).
void Plg114::ParticleOrigin2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg114Second,
                            frame, host);
}

// decomp: PLG146.BIN vs_effect_blackNebula_2_origin
// (src/EFFECT/particleSetup.h:37; was func_800FB544). decomp: PLG146.BIN
// vs_effect_blackNebula_2_origin (src/EFFECT/particleSetup.h:37; was
// func_800FB544).
void Plg146::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg146, frame,
                            host);
}

// decomp: PLG147.BIN vs_effect_bearClaw_1_origin
// (src/EFFECT/particleSetup.h:37; was func_800FA51C). decomp: PLG147.BIN
// vs_effect_bearClaw_1_origin (src/EFFECT/particleSetup.h:37; was
// func_800FA51C).
void Plg147::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg147, frame,
                            host);
}

// decomp: PLG152.BIN vs_effect_riskbreak_1_origin
// (src/EFFECT/particleSetup.h:37; was func_800FA478).
void Plg152::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg152, frame,
                            host);
}

// decomp: PLG153.BIN vs_effect_gravisAether_2_origin
// (src/EFFECT/particleSetup.h:37; was func_800FB040).
void Plg153::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg153, frame,
                            host);
}

// decomp: PLG156.BIN vs_effect_quickshock_0_origin
// (src/EFFECT/particleSetup.h:37; was func_800F9928).
void Plg156::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg156, frame,
                            host);
}

// decomp: PLG168.BIN vs_effect_vertigo_1_origin (src/EFFECT/particleSetup.h:37;
// was func_800FBB10). decomp: PLG168.BIN vs_effect_vertigo_1_origin
// (src/EFFECT/particleSetup.h:37; was func_800FBB10).
void Plg168::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg168, frame,
                            host);
}

// decomp: PLG224.BIN vs_effect_plg224_0_origin (src/EFFECT/particleSetup.h:37;
// was func_800F98F8). decomp: PLG224.BIN vs_effect_plg224_0_origin
// (src/EFFECT/particleSetup.h:37; was func_800F98F8).
void Plg224::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg224, frame,
                            host);
}

// decomp: PLG255.BIN vs_effect_plg255_1_origin (src/EFFECT/particleSetup.h:37;
// was func_800FBAF8). decomp: PLG255.BIN vs_effect_plg255_1_origin
// (src/EFFECT/particleSetup.h:37; was func_800FBAF8).
void Plg255::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg255, frame,
                            host);
}

// decomp: PLG263.BIN vs_effect_plg263_3_origin (src/EFFECT/particleSetup.h:37;
// was func_800FC00C).
void Plg263::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg263, frame,
                            host);
}

// decomp: PLG265.BIN vs_effect_plg265_1_origin (src/EFFECT/particleSetup.h:37;
// was func_800FA9E0). decomp: PLG265.BIN vs_effect_plg265_1_origin
// (src/EFFECT/particleSetup.h:37; was func_800FA9E0).
void Plg265::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg265, frame,
                            host);
}

// decomp: PLG265.BIN vs_effect_plg265_3_origin (src/EFFECT/particleSetup.h:37;
// was func_800FE0F4).
void Plg265::ParticleOrigin2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg265Second,
                            frame, host);
}

// decomp: PLG266.BIN vs_effect_plg266_0_origin (src/EFFECT/particleSetup.h:37;
// was func_800F9AE8).
void Plg266::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg266, frame,
                            host);
}

// decomp: PLG266.BIN vs_effect_plg266_1_origin (src/EFFECT/particleSetup.h:37;
// was func_800FB78C). decomp: PLG266.BIN vs_effect_plg266_1_origin
// (src/EFFECT/particleSetup.h:37; was func_800FB78C).
void Plg266::ParticleOrigin2(Templates::ParticleSetFrame& frame,
                             Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg266Second,
                            frame, host);
}

// decomp: PLG300.BIN vs_effect_plg300_1_origin (src/EFFECT/particleSetup.h:37;
// was func_800FA584).
void Plg300::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg300, frame,
                            host);
}

// decomp: PLG301.BIN vs_effect_plg301_0_origin (src/EFFECT/particleSetup.h:37;
// was func_800F9910).
void Plg301::ParticleOrigin(Templates::ParticleSetFrame& frame,
                            Templates::ParticleSetHost& host) {
  Templates::ParticleOrigin(Templates::ParticleEmissionCounter::kPlg301, frame,
                            host);
}
}  // namespace VS::Effects
