#pragma once

#include "Field/GimmickReactions.hpp"

namespace VS::Field {
struct GimmickInstance;
}  // namespace VS::Field

namespace VS::Field {
GimmickReactionResult UpdateLiveGimmickReactionAnimation(
    VS::Field::GimmickInstance* record);
// The module image entry (BATTLE.PRG 0x800B101C) that the copied gimmick
// update calls. decomp: func_800B101C(D_800F45E0_t*)
// (src/BATTLE/BATTLE.PRG/44F14.c:1865)
int UpdateGimmickReactionAnimationEntry(VS::Field::GimmickInstance* record);
}  // namespace VS::Field
