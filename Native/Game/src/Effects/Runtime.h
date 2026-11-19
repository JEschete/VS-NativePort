// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Effects_ActionControlKey;
struct Effects_ActionEffectRequest;
struct Effects_ActionEffectTarget;
struct Effects_ChildTrackKey;
struct Effects_ChildTrackView;
struct Effects_ColorTrackKey;
struct Effects_CurveBlock;
struct Effects_EffectInstance;
struct Effects_EffectInstanceTargets;
struct Effects_EffectPackage;
struct Effects_EffectSpawnRequest;
struct Effects_EffectTimeline;
struct Effects_HitEffectRequest;
struct Effects_PackageBlock3;
struct Effects_PackageBlock5;
struct Effects_PackageBlock9;
struct Effects_PackageTexture;
struct Effects_PackageTextureInfo;
struct Effects_ParticleTrackKey;
struct Effects_RendererSlot;
struct Effects_RendererTrackKey;
struct Effects_ShakeChannelKey;
struct Effects_ShakeTrackKey;
struct Effects_TimelineTrack;
struct Effects_TimelineTrackHeader;
struct Effects_TransformControlView;

namespace VS::Effects {
struct LiveParticle;
}  // namespace VS::Effects

#include "Battle/ActionResolution.h"
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
    Effects_EffectTimeline* timeline;
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

// decomp: func_800D5780_t2 (src/BATTLE/BATTLE.PRG/5BF94.h:444)
struct Effects_TimelineTrackHeader {
    u_short kind : 8;
    u_short keyCount : 8;
};
static_assert(sizeof(Effects_TimelineTrackHeader) == 0x2, "Effects_TimelineTrackHeader must keep its PS1 layout");

// decomp: func_800D5780_t (src/BATTLE/BATTLE.PRG/5BF94.h:453)
struct Effects_TimelineTrack {
    /* 0x0 */ u_long* keys;
    /* 0x4 */ Effects_TimelineTrackHeader header;
    /* 0x6 */ short keyIndex;
    /* 0x8 */ short curveFrame;
    /* 0xA */ short keyTimer;
    /* 0xC */ int keyParameter;
};

// decomp: D_800F53B8_t4 (src/BATTLE/BATTLE.PRG/5BF94.h:461)
struct Effects_EffectTimeline {
    char trackCount;
    char flags;
    short frame;
    int activeTracks;
    Effects_TimelineTrack tracks[0];
};

// decomp: D_800F569C_t (src/BATTLE/BATTLE.PRG/5BF94.h:373)
struct Effects_EffectPackage {
    int reserved0;
    int reserved4;
    Effects_PackageBlock5* block5Data;
    u_char* curves[32];
    Effects_CurveBlock* curveBlock;
    char* block8Data;
    int block1TableCount;
    Effects_PackageTextureInfo* textureInfoTable[4];
    Effects_PackageTexture* textures;
    int block2Count;
    u_short* block2Data;
    Effects_PackageBlock9* block9Data;
    VS::Effects::LiveTransformDefinition* block10Data;
    Effects_PackageBlock3* block3Data;
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

// decomp: func_8006C4C4_t (src/BATTLE/BATTLE.PRG/146C.c:244)
struct Effects_HitEffectRequest {
    short opaque0;
    char opaque2;
    char opaque3;
    short opaque4;
    short reserved6;
    char targetActor;
    char reserved9;
    char opaqueA;
    char reservedB;
    int reservedC;
    short opaque10;
    short opaque12;
    Battle_HitTargetWord opaque14;
    int reserved1C[50];
};
static_assert(sizeof(Effects_HitEffectRequest) == 0xE4, "Effects_HitEffectRequest must keep its PS1 layout");

// decomp: effectExec (src/BATTLE/BATTLE.PRG/6E644.h:35)
using Effects_EffectRenderer = int (*)(Effects_RendererSlot*, u_int, int);

// decomp: func_800D4910_t (src/BATTLE/BATTLE.PRG/6E644.h:37)
struct Effects_RendererSlot {
    struct Effects_RendererSlot* next;
    Effects_EffectRenderer renderer;
    void* rendererState;
};
struct Effects_RendererSlot;

// decomp: func_800D57FC_t (src/BATTLE/BATTLE.PRG/5BF94.c:36)
struct Effects_ParticleTrackKey {
    u_int frame : 8;
    u_int duration : 8;
    u_int opaque2Bit0 : 4;
    u_int opaque2Bit4 : 6;
    u_int opaque3Bit2 : 6;
};
static_assert(sizeof(Effects_ParticleTrackKey) == 0x4, "Effects_ParticleTrackKey must keep its PS1 layout");

// decomp: func_800D57FC_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:41)
struct Effects_ShakeTrackKey {
    unsigned reserved0Bit0 : 4;
    unsigned curve : 5;
};
static_assert(sizeof(Effects_ShakeTrackKey) == 0x4, "Effects_ShakeTrackKey must keep its PS1 layout");

// decomp: func_800D5A98_t (src/BATTLE/BATTLE.PRG/5BF94.c:335)
struct Effects_ChildTrackKey {
    u_char actors[6];
    u_char final;
    u_char frame;
    u_char delay;
};
static_assert(sizeof(Effects_ChildTrackKey) == 0x9, "Effects_ChildTrackKey must keep its PS1 layout");

// decomp: func_800D5A98_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:345)
struct Effects_ChildTrackView {
    Effects_ChildTrackKey* event;
    Effects_TimelineTrackHeader reserved4;
    short reserved6;
    short reserved8;
    short reservedA;
    short delay;
    short index;
};

// decomp: func_800D5D74_t (src/BATTLE/BATTLE.PRG/5BF94.c:218)
struct Effects_RendererTrackKey {
    u_int frame : 9;
    u_int opaque1Bit1 : 9;
    u_int effectRenderer : 3;
    u_int opaque2Bit5 : 6;
    u_int reserved3Bit3 : 5;
    u_short opaque4;
};
static_assert(sizeof(Effects_RendererTrackKey) == 0x8, "Effects_RendererTrackKey must keep its PS1 layout");

// decomp: func_800D6310_t (src/BATTLE/BATTLE.PRG/5BF94.c:209)
struct Effects_ColorTrackKey {
    short frame;
    short duration;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char mode;
};
static_assert(sizeof(Effects_ColorTrackKey) == 0x8, "Effects_ColorTrackKey must keep its PS1 layout");

// decomp: func_800D6508_t (src/BATTLE/BATTLE.PRG/5BF94.c:106)
struct Effects_TransformControlView {
    char reserved0[0x22];
    u_short attachmentMode : 2;
    u_short motionResumed : 1;
    u_short reserved22Bit3 : 13;
    char reserved24[0xAC];
};
static_assert(sizeof(Effects_TransformControlView) == 0xD0, "Effects_TransformControlView must keep its PS1 layout");

// decomp: func_800D6894_t (src/BATTLE/BATTLE.PRG/5BF94.c:57)
struct Effects_ActionControlKey {
    u_short frame;
    u_char opaque2;
    u_char opaque3;
};
static_assert(sizeof(Effects_ActionControlKey) == 0x4, "Effects_ActionControlKey must keep its PS1 layout");

// decomp: func_800D6A18_t (src/BATTLE/BATTLE.PRG/5BF94.c:231)
struct Effects_ShakeChannelKey {
    u_short frame : 9;
    u_short reserved1Bit1 : 7;
    u_short reserved2;
    u_int opaque4Bit0 : 4;
    u_int opaque4Bit4 : 5;
    u_int opaque5Bit1 : 7;
    u_int opaque6Bit0 : 1;
    u_int opaque6Bit1 : 7;
    u_int opaque7Bit0 : 1;
    u_int opaque7Bit1 : 7;
};
static_assert(sizeof(Effects_ShakeChannelKey) == 0x8, "Effects_ShakeChannelKey must keep its PS1 layout");

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
