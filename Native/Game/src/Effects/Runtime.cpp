// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "DiscQueue.h"
#include "Effects/Package.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

// decomp: D_800EC2E4 (D_800EC2E4@BATTLE/BATTLE.PRG)
u_char Effects_effectBufferIndex; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC324 (D_800EC324@BATTLE/BATTLE.PRG)
Effects_EffectRenderer Effects_scriptRenderers[2]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800EC32C (D_800EC32C@BATTLE/BATTLE.PRG)
char Effects_actionVariantValues[21824]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC3F4 (D_800EC3F4@BATTLE/BATTLE.PRG)
int (*Effects_scriptOpcodeHandlers[49])(void*); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800EC4B8 (D_800EC4B8@BATTLE/BATTLE.PRG)
u_char Effects_lastReactionSoundVariant; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC4BC (D_800EC4BC@BATTLE/BATTLE.PRG)
u_int Effects_emptyPackage[0x5200 / sizeof(u_int)]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F185C (D_800F185C@BATTLE/BATTLE.PRG)
int Effects_tintedActor;

// decomp: D_800F1868 (D_800F1868@BATTLE/BATTLE.PRG)
int Effects_effectBusyFlags;

// decomp: D_800F5218 (D_800F5218@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* Effects_menuReloadSlot;

// decomp: D_800F521C (D_800F521C@BATTLE/BATTLE.PRG)
u_int Effects_pendingTracks;

// decomp: D_800F5224 (D_800F5224@BATTLE/BATTLE.PRG)
int Effects_trackCount;

// decomp: D_800F5228 (D_800F5228@BATTLE/BATTLE.PRG)
int Effects_startedTrackCount;

// decomp: D_800F522C (D_800F522C@BATTLE/BATTLE.PRG)
u_char Effects_actionVariant;

// decomp: D_800F5230 (D_800F5230@BATTLE/BATTLE.PRG)
Effects_ActionEffectRequest Effects_actionRequest;

// decomp: D_800F5318 (D_800F5318@BATTLE/BATTLE.PRG)
char Effects_primaryToken;

// decomp: D_800F531C (D_800F531C@BATTLE/BATTLE.PRG)
u_int Effects_primaryPhase;

// decomp: D_800F5320 (D_800F5320@BATTLE/BATTLE.PRG)
int Effects_tokenBitmaps[215];

// decomp: D_800F5330 (D_800F5330@BATTLE/BATTLE.PRG)
int Effects_curveSamples[211];

// decomp: D_800F53B4 (D_800F53B4@BATTLE/BATTLE.PRG)
int Effects_unusedPrimaryWord;

// decomp: D_800F53B8 (D_800F53B8@BATTLE/BATTLE.PRG)
Effects_EffectInstance* Effects_primaryInstances;

// decomp: D_800F53BC (D_800F53BC@BATTLE/BATTLE.PRG)
Effects_EffectInstance* Effects_currentInstance;

// decomp: D_800F53C0 (D_800F53C0@BATTLE/BATTLE.PRG)
Effects_ActionEffectRequest Effects_secondaryRequest;

// decomp: D_800F54A8 (D_800F54A8@BATTLE/BATTLE.PRG)
char Effects_primaryLoaded;

// decomp: D_800F54A9 (D_800F54A9@BATTLE/BATTLE.PRG)
char Effects_actionVariantArgument;

// decomp: D_800F54B0 (D_800F54B0@BATTLE/BATTLE.PRG)
Effects_EffectInstance* Effects_secondaryInstances;

// decomp: D_800F5610 (D_800F5610@BATTLE/BATTLE.PRG)
int Effects_scriptSelector;

// decomp: D_800F5698 (D_800F5698@BATTLE/BATTLE.PRG)
Effects_EffectPackage* Effects_savedPackage;

// decomp: D_800F569C (D_800F569C@BATTLE/BATTLE.PRG)
Effects_EffectPackage* Effects_package;

// decomp: D_800F56A8 (D_800F56A8@BATTLE/BATTLE.PRG)
Effects_EffectRenderer Effects_rendererTableAlias[1];

// decomp: vs_battle_effectRenderers (vs_battle_effectRenderers@BATTLE/BATTLE.PRG)
Effects_EffectRenderer Effects_effectRenderers[8];

// decomp: D_800F56C8 (D_800F56C8@BATTLE/BATTLE.PRG)
Effects_EffectPackage Effects_actionPackage;

// decomp: D_800F57A0 (D_800F57A0@BATTLE/BATTLE.PRG)
Effects_EffectPackage Effects_secondaryPackage;

}  // extern "C"
