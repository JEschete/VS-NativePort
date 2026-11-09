#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace VS::Field {

struct TrapTarget {
  std::uint32_t index = 0;
  std::uint64_t generation = 0;
  friend bool operator==(const TrapTarget&, const TrapTarget&) = default;
};

struct ChestTarget {
  std::uint32_t index = 0;
  std::uint64_t generation = 0;
  friend bool operator==(const ChestTarget&, const ChestTarget&) = default;
};

struct RoomObjectTarget {
  std::uint32_t index = 0;
  std::uint64_t generation = 0;
  friend bool operator==(const RoomObjectTarget&,
                         const RoomObjectTarget&) = default;
};

using InteractionTarget = std::variant<std::monostate, TrapTarget, ChestTarget>;

struct InteractionTargetDomain {
  std::uint64_t generation = 1;
  std::size_t trap_count = 0;
  std::size_t chest_count = 0;
};

struct RoomTriggerInfo {
  TrapTarget target{};
  std::int16_t kind = 0;
  std::int16_t tile_x = 0;
  std::int16_t tile_z = 0;
  std::int16_t floor_layer = 0;
};

struct RoomTriggerTile {
  std::int32_t x = 0;
  std::int32_t z = 0;
  std::int32_t layer = 0;
};

std::optional<TrapTarget> FindRoomTriggerAtTile(
    std::span<const RoomTriggerInfo> records, const RoomTriggerTile& tile,
    bool suppress_panels);

class RoomTriggerActivationHost {
 public:
  virtual ~RoomTriggerActivationHost() = default;
  virtual InteractionTargetDomain Domain() = 0;
  virtual bool StoreActivation(TrapTarget target, std::uint8_t activation) = 0;
};

bool SetRoomTriggerEnabled(std::optional<TrapTarget> target,
                           std::int32_t activation,
                           RoomTriggerActivationHost& host);

struct TrapWorldPosition {
  std::int16_t x = 0;
  std::int16_t z = 0;
};

class TrapDisableHost : public RoomTriggerActivationHost {
 public:
  virtual std::optional<TrapTarget> Find(const RoomTriggerTile& tile) = 0;
  virtual std::optional<RoomTriggerTile> Tile(TrapTarget target) = 0;
  virtual bool MarkObstacle(const RoomTriggerTile& tile) = 0;
};

bool DisableTrapAtPosition(TrapWorldPosition position, TrapDisableHost& host);

inline std::optional<std::size_t> ResolveTrapTarget(
    TrapTarget target, InteractionTargetDomain domain) {
  if (target.generation != domain.generation ||
      target.index >= domain.trap_count) {
    return std::nullopt;
  }
  return target.index;
}

inline std::optional<std::size_t> ResolveChestTarget(
    ChestTarget target, InteractionTargetDomain domain) {
  if (target.generation != domain.generation ||
      target.index >= domain.chest_count) {
    return std::nullopt;
  }
  return target.index;
}

class InteractionTargetState {
 public:
  void Select(TrapTarget target) { target_ = target; }
  void Select(ChestTarget target) { target_ = target; }
  void Clear() { target_ = std::monostate{}; }
  const TrapTarget* Trap() const { return std::get_if<TrapTarget>(&target_); }
  const ChestTarget* Chest() const {
    return std::get_if<ChestTarget>(&target_);
  }
  const InteractionTarget& Target() const { return target_; }
  const TrapTarget* Trap(InteractionTargetDomain domain) const {
    const auto* target = Trap();
    return target != nullptr && ResolveTrapTarget(*target, domain) ? target
                                                                   : nullptr;
  }
  const ChestTarget* Chest(InteractionTargetDomain domain) const {
    const auto* target = Chest();
    return target != nullptr && ResolveChestTarget(*target, domain) ? target
                                                                    : nullptr;
  }

 private:
  InteractionTarget target_;
};

}  // namespace VS::Field
