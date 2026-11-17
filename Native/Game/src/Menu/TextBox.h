// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_TEXTBOX_H_TYPES
#define VS_MENU_TEXTBOX_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_MENU_TEXTBOX_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_MENU_TEXTBOX_H_DECLS)
#define VS_MENU_TEXTBOX_H_DECLS
#include "Battle/TextBoxes.h"
#include "Menu/Framework/TextBoxes.h"

extern int* Menu_sceneNotificationBuffer;
extern char Menu_kNotificationItemNames[65][12];
extern int Menu_kInformationHeaderSpriteWords[5477];
extern char Menu_sceneNotificationTimer;
extern char Menu_sceneNotificationTimed;
extern Menu_Framework_TextBoxSelector Menu_textBoxSelector;
extern Menu_TextBoxState Menu_textBoxes[8];
extern u_long* Menu_activeTextBoxOrderingTable;

#endif // VS_MENU_TEXTBOX_H_DECLS
