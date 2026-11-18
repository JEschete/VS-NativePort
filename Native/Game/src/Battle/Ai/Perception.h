// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Battle_Ai_RelationWord;

// layout: ledger (upstream merge) for func_800DA1D4_t
struct Battle_Ai_RelationWord {
    /* 0x00 */ u_int active : 1;
    /* 0x00 */ u_int reserved0Bit1 : 4;
    /* 0x00 */ u_int visible : 1;
    /* 0x00 */ u_int classification : 2;
    /* 0x01 */ u_int counter : 7;
    /* 0x01 */ u_int timer : 5;
    /* 0x02 */ u_int classificationTimer : 7;
    /* 0x03 */ u_int blocked : 1;
    /* 0x03 */ u_int previousClassification : 2;
    /* 0x03 */ u_int reserved3Bit6 : 2;
};
static_assert(sizeof(Battle_Ai_RelationWord) == 0x4, "Battle_Ai_RelationWord must keep its PS1 layout");

extern int Battle_Ai_frameTickBudget;
extern int Battle_Ai_secondPerceptionTarget;
extern int Battle_Ai_candidateEngagedThisUpdate;
