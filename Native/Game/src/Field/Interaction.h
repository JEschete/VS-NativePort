// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_INTERACTION_H_TYPES
#define VS_FIELD_INTERACTION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_InteractionFocus Field_InteractionFocus;
typedef struct Field_MpdLockTrigger Field_MpdLockTrigger;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_FIELD_INTERACTION_H_SET_TYPES_ONLY
#endif
#include "Actors/Location.h"
#ifdef VS_FIELD_INTERACTION_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_FIELD_INTERACTION_H_SET_TYPES_ONLY
#endif

// decomp: D_800F1910_t (src/BATTLE/BATTLE.PRG/146C.c:159)
typedef struct Field_InteractionFocus {
    Field_MpdLockTrigger* triggerRecord;
    Actors_ActorLocationQuery actorQuery;
} Field_InteractionFocus;

#endif // VS_FIELD_INTERACTION_H_TYPES
