// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_BATTLEABILITIES_H_TYPES
#define VS_MENU_BATTLEABILITIES_H_TYPES
#include "Legacy/Psx.h"


// decomp: abilityType (src/MENU/MENU2.PRG/64.c:55)
enum Menu_BattleAbilities_AbilityType { Menu_BattleAbilities_AbilityType_Chain = 24, Menu_BattleAbilities_AbilityType_Defense };

#endif // VS_MENU_BATTLEABILITIES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_BATTLEABILITIES_H_DECLS)
#define VS_MENU_BATTLEABILITIES_H_DECLS

extern char Menu_BattleAbilities_abilityCount;
extern u_int Menu_BattleAbilities_abilityMenuState;
extern u_short* Menu_BattleAbilities_abilityText;
extern u_short Menu_BattleAbilities_battleAbilityMenuStrings[292]; /* contents: disc string table (ModuleData.cpp) */;
extern u_short Menu_BattleAbilities_battleAbilityStrings[1060]; /* contents: disc string table (ModuleData.cpp) */;
extern char Menu_BattleAbilities_firstAbility;
extern int Menu_BattleAbilities_highlightedAbility;
extern char Menu_BattleAbilities_selectedAbilityType;
extern char Menu_BattleAbilities_unlockableChainAbilities[];
extern char Menu_BattleAbilities_unlockableDefenseAbilities[];
extern char Menu_BattleAbilities_unlockedChainAbilities[16];;
extern char Menu_BattleAbilities_unlockedDefenseAbilities[16];;
void Menu_BattleAbilities_ResetMenu2Statics();

#endif // VS_MENU_BATTLEABILITIES_H_DECLS
