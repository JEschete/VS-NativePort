// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Battle/TextBoxes.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"

// decomp: D_800EB9D8 (D_800EB9D8@BATTLE/BATTLE.PRG)
int* Menu_sceneNotificationBuffer; /* data: contents loaded in Step 5 */

// decomp: D_800EBF58 (D_800EBF58@BATTLE/BATTLE.PRG)
char Menu_kNotificationItemNames[65][12]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC2D8 (D_800EC2D8@BATTLE/BATTLE.PRG)
int Menu_kInformationHeaderSpriteWords[5477]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4E68 (D_800F4E68@BATTLE/BATTLE.PRG)
char Menu_sceneNotificationTimer;

// decomp: D_800F4E69 (D_800F4E69@BATTLE/BATTLE.PRG)
char Menu_sceneNotificationTimed;

// decomp: textBoxSelector (textBoxSelector@BATTLE/BATTLE.PRG)
Menu_Framework_TextBoxSelector Menu_textBoxSelector;

// decomp: vs_battle_textBoxes (vs_battle_textBoxes@BATTLE/BATTLE.PRG)
Menu_TextBoxState Menu_textBoxes[8];

// decomp: D_800F51B8 (D_800F51B8@BATTLE/BATTLE.PRG)
u_long* Menu_activeTextBoxOrderingTable;

