// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_SPAWN_H_TYPES
#define VS_ACTORS_SPAWN_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ActorLocationWords Actors_ActorLocationWords;
typedef union Actors_AuxiliaryActorWord Actors_AuxiliaryActorWord;
typedef struct Actors_JumpContactView Actors_JumpContactView;
typedef struct Actors_JumpFlagsView Actors_JumpFlagsView;
typedef struct Actors_SpawnScratch Actors_SpawnScratch;
typedef struct Actors_SpawnScratchPair Actors_SpawnScratchPair;

// decomp: func_8006EBF8_t2 (src/BATTLE/BATTLE.PRG/146C.c:174)
typedef struct Actors_ActorLocationWords {
    int reserved0;
    int reserved4;
    int reserved8;
} Actors_ActorLocationWords;
_Static_assert(sizeof(Actors_ActorLocationWords) == 0xC, "Actors_ActorLocationWords must keep its PS1 layout");

// decomp: func_80077DF0_t2 (src/BATTLE/BATTLE.PRG/146C.c:201)
typedef struct Actors_SpawnScratch {
    int reserved0[4];
} Actors_SpawnScratch;
_Static_assert(sizeof(Actors_SpawnScratch) == 0x10, "Actors_SpawnScratch must keep its PS1 layout");

// decomp: func_80077DF0_t3 (src/BATTLE/BATTLE.PRG/146C.c:206)
typedef struct Actors_SpawnScratchPair {
    int reserved0[17];
    Actors_SpawnScratch reserved44;
} Actors_SpawnScratchPair;
_Static_assert(sizeof(Actors_SpawnScratchPair) == 0x54, "Actors_SpawnScratchPair must keep its PS1 layout");

// decomp: func_800AC690_contact_t (src/BATTLE/BATTLE.PRG/40564.c:58)
typedef struct Actors_JumpContactView {
    u_int reserved0Bit0 : 4;
    u_int reserved0Bit4 : 28;
} Actors_JumpContactView;
_Static_assert(sizeof(Actors_JumpContactView) == 0x4, "Actors_JumpContactView must keep its PS1 layout");

// decomp: func_800AC690_flags_t (src/BATTLE/BATTLE.PRG/40564.c:53)
typedef struct Actors_JumpFlagsView {
    u_int reserved0Bit0 : 16;
    u_int reserved2Bit0 : 3;
    u_int reserved2Bit3 : 13;
} Actors_JumpFlagsView;
_Static_assert(sizeof(Actors_JumpFlagsView) == 0x4, "Actors_JumpFlagsView must keep its PS1 layout");

// decomp: func_800765B0_t (src/BATTLE/BATTLE.PRG/146C.c:295)
typedef union Actors_AuxiliaryActorWord {
    char bytes[4];
    int word;
} Actors_AuxiliaryActorWord;
_Static_assert(sizeof(Actors_AuxiliaryActorWord) == 0x4, "Actors_AuxiliaryActorWord must keep its PS1 layout");

#endif // VS_ACTORS_SPAWN_H_TYPES
