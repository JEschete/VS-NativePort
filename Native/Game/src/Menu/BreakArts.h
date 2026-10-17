// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_BREAKARTS_H_TYPES
#define VS_MENU_BREAKARTS_H_TYPES
#include "Legacy/Psx.h"


// decomp: drawArtsListOpt (src/MENU/MENU1.PRG/30.c:94)
enum Menu_BreakArts_ArtListOption { Menu_BreakArts_ArtListOption_OptForceCursorMemory = 0x10 };
typedef enum Menu_BreakArts_ArtListOption Menu_BreakArts_ArtListOption;

#endif // VS_MENU_BREAKARTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_BREAKARTS_H_DECLS)
#define VS_MENU_BREAKARTS_H_DECLS

extern char Menu_BreakArts_layoutPadding1;;
extern u_short Menu_BreakArts_strings[];
void Menu_BreakArts_SetHpCost(int id);
void Menu_BreakArts_DrawNextUnlockPoints(int xOffset, int weaponCategory, int row);
int Menu_BreakArts_UpdateArtList(int typeCursorMem);
int Menu_BreakArts_UpdateWeaponCategories(int initialize);
void Menu_BreakArts_CreateHeader(void);
int Menu_BreakArts_Update(u_char* state);
void Menu_BreakArts_ResetMenu1Statics(void);

#endif // VS_MENU_BREAKARTS_H_DECLS
