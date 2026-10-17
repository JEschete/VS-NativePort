// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ROOMLOADING_H_TYPES
#define VS_FIELD_ROOMLOADING_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_FIELD_ROOMLOADING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_ROOMLOADING_H_DECLS)
#define VS_FIELD_ROOMLOADING_H_DECLS
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "DiscQueue.h"
#include "Field/Mpd.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

extern Field_ZoneContext Field_zoneContext;
extern int Field_zoneSoundFileId;
extern int Field_roomMusicSlot;
extern Field_RoomLoadResetBlock Field_roomLoadResetBlock;
extern int Field_isRoomActorSpawnEnabled;
extern void* Field_roomCleanupBuffer;
extern int Field_roomResetWord;
extern Battle_Actor* Field_auxiliaryActor;
extern int Field_screenTransitionAlpha;
extern char Field_reservedResetByte;
void Field_BeginSceneActorTransition(void);
void Field_LoadZoneRoomFileTable(void* arg0, int size);
void Field_LoadZoneEnemyTemplates(long* data, int arg1 __attribute__((unused)));
void Field_QueueZoneLoad(int id);
void Field_FinishZoneLoad(int id);
void Field_ReleaseZoneData(void);
int Field_ZoneSoundFileId(void);
void Field_SpawnRoomActors(void);
void Field_PlaceAshleyAtRoomEntrance(void);
void Field_RestoreRoomEnemyCache(void);
void Field_LoadRoomEnemyPlacements(void* data, u_int dataLen);
void Field_QueueRoomLoad(int id);
void Field_FinishRoomLoad(int arg0);
void Field_RemoveRoomActors(void);
void Field_UnloadCurrentRoom(void);
void Field_UploadZoneTextures(void* data);
int Field_LoadRoomSections(int arg0, void* data);
int Field_ReleaseOwnedRoomSections(int arg0);
void Field_ZoneCleanupHook(int arg0 __attribute__((unused)));
intptr_t Field_RoomAuxiliaryData(void);
intptr_t Field_RoomSupplementalRecord(int arg0);
int Field_IsCurrentRoomWorkshop(void);
int Field_RoomEnvironmentFlags(void);
int Field_CurrentSceneId(void);
int Field_RoomFallDestination(void);
int Field_IsRoomAudioSequenceAbsent(void);
short Field_RoomEnvironmentParameter(void);
int Field_RoomTrapLevel(void);

#endif // VS_FIELD_ROOMLOADING_H_DECLS
