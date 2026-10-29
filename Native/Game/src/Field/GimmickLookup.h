#ifndef NATIVE_GAME_SRC_FIELD_GIMMICKLOOKUP_H_
#define NATIVE_GAME_SRC_FIELD_GIMMICKLOOKUP_H_

#include "Field/Gimmicks.h"

// Original actor-query fallbacks (38C1C.c): renderer slot16 has no gimmick
// slot.
static inline Field_GimmickInstance* Field_FindGimmickForActorSlot(
    int actor_slot) {
  enum {
    kGimmickSlotCount =
        sizeof Field_gimmickInstances / sizeof Field_gimmickInstances[0]
  };
  return actor_slot >= 0 && actor_slot < kGimmickSlotCount
             ? Field_gimmickInstances[actor_slot]
             : NULL;
}

#endif  // NATIVE_GAME_SRC_FIELD_GIMMICKLOOKUP_H_
