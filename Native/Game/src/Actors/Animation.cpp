// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/Animation.h"
#include "Actors/TextureAnimation.h"

// decomp: D_800E8960 (D_800E8960@BATTLE/BATTLE.PRG)
u_char Actors_sharedSequenceIdsByKind[592]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8F2C (D_800E8F2C@BATTLE/BATTLE.PRG)
u_char Actors_sharedSequenceBankIndex; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E8FC0 (D_800E8FC0@BATTLE/BATTLE.PRG)
char Actors_reservedControlByte; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9090 (D_800E9090@BATTLE/BATTLE.PRG)
u_char Actors_drawnWeaponAnimationBases[3232]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E909C (D_800E909C@BATTLE/BATTLE.PRG)
u_char Actors_sheathedWeaponAnimationBases[3220]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E916C (D_800E916C@BATTLE/BATTLE.PRG)
Actors_TextureAnimationKey Actors_secondarySlotSequences[188][8]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9298 (D_800E9298@BATTLE/BATTLE.PRG)
u_char Actors_animationEventLengths[2712]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E92DC (D_800E92DC@BATTLE/BATTLE.PRG)
u_char Actors_drawnWeaponSoundIds[2644]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9A28 (D_800E9A28@BATTLE/BATTLE.PRG)
u_short Actors_animationEventPoints[97][4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F244F (D_800F244F@BATTLE/BATTLE.PRG)
u_char Actors_sequenceTexturePagesOneBased[8425];

// decomp: D_800F2450 (D_800F2450@BATTLE/BATTLE.PRG)
u_char Actors_sharedSequenceBankTexturePages[8424];

// decomp: D_800F4528 (D_800F4528@BATTLE/BATTLE.PRG)
void* Actors_sharedSequenceData[4];

// decomp: D_800F4628 (D_800F4628@BATTLE/BATTLE.PRG)
short Actors_sharedSequenceAnimationIds[4][16];

// decomp: D_800F4768 (D_800F4768@BATTLE/BATTLE.PRG)
void* Actors_sequenceWorkBuffer;

// decomp: D_800F4810 (D_800F4810@BATTLE/BATTLE.PRG)
char* Actors_sharedSequenceAnimationData[4][16];

// decomp: D_800F4930 (D_800F4930@BATTLE/BATTLE.PRG)
void* Actors_primarySequenceBuffer;

// decomp: D_800F4938 (D_800F4938@BATTLE/BATTLE.PRG)
short Actors_sharedSequenceEntryParams[4][16];

// decomp: D_800F49DC (D_800F49DC@BATTLE/BATTLE.PRG)
char Actors_lastResolvedSequenceIndex;

}  // extern "C"
