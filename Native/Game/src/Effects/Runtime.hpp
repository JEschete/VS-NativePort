#pragma once
// The battle effect runtime's globals: the selected effect package and its
// action and secondary copies, the requests and instance chains of the primary
// and secondary effects, the effect tokens and curve samples, and the tables
// the module image fills. Runtime.cpp defines each one.

#include <cstddef>
#include <cstdint>

#include "Effects/ActionRequest.hpp"
#include "Effects/EffectInstances.hpp"
#include "Effects/EffectScript.hpp"
#include "Effects/Instance.hpp"
#include "Effects/Package.hpp"

namespace VS::Content {
struct DiscQueueSlot;
}  // namespace VS::Content

namespace VS::Effects {
// An effect script opcode handler in the module image's dispatch table.
using ScriptOpcodeHandler = int (*)(void*);

// The renderer table runs up to the action package at 0x800F56C8.
inline constexpr std::size_t kEffectRenderers = 8;
// The embedded default package: nine tagged blocks and a zero terminator.
inline constexpr std::size_t kEmptyPackageBytes = 0x5200;
inline constexpr std::size_t kEmptyPackageWords = kEmptyPackageBytes / 4;
// Extents the copier took from the address gap to the next symbol it knew,
// not capacities. Code reads kActionVariantValues values and kEffectTokenWords
// token words, but ModuleData.cpp fills the whole value extent from the module
// image and CurvesLive.cpp bounds curve sampling by the sample extent.
inline constexpr std::size_t kActionVariantImageBytes = 21824;
inline constexpr std::size_t kTokenBitmapExtent = 215;
inline constexpr std::size_t kCurveSampleExtent = 211;

// Alternates each frame between the two effect render buffers.
// decomp: D_800EC2E4 (D_800EC2E4@BATTLE/BATTLE.PRG)
extern std::uint8_t effectBufferIndex;

// Gives the renderer an effect script attaches by index.
// decomp: D_800EC324 (D_800EC324@BATTLE/BATTLE.PRG)
extern EffectRenderer scriptRenderers[kScriptRenderers];

// Gives the script register value for each action variant below 4.
// decomp: D_800EC32C (D_800EC32C@BATTLE/BATTLE.PRG)
extern char actionVariantValues[kActionVariantImageBytes];

// Dispatches effect script opcodes.
// decomp: D_800EC3F4 (D_800EC3F4@BATTLE/BATTLE.PRG)
extern ScriptOpcodeHandler scriptOpcodeHandlers[kEffectScriptOpcodes];

// Keeps the last random reaction sound so the next one differs.
// decomp: D_800EC4B8 (D_800EC4B8@BATTLE/BATTLE.PRG)
extern std::uint8_t lastReactionSoundVariant;

// Contains the embedded default effect package bound when effect resources are
// initialized. decomp: D_800EC4BC (D_800EC4BC@BATTLE/BATTLE.PRG)
extern std::uint32_t emptyPackage[kEmptyPackageWords];
static_assert(sizeof emptyPackage == kEmptyPackageBytes);

// Remembers the actor tinted red during an action, to restore its color.
// decomp: D_800F185C (D_800F185C@BATTLE/BATTLE.PRG)
extern int tintedActor;

// Holds the effect runtime's busy bits each frame: bit 0 primary, bit 1
// secondary effects still running. decomp: D_800F1868
// (D_800F1868@BATTLE/BATTLE.PRG)
extern int effectBusyFlags;

// Holds the read that reloads the main menu after a primary effect.
// decomp: D_800F5218 (D_800F5218@BATTLE/BATTLE.PRG)
extern VS::Content::DiscQueueSlot* menuReloadSlot;

// Holds one bit per primary effect track not yet started.
// decomp: D_800F521C (D_800F521C@BATTLE/BATTLE.PRG)
extern std::uint32_t pendingTracks;

// Gives the number of primary effect tracks in the package.
// decomp: D_800F5224 (D_800F5224@BATTLE/BATTLE.PRG)
extern int trackCount;

// Counts the primary effect tracks started; the effect ends once all have.
// decomp: D_800F5228 (D_800F5228@BATTLE/BATTLE.PRG)
extern int startedTrackCount;

// Gives the action variant of the current effect; 0 for none.
// decomp: D_800F522C (D_800F522C@BATTLE/BATTLE.PRG)
extern std::uint8_t actionVariant;

// Holds the action effect request being presented.
// decomp: D_800F5230 (D_800F5230@BATTLE/BATTLE.PRG)
extern LiveActionEffectRequest actionRequest;

// Holds the token of the current primary effect.
// decomp: D_800F5318 (D_800F5318@BATTLE/BATTLE.PRG)
extern char primaryToken;

// Steps the primary effect: 6 load the purge overlay, 2/8/3 load package, 4/5
// run tracks, 9 reload the menu, 0 idle. decomp: D_800F531C
// (D_800F531C@BATTLE/BATTLE.PRG)
extern std::uint32_t primaryPhase;

// Holds four 32-bit maps of allocated effect tokens.
// decomp: D_800F5320 (D_800F5320@BATTLE/BATTLE.PRG)
extern int tokenBitmaps[kTokenBitmapExtent];

// Holds the current sample of each package curve; entry 0 is zero.
// decomp: D_800F5330 (D_800F5330@BATTLE/BATTLE.PRG)
extern int curveSamples[kCurveSampleExtent];

// Cleared with the primary effect state; not otherwise used.
// decomp: D_800F53B4 (D_800F53B4@BATTLE/BATTLE.PRG)
extern int unusedPrimaryWord;

// Points to the first primary effect instance, in priority order.
// decomp: D_800F53B8 (D_800F53B8@BATTLE/BATTLE.PRG)
extern LiveEffectInstance* primaryInstances;

// Points to the effect instance being updated.
// decomp: D_800F53BC (D_800F53BC@BATTLE/BATTLE.PRG)
extern LiveEffectInstance* currentInstance;

// Holds the request of the secondary effects being spawned.
// decomp: D_800F53C0 (D_800F53C0@BATTLE/BATTLE.PRG)
extern LiveActionEffectRequest secondaryRequest;

// Set while a primary effect package is loaded and running.
// decomp: D_800F54A8 (D_800F54A8@BATTLE/BATTLE.PRG)
extern char primaryLoaded;

// Points to the first secondary effect instance, in priority order.
// decomp: D_800F54B0 (D_800F54B0@BATTLE/BATTLE.PRG)
extern LiveEffectInstance* secondaryInstances;

// Keeps the selector byte read by the effect script; not otherwise read.
// decomp: D_800F5610 (D_800F5610@BATTLE/BATTLE.PRG)
extern int scriptSelector;

// Keeps the selected effect package while another is used.
// decomp: D_800F5698 (D_800F5698@BATTLE/BATTLE.PRG)
extern LiveEffectPackage* savedPackage;

// Points to the selected effect package.
// decomp: D_800F569C (D_800F569C@BATTLE/BATTLE.PRG)
extern LiveEffectPackage* package;

// Gives the renderer bound to each renderer index by the plugin.
// decomp: effectRenderers (src/BATTLE/BATTLE.PRG/5BF94Private.h:140)
extern EffectRenderer effectRenderers[kEffectRenderers];

// Holds the action effect package.
// decomp: D_800F56C8 (D_800F56C8@BATTLE/BATTLE.PRG)
extern LiveEffectPackage actionPackage;

// Holds the secondary effect package, whose request is the secondary request.
// decomp: D_800F57A0 (D_800F57A0@BATTLE/BATTLE.PRG)
extern LiveEffectPackage secondaryPackage;
}  // namespace VS::Effects
