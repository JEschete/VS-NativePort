// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Content/DiscLayout.h"
#include "Ending/Credits.h"

// decomp: D_8006E3FC (D_8006E3FC@ENDING/ENDING.PRG)
u_char Ending_Credits_scriptBytes[7040]; /* contents: module image (ModuleData.cpp) */

// decomp: _glyphWidths (_glyphWidths@ENDING/ENDING.PRG)
u_char Ending_Credits_glyphWidths[440244]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8007005C (D_8007005C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureOne; /* contents: module image (ModuleData.cpp) */

// decomp: D_8007709C (D_8007709C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureEight; /* contents: module image (ModuleData.cpp) */

// decomp: D_80088ABC (D_80088ABC@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureNine; /* contents: module image (ModuleData.cpp) */

// decomp: D_8009A4DC (D_8009A4DC@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureTen; /* contents: module image (ModuleData.cpp) */

// decomp: D_800ABEFC (D_800ABEFC@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureEleven; /* contents: module image (ModuleData.cpp) */

// decomp: D_800BD91C (D_800BD91C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureTwelve; /* contents: module image (ModuleData.cpp) */

// decomp: D_800CF33C (D_800CF33C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureTwo; /* contents: module image (ModuleData.cpp) */

// decomp: D_800D7F7C (D_800D7F7C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureThree; /* contents: module image (ModuleData.cpp) */

// decomp: D_800D837C (D_800D837C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureFive; /* contents: module image (ModuleData.cpp) */

// decomp: D_800D975C (D_800D975C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureFour; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DB09C (D_800DB09C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureSix; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DB26C (D_800DB26C@ENDING/ENDING.PRG)
Ending_Credits_TimHeader Ending_Credits_embeddedTextureSeven; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DB72C (D_800DB72C@ENDING/ENDING.PRG)
u_char Ending_Credits_illustrationDecodePhase; /* contents: module image (ModuleData.cpp) */

// decomp: D_800DB7B0 (D_800DB7B0@ENDING/ENDING.PRG)
Ending_Credits_TimHeader* Ending_Credits_illustrationTimWorkspace; /* data: contents loaded in Step 5 */

// decomp: D_800DBB6A (D_800DBB6A@ENDING/ENDING.PRG)
short Ending_Credits_illustrationTexturePageX;

// decomp: D_800DBB6C (D_800DBB6C@ENDING/ENDING.PRG)
u_short Ending_Credits_illustrationPaletteY;

// decomp: D_800DBB70 (D_800DBB70@ENDING/ENDING.PRG)
u_char* Ending_Credits_decodeBackreferenceCursor;

// decomp: D_800DBB74 (D_800DBB74@ENDING/ENDING.PRG)
u_char* Ending_Credits_decodeInputCursor;

// decomp: D_800DBB78 (D_800DBB78@ENDING/ENDING.PRG)
u_char* Ending_Credits_decodeOutputCursor;

// decomp: D_800DBB7C (D_800DBB7C@ENDING/ENDING.PRG)
u_char Ending_Credits_decodeFillByteZero;

// decomp: D_800DBB7D (D_800DBB7D@ENDING/ENDING.PRG)
u_char Ending_Credits_decodeFillByteOne;

// decomp: D_800DBB7E (D_800DBB7E@ENDING/ENDING.PRG)
u_short Ending_Credits_decodeRunLength;

// decomp: D_800DBB80 (D_800DBB80@ENDING/ENDING.PRG)
u_short Ending_Credits_decodeBackreferenceOffset;

// decomp: D_800DC19A (D_800DC19A@ENDING/ENDING.PRG)
short Ending_Credits_decodeBudgetStartTick;

// decomp: D_800DC1A0 (D_800DC1A0@ENDING/ENDING.PRG)
void* Ending_Credits_queuedCompressedImage;

// decomp: D_800DC1A4 (D_800DC1A4@ENDING/ENDING.PRG)
Ending_Credits_TimHeader* Ending_Credits_queuedTimDestination;

// decomp: D_800DC1A8 (D_800DC1A8@ENDING/ENDING.PRG)
void* Ending_Credits_illustrationResources[16];

// decomp: _illustLbas (_illustLbas@ENDING/ENDING.PRG)
int Ending_Credits_illustrationDiscLocations[] = { VS_ILLUST06_BIN_LBA, VS_ILLUST07_BIN_LBA, VS_ILLUST08_BIN_LBA,
    VS_ILLUST09_BIN_LBA, VS_ILLUST10_BIN_LBA, VS_ILLUST11_BIN_LBA, VS_ILLUST12_BIN_LBA,
    VS_ILLUST13_BIN_LBA, VS_ILLUST14_BIN_LBA, VS_ILLUST15_BIN_LBA, VS_ILLUST16_BIN_LBA, 0,
    0, 0, 0, 0 };

// decomp: _illustSizes (_illustSizes@ENDING/ENDING.PRG)
int Ending_Credits_illustrationDiscSizes[] = { VS_ILLUST06_BIN_SIZE, VS_ILLUST07_BIN_SIZE, VS_ILLUST08_BIN_SIZE,
    VS_ILLUST09_BIN_SIZE, VS_ILLUST10_BIN_SIZE, VS_ILLUST11_BIN_SIZE,
    VS_ILLUST12_BIN_SIZE, VS_ILLUST13_BIN_SIZE, VS_ILLUST14_BIN_SIZE,
    VS_ILLUST15_BIN_SIZE, VS_ILLUST16_BIN_SIZE, 0, 0, 0, 0, 0 };

}  // extern "C"
