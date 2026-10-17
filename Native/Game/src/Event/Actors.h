// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EVENT_ACTORS_H_TYPES
#define VS_EVENT_ACTORS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Event_ActorSelection Event_ActorSelection;

// decomp: func_800BB68C_t (src/BATTLE/BATTLE.PRG/4A0A8.c:182)
typedef struct Event_ActorSelection {
    char remainingCount;
    char cursor;
    char actorIds[22];
} Event_ActorSelection;
_Static_assert(sizeof(Event_ActorSelection) == 0x18, "Event_ActorSelection must keep its PS1 layout");

#endif // VS_EVENT_ACTORS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EVENT_ACTORS_H_DECLS)
#define VS_EVENT_ACTORS_H_DECLS

extern int Event_weaponDrawStates[121];
extern char Event_animationBankBases[472];

#endif // VS_EVENT_ACTORS_H_DECLS
