#pragma once

#include <cstddef>
#include <cstdint>

#include "Legacy/Psx.h"

namespace VS::Battle {
void PlayLiveMeleeAttackAnimation(int slot, SVECTOR* target, int parameter);
void PlayLiveDefenseAnimation(int slot, SVECTOR* target);
void SetLiveEnemyCombatPose(int slot, int mode);
void PlayLiveActorDamageReaction(int slot, SVECTOR* source_position);
void PlayLiveTargetDamageReaction(int slot, SVECTOR* source_position,
                                  std::size_t visited_hit_count);
void PlayLiveSpellCastAnimation(int slot, SVECTOR* target, int phase);
void PlayLiveChainAbilityAnimation(int slot, SVECTOR* target, int ability,
                                   int sequence);
void PlayLiveDefenseAbilityAnimation(int slot, SVECTOR* target, int unused);
void PlayLiveSpecialActionAnimation(int slot, SVECTOR* target, int mode);
void PlayLiveBreakArtAnimation(int slot, SVECTOR* target, int mode);
void FinishLiveEnemyCombatPose(int slot, std::int16_t facing);
}  // namespace VS::Battle
