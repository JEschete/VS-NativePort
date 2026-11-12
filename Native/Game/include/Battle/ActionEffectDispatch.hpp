#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>

#include "Battle/ActionResults.hpp"

namespace VS::Battle {
// Implementations retain the initial stats owner, but read its fields and
// pending hit anew after each status callback.
class LimbInjuryQueue {
 public:
  virtual ~LimbInjuryQueue() = default;
  [[nodiscard]] virtual std::uint8_t MobilityLimb(std::size_t index) = 0;
  [[nodiscard]] virtual std::int16_t CurrentHp(std::uint8_t limb) = 0;
  [[nodiscard]] virtual ActionResultChange PendingHp(std::uint8_t limb) = 0;
  [[nodiscard]] virtual std::uint32_t ActiveStatuses() = 0;
  [[nodiscard]] virtual bool PlayerTarget() = 0;
  [[nodiscard]] virtual std::uint8_t InjuryRule(std::size_t limb) = 0;
  [[nodiscard]] virtual std::uint32_t RuleMask(std::uint8_t rule) = 0;
  virtual void QueueStatus(std::uint8_t rule, bool remove) = 0;
};

class ActionEffectWorld {
 public:
  virtual ~ActionEffectWorld() = default;
  [[nodiscard]] virtual bool TargetPosition() = 0;
  [[nodiscard]] virtual std::uint8_t TargetActorId() = 0;
  [[nodiscard]] virtual std::uint32_t TargetStatuses() = 0;
  [[nodiscard]] virtual std::uint8_t ActionType() = 0;
  virtual void QueueExemptDamage() = 0;
  virtual void ClearLastValue() = 0;
  virtual void Dispatch(int hit, int execute) = 0;
  virtual void QueueStatusRemoval(std::uint32_t mask) = 0;
  [[nodiscard]] virtual bool SourcePosition() = 0;
  [[nodiscard]] virtual std::uint8_t SourceSlot() = 0;
  [[nodiscard]] virtual std::int16_t TargetHp() = 0;
  [[nodiscard]] virtual std::int16_t PendingHp() = 0;
  [[nodiscard]] virtual std::uint8_t TargetSlot() = 0;
  [[nodiscard]] virtual bool KillCounted() = 0;
  virtual void MarkKillCounted() = 0;
  [[nodiscard]] virtual std::uint8_t WeaponCategory() = 0;
  [[nodiscard]] virtual std::uint32_t ProgressFlag() = 0;
  [[nodiscard]] virtual std::uint8_t LearnedArts(std::uint8_t category) = 0;
  [[nodiscard]] virtual std::uint16_t WeaponKills(std::uint8_t category) = 0;
  [[nodiscard]] virtual std::uint16_t RequiredKills(std::uint8_t category,
                                                    std::uint8_t learned) = 0;
  virtual void SetWeaponKills(std::uint8_t category, std::uint16_t kills) = 0;
};

// decomp: func_80085008 (src/BATTLE/BATTLE.PRG/146C.c:12930).
void DeriveLimbInjuryStatuses(LimbInjuryQueue& queue);
// This entry's final arguments precede the mediator's in the opposite order.
// decomp: func_80085390 (src/BATTLE/BATTLE.PRG/146C.c:13012).
void ApplyActionHitEffect(ActionEffectWorld& world, int execute, int hit);
}  // namespace VS::Battle
