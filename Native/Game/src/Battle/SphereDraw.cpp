// Hand-maintained (tools/port/owned.csv).
#include "Battle/SphereDraw.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/TargetingDraw.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;

constexpr unsigned kHalfwordBits = 16;
constexpr std::uint32_t kHalfwordMask = 0xFFFF;

[[nodiscard]] constexpr std::uint32_t PackXy(std::int32_t x,
                                             std::int32_t y) noexcept {
  return (static_cast<std::uint32_t>(x) & kHalfwordMask) |
         (static_cast<std::uint32_t>(y) << kHalfwordBits);
}

// ---- Wire line ----
constexpr std::uint32_t kWireLineMode = 0xE1000200;
constexpr std::int32_t kOctantRounding = 0x100;
constexpr unsigned kOctantShift = 8;
constexpr std::int32_t kOctantMask = 0xE;
constexpr std::int32_t kNudgeBits = 30;
// Half-pixel nudges per octant: bit pairs of these words picked by the shift.
constexpr std::uint32_t kNudgeX = 0x4FC5;
constexpr std::uint32_t kNudgeY = 0xFC54;
constexpr std::int32_t kMaxIntensity = 63;
constexpr std::uint32_t kRgbMask = 0x00FFFFFF;
constexpr std::uint32_t kCommandMask = 0xFF000000;

[[nodiscard]] std::int32_t OctantNudge(std::uint32_t pattern,
                                       std::int32_t shift) noexcept {
  return static_cast<std::int32_t>(pattern << static_cast<unsigned>(shift)) >>
         kNudgeBits;
}

// ---- Sphere traversal ----
constexpr std::int32_t kSineSampleStep = 128;
constexpr std::size_t kTransformYColumn = 1;
constexpr std::uint8_t kTiltHalfTurn = 0x80;

// Wireframe shapes of volume kinds 1 to 7 (the jump table at 0x80069824).
constexpr std::array<std::int32_t, 7> kWireframeTypeOfKind{0, 1, 1, 2, 2, 3, 4};
// Kind 2 doubles extent 1 and moves half a transformed y axis; kind 4 turns
// extent 3 half over and moves a whole one.
constexpr std::uint8_t kStretchedKind = 2;
constexpr std::uint8_t kTiltedKind = 4;
constexpr std::size_t kStretchedExtent = 1;
constexpr std::size_t kTiltExtent = 3;

void OffsetCentre(SphereDrawVolume& volume, bool halve, SphereDrawPort& port) {
  const math::Mat3 transform =
      port.VolumeTransform(volume.extents, volume.angle);
  for (std::size_t axis = 0; axis < kTargetWireAxes; ++axis) {
    const std::int32_t lift = transform.m[axis][kTransformYColumn];
    volume.position[axis] = math::WrapSigned16(static_cast<std::uint32_t>(
        volume.position[axis] + (halve ? lift >> 1 : lift)));
  }
}

// The volume's wireframe shape, after adjusting kinds 2 and 4.
[[nodiscard]] std::optional<std::int32_t> PrepareVolume(
    SphereDrawVolume& volume, SphereDrawPort& port) {
  const auto index = static_cast<std::size_t>(volume.kind) - 1U;
  if (index >= kWireframeTypeOfKind.size()) {
    return std::nullopt;
  }
  if (volume.kind == kStretchedKind) {
    volume.extents[kStretchedExtent] =
        static_cast<std::uint8_t>(volume.extents[kStretchedExtent] << 1);
    OffsetCentre(volume, true, port);
  } else if (volume.kind == kTiltedKind) {
    volume.extents[kTiltExtent] =
        static_cast<std::uint8_t>(volume.extents[kTiltExtent] + kTiltHalfTurn);
    OffsetCentre(volume, false, port);
  }
  return kWireframeTypeOfKind[index];
}

// ---- Position cursor ----
constexpr std::uint8_t kBlinkMask = 0xF;
constexpr std::int32_t kHighlightedPose = 2;
constexpr std::uint32_t kPitchQuadColor = 0x2C808080;
constexpr std::size_t kQuadCorners = 4;

struct PitchQuadLayout {
  std::array<std::uint32_t, kQuadCorners> offsets;
  std::array<std::uint32_t, kQuadCorners> texcoords;
};

// The up and down pitch arrows: corner offsets from the cursor (x | y << 16,
// wrapping) and their texture words (first with the CLUT, second with the
// page).
constexpr std::array<PitchQuadLayout, kSphereCursorQuads> kPitchQuads{{
    {.offsets = {0xFFF7FFF3, 0xFFF7FFFD, 0xFFFEFFF3, 0xFFFEFFFD},
     .texcoords = {0x37FD38F1, 0x000C38E7, 0x00003FF1, 0x00003FE7}},
    {.offsets = {0x0001FFF3, 0x0001FFFD, 0x0008FFF3, 0x0008FFFD},
     .texcoords = {0x37FD40F1, 0x000C40E7, 0x000047F1, 0x000047E7}},
}};

[[nodiscard]] SphereCursorQuad BuildPitchQuad(const PitchQuadLayout& layout,
                                              std::uint32_t screen_xy) {
  SphereCursorQuad quad{};
  quad[0] = kPitchQuadColor;
  for (std::size_t corner = 0; corner < kQuadCorners; ++corner) {
    quad[1 + (2 * corner)] = screen_xy + layout.offsets[corner];
    quad[2 + (2 * corner)] = layout.texcoords[corner];
  }
  return quad;
}

// ---- Terrain cache ----
constexpr std::int32_t kTerrainWindowRadius = 4;
constexpr unsigned kTileShift = 7;
constexpr unsigned kMapWidthShift = 4;
constexpr unsigned kMapHeightShift = 20;
constexpr std::int32_t kOpenTerrain = 0x10000;
constexpr std::uint32_t kBlockedTerrain = 0xF0800000;
constexpr unsigned kBlocksTerrainBit = 6;
constexpr unsigned kDoorShift = 10;
constexpr std::uint32_t kDoorMask = 7;
constexpr std::uint32_t kFirstImpassableDoor = 5;
constexpr unsigned kFloorFlagBit = 19;
constexpr std::uint8_t kGimmickObjectMask = 7;
constexpr std::uint8_t kLowGimmickObjects = 2;
constexpr std::int32_t kLowGimmickLift = 0x20;
constexpr std::int32_t kGimmickLift = 0x10;
constexpr std::int32_t kSampleStep = 0x20;
constexpr std::int32_t kTileSpan = 0x80;
constexpr std::int32_t kHeightLimit = 0x4000;
constexpr unsigned kFloorSignShift = 17;
constexpr unsigned kCeilingLeftShift = 1;
constexpr std::int32_t kCeilingClearance = 0x30;
constexpr unsigned kCeilingShift = 17;
constexpr std::uint32_t kFloorFieldMask = 0x7FFF;
constexpr unsigned kFloorFieldShift = 2;
constexpr std::uint32_t kDoorFlagScale = 6;

[[nodiscard]] constexpr bool Bit(std::uint32_t word, unsigned bit) noexcept {
  return ((word >> bit) & 1U) != 0;
}

struct TerrainHeights {
  std::int32_t low = kHeightLimit;
  std::int32_t high = -kHeightLimit;
  std::int32_t top = -kHeightLimit;
};

// The lowest and highest floor (lifted by `offset`) and highest ceiling over
// a 4x4 grid of samples across the tile.
[[nodiscard]] TerrainHeights SampleTile(std::int32_t world_x,
                                        std::int32_t world_z,
                                        std::int32_t offset,
                                        SphereTerrainSource& source) {
  TerrainHeights heights;
  for (std::int32_t dz = 0; dz < kTileSpan; dz += kSampleStep) {
    for (std::int32_t dx = 0; dx < kTileSpan; dx += kSampleStep) {
      const auto sample = static_cast<std::uint32_t>(
          source.FloorAndCeiling(world_x + dx, world_z + dz));
      const std::int32_t floor = math::WrappingAdd(
          static_cast<std::int32_t>(sample << kFloorSignShift) >>
              kFloorSignShift,
          offset);
      heights.low = std::min(heights.low, floor);
      heights.high = std::max(heights.high, floor);
      heights.top = std::max(
          heights.top, static_cast<std::int32_t>(sample << kCeilingLeftShift) >>
                           kFloorSignShift);
    }
  }
  return heights;
}

// One tile's cache word; nullopt when its gimmick stack has no gimmick.
[[nodiscard]] std::optional<std::int32_t> CacheTile(
    std::int32_t tile_x, std::int32_t tile_z, std::uint32_t cell,
    std::uint8_t& flag, SphereTerrainSource& source) {
  if (Bit(cell, kBlocksTerrainBit)) {
    flag = 0;
    return static_cast<std::int32_t>(kBlockedTerrain);
  }
  const std::uint32_t door = (cell >> kDoorShift) & kDoorMask;
  if (door >= kFirstImpassableDoor && !Bit(cell, kFloorFlagBit)) {
    return kOpenTerrain;
  }
  std::int32_t value = kOpenTerrain;
  std::int32_t offset = source.TileStackOffset(tile_x, tile_z);
  if (offset != 0) {
    const std::optional<std::uint8_t> object =
        source.TopGimmickObject(tile_x, tile_z);
    if (!object.has_value()) {
      return std::nullopt;
    }
    if ((*object & kGimmickObjectMask) < kLowGimmickObjects) {
      value = 0;
      offset = math::WrappingAdd(offset, kLowGimmickLift);
    } else {
      offset = math::WrappingAdd(offset, kGimmickLift);
    }
  }
  TerrainHeights heights =
      SampleTile(tile_x << kTileShift, tile_z << kTileShift, offset, source);
  if (value != 0) {
    value = heights.low == heights.high ? 1 : 0;
  }
  if (door - 1U < kFirstImpassableDoor - 1U) {
    heights.low = heights.high;
    flag = static_cast<std::uint8_t>(door * kDoorFlagScale);
  }
  return static_cast<std::int32_t>(
      static_cast<std::uint32_t>(value) |
      (static_cast<std::uint32_t>(heights.top + kCeilingClearance)
       << kCeilingShift) |
      ((static_cast<std::uint32_t>(heights.low) & kFloorFieldMask)
       << kFloorFieldShift));
}
}  // namespace

// decomp: BATTLE.PRG func_800C0990 (src/BATTLE/BATTLE.PRG/573B8.c:552).
std::optional<SphereWireLinePacket> DrawSphereWireLine(
    const TargetWirePoint& start, const TargetWirePoint& end,
    std::uint32_t color, std::int32_t intensity) noexcept {
  const std::int32_t depth =
      (std::int32_t{start.depth} + std::int32_t{end.depth}) >> 1;
  if (static_cast<std::uint32_t>(depth) >= kSphereWireLineSlots) {
    return std::nullopt;
  }
  // ratan2 of the line's direction, from the same D_80040A14 octant table.
  const std::int32_t angle =
      math::Arctangent2(std::int32_t{end.y} - std::int32_t{start.y},
                        std::int32_t{end.x} - std::int32_t{start.x});
  const std::int32_t shift =
      kNudgeBits - (((angle + kOctantRounding) >> kOctantShift) & kOctantMask);
  const std::int32_t start_x = start.x + OctantNudge(kNudgeX, shift);
  const std::int32_t start_y = start.y + OctantNudge(kNudgeY, shift);
  const auto scale =
      static_cast<std::uint32_t>(std::clamp(intensity, 0, kMaxIntensity));
  return SphereWireLinePacket{
      .slot = static_cast<std::uint32_t>(depth),
      .mode = kWireLineMode,
      .color = ((color & kRgbMask) * scale) | (color & kCommandMask),
      .start_xy = PackXy(start_x, start_y),
      .end_xy = PackXy(end.x, end.y)};
}

// decomp: BATTLE.PRG func_800C0D78 (src/BATTLE/BATTLE.PRG/58578.c:104).
void DrawSphere(const std::optional<SphereDrawVolumes>& volumes,
                SphereDrawPort& port) {
  if (!volumes.has_value()) {
    return;
  }
  std::array<std::int16_t, kSphereSineSamples> sine{};
  for (std::size_t index = 0; index < sine.size(); ++index) {
    sine[index] = math::WrapSigned16(static_cast<std::uint32_t>(
        math::Sine(static_cast<std::int32_t>(index) * kSineSampleStep)));
  }
  port.StoreSineSamples(sine);
  std::uint32_t color = kSpherePrimaryVolumeColor;
  for (std::size_t slot = 0; slot < kSphereDrawVolumes; ++slot) {
    SphereDrawVolume volume = (*volumes)[slot];
    port.StageVolume(slot, volume);
    port.LoadFrameView();
    port.StoreCenterDepth(port.ProjectCenterDepth(volume.position));
    if (const std::optional<std::int32_t> type = PrepareVolume(volume, port)) {
      port.BuildWireframe(*type);
      port.DrawVolumeWireframe(slot, volume, color);
    }
    color = kSphereSecondaryVolumeColor;
  }
  port.UpdateTargetLabels();
  port.DrawTargetPageHint();
}

// decomp: BATTLE.PRG func_800C282C (src/BATTLE/BATTLE.PRG/58578.c:965).
SphereCameraBasis BuildSphereCameraBasis(std::int32_t yaw) noexcept {
  const std::int32_t cosine = math::Cosine(yaw);
  const std::int32_t sine = math::Sine(yaw);
  return {
      .cosine = math::WrapSigned16(static_cast<std::uint32_t>(cosine)),
      .sine = math::WrapSigned16(static_cast<std::uint32_t>(sine)),
      .negated_sine = math::WrapSigned16(0U - static_cast<std::uint32_t>(sine)),
      .yaw = math::WrapSigned16(static_cast<std::uint32_t>(yaw))};
}

// decomp: BATTLE.PRG func_800C28AC (src/BATTLE/BATTLE.PRG/58578.c:984).
std::optional<SpherePositionCursorFrame> DrawSpherePositionCursor(
    std::uint32_t screen_xy, std::int32_t lock_pitch, std::uint8_t blink,
    const SphereCursorTables& tables) noexcept {
  if (lock_pitch != 0 && lock_pitch != 1) {
    return std::nullopt;
  }
  SpherePositionCursorFrame frame;
  frame.blink = static_cast<std::uint8_t>((blink + 1U) & kBlinkMask);
  if (lock_pitch == 0) {
    frame.pitch_quads = std::array<SphereCursorQuad, kSphereCursorQuads>{
        BuildPitchQuad(kPitchQuads[0], screen_xy),
        BuildPitchQuad(kPitchQuads[1], screen_xy)};
  }
  for (std::size_t sprite = 0; sprite < kSphereCursorSprites; ++sprite) {
    const auto pose =
        static_cast<std::int32_t>(sprite) + (lock_pitch * kHighlightedPose);
    const auto index = static_cast<std::size_t>(pose);
    frame.sprites[sprite] = {
        .brightness = pose == kHighlightedPose
                          ? std::int32_t{tables.brightness[frame.blink]}
                          : kSphereCursorPlainBrightness,
        .xy = screen_xy + static_cast<std::uint32_t>(tables.offsets[index]),
        .wh = static_cast<std::uint32_t>(tables.sizes[index]),
        .texcoord = static_cast<std::uint32_t>(tables.texcoords[index])};
  }
  return frame;
}

// decomp: BATTLE.PRG func_800C2B0C (src/BATTLE/BATTLE.PRG/58578.c:1036).
SphereTerrainRow CacheSphereTerrainRow(std::int16_t x, std::int16_t z,
                                       std::uint32_t row,
                                       SphereTerrainSource& source) {
  SphereTerrainRow result;
  if (row >= kSphereTerrainRows) {
    result.status = SphereTerrainRowStatus::kRowOutOfRange;
    return result;
  }
  const std::int32_t dimensions = source.MapDimensions();
  const std::uint32_t width =
      (static_cast<std::uint32_t>(dimensions) & kHalfwordMask) >>
      kMapWidthShift;
  const auto height = static_cast<std::uint32_t>(dimensions >> kMapHeightShift);
  const std::int32_t tile_z =
      (z >> kTileShift) + static_cast<std::int32_t>(row) - kTerrainWindowRadius;
  for (std::size_t column = 0; column < kSphereTerrainColumns; ++column) {
    const std::int32_t tile_x = (x >> kTileShift) +
                                static_cast<std::int32_t>(column) -
                                kTerrainWindowRadius;
    result.heights[column] = kOpenTerrain;
    if (static_cast<std::uint32_t>(tile_x) >= width ||
        static_cast<std::uint32_t>(tile_z) >= height) {
      continue;
    }
    const std::optional<std::int32_t> value =
        CacheTile(tile_x, tile_z, source.CellWord(tile_x, tile_z),
                  result.flags[column], source);
    if (!value.has_value()) {
      result.status = SphereTerrainRowStatus::kMissingGimmick;
      return result;
    }
    result.heights[column] = *value;
  }
  return result;
}
}  // namespace VS::Battle
