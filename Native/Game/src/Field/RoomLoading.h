// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_FIELD_ROOMLOADING_H_TYPES
#define VS_FIELD_ROOMLOADING_H_TYPES
#include "Legacy/Psx.h"


// decomp: D_800F18A8 (D_800F18A8@BATTLE/BATTLE.PRG): a view into another global
#define Field_zoneSoundFileId (*(int*)(&Field_zoneContext.musicId))

// decomp: D_800F18AC (D_800F18AC@BATTLE/BATTLE.PRG): a view into another global
#define Field_roomMusicSlot (*(int*)(&Field_zoneContext.musicSlot))

#endif // VS_FIELD_ROOMLOADING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_FIELD_ROOMLOADING_H_DECLS)
#define VS_FIELD_ROOMLOADING_H_DECLS
#include "Actors/ActorModelSlot.h"
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
extern Field_RoomLoadResetBlock Field_roomLoadResetBlock;
extern int Field_isRoomActorSpawnEnabled;
extern void* Field_roomCleanupBuffer;
extern int Field_roomResetWord;
extern Battle_Actor* Field_auxiliaryActor;
extern int Field_screenTransitionAlpha;
extern char Field_reservedResetByte;
void Field_BeginSceneActorTransition(void);
void Field_LoadZoneRoomFileTable(void* room_file_table_data, int size);
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
void Field_FinishRoomLoad(int requested_room_id);
void Field_RemoveRoomActors(void);
void Field_UnloadCurrentRoom(void);
void Field_UploadZoneTextures(void* data);
int Field_LoadRoomSections(int owner_room_index, void* data);
int Field_ReleaseOwnedRoomSections(int owner_room_index);
void Field_ZoneCleanupHook(int arg0 __attribute__((unused)));
intptr_t Field_RoomAuxiliaryData(void);
intptr_t Field_RoomSupplementalRecord(int supplemental_record_index);
int Field_IsCurrentRoomWorkshop(void);
int Field_RoomEnvironmentFlags(void);
int Field_CurrentSceneId(void);
int Field_RoomFallDestination(void);
int Field_IsRoomAudioSequenceAbsent(void);
short Field_RoomEnvironmentParameter(void);
int Field_RoomTrapLevel(void);

#endif // VS_FIELD_ROOMLOADING_H_DECLS
