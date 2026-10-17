// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_OPTIONS_H_TYPES
#define VS_MENU_OPTIONS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_OPTIONS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_OPTIONS_H_DECLS)
#define VS_MENU_OPTIONS_H_DECLS

extern char Menu_Options_layoutPadding1[2];;
extern char Menu_Options_layoutPadding2[8];;
extern u_short Menu_Options_menuStrings[];
extern char Menu_Options_simpleMapValues[];
int Menu_Options_UpdateSimpleMapChoice(int init);
int Menu_Options_UpdateAbilityTimingChoice(int init);
int Menu_Options_UpdateWeaponStatusChoice(int init);
int Menu_Options_UpdateArmorStatusChoice(int init);
int Menu_Options_UpdateCursorMemoryChoice(int init);
int Menu_Options_UpdateInformationChoice(int init);
int Menu_Options_UpdatePuzzleModeChoice(int init);
int Menu_Options_UpdateSoundChoice(int init);
int Menu_Options_UpdateVibrationChoice(int init);
int Menu_Options_Update(u_char* state);
void Menu_Options_ResetMenu8Statics(void);

#endif // VS_MENU_OPTIONS_H_DECLS
