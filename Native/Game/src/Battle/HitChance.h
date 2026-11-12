// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_HITCHANCE_H_TYPES
#define VS_BATTLE_HITCHANCE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_HITCHANCE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_HITCHANCE_H_DECLS)
#define VS_BATTLE_HITCHANCE_H_DECLS
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"

extern int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);

#endif // VS_BATTLE_HITCHANCE_H_DECLS
