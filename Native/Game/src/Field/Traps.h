// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_TRAPS_H_TYPES
#define VS_FIELD_TRAPS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_TRAPS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_TRAPS_H_DECLS)
#define VS_FIELD_TRAPS_H_DECLS
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"

int Field_RollRiskLockBreak(void);
short Field_CalculateTrapPanelDamage(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* hit, int nHit,
    int withRandMod);
short Field_CalculateHealingPanelRecovery(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* hit, int nHit,
    int withRandMod);

#endif // VS_FIELD_TRAPS_H_DECLS
