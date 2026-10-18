// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/RoomState.h"
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Shadow.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/Movement.h"
#include "Core/Math/Vector.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoading.h"

#include <stdlib.h>
#include <string.h>


// decomp: BATTLE.PRG func_800E6F1C (src/BATTLE/BATTLE.PRG/7E05C.s:541).
void Battle_Ai_FreeRoomAiState(void)
{
    for (unsigned index = 0; index < 16; ++index) {
        if (Battle_actors[index] != NULL) Battle_Ai_FreeActorAiState(Battle_actors[index]);
    }
    if (Battle_Ai_roomAiState != NULL) Core_Memory_FreeFromLowEnd(Battle_Ai_roomAiState);
    Battle_Ai_roomAiState = NULL;
    Battle_Ai_tileFlagMap = NULL;
    Battle_Ai_specialTerrainHeights = NULL;
    Battle_Ai_roomHasWaypoints = 0;
    Battle_Ai_patrolRoutes = NULL;
    Battle_Ai_roomWaypoints = NULL;
    Battle_Ai_routeGates = NULL;
}

u_short Battle_Ai_DecodeCollisionFlags(unsigned bits)
{
    return (u_short)(((bits >> 23) & 15) | ((bits >> 23) & 0x10)
        | ((bits >> 12) & 0x20) | ((bits >> 22) & 0x40)
        | ((bits >> 20) & 0x800) | ((bits >> 18) & 0x1000) | ((bits >> 16) & 0x2000));
}

int Battle_Ai_BuildRoomAiMap(Battle_Ai_RoomAiMap* map)
{
    const unsigned char* auxiliary = Field_roomData.section6;
    const Field_MpdCollisionHeader* dimensions = Field_roomData.collisionMap;
    if (map == NULL || (auxiliary != NULL && Field_roomData.header.section6Length < 0x498)
        || (dimensions != NULL && (dimensions->width < 0 || dimensions->width > 32
            || dimensions->height < 0 || dimensions->height > 32))) return 0;
    memset(map, 0, sizeof *map);
    const unsigned short bounds = (unsigned short)((Field_CollisionMapDimensions(0) & 0xFFFF) >> 4
        | (unsigned)Field_CollisionMapDimensions(0) >> 12);
    memcpy(&map->state.maxTileX, &bounds, sizeof bounds);
    memcpy(map->state.reserved4, &bounds, sizeof bounds);
    map->state.flightCeiling = Field_roomData.environment != NULL ? -Field_RoomEnvironmentParameter() * 64 : 0;
    map->state.defaultActionDelay = 150;
    for (unsigned tile = 0; tile < 1024; ++tile) {
        const unsigned tileX = tile & 31, tileZ = tile >> 5;
        const Field_MpdTileCell* cell = Field_CollisionCellAt(tileX, tileZ, 0);
        if (cell == NULL) {
            map->flags[tileZ][tileX] = 0x8000;
            continue;
        }
        unsigned bits;
        memcpy(&bits, cell, sizeof bits);
        unsigned flags = Battle_Ai_DecodeCollisionFlags(bits);
        map->heights[tileZ][tileX] = (u_short)((Battle_Ai_SampleAiTileGroundHeight(tileX, tileZ) >> 6) & 0x3FF);
        const Field_MpdFloorTrigger* trigger = Field_RoomTriggerAtTile(tileX, tileZ, 0);
        if (trigger != NULL && (unsigned short)trigger->kind < 16
            && (unsigned short)trigger->lockState.switchValue < 2) {
            flags |= 0x100u << trigger->lockState.switchValue;
        }
        map->flags[tileZ][tileX] = (u_short)flags;
    }
    if (auxiliary != NULL) {
        unsigned settings;
        memcpy(&settings, auxiliary + 0x450, sizeof settings);
        memcpy(&map->state.maxTileX, auxiliary + 0x494, 4);
        map->state.defaultActionDelay = (unsigned char)(settings >> 21);
        map->state.allowsNonSpellActions = (unsigned char)((settings >> 20) & 1);
        map->state.reservedB[2] = (char)(settings >> 29);
    }
    return 1;
}

// decomp: BATTLE.PRG func_800E6F9C (src/BATTLE/BATTLE.PRG/7E05C.s:582).
void Battle_Ai_InitializeRoomAiMap(void)
{
    Battle_Ai_RoomAiMap* map = Core_Memory_AllocateFromLowEnd(sizeof *map);
    if (map == NULL || !Battle_Ai_BuildRoomAiMap(map)) {
        Core_Debugf("ai", "Invalid room AI data or allocation failure");
        abort();
    }
    Battle_Ai_roomAiState = &map->state;
    Battle_Ai_tileFlagMap = map->flags;
    Battle_Ai_specialTerrainHeights = map->heights;
    unsigned char* auxiliary = Field_roomData.section6;
    Battle_Ai_roomHasWaypoints = auxiliary;
    Battle_Ai_patrolRoutes = auxiliary != NULL ? (unsigned short (*)[8])(auxiliary + 0x400) : NULL;
    Battle_Ai_roomWaypoints = auxiliary != NULL ? (unsigned short*)(auxiliary + 0x440) : NULL;
    Battle_Ai_routeGates = auxiliary != NULL ? (Battle_Ai_RouteGateEnd*)(auxiliary + 0x454) : NULL;
    Battle_Ai_BuildAiTerrainHeightsWithWorkspace();
    Battle_Ai_UpdateRouteGateAvailability();
}

// decomp: BATTLE.PRG func_800E71DC (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
int Battle_Ai_SelectTileHeight(const int samples[16], int center)
{
    int highest = center;
    unsigned invalid = 0;
    for (unsigned index = 0; index < 16; ++index) {
        const int sample = samples[index];
        if (sample < 0) invalid |= 1u << index;
        const int height = (sample & 0x3FFF) - (sample & 0x4000);
        const int previous = (highest & 0x3FFF) - (highest & 0x4000);
        if (height > previous) highest = sample;
    }
    if (invalid == 0) return highest;
    for (unsigned index = 0; index < 10; ++index) {
        if (index == 3 || index == 7) continue;
        const unsigned patch = 0x33u << index;
        if ((~invalid & patch) == patch) return highest;
    }
    return samples[10];
}

// decomp: BATTLE.PRG func_800E71DC (src/BATTLE/BATTLE.PRG/7E05C.s:738).
int Battle_Ai_SampleAiTileGroundHeight(int tileX, int tileZ)
{
    const int originX = tileX * 128, originZ = tileZ * 128;
    const int center = Field_FloorHeightAt(originX + 64, originZ + 64);
    int samples[16];
    for (unsigned index = 0; index < 16; ++index) {
        samples[index] = Field_FloorHeightAt(originX + (index >> 2) * 32, originZ + (index & 3) * 32);
    }
    return Battle_Ai_SelectTileHeight(samples, center);
}
