// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "DiscQueue.h"
#include "Effects/Particles.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/TransformsLiveState.hpp"

// decomp: D_800EC2E4 (D_800EC2E4@BATTLE/BATTLE.PRG)
std::uint8_t VS::Effects::effectBufferIndex; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC324 (D_800EC324@BATTLE/BATTLE.PRG)
VS::Effects::EffectRenderer VS::Effects::scriptRenderers[VS::Effects::kScriptRenderers]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800EC32C (D_800EC32C@BATTLE/BATTLE.PRG)
char VS::Effects::actionVariantValues[VS::Effects::kActionVariantImageBytes]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC3F4 (D_800EC3F4@BATTLE/BATTLE.PRG)
VS::Effects::ScriptOpcodeHandler VS::Effects::scriptOpcodeHandlers[VS::Effects::kEffectScriptOpcodes]; /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800EC4B8 (D_800EC4B8@BATTLE/BATTLE.PRG)
std::uint8_t VS::Effects::lastReactionSoundVariant; /* contents: module image (ModuleData.cpp) */

// decomp: D_800EC4BC (D_800EC4BC@BATTLE/BATTLE.PRG)
std::uint32_t VS::Effects::emptyPackage[VS::Effects::kEmptyPackageWords]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F185C (D_800F185C@BATTLE/BATTLE.PRG)
int VS::Effects::tintedActor;

// decomp: D_800F1868 (D_800F1868@BATTLE/BATTLE.PRG)
int VS::Effects::effectBusyFlags;

// decomp: D_800F5218 (D_800F5218@BATTLE/BATTLE.PRG)
Content_DiscQueueSlot* VS::Effects::menuReloadSlot;

// decomp: D_800F521C (D_800F521C@BATTLE/BATTLE.PRG)
std::uint32_t VS::Effects::pendingTracks;

// decomp: D_800F5224 (D_800F5224@BATTLE/BATTLE.PRG)
int VS::Effects::trackCount;

// decomp: D_800F5228 (D_800F5228@BATTLE/BATTLE.PRG)
int VS::Effects::startedTrackCount;

// decomp: D_800F522C (D_800F522C@BATTLE/BATTLE.PRG)
std::uint8_t VS::Effects::actionVariant;

// decomp: D_800F5230 (D_800F5230@BATTLE/BATTLE.PRG)
VS::Effects::LiveActionEffectRequest VS::Effects::actionRequest;

// decomp: D_800F5318 (D_800F5318@BATTLE/BATTLE.PRG)
char VS::Effects::primaryToken;

// decomp: D_800F531C (D_800F531C@BATTLE/BATTLE.PRG)
std::uint32_t VS::Effects::primaryPhase;

// decomp: D_800F5320 (D_800F5320@BATTLE/BATTLE.PRG)
int VS::Effects::tokenBitmaps[VS::Effects::kTokenBitmapExtent];

// decomp: D_800F5330 (D_800F5330@BATTLE/BATTLE.PRG)
int VS::Effects::curveSamples[VS::Effects::kCurveSampleExtent];

// decomp: D_800F53B4 (D_800F53B4@BATTLE/BATTLE.PRG)
int VS::Effects::unusedPrimaryWord;

// decomp: D_800F53B8 (D_800F53B8@BATTLE/BATTLE.PRG)
VS::Effects::LiveEffectInstance* VS::Effects::primaryInstances;

// decomp: D_800F53BC (D_800F53BC@BATTLE/BATTLE.PRG)
VS::Effects::LiveEffectInstance* VS::Effects::currentInstance;

// decomp: D_800F53C0 (D_800F53C0@BATTLE/BATTLE.PRG)
VS::Effects::LiveActionEffectRequest VS::Effects::secondaryRequest;

// decomp: D_800F54A8 (D_800F54A8@BATTLE/BATTLE.PRG)
char VS::Effects::primaryLoaded;

// D_800F54A9 was written by func_800CF0E8 (0x800CF15C) and never read; it is
// retired (#445).

// decomp: D_800F54B0 (D_800F54B0@BATTLE/BATTLE.PRG)
VS::Effects::LiveEffectInstance* VS::Effects::secondaryInstances;

// decomp: D_800F5610 (D_800F5610@BATTLE/BATTLE.PRG)
int VS::Effects::scriptSelector;

// decomp: D_800F5698 (D_800F5698@BATTLE/BATTLE.PRG)
VS::Effects::LiveEffectPackage* VS::Effects::savedPackage;

// decomp: D_800F569C (D_800F569C@BATTLE/BATTLE.PRG)
VS::Effects::LiveEffectPackage* VS::Effects::package;

// D_800F56A8 declared the renderer table below a second time, under its
// address, and nothing read it; upstream now names the address once
// (effectRenderers), and the second declaration is retired (#441).

// decomp: vs_battle_effectRenderers (vs_battle_effectRenderers@BATTLE/BATTLE.PRG)
VS::Effects::EffectRenderer VS::Effects::effectRenderers[VS::Effects::kEffectRenderers];

// decomp: D_800F56C8 (D_800F56C8@BATTLE/BATTLE.PRG)
VS::Effects::LiveEffectPackage VS::Effects::actionPackage;

// decomp: D_800F57A0 (D_800F57A0@BATTLE/BATTLE.PRG)
VS::Effects::LiveEffectPackage VS::Effects::secondaryPackage;

