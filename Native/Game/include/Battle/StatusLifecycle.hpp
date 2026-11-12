#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>

#include "Battle/StatusHelpers.hpp"
#include "Items/ItemRecords.hpp"

namespace VS::Battle {
inline constexpr std::size_t kStatusTimerCount = 8;
inline constexpr std::size_t kPlayerLimbStatusCount = 4;
using LimbStatusMasks = std::array<std::uint32_t, kPlayerLimbStatusCount>;
struct StatusAttributes {
  std::uint16_t current{};
  std::uint16_t total{};
  friend bool operator==(const StatusAttributes&,
                         const StatusAttributes&) = default;
};

struct StatusLimb {
  std::int16_t hp{};
  std::int16_t max_hp{};
  std::uint8_t name_index{};
  Items::ArmorLoadout armor{};
  friend bool operator==(const StatusLimb&, const StatusLimb&) = default;
};

struct StatusLifecycleState {
  std::uint32_t statuses{};
  std::array<std::uint8_t, kStatusTimerCount> timers{};
  std::int16_t hp{};
  std::int16_t max_hp{};
  StatusAttributes strength{};
  StatusAttributes intelligence{};
  StatusAttributes agility{};
  std::uint8_t base_walk{};
  std::uint8_t base_run{};
  std::uint8_t walk{};
  std::uint8_t run{};
  std::array<std::uint8_t, 2> mobility_limbs{};
  std::uint8_t actor_id{};
  bool analyzed{};
  Items::WeaponLoadout weapon{};
  Items::ShieldLoadout shield{};
  std::array<StatusLimb, Items::kLimbCount> limbs{};
  friend bool operator==(const StatusLifecycleState&,
                         const StatusLifecycleState&) = default;
};

struct StatusDurations {
  std::uint8_t attributes{};
  std::uint8_t movement{};
  std::uint8_t equipment{};
  std::uint8_t paralysis{};
  std::uint8_t poison_interval{};
  std::uint8_t regeneration_interval{};
  friend bool operator==(const StatusDurations&,
                         const StatusDurations&) = default;
};

// Calls observe all preceding writes. A binding must reload the state after
// an external callback, retaining mutations made by that callback.
class StatusLifecycleWorld {
 public:
  virtual ~StatusLifecycleWorld() = default;
  virtual std::int32_t RandomBounded(StatusLifecycleState& state,
                                     int bound) = 0;
  virtual std::int32_t RandomAveraged(StatusLifecycleState& state,
                                      int bound) = 0;
  virtual void UpdateMovement(StatusLifecycleState& state) = 0;
  virtual void EquipmentLabel(StatusLifecycleState& state, int label, int stat,
                              std::uint32_t amount) = 0;
  virtual void ValueLabel(StatusLifecycleState& state,
                          std::uint32_t amount) = 0;
  virtual void StatusLabels(StatusLifecycleState& state,
                            std::uint32_t statuses) = 0;
  virtual void RebuildWeapon(StatusLifecycleState& state) = 0;
  virtual void RebuildShield(StatusLifecycleState& state) = 0;
  virtual void RebuildArmor(StatusLifecycleState& state, std::size_t limb) = 0;
  virtual void EnchantWeapon(StatusLifecycleState& state,
                             const ElementEnchantment& change) = 0;
  virtual void EnchantArmor(StatusLifecycleState& state,
                            const ElementEnchantment& change) = 0;
  virtual void RefreshEquipmentDisplay(StatusLifecycleState& state) = 0;
};

// Only mobility indices actually dereferenced must be below the limb count;
// zero means no limb. Refusal precedes the operation's writes.
// decomp: func_8007C31C (src/BATTLE/BATTLE.PRG/146C.c:8736).
bool PreserveAshleyStatuses(StatusLifecycleState& state,
                            StatusLifecycleWorld& world);
// decomp: func_80086754 (src/BATTLE/BATTLE.PRG/146C.c:13532).
bool RemoveActorStatuses(StatusLifecycleState& state, std::uint32_t requested,
                         StatusLifecycleWorld& world);
// decomp: func_80086FA8 (src/BATTLE/BATTLE.PRG/146C.c:13832).
void InflictActorStatuses(StatusLifecycleState& state, std::uint32_t requested,
                          const StatusDurations& durations,
                          StatusLifecycleWorld& world);
// decomp: func_80087EF4 (src/BATTLE/BATTLE.PRG/146C.c:14376).
bool UpdateActorStatusTimers(StatusLifecycleState& state,
                             const StatusDurations& durations,
                             const LimbStatusMasks& limb_statuses,
                             StatusLifecycleWorld& world);
}  // namespace VS::Battle
