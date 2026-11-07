#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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

using InteractionTarget = std::variant<std::monostate, TrapTarget, ChestTarget>;

struct InteractionTargetDomain {
  std::uint64_t generation = 1;
  std::size_t trap_count = 0;
  std::size_t chest_count = 0;
};

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
