// Hand-maintained (tools/port/owned.csv).
#include "Effects/TrailEmitter.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace VS::Effects {
namespace {
constexpr std::int32_t kOne = 4096;
constexpr int kQ12Shift = 12;
constexpr std::int32_t kRingAngleOffset = kOne / 8;
constexpr std::int32_t kBoxFaces = 6;
constexpr std::int32_t kCornerMask = 7;
// Corner index bits pick the positive side of each box axis.
constexpr std::int32_t kCornerX = 1;
constexpr std::int32_t kCornerY = 2;
constexpr std::int32_t kCornerZ = 4;
constexpr int kRingInterpolationShift = 3;
constexpr std::int32_t kStoppedLifetime = 1;
constexpr std::int32_t kLifetimeMask = 0xFF;
constexpr int kDefinitionShift = 8;

enum class BoxFace : std::int32_t {
  kPositiveX,
  kNegativeX,
  kPositiveY,
  kNegativeY,
  kPositiveZ,
  kNegativeZ
};

// MIPS mult/mflo and addu keep the low 32 bits of the result.
std::int32_t WrapMul(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) *
                                   static_cast<std::uint32_t>(b));
}

std::int32_t WrapAdd(std::int32_t a, std::int32_t b) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) +
                                   static_cast<std::uint32_t>(b));
}

std::int32_t Q12(std::int32_t a, std::int32_t b) {
  return WrapMul(a, b) >> kQ12Shift;
}

std::int16_t Narrow(std::int32_t value) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

TrailPoint NarrowPoint(TrailVector value) {
  return {Narrow(value.x), Narrow(value.y), Narrow(value.z)};
}

TrailVector Add(TrailVector a, TrailVector b) {
  return {WrapAdd(a.x, b.x), WrapAdd(a.y, b.y), WrapAdd(a.z, b.z)};
}

TrailVector Widen(TrailPoint point) { return {point.x, point.y, point.z}; }

// Optional rotation, then the anchored position (trails5.h tail of each path).
void FinishOrigin(TrailPoint& point, std::uint32_t flags,
                  TrailOriginScratch& scratch, TrailOriginServices& services) {
  if ((flags & kTrailRotateOrigins) != 0) {
    scratch.aim_point = services.Rotate(point);
    point = NarrowPoint(scratch.aim_point);
  }
  point = NarrowPoint(Add(Widen(point), scratch.position));
}

void PlaceOnSphere(std::span<TrailPoint> origins, std::uint32_t flags,
                   TrailOriginScratch& scratch, TrailOriginServices& services) {
  const TrailVector extent = scratch.extent;
  for (TrailPoint& point : origins) {
    std::int32_t angle = services.Rand();
    scratch.sin_angle0 = services.Sin(angle);
    scratch.cos_angle0 = services.Cos(angle);
    angle = services.Rand();
    scratch.sin_angle1 = services.Sin(angle);
    scratch.cos_angle1 = services.Cos(angle);
    point.x =
        Narrow(Q12(Q12(extent.x, scratch.cos_angle0), scratch.cos_angle1));
    point.y =
        Narrow(Q12(Q12(extent.y, scratch.cos_angle0), scratch.sin_angle1));
    point.z = Narrow(Q12(extent.z, scratch.sin_angle0));
    FinishOrigin(point, flags, scratch, services);
  }
}

std::int16_t AcrossFace(std::int32_t extent, TrailOriginServices& services) {
  return Narrow(services.RandomRangeExclusive(extent, -extent));
}

void PlaceOnBoxFace(TrailPoint& point, TrailVector extent,
                    TrailOriginServices& services) {
  const auto face = static_cast<BoxFace>(services.Rand() % kBoxFaces);
  switch (face) {
    case BoxFace::kPositiveX:
    case BoxFace::kNegativeX:
      point.x = Narrow(face == BoxFace::kPositiveX ? extent.x : -extent.x);
      point.y = AcrossFace(extent.y, services);
      point.z = AcrossFace(extent.z, services);
      break;
    case BoxFace::kPositiveY:
    case BoxFace::kNegativeY:
      point.y = Narrow(face == BoxFace::kPositiveY ? extent.y : -extent.y);
      point.x = AcrossFace(extent.x, services);
      point.z = AcrossFace(extent.z, services);
      break;
    case BoxFace::kPositiveZ:
    case BoxFace::kNegativeZ:
      point.z = Narrow(face == BoxFace::kPositiveZ ? extent.z : -extent.z);
      point.x = AcrossFace(extent.x, services);
      point.y = AcrossFace(extent.y, services);
      break;
  }
}

void PlaceOnBoxCorner(TrailPoint& point, TrailVector extent,
                      std::int32_t& corner) {
  const std::int32_t next = WrapAdd(corner, 1);
  corner = next & kCornerMask;
  point.x = Narrow((next & kCornerX) != 0 ? extent.x : -extent.x);
  point.y = Narrow((next & kCornerY) != 0 ? extent.y : -extent.y);
  point.z = Narrow((next & kCornerZ) != 0 ? extent.z : -extent.z);
}

void PlaceOnBox(std::span<TrailPoint> origins, std::uint32_t flags,
                std::int32_t& corner, TrailOriginScratch& scratch,
                TrailOriginServices& services) {
  for (TrailPoint& point : origins) {
    switch (flags & kTrailPlacementMask) {
      case kTrailPlacementCorners:
        PlaceOnBoxCorner(point, scratch.extent, corner);
        break;
      case kTrailPlacementNone:
        break;
      default:
        PlaceOnBoxFace(point, scratch.extent, services);
        break;
    }
    FinishOrigin(point, flags, scratch, services);
  }
}

struct RingPoint {
  std::int32_t x{};
  std::int32_t z{};
};

RingPoint RingPosition(std::int32_t corner, std::int32_t step,
                       TrailVector extent, TrailOriginServices& services) {
  const std::int32_t angle = WrapAdd(WrapMul(corner, step), kRingAngleOffset);
  return {Q12(services.Cos(angle), extent.x),
          Q12(services.Sin(angle), extent.z)};
}

std::int32_t Lerp(std::int32_t from, std::int32_t to, std::int32_t q12) {
  return WrapAdd(Q12(WrapAdd(to, -from), q12), from);
}

void PlaceOnRing(std::span<TrailPoint> origins, std::uint32_t flags,
                 std::int32_t& corner, TrailOriginScratch& scratch,
                 TrailOriginServices& services) {
  const std::int32_t step = kOne / scratch.ring_segments;
  const bool exact = (flags & kTrailPlacementMask) == kTrailPlacementCorners;
  RingPoint start{};
  if (!exact) {
    corner %= scratch.ring_positions;
    start = RingPosition(corner, step, scratch.extent, services);
  }
  for (TrailPoint& point : origins) {
    corner = WrapAdd(corner, 1) % scratch.ring_positions;
    const RingPoint end = RingPosition(corner, step, scratch.extent, services);
    if (exact) {
      point.x = Narrow(end.x);
      point.z = Narrow(end.z);
    } else {
      const std::int32_t t = services.Rand() >> kRingInterpolationShift;
      point.x = Narrow(Lerp(start.x, end.x, t));
      point.z = Narrow(Lerp(start.z, end.z, t));
    }
    point.y = 0;
    FinishOrigin(point, flags, scratch, services);
  }
}

// Sets the ring layout for a ring shape; false for shapes with no origin.
bool ConfigureRing(const TrailOriginRequest& request,
                   TrailOriginScratch& scratch) {
  constexpr std::int32_t kFixedRingPoints = 4;
  switch (request.flags & kTrailShapeMask) {
    case kTrailShapeFourPointRingA:
    case kTrailShapeFourPointRingB:
    case kTrailShapeFourPointRingC:
    case kTrailShapeFourPointRingD:
      scratch.ring_segments = kFixedRingPoints;
      scratch.ring_positions = kFixedRingPoints;
      return true;
    case kTrailShapeRing:
      scratch.ring_segments = std::max<std::int32_t>(request.ring_segments, 1);
      scratch.ring_positions = request.ring_positions != 0
                                   ? request.ring_positions
                                   : scratch.ring_segments;
      return true;
    default:
      return false;
  }
}

// `Samples` evenly spaced entries of a transparency curve: sample j reads
// entry j * (length - 1) / (Samples - 1), the original's
// (j * trail) / (TRAIL_SAMPLE_COUNT - 1) with `trail` the curve's length less
// one (left at one for a one-entry curve).
template <std::size_t Samples>
SampledTransparency<Samples> SampleTransparency(
    std::span<const std::uint8_t> curve) {
  SampledTransparency<Samples> result;
  if (curve.empty()) {
    result.samples.fill(kOpaqueTrailTransparency);
    return result;
  }
  const std::size_t span = curve.size() > 1 ? curve.size() - 1 : curve.size();
  for (std::size_t sample = 0; sample < Samples; ++sample) {
    std::size_t index = (sample * span) / (Samples - 1);
    if (index >= curve.size()) {
      index = curve.size() - 1;
      result.clamped = true;
    }
    result.samples.at(sample) = curve[index];
  }
  return result;
}
}  // namespace

// decomp: EFFECT VS_TRAIL_ORIGIN_FUNCTION (src/EFFECT/trailEffect.h:72).
// decomp: PLG074 func_800FB414 (src/EFFECT/trails5.h:27).
// decomp: PLG057.BIN vs_effect_thunderBurstLv1_2_origin
// (src/EFFECT/trailEffect.h:72; was func_800FD894). decomp: PLG073.BIN
// vs_effect_radialSurgeLv1_2_origin (src/EFFECT/trailEffect.h:72; was
// func_800FB414). decomp: PLG086.BIN vs_effect_judgment_3_origin
// (src/EFFECT/trailEffect.h:72; was func_800FC270). decomp: PLG087.BIN
// vs_effect_apocalypse_3_origin (src/EFFECT/trailEffect.h:72; was
// func_800FC048). decomp: PLG128.BIN vs_effect_bloodySin_6_origin
// (src/EFFECT/trailEffect.h:72; was func_800FE3D0). decomp: PLG134.BIN
// vs_effect_wyrmScorn_0_origin (src/EFFECT/trailEffect.h:72; was
// func_800F98E0). decomp: PLG137.BIN vs_effect_cherryRonde_1_origin
// (src/EFFECT/trailEffect.h:72; was func_800FA404). decomp: PLG141.BIN
// vs_effect_swallowSlash_0_origin (src/EFFECT/trailEffect.h:72; was
// func_800F98E0). decomp: PLG146.BIN vs_effect_blackNebula_4_origin
// (src/EFFECT/trailEffect.h:72; was func_800FD75C). decomp: PLG148.BIN
// vs_effect_accursedUmbra_0_origin (src/EFFECT/trailEffect.h:72; was
// func_800F98E0). decomp: PLG242.BIN vs_effect_plg242_2_origin
// (src/EFFECT/trailEffect.h:72; was func_800FB340). decomp: PLG255.BIN
// vs_effect_plg255_0_origin (src/EFFECT/trailEffect.h:72; was func_800F99D8).
// decomp: PLG265.BIN vs_effect_plg265_2_origin (src/EFFECT/trailEffect.h:72;
// was func_800FBFD4). decomp: PLG266.BIN vs_effect_plg266_2_origin
// (src/EFFECT/trailEffect.h:72; was func_800FCD80). decomp: PLG291.BIN
// vs_effect_plg291_0_origin (src/EFFECT/trailEffect.h:72; was func_800F98F8).
// decomp: EFFECT VS_TRAIL_ORIGIN_FUNCTION (src/EFFECT/trailEffect.h:72).
// decomp: PLG049.BIN vs_spiritSurge_0_origin (src/EFFECT/trailEffect.h:72; was
// func_800F98E0). decomp: PLG050.BIN vs_effect_darkChant_1_origin
// (src/EFFECT/trailEffect.h:72; was func_800FA504). decomp: PLG057.BIN
// vs_effect_thunderBurstLv1_1_origin (src/EFFECT/trailEffect.h:72; was
// func_800FB774). decomp: PLG069.BIN vs_effect_avalancheLv1_1_origin
// (src/EFFECT/trailEffect.h:72; was func_800FA3EC). decomp: PLG135.BIN
// vs_effect_rendingGale_1_origin (src/EFFECT/trailEffect.h:72; was
// func_800FA84C). decomp: PLG161.BIN vs_effect_gigaTempest_2_origin
// (src/EFFECT/trailEffect.h:72; was func_800FB3D4). decomp: PLG165.BIN
// vs_effect_deathWail_0_origin (src/EFFECT/trailEffect.h:72; was
// func_800F98F8). decomp: PLG167.BIN vs_effect_lotusPalm_2_origin
// (src/EFFECT/trailEffect.h:72; was func_800FB704). decomp: PLG168.BIN
// vs_effect_vertigo_0_origin (src/EFFECT/trailEffect.h:72; was func_800F99F0).
// decomp: PLG169.BIN vs_effect_vermillionAura_0_origin
// (src/EFFECT/trailEffect.h:72; was func_800F98E0). decomp: PLG232.BIN
// vs_effect_plg232_0_origin (src/EFFECT/trailEffect.h:72; was func_800F98E0).
// The nine-sample origin stage is the five-sample one over a nine-sample
// state; only the trail record type differs.
void GenerateTrailOrigins(const TrailOriginRequest& request,
                          std::span<TrailPoint> origins, std::int32_t& corner,
                          TrailOriginScratch& scratch,
                          TrailOriginServices& services) {
  scratch.position = request.position;
  switch (request.flags & kTrailAnchorMask) {
    case kTrailAnchorEffectOrigin:
      scratch.position = Add(scratch.position, request.effect_origin);
      break;
    case kTrailAnchorCaster:
      scratch.position = Add(scratch.position, request.caster_translation);
      break;
    default:
      break;
  }
  scratch.extent = request.extent;

  switch (request.flags & kTrailShapeMask) {
    case kTrailShapeSphere:
      PlaceOnSphere(origins, request.flags, scratch, services);
      return;
    case kTrailShapeBox:
      PlaceOnBox(origins, request.flags, corner, scratch, services);
      return;
    default:
      if (ConfigureRing(request, scratch)) {
        PlaceOnRing(origins, request.flags, corner, scratch, services);
      }
      return;
  }
}

TrailEmitterPlan PlanTrailEmitter(std::int32_t packed) {
  return {
      static_cast<std::uint8_t>(packed & kLifetimeMask),
      static_cast<std::uint8_t>((packed >> kDefinitionShift) & kLifetimeMask)};
}

std::optional<std::uint8_t> TrailCountFor(std::int16_t interval,
                                          std::size_t limit) {
  if (interval < 0) return std::nullopt;
  return static_cast<std::uint8_t>(
      std::min<std::size_t>(static_cast<std::size_t>(interval), limit));
}

// decomp: src/EFFECT/trailEffect.h:713-846 (setup samples of the transparency
// curve).
SampledTrailTransparency SampleTrailTransparency(
    std::span<const std::uint8_t> curve) {
  return SampleTransparency<kTrailTransparencySamples>(curve);
}

// decomp: src/EFFECT/trailEffect.h:713-846 (setup samples of the transparency
// curve).
SampledTransparency<kTrailNineTransparencySamples> SampleTrailNineTransparency(
    std::span<const std::uint8_t> curve) {
  return SampleTransparency<kTrailNineTransparencySamples>(curve);
}

TrailVector TrailEventPosition(TrailVector point) {
  return {WrapMul(point.x, kOne), WrapMul(point.y, kOne),
          WrapMul(point.z, kOne)};
}

// decomp: EFFECT VS_TRAIL_DISPATCH_FUNCTION (src/EFFECT/trailEffect.h:713).
// decomp: PLG074 func_800FCFEC (src/EFFECT/trails5.h:677).
// decomp: PLG057.BIN vs_effect_thunderBurstLv1_2 (src/EFFECT/trailEffect.h:713;
// was func_800FF46C). decomp: PLG073.BIN vs_effect_radialSurgeLv1_2
// (src/EFFECT/trailEffect.h:713; was func_800FCFEC). decomp: PLG086.BIN
// vs_effect_judgment_3 (src/EFFECT/trailEffect.h:713; was func_800FDE48).
// decomp: PLG087.BIN vs_effect_apocalypse_3 (src/EFFECT/trailEffect.h:713; was
// func_800FDC20). decomp: PLG128.BIN vs_effect_bloodySin_6
// (src/EFFECT/trailEffect.h:713; was func_800FFFA8). decomp: PLG134.BIN
// vs_effect_wyrmScorn_0 (src/EFFECT/trailEffect.h:713; was func_800FB4B8).
// decomp: PLG137.BIN vs_effect_cherryRonde_1 (src/EFFECT/trailEffect.h:713; was
// func_800FBFDC). decomp: PLG141.BIN vs_effect_swallowSlash_0
// (src/EFFECT/trailEffect.h:713; was func_800FB4B8). decomp: PLG146.BIN
// vs_effect_blackNebula_4 (src/EFFECT/trailEffect.h:713; was func_800FF334).
// decomp: PLG148.BIN vs_effect_accursedUmbra_0 (src/EFFECT/trailEffect.h:713;
// was func_800FB4B8). decomp: PLG242.BIN vs_effect_plg242_2
// (src/EFFECT/trailEffect.h:713; was func_800FCF18). decomp: PLG255.BIN
// vs_effect_plg255_0 (src/EFFECT/trailEffect.h:713; was func_800FB5B0). decomp:
// PLG265.BIN vs_effect_plg265_2 (src/EFFECT/trailEffect.h:713; was
// func_800FDBAC). decomp: PLG266.BIN vs_effect_plg266_2
// (src/EFFECT/trailEffect.h:713; was func_800FE958). decomp: PLG291.BIN
// vs_effect_plg291_0 (src/EFFECT/trailEffect.h:713; was func_800FB4D0). decomp:
// EFFECT VS_TRAIL_DISPATCH_FUNCTION (src/EFFECT/trailEffect.h:713). decomp:
// PLG049.BIN vs_spiritSurge_0 (src/EFFECT/trailEffect.h:713; was
// vs_spiritSurge_renderTrails). decomp: PLG050.BIN vs_effect_darkChant_1
// (src/EFFECT/trailEffect.h:713; was func_800FC0E4). decomp: PLG057.BIN
// vs_effect_thunderBurstLv1_1 (src/EFFECT/trailEffect.h:713; was
// func_800FD354). decomp: PLG069.BIN vs_effect_avalancheLv1_1
// (src/EFFECT/trailEffect.h:713; was func_800FBFCC). decomp: PLG135.BIN
// vs_effect_rendingGale_1 (src/EFFECT/trailEffect.h:713; was func_800FC42C).
// decomp: PLG161.BIN vs_effect_gigaTempest_2 (src/EFFECT/trailEffect.h:713; was
// func_800FCFB4). decomp: PLG165.BIN vs_effect_deathWail_0
// (src/EFFECT/trailEffect.h:713; was func_800FB4D8). decomp: PLG167.BIN
// vs_effect_lotusPalm_2 (src/EFFECT/trailEffect.h:713; was func_800FD2E4).
// decomp: PLG168.BIN vs_effect_vertigo_0 (src/EFFECT/trailEffect.h:713; was
// func_800FB5D0). decomp: PLG169.BIN vs_effect_vermillionAura_0
// (src/EFFECT/trailEffect.h:713; was func_800FB4C0). decomp: PLG232.BIN
// vs_effect_plg232_0 (src/EFFECT/trailEffect.h:713; was func_800FB4C0). The
// nine-sample lifecycle differs only in what the host allocates: at most eight
// trails and nine transparency samples (TrailEmitterLive.cpp).
bool UpdateTrailEmitter(TrailEmitterCommand command, std::int32_t packed,
                        TrailEmitterHost& host) {
  switch (command) {
    case TrailEmitterCommand::kCreate:
      host.Create(PlanTrailEmitter(packed));
      return true;
    case TrailEmitterCommand::kTick: {
      if (!host.HasState()) return false;
      if (!host.BeginTick()) {
        host.ReleaseTrails();
        return false;
      }
      if (!host.Counters().initialized) {
        host.InitializeTrails();
        TrailEmitterCounters initialized = host.Counters();
        initialized.initialized = true;
        host.StoreCounters(initialized);
      }
      host.DrawTrails();
      TrailEmitterCounters counters = host.Counters();
      counters.curve_sample_distance =
          WrapAdd(counters.curve_sample_distance, 1);
      bool alive = true;
      if (counters.lifetime != 0) {
        --counters.lifetime;
        alive = counters.lifetime != 0;
      }
      host.StoreCounters(counters);
      if (!alive) host.ReleaseTrails();
      return alive;
    }
    case TrailEmitterCommand::kExpire:
      if (host.HasState()) {
        TrailEmitterCounters counters = host.Counters();
        counters.lifetime = kStoppedLifetime;
        host.StoreCounters(counters);
      }
      return true;
    case TrailEmitterCommand::kStop:
      if (host.HasState()) host.ReleaseTrails();
      return false;
  }
  return true;
}
}  // namespace VS::Effects
