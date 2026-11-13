#pragma once

#include "Legacy/Psx.h"

namespace VS::Battle {
void PlayLiveMeleeAttackAnimation(int slot, SVECTOR* target, int parameter);
void PlayLiveDefenseAnimation(int slot, SVECTOR* target);
void PlayLiveSpellCastAnimation(int slot, SVECTOR* target, int phase);
void PlayLiveChainAbilityAnimation(int slot, SVECTOR* target, int ability,
                                   int sequence);
void PlayLiveSpecialActionAnimation(int slot, SVECTOR* target, int mode);
void PlayLiveBreakArtAnimation(int slot, SVECTOR* target, int mode);
}  // namespace VS::Battle
