#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Actors/MotionRequests.hpp"
#include "Field/CollisionMap.hpp"
#include "Field/PlatformPlacement.hpp"

// Actor terrain collision: support heights under an actor, the eight-sector
// perimeter that finds walls, the wall push, terrain motion resolution with
// ledge slides, actor overlap tests and ledge hang checks. Actors are values
// the host loads and stores; the room's cells, heights, gimmicks and moving
// platforms come through the host. The perimeter samples and the last probe
// are host-owned state that persists between calls, as the original's
// scratchpad block and probe globals did.
namespace VS::Field {
using Actors::MotionVector;

inline constexpr std::size_t kCollisionActorSlots = 17;
inline constexpr std::size_t kCollisionSectorCount = 8;
// The height a blocked cell reports, and the height off the floor.
inline constexpr std::int32_t kBlockedSupportHeight = 3000;
inline constexpr std::int32_t kNoSupportHeight = -3000;
// The result of a terrain probe the actor cannot stand on.
inline constexpr std::int32_t kRefusedProbe = 0xFF;

// The actor record fields the collision rules read and write. table_index is
// the actor's slot in the actor table, or -1 for a record outside it.
struct CollisionActor {
  std::int32_t table_index = -1;
  bool skipped = false;
  bool movement_pending = false;
  std::uint8_t movement_mode = 0;
  std::uint8_t locomotion_state = 0;
  bool motion_locked = false;
  bool weapon_drawn = false;
  std::uint8_t reaction_state = 0;
  std::uint8_t pose_state = 0;
  std::uint8_t support_layer = 0;
  std::uint8_t slot = 0;
  std::uint8_t attach_bone = 0;
  MotionVector position{};
  MotionVector root_motion{};
  std::uint8_t tile_x = 0;
  std::uint8_t tile_layer = 0;
  std::uint8_t tile_z = 0;
  bool action_pose_active = false;
  bool bypass_actor_collision = false;
  bool bypass_terrain_collision = false;
  bool ignored_by_collision = false;
  bool wall_contact = false;
  bool on_ledge = false;
  bool was_on_ledge = false;
  bool animation_locked = false;
  std::uint8_t support_gimmick = 0;
  MotionVector route_target{};
  std::uint16_t collision_radius = 0;
  std::uint16_t occupancy_radius = 0;
  std::uint16_t occupancy_height = 0;
  std::uint16_t model_id = 0;
  std::int16_t ledge_height = 0;
  std::int8_t platform_offset_x = 0;
  std::int8_t platform_offset_z = 0;
  MotionVector platform_anchor{};
  std::uint8_t terrain_facing = 0;
  std::uint16_t move_facing = 0;
  std::int16_t turn_direction = 0;
  std::uint32_t turn_distance = 0;

  friend bool operator==(const CollisionActor&,
                         const CollisionActor&) = default;
};

// A gimmick's tile, read as the original's signed bytes.
struct GimmickTile {
  std::int8_t x = 0;
  std::int8_t z = 0;
};

// The perimeter samples and terrain search window (the original's
// scratchpad block at 0x1F8003BC). Sector n faces direction n * 512; sector 0
// samples -Z and sector 6 samples +X.
struct CollisionSamples {
  std::int32_t search_floor = 0;
  std::int32_t search_ceiling = 0;
  // Actor slots an overlap test already accepted for the current move.
  std::array<bool, kCollisionActorSlots> excluded{};
  std::array<std::int16_t, kCollisionSectorCount> heights{};
  std::array<std::uint8_t, kCollisionSectorCount> blocked{};
  std::array<std::uint8_t, kCollisionSectorCount> attributes{};
  std::uint8_t center_attribute = 0;

  friend bool operator==(const CollisionSamples&,
                         const CollisionSamples&) = default;
};

// The last terrain probe: the cell a support sample found (sticky until the
// next sample finds one), the accepted probe position (w holds the supporting
// platform layer in its low byte and gimmick slot in its high byte) and the
// probe's sector.
struct CollisionProbe {
  std::optional<CollisionCellRef> cell;
  MotionVector position{};
  std::uint8_t direction = 0;

  friend bool operator==(const CollisionProbe&,
                         const CollisionProbe&) = default;
};

// What a support sample reads.
enum class SupportSample : std::uint8_t {
  kFloor = 0,           // the unsmoothed floor, gimmicks and platforms
  kInterpolated = 1,    // the smoothed floor, gimmicks and platforms
  kBelowReference = 2,  // the smoothed floor and gimmicks below the origin
  kShadow = 3,          // as kInterpolated, never far above the origin
  kNoPlatforms = 4,     // the smoothed floor and gimmicks
};

// How terrain motion resolved: the whole step, only one axis of it, or none.
enum class TerrainMotion : std::uint8_t {
  kAccepted = 0,
  kAlongX = 1,
  kAlongZ = 2,
  kBlocked = 3,
};

struct TerrainMotionResult {
  TerrainMotion motion = TerrainMotion::kBlocked;
  // The climb in 64-unit steps the accepted probe needs; nullopt when the
  // motion was blocked before any probe was accepted.
  std::optional<std::int32_t> climb;

  friend bool operator==(const TerrainMotionResult&,
                         const TerrainMotionResult&) = default;
};

// The room and actor table the collision rules run against.
class ActorCollisionHost {
 public:
  ActorCollisionHost() = default;
  ActorCollisionHost(const ActorCollisionHost&) = delete;
  ActorCollisionHost& operator=(const ActorCollisionHost&) = delete;
  ActorCollisionHost(ActorCollisionHost&&) = delete;
  ActorCollisionHost& operator=(ActorCollisionHost&&) = delete;
  virtual ~ActorCollisionHost() = default;

  // The cell FindCollisionCellAtPosition finds.
  [[nodiscard]] virtual std::optional<CollisionCellRef> CellAt(
      std::int16_t world_x, std::int16_t world_z, std::int32_t layer) const = 0;
  // A cell's word; nullopt when the cell is outside the current tables.
  [[nodiscard]] virtual std::optional<std::uint32_t> CellWord(
      CollisionCellRef cell) const = 0;
  [[nodiscard]] virtual std::int16_t FloorHeight(
      std::int32_t world_x, std::int32_t world_z) const = 0;
  [[nodiscard]] virtual std::int16_t InterpolatedFloorHeight(
      std::int32_t world_x, std::int32_t world_z) const = 0;
  [[nodiscard]] virtual std::int16_t CeilingHeight(
      std::int32_t world_x, std::int32_t world_z) const = 0;
  // Also records the supporting gimmick slot for the movement rules.
  virtual GimmickSupport SupportingGimmick(std::int32_t tile_x,
                                           std::int32_t tile_z) = 0;
  [[nodiscard]] virtual std::int32_t GimmickSupportBelow(
      std::int32_t tile_x, std::int32_t tile_z,
      std::int32_t reference_height) const = 0;
  // The stand-on platform search; also records the supporting platform layer.
  virtual PlatformContactHeight PlatformContactAt(PlatformProbe probe) = 0;
  // The visible platform on a support layer.
  [[nodiscard]] virtual std::optional<PlatformContact> VisiblePlatform(
      std::int32_t support_layer) const = 0;
  // The surface id of a cell's low five bits.
  [[nodiscard]] virtual std::uint8_t TileSurfaceId(
      std::uint32_t surface) const = 0;

  // nullopt for an empty actor slot.
  [[nodiscard]] virtual std::optional<CollisionActor> Actor(
      std::size_t index) const = 0;
  // Writes back the fields the rules change for an actor in the table.
  virtual void StoreActor(const CollisionActor& actor) = 0;
  // The tile of a gimmick slot; nullopt for an empty slot.
  [[nodiscard]] virtual std::optional<GimmickTile> SupportGimmickTile(
      std::uint8_t slot) const = 0;
  // Settles a stored actor onto (or off) its platform.
  virtual void UpdatePlatformContact(std::int32_t actor_index,
                                     std::int32_t mode) = 0;

  virtual CollisionSamples& Samples() = 0;
  virtual CollisionProbe& Probe() = 0;
};

// The diagonal reach of a collision radius: radius * sin(45 degrees).
[[nodiscard]] std::int32_t CollisionDiagonalReach(std::int32_t radius);

// A sector's sampled height relative to `y` (vs_battleRelativeSampleHeight).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
[[nodiscard]] std::int32_t RelativeCollisionSampleHeight(
    const CollisionSamples& samples, std::int32_t sector, std::int32_t offset,
    std::int32_t y);

// A tile occupancy search. Mode 1 matches busy actors by their route target
// tile instead; modes 2 and up start the search at that slot.
struct OccupiedTileQuery {
  std::int32_t tile_x = 0;
  std::int32_t tile_z = 0;
  // Actors lower than this less 192 are not on the tile.
  std::int32_t minimum_height = 0;
  std::int32_t mode = 0;
};

// The first actor standing on the queried tile.
[[nodiscard]] std::optional<std::size_t> FindActorOccupyingTile(
    const ActorCollisionHost& host, const OccupiedTileQuery& query);

// Whether a moving platform moving to `point` on `layer` is obstructed by an
// actor; a hanging actor it knocks loose drops.
bool CheckPlatformActorObstruction(ActorCollisionHost& host, std::int32_t layer,
                                   MotionVector point);

// Whether a hanging actor's perimeter is blocked away from its facing.
bool IsHangingActorObstructed(ActorCollisionHost& host, CollisionActor& actor);

// Whether an actor can climb to the ledge `offset` raises it toward; resolves
// `motion` from there and checks both sides of the ledge for support.
bool CanActorHangFromLedge(ActorCollisionHost& host, CollisionActor& actor,
                           MotionVector& offset, MotionVector& motion);

// Whether both sides of the last probe position have support near its height.
bool HasActorLedgeSideSupport(ActorCollisionHost& host,
                              const CollisionActor& actor);

// The support height at an offset from `origin`; kNoSupportHeight off the
// floor (the origin's height for kShadow).
std::int32_t SampleActorSupportHeight(ActorCollisionHost& host,
                                      MotionVector origin,
                                      std::int32_t offset_x,
                                      std::int32_t offset_z,
                                      SupportSample sample);

// Samples the eight perimeter sectors at the collision radius (`diagonal` on
// the diagonals) and marks the walls.
void SampleActorCollisionPerimeter(ActorCollisionHost& host,
                                   CollisionActor& actor,
                                   std::int32_t diagonal);

// Pushes `motion` away from the walls the perimeter marked.
void ApplyActorWallRepulsion(const CollisionSamples& samples,
                             CollisionActor& actor, MotionVector& motion);

// Resolves one step of terrain motion: the whole step, one axis of it, or a
// slide along a ledge. `motion` is changed to the step taken.
TerrainMotionResult ResolveActorTerrainMotion(ActorCollisionHost& host,
                                              CollisionActor& actor,
                                              MotionVector& motion,
                                              bool probe_only);

// The climb in 64-unit steps up to the terrain at `probe`, 0 for level or
// down, or kRefusedProbe. Accepted probes take their support height in y.
std::int32_t ClassifyActorTerrainProbe(ActorCollisionHost& host,
                                       const CollisionActor& mover,
                                       MotionVector& probe,
                                       std::int32_t sector);

// Whether no other actor occupies `position` at the actor's height.
[[nodiscard]] bool IsActorPositionUnoccupied(ActorCollisionHost& host,
                                             const CollisionActor& actor,
                                             const MotionVector& position);

// Marks the actors the actor overlaps; false when `motion` moves it closer to
// one of them.
bool CanActorSeparateFromOverlaps(ActorCollisionHost& host,
                                  const CollisionActor& actor,
                                  const MotionVector& motion);

// The offset that puts a hanging actor against its ledge edge, facing it.
// False when the support platform is not visible (before anything changes) or
// when the edge has a wall.
bool CalcActorLedgeOffset(ActorCollisionHost& host, CollisionActor& actor,
                          MotionVector& offset);
}  // namespace VS::Field
