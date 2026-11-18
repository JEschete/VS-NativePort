#pragma once

#include "Field/GimmickReactions.hpp"

struct Field_GimmickInstance;
struct Field_GimmickMotionView;

namespace VS::Field {
GimmickReactionResult UpdateLiveGimmickReactionAnimation(
    Field_GimmickInstance* record);
// The module image entry (BATTLE.PRG 0x800B101C) that the copied gimmick
// update calls.
int UpdateGimmickReactionAnimationEntry(Field_GimmickMotionView* record);
}  // namespace VS::Field
