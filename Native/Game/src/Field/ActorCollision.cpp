// Hand-maintained (tools/port/owned.csv).
#include "Field/ActorCollision.hpp"  // NOLINT(build/include): typed API

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/CollisionMap.hpp"
#include "Field/PlatformPlacement.hpp"

namespace VS::Field {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingNegate;
using Core::Math::WrappingSubtract;
using Core::Math::WrapSigned16;

constexpr std::int32_t kFixedOne = 4096;
constexpr std::int32_t kSectorTurn = 512;  // an eighth of a turn
constexpr std::int32_t kQuarterTurn = 1024;
constexpr auto kSectors = static_cast<std::int32_t>(kCollisionSectorCount);
constexpr std::int32_t kSectorMask = kSectors - 1;
constexpr int kFacingSectorShift = 9;
// The sector facing each direction; sector n faces n * 512.
constexpr std::int32_t kSectorNegZ = 0;
constexpr std::int32_t kSectorNegXNegZ = 1;
constexpr std::int32_t kSectorNegX = 2;
constexpr std::int32_t kSectorNegXPosZ = 3;
constexpr std::int32_t kSectorPosZ = 4;
constexpr std::int32_t kSectorPosXPosZ = 5;
constexpr std::int32_t kSectorPosX = 6;
constexpr std::int32_t kSectorPosXNegZ = 7;
constexpr std::int32_t kTurnOpposite = 4;
constexpr std::int32_t kTurnCorner = 3;

constexpr std::size_t kFirstRoomActor = 2;
constexpr std::uint8_t kHangingLocomotion = 2;
constexpr std::uint8_t kFallingMovement = 3;
constexpr std::uint8_t kRoutedPoseLimit = 3;
constexpr std::uint16_t kHiddenModelId = 0x7F;
constexpr std::uint8_t kDetachedBone = 0xFC;
constexpr std::uint8_t kIntangibleReaction = 2;
constexpr std::int32_t kOccupantHeightReach = 192;
constexpr std::uint32_t kTileByteMask = 0xFF;
constexpr std::int32_t kOccupancyFindMode = 1;
constexpr std::int32_t kPosedOccupancyReach = 128;

constexpr std::int32_t kPlatformObstructionReach = 64;
constexpr std::int32_t kPlatformObstructionHeight = 288;
constexpr std::int16_t kDropFallSpeed = 3;
constexpr std::int32_t kPlatformDropContact = 2;

constexpr std::int32_t kLedgeClimbHeight = 236;
constexpr std::int32_t kLedgeMaxClimb = 5;
constexpr std::int32_t kLedgeSupportDrop = 64;

constexpr std::int32_t kTileSize = 128;
constexpr std::uint32_t kSurfaceAttributeShift = 5;
constexpr std::uint32_t kBlockedCellShift = 17;
constexpr std::uint32_t kSurfaceIdMask = 0x1F;
// The surface that casts no shadow on a blocked cell.
constexpr std::uint8_t kShadowlessSurfaceId = 0x14;
constexpr std::int32_t kShadowDropLimit = 64;

constexpr std::int32_t kWallRisingMargin = 32;
constexpr std::int32_t kWallStillMargin = 64;
constexpr std::int32_t kWallMovingMargin = 96;
constexpr std::int32_t kTurnDropDeepest = 255;
constexpr std::int32_t kTurnDropShallowest = 192;
constexpr std::int32_t kTurnFirstFarSector = 2;
constexpr std::int32_t kTurnLastFarSector = 6;
constexpr std::size_t kSampleByteBits = 8;
constexpr std::uint32_t kSampleByteMask = 0xFF;
constexpr std::int32_t kWallPush = 16;
constexpr std::int32_t kWallPushDiagonal = 8;

constexpr std::int32_t kSearchFloor = -0xC0;
constexpr std::int32_t kPosedSearchFloor = -0x140;
constexpr std::int32_t kArmedSearchCeiling = -0x40;
constexpr std::int32_t kSearchCeiling = -0x100;
constexpr std::int32_t kMaxProbeDrop = 0x17F;
constexpr std::int32_t kClimbStep = 64;
constexpr std::int32_t kClimbStepMask = kClimbStep - 1;
constexpr std::int32_t kSteppableClimb = 2;
constexpr std::int32_t kDiagonalClimbLimit = 4;
constexpr std::size_t kAxisProbeCount = 5;

constexpr std::int32_t kPlatformLedgeEdge = 65;
constexpr std::int32_t kTileLedgeEdge = 127;
constexpr std::int32_t kTileOffsetMask = 127;
constexpr std::int32_t kHalfTileWrap = 32;
constexpr std::int32_t kHalfTileStep = 64;
constexpr std::int32_t kGimmickLedgeLift = 12;
constexpr std::int32_t kLedgeLift = 8;

using AxisResults = std::array<std::int32_t, kAxisProbeCount>;

std::int16_t Halfword(std::int32_t value) {
  return WrapSigned16(static_cast<std::uint32_t>(value));
}

// The original's `value << 17 >> 17`: the low 15 bits, sign-extended.
std::int32_t SignExtend15(std::int32_t value) {
  constexpr int kShift = 17;
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value)
                                   << kShift) >>
         kShift;
}

std::size_t SectorIndex(std::int32_t sector) {
  return static_cast<std::size_t>(sector & kSectorMask);
}

std::uint32_t CellWordOrAbsent(const ActorCollisionHost& host,
                               std::optional<CollisionCellRef> cell) {
  if (!cell) return kAbsentCellWord;
  return host.CellWord(*cell).value_or(kAbsentCellWord);
}

std::uint8_t SurfaceAttribute(std::uint32_t word) {
  return static_cast<std::uint8_t>((word >> kSurfaceAttributeShift) & 1U);
}

bool IsBlockedCell(std::uint32_t word) {
  return ((word >> kBlockedCellShift) & 1U) != 0;
}

std::int32_t WrappedSquare(std::int32_t value) {
  return WrappingMultiply(value, value);
}

MotionVector AddXyz(MotionVector position, const MotionVector& offset) {
  position.x = Halfword(position.x + offset.x);
  position.y = Halfword(position.y + offset.y);
  position.z = Halfword(position.z + offset.z);
  return position;
}

// Actors the overlap tests pass through.
bool IsIntangible(const CollisionActor& actor) {
  return actor.skipped || actor.animation_locked ||
         actor.attach_bone == kDetachedBone ||
         actor.reaction_state == kIntangibleReaction;
}

// Actors whose occupancy is their route target while they move or react.
bool IsRouting(const CollisionActor& actor) {
  return (actor.pose_state != 0 && actor.pose_state < kRoutedPoseLimit) ||
         actor.movement_mode != 0 || actor.reaction_state != 0;
}

// The climb in 64-unit steps up to a probe `rise` below the sample (negative
// rises climb); 0 for level ground or a drop.
std::int32_t ClimbSteps(std::int32_t rise) {
  std::int32_t steps = rise / kClimbStep;
  if (steps >= 0) return 0;
  if ((rise & kClimbStepMask) != 0) --steps;
  return -steps;
}

// The climb the side probes may need beside an accepted probe `rise` below
// the actor: at least one step.
std::int32_t ClimbLimit(std::int32_t rise) {
  std::int32_t steps = rise / kClimbStep;
  if ((rise & kClimbStepMask) != 0) --steps;
  steps = -steps;
  return steps <= 0 ? 1 : steps;
}

bool WithinClimb(const AxisResults& results, std::int32_t limit) {
  return results.front() != kRefusedProbe &&
         std::all_of(results.begin() + 1, results.end(),
                     [limit](std::int32_t climb) { return climb <= limit; });
}

bool Steppable(const AxisResults& results) {
  return results.front() != kRefusedProbe &&
         std::all_of(
             results.begin() + 1, results.end(),
             [](std::int32_t climb) { return climb < kSteppableClimb; });
}

void DropHangingActor(ActorCollisionHost& host, CollisionActor& actor) {
  actor.root_motion.x = 0;
  actor.root_motion.y = kDropFallSpeed;
  actor.root_motion.z = 0;
  actor.platform_offset_x = 0;
  actor.platform_offset_z = 0;
  actor.movement_mode = kFallingMovement;
  actor.locomotion_state = 0;
  actor.movement_pending = false;
  actor.support_layer = 0;
  host.StoreActor(actor);
  host.UpdatePlatformContact(actor.table_index, kPlatformDropContact);
}

// Whether a platform moving to `point` reaches into the actor's column.
bool PlatformReachesActor(const CollisionActor& actor,
                          const MotionVector& point) {
  const std::int32_t radius = actor.collision_radius;
  const std::int32_t x = point.x;
  const std::int32_t z = point.z;
  if (x + kPlatformObstructionReach < actor.position.x - radius ||
      x - kPlatformObstructionReach > actor.position.x + radius) {
    return false;
  }
  if (z + kPlatformObstructionReach < actor.position.z - radius ||
      z - kPlatformObstructionReach > actor.position.z + radius) {
    return false;
  }
  const std::int32_t y = actor.position.y;
  return point.y >= y - kPlatformObstructionHeight && point.y < y;
}

// The original clears the byte past the eight blocked flags for facings at
// or past sector 7: those bytes are the attribute flags and centre attribute,
// which every perimeter sample rewrites before reading, so they are skipped.
void ClearBlockedSector(CollisionSamples& samples, std::int32_t sector) {
  constexpr std::int32_t kLastSampleByte = 2 * kSectors;
  if (sector < kSectors) {
    samples.blocked.at(static_cast<std::size_t>(sector)) = 0;
  } else if (sector > kLastSampleByte) {
    VS_LOG_WARN("field.collision",
                "Hanging facing sector {} outside the sample block skipped",
                sector);
  }
}

bool HasLedgeSupportBothSides(ActorCollisionHost& host, std::int32_t side_x,
                              std::int32_t side_z) {
  const MotionVector origin = host.Probe().position;
  if (SampleActorSupportHeight(host, origin, side_x, side_z,
                               SupportSample::kInterpolated) -
          origin.y <=
      -kLedgeSupportDrop) {
    return false;
  }
  return SampleActorSupportHeight(host, origin, WrappingNegate(side_x),
                                  WrappingNegate(side_z),
                                  SupportSample::kInterpolated) -
             origin.y >
         -kLedgeSupportDrop;
}

// The ledge sides at the reach of a radius across the last probe's sector.
MotionVector LedgeSideOffset(ActorCollisionHost& host, std::int32_t radius,
                             MotionVector offset) {
  const std::int16_t reach = Halfword(CollisionDiagonalReach(radius));
  if (((host.Probe().direction >> 1) & 1U) != 0) {
    offset.z = reach;
  } else {
    offset.x = reach;
  }
  return offset;
}

std::int32_t SupportAboveFloor(ActorCollisionHost& host,
                               const MotionVector& origin, PlatformProbe at,
                               CollisionCellRef cell, std::int32_t height,
                               SupportSample sample) {
  const std::int32_t tile_x = at.x / kTileSize;
  const std::int32_t tile_z = at.z / kTileSize;
  const std::int32_t gimmick =
      sample == SupportSample::kBelowReference
          ? host.GimmickSupportBelow(tile_x, tile_z, origin.y)
          : host.SupportingGimmick(tile_x, tile_z).height;
  if (gimmick != 0) height = gimmick;
  const std::uint32_t word = CellWordOrAbsent(host, cell);
  if (IsBlockedCell(word) && height >= 0) {
    height = kBlockedSupportHeight;
    if (sample == SupportSample::kShadow &&
        host.TileSurfaceId(word & kSurfaceIdMask) == kShadowlessSurfaceId) {
      height = 0;
    }
  }
  if (sample == SupportSample::kBelowReference ||
      sample == SupportSample::kNoPlatforms) {
    return height;
  }
  const std::int32_t platform = host.PlatformContactAt(at).height;
  const std::int32_t floor = height;
  if (platform != 0 && platform < height) height = platform;
  if (sample == SupportSample::kShadow &&
      height < origin.y - kShadowDropLimit) {
    height = floor;
  }
  return height;
}

struct PerimeterSample {
  std::int32_t sector = 0;
  std::int32_t x = 0;
  std::int32_t z = 0;
};

// The eight sectors in the original's sampling order: the probe cell each
// sample leaves behind is the one its attribute reads.
void SamplePerimeterSectors(ActorCollisionHost& host,
                            const CollisionActor& actor,
                            std::int32_t diagonal) {
  CollisionSamples& samples = host.Samples();
  const std::int32_t radius = actor.collision_radius;
  const std::int32_t back = WrappingNegate(diagonal);
  const std::array<PerimeterSample, kCollisionSectorCount> order{{
      {.sector = kSectorNegZ, .x = 0, .z = -radius},
      {.sector = kSectorPosX, .x = radius, .z = 0},
      {.sector = kSectorNegX, .x = -radius, .z = 0},
      {.sector = kSectorPosXNegZ, .x = diagonal, .z = back},
      {.sector = kSectorNegXNegZ, .x = back, .z = back},
      {.sector = kSectorNegXPosZ, .x = back, .z = diagonal},
      {.sector = kSectorPosXPosZ, .x = diagonal, .z = diagonal},
      {.sector = kSectorPosZ, .x = 0, .z = radius},
  }};
  samples.center_attribute = SurfaceAttribute(CellWordOrAbsent(
      host, host.CellAt(actor.position.x, actor.position.z, 0)));
  for (const PerimeterSample& sample : order) {
    const std::size_t index = SectorIndex(sample.sector);
    samples.heights.at(index) = Halfword(SampleActorSupportHeight(
        host, actor.position, sample.x, sample.z, SupportSample::kFloor));
    samples.attributes.at(index) =
        SurfaceAttribute(CellWordOrAbsent(host, host.Probe().cell));
  }
}

void MarkPerimeterWalls(CollisionSamples& samples, CollisionActor& actor) {
  const std::int32_t y = actor.position.y;
  std::int32_t threshold = y - kWallMovingMargin;
  if (actor.movement_mode == kFallingMovement && actor.root_motion.y > 0) {
    threshold = y - kWallRisingMargin;
  } else if (actor.turn_distance == 0) {
    threshold = y - kWallStillMargin;
  }
  for (std::size_t sector = 0; sector < kCollisionSectorCount; ++sector) {
    std::int16_t& height = samples.heights.at(sector);
    if (height > y) height = actor.position.y;
    if ((samples.attributes.at(sector) == 0 || samples.center_attribute == 0) &&
        threshold >= height) {
      samples.blocked.at(sector) = 1;
      actor.wall_contact = true;
    }
  }
}

void WriteSampleByte(CollisionSamples& samples, std::size_t byte,
                     std::uint8_t value) {
  if (byte < kCollisionSectorCount) {
    samples.blocked.at(byte) = value;
  } else {
    samples.attributes.at(byte - kCollisionSectorCount) = value;
  }
}

// The original stores the height through a halfword index past the eight
// heights: sectors 8..15 land (little-endian) on the blocked and attribute
// bytes that follow them, which the wall rules then read. Sectors outside
// 0..15 would land on other scratchpad state and are refused.
void StoreTurnSectorHeight(CollisionSamples& samples, std::int32_t sector,
                           std::int16_t height) {
  if (sector >= 0 && sector < kSectors) {
    samples.heights.at(static_cast<std::size_t>(sector)) = height;
    return;
  }
  if (sector < kSectors || sector >= 2 * kSectors) {
    VS_LOG_WARN("field.collision",
                "Turn sector {} outside the sample block refused", sector);
    return;
  }
  const auto bits = static_cast<std::uint16_t>(height);
  const std::size_t byte = static_cast<std::size_t>(sector - kSectors) * 2;
  WriteSampleByte(samples, byte,
                  static_cast<std::uint8_t>(bits & kSampleByteMask));
  WriteSampleByte(samples, byte + 1,
                  static_cast<std::uint8_t>(bits >> kSampleByteBits));
}

// A turning actor on a ledge between 192 and 255 below it keeps the turn
// sector open when the far sectors are not as deep.
void OpenTurnSector(CollisionSamples& samples, CollisionActor& actor) {
  const std::int32_t y = actor.position.y;
  const std::int32_t sector =
      (actor.turn_direction + kSectorTurn / 2) / kSectorTurn;
  for (std::int32_t step = -1; step <= 1; ++step) {
    const std::int32_t delta =
        RelativeCollisionSampleHeight(samples, sector, step, y);
    if (delta < -kTurnDropDeepest || delta >= -kTurnDropShallowest) return;
  }
  for (std::int32_t step = kTurnFirstFarSector; step <= kTurnLastFarSector;
       ++step) {
    if (RelativeCollisionSampleHeight(samples, sector, step, y) <
        -kTurnDropShallowest) {
      return;
    }
  }
  StoreTurnSectorHeight(samples, sector, actor.position.y);
  actor.wall_contact = false;
}

struct WallPush {
  std::int32_t x = 0;
  std::int32_t z = 0;
};

// The push away from each blocked sector.
constexpr std::array<WallPush, kCollisionSectorCount> kWallPushes{{
    {.x = 0, .z = kWallPush},
    {.x = kWallPushDiagonal, .z = kWallPushDiagonal},
    {.x = kWallPush, .z = 0},
    {.x = kWallPushDiagonal, .z = -kWallPushDiagonal},
    {.x = 0, .z = -kWallPush},
    {.x = -kWallPushDiagonal, .z = -kWallPushDiagonal},
    {.x = -kWallPush, .z = 0},
    {.x = -kWallPushDiagonal, .z = kWallPushDiagonal},
}};

// A push against the motion stops that axis of it.
std::int16_t PushedAxis(std::int16_t motion, std::int32_t push) {
  if ((push < 0 && motion > 0) || (push > 0 && motion < 0)) motion = 0;
  return Halfword(motion + push);
}

struct SlideAxes {
  // The axis the blocked motion runs along and the axis the slide crosses.
  std::int16_t MotionVector::* primary = nullptr;
  std::int16_t MotionVector::* side = nullptr;
  std::int32_t side_sign = 1;
  std::int32_t positive_sector = 0;
  std::int32_t negative_sector = 0;
  // The corner probe's side offset is the half step on X slides and the
  // cross step on Z slides, as in the original.
  bool corner_steps_half = false;
  TerrainMotion partial = TerrainMotion::kBlocked;
};

constexpr SlideAxes kXSlide{
    .primary = &MotionVector::x,
    .side = &MotionVector::z,
    .side_sign = 1,
    .positive_sector = kSectorPosX,
    .negative_sector = kSectorNegX,
    .corner_steps_half = true,
    .partial = TerrainMotion::kAlongZ,
};
constexpr SlideAxes kZSlide{
    .primary = &MotionVector::z,
    .side = &MotionVector::x,
    .side_sign = -1,
    .positive_sector = kSectorPosZ,
    .negative_sector = kSectorNegZ,
    .corner_steps_half = false,
    .partial = TerrainMotion::kAlongX,
};

struct SlideReach {
  std::int32_t origin_primary = 0;
  std::int32_t origin_side = 0;
  std::int32_t offset = 0;
  std::int32_t diagonal = 0;
  std::int32_t half = 0;
};

struct SlideStep {
  std::int32_t side = 0;
  std::int32_t cross = 0;
  std::int32_t cross_diagonal = 0;
  std::int32_t turn = 0;
  std::int32_t corner = 0;
};

void SetProbe(MotionVector& probe, const SlideAxes& axes, std::int32_t primary,
              std::int32_t side) {
  probe.*axes.primary = Halfword(primary);
  probe.*axes.side = Halfword(side);
}

std::int32_t DiagonalSector(bool negative_x, bool negative_z) {
  if (negative_x) return negative_z ? kSectorNegXNegZ : kSectorNegXPosZ;
  return negative_z ? kSectorPosXNegZ : kSectorPosXPosZ;
}

TerrainMotionResult Blocked() {
  return {.motion = TerrainMotion::kBlocked, .climb = std::nullopt};
}

// decomp: BATTLE.PRG func_800A76BC (src/BATTLE/BATTLE.PRG/3A1A0.c:2443).
// One probe and its opposite serve every test, as in the original: a probe
// the rules refuse keeps the height and support word of the last one taken.
class TerrainMotionPass {
 public:
  TerrainMotionPass(ActorCollisionHost& host, CollisionActor& actor,
                    MotionVector& motion);

  TerrainMotionResult ProbeOnly();
  TerrainMotionResult Resolve();

 private:
  std::int32_t Classify(MotionVector& probe, std::int32_t sector) {
    return ClassifyActorTerrainProbe(host_, actor_, probe, sector);
  }
  std::optional<TerrainMotionResult> TryDiagonal();
  void ProbeX();
  void ProbeZ();
  TerrainMotionResult ChooseAlongX();
  TerrainMotionResult ChooseAlongZ();
  TerrainMotionResult AcceptX();
  TerrainMotionResult AcceptZ();
  [[nodiscard]] bool FacesAgainst(std::int32_t motion_sector) const;
  TerrainMotionResult Slide(const SlideAxes& axes);
  std::optional<bool> SlideTurnsBack(const SlideAxes& axes,
                                     const SlideReach& reach,
                                     std::int32_t sector,
                                     const std::array<std::int32_t, 2>& first);
  TerrainMotionResult SlideAlong(const SlideAxes& axes, const SlideReach& reach,
                                 std::int32_t sector, const SlideStep& step);
  TerrainMotionResult Moved(std::int32_t sector, TerrainMotion motion,
                            std::int32_t climb);

  ActorCollisionHost& host_;
  CollisionActor& actor_;
  MotionVector& motion_;
  CollisionSamples& samples_;
  CollisionProbe& last_probe_;
  std::int32_t radius_;
  std::int32_t diagonal_;
  AxisResults x_results_{};
  AxisResults z_results_{};
  std::uint8_t x_direction_ = 0;
  std::uint8_t z_direction_ = 0;
  std::optional<CollisionCellRef> x_cell_;
  std::optional<CollisionCellRef> z_cell_;
  MotionVector probe_{};
  MotionVector opposite_{};
  MotionVector x_probe_{};
  MotionVector z_probe_{};
};

TerrainMotionPass::TerrainMotionPass(ActorCollisionHost& host,
                                     CollisionActor& actor,
                                     MotionVector& motion)
    : host_(host),
      actor_(actor),
      motion_(motion),
      samples_(host.Samples()),
      last_probe_(host.Probe()),
      radius_(actor.collision_radius),
      diagonal_(CollisionDiagonalReach(actor.collision_radius)) {
  actor_.was_on_ledge = actor_.on_ledge;
  actor_.on_ledge = false;
  samples_.search_floor = kSearchFloor;
  if (actor_.weapon_drawn) {
    samples_.search_ceiling = kArmedSearchCeiling;
  } else if (actor_.action_pose_active) {
    samples_.search_floor = kPosedSearchFloor;
    samples_.search_ceiling = kArmedSearchCeiling;
  } else {
    samples_.search_ceiling = kSearchCeiling;
  }
}

TerrainMotionResult TerrainMotionPass::ProbeOnly() {
  samples_.search_ceiling = kSearchCeiling;
  actor_.bypass_terrain_collision = false;
  actor_.wall_contact = false;
  // On a platform the original keeps an uninitialised local as the probe
  // cell; no cell is kept here.
  std::optional<CollisionCellRef> start_cell;
  if (actor_.support_layer == 0) {
    start_cell =
        host_.CellAt(actor_.position.x, actor_.position.z, actor_.tile_layer);
    if (!start_cell) actor_.bypass_terrain_collision = true;
  }
  probe_.x = Halfword(actor_.position.x + motion_.x);
  probe_.z = Halfword(actor_.position.z + motion_.z);
  samples_.heights.front() = actor_.position.y;
  samples_.search_floor = 0;
  const std::int32_t climb = Classify(probe_, kSectorNegZ);
  last_probe_.position = probe_;
  if (climb == kRefusedProbe) {
    return {.motion = TerrainMotion::kBlocked, .climb = 0};
  }
  last_probe_.cell = start_cell;
  return {.motion = TerrainMotion::kAccepted, .climb = std::min(climb, 1)};
}

TerrainMotionResult TerrainMotionPass::Resolve() {
  if (!CanActorSeparateFromOverlaps(host_, actor_, motion_)) return Blocked();
  SampleActorCollisionPerimeter(host_, actor_, diagonal_);
  ApplyActorWallRepulsion(samples_, actor_, motion_);
  actor_.bypass_terrain_collision = false;
  if (actor_.support_layer == 0) {
    actor_.bypass_terrain_collision =
        !host_.CellAt(actor_.position.x, actor_.position.z, actor_.tile_layer)
             .has_value();
  }
  if (motion_.x == 0) {
    x_results_.front() = kRefusedProbe;
    ProbeZ();
  } else if (motion_.z == 0) {
    ProbeX();
    z_results_.front() = kRefusedProbe;
  } else {
    if (const auto accepted = TryDiagonal()) return *accepted;
    ProbeX();
    ProbeZ();
  }
  if (std::abs(motion_.x) >= std::abs(motion_.z)) return ChooseAlongX();
  return ChooseAlongZ();
}

std::optional<TerrainMotionResult> TerrainMotionPass::TryDiagonal() {
  const bool negative_x = motion_.x < 0;
  const bool negative_z = motion_.z < 0;
  const std::int32_t offset = negative_x ? -radius_ : radius_;
  const std::int32_t diagonal = negative_x ? -diagonal_ : diagonal_;
  const std::int32_t cross = negative_z ? -radius_ : radius_;
  const std::int32_t cross_diagonal = negative_z ? -diagonal_ : diagonal_;
  std::int32_t sector = DiagonalSector(negative_x, negative_z);
  probe_.x = Halfword(actor_.position.x + motion_.x + diagonal);
  probe_.z = Halfword(actor_.position.z + motion_.z + cross_diagonal);
  const std::int32_t ahead = Classify(probe_, sector);
  last_probe_.position = probe_;
  const std::optional<CollisionCellRef> ahead_cell = last_probe_.cell;
  probe_.x = Halfword(probe_.x - diagonal * 2);
  sector -= 2;
  const std::int32_t behind_x = Classify(probe_, sector);
  probe_.x = Halfword(probe_.x + diagonal * 2);
  probe_.z = Halfword(probe_.z - cross_diagonal * 2);
  sector += kTurnOpposite;
  const std::int32_t behind_z = Classify(probe_, sector);
  probe_.x = Halfword(probe_.x - diagonal);
  probe_.z = Halfword(probe_.z + cross_diagonal + cross);
  sector -= kTurnCorner;
  const std::int32_t side_z = Classify(probe_, sector);
  probe_.x = Halfword(probe_.x + offset);
  probe_.z = Halfword(probe_.z - cross);
  sector += 2;
  const std::int32_t side_x = Classify(probe_, sector);
  const AxisResults results{ahead, behind_x, behind_z, side_z, side_x};
  const std::int32_t limit =
      ClimbLimit(last_probe_.position.y - actor_.position.y);
  if (!WithinClimb(results, limit) ||
      (ahead >= kDiagonalClimbLimit && (sector & 1) == 0)) {
    return std::nullopt;
  }
  last_probe_.cell = ahead_cell;
  last_probe_.direction = static_cast<std::uint8_t>((sector - 1) & kSectorMask);
  return TerrainMotionResult{.motion = TerrainMotion::kAccepted,
                             .climb = ahead};
}

void TerrainMotionPass::ProbeX() {
  const bool negative = motion_.x < 0;
  const std::int32_t offset = negative ? -radius_ : radius_;
  const std::int32_t diagonal = negative ? -diagonal_ : diagonal_;
  std::int32_t sector = negative ? kSectorNegX : kSectorPosX;
  probe_.x = Halfword(actor_.position.x + motion_.x + offset);
  probe_.z = actor_.position.z;
  const std::int32_t ahead = Classify(probe_, sector);
  x_probe_ = probe_;
  x_direction_ = static_cast<std::uint8_t>(sector);
  sector -= 2;
  x_cell_ = last_probe_.cell;
  probe_.x = Halfword(probe_.x - offset);
  probe_.z = Halfword(probe_.z + offset);
  const std::int32_t first_side = Classify(probe_, sector);
  sector += kTurnOpposite;
  probe_.z = Halfword(probe_.z - offset * 2);
  const std::int32_t second_side = Classify(probe_, sector);
  sector -= kTurnCorner;
  probe_.x = Halfword(probe_.x + diagonal);
  probe_.z = Halfword(probe_.z + offset + diagonal);
  const std::int32_t first_corner = Classify(probe_, sector);
  probe_.z = Halfword(probe_.z - diagonal * 2);
  const std::int32_t second_corner = Classify(probe_, sector + 2);
  x_results_ = {ahead, first_side, second_side, first_corner, second_corner};
}

void TerrainMotionPass::ProbeZ() {
  const bool negative = motion_.z < 0;
  const std::int32_t offset = negative ? -radius_ : radius_;
  const std::int32_t diagonal = negative ? -diagonal_ : diagonal_;
  std::int32_t sector = negative ? kSectorNegZ : kSectorPosZ;
  probe_.x = actor_.position.x;
  probe_.z = Halfword(actor_.position.z + motion_.z + offset);
  const std::int32_t ahead = Classify(probe_, sector);
  z_probe_ = probe_;
  z_direction_ = static_cast<std::uint8_t>(sector);
  sector -= 2;
  z_cell_ = last_probe_.cell;
  probe_.x = Halfword(probe_.x - offset);
  probe_.z = Halfword(probe_.z - offset);
  const std::int32_t first_side = Classify(probe_, sector);
  sector += kTurnOpposite;
  probe_.x = Halfword(probe_.x + offset * 2);
  const std::int32_t second_side = Classify(probe_, sector);
  sector -= kTurnCorner;
  probe_.x = Halfword(probe_.x - (offset + diagonal));
  probe_.z = Halfword(probe_.z + diagonal);
  const std::int32_t first_corner = Classify(probe_, sector);
  probe_.x = Halfword(probe_.x + diagonal * 2);
  const std::int32_t second_corner = Classify(probe_, sector + 2);
  z_results_ = {ahead, first_side, second_side, first_corner, second_corner};
}

// Whether the actor was on a ledge with its terrain facing against the
// motion along an axis (sector 6 for -X motion, 0 for +Z motion).
bool TerrainMotionPass::FacesAgainst(std::int32_t motion_sector) const {
  return actor_.was_on_ledge &&
         actor_.terrain_facing ==
             ((motion_sector + kTurnOpposite) & kSectorMask);
}

TerrainMotionResult TerrainMotionPass::ChooseAlongX() {
  const std::int32_t limit = ClimbLimit(x_probe_.y - actor_.position.y);
  if (WithinClimb(x_results_, limit)) {
    if (FacesAgainst(motion_.x < 0 ? kSectorNegX : kSectorPosX)) {
      return Slide(kXSlide);
    }
    return AcceptX();
  }
  if (Steppable(z_results_)) {
    if (FacesAgainst(motion_.z < 0 ? kSectorNegZ : kSectorPosZ)) {
      return Slide(kXSlide);
    }
    return AcceptZ();
  }
  return Slide(kXSlide);
}

TerrainMotionResult TerrainMotionPass::ChooseAlongZ() {
  const std::int32_t limit = ClimbLimit(z_probe_.y - actor_.position.y);
  if (WithinClimb(z_results_, limit)) {
    if (FacesAgainst(motion_.z < 0 ? kSectorNegZ : kSectorPosZ)) {
      return Slide(kZSlide);
    }
    return AcceptZ();
  }
  if (Steppable(x_results_)) {
    if (FacesAgainst(motion_.x < 0 ? kSectorNegX : kSectorPosX)) {
      return Slide(kZSlide);
    }
    return AcceptX();
  }
  return Slide(kZSlide);
}

TerrainMotionResult TerrainMotionPass::AcceptX() {
  motion_.z = 0;
  last_probe_.position = x_probe_;
  last_probe_.cell = x_cell_;
  last_probe_.direction = x_direction_;
  return {.motion = TerrainMotion::kAlongX, .climb = x_results_.front()};
}

TerrainMotionResult TerrainMotionPass::AcceptZ() {
  motion_.x = 0;
  last_probe_.position = z_probe_;
  last_probe_.cell = z_cell_;
  last_probe_.direction = z_direction_;
  return {.motion = TerrainMotion::kAlongZ, .climb = z_results_.front()};
}

// Slides along a ledge the motion runs into: probes both sides one radius
// ahead, then steps half the motion along the side the ledge allows.
TerrainMotionResult TerrainMotionPass::Slide(const SlideAxes& axes) {
  if (actor_.movement_mode != 0) return Blocked();
  const std::int32_t moving = motion_.*axes.primary;
  const bool negative = moving < 0;
  const SlideReach reach{.origin_primary = actor_.position.*axes.primary,
                         .origin_side = actor_.position.*axes.side,
                         .offset = negative ? -radius_ : radius_,
                         .diagonal = negative ? -diagonal_ : diagonal_,
                         .half = moving / 2};
  const std::int32_t side = axes.side_sign * reach.offset;
  std::int32_t sector =
      (negative ? axes.negative_sector : axes.positive_sector) - 1;
  SetProbe(probe_, axes, reach.origin_primary + moving + reach.offset,
           reach.origin_side + side);
  const std::int32_t ahead = Classify(probe_, sector);
  sector += 2;
  SetProbe(opposite_, axes, probe_.*axes.primary, reach.origin_side - side);
  const std::int32_t ahead_opposite = Classify(opposite_, sector);
  if (ahead == kRefusedProbe && ahead_opposite == kRefusedProbe) {
    return Blocked();
  }
  bool back = true;
  if (ahead < kSteppableClimb) {
    back = false;
    if (ahead_opposite < kSteppableClimb) {
      const auto turns_back =
          SlideTurnsBack(axes, reach, sector, {ahead, ahead_opposite});
      if (!turns_back) return Blocked();
      back = *turns_back;
    }
  } else if (ahead_opposite >= kSteppableClimb) {
    return Blocked();
  }
  const std::int32_t cross = axes.side_sign * reach.half;
  const std::int32_t cross_diagonal = axes.side_sign * reach.diagonal;
  if (back) {
    return SlideAlong(axes, reach, sector - 2,
                      {.side = -side,
                       .cross = -cross,
                       .cross_diagonal = cross_diagonal,
                       .turn = -1,
                       .corner = kTurnCorner});
  }
  return SlideAlong(axes, reach, sector,
                    {.side = side,
                     .cross = cross,
                     .cross_diagonal = -cross_diagonal,
                     .turn = 1,
                     .corner = -kTurnCorner});
}

// Whether a slide with both sides open ahead turns back along the ledge;
// nullopt when neither side of the step is passable.
std::optional<bool> TerrainMotionPass::SlideTurnsBack(
    const SlideAxes& axes, const SlideReach& reach, std::int32_t sector,
    const std::array<std::int32_t, 2>& first) {
  const std::int32_t side = axes.side_sign * reach.offset;
  const std::int32_t moving = motion_.*axes.primary;
  SetProbe(probe_, axes, reach.origin_primary + moving,
           reach.origin_side + side);
  const std::int32_t step = Classify(probe_, sector - 2);
  SetProbe(opposite_, axes, probe_.*axes.primary, reach.origin_side - side);
  const std::int32_t step_opposite = Classify(opposite_, sector);
  if (step == kRefusedProbe && step_opposite == kRefusedProbe) {
    return std::nullopt;
  }
  if (step > 0) return true;
  const auto [ahead, ahead_opposite] = first;
  return step_opposite <= 0 && (ahead != 0 || ahead_opposite != 1) &&
         ((ahead == 1 && ahead_opposite == 1) || probe_.y <= opposite_.y);
}

// The original's limit checks against the search ceiling here never refuse:
// the probe results are never negative.
TerrainMotionResult TerrainMotionPass::SlideAlong(const SlideAxes& axes,
                                                  const SlideReach& reach,
                                                  std::int32_t sector,
                                                  const SlideStep& step) {
  sector -= step.turn;
  SetProbe(probe_, axes, reach.origin_primary + reach.half + reach.offset,
           reach.origin_side + step.cross);
  const std::int32_t edge = Classify(probe_, sector);
  sector += step.turn;
  SetProbe(probe_, axes, reach.origin_primary + reach.half + reach.diagonal,
           reach.origin_side + step.cross + step.cross_diagonal);
  const std::int32_t edge_diagonal = Classify(probe_, sector);
  sector += step.corner;
  const std::int32_t corner_side =
      axes.corner_steps_half ? reach.half : step.cross;
  SetProbe(probe_, axes, reach.origin_primary,
           reach.origin_side + corner_side + step.side);
  const std::int32_t corner = Classify(probe_, sector);
  last_probe_.position = probe_;
  if (corner >= kSteppableClimb) return Blocked();
  if (edge < kSteppableClimb && edge_diagonal < kSteppableClimb) {
    SetProbe(probe_, axes, reach.origin_primary + reach.half,
             reach.origin_side + step.cross + step.side);
    if (Classify(probe_, sector) >= kSteppableClimb) return Blocked();
    motion_.*axes.primary = Halfword(reach.half);
    motion_.*axes.side = Halfword(step.cross);
    return Moved(sector + step.turn, TerrainMotion::kAccepted, corner);
  }
  motion_.*axes.primary = 0;
  motion_.*axes.side = Halfword(step.cross);
  return Moved(sector, axes.partial, corner);
}

TerrainMotionResult TerrainMotionPass::Moved(std::int32_t sector,
                                             TerrainMotion motion,
                                             std::int32_t climb) {
  actor_.terrain_facing = static_cast<std::uint8_t>(sector & kSectorMask);
  actor_.on_ledge = true;
  return {.motion = motion, .climb = climb};
}

// The ledge edge on a platform: 65 past the platform's centre.
void PlatformLedgeOffset(const CollisionActor& actor,
                         const PlatformContact& platform,
                         std::int32_t direction, MotionVector& offset) {
  std::int32_t reach = actor.collision_radius;
  std::int32_t edge = kPlatformLedgeEdge;
  if (direction < 2) {
    reach = -reach;
  } else {
    edge = -kPlatformLedgeEdge;
  }
  if ((direction & 1) != 0) {
    offset.x = Halfword(edge + platform.x - (reach + actor.position.x));
  } else {
    offset.z = Halfword(edge + platform.z - (reach + actor.position.z));
  }
}

std::int16_t WrapHalfTile(std::int16_t offset) {
  if (offset > kHalfTileWrap) offset = Halfword(offset - kHalfTileStep);
  if (offset < -kHalfTileWrap) offset = Halfword(offset + kHalfTileStep);
  return offset;
}

// The ledge edge on the room's tiles: the near edge of the actor's tile.
void TileLedgeOffset(const CollisionActor& actor, std::int32_t direction,
                     MotionVector& offset) {
  std::int32_t reach = actor.collision_radius;
  std::int32_t edge = 0;
  if (direction < 2) {
    reach = -reach;
  } else {
    edge = kTileLedgeEdge;
  }
  if ((direction & 1) != 0) {
    offset.x = Halfword(edge - ((reach + actor.position.x) & kTileOffsetMask));
  } else {
    offset.z = Halfword(edge - ((reach + actor.position.z) & kTileOffsetMask));
  }
  offset.x = WrapHalfTile(offset.x);
  offset.z = WrapHalfTile(offset.z);
  if (actor.support_gimmick != 0) {
    offset.y = Halfword(offset.y + kGimmickLedgeLift);
  }
  offset.y = Halfword(offset.y + kLedgeLift);
}
}  // namespace

std::int32_t CollisionDiagonalReach(std::int32_t radius) {
  return WrappingMultiply(Core::Math::Sine(kSectorTurn), radius) / kFixedOne;
}

// decomp: BATTLE.PRG vs_battleRelativeSampleHeight
// (src/BATTLE/BATTLE.PRG/3A1A0.c:80)
std::int32_t RelativeCollisionSampleHeight(const CollisionSamples& samples,
                                           std::int32_t sector,
                                           std::int32_t offset,
                                           std::int32_t y) {
  return samples.heights.at(SectorIndex(WrappingAdd(sector, offset))) - y;
}

// decomp: BATTLE.PRG func_800A3C34 (src/BATTLE/BATTLE.PRG/3A1A0.c:666)
std::optional<std::size_t> FindActorOccupyingTile(
    const ActorCollisionHost& host, const OccupiedTileQuery& query) {
  const auto [tile_x, tile_z, minimum_height, mode] = query;
  const std::int32_t lowest =
      WrappingSubtract(minimum_height, kOccupantHeightReach);
  const std::size_t first = mode < 2 ? 0 : static_cast<std::size_t>(mode);
  for (std::size_t index = first; index < kCollisionActorSlots; ++index) {
    const std::optional<CollisionActor> actor = host.Actor(index);
    if (!actor || actor->skipped || actor->model_id == kHiddenModelId) {
      continue;
    }
    if (mode == kOccupancyFindMode && IsRouting(*actor)) {
      if (static_cast<std::int32_t>(actor->route_target.x & kTileByteMask) ==
              tile_x &&
          static_cast<std::int32_t>(actor->route_target.y & kTileByteMask) ==
              tile_z) {
        return index;
      }
      continue;
    }
    if (index == 0 && actor->support_gimmick != 0) {
      const std::optional<GimmickTile> gimmick =
          host.SupportGimmickTile(actor->support_gimmick);
      if (!gimmick) {
        VS_LOG_WARN("field.collision",
                    "Leader support gimmick slot {} is empty",
                    static_cast<int>(actor->support_gimmick));
        continue;
      }
      if (tile_x == gimmick->x && tile_z == gimmick->z) return index;
      continue;
    }
    if (actor->locomotion_state != kHangingLocomotion &&
        actor->tile_x == tile_x && actor->tile_z == tile_z &&
        !actor->motion_locked && actor->position.y >= lowest) {
      return index;
    }
  }
  return std::nullopt;
}

// decomp: BATTLE.PRG func_800A4494 (src/BATTLE/BATTLE.PRG/3A1A0.c:931)
bool CheckPlatformActorObstruction(ActorCollisionHost& host, std::int32_t layer,
                                   MotionVector point) {
  for (std::size_t index = 0; index < kCollisionActorSlots; ++index) {
    std::optional<CollisionActor> loaded = host.Actor(index);
    if (!loaded || loaded->skipped) continue;
    CollisionActor& actor = *loaded;
    if (actor.support_layer != layer) {
      if (!PlatformReachesActor(actor, point)) continue;
      if (actor.locomotion_state == kHangingLocomotion) {
        DropHangingActor(host, actor);
      }
      return true;
    }
    if (actor.locomotion_state != kHangingLocomotion) continue;
    const std::int32_t dx = point.x - actor.platform_anchor.x;
    const std::int32_t dz = point.z - actor.platform_anchor.z;
    actor.position.x = Halfword(actor.position.x + dx);
    actor.position.z = Halfword(actor.position.z + dz);
    const bool hit = IsHangingActorObstructed(host, actor);
    actor.position.x = Halfword(actor.position.x - dx);
    actor.position.z = Halfword(actor.position.z - dz);
    host.StoreActor(actor);
    if (hit) {
      DropHangingActor(host, actor);
      return true;
    }
  }
  return false;
}

// decomp: BATTLE.PRG func_800A46A4 (src/BATTLE/BATTLE.PRG/3A1A0.c:993)
bool IsHangingActorObstructed(ActorCollisionHost& host, CollisionActor& actor) {
  const std::uint8_t layer = actor.support_layer;
  actor.support_layer = 0;
  actor.locomotion_state = 0;
  actor.movement_pending = false;
  SampleActorCollisionPerimeter(host, actor,
                                CollisionDiagonalReach(actor.collision_radius));
  actor.support_layer = layer;
  actor.locomotion_state = kHangingLocomotion;
  CollisionSamples& samples = host.Samples();
  const std::int32_t facing = actor.move_facing >> kFacingSectorShift;
  ClearBlockedSector(samples, facing);
  ClearBlockedSector(samples, facing + 1);
  ClearBlockedSector(samples, (facing - 1) & kSectorMask);
  return std::ranges::any_of(samples.blocked,
                             [](std::uint8_t blocked) { return blocked != 0; });
}

// decomp: BATTLE.PRG func_800A6798 (src/BATTLE/BATTLE.PRG/3A1A0.c:1960)
bool CanActorHangFromLedge(ActorCollisionHost& host, CollisionActor& actor,
                           MotionVector& offset, MotionVector& motion) {
  if (actor.ledge_height <= host.Probe().position.y) return false;
  offset.x = 0;
  offset.z = 0;
  offset.y = Halfword(offset.y + kLedgeClimbHeight);
  const MotionVector saved = actor.position;
  actor.position = AddXyz(actor.position, offset);
  actor.locomotion_state = kHangingLocomotion;
  const TerrainMotionResult resolved =
      ResolveActorTerrainMotion(host, actor, motion, false);
  bool valid = false;
  if (resolved.motion != TerrainMotion::kBlocked && !actor.on_ledge &&
      resolved.climb && *resolved.climb < kLedgeMaxClimb) {
    offset = LedgeSideOffset(host, actor.collision_radius, offset);
    valid = HasLedgeSupportBothSides(host, offset.x, offset.z);
  }
  actor.locomotion_state = 0;
  actor.movement_pending = false;
  offset.x = 0;
  offset.z = 0;
  actor.position = saved;
  return valid;
}

// decomp: BATTLE.PRG func_800A69B4 (src/BATTLE/BATTLE.PRG/3A1A0.c:2009)
bool HasActorLedgeSideSupport(ActorCollisionHost& host,
                              const CollisionActor& actor) {
  const MotionVector side =
      LedgeSideOffset(host, actor.collision_radius, MotionVector{});
  return HasLedgeSupportBothSides(host, side.x, side.z);
}

// decomp: BATTLE.PRG func_800A6EE8 (src/BATTLE/BATTLE.PRG/3A1A0.c:2175)
std::int32_t SampleActorSupportHeight(ActorCollisionHost& host,
                                      MotionVector origin,
                                      std::int32_t offset_x,
                                      std::int32_t offset_z,
                                      SupportSample sample) {
  const std::int32_t x = WrappingAdd(offset_x, origin.x);
  const std::int32_t z = WrappingAdd(offset_z, origin.z);
  const std::optional<CollisionCellRef> cell =
      host.CellAt(Halfword(x), Halfword(z), 0);
  if (cell) {
    host.Probe().cell = cell;
    const std::int32_t height = sample == SupportSample::kFloor
                                    ? host.FloorHeight(x, z)
                                    : host.InterpolatedFloorHeight(x, z);
    if (height >= 0) {
      return SupportAboveFloor(host, origin, {.x = x, .y = origin.y, .z = z},
                               *cell, SignExtend15(height), sample);
    }
  }
  return sample == SupportSample::kShadow ? origin.y : kNoSupportHeight;
}

// decomp: BATTLE.PRG func_800A70DC (src/BATTLE/BATTLE.PRG/3A1A0.c:2256).
// A cell the centre lookup misses reads as kAbsentCellWord (#288).
void SampleActorCollisionPerimeter(ActorCollisionHost& host,
                                   CollisionActor& actor,
                                   std::int32_t diagonal) {
  CollisionSamples& samples = host.Samples();
  actor.wall_contact = false;
  samples.blocked.fill(0);
  if (actor.bypass_actor_collision || actor.bypass_terrain_collision) {
    samples.heights.fill(0);
    return;
  }
  if (actor.locomotion_state == kHangingLocomotion) {
    samples.heights.fill(actor.position.y);
    return;
  }
  SamplePerimeterSectors(host, actor, diagonal);
  MarkPerimeterWalls(samples, actor);
  if (actor.movement_mode != 0) OpenTurnSector(samples, actor);
}

// decomp: BATTLE.PRG func_800A7524 (src/BATTLE/BATTLE.PRG/3A1A0.c:2355)
void ApplyActorWallRepulsion(const CollisionSamples& samples,
                             CollisionActor& actor, MotionVector& motion) {
  if (!actor.wall_contact) return;
  WallPush push;
  for (std::size_t sector = 0; sector < kCollisionSectorCount; ++sector) {
    if (samples.blocked.at(sector) == 0) continue;
    push.x += kWallPushes.at(sector).x;
    push.z += kWallPushes.at(sector).z;
  }
  push.x = std::clamp(push.x, -kWallPush, kWallPush);
  push.z = std::clamp(push.z, -kWallPush, kWallPush);
  motion.x = PushedAxis(motion.x, push.x);
  motion.z = PushedAxis(motion.z, push.z);
  actor.platform_offset_x = 0;
  actor.platform_offset_z = 0;
}

TerrainMotionResult ResolveActorTerrainMotion(ActorCollisionHost& host,
                                              CollisionActor& actor,
                                              MotionVector& motion,
                                              bool probe_only) {
  TerrainMotionPass pass(host, actor, motion);
  return probe_only ? pass.ProbeOnly() : pass.Resolve();
}

// decomp: BATTLE.PRG func_800A8B34 (src/BATTLE/BATTLE.PRG/3A1A0.c:3008).
// The probe's w takes the supporting platform layer (low byte) and gimmick
// slot (high byte) once the probe reaches the platform test.
std::int32_t ClassifyActorTerrainProbe(ActorCollisionHost& host,
                                       const CollisionActor& mover,
                                       MotionVector& probe,
                                       std::int32_t sector) {
  const CollisionSamples& samples = host.Samples();
  const std::int32_t limit = samples.heights.at(SectorIndex(sector));
  if (mover.bypass_actor_collision || mover.bypass_terrain_collision) {
    return 0;
  }
  const std::int32_t ceiling =
      SignExtend15(host.CeilingHeight(probe.x, probe.z));
  if (limit + samples.search_floor < ceiling) return kRefusedProbe;
  const std::optional<CollisionCellRef> cell = host.CellAt(probe.x, probe.z, 0);
  if (!cell) return kRefusedProbe;
  std::int32_t height = host.FloorHeight(probe.x, probe.z);
  if (height < 0) return kRefusedProbe;
  height = SignExtend15(height);
  const GimmickSupport gimmick =
      host.SupportingGimmick(probe.x / kTileSize, probe.z / kTileSize);
  if (gimmick.height != 0) height = gimmick.height;
  if (IsBlockedCell(CellWordOrAbsent(host, cell)) && height >= 0) {
    height = kBlockedSupportHeight;
  }
  if (height - mover.position.y < -kMaxProbeDrop) return kRefusedProbe;
  const PlatformContactHeight platform =
      host.PlatformContactAt({.x = probe.x, .y = limit, .z = probe.z});
  std::uint32_t support = 0;
  if (platform.height != 0) {
    support = static_cast<std::uint16_t>(platform.supporting_layer);
    height = platform.height;
  }
  support |= static_cast<std::uint32_t>(gimmick.gimmick_index)
             << kSampleByteBits;
  probe.w = WrapSigned16(support);
  if (samples.search_floor < ceiling - height) return kRefusedProbe;
  if (height < samples.search_ceiling + limit) return kRefusedProbe;
  if (!IsActorPositionUnoccupied(host, mover, probe)) return kRefusedProbe;
  probe.y = Halfword(height);
  return ClimbSteps(height - limit);
}

// decomp: BATTLE.PRG func_800A8E84 (src/BATTLE/BATTLE.PRG/40564.c:186)
bool IsActorPositionUnoccupied(ActorCollisionHost& host,
                               const CollisionActor& actor,
                               const MotionVector& position) {
  if (actor.slot != 0 || actor.ignored_by_collision || actor.wall_contact) {
    return true;
  }
  const CollisionSamples& samples = host.Samples();
  for (std::size_t index = kFirstRoomActor; index < kCollisionActorSlots;
       ++index) {
    if (static_cast<std::int32_t>(index) == actor.table_index) continue;
    const std::optional<CollisionActor> other = host.Actor(index);
    if (!other || IsIntangible(*other) || samples.excluded.at(index)) continue;
    const std::int32_t radius = other->occupancy_radius;
    if (other->position.x + radius < position.x ||
        position.x < other->position.x - radius ||
        other->position.z + radius < position.z ||
        position.z < other->position.z - radius ||
        other->position.y < actor.position.y - actor.occupancy_height ||
        actor.position.y < other->position.y - other->occupancy_height) {
      continue;
    }
    return false;
  }
  return true;
}

// decomp: BATTLE.PRG func_800A8FD4 (src/BATTLE/BATTLE.PRG/40564.c:234).
// The actor's own slot is tested as well, with its in-flight values.
bool CanActorSeparateFromOverlaps(ActorCollisionHost& host,
                                  const CollisionActor& actor,
                                  const MotionVector& motion) {
  CollisionSamples& samples = host.Samples();
  samples.excluded.fill(false);
  if (actor.ignored_by_collision) return true;
  const std::int32_t radius = actor.occupancy_radius;
  std::int32_t min_y = actor.position.y - actor.occupancy_height;
  if (actor.action_pose_active) min_y -= kPosedOccupancyReach;
  const std::int32_t min_x = actor.position.x - radius;
  const std::int32_t max_x = actor.position.x + radius;
  const std::int32_t min_z = actor.position.z - radius;
  const std::int32_t max_z = actor.position.z + radius;
  for (std::size_t index = kFirstRoomActor; index < kCollisionActorSlots;
       ++index) {
    const std::optional<CollisionActor> other =
        static_cast<std::int32_t>(index) == actor.table_index
            ? std::optional<CollisionActor>(actor)
            : host.Actor(index);
    if (!other || IsIntangible(*other)) continue;
    const std::int32_t other_radius = other->occupancy_radius;
    if (other->position.x + other_radius < min_x ||
        max_x < other->position.x - other_radius ||
        other->position.z + other_radius < min_z ||
        max_z < other->position.z - other_radius || other->position.y < min_y ||
        actor.position.y < other->position.y - other->occupancy_height) {
      continue;
    }
    samples.excluded.at(index) = true;
    const std::int32_t old_distance =
        WrappingAdd(WrappedSquare(other->position.x - actor.position.x),
                    WrappedSquare(other->position.z - actor.position.z));
    const std::int32_t new_distance = WrappingAdd(
        WrappedSquare(other->position.x - (actor.position.x + motion.x)),
        WrappedSquare(other->position.z - (actor.position.z + motion.z)));
    if (new_distance < old_distance) return false;
  }
  return true;
}

// decomp: BATTLE.PRG func_800A9530 (src/BATTLE/BATTLE.PRG/40564.c:432)
bool CalcActorLedgeOffset(ActorCollisionHost& host, CollisionActor& actor,
                          MotionVector& offset) {
  std::optional<PlatformContact> platform;
  if (actor.support_layer != 0) {
    platform = host.VisiblePlatform(actor.support_layer);
    if (!platform) {
      VS_LOG_ERROR("field.platform",
                   "ledge probe on invisible platform layer {}",
                   static_cast<int>(actor.support_layer));
      return false;
    }
  }
  const std::int32_t direction = host.Probe().direction >> 1;
  actor.move_facing = static_cast<std::uint16_t>(direction * kQuarterTurn);
  if (platform) {
    PlatformLedgeOffset(actor, *platform, direction, offset);
  } else {
    TileLedgeOffset(actor, direction, offset);
  }
  const std::uint8_t layer = actor.support_layer;
  actor.support_layer = 0;
  actor.position.x = Halfword(actor.position.x + offset.x);
  actor.position.z = Halfword(actor.position.z + offset.z);
  SampleActorCollisionPerimeter(host, actor,
                                CollisionDiagonalReach(actor.collision_radius));
  actor.position.x = Halfword(actor.position.x - offset.x);
  actor.position.z = Halfword(actor.position.z - offset.z);
  actor.support_layer = layer;
  if (actor.wall_contact) {
    actor.wall_contact = false;
    return false;
  }
  return true;
}
}  // namespace VS::Field
