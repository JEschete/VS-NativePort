#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>
#include <span>

#include "Items/ItemRecords.hpp"

namespace VS::Battle {
struct ElementEnchantment {
  int increased_affinity = 0;
  int decreased_affinity = 0;
  std::int32_t amount = 0;
};

// Missing equipment is untouched. An equipped record requires indices in
// its eight affinity slots; refusal leaves all records unchanged.
// decomp: func_80086EA4 (src/BATTLE/BATTLE.PRG/146C.c:13801).
bool ApplyWeaponElementEnchantment(Items::WeaponLoadout& weapon,
                                   const ElementEnchantment& change);
// decomp: func_80086EF4 (src/BATTLE/BATTLE.PRG/146C.c:13811).
bool ApplyArmorElementEnchantment(
    Items::ShieldLoadout& shield,
    std::span<Items::ArmorLoadout, Items::kLimbCount> armor,
    const ElementEnchantment& change);

struct StatNumberRequest {
  int actor_slot = 0;
  int limb_tag = 0;
  std::uint32_t encoded_amount = 0;
  int label_flags = 0;
};

class StatNumberWorld {
 public:
  virtual ~StatNumberWorld() = default;
  // Nothing when the slot is invalid or has no actor.
  [[nodiscard]] virtual std::optional<std::uint8_t> RoomSpawnIndex(
      int actor_slot) const = 0;
  virtual void QueueValueLabel(const StatNumberRequest& request) = 0;
};

// decomp: func_80087578 (src/BATTLE/BATTLE.PRG/146C.c:14011).
// True queues a label, false suppresses it, nothing refuses a missing actor.
std::optional<bool> QueueActorStatChangeNumber(
    StatNumberWorld& world, const StatNumberRequest& request);

struct TargetingRiskTarget {
  int actor_slot = 0;
  std::uint32_t statuses = 0;
  bool targets_position = false;
};

class TargetingRiskWorld {
 public:
  virtual ~TargetingRiskWorld() = default;
  [[nodiscard]] virtual std::uint32_t ControlMode() const = 0;
  [[nodiscard]] virtual std::uint32_t ActionFlowMode() const = 0;
  [[nodiscard]] virtual std::int32_t TickSpeed() const = 0;
  [[nodiscard]] virtual std::optional<TargetingRiskTarget> SelectedTarget()
      const = 0;
  // The player's owner is independent of the selected target's stats.
  [[nodiscard]] virtual std::optional<std::uint16_t> PlayerRisk() const = 0;
  virtual void SetPlayerRisk(std::uint16_t risk) = 0;
};

// decomp: func_80088B8C (src/BATTLE/BATTLE.PRG/146C.c:14745).
// Only active targeting advances the signed clock. Target/player reads are
// lazy; a required missing record is refused before advancing it.
bool UpdateTargetingRiskPenalty(std::int32_t& timer, TargetingRiskWorld& world);
// decomp: func_80088CA0 (src/BATTLE/BATTLE.PRG/146C.c:14764).
void ResetTargetingRiskPenaltyTimer(std::int32_t& timer);
}  // namespace VS::Battle
