#pragma once

#include "Field/GimmickReactions.hpp"

struct Field_GimmickInstance;

namespace VS::Field {
GimmickReactionResult UpdateLiveGimmickReactionAnimation(
    Field_GimmickInstance* record);
}  // namespace VS::Field
