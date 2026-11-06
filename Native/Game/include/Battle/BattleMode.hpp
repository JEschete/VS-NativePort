#pragma once
// Hand-maintained (tools/port/owned.csv).

// Typed battle mode (#276 row 31): drawing and sheathing the player's weapon
// and switching an enemy between its calm and battle-ready kinds. The actor
// records, the weapon pose and the HUD go through a BattleModeHost.
namespace VS::Battle {
// The battle mode a caller asks for.
inline constexpr int kSheatheWeapon = 0;
inline constexpr int kDrawWeapon = 1;
inline constexpr int kToggleWeapon = 2;

class BattleModeHost {
 public:
  virtual ~BattleModeHost() = default;

  // The actor's weaponDrawn word; bit 0 is the drawn state.
  [[nodiscard]] virtual int ActorWeaponDrawn(int actor_slot) const = 0;
  virtual void SetActorWeaponDrawn(int actor_slot, int weapon_drawn) = 0;
  [[nodiscard]] virtual int ActorKind(int actor_slot) const = 0;
  virtual void SetActorKind(int actor_slot, int kind) = 0;
  // Moves the actor's weapon to the drawn or sheathed pose.
  virtual void ShowWeaponDrawn(int actor_slot, int weapon_drawn,
                               int transition_mode) = 0;
  // The player character's weaponDrawn word.
  [[nodiscard]] virtual int CharacterWeaponDrawn() const = 0;
  virtual void SetHudBattleMode(int weapon_drawn) = 0;
};

[[nodiscard]] bool IsActorWeaponDrawn(int actor_slot,
                                      const BattleModeHost& host);
void SetActorBattleMode(int actor_slot, int battle_mode, BattleModeHost& host);
}  // namespace VS::Battle
