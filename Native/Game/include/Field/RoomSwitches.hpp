#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Field/GimmickAnimationQueue.hpp"

namespace VS::Field {

struct RoomDoorPlacement {
  std::int32_t tile_x = 0;
  std::int32_t tile_z = 0;
  std::int32_t floor_layer = 0;
  std::int16_t kind = 0;
  std::uint8_t activation = 0;
};

struct RoomDoorTileQuery {
  std::int32_t tile_x = 0;
  std::int32_t tile_z = 0;
  std::int32_t floor_layer = 0;
};

std::optional<std::size_t> InteractiveDoorAtTile(
    std::span<const RoomDoorPlacement> doors, const RoomDoorTileQuery& query);

class RoomDoorCollisionHost {
 public:
  virtual ~RoomDoorCollisionHost() = default;
  virtual std::optional<RoomDoorPlacement> State() const = 0;
  virtual void ToggleCell(const RoomDoorTileQuery& query,
                          std::uint8_t xor_mask) = 0;
};

bool ToggleDoorCollision(RoomDoorCollisionHost& host);

enum class DoorMember { kPrimary, kLinked };

struct RoomDoorAnimationState {
  RoomDoorPlacement placement{};
  std::uint16_t key_or_sigil = 0;
  std::int16_t open_value = 0;
  std::int16_t linked_index = -1;
  std::uint8_t effect = 0;
  std::uint8_t busy = 0;
  std::uint8_t delay = 0;
};

class RoomDoorAnimationHost {
 public:
  virtual ~RoomDoorAnimationHost() = default;
  virtual std::optional<RoomDoorAnimationState> State(
      DoorMember member) const = 0;
  virtual bool QueueAvailable() const = 0;
  virtual void PlaySound(std::int32_t sound) = 0;
  virtual void StartEffect(std::uint8_t effect) = 0;
  virtual bool EffectActive(std::uint8_t effect) const = 0;
  virtual bool ToggleCollision(DoorMember member) = 0;
  virtual bool StoreBusy(DoorMember member, std::uint8_t busy) = 0;
  virtual bool StoreDelay(std::uint8_t delay) = 0;
  virtual bool StoreOpen(DoorMember member, std::int16_t open_value,
                         std::uint8_t busy) = 0;
  virtual bool Queue(GimmickAnimationKind kind) = 0;
  virtual void MarkSigil(std::uint32_t sigil_index) = 0;
};

bool BeginDoorToggle(RoomDoorAnimationHost& host);
bool StartDoorAnimation(RoomDoorAnimationHost& host);
bool FinishDoorAnimation(RoomDoorAnimationHost& host);

struct SigilDoorEndpoint {
  std::uint8_t zone_id = 0;
  std::uint8_t map_id = 0;
};

struct SigilDoorRoomPair {
  SigilDoorEndpoint first{};
  SigilDoorEndpoint second{};
};

struct SigilDoorTables {
  std::span<const std::int16_t> index_ranges;
  std::span<const SigilDoorRoomPair> room_pairs;
};

struct SigilDoorRoomQuery {
  std::int32_t zone_id = 0;
  std::int32_t map_id = 0;
  std::uint32_t sigil_index = 0;
};

bool MarkSigilDoorUnlocked(const SigilDoorTables& tables,
                           const SigilDoorRoomQuery& query,
                           std::span<std::byte> flags);
std::uint8_t RoomSigilDoorUnlockByte(const SigilDoorTables& tables,
                                     const SigilDoorRoomQuery& query,
                                     std::span<const std::byte> flags);

struct RoomSwitchState {
  std::uint16_t switch_value = 0;
  std::uint8_t marker_fade = 0;
  std::uint8_t activation = 0;
  std::int32_t effect_kind = 0;
  std::int32_t tile_x = 0;
  std::int32_t tile_z = 0;
  std::int32_t floor_layer = 0;
};

class RoomSwitchHost {
 public:
  virtual ~RoomSwitchHost() = default;
  virtual std::optional<RoomSwitchState> State() const = 0;
  virtual void PlaySound(std::int32_t sound) = 0;
  virtual void StartEffect(std::int32_t kind) = 0;
  virtual void StoreSwitch(std::uint16_t value, std::uint8_t marker_fade) = 0;
  virtual void SetCellPassability(const RoomSwitchState& state,
                                  std::uint8_t passability) = 0;
};

bool ToggleRoomSwitch(RoomSwitchHost& host);

class RoomSwitchStartHost {
 public:
  virtual ~RoomSwitchStartHost() = default;
  virtual std::optional<RoomSwitchState> State() const = 0;
  virtual bool StoreStartState(std::uint8_t marker,
                               std::uint8_t activation) = 0;
  virtual bool QueueToggle() = 0;
  virtual bool ToggleImmediately() = 0;
};

bool BeginSwitchToggle(RoomSwitchStartHost& host);

}  // namespace VS::Field
