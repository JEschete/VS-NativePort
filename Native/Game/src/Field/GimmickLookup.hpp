#pragma once

#include <cstddef>
#include <iterator>

#include "Field/Gimmicks.h"

namespace VS::Field {

// Original actor-query fallbacks (38C1C.c): renderer slot16 has no gimmick
// slot.
inline Field_GimmickInstance* FindGimmickForActorSlot(int actor_slot) {
  return actor_slot >= 0 && static_cast<std::size_t>(actor_slot) <
                                std::size(Field_gimmickInstances)
             ? Field_gimmickInstances[actor_slot]
             : nullptr;
}

}  // namespace VS::Field
