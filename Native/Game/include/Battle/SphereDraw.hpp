#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/TargetingDraw.hpp"
#include "Core/Math/ScalarMath.hpp"

// The battle target sphere's per-frame drawing: the wire line primitive, the
// traversal over its two volumes, the camera yaw basis, the position cursor and
// the terrain height cache. Volumes, projected points, tables and terrain
// samples are passed as values and effects go through small ports, so these
// rules read no scratchpad, GTE register, target sphere global or PS1 address.
namespace VS::Battle {
// ---- Wire line (func_800C0990) --------------------------------------------

// Ordering-table slots the wire line may use; a deeper midpoint is culled.
inline constexpr std::uint32_t kSphereWireLineSlots = 0x800;

// One textured-mode line packet's body after its tag word. `slot` is the
// ordering-table entry the packet links into.
struct SphereWireLinePacket {
  std::uint32_t slot = 0;
  std::uint32_t mode = 0;
  std::uint32_t color = 0;
  std::uint32_t start_xy = 0;
  std::uint32_t end_xy = 0;
  friend bool operator==(const SphereWireLinePacket&,
                         const SphereWireLinePacket&) = default;
};

// The line from `start` to `end` (screen x, y and ordering depth), its start
// nudged half a pixel along the line's octant. `color` keeps its command byte
// and its RGB is scaled by `intensity` clamped to 0..63. Nothing is drawn
// when the midpoint depth is outside the ordering table.
[[nodiscard]] std::optional<SphereWireLinePacket> DrawSphereWireLine(
    const TargetWirePoint& start, const TargetWirePoint& end,
    std::uint32_t color, std::int32_t intensity) noexcept;

// ---- Sphere traversal (func_800C0D78) -------------------------------------

inline constexpr std::size_t kSphereDrawVolumes = 2;
inline constexpr std::size_t kSphereSineSamples = 40;
// Wire colours of the first and second volume.
inline constexpr std::uint32_t kSpherePrimaryVolumeColor = 0x10401;
inline constexpr std::uint32_t kSphereSecondaryVolumeColor = 0x10104;

// One 16-byte target volume record.
struct SphereDrawVolume {
  std::uint8_t kind = 0;
  std::uint8_t reserved = 0;
  std::int16_t angle = 0;
  TargetVolumeExtents extents{};
  TargetWorldPoint position{};
  std::int16_t position_pad = 0;
  friend bool operator==(const SphereDrawVolume&,
                         const SphereDrawVolume&) = default;
};

using SphereDrawVolumes = std::array<SphereDrawVolume, kSphereDrawVolumes>;

class SphereDrawPort {
 public:
  SphereDrawPort() = default;
  SphereDrawPort(const SphereDrawPort&) = delete;
  SphereDrawPort& operator=(const SphereDrawPort&) = delete;
  SphereDrawPort(SphereDrawPort&&) = delete;
  SphereDrawPort& operator=(SphereDrawPort&&) = delete;
  virtual ~SphereDrawPort() = default;

  // Q12 sine of 0, 128, ... 4992 for the wireframe builder.
  virtual void StoreSineSamples(
      std::span<const std::int16_t, kSphereSineSamples> samples) = 0;
  // The working copy of volume `slot` as read from the sphere.
  virtual void StageVolume(std::size_t slot,
                           const SphereDrawVolume& volume) = 0;
  virtual void LoadFrameView() = 0;
  // The ordering depth (SZ3 / 4) of `position` under the frame view.
  [[nodiscard]] virtual std::int32_t ProjectCenterDepth(
      const TargetWorldPoint& position) = 0;
  // Stores `depth` as one word over the sphere's depth and camera yaw.
  virtual void StoreCenterDepth(std::int32_t depth) = 0;
  // The volume's scale-and-tilt matrix (func_800C085C).
  [[nodiscard]] virtual Core::Math::Mat3 VolumeTransform(
      const TargetVolumeExtents& extents, std::int16_t angle) = 0;
  // Builds the wire vertices of shape `type` (func_800C253C).
  virtual void BuildWireframe(std::int32_t type) = 0;
  // Draws the adjusted working copy of volume `slot` (func_800C0B50).
  virtual void DrawVolumeWireframe(std::size_t slot,
                                   const SphereDrawVolume& volume,
                                   std::uint32_t color) = 0;
  // Updates and draws the target labels (func_800C20B4).
  virtual void UpdateTargetLabels() = 0;
  // Native addition: the extended target list's page indicator.
  virtual void DrawTargetPageHint() = 0;
};

// Draws both volumes of the active sphere; `volumes` is empty without one.
// Kinds 1 to 7 select wireframe shapes 0 to 4; kinds 2 and 4 first stretch
// or tilt the volume and move its centre along the transformed y axis. Other
// kinds are staged and projected but not drawn.
void DrawSphere(const std::optional<SphereDrawVolumes>& volumes,
                SphereDrawPort& port);

// ---- Camera basis (func_800C282C) -----------------------------------------

// The yaw rotation's matrix entries and the sphere's recorded yaw.
struct SphereCameraBasis {
  std::int16_t cosine = 0;
  std::int16_t sine = 0;
  std::int16_t negated_sine = 0;
  std::int16_t yaw = 0;
  friend bool operator==(const SphereCameraBasis&,
                         const SphereCameraBasis&) = default;
};

[[nodiscard]] SphereCameraBasis BuildSphereCameraBasis(
    std::int32_t yaw) noexcept;

// ---- Position cursor (func_800C28AC) --------------------------------------

inline constexpr std::size_t kSphereCursorPoses = 5;
inline constexpr std::size_t kSphereCursorSprites = 3;
inline constexpr std::size_t kSphereCursorBlinkFrames = 16;
inline constexpr std::size_t kSphereCursorQuads = 2;
// A textured quad's words after its tag.
inline constexpr std::size_t kSphereCursorQuadWords = 9;
inline constexpr std::int32_t kSphereCursorPlainBrightness = 0x80;

struct SphereCursorTables {
  std::array<std::int32_t, kSphereCursorPoses> offsets{};
  std::array<std::int32_t, kSphereCursorPoses> sizes{};
  std::array<std::int32_t, kSphereCursorPoses> texcoords{};
  std::array<std::uint8_t, kSphereCursorBlinkFrames> brightness{};
};

struct SphereCursorSprite {
  std::int32_t brightness = 0;
  std::uint32_t xy = 0;
  std::uint32_t wh = 0;
  std::uint32_t texcoord = 0;
  friend bool operator==(const SphereCursorSprite&,
                         const SphereCursorSprite&) = default;
};

using SphereCursorQuad = std::array<std::uint32_t, kSphereCursorQuadWords>;

struct SpherePositionCursorFrame {
  std::uint8_t blink = 0;
  // The pitch arrows, drawn only while the pitch is free.
  std::optional<std::array<SphereCursorQuad, kSphereCursorQuads>> pitch_quads;
  std::array<SphereCursorSprite, kSphereCursorSprites> sprites{};
};

// The cursor at packed screen point `screen_xy`. `blink` is the previous
// blink frame. `lock_pitch` selects the free (0) or locked (1) sprite set;
// other values have no sprite set and are refused.
[[nodiscard]] std::optional<SpherePositionCursorFrame> DrawSpherePositionCursor(
    std::uint32_t screen_xy, std::int32_t lock_pitch, std::uint8_t blink,
    const SphereCursorTables& tables) noexcept;

// ---- Terrain cache (func_800C2B0C) ----------------------------------------

inline constexpr std::uint32_t kSphereTerrainRows = 9;
inline constexpr std::size_t kSphereTerrainColumns = 9;

class SphereTerrainSource {
 public:
  SphereTerrainSource() = default;
  SphereTerrainSource(const SphereTerrainSource&) = delete;
  SphereTerrainSource& operator=(const SphereTerrainSource&) = delete;
  SphereTerrainSource(SphereTerrainSource&&) = delete;
  SphereTerrainSource& operator=(SphereTerrainSource&&) = delete;
  virtual ~SphereTerrainSource() = default;

  // Width in tiles << 4 in the low halfword, height in tiles from bit 20.
  [[nodiscard]] virtual std::int32_t MapDimensions() = 0;
  // The collision cell word of an in-bounds tile.
  [[nodiscard]] virtual std::uint32_t CellWord(std::int32_t tile_x,
                                               std::int32_t tile_z) = 0;
  // The height of the gimmick stack on a tile, 0 for none.
  [[nodiscard]] virtual std::int32_t TileStackOffset(std::int32_t tile_x,
                                                     std::int32_t tile_z) = 0;
  // The object information byte of the top gimmick on a tile; nullopt when
  // the tile reports no gimmick.
  [[nodiscard]] virtual std::optional<std::uint8_t> TopGimmickObject(
      std::int32_t tile_x, std::int32_t tile_z) = 0;
  // The floor in the low halfword and the ceiling in the high one.
  [[nodiscard]] virtual std::int32_t FloorAndCeiling(std::int32_t world_x,
                                                     std::int32_t world_z) = 0;
};

enum class SphereTerrainRowStatus : std::uint8_t {
  kCached,
  kRowOutOfRange,
  // A tile had a gimmick stack but no gimmick to read; nothing is cached.
  kMissingGimmick,
};

struct SphereTerrainRow {
  SphereTerrainRowStatus status = SphereTerrainRowStatus::kCached;
  std::array<std::int32_t, kSphereTerrainColumns> heights{};
  std::array<std::uint8_t, kSphereTerrainColumns> flags{};
};

// Row `row` of the 9x9 tile window centred on world point (`x`, `z`): each
// entry packs the ceiling, the lowest floor and whether the tile is flat, or
// marks a blocking or absent tile.
[[nodiscard]] SphereTerrainRow CacheSphereTerrainRow(
    std::int16_t x, std::int16_t z, std::uint32_t row,
    SphereTerrainSource& source);
}  // namespace VS::Battle
