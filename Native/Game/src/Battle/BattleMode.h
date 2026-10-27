// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_BATTLEMODE_H_TYPES
#define VS_BATTLE_BATTLEMODE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_BATTLEMODE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_BATTLEMODE_H_DECLS)
#define VS_BATTLE_BATTLEMODE_H_DECLS

int Battle_IsActorWeaponDrawn(int actor_slot);
void Battle_SetActorBattleMode(int actor_slot, int battle_mode);

#endif // VS_BATTLE_BATTLEMODE_H_DECLS
