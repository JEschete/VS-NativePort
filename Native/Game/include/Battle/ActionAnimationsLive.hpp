#pragma once

#include <cstddef>
#include <cstdint>

#include "Core/Gte.hpp"

namespace VS::Battle {
void PlayLiveMeleeAttackAnimation(int slot, VS::Core::Gte::SVECTOR* target,
                                  int parameter);
void PlayLiveDefenseAnimation(int slot, VS::Core::Gte::SVECTOR* target);
void SetLiveEnemyCombatPose(int slot, int mode);
void PlayLiveActorDamageReaction(int slot,
                                 VS::Core::Gte::SVECTOR* source_position);
void PlayLiveTargetDamageReaction(int slot,
                                  VS::Core::Gte::SVECTOR* source_position,
                                  std::size_t visited_hit_count);
void PlayLiveSpellCastAnimation(int slot, VS::Core::Gte::SVECTOR* target,
                                int phase);
void PlayLiveChainAbilityAnimation(int slot, VS::Core::Gte::SVECTOR* target,
                                   int ability, int sequence);
void PlayLiveDefenseAbilityAnimation(int slot, VS::Core::Gte::SVECTOR* target,
                                     int unused);
void PlayLiveSpecialActionAnimation(int slot, VS::Core::Gte::SVECTOR* target,
                                    int mode);
void PlayLiveBreakArtAnimation(int slot, VS::Core::Gte::SVECTOR* target,
                               int mode);
void FinishLiveEnemyCombatPose(int slot, std::int16_t facing);
}  // namespace VS::Battle
