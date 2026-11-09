#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ActorMovement.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {

inline constexpr std::size_t kGimmickQuerySlots = 16;
inline constexpr std::uint8_t kUnattachedGimmick = 0xFF;
inline constexpr std::size_t kGimmickOrientationCount = 24;
inline constexpr std::size_t kGimmickRollColumns = 4;

struct GimmickOrientationRow {
  std::uint8_t quarter_x = 0;
  std::uint8_t quarter_y = 0;
  std::uint8_t quarter_z = 0;
  std::uint8_t reserved = 0;
};

class GimmickOrientationHost {
 public:
  virtual ~GimmickOrientationHost() = default;
  virtual std::optional<std::uint8_t> ObjectKind() const = 0;
  virtual bool StoreMaterial(std::uint8_t material) = 0;
  virtual bool StoreRotation(const Core::Math::Mat3& rotation) = 0;
  virtual bool ResetRotation() = 0;
};

bool SetGimmickOrientation(std::int32_t orientation,
                           std::span<const GimmickOrientationRow> rotations,
                           GimmickOrientationHost& host);

struct GimmickQueryState {
  bool present = false;
  bool skipped = false;
  bool falling = false;
  std::int16_t movement_mode = 0;
  std::uint8_t parent = kUnattachedGimmick;
  std::uint8_t carrier = kUnattachedGimmick;
  std::uint32_t tile_word = 0;
  std::int16_t position_y = 0;
  std::uint8_t object_id = 0;
  std::uint8_t subtype = 0;
  std::uint8_t motion_state = 0;
  std::uint8_t interaction = 0;
};

struct GimmickTileQuery {
  std::int32_t tile_x = 0;
  std::int32_t tile_z = 0;
};

struct GimmickPlacementPoint {
  std::int32_t x = 0;
  std::int32_t z = 0;
  std::int32_t minimum_height = 0;
};

struct GimmickOccupancyQuery {
  GimmickTileQuery tile;
  std::int32_t minimum_height = 0;
};

class GimmickPointHost {
 public:
  virtual ~GimmickPointHost() = default;
  virtual std::int32_t SupportHeight(const GimmickPlacementPoint& point) = 0;
  virtual std::uint32_t LastProbeWord() const = 0;
  virtual bool ActorOccupies(const GimmickOccupancyQuery& query) = 0;
};

bool CanPlaceGimmickAtPoint(const GimmickPlacementPoint& point,
                            GimmickPointHost& host);

struct GimmickStackState {
  GimmickQueryState query;
  std::int16_t velocity_y = 0;
};

struct GimmickSupportedActor {
  std::size_t table_index = 0;
  Actors::MovementActor state;
};

class GimmickStackDropHost {
 public:
  virtual ~GimmickStackDropHost() = default;
  virtual std::optional<GimmickStackState> Gimmick(std::size_t slot) = 0;
  virtual bool StoreGimmick(std::size_t slot,
                            const GimmickStackState& state) = 0;
  virtual bool SameGimmick(std::size_t first, std::size_t second) const = 0;
  virtual bool RemoveOccupant(std::size_t slot) = 0;
  virtual std::optional<GimmickSupportedActor> ActorAbove(
      const GimmickQueryState& support, std::int32_t first_actor) = 0;
  virtual bool StoreActor(const GimmickSupportedActor& actor) = 0;
};

bool DropUnsupportedGimmickStack(std::int32_t slot, GimmickStackDropHost& host);

struct GimmickWorldPoint {
  std::int32_t x = 0;
  std::int32_t z = 0;
};

class GimmickInteractionQueryHost {
 public:
  virtual ~GimmickInteractionQueryHost() = default;
  virtual std::optional<Actors::MovementActor> Actor(std::int32_t slot) = 0;
  virtual bool StoreDirection(std::uint8_t direction) = 0;
  virtual std::optional<GimmickQueryState> Gimmick(std::size_t slot) = 0;
  virtual std::optional<std::size_t> TopAt(const GimmickTileQuery& tile) = 0;
  virtual bool PointAllowed(const GimmickPlacementPoint& point) = 0;
  virtual std::int32_t CeilingHeight(const GimmickWorldPoint& point) = 0;
  virtual bool ActorAbove(const GimmickQueryState& support) = 0;
};

std::optional<std::size_t> FindInteractableGimmickAhead(
    std::int32_t actor_slot, GimmickInteractionQueryHost& host);

struct GimmickInteractionObject {
  GimmickQueryState query;
  std::int16_t position_x = 0;
  std::int16_t position_z = 0;
  std::int16_t slide_x = 0;
  std::int16_t slide_z = 0;
};

class GimmickInteractionStartHost {
 public:
  virtual ~GimmickInteractionStartHost() = default;
  virtual std::optional<Actors::MovementActor> Actor(std::int32_t slot) = 0;
  virtual std::optional<std::size_t> SelectAhead(std::int32_t slot) = 0;
  virtual std::optional<std::int32_t> Direction() = 0;
  virtual std::optional<GimmickInteractionObject> Object(std::size_t slot) = 0;
  virtual bool StoreOrientation(std::size_t slot, std::uint16_t direction) = 0;
  virtual bool PublishSelected(std::size_t slot) = 0;
  virtual bool ClearSlide(std::size_t slot) = 0;
  virtual std::optional<std::uint8_t> Material(std::size_t slot) = 0;
  virtual std::span<const std::uint8_t> RollOrientations() = 0;
  virtual bool StoreMaterial(std::size_t slot, std::uint8_t material) = 0;
  virtual bool StartSlide(std::size_t slot, std::int16_t x, std::int16_t z) = 0;
  virtual bool PlaySounds(std::int32_t actor_slot, std::uint16_t first,
                          std::uint16_t second) = 0;
  virtual bool PlayAnimation(std::int32_t actor_slot,
                             std::int32_t animation) = 0;
  virtual bool SetWeaponPose(std::int32_t actor_slot) = 0;
  virtual bool ReserveTile(const GimmickTileQuery& tile) = 0;
  virtual bool TurnToward(std::int32_t actor_slot,
                          const Actors::PackedTile& tile) = 0;
  virtual bool TranslateToward(std::int32_t actor_slot,
                               const Actors::MotionVector& point) = 0;
};

bool BeginGimmickInteraction(std::int32_t actor_slot,
                             GimmickInteractionStartHost& host);

struct GimmickCommandObject {
  GimmickInteractionObject state;
  std::uint16_t yaw = 0;
};

class GimmickInteractionCommandHost {
 public:
  virtual ~GimmickInteractionCommandHost() = default;
  virtual std::optional<Actors::MovementActor> Actor() = 0;
  virtual std::optional<GimmickCommandObject> Object() = 0;
  virtual std::optional<std::int32_t> Direction() = 0;
  virtual bool PreparePush(std::optional<std::int16_t> mode) = 0;
  virtual bool RemoveOccupant() = 0;
  virtual bool ReleaseTile(const GimmickTileQuery& tile) = 0;
  virtual bool StoreDropPlacement(const GimmickTileQuery& tile,
                                  std::uint16_t yaw) = 0;
  virtual std::optional<std::int16_t> ParentBoneHeight(std::uint8_t tag) = 0;
  virtual bool FinishDrop(std::int16_t height) = 0;
  virtual bool SetCarrying(bool carrying) = 0;
  virtual bool StoreSlide(std::int16_t x, std::int16_t z) = 0;
  virtual bool StoreYaw(std::uint16_t yaw) = 0;
  virtual bool AttachObject(std::uint8_t actor_slot) = 0;
  virtual bool PlayIdleAnimation() = 0;
};

bool ApplyGimmickInteractionCommand(std::int32_t mode,
                                    GimmickInteractionCommandHost& host);

std::optional<std::size_t> FindGimmickBelow(
    std::span<const GimmickQueryState> gimmicks,
    const GimmickQueryState& reference);
std::optional<std::size_t> FindTopGimmickAtTile(
    std::span<const GimmickQueryState> gimmicks, const GimmickTileQuery& query,
    bool require_idle_mode);

std::optional<std::uint8_t> GimmickSubtype(
    std::span<const GimmickQueryState> gimmicks, std::int32_t slot);
bool AreGimmicksMoving(std::span<const GimmickQueryState> gimmicks);

struct GimmickStackMeasure {
  std::int32_t height_offset = 0;
  bool interaction_two = false;

  friend bool operator==(const GimmickStackMeasure&,
                         const GimmickStackMeasure&) = default;
};

GimmickStackMeasure MeasureTileStack(
    std::span<const GimmickQueryState> gimmicks, const GimmickTileQuery& query);
bool IsGimmickWithinHeightBand(std::span<const GimmickQueryState> gimmicks,
                               const GimmickTileQuery& query,
                               std::int32_t minimum_height);

struct GimmickPlacementState {
  bool attached = false;
  bool falling = false;
  std::int16_t movement_mode = 0;
  std::uint8_t tile_x = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t orientation = 0;
  std::int16_t position_x = 0;
  std::int16_t position_y = 0;
  std::int16_t position_z = 0;
  std::int16_t position_tail = 0;
};

struct GimmickPlacementSnapshot {
  std::uint8_t tile_x = 0;
  std::uint8_t height_above_ground = 0;
  std::uint8_t tile_z = 0;
  std::uint8_t orientation = 0;
  std::int32_t position_xy = 0;
  std::int32_t position_z_tail = 0;

  friend bool operator==(const GimmickPlacementSnapshot&,
                         const GimmickPlacementSnapshot&) = default;
};

class GimmickPlacementHost {
 public:
  virtual ~GimmickPlacementHost() = default;
  virtual std::size_t GimmickSlots() const = 0;
  virtual std::optional<GimmickPlacementState> Gimmick(
      std::size_t slot) const = 0;
  virtual std::int32_t SupportHeight(const GimmickPlacementState& state) = 0;
};

std::optional<GimmickPlacementSnapshot> GimmickPlacement(
    std::int32_t slot, GimmickPlacementHost& host);

}  // namespace VS::Field
