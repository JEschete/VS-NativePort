// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


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
