// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_COLLISION_H_TYPES
#define VS_FIELD_COLLISION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Field_CollisionSampleScratch Field_CollisionSampleScratch;
typedef struct Field_GimmickInstance Field_GimmickInstance;
typedef struct Field_SavedPositionWords Field_SavedPositionWords;

// decomp: D_800F4B0A (D_800F4B0A@BATTLE/BATTLE.PRG): a view into another global
#define Field_collisionProbeHeight (*(short*)((u_char*)&Field_collisionProbePosition + 2))

// decomp: D_1F8003BC_t (src/BATTLE/BATTLE.PRG/3A1A0.h:646)
typedef struct Field_CollisionSampleScratch {
    int searchFloor;
    u_char excluded[17];
    u_char pad15[3];
    Field_GimmickInstance* movingActor;
    short sampleHeights[8];
    u_char blocked[8];
    u_char attributes[8];
    u_char centerAttribute;
    u_char pad3D[3];
    int searchCeiling;
} Field_CollisionSampleScratch;

// decomp: func_800A6798_t (src/BATTLE/BATTLE.PRG/3A1A0.c:29)
typedef struct Field_SavedPositionWords {
    int words[2];
} Field_SavedPositionWords;
_Static_assert(sizeof(Field_SavedPositionWords) == 0x8, "Field_SavedPositionWords must keep its PS1 layout");

#endif // VS_FIELD_COLLISION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_COLLISION_H_DECLS)
#define VS_FIELD_COLLISION_H_DECLS
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Zone.h"
#include "Render/Palette.h"

extern u_char Field_kTileSurfaceIds[2744];
extern u_int* Field_collisionProbeTile;
extern u_char Field_collisionProbeDirection;
extern short Field_platformContactSearchDepth;
extern SVECTOR Field_collisionProbePosition;
Field_MpdTileCell* Field_RoomCollisionCells(void);
int Field_CollisionMapDimensions(int arg0 __attribute__((unused)));
Field_ZoneBounds* Field_RoomCameraBounds(int arg0 __attribute__((unused)));
Field_MpdTileCell* Field_CollisionCellAt(u_int width, u_int height, int arg2);
int Field_SetCollisionCellBlocked(int arg0, int arg1, int arg2, int arg3);
short Field_InterpolatedFloorHeight(int arg0, int arg1);
short Field_FloorHeightAt(int arg0, int arg1);
short Field_CeilingHeightAt(int arg0, int arg1);
int Field_FloorAndCeilingHeightsAt(int arg0, int arg1);
void Field_SetActorCollisionBypass(int arg0, int arg1);
int Field_ActorCollisionBypass(int arg0);
int Field_RelativeCollisionSampleHeight(
    Field_CollisionSampleScratch* samples, int sector, int offset, int y);
int Field_ActorSupportHeight(int arg0, int arg1);
void Field_SnapRoomActorsToSupport(void);
int Field_RoomCeilingHeight(int arg0, int arg1);
Actors_ActorRecord* Field_FindActorOccupyingTile(int arg0, int arg1, int arg2, int arg3);
int Field_CheckPlatformActorObstruction(int contact, SVECTOR* point);
int Field_IsHangingActorObstructed(Actors_ActorRecord* actor);
Field_PlatformContactPoint* Field_FindMovingPlatformById(int arg0);
int Field_IsPointOnMovingPlatform(int arg0, SVECTOR* arg1);
int Field_CanActorHangFromLedge(Actors_ActorRecord* actor, SVECTOR* offset, SVECTOR* motion);
int Field_HasActorLedgeSideSupport(Actors_ActorRecord* arg0);
int Field_SampleActorSupportHeight(SVECTOR* arg0, int arg1, int arg2, int arg3);
void Field_SampleActorCollisionPerimeter(Actors_ActorRecord* actor, int diagonal);
void Field_ApplyActorWallRepulsion(Actors_ActorRecord* actor, SVECTOR* motion);
int Field_ResolveActorTerrainMotion(Actors_ActorRecord* actor, SVECTOR* motion, int* result, int probeOnly);
int Field_ClassifyActorTerrainProbe(SVECTOR* arg0, int arg1);
Field_MpdTileCell* Field_FindCollisionCellAtPosition(SVECTOR* arg0, int arg1);
int Field_IsActorPositionUnoccupied(Actors_ActorRecord* actor, SVECTOR* position);
int Field_CanActorSeparateFromOverlaps(Actors_ActorRecord* actor, SVECTOR* motion);
int Field_GimmickSupportHeight(int arg0, int arg1);
int Field_MovingPlatformContactHeight(int arg0, int arg1, int arg2, int arg3);
int Field_CalcActorLedgeOffset(Actors_ActorRecord* actor, SVECTOR* offset);
int Field_GimmickSupportHeightBelow(int arg0, int arg1, int arg2);
int Field_IsPointOutsideSolidVolumes();
int Field_TraceActorAndTerrainCollision(SVECTOR*, SVECTOR*, SVECTOR*, int);

#endif // VS_FIELD_COLLISION_H_DECLS
