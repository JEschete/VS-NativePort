// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ITEMS_RECOVERY_H_TYPES
#define VS_ITEMS_RECOVERY_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_ITEMS_RECOVERY_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ITEMS_RECOVERY_H_DECLS)
#define VS_ITEMS_RECOVERY_H_DECLS
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"

short Items_CalculateItemRecovery(Battle_ActionDefinition* arg0, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3, int arg4);

#endif // VS_ITEMS_RECOVERY_H_DECLS
