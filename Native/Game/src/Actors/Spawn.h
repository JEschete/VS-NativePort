// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_ActorLocationWords;
union Actors_AuxiliaryActorWord;
struct Actors_JumpContactView;
struct Actors_JumpFlagsView;
struct Actors_SpawnScratch;
struct Actors_SpawnScratchPair;

// decomp: func_8006EBF8_t2 (src/BATTLE/BATTLE.PRG/146C.c:174)
struct Actors_ActorLocationWords {
    int reserved0;
    int reserved4;
    int reserved8;
};
static_assert(sizeof(Actors_ActorLocationWords) == 0xC, "Actors_ActorLocationWords must keep its PS1 layout");

// decomp: func_80077DF0_t2 (src/BATTLE/BATTLE.PRG/146C.c:201)
struct Actors_SpawnScratch {
    int reserved0[4];
};
static_assert(sizeof(Actors_SpawnScratch) == 0x10, "Actors_SpawnScratch must keep its PS1 layout");

// decomp: func_80077DF0_t3 (src/BATTLE/BATTLE.PRG/146C.c:206)
struct Actors_SpawnScratchPair {
    int reserved0[17];
    Actors_SpawnScratch reserved44;
};
static_assert(sizeof(Actors_SpawnScratchPair) == 0x54, "Actors_SpawnScratchPair must keep its PS1 layout");

// decomp: func_800AC690_contact_t (src/BATTLE/BATTLE.PRG/40564.c:58)
struct Actors_JumpContactView {
    u_int reserved0Bit0 : 4;
    u_int reserved0Bit4 : 28;
};
static_assert(sizeof(Actors_JumpContactView) == 0x4, "Actors_JumpContactView must keep its PS1 layout");

// decomp: func_800AC690_flags_t (src/BATTLE/BATTLE.PRG/40564.c:53)
struct Actors_JumpFlagsView {
    u_int reserved0Bit0 : 16;
    u_int reserved2Bit0 : 3;
    u_int reserved2Bit3 : 13;
};
static_assert(sizeof(Actors_JumpFlagsView) == 0x4, "Actors_JumpFlagsView must keep its PS1 layout");

// decomp: func_800765B0_t (src/BATTLE/BATTLE.PRG/146C.c:295)
union Actors_AuxiliaryActorWord {
    char bytes[4];
    int word;
};
static_assert(sizeof(Actors_AuxiliaryActorWord) == 0x4, "Actors_AuxiliaryActorWord must keep its PS1 layout");
