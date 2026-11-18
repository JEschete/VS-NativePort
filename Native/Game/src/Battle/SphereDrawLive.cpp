// Hand-maintained (tools/port/owned.csv).
#include "Battle/SphereDrawLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>

#include "Battle/BattleTypes.h"
#include "Battle/Sphere.h"
#include "Battle/SphereDraw.hpp"
#include "Battle/SphereGeometryLive.hpp"
#include "Battle/SphereTargetLabelsLive.hpp"
#include "Battle/TargetPages.h"
#include "Battle/TargetVolume.h"
#include "Battle/TargetingDraw.hpp"
#include "Battle/TargetingDrawLive.hpp"
#include "Core/Gte.h"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Scratch.h"
#include "Field/Camera.h"
#include "Field/CameraLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "Field/GimmickLookup.h"
#include "Field/GimmickPlacementLive.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/Menus.h"
#include "Render/Render.h"
#include "Render/SpritesLive.hpp"
#include "Render/UiPrimitives.h"

namespace VS::Battle {
namespace {
namespace math = Core::Math;

constexpr std::size_t kTransformScratchOffset = 0x350;
constexpr std::size_t kVolumeScratchOffset = 0x378;
constexpr std::size_t kSineScratchOffset = 0x3B0;
// The yaw basis entries written into the scratchpad MATRIX (halfwords).
constexpr std::size_t kBasisCosine = 0;
constexpr std::size_t kBasisSine = 2;
constexpr std::size_t kBasisNegatedSine = 8;
constexpr std::size_t kBasisCosineAgain = 10;
constexpr std::size_t kWireLineWords = 5;
constexpr std::size_t kCursorQuadWords = 1 + kSphereCursorQuadWords;
constexpr u_long kLinkMask = 0x00FFFFFF;
constexpr u_long kWireLineTag = 0x04000000;
constexpr u_long kCursorQuadTag = 0x09000000;
constexpr std::size_t kSpriteTexcoordWord = 4;
constexpr unsigned kHalfwordBits = 16;

constexpr std::size_t kTargetVolumeBytes = 16;
static_assert(sizeof(Battle_TargetVolume) == kTargetVolumeBytes);
static_assert(sizeof(Field_MpdTileCell) == sizeof(std::uint32_t));
static_assert(std::size(Battle_TargetSphere{}.terrainHeights) ==
              kSphereTerrainRows * kSphereTerrainColumns);
static_assert(std::size(Battle_TargetSphere{}.terrainRowFlags) ==
              kSphereTerrainRows * kSphereTerrainColumns);

std::uint64_t refusals = 0;

void Refuse(const char* what) {
  if (std::has_single_bit(++refusals)) {
    VS_LOG_WARN("battle.sphere", "Sphere drawing refused: {} ({} refusals)",
                what, refusals);
  }
}

[[nodiscard]] std::byte* Scratch(std::size_t offset) {
  return static_cast<std::byte*>(Core_ScratchBase()) + offset;
}

[[nodiscard]] Battle_TargetSphere* LiveSphere() {
  return reinterpret_cast<Battle_TargetSphere*>(Battle_targetSphere);
}

[[nodiscard]] SphereDrawVolume VolumeOf(const Battle_TargetVolume& source) {
  SphereDrawVolume volume{.kind = source.kind,
                          .reserved = source.opaque1,
                          .angle = source.angle,
                          .position_pad = source.position[kTargetWireAxes]};
  std::copy_n(std::begin(source.scale.extents), volume.extents.size(),
              volume.extents.begin());
  std::copy_n(std::begin(source.position), volume.position.size(),
              volume.position.begin());
  return volume;
}

[[nodiscard]] Battle_TargetVolume RecordOf(const SphereDrawVolume& volume) {
  Battle_TargetVolume record{};
  record.kind = volume.kind;
  record.opaque1 = volume.reserved;
  record.angle = volume.angle;
  std::copy(volume.extents.begin(), volume.extents.end(),
            std::begin(record.scale.extents));
  std::copy(volume.position.begin(), volume.position.end(),
            std::begin(record.position));
  record.position[kTargetWireAxes] = volume.position_pad;
  return record;
}

[[nodiscard]] Battle_TargetVolume* StagedVolume(std::size_t slot) {
  return reinterpret_cast<Battle_TargetVolume*>(
      Scratch(kVolumeScratchOffset + (slot * sizeof(Battle_TargetVolume))));
}

class LiveSphereDrawPort final : public SphereDrawPort {
 public:
  explicit LiveSphereDrawPort(Battle_TargetSphere& sphere) : sphere_(sphere) {}

  void StoreSineSamples(
      std::span<const std::int16_t, kSphereSineSamples> samples) override {
    std::memcpy(Scratch(kSineScratchOffset), samples.data(),
                samples.size_bytes());
  }

  void StageVolume(std::size_t slot, const SphereDrawVolume& volume) override {
    *StagedVolume(slot) = RecordOf(volume);
  }

  void LoadFrameView() override { VS::Render::LoadLiveFrameViewMatrix(); }

  std::int32_t ProjectCenterDepth(const TargetWorldPoint& position) override {
    const SVECTOR point = {position[0], position[1], position[2], 0};
    Core_GteWord depth = 0;
    Core_Gte_LoadV0(&point);
    Core_Gte_Rtps();
    Core_Gte_StoreSz3Otz(&depth);
    return static_cast<std::int32_t>(depth);
  }

  // The original's word store also clears the camera yaw halfword.
  void StoreCenterDepth(std::int32_t depth) override {
    const auto word = static_cast<std::uint32_t>(depth);
    sphere_.depth = math::WrapSigned16(word);
    sphere_.cameraYaw = math::WrapSigned16(word >> kHalfwordBits);
  }

  math::Mat3 VolumeTransform(const TargetVolumeExtents& extents,
                             std::int16_t angle) override {
    std::array<u_char, kTargetVolumeExtents> scale{};
    std::copy(extents.begin(), extents.end(), scale.begin());
    return math::MatrixOf(
        *BuildSphereVolumeTransformEntry(scale.data(), angle));
  }

  void BuildWireframe(std::int32_t type) override {
    BuildSphereWireframeEntry(type);
  }

  void DrawVolumeWireframe(std::size_t slot, const SphereDrawVolume& volume,
                           std::uint32_t color) override {
    Battle_TargetVolume* staged = StagedVolume(slot);
    *staged = RecordOf(volume);
    DrawLiveSphereVolumeWireframe(staged, static_cast<int>(color));
  }

  void UpdateTargetLabels() override { UpdateSphereTargetLabelsEntry(); }

  void DrawTargetPageHint() override { Battle_DrawTargetPageHint(); }

 private:
  Battle_TargetSphere& sphere_;
};

[[nodiscard]] SphereCursorTables LiveCursorTables() {
  SphereCursorTables tables;
  std::copy_n(std::begin(Battle_positionCursorSpriteOffsets),
              kSphereCursorPoses, tables.offsets.begin());
  std::copy_n(std::begin(Battle_positionCursorSpriteSizes), kSphereCursorPoses,
              tables.sizes.begin());
  std::copy_n(std::begin(Battle_positionCursorSpriteTexCoords),
              kSphereCursorPoses, tables.texcoords.begin());
  // D_800EC270 is read with lbu: the brightness bytes are unsigned.
  for (std::size_t frame = 0; frame < kSphereCursorBlinkFrames; ++frame) {
    tables.brightness[frame] = static_cast<std::uint8_t>(
        Menu_Framework_cursorBrightnessAnimation[frame]);
  }
  return tables;
}

class LiveTerrainSource final : public SphereTerrainSource {
 public:
  std::int32_t MapDimensions() override {
    return VS::Field::LiveCollisionMapDimensions(0);
  }

  std::uint32_t CellWord(std::int32_t tile_x, std::int32_t tile_z) override {
    return std::bit_cast<std::uint32_t>(*VS::Field::LiveCollisionCellOrAbsent(
        static_cast<unsigned>(tile_x), static_cast<unsigned>(tile_z), 0));
  }

  std::int32_t TileStackOffset(std::int32_t tile_x,
                               std::int32_t tile_z) override {
    int offset = 0;
    static_cast<void>(Field_MeasureTileStackEntry(tile_x, tile_z, &offset));
    return offset;
  }

  std::optional<std::uint8_t> TopGimmickObject(std::int32_t tile_x,
                                               std::int32_t tile_z) override {
    const Field_GimmickInstance* gimmick = Field_FindGimmickForActorSlot(
        Field_FindTopGimmickAtTileEntry(tile_x, tile_z, 0));
    if (gimmick == nullptr) {
      return std::nullopt;
    }
    return static_cast<std::uint8_t>(gimmick->objectInfo.objectId);
  }

  std::int32_t FloorAndCeiling(std::int32_t world_x,
                               std::int32_t world_z) override {
    return VS::Field::LiveFloorAndCeilingHeightsAt(world_x, world_z);
  }
};

[[nodiscard]] u_long* OrderingTable() {
  return static_cast<u_long*>(Render_frameScratch.orderingTable);
}

// Reserves `words` words at the frame cursor and advances it past them.
[[nodiscard]] u_long* ReserveLinkedPacket(std::size_t words) {
  u_long* packet = VS::Render::ReserveLivePrimitiveWords(words);
  if (packet != nullptr) {
    Render_frameScratch.primitiveCursor = packet + words;
  }
  return packet;
}
}  // namespace

// decomp: BATTLE.PRG func_800C0990 (src/BATTLE/BATTLE.PRG/573B8.c:552).
void LiveDrawSphereWireLine(SVECTOR* start, SVECTOR* end, u_int color,
                            int intensity) {
  if (start == nullptr || end == nullptr) {
    Refuse("wire line without both endpoints");
    return;
  }
  const std::optional<SphereWireLinePacket> line = DrawSphereWireLine(
      {.x = start->vx, .y = start->vy, .depth = start->vz},
      {.x = end->vx, .y = end->vy, .depth = end->vz}, color, intensity);
  if (!line.has_value()) {
    return;
  }
  u_long* ordering = OrderingTable();
  if (ordering == nullptr) {
    Refuse("wire line without an ordering table");
    return;
  }
  u_long* packet = ReserveLinkedPacket(kWireLineWords);
  if (packet == nullptr) {
    Refuse("wire line with a full primitive buffer");
    return;
  }
  u_long& slot = ordering[line->slot];
  const std::array<u_long, kWireLineWords> words{
      (slot & kLinkMask) | kWireLineTag, line->mode, line->color,
      line->start_xy, line->end_xy};
  std::copy(words.begin(), words.end(), packet);
  slot = LegacyGpu_Handle(packet);
}

// decomp: BATTLE.PRG func_800C0D78 (src/BATTLE/BATTLE.PRG/58578.c:104).
void LiveDrawSphere() {
  Battle_TargetSphere* sphere = LiveSphere();
  if (sphere == nullptr) {
    return;
  }
  LiveSphereDrawPort port(*sphere);
  DrawSphere(SphereDrawVolumes{VolumeOf(sphere->volumes[0]),
                               VolumeOf(sphere->volumes[1])},
             port);
}

// decomp: BATTLE.PRG func_800C282C (src/BATTLE/BATTLE.PRG/58578.c:965).
void* LiveBuildSphereCameraBasis() {
  Field_SphericalCamera camera{};
  static_cast<void>(VS::Field::LiveSyncCameraAnglesFromPosition(&camera));
  const SphereCameraBasis basis = BuildSphereCameraBasis(camera.values.yaw);
  auto* matrix =
      reinterpret_cast<std::int16_t*>(Scratch(kTransformScratchOffset));
  const std::span<std::int16_t> entries(matrix, kBasisCosineAgain + 1);
  entries[kBasisCosine] = basis.cosine;
  entries[kBasisSine] = basis.sine;
  entries[kBasisNegatedSine] = basis.negated_sine;
  entries[kBasisCosineAgain] = basis.cosine;
  if (Battle_TargetSphere* sphere = LiveSphere()) {
    sphere->cameraYaw = basis.yaw;
  } else {
    Refuse("camera yaw without a target sphere");
  }
  return Scratch(kTransformScratchOffset);
}

// decomp: BATTLE.PRG func_800C28AC (src/BATTLE/BATTLE.PRG/58578.c:984).
void LiveDrawSpherePositionCursor(SVECTOR* position, int lock_pitch) {
  if (position == nullptr) {
    Refuse("position cursor without a position");
    return;
  }
  u_long* ordering = OrderingTable();
  if (ordering == nullptr) {
    Refuse("position cursor without an ordering table");
    return;
  }
  Core_GteWord screen = 0;
  Core_Gte_LoadV0(position);
  Core_Gte_Rtps();
  Core_Gte_StoreSxy2(&screen);
  const std::optional<SpherePositionCursorFrame> frame =
      DrawSpherePositionCursor(static_cast<std::uint32_t>(screen), lock_pitch,
                               Battle_positionCursorBlinkFrame,
                               LiveCursorTables());
  if (!frame.has_value()) {
    Refuse("position cursor with an unknown pitch lock");
    return;
  }
  Battle_positionCursorBlinkFrame = frame->blink;
  // The cursor links in front of the ordering table's first slot.
  u_long* ot = std::prev(ordering);
  if (frame->pitch_quads.has_value()) {
    for (const SphereCursorQuad& quad : *frame->pitch_quads) {
      u_long* packet = ReserveLinkedPacket(kCursorQuadWords);
      if (packet == nullptr) {
        Refuse("position cursor with a full primitive buffer");
        break;
      }
      packet[0] = (*ot & kLinkMask) | kCursorQuadTag;
      std::copy(quad.begin(), quad.end(), std::next(packet));
      *ot = LegacyGpu_Handle(packet);
    }
  }
  for (const SphereCursorSprite& sprite : frame->sprites) {
    u_long* packet = VS::Render::QueueLiveSpriteWithTexturePage(
        sprite.brightness, static_cast<int>(sprite.xy),
        static_cast<int>(sprite.wh), ot);
    packet[kSpriteTexcoordWord] = sprite.texcoord;
  }
}

// decomp: BATTLE.PRG func_800C2B0C (src/BATTLE/BATTLE.PRG/58578.c:1036).
void LiveCacheSphereTerrainRow(SVECTOR* position, u_int row) {
  if (row >= kSphereTerrainRows) {
    return;
  }
  Battle_TargetSphere* sphere = LiveSphere();
  if (position == nullptr || sphere == nullptr) {
    Refuse("terrain row without a position or target sphere");
    return;
  }
  LiveTerrainSource source;
  const SphereTerrainRow cached =
      CacheSphereTerrainRow(position->vx, position->vz, row, source);
  if (cached.status == SphereTerrainRowStatus::kMissingGimmick) {
    Refuse("terrain row over a gimmick stack with no gimmick");
    return;
  }
  if (cached.status != SphereTerrainRowStatus::kCached) {
    return;
  }
  const std::size_t first = row * kSphereTerrainColumns;
  std::copy(cached.heights.begin(), cached.heights.end(),
            std::next(std::begin(sphere->terrainHeights),
                      static_cast<std::ptrdiff_t>(first)));
  std::copy(cached.flags.begin(), cached.flags.end(),
            std::next(std::begin(sphere->terrainRowFlags),
                      static_cast<std::ptrdiff_t>(first)));
}
}  // namespace VS::Battle
