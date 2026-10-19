// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_MAGIC_H_TYPES
#define VS_MENU_MAGIC_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_MAGIC_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_MAGIC_H_DECLS)
#define VS_MENU_MAGIC_H_DECLS

extern char Menu_Magic_warlockSpellIds[20];
extern char Menu_Magic_shamanSpellIds[8];
extern char Menu_Magic_sorcererSpellIds[20];
extern char Menu_Magic_enchanterSpellIds[8];
extern u_char Menu_Magic_warlockLevelCursorBlink;;
extern char Menu_Magic_layoutPadding1[5];;
extern char Menu_Magic_layoutPadding2[2];;
extern char Menu_Magic_layoutPadding3[6];;
extern const int Menu_Magic_layoutPadding4 __attribute__((unused));
extern char Menu_Magic_mpCostDirectTextBuffer[16];;
extern signed char Menu_Magic_mpCostTextBuffer[16];;
extern char Menu_Magic_availableEnchanterSpells[8];;
extern char Menu_Magic_availableShamanSpells[6];;
extern char Menu_Magic_availableSorcererSpells[18];;
extern char Menu_Magic_availableWarlockSpells[18];;
extern u_short Menu_Magic_magicStrings[2462]; /* contents: disc string table (ModuleData.c) */;
extern int Menu_Magic_savePointData[];
extern u_short Menu_Magic_teleportationStrings[1599]; /* contents: disc string table (ModuleData.c) */;
int Menu_Magic_UpdateTeleportMenu(int init);
void Menu_Magic_CreateHeader(void);
int Menu_Magic_Update(u_char* state);
void Menu_Magic_SetSpellMpCost(int id);
void Menu_Magic_SetTeleportMpCost(int costDecimal, int disabled);
int Menu_Magic_TeleportMpCost(int targetSavePointId);
int Menu_Magic_UpdateWarlockMenu(u_int initShortcutInvoked);
int Menu_Magic_UpdateShamanMenu(u_int initShortcutInvoked);
int Menu_Magic_UpdateSorcererMenu(u_int initShortcutInvoked);
int Menu_Magic_UpdateEnchanterMenu(u_int initShortcutInvoked);
void Menu_Magic_ResetMenu0Statics(void);

#endif // VS_MENU_MAGIC_H_DECLS
