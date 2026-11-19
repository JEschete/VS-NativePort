// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Effects_ActionEffectRequest;
struct Effects_ActionEffectTarget;
struct Effects_EffectInstance;
struct Effects_EffectInstanceTargets;
struct Effects_EffectPackage;
struct Effects_EffectSpawnRequest;
struct Effects_RendererSlot;

namespace VS::Effects {
struct LiveCurveTable;
struct LiveEffectTimeline;
struct LivePackageParameters;
struct LivePackageTexture;
struct LivePackageTextureFrame;
struct LiveParticle;
struct LiveParticleDefinitionTable;
struct LiveTimelineTable;
}  // namespace VS::Effects

#include "Effects/TransformsLiveState.hpp"

// decomp: D_800F5310 (D_800F5310@BATTLE/BATTLE.PRG): a view into another global
#define Effects_effectOrigin ((SVECTOR*)(&Effects_actionRequest.position))

// decomp: D_800F5798 (D_800F5798@BATTLE/BATTLE.PRG): a view into another global
#define Effects_actionRequestPointer (*(Effects_ActionEffectRequest**)(&Effects_actionPackage.request))

// decomp: actionEffectTarget (src/BATTLE/BATTLE.PRG/6E644.h:21)
struct Effects_ActionEffectTarget {
    u_short type;
    u_short colorPreset;
    union {
        struct {
            u_int actor : 8;
            u_int eventActorSlot : 8;
            u_int part : 8;
            u_int reserved3 : 8;
        } actor;
        SVECTOR position;
    } value;
};

// decomp: D_800F53B8_t3 (src/BATTLE/BATTLE.PRG/5BF94.h:473)
struct Effects_EffectInstanceTargets {
    Effects_ActionEffectTarget source;
    Effects_ActionEffectTarget emitFrom;
    Effects_ActionEffectTarget target;
    Effects_ActionEffectTarget emitTo;
    VS::Effects::LiveEffectTimeline* timeline;
    Effects_RendererSlot* renderer;
    short effectId;
    short phase;
    int parameter;
};

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
    Effects_EffectInstanceTargets context;
};

// decomp: D_800F53B8_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:98)
struct Effects_EffectSpawnRequest {
    Effects_ActionEffectTarget* source;
    Effects_ActionEffectTarget* emitFrom;
    Effects_ActionEffectTarget* target;
    Effects_ActionEffectTarget* emitTo;
    int priority;
    char* script;
    int scriptOffset;
    Effects_EffectInstance* parent;
    u_char token;
    u_char reserved21;
    u_char reserved22;
    u_char reserved23;
    int reserved24;
};

// decomp: D_800F569C_t (src/BATTLE/BATTLE.PRG/5BF94.h:373)
struct Effects_EffectPackage {
    int reserved0;
    int reserved4;
    VS::Effects::LiveParticleDefinitionTable* block5Data;
    u_char* curves[32];
    VS::Effects::LiveCurveTable* curveBlock;
    char* block8Data;
    int block1TableCount;
    VS::Effects::LivePackageTextureFrame* textureInfoTable[4];
    VS::Effects::LivePackageTexture* textures;
    int block2Count;
    u_short* block2Data;
    VS::Effects::LivePackageParameters* block9Data;
    VS::Effects::LiveTransformDefinition* block10Data;
    VS::Effects::LiveTimelineTable* block3Data;
    u_char* block11Data;
    void* block12Data;
    void* block13Data;
    int auxiliarySoundHandle;
    Effects_ActionEffectRequest* request;
};

// decomp: actionEffectRequest (src/BATTLE/BATTLE.PRG/6E644.h:31)
struct Effects_ActionEffectRequest {
    u_short effectId;
    u_char count;
    u_char otherCount;
    Effects_ActionEffectTarget source;
    Effects_ActionEffectTarget targets[16];
    u_char others[16];
    SVECTOR position;
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
extern Effects_ActionEffectRequest Effects_actionRequest;
extern char Effects_primaryToken;
extern u_int Effects_primaryPhase;
extern int Effects_tokenBitmaps[215];
extern int Effects_curveSamples[211];
extern int Effects_unusedPrimaryWord;
extern Effects_EffectInstance* Effects_primaryInstances;
extern Effects_EffectInstance* Effects_currentInstance;
extern Effects_ActionEffectRequest Effects_secondaryRequest;
extern char Effects_primaryLoaded;
extern Effects_EffectInstance* Effects_secondaryInstances;
extern int Effects_scriptSelector;
extern Effects_EffectPackage* Effects_savedPackage;
extern Effects_EffectPackage* Effects_package;
extern Effects_EffectRenderer Effects_rendererTableAlias[];
extern Effects_EffectRenderer Effects_effectRenderers[8];
extern Effects_EffectPackage Effects_actionPackage;
extern Effects_EffectPackage Effects_secondaryPackage;
