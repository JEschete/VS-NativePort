// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_BATTLEABILITIES_H_TYPES
#define VS_MENU_BATTLEABILITIES_H_TYPES
#include "Legacy/Psx.h"


// decomp: src/MENU/MAINMENU.PRG/58EC.h:176
#define menuRowInfo(id, x, y) ((y) << 8) | ((x) << 4) | (id)

// decomp: src/SLUS_010.40/sfx.h:8
#define VS_SFX_SET 35

// decomp: abilityType (src/MENU/MENU2.PRG/64.c:55)
enum Menu_BattleAbilities_AbilityType { Menu_BattleAbilities_AbilityType_Chain = 24, Menu_BattleAbilities_AbilityType_Defense };
typedef enum Menu_BattleAbilities_AbilityType Menu_BattleAbilities_AbilityType;

#endif // VS_MENU_BATTLEABILITIES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_BATTLEABILITIES_H_DECLS)
#define VS_MENU_BATTLEABILITIES_H_DECLS

extern char Menu_BattleAbilities_abilityCount;
extern u_int Menu_BattleAbilities_abilityMenuState;
extern u_short* Menu_BattleAbilities_abilityText;
extern u_short Menu_BattleAbilities_battleAbilityMenuStrings[292]; /* contents: disc string table (ModuleData.c) */;
extern u_short Menu_BattleAbilities_battleAbilityStrings[1060]; /* contents: disc string table (ModuleData.c) */;
extern char Menu_BattleAbilities_firstAbility;
extern int Menu_BattleAbilities_highlightedAbility;
extern char Menu_BattleAbilities_selectedAbilityType;
extern char Menu_BattleAbilities_unlockableChainAbilities[];
extern char Menu_BattleAbilities_unlockableDefenseAbilities[];
extern char Menu_BattleAbilities_unlockedChainAbilities[16];;
extern char Menu_BattleAbilities_unlockedDefenseAbilities[16];;
int Menu_BattleAbilities_UpdateBreakArtUnlock(int init);
char* Menu_BattleAbilities_UnlockDescription(int abilityId);
int Menu_BattleAbilities_UpdateAbilityUnlock(int initialize);
int Menu_BattleAbilities_UpdateUnlockNotifications(char* state);
void Menu_BattleAbilities_SetRiskCost(int id);
int Menu_BattleAbilities_CreateAbilityRows(int abilityCount, int abilityType, u_short** menuText);
void Menu_BattleAbilities_ToggleButtonAssignment(int abilityType, int button, int abilityId);
int Menu_BattleAbilities_UpdateAssignments(void);
int Menu_BattleAbilities_UpdateChainAbilityMenu(int initialize);
int Menu_BattleAbilities_UpdateDefenseAbilityMenu(int arg0);
void Menu_BattleAbilities_DrawNextUnlockPoints(int row);
int Menu_BattleAbilities_Update(u_char* state);
void Menu_BattleAbilities_ResetMenu2Statics(void);

#endif // VS_MENU_BATTLEABILITIES_H_DECLS
