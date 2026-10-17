// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_SOUNDS_H_TYPES
#define VS_MENU_SOUNDS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_SOUNDS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_SOUNDS_H_DECLS)
#define VS_MENU_SOUNDS_H_DECLS

void Menu_PlayOpenSound(void);
void Menu_PlayInvalidSound(void);
void Menu_PlayLeaveSound(void);
void Menu_PlaySelectSound(void);
void Menu_PlayChangeSound(void);

#endif // VS_MENU_SOUNDS_H_DECLS
