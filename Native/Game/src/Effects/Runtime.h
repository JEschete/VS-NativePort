// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Effects_EffectInstance;
struct Effects_RendererSlot;

namespace VS::Effects {
struct LiveEffectTimeline;
struct LiveParticle;
}  // namespace VS::Effects

#include "Effects/ActionRequest.hpp"
#include "Effects/Package.hpp"
#include "Effects/SpawnRequest.hpp"
#include "Effects/TransformsLiveState.hpp"

// decomp: D_800F53B8_t (src/BATTLE/BATTLE.PRG/5BF94.h:490)
struct Effects_EffectInstance {
    struct Effects_EffectInstance* next;
    struct Effects_EffectInstance* parent;
    char particleCount;
    char priority;
    u_short scriptOffset;
    char* script;
    char registers[4];
    u_int token : 8;
    u_int busyBits : 3;
    u_int childCount : 5;
    u_int frame : 16;
    VS::Effects::LiveParticle* particles;
    VS::Effects::LiveTransformSlot transforms[16];
    VS::Effects::LiveEffectInstanceTargets context;
};

// decomp: effectExec (src/BATTLE/BATTLE.PRG/6E644.h:35)
using Effects_EffectRenderer = int (*)(Effects_RendererSlot*, u_int, int);

// decomp: func_800D4910_t (src/BATTLE/BATTLE.PRG/6E644.h:37)
struct Effects_RendererSlot {
    struct Effects_RendererSlot* next;
    Effects_EffectRenderer renderer;
    void* rendererState;
};
struct Effects_RendererSlot;

#include "DiscQueue.h"

extern u_char Effects_effectBufferIndex;
extern Effects_EffectRenderer Effects_scriptRenderers[2];
extern char Effects_actionVariantValues[21824];
extern int (*Effects_scriptOpcodeHandlers[49])(void*);
extern u_char Effects_lastReactionSoundVariant;
extern u_int Effects_emptyPackage[0x5200 / sizeof(u_int)];
extern int Effects_tintedActor;
extern int Effects_effectBusyFlags;
extern Content_DiscQueueSlot* Effects_menuReloadSlot;
extern u_int Effects_pendingTracks;
extern int Effects_trackCount;
extern int Effects_startedTrackCount;
extern u_char Effects_actionVariant;
extern VS::Effects::LiveActionEffectRequest Effects_actionRequest;
extern char Effects_primaryToken;
extern u_int Effects_primaryPhase;
extern int Effects_tokenBitmaps[215];
extern int Effects_curveSamples[211];
extern int Effects_unusedPrimaryWord;
extern Effects_EffectInstance* Effects_primaryInstances;
extern Effects_EffectInstance* Effects_currentInstance;
extern VS::Effects::LiveActionEffectRequest Effects_secondaryRequest;
extern char Effects_primaryLoaded;
extern Effects_EffectInstance* Effects_secondaryInstances;
extern int Effects_scriptSelector;
extern VS::Effects::LiveEffectPackage* Effects_savedPackage;
extern VS::Effects::LiveEffectPackage* Effects_package;
extern Effects_EffectRenderer Effects_rendererTableAlias[];
extern Effects_EffectRenderer Effects_effectRenderers[8];
extern VS::Effects::LiveEffectPackage Effects_actionPackage;
extern VS::Effects::LiveEffectPackage Effects_secondaryPackage;
