// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_RUNTIME_H_TYPES
#define VS_EFFECTS_RUNTIME_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_ActionControlKey Effects_ActionControlKey;
typedef struct Effects_ActionEffectRequest Effects_ActionEffectRequest;
typedef struct Effects_ActionEffectTarget Effects_ActionEffectTarget;
typedef struct Effects_ChildTrackKey Effects_ChildTrackKey;
typedef struct Effects_ChildTrackView Effects_ChildTrackView;
typedef struct Effects_ColorTrackKey Effects_ColorTrackKey;
typedef struct Effects_CurveBlock Effects_CurveBlock;
typedef struct Effects_EffectInstance Effects_EffectInstance;
typedef struct Effects_EffectInstanceTargets Effects_EffectInstanceTargets;
typedef struct Effects_EffectPackage Effects_EffectPackage;
typedef struct Effects_EffectSpawnRequest Effects_EffectSpawnRequest;
typedef struct Effects_EffectTimeline Effects_EffectTimeline;
typedef struct Effects_HitEffectRequest Effects_HitEffectRequest;
typedef struct Effects_PackageBlock3 Effects_PackageBlock3;
typedef struct Effects_PackageBlock5 Effects_PackageBlock5;
typedef struct Effects_PackageBlock9 Effects_PackageBlock9;
typedef struct Effects_PackageTexture Effects_PackageTexture;
typedef struct Effects_PackageTextureInfo Effects_PackageTextureInfo;
typedef struct Effects_Particle Effects_Particle;
typedef struct Effects_ParticleTrackKey Effects_ParticleTrackKey;
typedef struct Effects_RendererSlot Effects_RendererSlot;
typedef struct Effects_RendererTrackKey Effects_RendererTrackKey;
typedef struct Effects_ShakeChannelKey Effects_ShakeChannelKey;
typedef struct Effects_ShakeTrackKey Effects_ShakeTrackKey;
typedef struct Effects_TimelineTrack Effects_TimelineTrack;
typedef struct Effects_TimelineTrackHeader Effects_TimelineTrackHeader;
typedef struct Effects_TransformControlView Effects_TransformControlView;
typedef struct Effects_TransformDefinition Effects_TransformDefinition;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_EFFECTS_RUNTIME_H_SET_TYPES_ONLY
#endif
#include "Battle/ActionResolution.h"
#include "Effects/Transforms.h"
#ifdef VS_EFFECTS_RUNTIME_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_EFFECTS_RUNTIME_H_SET_TYPES_ONLY
#endif

// decomp: actionEffectTarget (src/BATTLE/BATTLE.PRG/6E644.h:21)
typedef struct Effects_ActionEffectTarget {
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
} Effects_ActionEffectTarget;

// decomp: D_800F53B8_t3 (src/BATTLE/BATTLE.PRG/5BF94.h:473)
typedef struct Effects_EffectInstanceTargets {
    Effects_ActionEffectTarget source;
    Effects_ActionEffectTarget emitFrom;
    Effects_ActionEffectTarget target;
    Effects_ActionEffectTarget emitTo;
    Effects_EffectTimeline* timeline;
    Effects_RendererSlot* renderer;
    short effectId;
    short phase;
    int parameter;
} Effects_EffectInstanceTargets;

// decomp: D_800F53B8_t (src/BATTLE/BATTLE.PRG/5BF94.h:490)
typedef struct Effects_EffectInstance {
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
    Effects_Particle* particles;
    Effects_TransformSlot transforms[16];
    Effects_EffectInstanceTargets context;
} Effects_EffectInstance;

// decomp: D_800F53B8_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:98)
typedef struct Effects_EffectSpawnRequest {
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
} Effects_EffectSpawnRequest;

// decomp: func_800D5780_t2 (src/BATTLE/BATTLE.PRG/5BF94.h:444)
typedef struct Effects_TimelineTrackHeader {
    u_short kind : 8;
    u_short keyCount : 8;
} Effects_TimelineTrackHeader;
_Static_assert(sizeof(Effects_TimelineTrackHeader) == 0x2, "Effects_TimelineTrackHeader must keep its PS1 layout");

// decomp: func_800D5780_t (src/BATTLE/BATTLE.PRG/5BF94.h:453)
typedef struct Effects_TimelineTrack {
    /* 0x0 */ u_long* keys;
    /* 0x4 */ Effects_TimelineTrackHeader header;
    /* 0x6 */ short keyIndex;
    /* 0x8 */ short curveFrame;
    /* 0xA */ short keyTimer;
    /* 0xC */ int keyParameter;
} Effects_TimelineTrack;

// decomp: D_800F53B8_t4 (src/BATTLE/BATTLE.PRG/5BF94.h:461)
typedef struct Effects_EffectTimeline {
    char trackCount;
    char flags;
    short frame;
    int activeTracks;
    Effects_TimelineTrack tracks[0];
} Effects_EffectTimeline;

// decomp: D_800F569C_t (src/BATTLE/BATTLE.PRG/5BF94.h:373)
typedef struct Effects_EffectPackage {
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
    Effects_TransformDefinition* block10Data;
    Effects_PackageBlock3* block3Data;
    u_char* block11Data;
    void* block12Data;
    void* block13Data;
    int auxiliarySoundHandle;
    Effects_ActionEffectRequest* request;
} Effects_EffectPackage;

// decomp: actionEffectRequest (src/BATTLE/BATTLE.PRG/6E644.h:31)
typedef struct Effects_ActionEffectRequest {
    u_short effectId;
    u_char count;
    u_char otherCount;
    Effects_ActionEffectTarget source;
    Effects_ActionEffectTarget targets[16];
    u_char others[16];
    SVECTOR position;
} Effects_ActionEffectRequest;

// decomp: func_8006C4C4_t (src/BATTLE/BATTLE.PRG/146C.c:244)
typedef struct Effects_HitEffectRequest {
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
} Effects_HitEffectRequest;
_Static_assert(sizeof(Effects_HitEffectRequest) == 0xE4, "Effects_HitEffectRequest must keep its PS1 layout");

// decomp: effectExec (src/BATTLE/BATTLE.PRG/6E644.h:35)
typedef int (*Effects_EffectRenderer)(Effects_RendererSlot*, u_int, int);

// decomp: func_800D4910_t (src/BATTLE/BATTLE.PRG/6E644.h:37)
struct Effects_RendererSlot {
    struct Effects_RendererSlot* next;
    Effects_EffectRenderer renderer;
    void* rendererState;
};
typedef struct Effects_RendererSlot Effects_RendererSlot;

// decomp: func_800D57FC_t (src/BATTLE/BATTLE.PRG/5BF94.c:36)
typedef struct Effects_ParticleTrackKey {
    u_int frame : 8;
    u_int duration : 8;
    u_int opaque2Bit0 : 4;
    u_int opaque2Bit4 : 6;
    u_int opaque3Bit2 : 6;
} Effects_ParticleTrackKey;
_Static_assert(sizeof(Effects_ParticleTrackKey) == 0x4, "Effects_ParticleTrackKey must keep its PS1 layout");

// decomp: func_800D57FC_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:41)
typedef struct Effects_ShakeTrackKey {
    unsigned reserved0Bit0 : 4;
    unsigned curve : 5;
} Effects_ShakeTrackKey;
_Static_assert(sizeof(Effects_ShakeTrackKey) == 0x4, "Effects_ShakeTrackKey must keep its PS1 layout");

// decomp: func_800D5A98_t (src/BATTLE/BATTLE.PRG/5BF94.c:335)
typedef struct Effects_ChildTrackKey {
    u_char actors[6];
    u_char final;
    u_char frame;
    u_char delay;
} Effects_ChildTrackKey;
_Static_assert(sizeof(Effects_ChildTrackKey) == 0x9, "Effects_ChildTrackKey must keep its PS1 layout");

// decomp: func_800D5A98_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:345)
typedef struct Effects_ChildTrackView {
    Effects_ChildTrackKey* event;
    Effects_TimelineTrackHeader reserved4;
    short reserved6;
    short reserved8;
    short reservedA;
    short delay;
    short index;
} Effects_ChildTrackView;

// decomp: func_800D5D74_t (src/BATTLE/BATTLE.PRG/5BF94.c:218)
typedef struct Effects_RendererTrackKey {
    u_int frame : 9;
    u_int opaque1Bit1 : 9;
    u_int effectRenderer : 3;
    u_int opaque2Bit5 : 6;
    u_int reserved3Bit3 : 5;
    u_short opaque4;
} Effects_RendererTrackKey;
_Static_assert(sizeof(Effects_RendererTrackKey) == 0x8, "Effects_RendererTrackKey must keep its PS1 layout");

// decomp: func_800D6310_t (src/BATTLE/BATTLE.PRG/5BF94.c:209)
typedef struct Effects_ColorTrackKey {
    short frame;
    short duration;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char mode;
} Effects_ColorTrackKey;
_Static_assert(sizeof(Effects_ColorTrackKey) == 0x8, "Effects_ColorTrackKey must keep its PS1 layout");

// decomp: func_800D6508_t (src/BATTLE/BATTLE.PRG/5BF94.c:106)
typedef struct Effects_TransformControlView {
    char reserved0[0x22];
    u_short attachmentMode : 2;
    u_short motionResumed : 1;
    u_short reserved22Bit3 : 13;
    char reserved24[0xAC];
} Effects_TransformControlView;
_Static_assert(sizeof(Effects_TransformControlView) == 0xD0, "Effects_TransformControlView must keep its PS1 layout");

// decomp: func_800D6894_t (src/BATTLE/BATTLE.PRG/5BF94.c:57)
typedef struct Effects_ActionControlKey {
    u_short frame;
    u_char opaque2;
    u_char opaque3;
} Effects_ActionControlKey;
_Static_assert(sizeof(Effects_ActionControlKey) == 0x4, "Effects_ActionControlKey must keep its PS1 layout");

// decomp: func_800D6A18_t (src/BATTLE/BATTLE.PRG/5BF94.c:231)
typedef struct Effects_ShakeChannelKey {
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
} Effects_ShakeChannelKey;
_Static_assert(sizeof(Effects_ShakeChannelKey) == 0x8, "Effects_ShakeChannelKey must keep its PS1 layout");

#endif // VS_EFFECTS_RUNTIME_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_RUNTIME_H_DECLS)
#define VS_EFFECTS_RUNTIME_H_DECLS
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
extern SVECTOR Effects_effectOrigin[109];
extern char Effects_primaryToken;
extern u_int Effects_primaryPhase;
extern int Effects_tokenBitmaps[215];
extern int Effects_curveSamples[211];
extern int Effects_unusedPrimaryWord;
extern Effects_EffectInstance* Effects_primaryInstances;
extern Effects_EffectInstance* Effects_currentInstance;
extern Effects_ActionEffectRequest Effects_secondaryRequest;
extern char Effects_primaryLoaded;
extern char Effects_actionVariantArgument;
extern Effects_EffectInstance* Effects_secondaryInstances;
extern int Effects_scriptSelector;
extern Effects_EffectPackage* Effects_savedPackage;
extern Effects_EffectPackage* Effects_package;
extern Effects_EffectRenderer Effects_rendererTableAlias[];
extern Effects_EffectRenderer Effects_effectRenderers[];
extern Effects_EffectPackage Effects_actionPackage;
extern Effects_ActionEffectRequest* Effects_actionRequestPointer;
extern Effects_EffectPackage Effects_secondaryPackage;
int Effects_IgnoreRuntimeDiagnostic(int arg0 __attribute__((unused)));
void Effects_InitEffectRuntime(void);
void Effects_ResetEffectInstances(void);
void Effects_InitEffectInstance(Effects_EffectSpawnRequest* arg0, Effects_EffectInstance* arg1);
Effects_EffectInstance* Effects_CreatePrimaryEffectInstance(Effects_EffectSpawnRequest* arg0);
void Effects_DestroyPrimaryEffectInstance(Effects_EffectInstance* arg0);
int Effects_UpdatePrimaryEffects(void);
int Effects_UpdateSecondaryEffects(void);
int Effects_CanStartEffectTrack(void);
int Effects_PrimaryEffectTrackCount(void);
void Effects_RequeueEffectTrack(int arg0);
int Effects_IsPrimaryEffectLoaded(void);
int Effects_StartEffectTrack(int arg0);
int Effects_UpdateEffectRuntime(void);
int Effects_BeginActionEffect(Effects_ActionEffectRequest* arg0, int arg1, int arg2);
void Effects_ClearPrimaryEffects(void);
char Effects_SpawnSecondaryEffects(Effects_ActionEffectRequest* arg0, int arg1, int arg2);
int Effects_BeginSecondaryEffects(Effects_ActionEffectRequest* arg0, int arg1);
int Effects_SpawnSecondaryEffectRequest(Effects_ActionEffectRequest* arg0, int arg1, int arg2);
void Effects_SetPrimaryEffectPhase(int arg0);
void Effects_SetSecondaryEffectPhase(int arg0, Effects_EffectInstance* arg1);
int Effects_PrimaryEffectPhase(void);
int Effects_AllocateEffectToken(void);
void Effects_ReleaseEffectToken(int arg0);
Effects_EffectInstance* Effects_CreateSecondaryEffectInstance(Effects_EffectSpawnRequest* arg0);
void Effects_DestroySecondaryEffectInstance(Effects_EffectInstance* arg0);
Effects_RendererSlot* Effects_AttachEffectRenderer(Effects_EffectInstance* arg0, Effects_EffectRenderer renderer, int arg2);
void Effects_DetachEffectRenderer(Effects_EffectInstance* arg0, Effects_RendererSlot* arg1);
void Effects_SpawnSourceActorEffect(void);
void Effects_ClearActionEffectMode(void);

#endif // VS_EFFECTS_RUNTIME_H_DECLS
