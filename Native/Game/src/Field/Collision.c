// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Field/Collision.h"
#include "Field/CollisionSampling.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Render/Palette.h"

// decomp: D_800E9278 (D_800E9278@BATTLE/BATTLE.PRG)
u_char Field_kTileSurfaceIds[2744]; /* contents: module image (ModuleData.c) */

// decomp: D_800F49F0 (D_800F49F0@BATTLE/BATTLE.PRG)
u_int* Field_collisionProbeTile;

// decomp: D_800F49F9 (D_800F49F9@BATTLE/BATTLE.PRG)
u_char Field_collisionProbeDirection;

// decomp: D_800F4B00 (D_800F4B00@BATTLE/BATTLE.PRG)
short Field_platformContactSearchDepth;

// decomp: D_800F4B08 (D_800F4B08@BATTLE/BATTLE.PRG)
SVECTOR Field_collisionProbePosition;

// decomp: BATTLE.PRG func_8008B6EC (src/BATTLE/BATTLE.PRG/146C.c:16059)
Field_MpdTileCell* Field_RoomCollisionCells(void) { return Field_roomData.tileCells; }

// decomp: BATTLE.PRG _getCollisionMapDimensions (src/BATTLE/BATTLE.PRG/146C.c:16063)
int Field_CollisionMapDimensions(int arg0 __attribute__((unused)))
{
    if (Field_roomData.collisionMap != NULL) {
        return (Field_roomData.collisionMap->height << 0x14)
             | ((u_short)(Field_roomData.collisionMap->width * 16));
    }
    return 0;
}

// decomp: BATTLE.PRG _getMapBounds (src/BATTLE/BATTLE.PRG/146C.c:16072)
Field_ZoneBounds* Field_RoomCameraBounds(int arg0 __attribute__((unused)))
{
    if (Field_roomData.zoneBounds != NULL) {
        return Field_roomData.zoneBounds;
    }
    return NULL;
}

// decomp: BATTLE.PRG func_8008B764 (src/BATTLE/BATTLE.PRG/146C.c:16080)
Field_MpdTileCell* Field_CollisionCellAt(u_int width, u_int height, int collision_layer)
{
    if (Field_roomData.tileCells != NULL) {
        if (collision_layer >= 2) {
            if (Field_roomData.movingPlatforms != NULL) {
                return &Field_roomData.movingPlatforms[collision_layer - 2].tileCell;
            }
            return NULL;
        }
        if ((width < Field_roomData.collisionMap->width)
            && (height < Field_roomData.collisionMap->height)) {
            return &Field_roomData
                        .tileCells[height * Field_roomData.collisionMap->width
                                  + width];
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_8008B808 (src/BATTLE/BATTLE.PRG/146C.c:16099)
int Field_SetCollisionCellBlocked(int tile_x, int tile_z, int collision_layer, int blocked_passability)
{
    Field_MpdTileCell* collision_cell;

    if (Field_roomData.tileCells != NULL) {
        if (blocked_passability != 0) {
            blocked_passability = 5;
        }
        if (collision_layer >= 2) {
            if (Field_roomData.movingPlatforms != NULL) {
                collision_cell = &Field_roomData.movingPlatforms[collision_layer - 2].tileCell;
                collision_cell->doorPassability = blocked_passability;
                return 1;
            }
        } else {
            collision_cell =
                &Field_roomData
                     .tileCells[(collision_layer * Field_roomData.collisionMap->width
                                   * Field_roomData.collisionMap->height)
                               + (tile_z * Field_roomData.collisionMap->width)
                               + tile_x];
            collision_cell->doorPassability = blocked_passability;
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008DA24 (src/BATTLE/BATTLE.PRG/146C.c:17000)
short Field_InterpolatedFloorHeight(int world_x_or_sample_x, int world_z_or_sample_z)
{
    Field_MpdCollisionHeader* collisionMap;
    int sample_offset_z;
    int packed_absolute_sample_offsets;
    int neighbor_z_weight;
    int sample_offset_x;
    int neighbor_x_weight;
    int center_weight_or_weighted_height;
    int centered_sample_offset_x;
    int neighbor_sample_z_or_height;
    int neighbor_sample_x_or_height;
    int base_or_interpolated_height;
    int neighbor_height_delta;
    int centered_sample_offset_z;
    int v;
    int height_code_or_packed_height = 0x8000;

    if (Field_roomData.collisionMap != NULL) {
        sample_offset_x = world_x_or_sample_x;
        sample_offset_x = sample_offset_x & 0x1F;
        world_x_or_sample_x >>= 5;
        sample_offset_z = world_z_or_sample_z & 0x1F;
        world_z_or_sample_z >>= 5;
        collisionMap = Field_roomData.collisionMap;

        if (((world_x_or_sample_x >> 2) >= (u_int)collisionMap->width)
            || ((world_z_or_sample_z >> 2) >= (u_int)collisionMap->height)) {
            return -0x8000;
        }

        height_code_or_packed_height = Field_SampleCollisionHeightCode(world_x_or_sample_x, world_z_or_sample_z, 0);
        base_or_interpolated_height = (collisionMap->heightOrigin - ((height_code_or_packed_height & 0x7F) * 2)) * 8;

        neighbor_sample_x_or_height = sample_offset_x < 0x10 ? world_x_or_sample_x - 1 : world_x_or_sample_x + 1;
        neighbor_sample_z_or_height = world_z_or_sample_z - 1;

        if (sample_offset_z >= 0x10) {
            neighbor_sample_z_or_height = world_z_or_sample_z + 1;
        }

        if ((neighbor_sample_x_or_height >> 2) < (u_int)collisionMap->width) {
            neighbor_sample_x_or_height =
                (collisionMap->heightOrigin - ((Field_SampleCollisionHeightCode(neighbor_sample_x_or_height, world_z_or_sample_z, 0) & 0x7F) * 2)) * 8;
            neighbor_height_delta = neighbor_sample_x_or_height - base_or_interpolated_height;
            if (ABS(neighbor_height_delta) >= 0x40) {
                neighbor_sample_x_or_height = base_or_interpolated_height;
            }
        } else {
            neighbor_sample_x_or_height = base_or_interpolated_height;
        }

        if ((neighbor_sample_z_or_height >> 2) < (u_int)collisionMap->height) {
            neighbor_sample_z_or_height =
                (collisionMap->heightOrigin - ((Field_SampleCollisionHeightCode(world_x_or_sample_x, neighbor_sample_z_or_height, 0) & 0x7F) * 2)) * 8;
            neighbor_height_delta = neighbor_sample_z_or_height - base_or_interpolated_height;
            if (ABS(neighbor_height_delta) >= 0x40) {
                neighbor_sample_z_or_height = base_or_interpolated_height;
            }
        } else {
            neighbor_sample_z_or_height = base_or_interpolated_height;
        }

        centered_sample_offset_x = sample_offset_x - 0x10;
        v = ABS(centered_sample_offset_x);
        centered_sample_offset_z = sample_offset_z - 0x10;

        packed_absolute_sample_offsets = v | (ABS(centered_sample_offset_z) << 0x10);
        neighbor_z_weight = Core_Gte_NormalClip(0, 0x20, packed_absolute_sample_offsets);
        neighbor_x_weight = Core_Gte_NormalClip(0, packed_absolute_sample_offsets, 0x200000);

        center_weight_or_weighted_height = 0x400 - neighbor_x_weight;
        center_weight_or_weighted_height -= neighbor_z_weight;

        if (center_weight_or_weighted_height >= 0) {
            center_weight_or_weighted_height = (neighbor_sample_z_or_height * neighbor_z_weight) + (neighbor_sample_x_or_height * neighbor_x_weight) + (base_or_interpolated_height * center_weight_or_weighted_height);
            base_or_interpolated_height = center_weight_or_weighted_height / 1024;
        }
        height_code_or_packed_height = (base_or_interpolated_height & 0x7FFF) | ((height_code_or_packed_height & 0x80) << 8);
    }
    return height_code_or_packed_height;
}

// decomp: BATTLE.PRG func_8008DC7C (src/BATTLE/BATTLE.PRG/146C.c:17083)
short Field_FloorHeightAt(int world_x, int world_z)
{
    Field_MpdCollisionHeader* collisionMap = Field_roomData.collisionMap;
    int height_code_or_packed_floor = 0x8000;

    if ((collisionMap != NULL) && ((u_int)(world_x >> 7) < collisionMap->width)
        && ((u_int)(world_z >> 7) < collisionMap->height)) {
        height_code_or_packed_floor = Field_SampleCollisionHeightCode(world_x >> 5, world_z >> 5, 0);
        height_code_or_packed_floor = (((collisionMap->heightOrigin - ((height_code_or_packed_floor & 0x7F) * 2)) * 8) & 0x7FFF)
               | ((height_code_or_packed_floor & 0x80) << 8);
    }
    return height_code_or_packed_floor;
}

// decomp: BATTLE.PRG func_8008DD0C (src/BATTLE/BATTLE.PRG/146C.c:17097)
short Field_CeilingHeightAt(int world_x, int world_z)
{
    int decoded_ceiling_height;
    Field_MpdCollisionHeader* collisionMap = Field_roomData.collisionMap;
    int height_code_or_packed_ceiling = 0;
    int new_var = 8;

    if ((Field_roomData.collisionMap != NULL)
        && ((world_x >> 7) < (u_int)collisionMap->width)
        && ((world_z >> 7) < (u_int)collisionMap->height)) {
        height_code_or_packed_ceiling = Field_SampleCollisionHeightCode(world_x >> 5, world_z >> 5, 1);
        decoded_ceiling_height = (collisionMap->heightOrigin - ((height_code_or_packed_ceiling & 0x7F) * 2)) * new_var;
        if (decoded_ceiling_height > 0) {
            height_code_or_packed_ceiling = 0xF810;
        } else {
            height_code_or_packed_ceiling = (decoded_ceiling_height & 0x7FFF) | ((height_code_or_packed_ceiling & 0x80) << new_var);
        }
    }
    return height_code_or_packed_ceiling;
}

// decomp: BATTLE.PRG func_8008DDA8 (src/BATTLE/BATTLE.PRG/146C.c:17118)
int Field_FloorAndCeilingHeightsAt(int world_x, int world_z)
{
    int packed_floor_and_ceiling = 0x80008000;
    short* height_halfwords = (short*)&packed_floor_and_ceiling;

    Field_MpdCollisionHeader* collisionMap = Field_roomData.collisionMap;

    if ((collisionMap != NULL) && ((u_int)(world_x >> 7) < collisionMap->width)) {
        if ((u_int)(world_z >> 7) < collisionMap->height) {
            int ceiling_height_work;
            int height_code_or_packed_plane = Field_SampleCollisionHeightCode(world_x >> 5, world_z >> 5, 0);
            height_code_or_packed_plane = (((collisionMap->heightOrigin - ((height_code_or_packed_plane & 0x7F) * 2)) * 8) & 0x7FFF)
                    | ((height_code_or_packed_plane & 0x80) << 8);
            height_halfwords[0] = height_code_or_packed_plane;
            height_code_or_packed_plane = Field_SampleCollisionHeightCode(world_x >> 5, world_z >> 5, 1);
            ceiling_height_work = collisionMap->heightOrigin - (height_code_or_packed_plane & 0x7F) * 2;
            ceiling_height_work *= 8;
            if (ceiling_height_work > 0) {
                height_code_or_packed_plane = 0xF810;
            } else {
                ceiling_height_work &= 0x7FFF;
                height_code_or_packed_plane = ceiling_height_work | ((height_code_or_packed_plane & 0x80) << 8);
            }
            height_halfwords[1] = height_code_or_packed_plane;
        }
    }
    return packed_floor_and_ceiling;
}

// decomp: BATTLE.PRG func_800A0A1C (src/BATTLE/BATTLE.PRG/30DB0.c:3994)
void Field_SetActorCollisionBypass(int actor_index, int collision_bypass_flags)
{
    Actors_ActorRecord* actor_record = Actors_actorRecords[actor_index];
    if (Actors_actorRecords[actor_index] != NULL) {
        actor_record->bypassActorCollision = collision_bypass_flags;
        actor_record->ignoredByCollision = collision_bypass_flags >> 1;
    }
}

// decomp: BATTLE.PRG func_800A0A74 (src/BATTLE/BATTLE.PRG/30DB0.c:4003)
int Field_ActorCollisionBypass(int actor_index)
{
    Actors_ActorRecord* actor_record = Actors_actorRecords[actor_index];
    if (actor_record != NULL) {
        int collision_bypass_flags = actor_record->bypassActorCollision;
        if ((actor_record->ignoredByCollision << 11)) {
            collision_bypass_flags |= 2;
        }
        return collision_bypass_flags;
    }
}

// decomp: BATTLE.PRG vs_battleRelativeSampleHeight (src/BATTLE/BATTLE.PRG/3A1A0.c:80)
int Field_RelativeCollisionSampleHeight(
    Field_CollisionSampleScratch* samples, int sector, int offset, int y)
{
    return samples->sampleHeights[(sector + offset) & 7] - y;
}

// decomp: BATTLE.PRG func_800A3500 (src/BATTLE/BATTLE.PRG/3A1A0.c:394)
int Field_ActorSupportHeight(int world_x, int world_z)
{
    int h;
    int i;
    Field_GimmickInstance* actor;

    h = Field_InterpolatedFloorHeight(world_x, world_z);
    h <<= 17;
    h >>= 17;

    for (i = 0; i < 16; ++i) {
        actor = Field_gimmickInstances[i];
        if (actor != NULL && actor->positionX == world_x && actor->positionZ == world_z
            && actor->positionY - 0x80 < h) {
            h = actor->positionY - 0x80;
        }
    }
    return h;
}

// decomp: BATTLE.PRG func_800A35A8 (src/BATTLE/BATTLE.PRG/3A1A0.c:414)
void Field_SnapRoomActorsToSupport(void)
{
    int i;
    int j;
    int h;
    int x;
    int z;
    Actors_ActorRecord* actor;
    Field_GimmickInstance* obj;

    for (i = 2; i < 17; ++i) {
        actor = Actors_actorRecords[i];

        if (actor != NULL && !actor->instance.motionLocked && actor->instance.currentTileLayer < 2
            && !actor->instance.movementMode) {
            x = actor->instance.position.vx;
            z = actor->instance.position.vz;
            h = Field_InterpolatedFloorHeight(x, z);
            h <<= 17;
            h >>= 17;

            for (j = 0; j < 16; ++j) {
                obj = Field_gimmickInstances[j];
                if (obj != NULL && obj->positionX == x && obj->positionZ == z
                    && obj->positionY - 0x80 < h) {
                    h = obj->positionY - 0x80;
                }
            }

            actor->instance.position.vy = h;
        }
    }
}

// decomp: BATTLE.PRG func_800A3BC4 (src/BATTLE/BATTLE.PRG/3A1A0.c:652)
int Field_RoomCeilingHeight(int world_x, int world_z)
{
    short packed_ceiling_height = Field_CeilingHeightAt(world_x, world_z);
    if (packed_ceiling_height == 0) {
        return 0;
    }
    return (packed_ceiling_height << 0x11) >> 0x11;
}

// decomp: BATTLE.PRG func_800A3C34 (src/BATTLE/BATTLE.PRG/3A1A0.c:666)
Actors_ActorRecord* Field_FindActorOccupyingTile(int query_tile_x, int query_tile_z, int minimum_actor_height, int occupancy_mode_or_first_actor)
{
    int i;

    minimum_actor_height -= 192;

    if (occupancy_mode_or_first_actor < 2) {
        i = 0;
    } else {
        i = occupancy_mode_or_first_actor;
    }

    for (; i < 17; ++i) {
        Actors_ActorRecord* actor = Actors_actorRecords[i];
        if ((actor == NULL) || actor->instance.isSkipped || (actor->modelId == 0x7F)) {
            continue;
        }
        if ((occupancy_mode_or_first_actor == 1)
            && (((actor->instance.poseState != 0) && (actor->instance.poseState < 3))
                || actor->instance.movementMode || actor->instance.reactionState)) {
            SVECTOR* pos = &actor->routeTarget;
            if (((pos->vx & 0xFF) == query_tile_x) && (query_tile_z == (pos->vy & 0xFF))) {
                return actor;
            }
            continue;
        }
        if ((i == 0) && (*((int*)&actor->sequenceFlags - 2) & 0xF0000000)) {
            Field_GimmickInstance* support_gimmick_record = Field_gimmickInstances[actor->supportGimmick];
            if ((query_tile_x == support_gimmick_record->tileX) && (query_tile_z == support_gimmick_record->tileZ)) {
                return actor;
            }
            continue;
        }
        if ((actor->instance.locomotionState != 2) && (actor->instance.currentTileX == query_tile_x)
            && (actor->instance.currentTileZ == query_tile_z) && !actor->instance.motionLocked
            && (actor->instance.position.vy >= minimum_actor_height)) {
            return actor;
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_800A4494 (src/BATTLE/BATTLE.PRG/3A1A0.c:931)
int Field_CheckPlatformActorObstruction(int contact, SVECTOR* point)
{
    int i;
    Actors_ActorRecord* actor;
    int dx, dz, hit;
    int radius, x, z, y;
    for (i = 0; i < 17; i++) {
        actor = Actors_actorRecords[i];
        if (actor == 0 || actor->instance.isSkipped) {
            continue;
        }
        if (actor->instance.supportLayer == contact) {
            if (actor->instance.locomotionState != 2) {
                continue;
            }
            dx = point->vx - actor->platformAnchor.vx;
            dz = point->vz - actor->platformAnchor.vz;
            actor->instance.position.vx += dx;
            actor->instance.position.vz += dz;
            hit = Field_IsHangingActorObstructed(actor);
            actor->instance.position.vx -= dx;
            actor->instance.position.vz -= dz;
            if (!hit) {
                continue;
            }
        transition:
            actor->instance.rootMotion.vx = 0;
            actor->instance.rootMotion.vy = 3;
            actor->instance.rootMotion.vz = 0;
            actor->platformOffsetX = 0;
            actor->platformOffsetZ = 0;
            actor->instance.movementMode = 3;
            actor->instance.locomotionState = 0;
            actor->instance.movementPending = 0;
            actor->instance.supportLayer = 0;
            Actors_UpdateActorPlatformContact(actor, 2);
            return 1;
        } else {
            radius = actor->collisionRadius;
            x = point->vx;
            if (x + 64 < actor->instance.position.vx - radius
                || x - 64 > actor->instance.position.vx + radius) {
                continue;
            }
            z = point->vz;
            if (z + 64 < actor->instance.position.vz - radius
                || z - 64 > actor->instance.position.vz + radius) {
                continue;
            }
            y = actor->instance.position.vy;
            if (point->vy < y - 288 || point->vy >= y) {
                continue;
            }
            if (actor->instance.locomotionState == 2) {
                goto transition;
            }
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A46A4 (src/BATTLE/BATTLE.PRG/3A1A0.c:993)
int Field_IsHangingActorObstructed(Actors_ActorRecord* actor)
{
    Field_CollisionSampleScratch* scratch = (Field_CollisionSampleScratch*)((uintptr_t)Core_ScratchBase() + 0x3BC);
    int saved = actor->instance.supportLayer;
    int radius = actor->collisionRadius;
    int direction;

    actor->instance.supportLayer = 0;
    actor->instance.locomotionState = 0;
    actor->instance.movementPending = 0;
    Field_SampleActorCollisionPerimeter(actor, Core_Gte_Sin(ONE / 8) * radius / ONE);
    actor->instance.supportLayer = saved;
    actor->instance.locomotionState = 2;
    direction = actor->moveFacing >> 9;
    scratch->blocked[direction] = 0;
    scratch->blocked[direction + 1] = 0;
    --direction;
    direction &= 7;
    scratch->blocked[direction] = 0;

    for (direction = 0; direction < 8; ++direction) {
        if (scratch->blocked[direction]) {
            return 1;
        }
    }

    return 0;
}

// decomp: BATTLE.PRG func_800A4A24 (src/BATTLE/BATTLE.PRG/3A1A0.c:1089)
Field_PlatformContactPoint* Field_FindMovingPlatformById(int platform_layer_or_index)
{
    Field_PlatformContactPoint platform_contacts[4];
    int platform_contact_count;
    int i;
    platform_layer_or_index -= 2;

    platform_contact_count = Field_CollectMovingPlatformPositions(platform_contacts);

    for (i = 0; i < platform_contact_count; ++i) {
        if (platform_contacts[i].platformIndex == platform_layer_or_index) {
            // BUG: returns stack variable
            return &platform_contacts[i];
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_800A51A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:1307)
int Field_IsPointOnMovingPlatform(int platform_layer_or_index, SVECTOR* world_position)
{
    Field_PlatformContactPoint platform_contacts[4];
    Field_PlatformContactPoint* p;
    int n;
    int i;

    platform_layer_or_index -= 2;
    n = Field_CollectMovingPlatformPositions(platform_contacts);
    for (i = 0; i < n; i++) {
        if (platform_contacts[i].platformIndex == platform_layer_or_index) {
            p = &platform_contacts[i];
            goto found;
        }
    }
    p = NULL;
found:
    if (p->x + 0x40 < world_position->vx) {
        return 0;
    }
    if (world_position->vx < p->x - 0x40) {
        return 0;
    }
    if (p->z + 0x40 < world_position->vz) {
        return 0;
    }
    return !(world_position->vz < p->z - 0x40);
}

// decomp: BATTLE.PRG func_800A6798 (src/BATTLE/BATTLE.PRG/3A1A0.c:1960)
int Field_CanActorHangFromLedge(Actors_ActorRecord* actor, SVECTOR* offset, SVECTOR* motion)
{
    Field_SavedPositionWords saved;
    int height;
    int valid = 0;

    if (actor->ledgeHeight <= Field_collisionProbePosition.vy) {
        return 0;
    }

    offset->vx = 0;
    offset->vz = 0;
    offset->vy += 236;
    saved = *(Field_SavedPositionWords*)&actor->instance.position;
    addVector(&actor->instance.position, offset);
    actor->instance.locomotionState = 2;

    if (Field_ResolveActorTerrainMotion(actor, motion, &height, 0) != 3 && !actor->onLedge
        && height < 5) {
        height = Core_Gte_Sin(ONE / 8);
        height = height * actor->collisionRadius / ONE;

        if ((Field_collisionProbeDirection >> 1) & 1) {
            offset->vz = height;
        } else {
            offset->vx = height;
        }

        height = Field_SampleActorSupportHeight(&Field_collisionProbePosition, offset->vx, offset->vz, 1);
        height -= Field_collisionProbePosition.vy;

        if (height > -64) {
            height = Field_SampleActorSupportHeight(&Field_collisionProbePosition, -offset->vx, -offset->vz, 1);
            height -= Field_collisionProbePosition.vy;
        }

        if (height > -64) {
            valid = 1;
        }
    }

    actor->instance.locomotionState = 0;
    actor->instance.movementPending = 0;
    offset->vx = 0;
    offset->vz = 0;
    *(Field_SavedPositionWords*)&actor->instance.position = saved;
    return valid;
}

// decomp: BATTLE.PRG func_800A69B4 (src/BATTLE/BATTLE.PRG/3A1A0.c:2009)
int Field_HasActorLedgeSideSupport(Actors_ActorRecord* actor_record)
{
    SVECTOR v;
    int ret = 0;
    int h = Core_Gte_Sin(ONE / 8) * actor_record->collisionRadius / ONE;

    v.vx = 0;
    v.vz = 0;
    if ((Field_collisionProbeDirection >> 1) & 1) {
        v.vz = h;
    } else {
        v.vx = h;
    }
    h = Field_SampleActorSupportHeight(&Field_collisionProbePosition, v.vx, v.vz, 1) - Field_collisionProbePosition.vy;
    if (h > -64) {
        h = Field_SampleActorSupportHeight(&Field_collisionProbePosition, -v.vx, -v.vz, 1) - Field_collisionProbePosition.vy;
        if (h > -64) {
            ret = 1;
        }
    }
    return ret;
}

// decomp: BATTLE.PRG func_800A6EE8 (src/BATTLE/BATTLE.PRG/3A1A0.c:2175)
int Field_SampleActorSupportHeight(SVECTOR* origin_position, int offset_x, int offset_z, int support_probe_mode)
{
    SVECTOR sp;
    u_int* ent;
    int h;
    int r;
    int los;

    int x = offset_x + origin_position->vx;
    int z = offset_z + origin_position->vz;

    sp.vx = x;
    sp.vz = z;

    ent = ((u_int* (*)(SVECTOR*, int))Field_FindCollisionCellAtPosition)(&sp, 0);

    if (ent != NULL) {

        Field_collisionProbeTile = ent;

        if (support_probe_mode == 0) {
            h = Field_FloorHeightAt(x, z);
        } else {
            h = Field_InterpolatedFloorHeight(x, z);
        }

        if (h >= 0) {

            h <<= 17;
            h >>= 17;

            if (support_probe_mode == 2) {
                r = Field_GimmickSupportHeightBelow(x / 128, z / 128, origin_position->vy);
            } else {
                r = Field_GimmickSupportHeight(x / 128, z / 128);
            }

            if (r != 0) {
                h = r;
            }

            if (((*ent >> 17) & 1) && (h >= 0)) {

                h = 3000;

                if (support_probe_mode == 3) {

                    los = Field_kTileSurfaceIds[*ent & 0x1F];

                    if (los == 0x14) {
                        h = 0;
                    }
                }
            }

            if (support_probe_mode != 2 && support_probe_mode != 4) {

                los = Field_MovingPlatformContactHeight(x, origin_position->vy, z, 0);

                r = h;

                if ((los != 0) && (los < h)) {
                    h = los;
                }

                if (support_probe_mode == 3 && h < origin_position->vy - 64) {
                    h = r;
                }
            }

            return h;
        }
    }

    if (support_probe_mode == 3) {
        return origin_position->vy;
    }

    return -3000;
}

// decomp: BATTLE.PRG func_800A70DC (src/BATTLE/BATTLE.PRG/3A1A0.c:2256)
void Field_SampleActorCollisionPerimeter(Actors_ActorRecord* actor, int diagonal)
{
    Field_CollisionSampleScratch* samples = (Field_CollisionSampleScratch*)((uintptr_t)Core_ScratchBase() + 0x3BC);
    int i;
    int j;
    int delta;

    actor->wallContact = 0;

    for (i = 0; i < 8; ++i) {
        samples->blocked[i] = 0;
    }

    if (actor->bypassActorCollision || actor->bypassTerrainCollision) {
        for (i = 0; i < 8; ++i) {
            samples->sampleHeights[i] = 0;
        }

        return;
    }

    if (actor->instance.locomotionState == 2) {
        for (i = 0; i < 8; ++i) {
            samples->sampleHeights[i] = actor->instance.position.vy;
        }

        return;
    }

    i = actor->collisionRadius;
    samples->centerAttribute = (*((u_int* (*)(SVECTOR*, int))Field_FindCollisionCellAtPosition)(&actor->instance.position, 0) >> 5) & 1;
    samples->sampleHeights[0] = Field_SampleActorSupportHeight(&actor->instance.position, 0, -i, 0);
    samples->attributes[0] = (*Field_collisionProbeTile >> 5) & 1;
    samples->sampleHeights[6] = Field_SampleActorSupportHeight(&actor->instance.position, i, 0, 0);
    samples->attributes[6] = (*Field_collisionProbeTile >> 5) & 1;
    samples->sampleHeights[2] = Field_SampleActorSupportHeight(&actor->instance.position, -i, 0, 0);
    samples->attributes[2] = (*Field_collisionProbeTile >> 5) & 1;
    samples->sampleHeights[7] = Field_SampleActorSupportHeight(&actor->instance.position, diagonal, -diagonal, 0);
    samples->attributes[7] = (*Field_collisionProbeTile >> 5) & 1;
    samples->sampleHeights[1] = Field_SampleActorSupportHeight(&actor->instance.position, -diagonal, -diagonal, 0);
    samples->attributes[1] = (*Field_collisionProbeTile >> 5) & 1;
    samples->sampleHeights[3] = Field_SampleActorSupportHeight(&actor->instance.position, -diagonal, diagonal, 0);
    samples->attributes[3] = (*Field_collisionProbeTile >> 5) & 1;
    samples->sampleHeights[5] = Field_SampleActorSupportHeight(&actor->instance.position, diagonal, diagonal, 0);
    samples->attributes[5] = (*Field_collisionProbeTile >> 5) & 1;
    samples->sampleHeights[4] = Field_SampleActorSupportHeight(&actor->instance.position, 0, i, 0);
    samples->attributes[4] = (*Field_collisionProbeTile >> 5) & 1;

    if (actor->instance.movementMode == 3 && actor->instance.rootMotion.vy > 0) {
        j = actor->instance.position.vy - 32;
    } else if (actor->turnState.distance == 0) {
        j = actor->instance.position.vy - 64;
    } else {
        j = actor->instance.position.vy - 96;
    }

    for (i = 0; i < 8; ++i) {
        if (samples->sampleHeights[i] > actor->instance.position.vy) {
            samples->sampleHeights[i] = actor->instance.position.vy;
        }

        if ((!samples->attributes[i] || !samples->centerAttribute)
            && j >= samples->sampleHeights[i]) {
            samples->blocked[i] = 1;
            actor->wallContact = 1;
        }
    }

    if (!actor->instance.movementMode) {
        return;
    }

    i = actor->turnState.direction + ONE / 16;
    i /= ONE / 8;

    for (j = 0; j < 3; ++j) {
        delta = Field_RelativeCollisionSampleHeight(samples, i, j - 1, actor->instance.position.vy);

        if (delta < -255) {
            return;
        }

        if (delta >= -192) {
            return;
        }
    }

    for (j = 0; j < 5; ++j) {
        delta = Field_RelativeCollisionSampleHeight(samples, i, j + 2, actor->instance.position.vy);

        if (delta < -192) {
            return;
        }
    }

    samples->sampleHeights[i] = actor->instance.position.vy;
    actor->wallContact = 0;
}

// decomp: BATTLE.PRG func_800A7524 (src/BATTLE/BATTLE.PRG/3A1A0.c:2355)
void Field_ApplyActorWallRepulsion(Actors_ActorRecord* actor, SVECTOR* motion)
{
    Field_CollisionSampleScratch* scratch;
    int x;
    int z;

    if (!actor->wallContact) {
        return;
    }

    scratch = (Field_CollisionSampleScratch*)((uintptr_t)Core_ScratchBase() + 0x3BC);
    x = 0;
    z = (scratch->blocked[0] != 0) * 16;

    if (scratch->blocked[1]) {
        x = 8;
        z += 8;
    }

    if (scratch->blocked[2]) {
        x += 16;
    }

    if (scratch->blocked[3]) {
        x += 8;
        z -= 8;
    }

    if (scratch->blocked[4]) {
        z -= 16;
    }

    if (scratch->blocked[5]) {
        x -= 8;
        z -= 8;
    }

    if (scratch->blocked[6]) {
        x -= 16;
    }

    if (scratch->blocked[7]) {
        x -= 8;
        z += 8;
    }

    if (x > 16) {
        x = 16;
    }

    if (x < -16) {
        x = -16;
    }

    if (z > 16) {
        z = 16;
    }

    if (z < -16) {
        z = -16;
    }

    if (x != 0) {
        if (x < 0) {
            if (motion->vx > 0) {
                motion->vx = 0;
            }
        } else if (motion->vx < 0) {
            motion->vx = 0;
        }
    }

    if (z != 0) {
        if (z < 0) {
            if (motion->vz > 0) {
                motion->vz = 0;
            }
        } else if (motion->vz < 0) {
            motion->vz = 0;
        }
    }

    motion->vx += x;
    motion->vz += z;
    actor->platformOffsetX = 0;
    actor->platformOffsetZ = 0;
}

// decomp: BATTLE.PRG func_800A76BC (src/BATTLE/BATTLE.PRG/3A1A0.c:2443)
int Field_ResolveActorTerrainMotion(Actors_ActorRecord* actor, SVECTOR* motion, int* result, int probeOnly)
{
    int xResults[5];
    int zResults[5];
    int probeDirections[2];
    SVECTOR probe;
    SVECTOR opposite;
    SVECTOR xProbe;
    SVECTOR zProbe;
    u_int* probeTiles[2];
    int sideOffset;
    int cornerStep;
    int diagonal;
    int radius;
    Field_CollisionSampleScratch* scratch;
    int direction;
    int offset;
    int diagonalOffset;
    int crossOffset;
    int crossDiagonalOffset;
    int cross;
    int crossDiagonal;
    int expected;
    int facing;
    int x;
    int z;

    scratch = (Field_CollisionSampleScratch*)((uintptr_t)Core_ScratchBase() + 0x3BC);
    scratch->movingActor = (Field_GimmickInstance*)actor;
    actor->wasOnLedge = actor->onLedge;
    actor->onLedge = 0;
    radius = actor->collisionRadius;
    diagonal = Core_Gte_Sin(ONE / 8) * radius / ONE;
    scratch->searchFloor = -0xC0;
    if (actor->instance.weaponDrawn) {
        scratch->searchCeiling = -0x40;
    } else if (actor->actionPoseActive) {
        scratch->searchFloor = -0x140;
        scratch->searchCeiling = -0x40;
    } else {
        scratch->searchCeiling = -0x100;
    }

    if (probeOnly) {
        scratch->searchCeiling = -0x100;
        actor->bypassTerrainCollision = 0;
        actor->wallContact = 0;
        if (!actor->instance.supportLayer) {
            probeTiles[0] = ((u_int* (*)(SVECTOR*, int))Field_FindCollisionCellAtPosition)(&actor->instance.position, actor->instance.currentTileLayer);
            if (probeTiles[0] == NULL) {
                actor->bypassTerrainCollision = 1;
            }
        }
        probe.vx = actor->instance.position.vx + motion->vx;
        probe.vz = actor->instance.position.vz + motion->vz;
        scratch->sampleHeights[0] = actor->instance.position.vy;
        scratch->searchFloor = 0;
        xResults[0] = Field_ClassifyActorTerrainProbe(&probe, 0);
        Field_collisionProbePosition = probe;
        if (xResults[0] == 0xFF) {
            *result = 0;
            return 3;
        }
        Field_collisionProbeTile = probeTiles[0];
        *result = xResults[0];
        if (*result >= 2) {
            *result = 1;
        }
        return 0;
    }

    if (((int (*)())Field_CanActorSeparateFromOverlaps)(actor, motion) == 0) {
        return 3;
    }
    Field_SampleActorCollisionPerimeter(actor, diagonal);
    Field_ApplyActorWallRepulsion(actor, motion);
    actor->bypassTerrainCollision = 0;
    if (!actor->instance.supportLayer) {
        probeTiles[0] = ((u_int* (*)(SVECTOR*, int))Field_FindCollisionCellAtPosition)(&actor->instance.position, actor->instance.currentTileLayer);
        if (probeTiles[0] == NULL) {
            actor->bypassTerrainCollision = 1;
        } else {
            actor->bypassTerrainCollision = 0;
        }
    }

    if (motion->vx == 0) {
        xResults[0] = 0xFF;
        goto probeZ;
    }
    if (motion->vz == 0) {
        zResults[0] = 0xFF;
        goto probeX;
    }

    direction = 6;
    if (motion->vx < 0) {
        offset = -radius;
        direction = 2;
        diagonalOffset = -diagonal;
    } else {
        offset = radius;
        diagonalOffset = diagonal;
    }
    if (motion->vz < 0) {
        crossOffset = -radius;
        crossDiagonalOffset = -diagonal;
        if (direction == 2) {
            direction = 1;
        } else {
            direction = 7;
        }
    } else {
        crossOffset = radius;
        crossDiagonalOffset = diagonal;
        if (direction == 2) {
            direction = 3;
        } else {
            direction = 5;
        }
    }
    probe.vx = actor->instance.position.vx + motion->vx + diagonalOffset;
    probe.vz = actor->instance.position.vz + motion->vz + crossDiagonalOffset;
    xResults[0] = Field_ClassifyActorTerrainProbe(&probe, direction);
    Field_collisionProbePosition = probe;
    probeTiles[0] = Field_collisionProbeTile;
    probe.vx -= diagonalOffset * 2;
    direction -= 2;
    xResults[1] = Field_ClassifyActorTerrainProbe(&probe, direction);
    probe.vx += diagonalOffset * 2;
    probe.vz -= crossDiagonalOffset * 2;
    direction += 4;
    xResults[2] = Field_ClassifyActorTerrainProbe(&probe, direction);
    probe.vx -= diagonalOffset;
    probe.vz += crossDiagonalOffset + crossOffset;
    direction -= 3;
    xResults[3] = Field_ClassifyActorTerrainProbe(&probe, direction);
    probe.vx += offset;
    probe.vz -= crossOffset;
    direction += 2;
    xResults[4] = Field_ClassifyActorTerrainProbe(&probe, direction);
    // offset/diagonalOffset are reused for the height delta and the climb limit
    offset = Field_collisionProbePosition.vy - actor->instance.position.vy;
    diagonalOffset = offset / 64;
    if (offset & 0x3F) {
        diagonalOffset--;
    }
    diagonalOffset = -diagonalOffset;
    if (diagonalOffset <= 0) {
        diagonalOffset = 1;
    }
    if (xResults[0] != 0xFF && xResults[1] <= diagonalOffset
        && xResults[2] <= diagonalOffset && xResults[3] <= diagonalOffset
        && xResults[4] <= diagonalOffset && (xResults[0] < 4 || (direction & 1))) {
        Field_collisionProbeTile = probeTiles[0];
        Field_collisionProbeDirection = (direction - 1) & 7;
        *result = xResults[0];
        return 0;
    }

    if (motion->vx != 0) {
    probeX:
        direction = 6;
        if (motion->vx < 0) {
            offset = -radius;
            direction = 2;
            diagonalOffset = -diagonal;
        } else {
            offset = radius;
            diagonalOffset = diagonal;
        }
        probe.vx = actor->instance.position.vx + motion->vx + offset;
        probe.vz = actor->instance.position.vz;
        xResults[0] = Field_ClassifyActorTerrainProbe(&probe, direction);
        *(Field_SavedPositionWords*)&xProbe = *(Field_SavedPositionWords*)&probe;
        probeDirections[0] = direction;
        direction -= 2;
        probeTiles[0] = Field_collisionProbeTile;
        probe.vx -= offset;
        probe.vz += offset;
        xResults[1] = Field_ClassifyActorTerrainProbe(&probe, direction);
        direction += 4;
        probe.vz -= offset * 2;
        xResults[2] = Field_ClassifyActorTerrainProbe(&probe, direction);
        direction -= 3;
        probe.vx += diagonalOffset;
        probe.vz += offset + diagonalOffset;
        xResults[3] = Field_ClassifyActorTerrainProbe(&probe, direction);
        probe.vz -= diagonalOffset * 2;
        xResults[4] = Field_ClassifyActorTerrainProbe(&probe, direction + 2);
    } else {
        xResults[0] = 0xFF;
    }

    if (motion->vz != 0) {
    probeZ:
        direction = 4;
        if (motion->vz < 0) {
            offset = -radius;
            direction = 0;
            diagonalOffset = -diagonal;
        } else {
            offset = radius;
            diagonalOffset = diagonal;
        }
        probe.vx = actor->instance.position.vx;
        probe.vz = actor->instance.position.vz + motion->vz + offset;
        zResults[0] = Field_ClassifyActorTerrainProbe(&probe, direction);
        *(Field_SavedPositionWords*)&zProbe = *(Field_SavedPositionWords*)&probe;
        probeDirections[1] = direction;
        direction -= 2;
        probeTiles[1] = Field_collisionProbeTile;
        probe.vx -= offset;
        probe.vz -= offset;
        zResults[1] = Field_ClassifyActorTerrainProbe(&probe, direction);
        direction += 4;
        probe.vx += offset * 2;
        zResults[2] = Field_ClassifyActorTerrainProbe(&probe, direction);
        direction -= 3;
        probe.vx -= offset + diagonalOffset;
        probe.vz += diagonalOffset;
        zResults[3] = Field_ClassifyActorTerrainProbe(&probe, direction);
        probe.vx += diagonalOffset * 2;
        zResults[4] = Field_ClassifyActorTerrainProbe(&probe, direction + 2);
    } else {
        zResults[0] = 0xFF;
    }

    x = motion->vx;
    z = motion->vz;

    if (ABS(x) == ABS(z) || ABS(x) >= ABS(z)) {
        offset = xProbe.vy - actor->instance.position.vy;
        diagonalOffset = offset / 64;
        if (offset & 0x3F) {
            diagonalOffset--;
        }
        diagonalOffset = -diagonalOffset;
        if (diagonalOffset <= 0) {
            diagonalOffset = 1;
        }
        if (xResults[0] != 0xFF && xResults[1] <= diagonalOffset
            && xResults[2] <= diagonalOffset && xResults[3] <= diagonalOffset
            && xResults[4] <= diagonalOffset) {
            if (actor->wasOnLedge) {
                if (x < 0) {
                    facing = actor->terrainFacing;
                    expected = 6;
                } else {
                    facing = actor->terrainFacing;
                    expected = 2;
                }
                if (facing == expected) {
                    goto slideX;
                }
            }
            motion->vz = 0;
            *result = xResults[0];
            Field_collisionProbePosition = xProbe;
            Field_collisionProbeTile = probeTiles[0];
            Field_collisionProbeDirection = probeDirections[0];
            return 1;
        }
        if (zResults[0] != 0xFF && zResults[1] < 2 && zResults[2] < 2 && zResults[3] < 2
            && zResults[4] < 2) {
            if (actor->wasOnLedge) {
                if (motion->vz < 0) {
                    if (actor->terrainFacing == 4) {
                        goto slideX;
                    }
                } else if (actor->terrainFacing == 0) {
                    goto slideX;
                }
            }
            motion->vx = 0;
            *result = zResults[0];
            Field_collisionProbePosition = zProbe;
            Field_collisionProbeTile = probeTiles[1];
            Field_collisionProbeDirection = probeDirections[1];
            return 2;
        }
    slideX:
        if (actor->instance.movementMode) {
            return 3;
        }
        direction = 6;
        if (motion->vx < 0) {
            offset = -radius;
            direction = 2;
            diagonalOffset = -diagonal;
        } else {
            offset = radius;
            diagonalOffset = diagonal;
        }
        direction -= 1;
        probe.vx = actor->instance.position.vx + motion->vx + offset;
        probe.vz = actor->instance.position.vz + offset;
        zResults[0] = Field_ClassifyActorTerrainProbe(&probe, direction);
        direction += 2;
        opposite.vx = probe.vx;
        opposite.vz = actor->instance.position.vz - offset;
        zResults[1] = Field_ClassifyActorTerrainProbe(&opposite, direction);
        if (zResults[0] == 0xFF && zResults[1] == 0xFF) {
            return 3;
        }
        // crossOffset/crossDiagonalOffset are reused for the half step and the turn
        crossOffset = motion->vx / 2;
        if (zResults[0] < 2) {
            if (zResults[1] < 2) {
                direction -= 2;
                probe.vx = actor->instance.position.vx + motion->vx;
                probe.vz = actor->instance.position.vz + offset;
                zResults[2] = Field_ClassifyActorTerrainProbe(&probe, direction);
                direction += 2;
                opposite.vx = probe.vx;
                opposite.vz = actor->instance.position.vz - offset;
                zResults[3] = Field_ClassifyActorTerrainProbe(&opposite, direction);
                if (zResults[2] == 0xFF && zResults[3] == 0xFF) {
                    return 3;
                }
                if (zResults[2] > 0) {
                    goto slideXBack;
                }
                if (zResults[3] <= 0 && (zResults[0] != 0 || zResults[1] != 1)) {
                    if (zResults[0] == 1 && zResults[1] == 1) {
                        goto slideXBack;
                    }
                    if (probe.vy <= opposite.vy) {
                        goto slideXBack;
                    }
                }
            }
            sideOffset = offset;
            crossDiagonal = -diagonalOffset;
            cross = crossOffset;
            crossDiagonalOffset = 1;
            cornerStep = -3;
        slideXProbe:
            direction -= crossDiagonalOffset;
            probe.vx = actor->instance.position.vx + crossOffset + offset;
            probe.vz = actor->instance.position.vz + cross;
            zResults[1] = Field_ClassifyActorTerrainProbe(&probe, direction);
            if (zResults[1] < scratch->searchCeiling) {
                zResults[1] = 0xFF;
            }
            direction += crossDiagonalOffset;
            probe.vx = actor->instance.position.vx + crossOffset + diagonalOffset;
            probe.vz = actor->instance.position.vz + cross + crossDiagonal;
            zResults[2] = Field_ClassifyActorTerrainProbe(&probe, direction);
            if (zResults[2] < scratch->searchCeiling) {
                zResults[2] = 0xFF;
            }
            direction += cornerStep;
            probe.vx = actor->instance.position.vx;
            probe.vz = actor->instance.position.vz + crossOffset + sideOffset;
            zResults[3] = Field_ClassifyActorTerrainProbe(&probe, direction);
            if (zResults[3] < scratch->searchCeiling) {
                zResults[3] = 0xFF;
            }
            Field_collisionProbePosition = probe;
            if (zResults[1] < 2 && zResults[2] < 2) {
                if (zResults[3] >= 2) {
                    return 3;
                }
                probe.vx = actor->instance.position.vx + crossOffset;
                probe.vz = actor->instance.position.vz + cross + sideOffset;
                zResults[0] = Field_ClassifyActorTerrainProbe(&probe, direction);
                if (zResults[0] >= 2) {
                    return 3;
                }
                direction += crossDiagonalOffset;
                offset = 0;
            slideXMove:
                motion->vx = crossOffset;
                motion->vz = cross;
                goto moved;
            }
            if (zResults[3] >= 2) {
                return 3;
            }
            crossOffset = 0;
            offset = 2;
            goto slideXMove;
        }
        if (zResults[1] >= 2) {
            return 3;
        }
    slideXBack:
        direction -= 2;
        sideOffset = -offset;
        crossDiagonal = diagonalOffset;
        cross = -crossOffset;
        crossDiagonalOffset = -1;
        cornerStep = 3;
        goto slideXProbe;
    }

    offset = zProbe.vy - actor->instance.position.vy;
    diagonalOffset = offset / 64;
    if (offset & 0x3F) {
        diagonalOffset--;
    }
    diagonalOffset = -diagonalOffset;
    if (diagonalOffset <= 0) {
        diagonalOffset = 1;
    }
    if (zResults[0] != 0xFF && zResults[1] <= diagonalOffset
        && zResults[2] <= diagonalOffset && zResults[3] <= diagonalOffset
        && zResults[4] <= diagonalOffset) {
        if (actor->wasOnLedge) {
            if (z < 0) {
                if (actor->terrainFacing == 4) {
                    goto slideZ;
                }
            } else if (actor->terrainFacing == 0) {
                goto slideZ;
            }
        }
        motion->vx = 0;
        *result = zResults[0];
        Field_collisionProbePosition = zProbe;
        Field_collisionProbeTile = probeTiles[1];
        Field_collisionProbeDirection = probeDirections[1];
        return 2;
    }
    if (xResults[0] != 0xFF && xResults[1] < 2 && xResults[2] < 2 && xResults[3] < 2
        && xResults[4] < 2) {
        if (actor->wasOnLedge) {
            if (motion->vx < 0) {
                facing = actor->terrainFacing;
                expected = 6;
            } else {
                facing = actor->terrainFacing;
                expected = 2;
            }
            if (facing == expected) {
                goto slideZ;
            }
        }
        motion->vz = 0;
        *result = xResults[0];
        Field_collisionProbePosition = xProbe;
        Field_collisionProbeTile = probeTiles[0];
        Field_collisionProbeDirection = probeDirections[0];
        return 1;
    }
slideZ:
    if (actor->instance.movementMode) {
        return 3;
    }
    direction = 4;
    if (motion->vz < 0) {
        offset = -radius;
        direction = 0;
        diagonalOffset = -diagonal;
    } else {
        offset = radius;
        diagonalOffset = diagonal;
    }
    direction -= 1;
    probe.vz = actor->instance.position.vz + motion->vz + offset;
    probe.vx = actor->instance.position.vx - offset;
    zResults[0] = Field_ClassifyActorTerrainProbe(&probe, direction);
    direction += 2;
    opposite.vz = probe.vz;
    opposite.vx = actor->instance.position.vx + offset;
    zResults[1] = Field_ClassifyActorTerrainProbe(&opposite, direction);
    if (zResults[0] == 0xFF && zResults[1] == 0xFF) {
        return 3;
    }
    crossOffset = motion->vz / 2;
    if (zResults[0] < 2) {
        if (zResults[1] < 2) {
            direction -= 2;
            probe.vz = actor->instance.position.vz + motion->vz;
            probe.vx = actor->instance.position.vx - offset;
            zResults[2] = Field_ClassifyActorTerrainProbe(&probe, direction);
            direction += 2;
            opposite.vz = probe.vz;
            opposite.vx = actor->instance.position.vx + offset;
            zResults[3] = Field_ClassifyActorTerrainProbe(&opposite, direction);
            if (zResults[2] == 0xFF && zResults[3] == 0xFF) {
                return 3;
            }
            if (zResults[2] > 0) {
                goto slideZBack;
            }
            if (zResults[3] <= 0 && (zResults[0] != 0 || zResults[1] != 1)) {
                if (zResults[0] == 1 && zResults[1] == 1) {
                    goto slideZBack;
                }
                if (probe.vy <= opposite.vy) {
                    goto slideZBack;
                }
            }
        }
        sideOffset = -offset;
        crossDiagonal = diagonalOffset;
        cross = -crossOffset;
        crossDiagonalOffset = 1;
        cornerStep = -3;
    slideZProbe:
        direction -= crossDiagonalOffset;
        probe.vz = actor->instance.position.vz + crossOffset + offset;
        probe.vx = actor->instance.position.vx + cross;
        zResults[1] = Field_ClassifyActorTerrainProbe(&probe, direction);
        if (zResults[1] < scratch->searchCeiling) {
            zResults[1] = 0xFF;
        }
        direction += crossDiagonalOffset;
        probe.vz = actor->instance.position.vz + crossOffset + diagonalOffset;
        probe.vx = actor->instance.position.vx + cross + crossDiagonal;
        zResults[2] = Field_ClassifyActorTerrainProbe(&probe, direction);
        if (zResults[2] < scratch->searchCeiling) {
            zResults[2] = 0xFF;
        }
        direction += cornerStep;
        probe.vz = actor->instance.position.vz;
        probe.vx = actor->instance.position.vx + cross + sideOffset;
        zResults[3] = Field_ClassifyActorTerrainProbe(&probe, direction);
        if (zResults[3] < scratch->searchCeiling) {
            zResults[3] = 0xFF;
        }
        Field_collisionProbePosition = probe;
        if (zResults[1] < 2 && zResults[2] < 2) {
            if (zResults[3] >= 2) {
                return 3;
            }
            probe.vx = actor->instance.position.vx + cross + sideOffset;
            probe.vz = actor->instance.position.vz + crossOffset;
            zResults[0] = Field_ClassifyActorTerrainProbe(&probe, direction);
            if (zResults[0] >= 2) {
                return 3;
            }
            direction += crossDiagonalOffset;
            offset = 0;
        slideZMove:
            motion->vz = crossOffset;
            motion->vx = cross;
        moved:
            *result = zResults[3];
            actor->terrainFacing = direction & 7;
            actor->onLedge = 1;
            return offset;
        }
        if (zResults[3] >= 2) {
            return 3;
        }
        crossOffset = 0;
        offset = 1;
        goto slideZMove;
    }
    if (zResults[1] < 2) {
    slideZBack:
        direction -= 2;
        crossDiagonal = -diagonalOffset;
        cross = crossOffset;
        crossDiagonalOffset = -1;
        sideOffset = offset;
        cornerStep = 3;
        goto slideZProbe;
    }
    return 3;
}

// decomp: BATTLE.PRG func_800A8B34 (src/BATTLE/BATTLE.PRG/3A1A0.c:3008)
int Field_ClassifyActorTerrainProbe(SVECTOR* probe_position, int sample_sector)
{
    u_int* ent;
    int h;
    int elev2;
    int gx;
    int gz;
    int r;
    int los;
    int diff;
    int q;
    Field_CollisionSampleScratch* sb = (Field_CollisionSampleScratch*)((uintptr_t)Core_ScratchBase() + 0x3BC);
    Field_GimmickInstance* actor = sb->movingActor;
    int limit = sb->sampleHeights[sample_sector & 7];

    if (actor->bypassActorCollision << 9) {
        return 0;
    }

    gx = probe_position->vx / 128;
    gz = probe_position->vz / 128;
    elev2 = Field_CeilingHeightAt(probe_position->vx, probe_position->vz);
    elev2 <<= 17;
    elev2 >>= 17;

    if (limit + sb->searchFloor < elev2) {
        return 0xFF;
    }

    ent = ((u_int* (*)(SVECTOR*, int))Field_FindCollisionCellAtPosition)(probe_position, 0);

    if (ent == NULL) {
        return 0xFF;
    }

    h = Field_FloorHeightAt(probe_position->vx, probe_position->vz);

    if (h < 0) {
        return 0xFF;
    }

    h <<= 17;
    h >>= 17;

    r = Field_GimmickSupportHeight(gx, gz);

    if (r != 0) {
        h = r;
    }

    if ((*ent >> 17) & 1) {
        if (h >= 0) {
            h = 0xBB8;
        }
    }

    if (h - actor->positionY < -0x17F) {
        return 0xFF;
    }

    los = Field_MovingPlatformContactHeight(probe_position->vx, limit, probe_position->vz, 0);

    if (los == 0) {
        probe_position->pad = 0;
    } else {
        probe_position->pad = Actors_supportingPlatformLayer;
        h = los;
    }

    probe_position->pad |= Actors_supportingGimmickIndex << 8;

    if (sb->searchFloor < elev2 - h) {
        return 0xFF;
    }

    if (h < sb->searchCeiling + limit) {
        return 0xFF;
    }

    if (((int (*)(Field_GimmickInstance*, SVECTOR*))Field_IsActorPositionUnoccupied)(actor, probe_position) == 0) {
        return 0xFF;
    }

    probe_position->vy = h;
    diff = h - limit;
    q = diff / 64;

    if (q >= 0) {
        return 0;
    }

    if (diff & 0x3F) {
        --q;
    }

    return -q;
}

// decomp: BATTLE.PRG func_800A8D64 (src/BATTLE/BATTLE.PRG/40564.c:139)
Field_MpdTileCell* Field_FindCollisionCellAtPosition(SVECTOR* world_position, int collision_layer)
{
    int tile_x_or_local_x = world_position->vx / 128;
    int tile_z_or_local_z = world_position->vz / 128;
    Field_MpdTileCell* collision_cell = Field_CollisionCellAt(tile_x_or_local_x, tile_z_or_local_z, collision_layer);

    if (collision_cell == NULL) {
        return NULL;
    }

    tile_x_or_local_x = world_position->vx - (tile_x_or_local_x << 7);
    tile_z_or_local_z = world_position->vz - (tile_z_or_local_z << 7);

    // BUG: potentially unhandled values
    switch (collision_cell->doorPassability) {
    case 0:
        return collision_cell;

    case 1:
        if ((tile_x_or_local_x + tile_z_or_local_z) > 0x80) {
            return NULL;
        }
        return collision_cell;

    case 2:
        if ((tile_x_or_local_x - tile_z_or_local_z) > 0) {
            return NULL;
        }
        return collision_cell;

    case 3:
        if ((tile_x_or_local_x + tile_z_or_local_z) < 0x80) {
            return NULL;
        }
        return collision_cell;

    case 4:
        if ((tile_x_or_local_x - tile_z_or_local_z) < 0) {
            return NULL;
        }
        return collision_cell;

    case 5:
        return NULL;
    }
}

// decomp: src/BATTLE/BATTLE.PRG/40564.c:108
#define getActorFlags(actor) (*((int*)&(actor)->sequenceFlags - 2))
// decomp: BATTLE.PRG func_800A8E84 (src/BATTLE/BATTLE.PRG/40564.c:186)
int Field_IsActorPositionUnoccupied(Actors_ActorRecord* actor, SVECTOR* position)
{
    Field_CollisionSampleScratch* scratch = (Field_CollisionSampleScratch*)((uintptr_t)Core_ScratchBase() + 0x3BC);
    int i;

    if (actor->instance.slot != 0) {
        return 1;
    }

    if (actor->ignoredByCollision << 11) {
        return 1;
    }

    if (actor->wallContact << 12) {
        return 1;
    }

    for (i = 2; i < 17; ++i) {
        Actors_ActorRecord* other = Actors_actorRecords[i];
        int radius;

        if (other == NULL || other == actor) {
            continue;
        }

        if (other->instance.isSkipped || (getActorFlags(other) & 0x1000000)
            || other->instance.attachBone == 0xFC || other->instance.reactionState == 2
            || scratch->excluded[i]) {
            continue;
        }

        radius = other->occupancyRadius;

        if (other->instance.position.vx + radius < position->vx
            || position->vx < other->instance.position.vx - radius
            || other->instance.position.vz + radius < position->vz
            || position->vz < other->instance.position.vz - radius
            || other->instance.position.vy < actor->instance.position.vy - actor->occupancyHeight
            || actor->instance.position.vy < other->instance.position.vy - other->occupancyHeight) {
            continue;
        }

        return 0;
    }

    return 1;
}
#undef getActorFlags

// decomp: src/BATTLE/BATTLE.PRG/40564.c:108
#define getActorFlags(actor) (*((int*)&(actor)->sequenceFlags - 2))
// decomp: BATTLE.PRG func_800A8FD4 (src/BATTLE/BATTLE.PRG/40564.c:234)
int Field_CanActorSeparateFromOverlaps(Actors_ActorRecord* actor, SVECTOR* motion)
{
    Field_CollisionSampleScratch* scratch = (Field_CollisionSampleScratch*)((uintptr_t)Core_ScratchBase() + 0x3BC);
    Field_CollisionSampleScratch* clear;
    Actors_ActorRecord** entry;
    int flags;
    int i;
    int radius;
    int minX;
    int maxX;
    int minY;
    int minZ;
    int maxZ;

    for (i = 16, clear = (Field_CollisionSampleScratch*)((u_char*)scratch + 16); i >= 0; --i) {
        clear->excluded[0] = 0;
        clear = (Field_CollisionSampleScratch*)((u_char*)clear - 1);
    }

    flags = getActorFlags(actor);

    if (flags & 0x800) {
        return flags & 0x800;
    }

    radius = actor->occupancyRadius;
    minY = actor->instance.position.vy - actor->occupancyHeight;
    minX = actor->instance.position.vx - radius;
    maxX = actor->instance.position.vx + radius;
    minZ = actor->instance.position.vz - radius;
    maxZ = actor->instance.position.vz + radius;
    i = 2;

    if (flags & 8) {
        minY -= 128;
    }

    for (entry = &Actors_actorRecords[i]; i < 17; ++entry) {
        Actors_ActorRecord* other = *entry;
        int otherRadius;
        int dx;
        int dz;
        int oldDistance;

        if (other != NULL
            && !(other->instance.isSkipped || (getActorFlags(other) & 0x1000000)
                 || other->instance.attachBone == 0xFC || other->instance.reactionState == 2)) {
            otherRadius = other->occupancyRadius;

            if (!(other->instance.position.vx + otherRadius < minX
                    || maxX < other->instance.position.vx - otherRadius
                    || other->instance.position.vz + otherRadius < minZ
                    || maxZ < other->instance.position.vz - otherRadius
                    || other->instance.position.vy < minY
                    || actor->instance.position.vy
                           < other->instance.position.vy - other->occupancyHeight)) {
                scratch->excluded[i] = 1;
                dx = other->instance.position.vx - actor->instance.position.vx;
                dx *= dx;
                dz = other->instance.position.vz - actor->instance.position.vz;
                dz *= dz;
                oldDistance = dx + dz;
                dx = other->instance.position.vx - (actor->instance.position.vx + motion->vx);
                dx *= dx;
                dz = other->instance.position.vz - (actor->instance.position.vz + motion->vz);
                dz *= dz;
                dx += dz;
                ++i;

                if (dx < oldDistance) {
                    return 0;
                }

                continue;
            }
        }

        ++i;
    }

    return 1;
}
#undef getActorFlags

// decomp: BATTLE.PRG func_800A92B8 (src/BATTLE/BATTLE.PRG/40564.c:343)
int Field_GimmickSupportHeight(int query_tile_x, int query_tile_z)
{
    int minimum_gimmick_y_or_support_height;
    int i;

    minimum_gimmick_y_or_support_height = 0xBB8;
    Actors_supportingGimmickIndex = 0;

    for (i = 0; i < 16; ++i) {

        Field_GimmickInstance* gimmick_record = Field_gimmickInstances[i];

        if (gimmick_record == NULL) {
            continue;
        }

        if (!gimmick_record->isSkipped && (gimmick_record->tileX == query_tile_x) && (gimmick_record->tileZ == query_tile_z)
            && !gimmick_record->motionState && (gimmick_record->mode != 0xFE)
            && (gimmick_record->positionY < minimum_gimmick_y_or_support_height)) {

            minimum_gimmick_y_or_support_height = gimmick_record->positionY;
            Actors_supportingGimmickIndex = i;
        }
    }

    minimum_gimmick_y_or_support_height -= 128;

    if (minimum_gimmick_y_or_support_height > 0) {
        minimum_gimmick_y_or_support_height = 0;
    }

    return minimum_gimmick_y_or_support_height;
}

// decomp: BATTLE.PRG func_800A9378 (src/BATTLE/BATTLE.PRG/40564.c:377)
int Field_MovingPlatformContactHeight(int world_x, int world_y, int world_z, int contact_search_mode)
{
    Field_PlatformContactPoint platform_contacts[4];
    int count;
    int i;

    (*(int (*))&Actors_supportingPlatformLayer) = 0;
    count = Field_CollectMovingPlatformPositions(platform_contacts);

    if (count == 0) {
        return 0;
    }

    for (i = 0; i < count; ++i) {
        if ((world_x >= platform_contacts[i].x + 64) || (world_x < platform_contacts[i].x - 64)) {
            continue;
        }

        if ((world_z >= platform_contacts[i].z + 64) || (world_z < platform_contacts[i].z - 64)) {
            continue;
        }

        switch (contact_search_mode) {
        case 0:
            if (platform_contacts[i].height < world_y - 256) {
                continue;
            }
            break;

        case 1:
            if (platform_contacts[i].height < world_y - 256) {
                return platform_contacts[i].height + 96;
            }
            continue;

        case 2:
            if ((platform_contacts[i].height >= world_y) || (platform_contacts[i].height < world_y - 128)) {
                continue;
            }
            break;

        case 3:
            if ((platform_contacts[i].height >= world_y) || (platform_contacts[i].height < world_y - Field_platformContactSearchDepth)) {
                continue;
            }
            return platform_contacts[i].height + 96;
        }

        (*(int (*))&Actors_supportingPlatformLayer) = platform_contacts[i].platformIndex + 2;
        return platform_contacts[i].height;
    }

    return 0;
}

// decomp: src/BATTLE/BATTLE.PRG/40564.c:108
#define getActorFlags(actor) (*((int*)&(actor)->sequenceFlags - 2))
// decomp: BATTLE.PRG func_800A9530 (src/BATTLE/BATTLE.PRG/40564.c:432)
int Field_CalcActorLedgeOffset(Actors_ActorRecord* actor, SVECTOR* offset)
{
    int position = actor->collisionRadius;
    int direction = Field_collisionProbeDirection >> 1;
    int edge;
    int surface;
    int distance;

    actor->moveFacing = direction * (ONE / 4);

    if (actor->instance.supportLayer) {
        Field_PlatformContactPoint* platform = Field_FindMovingPlatformById(actor->instance.supportLayer);

        edge = 65;

        if (direction < 2) {
            position = -position;
        } else {
            edge = -65;
        }

        if (direction & 1) {
            position += actor->instance.position.vx;
            edge += platform->x;
            offset->vx = edge - position;
        } else {
            position += actor->instance.position.vz;
            edge += platform->z;
            offset->vz = edge - position;
        }
    } else {
        edge = 0;

        if (direction < 2) {
            position = -position;
        } else {
            edge = 127;
        }

        if (direction & 1) {
            position += actor->instance.position.vx;
            offset->vx = edge - (position & 127);
        } else {
            position += actor->instance.position.vz;
            offset->vz = edge - (position & 127);
        }

        if (offset->vx > 32) {
            offset->vx -= 64;
        }

        if (offset->vx < -32) {
            offset->vx += 64;
        }

        if (offset->vz > 32) {
            offset->vz -= 64;
        }

        if (offset->vz < -32) {
            offset->vz += 64;
        }

        if (getActorFlags(actor) & 0xF0000000) {
            offset->vy += 12;
        }

        offset->vy += 8;
    }

    surface = actor->instance.supportLayer;
    actor->instance.supportLayer = 0;
    actor->instance.position.vx += offset->vx;
    actor->instance.position.vz += offset->vz;
    distance = Core_Gte_Sin(ONE / 8);
    distance *= actor->collisionRadius;
    distance /= ONE;
    Field_SampleActorCollisionPerimeter(actor, distance);
    actor->instance.position.vx -= offset->vx;
    actor->instance.position.vz -= offset->vz;
    actor->instance.supportLayer = surface;

    if (actor->wallContact << 12) {
        actor->wallContact = 0;
        return 0;
    }

    return 1;
}
#undef getActorFlags

// decomp: BATTLE.PRG func_800B13CC (src/BATTLE/BATTLE.PRG/44F14.c:2020)
int Field_GimmickSupportHeightBelow(int query_tile_x, int query_tile_z, int reference_height)
{
    int i;
    int minimum_gimmick_y_or_support_height = 0xBB8;

    for (i = 0; i < 16; ++i) {

        Field_GimmickInstance* gimmick_record = Field_gimmickInstances[i];

        if ((gimmick_record != NULL) && !(gimmick_record->isSkipped) && !(gimmick_record->objectInfo.isFalling)
            && (gimmick_record->tileX == query_tile_x) && (gimmick_record->tileZ == query_tile_z)) {

            int gimmick_height = gimmick_record->positionY;

            if ((reference_height < gimmick_height) && (gimmick_height < minimum_gimmick_y_or_support_height)) {
                minimum_gimmick_y_or_support_height = gimmick_height;
            }
        }
    }

    minimum_gimmick_y_or_support_height -= 128;

    if (minimum_gimmick_y_or_support_height > 0) {
        minimum_gimmick_y_or_support_height = 0;
    }

    return minimum_gimmick_y_or_support_height;
}

