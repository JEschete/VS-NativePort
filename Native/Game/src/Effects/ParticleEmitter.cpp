#include "Effects/ParticleEmitter.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <vector>

#include "Core/Math/Vector.hpp"

namespace VS::Effects {
namespace {
namespace math = Core::Math;
using math::Vec2;
using math::Vec3;

constexpr std::int32_t kOne = 4096;
constexpr unsigned kFractionBits = 12;
constexpr std::uint32_t kAnchorMask = 7;
constexpr std::uint32_t kParentAnchor = 4;
constexpr std::uint32_t kShapeMask = 0x38;
constexpr std::uint32_t kDistributionMask = 0x1800;
constexpr std::uint32_t kSurface = 0x800;
constexpr std::uint32_t kVertices = 0x1000;
constexpr std::uint32_t kDirectionMask = 0xE000;
constexpr std::uint32_t kTowardOrigin = 0x2000;
constexpr std::uint32_t kTowardSource = 0x4000;
constexpr std::uint32_t kTowardDestination = 0x6000;
constexpr std::uint32_t kActorDirection = 0x8000;
constexpr std::uint32_t kAttractionMask = 0x1C0;
constexpr std::uint32_t kActorAttraction = 0xC0;
constexpr std::uint32_t kOffsetTargetMode = 0x40;
constexpr std::uint32_t kClearTargetMode = 0x80;
constexpr std::uint32_t kClearTargetMode2 = 0x140;
constexpr std::uint8_t kVariantMask = 0xF;
constexpr std::uint8_t kDefaultTransparency = 4;
constexpr std::int32_t kPolygonPhase = 0x200;
constexpr std::int32_t kPolygonFillRange = 0x1194;
constexpr std::int32_t kQuadrilateralVertices = 4;
constexpr std::int32_t kBoxVertexMask = 7;
constexpr int kFirstZFace = 4;
constexpr int kZSignBit = 4;
constexpr std::int32_t kSpeedScale = 32;

enum class SpawnShape : std::uint32_t {
  kSphere = 0,
  kBox = 8,
  kCone = 16,
  kCylinder = 24,
  kDisc = 32,
  kQuadrilateral = 40,
  kPolygon = 48,
  kUnchanged = 56,
};

[[nodiscard]] std::int16_t Halfword(std::int32_t value) {
  return math::WrapSigned16(static_cast<std::uint32_t>(value));
}

[[nodiscard]] Vec3 Halfwords(Vec3 value) {
  return {Halfword(value.x), Halfword(value.y), Halfword(value.z)};
}

[[nodiscard]] ParticleOffset Offset(Vec3 value) {
  return {Halfword(value.x), Halfword(value.y), Halfword(value.z)};
}

[[nodiscard]] Vec3 Wide(ParticleOffset value) {
  return {value.x, value.y, value.z};
}

[[nodiscard]] std::int32_t Fixed(std::int32_t value, std::int32_t factor) {
  return math::WrappingMultiply(value, factor) >> kFractionBits;
}

[[nodiscard]] Vec3 Fixed(Vec3 value, std::int32_t factor) {
  return {Fixed(value.x, factor), Fixed(value.y, factor),
          Fixed(value.z, factor)};
}

[[nodiscard]] Vec3 Whole(Vec3 value) {
  return {value.x >> kFractionBits, value.y >> kFractionBits,
          value.z >> kFractionBits};
}

[[nodiscard]] Vec3 ToFixed(Vec3 value) {
  return {math::WrappingMultiply(value.x, kOne),
          math::WrappingMultiply(value.y, kOne),
          math::WrappingMultiply(value.z, kOne)};
}

class SpawnRotation final : public ParticleRotation {
 public:
  explicit SpawnRotation(const math::Mat3& matrix) : matrix_(matrix) {}
  [[nodiscard]] Vec3 Rotate(ParticleOffset offset) override {
    const auto value = math::RotateTranslate({matrix_, {}}, Wide(offset));
    // ApplyRotMatrix stores saturated IRs, unlike the full-MAC RotTrans path.
    constexpr std::int32_t kIrMin = std::numeric_limits<std::int16_t>::min();
    constexpr std::int32_t kIrMax = std::numeric_limits<std::int16_t>::max();
    return {std::clamp(value.x, kIrMin, kIrMax),
            std::clamp(value.y, kIrMin, kIrMax),
            std::clamp(value.z, kIrMin, kIrMax)};
  }

 private:
  const math::Mat3& matrix_;
};

struct EmissionBatch {
  std::optional<ParticleEmissionHandle> first{};
  std::vector<ParticleEmissionHandle> allocated;
};

template <typename Update>
[[nodiscard]] bool Visit(const EmissionBatch& batch, ParticleEmissionHost& host,
                         Update update) {
  auto current = batch.first;
  for (auto expected = batch.allocated.rbegin();
       expected != batch.allocated.rend(); ++expected) {
    if (!current || *current != *expected) return false;
    auto particle = host.Read(*current);
    if (!particle) return false;
    update(*particle);
    host.Store(*current, *particle);
    current = particle->next;
  }
  return true;
}

void Rollback(const EmissionBatch& batch, ParticleEmissionHost& host) {
  for (auto particle = batch.allocated.rbegin();
       particle != batch.allocated.rend(); ++particle) {
    host.Release(*particle);
  }
}

class Emitter {
 public:
  Emitter(const ParticleEmissionDefinition& definition,
          const ParticleEmissionContext& context,
          ParticleEmissionScratch& scratch, std::int32_t& counter,
          Core::Rand15& random, ParticleEmissionHost& host)
      : definition_(definition),
        context_(context),
        scratch_(scratch),
        counter_(counter),
        random_(random),
        host_(host) {}

  [[nodiscard]] ParticleEmissionResult Run();

 private:
  [[nodiscard]] std::int32_t Sample(EmissionCurve curve) const {
    const auto index = definition_.curves.at(static_cast<std::size_t>(curve));
    return context_.samples[index];
  }
  [[nodiscard]] Vec3 Vector(EmissionVector field, EmissionCurve curve) const {
    return math::InterpolateSevenBitVector(
        definition_.vectors.at(static_cast<std::size_t>(field)), Sample(curve));
  }
  [[nodiscard]] Vec2 Pair(EmissionPair field, EmissionCurve curve) const {
    return math::InterpolateSevenBitPlane(
        definition_.pairs.at(static_cast<std::size_t>(field)), Sample(curve));
  }
  [[nodiscard]] std::int32_t Uniform(std::int32_t first, std::int32_t second) {
    return Core::Random::RandomRangeExclusive(random_, first, second);
  }
  [[nodiscard]] std::int32_t Uniform(Vec2 range) {
    return Uniform(range.x, range.y);
  }
  [[nodiscard]] Vec3 Uniform(Vec3 first, Vec3 second) {
    return {Uniform(first.x, second.x), Uniform(first.y, second.y),
            Uniform(first.z, second.z)};
  }
  [[nodiscard]] std::int32_t WideUniform(std::int32_t first,
                                         std::int32_t second);
  [[nodiscard]] ParticleEmissionStatus Validate() const;
  void SetHeader();
  [[nodiscard]] bool Allocate(std::int32_t count);
  void PrepareShape();
  void SphericalOffset();
  void BoxOffset();
  void BoxFace();
  void RadialOffset(SpawnShape shape);
  void PolygonOffset();
  void Place(EmittedParticle& particle);
  [[nodiscard]] bool SetVelocities();
  void DirectedVelocity(EmittedParticle& particle,
                        const math::Mat3& orientation, bool rotate);
  void InwardVelocity(EmittedParticle& particle);
  [[nodiscard]] bool SetForces();
  [[nodiscard]] bool SetLifetime();
  [[nodiscard]] bool SetTargets();
  [[nodiscard]] bool SetAttributes();

  const ParticleEmissionDefinition& definition_;
  const ParticleEmissionContext& context_;
  ParticleEmissionScratch& scratch_;
  std::int32_t& counter_;
  Core::Rand15& random_;
  ParticleEmissionHost& host_;
  EmissionBatch batch_{};
  std::optional<math::Mat3> rotation_{};
  std::int32_t fraction_{};
  std::int32_t polygon_step_{};
  Vec2 previous_vertex_{};
};

ParticleEmissionStatus Emitter::Validate() const {
  if (std::ranges::any_of(definition_.curves, [this](std::uint8_t curve) {
        return curve >= context_.samples.size();
      })) {
    return ParticleEmissionStatus::kMissingSamples;
  }
  if (context_.capacity < 0 || context_.used < 0 ||
      context_.used > context_.capacity) {
    return ParticleEmissionStatus::kInvalidPool;
  }
  const auto direction = definition_.flags & kDirectionMask;
  if ((direction == kTowardSource || direction == kTowardDestination) &&
      !context_.aim_target) {
    return ParticleEmissionStatus::kMissingAim;
  }
  return ParticleEmissionStatus::kEmitted;
}

void Emitter::SetHeader() {
  scratch_.flags = definition_.flags;
  scratch_.saved_flags = static_cast<std::uint16_t>(definition_.flags);
  scratch_.transform = definition_.transform;
  scratch_.attraction_transform = definition_.attraction_transform;
  if (context_.transparency_override != 0) {
    scratch_.transparency =
        static_cast<std::uint8_t>(context_.transparency_override);
  } else if (definition_.transparency != 0) {
    scratch_.transparency = definition_.transparency;
  } else {
    scratch_.transparency =
        context_.parent ? context_.parent->transparency : kDefaultTransparency;
  }
}

bool Emitter::Allocate(std::int32_t count) {
  try {
    batch_.allocated.reserve(static_cast<std::size_t>(count));
  } catch (const std::bad_alloc&) {
    return false;
  }
  for (std::int32_t index = 0; index < count; ++index) {
    const auto allocated = host_.Allocate();
    if (!allocated) return false;
    batch_.allocated.push_back(*allocated);
    auto particle = host_.Read(*allocated);
    if (!particle) return false;
    batch_.first = *allocated;
    particle->transform = scratch_.transform;
    particle->attraction_transform = scratch_.attraction_transform;
    host_.Store(*allocated, *particle);
    host_.InitializeTexture(*allocated, definition_.texture,
                            definition_.animation);
  }
  return true;
}

std::int32_t Emitter::WideUniform(std::int32_t first, std::int32_t second) {
  if (first == second) return first;
  const auto low = std::min(first, second);
  const auto high = std::max(first, second);
  const auto choices = math::WrappingAdd(math::WrappingSubtract(high, low), 1);
  // The low draw precedes the high draw in the original JAL sequence.
  const auto low_draw = static_cast<std::uint32_t>(random_.Next());
  const auto high_draw = static_cast<std::uint32_t>(random_.Next());
  const auto value = static_cast<std::int32_t>(low_draw + (high_draw << 16));
  return math::WrappingAdd(math::R3000Remainder(value, choices), low);
}

void Emitter::PrepareShape() {
  scratch_.position = math::InterpolateSevenBitVector(
      definition_.position, Sample(EmissionCurve::kPosition));
  switch (scratch_.flags & kAnchorMask) {
    case 1:
      scratch_.position =
          math::AddVectors(scratch_.position, context_.effect_origin);
      break;
    case 2:
      scratch_.position =
          math::AddVectors(scratch_.position, context_.transform.translation);
      break;
    case kParentAnchor:
      if (context_.parent) {
        scratch_.position = math::AddVectors(scratch_.position,
                                             Whole(context_.parent->position));
      }
      break;
    default:
      break;
  }
  scratch_.position = ToFixed(scratch_.position);
  scratch_.extent = math::InterpolateSevenBitVector(
      definition_.extent, Sample(EmissionCurve::kExtent));
  fraction_ = math::InterpolateSevenBitScalar(
      definition_.inner_extent, Sample(EmissionCurve::kInnerExtent));
  rotation_ = context_.transform.rotation;
  const auto shape = static_cast<SpawnShape>(scratch_.flags & kShapeMask);
  if (shape <= SpawnShape::kBox) {
    scratch_.inner_extent = Fixed(scratch_.extent, fraction_);
  } else if (shape <= SpawnShape::kDisc) {
    scratch_.inner_extent.x = Fixed(scratch_.extent.x, fraction_);
    scratch_.inner_extent.z = Fixed(scratch_.extent.z, fraction_);
    if (shape == SpawnShape::kCone) scratch_.inner_extent.y = scratch_.extent.y;
  } else if (shape == SpawnShape::kQuadrilateral ||
             shape == SpawnShape::kPolygon) {
    scratch_.ring_segments =
        shape == SpawnShape::kQuadrilateral
            ? kQuadrilateralVertices
            : std::max<std::int32_t>(definition_.ring_segments, 1);
    scratch_.ring_positions =
        shape == SpawnShape::kQuadrilateral
            ? kQuadrilateralVertices
            : (definition_.ring_positions == 0 ? scratch_.ring_segments
                                               : definition_.ring_positions);
    polygon_step_ = kOne / scratch_.ring_segments;
    if ((scratch_.flags & kDistributionMask) != kVertices) {
      counter_ = math::R3000Remainder(counter_, scratch_.ring_positions);
      const auto angle = math::WrappingAdd(
          math::WrappingMultiply(counter_, polygon_step_), kPolygonPhase);
      previous_vertex_ = {Fixed(math::Cosine(angle), scratch_.extent.x),
                          Fixed(math::Sine(angle), scratch_.extent.z)};
    }
  }
}

void Emitter::SphericalOffset() {
  const auto first_angle = random_.Next();
  scratch_.sin_angle0 = math::Sine(first_angle);
  scratch_.cos_angle0 = math::Cosine(first_angle);
  const auto second_angle = random_.Next();
  scratch_.sin_angle1 = math::Sine(second_angle);
  scratch_.cos_angle1 = math::Cosine(second_angle);
  const auto distribution = scratch_.flags & kDistributionMask;
  if (distribution == 0) {
    scratch_.offset = Offset(Uniform(scratch_.extent, scratch_.inner_extent));
  } else if (distribution == kSurface || distribution == kVertices) {
    scratch_.offset = Offset(scratch_.extent);
  } else {
    return;
  }
  scratch_.offset = Offset({Fixed(Fixed(scratch_.offset.x, scratch_.cos_angle0),
                                  scratch_.cos_angle1),
                            Fixed(Fixed(scratch_.offset.y, scratch_.cos_angle0),
                                  scratch_.sin_angle1),
                            Fixed(scratch_.offset.z, scratch_.sin_angle0)});
}

void Emitter::BoxFace() {
  const auto face = random_.Next() % 6;
  const Vec3 extent = scratch_.extent;
  const auto sign = (face & 1) == 0 ? 1 : -1;
  if (face < 2) {
    scratch_.offset =
        Offset({math::WrappingMultiply(extent.x, sign),
                Uniform(extent.y, math::WrappingNegate(extent.y)),
                Uniform(extent.z, math::WrappingNegate(extent.z))});
  } else if (face < kFirstZFace) {
    scratch_.offset =
        Offset({Uniform(extent.x, math::WrappingNegate(extent.x)),
                math::WrappingMultiply(extent.y, sign),
                Uniform(extent.z, math::WrappingNegate(extent.z))});
  } else {
    scratch_.offset = Offset({Uniform(extent.x, math::WrappingNegate(extent.x)),
                              Uniform(extent.y, math::WrappingNegate(extent.y)),
                              math::WrappingMultiply(extent.z, sign)});
  }
}

void Emitter::BoxOffset() {
  const auto distribution = scratch_.flags & kDistributionMask;
  if (distribution == kSurface) {
    BoxFace();
  } else if (distribution == kVertices) {
    counter_ = math::WrappingAdd(counter_, 1) & kBoxVertexMask;
    scratch_.offset =
        Offset({(counter_ & 1) != 0 ? scratch_.extent.x
                                    : math::WrappingNegate(scratch_.extent.x),
                (counter_ & 2) != 0 ? scratch_.extent.y
                                    : math::WrappingNegate(scratch_.extent.y),
                (counter_ & kZSignBit) != 0
                    ? scratch_.extent.z
                    : math::WrappingNegate(scratch_.extent.z)});
  } else if (distribution == 0) {
    const auto signs = random_.Next();
    const auto component = [this, signs](std::int32_t extent,
                                         std::int32_t inner, int bit) {
      return (signs & bit) != 0 ? Uniform(extent, inner)
                                : Uniform(math::WrappingNegate(extent),
                                          math::WrappingNegate(inner));
    };
    scratch_.offset = Offset(
        {component(scratch_.extent.x, scratch_.inner_extent.x, 1),
         component(scratch_.extent.y, scratch_.inner_extent.y, 2),
         component(scratch_.extent.z, scratch_.inner_extent.z, kZSignBit)});
  }
}

void Emitter::RadialOffset(SpawnShape shape) {
  const auto angle = random_.Next();
  scratch_.sin_angle0 = math::Sine(angle);
  scratch_.cos_angle0 = math::Cosine(angle);
  const auto distribution = scratch_.flags & kDistributionMask;
  if (distribution == 0) {
    const auto radius = [this, shape](std::int32_t extent, std::int32_t inner) {
      return shape == SpawnShape::kCone
                 ? Uniform(extent, inner)
                 : math::SquareRootInteger(
                       WideUniform(math::WrappingMultiply(extent, extent),
                                   math::WrappingMultiply(inner, inner)));
    };
    scratch_.offset.x =
        Halfword(radius(scratch_.extent.x, scratch_.inner_extent.x));
    scratch_.offset.z =
        Halfword(radius(scratch_.extent.z, scratch_.inner_extent.z));
  } else if (distribution == kSurface || distribution == kVertices) {
    scratch_.offset.x = Halfword(scratch_.extent.x);
    scratch_.offset.z = Halfword(scratch_.extent.z);
  } else {
    return;
  }
  if (shape == SpawnShape::kCone) {
    scratch_.offset.y = Halfword(math::SquareRootInteger(WideUniform(
        math::WrappingMultiply(scratch_.extent.y, scratch_.extent.y), 0)));
    scratch_.offset.x = Halfword(math::R3000Quotient(
        math::WrappingMultiply(Fixed(scratch_.offset.x, scratch_.cos_angle0),
                               scratch_.offset.y),
        scratch_.extent.y));
    scratch_.offset.z = Halfword(math::R3000Quotient(
        math::WrappingMultiply(Fixed(scratch_.offset.z, scratch_.sin_angle0),
                               scratch_.offset.y),
        scratch_.extent.y));
  } else {
    scratch_.offset.x = Halfword(Fixed(scratch_.offset.x, scratch_.cos_angle0));
    scratch_.offset.y = shape == SpawnShape::kCylinder
                            ? Halfword(Uniform(scratch_.extent.y, 0))
                            : 0;
    scratch_.offset.z = Halfword(Fixed(scratch_.offset.z, scratch_.sin_angle0));
  }
}

void Emitter::PolygonOffset() {
  counter_ = math::R3000Remainder(math::WrappingAdd(counter_, 1),
                                  scratch_.ring_positions);
  const auto angle = math::WrappingAdd(
      math::WrappingMultiply(counter_, polygon_step_), kPolygonPhase);
  const Vec2 vertex{Fixed(math::Cosine(angle), scratch_.extent.x),
                    Fixed(math::Sine(angle), scratch_.extent.z)};
  const auto distribution = scratch_.flags & kDistributionMask;
  if (distribution == kVertices) {
    scratch_.offset = Offset({vertex.x, 0, vertex.y});
    return;
  }
  const auto along = random_.Next() >> 3;
  scratch_.offset = Offset(
      {math::WrappingAdd(
           Fixed(math::WrappingSubtract(vertex.x, previous_vertex_.x), along),
           previous_vertex_.x),
       0,
       math::WrappingAdd(
           Fixed(math::WrappingSubtract(vertex.y, previous_vertex_.y), along),
           previous_vertex_.y)});
  previous_vertex_ = vertex;
  if (distribution != kSurface) {
    const auto range = math::WrappingSubtract(kPolygonFillRange, fraction_);
    const auto sample = math::WrappingAdd(
        math::WrappingMultiply(random_.Next(), range) >> 15, fraction_);
    const auto scale = std::min(math::SquareRootFixed(sample), kOne);
    scratch_.offset = Offset(Fixed(Wide(scratch_.offset), scale));
  }
}

void Emitter::Place(EmittedParticle& particle) {
  const auto shape = static_cast<SpawnShape>(scratch_.flags & kShapeMask);
  switch (shape) {
    case SpawnShape::kSphere:
      SphericalOffset();
      break;
    case SpawnShape::kBox:
      BoxOffset();
      break;
    case SpawnShape::kCone:
    case SpawnShape::kCylinder:
    case SpawnShape::kDisc:
      RadialOffset(shape);
      break;
    case SpawnShape::kQuadrilateral:
    case SpawnShape::kPolygon:
      PolygonOffset();
      break;
    default:
      return;
  }
  SpawnRotation rotation(context_.transform.rotation);
  const auto spawn = PlaceParticleSpawn(scratch_.flags, scratch_.offset,
                                        scratch_.position, rotation);
  scratch_.aim = spawn.aim;
  particle.motion.position = spawn.position;
}

void Emitter::DirectedVelocity(EmittedParticle& particle,
                               const math::Mat3& orientation, bool rotate) {
  const Vec3 jitter = scratch_.direction_jitter;
  scratch_.offset = Offset(math::AddVectors(
      scratch_.base_direction,
      Uniform(jitter,
              {math::WrappingNegate(jitter.x), math::WrappingNegate(jitter.y),
               math::WrappingNegate(jitter.z)})));
  scratch_.direction_matrix = math::BuildRotationXyz(
      {scratch_.offset.x, scratch_.offset.y, scratch_.offset.z});
  rotation_ = scratch_.direction_matrix;
  scratch_.offset = {0, 0, kOne};
  scratch_.direction = math::RotateTranslate({scratch_.direction_matrix, {}},
                                             Wide(scratch_.offset));
  if (rotate) {
    scratch_.offset = Offset(scratch_.direction);
    rotation_ = orientation;
    scratch_.direction =
        math::RotateTranslate({orientation, {}}, Wide(scratch_.offset));
  }
  scratch_.speed = Uniform(scratch_.speed_range);
  particle.motion.velocity = Fixed(scratch_.direction, scratch_.speed);
}

void Emitter::InwardVelocity(EmittedParticle& particle) {
  scratch_.direction =
      Whole(math::SubtractVectors(scratch_.position, particle.motion.position));
  scratch_.speed = Uniform(scratch_.speed_range);
  if (scratch_.direction != Vec3{}) {
    scratch_.direction = math::NormalizeVector(scratch_.direction).direction;
    particle.motion.velocity = Fixed(scratch_.direction, scratch_.speed);
  } else {
    particle.motion.velocity = {0, scratch_.speed, 0};
  }
}

bool Emitter::SetVelocities() {
  scratch_.speed_range = Pair(EmissionPair::kSpeed, EmissionCurve::kSpeed);
  scratch_.speed_range.x =
      math::WrappingMultiply(scratch_.speed_range.x, kSpeedScale);
  scratch_.speed_range.y =
      math::WrappingMultiply(scratch_.speed_range.y, kSpeedScale);
  scratch_.direction_jitter =
      Vector(EmissionVector::kDirectionJitter, EmissionCurve::kDirectionJitter);
  scratch_.base_direction =
      Vector(EmissionVector::kDirection, EmissionCurve::kDirection);
  const auto mode = scratch_.flags & kDirectionMask;
  math::Mat3 orientation = context_.transform.rotation;
  if ((mode == kTowardSource || mode == kTowardDestination) &&
      context_.aim_target) {
    scratch_.offset = Offset(*context_.aim_target);
    scratch_.aim = Wide(scratch_.offset);
    scratch_.spawn = Whole(scratch_.position);
    scratch_.aim_matrix =
        math::BuildLookAtOrientation(scratch_.spawn, scratch_.aim);
    orientation = scratch_.aim_matrix;
  }
  return Visit(batch_, host_, [this, mode, &orientation](auto& particle) {
    if (mode == kTowardOrigin) {
      InwardVelocity(particle);
    } else if (mode == 0 || mode == kTowardSource ||
               mode == kTowardDestination || mode == kActorDirection) {
      DirectedVelocity(particle, orientation,
                       mode == kActorDirection ||
                           (scratch_.flags & kParticleRotateSpawnOffset) != 0);
    }
  });
}

bool Emitter::SetForces() {
  scratch_.force_min =
      Halfwords(Vector(EmissionVector::kForceMin, EmissionCurve::kForce));
  scratch_.force_max =
      Halfwords(Vector(EmissionVector::kForceMax, EmissionCurve::kForce));
  scratch_.acceleration_min = Halfwords(
      Vector(EmissionVector::kAccelerationMin, EmissionCurve::kAcceleration));
  scratch_.acceleration_max = Halfwords(
      Vector(EmissionVector::kAccelerationMax, EmissionCurve::kAcceleration));
  return Visit(batch_, host_, [this](auto& particle) {
    particle.motion.force = Uniform(scratch_.force_min, scratch_.force_max);
    particle.motion.acceleration =
        Uniform(scratch_.acceleration_min, scratch_.acceleration_max);
  });
}

bool Emitter::SetLifetime() {
  scratch_.mass_range = Pair(EmissionPair::kMass, EmissionCurve::kMass);
  scratch_.gravity_range =
      Pair(EmissionPair::kGravityScale, EmissionCurve::kGravityScale);
  scratch_.lifetime_range =
      Pair(EmissionPair::kLifetime, EmissionCurve::kLifetime);
  return Visit(batch_, host_, [this](auto& particle) {
    particle.motion.mass = Halfword(Uniform(scratch_.mass_range));
    particle.motion.gravity_scale = Halfword(Uniform(scratch_.gravity_range));
    particle.lifetime = Halfword(Uniform(scratch_.lifetime_range));
  });
}

bool Emitter::SetTargets() {
  scratch_.attraction_offset = Halfwords(Vector(
      EmissionVector::kAttractionOffset, EmissionCurve::kAttractionOffset));
  scratch_.attraction_jitter = Halfwords(Vector(
      EmissionVector::kAttractionJitter, EmissionCurve::kAttractionJitter));
  return Visit(batch_, host_, [this](auto& particle) {
    const auto mode = scratch_.flags & kAttractionMask;
    Vec3 target = scratch_.attraction_offset;
    if (mode == kActorAttraction) {
      const Vec3 jitter = scratch_.attraction_jitter;
      // 800D44E8 loads Y for the negative X bound, not X.
      target = Halfwords(math::AddVectors(
          math::AddVectors(context_.attraction_translation, target),
          {Uniform(jitter.x, math::WrappingNegate(jitter.y)),
           Uniform(jitter.y, math::WrappingNegate(jitter.y)),
           Uniform(jitter.z, math::WrappingNegate(jitter.z))}));
    } else if (mode == kClearTargetMode || mode == kClearTargetMode2) {
      target = {};
    } else if (mode != 0 && mode != kOffsetTargetMode) {
      return;
    }
    particle.motion.target = {Halfword(target.x), Halfword(target.y),
                              Halfword(target.z)};
  });
}

bool Emitter::SetAttributes() {
  scratch_.attraction_range =
      Pair(EmissionPair::kAttraction, EmissionCurve::kAttraction);
  if (!batch_.first) return false;
  const auto first = host_.Read(*batch_.first);
  if (!first) return false;
  scratch_.color_curves = definition_.color_curves;
  scratch_.age = first->age;
  const auto variant = context_.variant == 0 && context_.parent
                           ? context_.parent->variant
                           : static_cast<std::uint8_t>(context_.variant);
  return Visit(batch_, host_, [this, variant](auto& particle) {
    particle.flags = scratch_.flags;
    particle.motion.attraction = Uniform(scratch_.attraction_range);
    particle.color_curves = scratch_.color_curves;
    particle.age = scratch_.age;
    particle.end_event = definition_.end_event;
    particle.tick_event = definition_.tick_event;
    particle.transparency = scratch_.transparency;
    particle.variant = variant & kVariantMask;
  });
}

ParticleEmissionResult Emitter::Run() {
  const auto validation = Validate();
  if (validation != ParticleEmissionStatus::kEmitted) {
    return {.status = validation};
  }
  SetHeader();
  const auto interval = math::InterpolateSevenBitScalar(
      definition_.interval, Sample(EmissionCurve::kInterval));
  if (math::R3000Remainder(context_.frame, interval) != 0) {
    return {.status = ParticleEmissionStatus::kIntervalSkipped};
  }
  auto count = math::InterpolateSevenBitScalar(definition_.count,
                                               Sample(EmissionCurve::kCount));
  if (math::WrappingAdd(count, context_.used) >= context_.capacity) {
    count = context_.capacity - context_.used;
  }
  if (count <= 0) {
    return {.status = ParticleEmissionStatus::kEmpty,
            .clear_translation = true};
  }
  if (!Allocate(count)) {
    Rollback(batch_, host_);
    return {.status = ParticleEmissionStatus::kAllocationRefused,
            .clear_translation = true};
  }
  PrepareShape();
  const bool complete =
      Visit(batch_, host_, [this](auto& particle) { Place(particle); }) &&
      SetVelocities() && SetForces() && SetLifetime() && SetTargets() &&
      SetAttributes();
  if (!complete) {
    Rollback(batch_, host_);
    return {.status = ParticleEmissionStatus::kInvalidLinks,
            .clear_translation = true,
            .rotation = rotation_};
  }
  return {.status = ParticleEmissionStatus::kEmitted,
          .count = count,
          .clear_translation = true,
          .rotation = rotation_};
}
}  // namespace

// decomp: BATTLE.PRG func_800D2ADC (src/BATTLE/BATTLE.PRG/5BF94.c:6497).
ParticleEmissionResult EmitParticleEvent(
    const ParticleEmissionDefinition& definition,
    const ParticleEmissionContext& context, ParticleEmissionScratch& scratch,
    std::int32_t& emission_counter, Core::Rand15& random,
    ParticleEmissionHost& host) {
  return Emitter(definition, context, scratch, emission_counter, random, host)
      .Run();
}
}  // namespace VS::Effects
