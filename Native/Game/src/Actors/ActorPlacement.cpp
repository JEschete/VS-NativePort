// Hand-maintained (tools/port/owned.csv).
// The actor placement rules (BATTLE.PRG 30DB0.c:4182 and 3A1A0.c:274-500).
#include "Actors/ActorPlacement.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

#include "Actors/ActorMotion.hpp"
#include "Actors/ActorMovement.hpp"
#include "Actors/ActorStateRecord.hpp"
#include "Actors/MotionRequests.hpp"
#include "Core/Log.hpp"
#include "Field/PlatformPlacement.hpp"

namespace VS::Actors {
namespace {
constexpr int kTileCenter = kTileUnits / 2;
// How far above the floor a flyer starts: the open-room default, and the
// least clearance below a ceiling (offsets above it take this instead).
constexpr int kOpenRoomFlightOffset = -0x180;
constexpr int kLeastFlightClearance = -0x80;
constexpr int kLeastClearanceLimit = -0x7F;
constexpr int kFlightAnimation = 8;
constexpr int kStandAnimation = 1;
constexpr std::uint8_t kModeAirborne = 3;
constexpr std::uint8_t kFourBits = 0xF;
// Placement facings are in sixteenths of the 4096-unit turn.
constexpr int kFacingUnit = 16;
// Flyers stack a third of a tile and a half apart, or half that on a retry.
constexpr int kFlightStackStep = 0xC0;
constexpr int kFlightRetryStep = 0x60;
// Only earlier room actors (slot 2 up) can already hold a flight position.
constexpr int kFirstFlyerSlot = 2;
// Each probe misses at least once per stacked flyer, and retries once.
constexpr int kMaxSeparationProbes = 2 * (kActorStateSlots + 1);
constexpr int kQuarterTurn = kAngleOne / 4;

bool InActorTable(int id) { return id >= 0 && id < kActorStateSlots; }

ActorStateRecord* KnownRecord(ActorStateHost& host, int id,
                              const char* operation) {
  if (!InActorTable(id)) {
    VS_LOG_WARN("actors", "{} refused for actor {} outside the actor table",
                operation, id);
    return nullptr;
  }
  return host.Record(id);
}

std::int16_t TileCenter(std::uint8_t tile) {
  return static_cast<std::int16_t>(tile * kTileUnits + kTileCenter);
}

void PlaceFlyer(ActorStateHost& host, int id, ActorStateRecord& record) {
  MotionVector& position = record.motion.position;
  const int ceiling = host.NavigationCeilingHeight();
  int offset = kOpenRoomFlightOffset;
  if (ceiling != 0) {
    offset = ceiling - position.y;
    if (offset >= kLeastClearanceLimit) offset = kLeastFlightClearance;
  }
  position.y = static_cast<std::int16_t>(position.y + offset);
  if (FindActorAtFlightPosition(host, id, position).has_value()) {
    SeparateActorFlightPlacement(host, id, position);
  }
  record.motion.motion_locked = true;
  host.PlayAnimation(id, kFlightAnimation, 0);
}

void PlaceOnPlatform(ActorStateHost& host, int id, ActorStateRecord& record,
                     std::uint8_t layer) {
  const std::optional<Field::PlatformContact> contact =
      host.MovingPlatformContact(layer);
  if (!contact.has_value()) {
    VS_LOG_WARN("actors",
                "Actor {} is placed on missing moving platform layer {}; "
                "keeping it on the floor",
                id, layer);
    return;
  }
  record.motion.position = {.x = contact->x,
                            .y = contact->height,
                            .z = contact->z,
                            .w = contact->platform_index};
  record.platform_anchor = {
      .x = contact->x, .y = contact->height, .z = contact->z, .w = layer};
  record.support_layer = static_cast<std::uint8_t>(layer & kFourBits);
  record.placement_layer = static_cast<std::uint8_t>(layer & kFourBits);
}

void PlaceOnLayer(ActorStateHost& host, int id, ActorStateRecord& record,
                  int drop_height) {
  ActorMotion& motion = record.motion;
  const std::uint8_t layer = motion.current_tile.layer;
  if (layer == kDropPlacementLayer) {
    motion.current_tile.layer = 0;
    motion.position.y = static_cast<std::int16_t>(drop_height);
    motion.root_motion.y = 0;
    motion.movement_mode = kModeAirborne;
    return;
  }
  if (layer >= kFirstPlatformLayer) PlaceOnPlatform(host, id, record, layer);
  host.PlayAnimation(id, kStandAnimation, 0);
}

void FinishPlacement(ActorStateHost& host, int id, ActorStateRecord& record,
                     std::uint8_t facing) {
  host.SaveGroundedTransform(id);
  ActorMotion& motion = record.motion;
  motion.move_frames = 0;
  motion.facing_delta = 0;
  motion.turn_frames = 0;
  record.pending_target_x = 0;
  record.pending_target_z = 0;
  record.previous_target_x = 0;
  record.previous_target_z = 0;
  motion.facing = static_cast<std::int16_t>(facing * kFacingUnit);
  ResetActorRunMomentum(motion);
  record.contact_kind = 0;
}

std::int64_t SquaredDistance(int dx, int dz) {
  return static_cast<std::int64_t>(dx) * dx +
         static_cast<std::int64_t>(dz) * dz;
}
}  // namespace

bool IsFlightClass(int movement_class) {
  return movement_class == kFlyingClass || movement_class == kHoveringClass;
}

std::optional<ActorPlacementView> ActorPlacement(ActorStateHost& host, int id) {
  if (!InActorTable(id)) {
    VS_LOG_WARN("actors", "No placement for actor {} outside the actor table",
                id);
    return std::nullopt;
  }
  if (const ActorStateRecord* record = host.Record(id)) {
    MotionVector position = record->motion.position;
    position.w = record->motion.facing;
    return ActorPlacementView{.tile = record->motion.current_tile,
                              .position = position};
  }
  const std::optional<GimmickView> gimmick = host.Gimmick(id);
  if (!gimmick.has_value()) return std::nullopt;
  MotionVector position = gimmick->position;
  position.w = static_cast<std::int16_t>(gimmick->facing);
  return ActorPlacementView{.tile = gimmick->tile, .position = position};
}

void PlaceActorInRoom(ActorStateHost& host, int id, PackedTile tile,
                      int drop_height, int movement_class) {
  ActorStateRecord* record = KnownRecord(host, id, "Room placement");
  if (record == nullptr) {
    VS_LOG_WARN("actors", "No actor {} to place in the room", id);
    return;
  }
  ActorMotion& motion = record->motion;
  motion.current_tile = tile;
  motion.position.x = TileCenter(tile.x);
  motion.position.z = TileCenter(tile.z);
  motion.position.y = static_cast<std::int16_t>(
      host.SupportHeight(motion.position.x, motion.position.z));
  if (IsFlightClass(movement_class)) {
    PlaceFlyer(host, id, *record);
  } else {
    if (movement_class == kLeapingClass || movement_class == kBoundingClass) {
      motion.is_jumping = true;
    }
    PlaceOnLayer(host, id, *record, drop_height);
  }
  FinishPlacement(host, id, *record, tile.spare);
}

std::optional<int> FindActorAtFlightPosition(ActorStateHost& host, int id,
                                             const MotionVector& position) {
  if (id > kActorStateSlots) {
    VS_LOG_WARN("actors", "Flight placement of actor {} checks only the table",
                id);
  }
  const int end = std::min(id, kActorStateSlots);
  for (int other_id = kFirstFlyerSlot; other_id < end; ++other_id) {
    const ActorStateRecord* other = host.Record(other_id);
    if (other == nullptr || !other->motion.motion_locked) continue;
    const MotionVector& at = other->motion.position;
    if (at.x == position.x && at.y == position.y && at.z == position.z) {
      return other_id;
    }
  }
  return std::nullopt;
}

void SeparateActorFlightPlacement(ActorStateHost& host, int id,
                                  MotionVector& position) {
  const ActorStateRecord* record = KnownRecord(host, id, "Flight separation");
  if (record == nullptr) return;
  MotionVector probe = position;
  probe.y = static_cast<std::int16_t>(position.y - kFlightStackStep);
  const int ceiling = host.NavigationCeilingHeight();
  bool retried = false;
  for (int step = 0; step < kMaxSeparationProbes; ++step) {
    if (FindActorAtFlightPosition(host, id, probe).has_value()) {
      probe.y = static_cast<std::int16_t>(probe.y - kFlightStackStep);
    } else if (ceiling != 0 && probe.y - record->collision_height < ceiling) {
      if (retried) return;
      retried = true;
      probe.y = static_cast<std::int16_t>(position.y - kFlightRetryStep);
    } else {
      position.y = probe.y;
      return;
    }
  }
  VS_LOG_WARN("actors", "Flight separation of actor {} found no free height",
              id);
}

std::optional<PackedTile> ActorSavedPlacement(ActorStateHost& host, int id,
                                              int movement_class) {
  const ActorStateRecord* record = host.Record(id);
  if (record == nullptr) return std::nullopt;
  PackedTile tile = record->saved_placement;
  if (record->motion.motion_locked && IsFlightClass(movement_class)) {
    tile = record->motion.current_tile;
  }
  tile.spare = static_cast<std::uint8_t>(record->motion.facing / kFacingUnit);
  return tile;
}

std::optional<int> FindNearestActorAhead(ActorStateHost& host, int id,
                                         AheadRange range) {
  const ActorStateRecord* self = KnownRecord(host, id, "Nearest actor");
  if (self == nullptr) return std::nullopt;
  const MotionVector& origin = self->motion.position;
  const std::int64_t reach = SquaredDistance(range.horizontal, 0);
  std::optional<int> best;
  std::int64_t best_distance = 0;
  for (int other_id = 0; other_id < kActorStateSlots; ++other_id) {
    if (other_id == id) continue;
    const ActorStateRecord* other = host.Record(other_id);
    if (other == nullptr || other->skipped) continue;
    const MotionVector& at = other->motion.position;
    const int dy = at.y - origin.y;
    if (dy < -range.vertical || dy > range.vertical) continue;
    const std::int64_t distance =
        SquaredDistance(at.x - origin.x, at.z - origin.z);
    if (distance > reach) continue;
    const int angle = ActorFacingDeltaToPoint(self->motion, &at);
    if (angle < -kQuarterTurn || angle > kQuarterTurn) continue;
    if (!best.has_value() || distance < best_distance) {
      best = other_id;
      best_distance = distance;
    }
  }
  return best;
}
}  // namespace VS::Actors
