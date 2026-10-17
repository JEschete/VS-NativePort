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
int Menu_SetTextBoxText(int boxId, char* str);
int Menu_SelectTextBoxChoice(u_char boxMask);
void Menu_InitInformationTextBox(enum Menu_InformationBoxHeader header);
int Menu_DismissAllTextBoxes(void);
void Menu_ShowSceneNotification(int arg0, int arg1, int arg2);
int Menu_IsSceneNotificationPending(void);
void Menu_ReleaseSceneNotification(void);
void Menu_UpdateTextBoxes(void);
Menu_TextBoxState* Menu_TextBox(int id);
Menu_TextAnchorWord Menu_TextBoxAnchor(int arg0, int arg1, int arg2, int arg3);
void Menu_InitTextBox(
    int id, int flags, int x, int y, int charsPerLine, int h, int centerX, int centerY);
int Menu_DismissTextBox(int id);
void Menu_DrawInformationBoxHeader(int arg0, u_int arg1);
void Menu_DrawInformationBoxBackdrop(int id);
int Menu_FlipTextBoxShapePoint(int arg0, int arg1);
void Menu_DrawTextBoxFrame(int index);

#endif // VS_MENU_TEXTBOX_H_DECLS
