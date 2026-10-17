// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_BATTLETYPES_H_TYPES
#define VS_BATTLE_BATTLETYPES_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_EffectDefinitionView Battle_EffectDefinitionView;
typedef struct Battle_EffectParameterBytes Battle_EffectParameterBytes;
typedef struct Battle_EffectRendererState Battle_EffectRendererState;
typedef struct Battle_EffectScriptWord Battle_EffectScriptWord;
typedef struct Battle_EffectTrailConfig Battle_EffectTrailConfig;
typedef struct Battle_EffectTransformBlock Battle_EffectTransformBlock;
typedef struct Battle_EffectTransformDefinition Battle_EffectTransformDefinition;
typedef struct Battle_EffectTransformState Battle_EffectTransformState;
typedef struct Battle_EnvironmentTarget Battle_EnvironmentTarget;
typedef struct Battle_EnvironmentTargetList Battle_EnvironmentTargetList;
typedef struct Battle_FloatingLabelSpec Battle_FloatingLabelSpec;
typedef struct Battle_HitStatusView Battle_HitStatusView;
typedef struct Battle_LimbEffects Battle_LimbEffects;
typedef struct Battle_ShapeVertex Battle_ShapeVertex;
typedef struct Battle_TargetInfoLine Battle_TargetInfoLine;
typedef struct Battle_TargetLabelLine Battle_TargetLabelLine;
typedef struct Battle_TargetLabelPrimitive Battle_TargetLabelPrimitive;
typedef struct Battle_TargetSphereLabelEntry Battle_TargetSphereLabelEntry;
typedef struct Battle_TargetSphereLabelView Battle_TargetSphereLabelView;
typedef struct Battle_TargetVolume Battle_TargetVolume;
typedef struct Battle_TargetVolumeFlags Battle_TargetVolumeFlags;
typedef struct Battle_TargetVolumeMatrix Battle_TargetVolumeMatrix;
typedef union Battle_VectorWords Battle_VectorWords;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_BATTLETYPES_H_SET_TYPES_ONLY
#endif
#include "Effects/Runtime.h"
#include "Render/LegacyPackets.h"
#ifdef VS_BATTLE_BATTLETYPES_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_BATTLETYPES_H_SET_TYPES_ONLY
#endif

// decomp: effectDefinitionView (src/BATTLE/BATTLE.PRG/5BF94.c:617)
typedef struct Battle_EffectDefinitionView {
    u_short flags;
    u_short attachment;
    u_char reserved4;
    u_char reserved5;
    u_char reserved6;
    u_char reserved7;
    int rotationXAndTarget;
    int rotationY;
    int rotationZ;
    short reserved14[6];
    short reserved20[6];
    short reserved2C[6];
} Battle_EffectDefinitionView;

// decomp: effectTrailConfig (src/BATTLE/BATTLE.PRG/5BF94.c:121)
typedef struct Battle_EffectTrailConfig {
    u_int actor : 8;
    u_int bone : 8;
    u_int color : 4;
    u_int texture : 4;
    u_int life : 8;
} Battle_EffectTrailConfig;

// decomp: effectTransformState (src/BATTLE/BATTLE.PRG/5BF94.c:602)
typedef struct Battle_EffectTransformState {
    short rotation[3];
    u_short opaque6;
    u_char reserved8[6];
    short frame;
    u_char reserved10[12];
    VECTOR position;
    u_char reserved2C[8];
    u_char actor, bone, targetActor, targetBone;
    MATRIX reserved38, reserved58, attachment, facing;
    SVECTOR saved, reservedC0, reservedC8;
} Battle_EffectTransformState;

// decomp: environmentTarget (src/BATTLE/BATTLE.PRG/146C.c:349)
typedef struct Battle_EnvironmentTarget {
    short x;
    short y;
    short z;
    short radius;
    char reserved8[56];
    int kind;
} Battle_EnvironmentTarget;

// decomp: environmentTargetList (src/BATTLE/BATTLE.PRG/146C.c:356)
typedef struct Battle_EnvironmentTargetList {
    u_short action;
    char reserved2[72];
    u_short count;
    Battle_EnvironmentTarget targets[30];
} Battle_EnvironmentTargetList;

// decomp: func_80085A34_t (src/BATTLE/BATTLE.PRG/146C.c:227)
typedef struct Battle_HitStatusView {
    char reserved0;
    char reserved1;
    char opaque2;
    char opaque3;
    int reserved4;
    int reserved8;
    int reservedC;
    int reserved10;
    int opaque14;
    int opaque18;
    int opaque1C;
    int reserved20[8];
    int opaque40;
} Battle_HitStatusView;

// decomp: func_800962E4_t (src/BATTLE/BATTLE.PRG/2842C.c:83)
typedef struct Battle_FloatingLabelSpec {
    u_short opaque0;
    u_short opaque2;
    u_short opaque4;
    u_short reserved6;
    char opaque8;
    char reserved9;
    char reservedA;
    char opaqueB;
    short reservedC;
    char reservedE;
    char reservedF;
} Battle_FloatingLabelSpec;

// decomp: func_800C0FA8_t2 (src/BATTLE/BATTLE.PRG/58578.c:69)
typedef struct Battle_TargetVolumeMatrix {
    SVECTOR opaque0;
    SVECTOR opaque8;
    short opaque10[4];
    short opaque18[4];
} Battle_TargetVolumeMatrix;

// decomp: func_800C1564_flags (src/BATTLE/BATTLE.PRG/58578.h:10)
typedef struct Battle_TargetVolumeFlags {
    u_int reserved0Bit0 : 8;
    u_int opaque1Bit0 : 8;
    u_int reserved2Bit0 : 8;
    u_int opaque3Bit0 : 8;
} Battle_TargetVolumeFlags;

// decomp: func_800C1564_t (src/BATTLE/BATTLE.PRG/58578.h:21)
typedef struct Battle_TargetVolume {
    u_char kind;
    u_char opaque1;
    short angle;
    union {
        Battle_TargetVolumeFlags flags;
        u_char extents[4];
    } scale;
    short position[4];
} Battle_TargetVolume;

// decomp: func_800C4650_t (src/BATTLE/BATTLE.PRG/58578.c:62)
typedef struct Battle_TargetLabelLine {
    u_short opaque0[3];
    u_char reserved6;
    u_char reserved7;
    u_char reserved8;
    u_char opaque9;
    signed char opaqueA;
    char reservedB[0xD];
} Battle_TargetLabelLine;

// decomp: func_800C56C0_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:276)
typedef struct Battle_TargetSphereLabelEntry {
    char opaque0;
    char opaque1[3];
    short opaque4;
    u_char opaque6;
    char opaque7;
    int opaque8;
    int opaqueC;
    char* limbName;
} Battle_TargetSphereLabelEntry;

// layout: ledger (upstream merge) for func_800C56C0_t
typedef struct Battle_TargetSphereLabelView {
    /* 0x00 */ Battle_TargetVolume opaque0[2];
    /* 0x20 */ short reserved20;
    /* 0x22 */ short opaque22;
    /* 0x24 */ int reserved24;
    /* 0x28 */ short reserved28;
    /* 0x2A */ short opaque2A;
    /* 0x2C */ u_char opaque2C;
    /* 0x2D */ u_char opaque2D;
    /* 0x2E */ u_char opaque2E;
    /* 0x2F */ u_char opaque2F;
    /* 0x30 */ signed char opaque30;
    /* 0x31 */ u_char opaque31;
    /* 0x32 */ char reserved32[2];
    /* 0x34 */ int opaque34;
    /* 0x38 */ SVECTOR opaque38;
    /* 0x40 */ Battle_TargetInfoLine *opaque40;
    /* 0x44 */ Battle_TargetInfoLine *opaque44;
    /* 0x48 */ char reserved48[2376];
    /* 0x990 */ Battle_TargetSphereLabelEntry opaque990[24];
    /* 0xB70 */ char reservedB70[16100];
    /* 0x4A54 */ char textBuffer[200];
} Battle_TargetSphereLabelView;

// decomp: func_800C5798_t (src/BATTLE/BATTLE.PRG/5BF94.c:265)
typedef struct Battle_TargetInfoLine {
    int opaque0;
    int opaque4;
    u_char opaque8;
    u_char opaque9;
    signed char opaqueA;
    u_char opaqueB;
    u_char opaqueC;
    u_char reservedD;
    short opaqueE;
    int opaque10;
    u_char opaque14;
    u_char opaque15;
    u_char opaque16;
    u_char opaque17;
} Battle_TargetInfoLine;

// decomp: func_800CFE1C_t (src/BATTLE/BATTLE.PRG/5BF94.h:208)
typedef struct Battle_EffectScriptWord {
    short reserved0;
    short reserved2;
} Battle_EffectScriptWord;

// decomp: func_800D0548_arg0 (src/BATTLE/BATTLE.PRG/5BF94.c:578)
typedef struct Battle_EffectTransformDefinition {
    u_short reserved0;
    u_char reserved2[3];
    u_char reserved5;
    u_char reserved6[4];
    u_short reservedA;
    u_char reservedC[2];
    u_char reservedE;
    u_char reservedF;
    u_short reserved10;
    u_short reserved12;
} Battle_EffectTransformDefinition;

// decomp: func_800D0C60_t (src/BATTLE/BATTLE.PRG/5BF94.c:558)
typedef struct Battle_EffectRendererState {
    u_char reserved0[0x34];
    u_char reserved34;
    u_char reserved35;
    char reserved36[0x42];
    MATRIX reserved78;
    char reserved98[0x26];
    short reservedBE;
} Battle_EffectRendererState;

// decomp: func_800D6048_t (src/BATTLE/BATTLE.PRG/5BF94.c:562)
typedef struct Battle_EffectTransformBlock {
    char reserved0[0x10];
    Effects_ActionEffectTarget reserved10[16];
} Battle_EffectTransformBlock;

// decomp: func_80103080_t (src/BATTLE/BATTLE.PRG/146C.h:295)
typedef struct Battle_EffectParameterBytes {
    u_short reserved0;
    u_short reserved2;
    char reserved4;
    char reserved5;
    char reserved6;
    char reserved7;
} Battle_EffectParameterBytes;

// decomp: labelBoxPrim_t (src/BATTLE/BATTLE.PRG/58578.c:78)
typedef struct Battle_TargetLabelPrimitive {
    u_long tag;
    struct {
        Render_TilePacket tile;
        Render_PagedGouraudQuadPacket shadePoly;
        u_long texturePageCommand;
    } box;
} Battle_TargetLabelPrimitive;

// decomp: limbEffects_t (src/BATTLE/BATTLE.PRG/146C.c:42)
typedef struct Battle_LimbEffects {
    short opaque0;
    short opaque2;
} Battle_LimbEffects;

// decomp: menuShapeVertex (src/BATTLE/BATTLE.PRG/573B8.h:8)
typedef struct Battle_ShapeVertex {
    short xyz[3];
    short opaque6;
} Battle_ShapeVertex;

// decomp: func_800C4794_t (src/BATTLE/BATTLE.PRG/5BF94.c:566)
typedef union Battle_VectorWords {
    SVECTOR vector;
    int words[2];
} Battle_VectorWords;

#endif // VS_BATTLE_BATTLETYPES_H_TYPES
