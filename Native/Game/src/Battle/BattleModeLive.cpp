// Hand-maintained (tools/port/owned.csv).
// Binds the typed battle mode (BattleMode.cpp) to the battle actor table, the
// weapon pose and the HUD.
#include "Battle/BattleModeLive.hpp"

#include <cstddef>
#include <iterator>

#include "Actors/AnimationControlLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/Log.hpp"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Hud.h"
}

namespace {
namespace battle = VS::Battle;

Battle_Actor* LiveActor(int actor_slot) {
  if (actor_slot < 0 ||
      static_cast<std::size_t>(actor_slot) >= std::size(Battle_actors)) {
    return nullptr;
  }
  return Battle_actors[actor_slot];
}

class LiveBattleModeHost final : public battle::BattleModeHost {
 public:
  [[nodiscard]] int ActorWeaponDrawn(int actor_slot) const override {
    const Battle_Actor* actor = LiveActor(actor_slot);
    return actor != nullptr ? actor->weaponDrawn : 0;
  }
  void SetActorWeaponDrawn(int actor_slot, int weapon_drawn) override {
    if (Battle_Actor* actor = LiveActor(actor_slot); actor != nullptr) {
      actor->weaponDrawn = weapon_drawn;
    }
  }
  [[nodiscard]] int ActorKind(int actor_slot) const override {
    const Battle_Actor* actor = LiveActor(actor_slot);
    return actor != nullptr ? actor->kind : 0;
  }
  void SetActorKind(int actor_slot, int kind) override {
    if (Battle_Actor* actor = LiveActor(actor_slot); actor != nullptr) {
      actor->kind = static_cast<u_short>(kind);
    }
  }
  void ShowWeaponDrawn(int actor_slot, int weapon_drawn,
                       int transition_mode) override {
    VS::Actors::SetLiveActorWeaponDrawn(actor_slot, weapon_drawn,
                                        transition_mode);
  }
  [[nodiscard]] int CharacterWeaponDrawn() const override {
    return Battle_characterState != nullptr ? Battle_characterState->weaponDrawn
                                            : 0;
  }
  void SetHudBattleMode(int weapon_drawn) override {
    VS::Battle::SetLiveHudBattleMode(weapon_drawn);
  }
};

// The original dereferences the actor (and, for the player, the character
// state) without a check; an absent one is refused.
bool CanSetBattleMode(int actor_slot) {
  if (LiveActor(actor_slot) == nullptr) {
    VS_LOG_WARN("battle.targeting", "battle mode for absent actor slot={}",
                actor_slot);
    return false;
  }
  if (actor_slot == 0 && Battle_characterState == nullptr) {
    VS_LOG_WARN("battle.targeting", "battle mode without a character state");
    return false;
  }
  return true;
}
}  // namespace

namespace VS::Battle {
bool LiveIsActorWeaponDrawn(int actor_slot) {
  if (LiveActor(actor_slot) == nullptr) {
    VS_LOG_WARN("battle.targeting", "weapon state of absent actor slot={}",
                actor_slot);
    return false;
  }
  const LiveBattleModeHost host;
  return IsActorWeaponDrawn(actor_slot, host);
}

void LiveSetActorBattleMode(int actor_slot, int battle_mode) {
  if (!CanSetBattleMode(actor_slot)) return;
  LiveBattleModeHost host;
  SetActorBattleMode(actor_slot, battle_mode, host);
}
}  // namespace VS::Battle
