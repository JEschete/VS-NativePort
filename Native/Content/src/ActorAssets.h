// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORASSETS_H_TYPES
#define VS_ACTORASSETS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_ActorAssetRequest Content_ActorAssetRequest;
typedef struct Content_SequenceBuffer Content_SequenceBuffer;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_ACTORASSETS_H_SET_TYPES_ONLY
#endif
#include "Field/Actors.h"
#ifdef VS_ACTORASSETS_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_ACTORASSETS_H_SET_TYPES_ONLY
#endif

// decomp: func_8009C378_t (src/BATTLE/BATTLE.PRG/30DB0.h:9)
typedef struct Content_SequenceBuffer {
    u_int firstSectionPointer; /* offset from the struct (on-disc pointer) */
    u_int secondSectionPointer; /* offset from the struct (on-disc pointer) */
    char body[0x78F8];
} Content_SequenceBuffer;
_Static_assert(sizeof(Content_SequenceBuffer) == 0x7900, "Content_SequenceBuffer must keep its PS1 layout");

// decomp: vs_battle_objectData (src/BATTLE/BATTLE.PRG/2842C.h:36)
typedef struct Content_ActorAssetRequest {
    u_char assetKind;
    signed char targetSlot;
    short modelId;
    void* destination;
    uintptr_t dataAddress;
    Field_ActorPlacement placement;
    u_char subKind;
    char textureSlot;
    char material;
    char variant;
} Content_ActorAssetRequest;

#endif // VS_ACTORASSETS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_ACTORASSETS_H_DECLS)
#define VS_ACTORASSETS_H_DECLS
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Render/Palette.h"

extern u_char Content_modelParseResetFlag;
extern int Content_assetQueuePeakUsage;
extern int Content_lastLoadPhaseSnapshot;
extern int Content_assetQueueFrameTicks;
extern int Content_lastAssetRequestType;
int Content_FindFreeActorAssetRequest(void);
int Content_QueueActorAssetRequest(Content_ActorAssetRequest* objectData);
int Content_UpdateActorAssetQueue(void);
void Content_QueueAshleyWeaponSequence(int arg0);
void Content_OffsetModelTextureCoordinates(void* arg0, int arg1, int arg2, int arg3, int arg4);
int Content_FindSharedWeaponModel(Content_ActorAssetRequest* objData);
void* Content_CopyRelocatedModelGeometry(int* arg0, void* arg1, int arg2);
int Content_UpdateWeaponModelLoad(Content_ActorAssetRequest* arg0);
int Content_ParseWeaponModel(Content_ActorAssetRequest* objectData);
void Content_LoadWeaponMaterialPalette(int index, int material);
int Content_ReleaseWeaponModel(int index);
int Content_FindSharedActorModel(Content_ActorAssetRequest* arg0);
int Content_UpdateActorModelLoad(Content_ActorAssetRequest* arg0);
int Content_ParseActorModel(Content_ActorAssetRequest* object);
int Content_FindSharedActorSequence(Content_ActorAssetRequest* arg0);
int Content_UpdateActorSequenceLoad(Content_ActorAssetRequest* object);
void Content_CopyRelocatedWeaponSequence(Content_SequenceBuffer* arg0, Content_SequenceBuffer* arg1);
int Content_ParseActorSequence(Content_ActorAssetRequest* object);
void Content_BindSharedActorSequence(Actors_ActorRecord* arg0, int arg1);
int Content_ReleaseActorSequence(int arg0, int arg1);
int Content_ReleaseActorModel(int arg0);
int Content_ReleaseSequenceEffectTexture(int arg0);
int Content_UpdateSequenceEffectTextureLoad(Content_ActorAssetRequest* objdata);

#endif // VS_ACTORASSETS_H_DECLS
