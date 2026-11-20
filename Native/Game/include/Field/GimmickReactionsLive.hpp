#pragma once

#include "Field/GimmickReactions.hpp"

struct Field_GimmickInstance;

namespace VS::Field {
GimmickReactionResult UpdateLiveGimmickReactionAnimation(
    Field_GimmickInstance* record);
// The module image entry (BATTLE.PRG 0x800B101C) that the copied gimmick
// update calls. decomp: func_800B101C(D_800F45E0_t*)
// (src/BATTLE/BATTLE.PRG/44F14.c:1865)
int UpdateGimmickReactionAnimationEntry(Field_GimmickInstance* record);
}  // namespace VS::Field
