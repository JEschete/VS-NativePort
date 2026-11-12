#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "Items/ItemRecords.hpp"

namespace VS::Battle {
struct DamageHit {
  std::uint8_t type{};
  std::uint8_t affinity{};
  std::uint8_t factor{};
};
struct DamageTarget {
  std::uint8_t actor{};
  std::int8_t limb{};
};
struct DamageStats {
  std::uint8_t actor_id{};
  std::uint8_t enemy_class{};
  std::uint16_t strength{};
  std::uint16_t intelligence{};
  std::uint16_t risk{};
  Items::WeaponLoadout weapon{};
  Items::ShieldLoadout shield{};
  Items::AccessorySpawnLoadout accessory{};
};
struct DamageLimb {
  std::array<std::int16_t, Items::kPartTypeSlots> types{};
  Items::AffinityTotals affinities{};
  Items::ArmorLoadout armor{};
};
class DamageOwner {
 public:
  virtual ~DamageOwner() = default;
  virtual DamageStats Read() = 0;
  virtual std::optional<DamageLimb> Limb(int limb) = 0;
  virtual std::uint32_t Statuses() = 0;
  virtual std::uint16_t Risk() = 0;
  virtual std::uint8_t EnemyClass() = 0;
  virtual void GrowAttacker(int target_class, int target_affinity) = 0;
  virtual void GrowDefender(int source_class, int source_affinity,
                            int limb) = 0;
};
class DamageWorld {
 public:
  virtual ~DamageWorld() = default;
  // Handles retain the resolved stats owner, even if a callback changes the
  // table.
  virtual DamageOwner* Stats(std::uint8_t slot) = 0;
  virtual std::optional<std::uint32_t> WeaponDrawn(std::uint8_t slot) = 0;
  virtual std::optional<DamageHit> Hit(int hit) = 0;
  virtual std::optional<std::uint8_t> ActionType() = 0;
  virtual std::optional<std::uint8_t> ActionId() = 0;
  virtual std::optional<std::uint16_t> BladeBonus(std::uint8_t category) = 0;
  virtual std::int32_t TargetPosition() = 0;
  virtual std::int32_t RandomBounded(int bound) = 0;
  virtual std::int32_t RandomAveraged(int bound) = 0;
  virtual void SetCriticalHit() = 0;
};
std::optional<std::int16_t> CalculatePhysicalHitDamage(
    const DamageTarget* source, const DamageTarget* target, int hit, int random,
    bool apply_statuses, DamageWorld& world);
std::optional<std::int16_t> PhysicalDamageWithoutDebuffs(
    const DamageTarget* source, const DamageTarget* target, int hit, int random,
    DamageWorld& world);
std::optional<std::int16_t> CalculateSpellHitDamage(const DamageTarget* source,
                                                    const DamageTarget* target,
                                                    int hit, int random,
                                                    DamageWorld& world);
// Refuses encoded zero (index 255), and drawn encoded one (original unwritten
// stack halfword, #351). Undrawn encoded one is defined and remains supported.
std::optional<std::int16_t> CalculateSpellHealing(const DamageTarget* source,
                                                  const DamageTarget* target,
                                                  int hit, int random,
                                                  DamageWorld& world);
std::optional<std::int16_t> PhysicalDamageWithDebuffs(
    const DamageTarget* source, const DamageTarget* target, int hit, int random,
    DamageWorld& world);
}  // namespace VS::Battle
