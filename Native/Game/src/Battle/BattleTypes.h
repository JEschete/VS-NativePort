// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Battle_EffectDefinitionView;
struct Battle_EffectParameterBytes;
struct Battle_EffectRendererState;
struct Battle_EffectScriptWord;
struct Battle_EffectTrailConfig;
struct Battle_EffectTransformBlock;
struct Battle_EffectTransformDefinition;
struct Battle_EffectTransformState;
struct Battle_EnvironmentTarget;
struct Battle_EnvironmentTargetList;
struct Battle_FloatingLabelSpec;
struct Battle_HitStatusView;
struct Battle_LimbEffects;
struct Battle_ShapeVertex;
struct Battle_TargetInfoLine;
struct Battle_TargetLabelLine;
struct Battle_TargetLabelPrimitive;
struct Battle_TargetSphereLabelEntry;
struct Battle_TargetSphereLabelView;
struct Battle_TargetVolume;
struct Battle_TargetVolumeFlags;
struct Battle_TargetVolumeMatrix;
union Battle_VectorWords;

#include "Effects/Runtime.h"
#include "Render/LegacyPackets.h"

// decomp: effectDefinitionView (src/BATTLE/BATTLE.PRG/5BF94.c:617)
struct Battle_EffectDefinitionView {
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
};
static_assert(sizeof(Battle_EffectDefinitionView) == 0x38, "Battle_EffectDefinitionView must keep its PS1 layout");

// decomp: effectTrailConfig (src/BATTLE/BATTLE.PRG/5BF94.c:121)
struct Battle_EffectTrailConfig {
    u_int actor : 8;
    u_int bone : 8;
    u_int color : 4;
    u_int texture : 4;
    u_int life : 8;
};
static_assert(sizeof(Battle_EffectTrailConfig) == 0x4, "Battle_EffectTrailConfig must keep its PS1 layout");

// decomp: effectTransformState (src/BATTLE/BATTLE.PRG/5BF94.c:602)
struct Battle_EffectTransformState {
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
};
static_assert(sizeof(Battle_EffectTransformState) == 0xD0, "Battle_EffectTransformState must keep its PS1 layout");

// decomp: environmentTarget (src/BATTLE/BATTLE.PRG/146C.c:349)
struct Battle_EnvironmentTarget {
    short x;
    short y;
    short z;
    short radius;
    char reserved8[56];
    int kind;
};
static_assert(sizeof(Battle_EnvironmentTarget) == 0x44, "Battle_EnvironmentTarget must keep its PS1 layout");

// decomp: environmentTargetList (src/BATTLE/BATTLE.PRG/146C.c:356)
struct Battle_EnvironmentTargetList {
    u_short action;
    char reserved2[72];
    u_short count;
    Battle_EnvironmentTarget targets[30];
};
static_assert(sizeof(Battle_EnvironmentTargetList) == 0x844, "Battle_EnvironmentTargetList must keep its PS1 layout");

// decomp: func_80085A34_t (src/BATTLE/BATTLE.PRG/146C.c:227)
struct Battle_HitStatusView {
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
};
static_assert(sizeof(Battle_HitStatusView) == 0x44, "Battle_HitStatusView must keep its PS1 layout");

// decomp: func_800962E4_t (src/BATTLE/BATTLE.PRG/2842C.c:83)
struct Battle_FloatingLabelSpec {
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
};
static_assert(sizeof(Battle_FloatingLabelSpec) == 0x10, "Battle_FloatingLabelSpec must keep its PS1 layout");

// decomp: func_800C0FA8_t2 (src/BATTLE/BATTLE.PRG/58578.c:69)
struct Battle_TargetVolumeMatrix {
    SVECTOR opaque0;
    SVECTOR opaque8;
    short opaque10[4];
    short opaque18[4];
};
static_assert(sizeof(Battle_TargetVolumeMatrix) == 0x20, "Battle_TargetVolumeMatrix must keep its PS1 layout");

// decomp: func_800C1564_flags (src/BATTLE/BATTLE.PRG/58578.h:10)
struct Battle_TargetVolumeFlags {
    u_int reserved0Bit0 : 8;
    u_int opaque1Bit0 : 8;
    u_int reserved2Bit0 : 8;
    u_int opaque3Bit0 : 8;
};
static_assert(sizeof(Battle_TargetVolumeFlags) == 0x4, "Battle_TargetVolumeFlags must keep its PS1 layout");

// decomp: func_800C1564_t (src/BATTLE/BATTLE.PRG/58578.h:21)
struct Battle_TargetVolume {
    u_char kind;
    u_char opaque1;
    short angle;
    union {
        Battle_TargetVolumeFlags flags;
        u_char extents[4];
    } scale;
    short position[4];
};

// decomp: func_800C4650_t (src/BATTLE/BATTLE.PRG/58578.c:62)
struct Battle_TargetLabelLine {
    u_short opaque0[3];
    u_char reserved6;
    u_char reserved7;
    u_char reserved8;
    u_char opaque9;
    signed char opaqueA;
    char reservedB[0xD];
};
static_assert(sizeof(Battle_TargetLabelLine) == 0x18, "Battle_TargetLabelLine must keep its PS1 layout");

// decomp: func_800C56C0_t2 (src/BATTLE/BATTLE.PRG/5BF94.c:276)
struct Battle_TargetSphereLabelEntry {
    char opaque0;
    char opaque1[3];
    short opaque4;
    u_char opaque6;
    char opaque7;
    int opaque8;
    int opaqueC;
    char* limbName;
};

// decomp: func_800C56C0_t (src/BATTLE/BATTLE.PRG/5BF94.c:300)
struct Battle_TargetSphereLabelView {
    Battle_TargetVolume opaque0[2];
    short reserved20;
    short opaque22;
    int reserved24;
    short reserved28;
    short opaque2A;
    u_char opaque2C;
    u_char opaque2D;
    u_char opaque2E;
    u_char opaque2F;
    signed char opaque30;
    u_char opaque31;
    char reserved32[2];
    int opaque34;
    SVECTOR opaque38;
    Battle_TargetInfoLine* opaque40;
    Battle_TargetInfoLine* opaque44;
    char reserved48[0x948];
    Battle_TargetSphereLabelEntry opaque990[24];
    char reservedB70[0x3EE4];
    char textBuffer[200];
};

// decomp: func_800C5798_t (src/BATTLE/BATTLE.PRG/5BF94.c:265)
struct Battle_TargetInfoLine {
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
};
static_assert(sizeof(Battle_TargetInfoLine) == 0x18, "Battle_TargetInfoLine must keep its PS1 layout");

// decomp: func_800CFE1C_t (src/BATTLE/BATTLE.PRG/5BF94.h:208)
struct Battle_EffectScriptWord {
    short reserved0;
    short reserved2;
};
static_assert(sizeof(Battle_EffectScriptWord) == 0x4, "Battle_EffectScriptWord must keep its PS1 layout");

// decomp: func_800D0548_arg0 (src/BATTLE/BATTLE.PRG/5BF94.c:578)
struct Battle_EffectTransformDefinition {
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
};
static_assert(sizeof(Battle_EffectTransformDefinition) == 0x14, "Battle_EffectTransformDefinition must keep its PS1 layout");

// decomp: func_800D0C60_t (src/BATTLE/BATTLE.PRG/5BF94.c:558)
struct Battle_EffectRendererState {
    u_char reserved0[0x34];
    u_char reserved34;
    u_char reserved35;
    char reserved36[0x42];
    MATRIX reserved78;
    char reserved98[0x26];
    short reservedBE;
};
static_assert(sizeof(Battle_EffectRendererState) == 0xC0, "Battle_EffectRendererState must keep its PS1 layout");

// decomp: func_800D6048_t (src/BATTLE/BATTLE.PRG/5BF94.c:562)
struct Battle_EffectTransformBlock {
    char reserved0[0x10];
    Effects_ActionEffectTarget reserved10[16];
};

// decomp: func_80103080_t (src/BATTLE/BATTLE.PRG/146C.h:295)
struct Battle_EffectParameterBytes {
    u_short reserved0;
    u_short reserved2;
    char reserved4;
    char reserved5;
    char reserved6;
    char reserved7;
};
static_assert(sizeof(Battle_EffectParameterBytes) == 0x8, "Battle_EffectParameterBytes must keep its PS1 layout");

// decomp: labelBoxPrim_t (src/BATTLE/BATTLE.PRG/58578.c:78)
struct Battle_TargetLabelPrimitive {
    u_long tag;
    struct {
        Render_TilePacket tile;
        Render_PagedGouraudQuadPacket shadePoly;
        u_long texturePageCommand;
    } box;
};

// decomp: limbEffects_t (src/BATTLE/BATTLE.PRG/146C.c:42)
struct Battle_LimbEffects {
    short opaque0;
    short opaque2;
};
static_assert(sizeof(Battle_LimbEffects) == 0x4, "Battle_LimbEffects must keep its PS1 layout");

// decomp: menuShapeVertex (src/BATTLE/BATTLE.PRG/573B8.h:8)
struct Battle_ShapeVertex {
    short xyz[3];
    short opaque6;
};
static_assert(sizeof(Battle_ShapeVertex) == 0x8, "Battle_ShapeVertex must keep its PS1 layout");

// decomp: func_800C4794_t (src/BATTLE/BATTLE.PRG/5BF94.c:566)
union Battle_VectorWords {
    SVECTOR vector;
    int words[2];
};
static_assert(sizeof(Battle_VectorWords) == 0x8, "Battle_VectorWords must keep its PS1 layout");
