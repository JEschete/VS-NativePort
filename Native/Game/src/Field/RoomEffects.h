// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ROOMEFFECTS_H_TYPES
#define VS_FIELD_ROOMEFFECTS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_RoomEffectChunkHeader Field_RoomEffectChunkHeader;
typedef struct Field_RoomEffectCommand Field_RoomEffectCommand;
typedef struct Field_RoomEffectCommandHeader Field_RoomEffectCommandHeader;
typedef struct Field_RoomEffectInstance Field_RoomEffectInstance;
typedef struct Field_RoomEffectObject Field_RoomEffectObject;
typedef struct Field_RoomEffectObjectView Field_RoomEffectObjectView;
typedef struct Field_RoomEffectScriptHeader Field_RoomEffectScriptHeader;
typedef struct Field_SwingEffectRecord Field_SwingEffectRecord;
typedef struct Field_TextureFrameEffect Field_TextureFrameEffect;
typedef struct Field_TextureRegionScrollEffect Field_TextureRegionScrollEffect;
typedef struct Field_TextureScrollEffect Field_TextureScrollEffect;

// decomp: D_800F1DD8_t (src/BATTLE/BATTLE.PRG/2842C.c:58)
typedef struct Field_RoomEffectInstance {
    char isActive;
    u_char kindOrObjectCount;
    char isUpdateEnabled;
    char reserved03;
    Field_SwingEffectRecord* currentData;
    Field_SwingEffectRecord* originalData;
} Field_RoomEffectInstance;

// decomp: D_800F1DD8_t2 (src/BATTLE/BATTLE.PRG/2842C.c:49)
typedef struct Field_SwingEffectRecord {
    int geometryOffset;
    char needsReset;
    char startsInactive;
    u_char kind;
    char axis;
    int angularVelocity;
    int acceleration;
    int dampingShift;
    int angle;
    int restAngle;
    int startAngle;
    int initialVelocity;
} Field_SwingEffectRecord;
_Static_assert(sizeof(Field_SwingEffectRecord) == 0x24, "Field_SwingEffectRecord must keep its PS1 layout");

// decomp: func_80091C6C_t (src/BATTLE/BATTLE.PRG/2842C.c:133)
typedef struct Field_TextureFrameEffect {
    int geometryOffset;
    char reserved04[3];
    u_char delay;
    u_char baseU;
    u_char baseV;
    u_char frameCount;
    u_char columns;
    u_char frameWidth;
    u_char frameHeight;
    u_char tick;
    u_char frame;
    u_char u[4];
    u_char v[4];
} Field_TextureFrameEffect;
_Static_assert(sizeof(Field_TextureFrameEffect) == 0x18, "Field_TextureFrameEffect must keep its PS1 layout");

// decomp: func_80091E10_t (src/BATTLE/BATTLE.PRG/2842C.c:171)
typedef struct Field_TextureScrollEffect {
    int geometryOffset;
    char reserved04[3];
    u_char delay;
    u_char baseU;
    u_char baseV;
    u_char width;
    u_char height;
    u_char currentU;
    u_char currentV;
    u_char windowWidth;
    u_char windowHeight;
    signed char stepU;
    signed char stepV;
    u_char tick;
    char reserved13;
    u_char u[4];
    u_char v[4];
} Field_TextureScrollEffect;
_Static_assert(sizeof(Field_TextureScrollEffect) == 0x1C, "Field_TextureScrollEffect must keep its PS1 layout");

// decomp: func_80091FE8_t (src/BATTLE/BATTLE.PRG/2842C.c:182)
typedef struct Field_TextureRegionScrollEffect {
    u_char tick;
    u_char frame;
    u_char frameCount;
    u_char columns;
    char reserved04[3];
    u_char delay;
    int scrollStep;
    RECT rect;
} Field_TextureRegionScrollEffect;
_Static_assert(sizeof(Field_TextureRegionScrollEffect) == 0x14, "Field_TextureRegionScrollEffect must keep its PS1 layout");

// decomp: func_80092548_t (src/BATTLE/BATTLE.PRG/2842C.c:189)
typedef struct Field_RoomEffectScriptHeader {
    u_char objectCount;
    u_char reserved01;
    u_char isActive;
    u_char reserved03;
} Field_RoomEffectScriptHeader;
_Static_assert(sizeof(Field_RoomEffectScriptHeader) == 0x4, "Field_RoomEffectScriptHeader must keep its PS1 layout");

// decomp: func_80092548_t2 (src/BATTLE/BATTLE.PRG/2842C.c:194)
typedef struct Field_RoomEffectChunkHeader {
    short size;
    short type;
} Field_RoomEffectChunkHeader;
_Static_assert(sizeof(Field_RoomEffectChunkHeader) == 0x4, "Field_RoomEffectChunkHeader must keep its PS1 layout");

// decomp: func_80092B04_t (src/BATTLE/BATTLE.PRG/2842C.c:69)
typedef struct Field_RoomEffectObjectView {
    int sceneEntry;
    void* scriptCursor;
    char reserved08[0x2C];
} Field_RoomEffectObjectView;

// decomp: func_80092B04_t2 (src/BATTLE/BATTLE.PRG/2842C.c:63)
typedef struct Field_RoomEffectCommandHeader {
    short size;
    short opcode;
} Field_RoomEffectCommandHeader;
_Static_assert(sizeof(Field_RoomEffectCommandHeader) == 0x4, "Field_RoomEffectCommandHeader must keep its PS1 layout");

// decomp: func_80092F74_t (src/BATTLE/BATTLE.PRG/2842C.c:23)
typedef struct Field_RoomEffectObject {
    int* sceneEntry;
    void* scriptCursor;
    short objectId;
    short reserved0A;
    VECTOR position;
    SVECTOR rotation;
    SVECTOR velocity;
    SVECTOR angularVelocity;
} Field_RoomEffectObject;

// decomp: func_80092F74_t2 (src/BATTLE/BATTLE.PRG/2842C.c:34)
typedef struct Field_RoomEffectCommand {
    int* reserved00;
    union {
        u_short unsignedHalves[2];
        short signedHalves[2];
        int word;
    } operands[4];
    int extraOperandOne;
    int extraOperandTwo;
} Field_RoomEffectCommand;

#endif // VS_FIELD_ROOMEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_ROOMEFFECTS_H_DECLS)
#define VS_FIELD_ROOMEFFECTS_H_DECLS

extern int (*Field_kTextureEffectHandlers[37])(Field_SwingEffectRecord*);
extern int (*Field_kRoomEffectOpcodeHandlers[30])(Field_RoomEffectObjectView*, Field_RoomEffectCommandHeader*);
extern short Field_isRoomEffectSoundRestored;
extern Field_RoomEffectInstance Field_roomEffectInstances[100];
extern short Field_roomEffectSoundId;
extern char Field_textureEffectCount;
extern char Field_roomEffectScriptCount;
void* Field_FindRoomGeometryGroup(int arg0);
int Field_ShowRoomGeometryGroup(int arg0);
int Field_HideRoomGeometryGroup(int arg0);
int Field_SwapRoomGeometryGroupFlags(int arg0, int arg1);
int Field_AddRoomGeometryDepthBias(int arg0, int arg1);
int Field_DisableRoomGeometryGroup(int arg0);
void Field_UpdateRoomEffects(void);
int Field_AdvanceRoomTextureFrames(Field_TextureFrameEffect* arg0);
int Field_ScrollRoomTextureCoordinates(Field_TextureScrollEffect* arg0);
int Field_AnimateRoomTextureImages(Field_TextureRegionScrollEffect* arg0);
int Field_UpdateRoomRotationEffect(Field_SwingEffectRecord* arg0);
int Field_SkipRoomEffectCommand(void);
void Field_InitializeRoomEffects(void);
void Field_CreateRoomEffectObjects(Field_RoomEffectInstance* arg0);
void Field_FinishRoomEffectSetup(void);
int Field_StartRoomEffect(int arg0);
int Field_ReleaseRoomEffects(void);
int Field_ExecuteRoomEffectCommands(Field_RoomEffectObjectView* arg0, int arg1, Field_RoomEffectInstance* arg2);
int Field_SetRoomEffectRadialVelocity(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1);
int Field_SetRoomEffectVelocity(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1);
int Field_SetRoomEffectTransform(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1);
void Field_WriteRoomEffectTransform(Field_RoomEffectObject* arg0);
int Field_HideRoomEffectObject(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1 __attribute__((unused)));
int Field_ExecuteRoomEffectShowGroup(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1);
int Field_AdvanceRoomEffectMotion(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1);
int Field_StepRoomEffectGravity(Field_RoomEffectObject* arg0);
int Field_StepRoomEffectDampedMotion(Field_RoomEffectObject* arg0);
int Field_ExecuteRoomEffectPlaySound(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1);
int Field_ExecuteRoomEffectStopSound(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1);
int Field_ExecuteRoomEffectStartEffect(Field_RoomEffectObject* arg0 __attribute__((unused)), Field_RoomEffectCommand* arg1);
int Field_ExecuteRoomEffectHideGroup(Field_RoomEffectObject* unk0 __attribute__((unused)), Field_RoomEffectCommand* arg1);
int Field_ExecuteRoomEffectStopScript(Field_RoomEffectObject* arg0, Field_RoomEffectCommand* arg1);
int Field_ToggleRoomEffectUpdates(int arg0);
int Field_IsRoomEffectActive(int arg0);
void Field_FinishRoomDoorRotation(int arg0);
void Field_FinishRoomTiltRotation(int arg0);
void Field_DuckRoomEffectSound(int arg0);
void Field_SwapRoomGeometryRedGreen(int arg0);
void Field_RestoreRoomEffectSoundVolume(void);
void Field_RestartRoomEffectSound(void);
int Field_CallRoomEntryPoint();

#endif // VS_FIELD_ROOMEFFECTS_H_DECLS
