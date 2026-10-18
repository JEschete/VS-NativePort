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
Field_MpdTileCell* Field_CollisionCellAt(u_int width, u_int height, int arg2)
{
    if (Field_roomData.tileCells != NULL) {
        if (arg2 >= 2) {
            if (Field_roomData.movingPlatforms != NULL) {
                return &Field_roomData.movingPlatforms[arg2 - 2].tileCell;
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
int Field_SetCollisionCellBlocked(int arg0, int arg1, int arg2, int arg3)
{
    Field_MpdTileCell* temp_a0;

    if (Field_roomData.tileCells != NULL) {
        if (arg3 != 0) {
            arg3 = 5;
        }
        if (arg2 >= 2) {
            if (Field_roomData.movingPlatforms != NULL) {
                temp_a0 = &Field_roomData.movingPlatforms[arg2 - 2].tileCell;
                temp_a0->doorPassability = arg3;
                return 1;
            }
        } else {
            temp_a0 =
                &Field_roomData
                     .tileCells[(arg2 * Field_roomData.collisionMap->width
                                   * Field_roomData.collisionMap->height)
                               + (arg1 * Field_roomData.collisionMap->width)
                               + arg0];
            temp_a0->doorPassability = arg3;
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008DA24 (src/BATTLE/BATTLE.PRG/146C.c:17000)
short Field_InterpolatedFloorHeight(int arg0, int arg1)
{
    Field_MpdCollisionHeader* collisionMap;
    int temp_fp;
    int temp_s0_2;
    int temp_s0_3;
    int temp_s7;
    int temp_v0;
    int temp_v0_2;
    int var_s0;
    int var_s1;
    int var_s3;
    int var_s5;
    int var_v0;
    int var_v0_3;
    int v;
    int temp_s6 = 0x8000;

    if (Field_roomData.collisionMap != NULL) {
        temp_s7 = arg0;
        temp_s7 = temp_s7 & 0x1F;
        arg0 >>= 5;
        temp_fp = arg1 & 0x1F;
        arg1 >>= 5;
        collisionMap = Field_roomData.collisionMap;

        if (((arg0 >> 2) >= (u_int)collisionMap->width)
            || ((arg1 >> 2) >= (u_int)collisionMap->height)) {
            return -0x8000;
        }

        temp_s6 = Field_SampleCollisionHeightCode(arg0, arg1, 0);
        var_s5 = (collisionMap->heightOrigin - ((temp_s6 & 0x7F) * 2)) * 8;

        var_s3 = temp_s7 < 0x10 ? arg0 - 1 : arg0 + 1;
        var_s1 = arg1 - 1;

        if (temp_fp >= 0x10) {
            var_s1 = arg1 + 1;
        }

        if ((var_s3 >> 2) < (u_int)collisionMap->width) {
            var_s3 =
                (collisionMap->heightOrigin - ((Field_SampleCollisionHeightCode(var_s3, arg1, 0) & 0x7F) * 2)) * 8;
            var_v0 = var_s3 - var_s5;
            if (ABS(var_v0) >= 0x40) {
                var_s3 = var_s5;
            }
        } else {
            var_s3 = var_s5;
        }

        if ((var_s1 >> 2) < (u_int)collisionMap->height) {
            var_s1 =
                (collisionMap->heightOrigin - ((Field_SampleCollisionHeightCode(arg0, var_s1, 0) & 0x7F) * 2)) * 8;
            var_v0 = var_s1 - var_s5;
            if (ABS(var_v0) >= 0x40) {
                var_s1 = var_s5;
            }
        } else {
            var_s1 = var_s5;
        }

        var_s0 = temp_s7 - 0x10;
        v = ABS(var_s0);
        var_v0_3 = temp_fp - 0x10;

        temp_s0_2 = v | (ABS(var_v0_3) << 0x10);
        temp_s0_3 = Core_Gte_NormalClip(0, 0x20, temp_s0_2);
        temp_v0 = Core_Gte_NormalClip(0, temp_s0_2, 0x200000);

        temp_v0_2 = 0x400 - temp_v0;
        temp_v0_2 -= temp_s0_3;

        if (temp_v0_2 >= 0) {
            temp_v0_2 = (var_s1 * temp_s0_3) + (var_s3 * temp_v0) + (var_s5 * temp_v0_2);
            var_s5 = temp_v0_2 / 1024;
        }
        temp_s6 = (var_s5 & 0x7FFF) | ((temp_s6 & 0x80) << 8);
    }
    return temp_s6;
}

// decomp: BATTLE.PRG func_8008DC7C (src/BATTLE/BATTLE.PRG/146C.c:17083)
short Field_FloorHeightAt(int arg0, int arg1)
{
    Field_MpdCollisionHeader* collisionMap = Field_roomData.collisionMap;
    int var_a2 = 0x8000;

    if ((collisionMap != NULL) && ((u_int)(arg0 >> 7) < collisionMap->width)
        && ((u_int)(arg1 >> 7) < collisionMap->height)) {
        var_a2 = Field_SampleCollisionHeightCode(arg0 >> 5, arg1 >> 5, 0);
        var_a2 = (((collisionMap->heightOrigin - ((var_a2 & 0x7F) * 2)) * 8) & 0x7FFF)
               | ((var_a2 & 0x80) << 8);
    }
    return var_a2;
}

// decomp: BATTLE.PRG func_8008DD0C (src/BATTLE/BATTLE.PRG/146C.c:17097)
short Field_CeilingHeightAt(int arg0, int arg1)
{
    int temp_v1;
    Field_MpdCollisionHeader* collisionMap = Field_roomData.collisionMap;
    int var_a2 = 0;
    int new_var = 8;

    if ((Field_roomData.collisionMap != NULL)
        && ((arg0 >> 7) < (u_int)collisionMap->width)
        && ((arg1 >> 7) < (u_int)collisionMap->height)) {
        var_a2 = Field_SampleCollisionHeightCode(arg0 >> 5, arg1 >> 5, 1);
        temp_v1 = (collisionMap->heightOrigin - ((var_a2 & 0x7F) * 2)) * new_var;
        if (temp_v1 > 0) {
            var_a2 = 0xF810;
        } else {
            var_a2 = (temp_v1 & 0x7FFF) | ((var_a2 & 0x80) << new_var);
        }
    }
    return var_a2;
}

// decomp: BATTLE.PRG func_8008DDA8 (src/BATTLE/BATTLE.PRG/146C.c:17118)
int Field_FloorAndCeilingHeightsAt(int arg0, int arg1)
{
    int sp10 = 0x80008000;
    short* new_var = (short*)&sp10;

    Field_MpdCollisionHeader* collisionMap = Field_roomData.collisionMap;

    if ((collisionMap != NULL) && ((u_int)(arg0 >> 7) < collisionMap->width)) {
        if ((u_int)(arg1 >> 7) < collisionMap->height) {
            int temp_v1;
            int temp_a3 = Field_SampleCollisionHeightCode(arg0 >> 5, arg1 >> 5, 0);
            temp_a3 = (((collisionMap->heightOrigin - ((temp_a3 & 0x7F) * 2)) * 8) & 0x7FFF)
                    | ((temp_a3 & 0x80) << 8);
            new_var[0] = temp_a3;
            temp_a3 = Field_SampleCollisionHeightCode(arg0 >> 5, arg1 >> 5, 1);
            temp_v1 = collisionMap->heightOrigin - (temp_a3 & 0x7F) * 2;
            temp_v1 *= 8;
            if (temp_v1 > 0) {
                temp_a3 = 0xF810;
            } else {
                temp_v1 &= 0x7FFF;
                temp_a3 = temp_v1 | ((temp_a3 & 0x80) << 8);
            }
            new_var[1] = temp_a3;
        }
    }
    return sp10;
}

// decomp: BATTLE.PRG func_800A0A1C (src/BATTLE/BATTLE.PRG/30DB0.c:3994)
void Field_SetActorCollisionBypass(int arg0, int arg1)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];
    if (Actors_actorRecords[arg0] != NULL) {
        temp_a0->bypassActorCollision = arg1;
        temp_a0->ignoredByCollision = arg1 >> 1;
    }
}

// decomp: BATTLE.PRG func_800A0A74 (src/BATTLE/BATTLE.PRG/30DB0.c:4003)
int Field_ActorCollisionBypass(int arg0)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];
    if (temp_a0 != NULL) {
        int var_v1 = temp_a0->bypassActorCollision;
        if ((temp_a0->ignoredByCollision << 11)) {
            var_v1 |= 2;
        }
        return var_v1;
    }
}

// decomp: BATTLE.PRG vs_battleRelativeSampleHeight (src/BATTLE/BATTLE.PRG/3A1A0.c:80)
int Field_RelativeCollisionSampleHeight(
    Field_CollisionSampleScratch* samples, int sector, int offset, int y)
{
    return samples->sampleHeights[(sector + offset) & 7] - y;
}

// decomp: BATTLE.PRG func_800A3500 (src/BATTLE/BATTLE.PRG/3A1A0.c:394)
int Field_ActorSupportHeight(int arg0, int arg1)
{
    int h;
    int i;
    Field_GimmickInstance* actor;

    h = Field_InterpolatedFloorHeight(arg0, arg1);
    h <<= 17;
    h >>= 17;

    for (i = 0; i < 16; ++i) {
        actor = Field_gimmickInstances[i];
        if (actor != NULL && actor->positionX == arg0 && actor->positionZ == arg1
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
int Field_RoomCeilingHeight(int arg0, int arg1)
{
    short temp_v0 = Field_CeilingHeightAt(arg0, arg1);
    if (temp_v0 == 0) {
        return 0;
    }
    return (temp_v0 << 0x11) >> 0x11;
}

// decomp: BATTLE.PRG func_800A3C34 (src/BATTLE/BATTLE.PRG/3A1A0.c:666)
Actors_ActorRecord* Field_FindActorOccupyingTile(int arg0, int arg1, int arg2, int arg3)
{
    int i;

    arg2 -= 192;

    if (arg3 < 2) {
        i = 0;
    } else {
        i = arg3;
    }

    for (; i < 17; ++i) {
        Actors_ActorRecord* actor = Actors_actorRecords[i];
        if ((actor == NULL) || actor->instance.isSkipped || (actor->modelId == 0x7F)) {
            continue;
        }
        if ((arg3 == 1)
            && (((actor->instance.poseState != 0) && (actor->instance.poseState < 3))
                || actor->instance.movementMode || actor->instance.reactionState)) {
            SVECTOR* pos = &actor->routeTarget;
            if (((pos->vx & 0xFF) == arg0) && (arg1 == (pos->vy & 0xFF))) {
                return actor;
            }
            continue;
        }
        if ((i == 0) && (*((int*)&actor->sequenceFlags - 2) & 0xF0000000)) {
            Field_GimmickInstance* temp_v1 = Field_gimmickInstances[actor->supportGimmick];
            if ((arg0 == temp_v1->tileX) && (arg1 == temp_v1->tileZ)) {
                return actor;
            }
            continue;
        }
        if ((actor->instance.locomotionState != 2) && (actor->instance.currentTileX == arg0)
            && (actor->instance.currentTileZ == arg1) && !actor->instance.motionLocked
            && (actor->instance.position.vy >= arg2)) {
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
Field_PlatformContactPoint* Field_FindMovingPlatformById(int arg0)
{
    Field_PlatformContactPoint sp10[4];
    int temp_v0;
    int i;
    arg0 -= 2;

    temp_v0 = Field_CollectMovingPlatformPositions(sp10);

    for (i = 0; i < temp_v0; ++i) {
        if (sp10[i].platformIndex == arg0) {
            // BUG: returns stack variable
            return &sp10[i];
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_800A51A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:1307)
int Field_IsPointOnMovingPlatform(int arg0, SVECTOR* arg1)
{
    Field_PlatformContactPoint sp10[4];
    Field_PlatformContactPoint* p;
    int n;
    int i;

    arg0 -= 2;
    n = Field_CollectMovingPlatformPositions(sp10);
    for (i = 0; i < n; i++) {
        if (sp10[i].platformIndex == arg0) {
            p = &sp10[i];
            goto found;
        }
    }
    p = NULL;
found:
    if (p->x + 0x40 < arg1->vx) {
        return 0;
    }
    if (arg1->vx < p->x - 0x40) {
        return 0;
    }
    if (p->z + 0x40 < arg1->vz) {
        return 0;
    }
    return !(arg1->vz < p->z - 0x40);
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
int Field_HasActorLedgeSideSupport(Actors_ActorRecord* arg0)
{
    SVECTOR v;
    int ret = 0;
    int h = Core_Gte_Sin(ONE / 8) * arg0->collisionRadius / ONE;

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
int Field_SampleActorSupportHeight(SVECTOR* arg0, int arg1, int arg2, int arg3)
{
    SVECTOR sp;
    u_int* ent;
    int h;
    int r;
    int los;

    int x = arg1 + arg0->vx;
    int z = arg2 + arg0->vz;

    sp.vx = x;
    sp.vz = z;

    ent = ((u_int* (*)(SVECTOR*, int))Field_FindCollisionCellAtPosition)(&sp, 0);

    if (ent != NULL) {

        Field_collisionProbeTile = ent;

        if (arg3 == 0) {
            h = Field_FloorHeightAt(x, z);
        } else {
            h = Field_InterpolatedFloorHeight(x, z);
        }

        if (h >= 0) {

            h <<= 17;
            h >>= 17;

            if (arg3 == 2) {
                r = Field_GimmickSupportHeightBelow(x / 128, z / 128, arg0->vy);
            } else {
                r = Field_GimmickSupportHeight(x / 128, z / 128);
            }

            if (r != 0) {
                h = r;
            }

            if (((*ent >> 17) & 1) && (h >= 0)) {

                h = 3000;

                if (arg3 == 3) {

                    los = Field_kTileSurfaceIds[*ent & 0x1F];

                    if (los == 0x14) {
                        h = 0;
                    }
                }
            }

            if (arg3 != 2 && arg3 != 4) {

                los = Field_MovingPlatformContactHeight(x, arg0->vy, z, 0);

                r = h;

                if ((los != 0) && (los < h)) {
                    h = los;
                }

                if (arg3 == 3 && h < arg0->vy - 64) {
                    h = r;
                }
            }

            return h;
        }
    }

    if (arg3 == 3) {
        return arg0->vy;
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
int Field_ClassifyActorTerrainProbe(SVECTOR* arg0, int arg1)
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
    int limit = sb->sampleHeights[arg1 & 7];

    if (actor->bypassActorCollision << 9) {
        return 0;
    }

    gx = arg0->vx / 128;
    gz = arg0->vz / 128;
    elev2 = Field_CeilingHeightAt(arg0->vx, arg0->vz);
    elev2 <<= 17;
    elev2 >>= 17;

    if (limit + sb->searchFloor < elev2) {
        return 0xFF;
    }

    ent = ((u_int* (*)(SVECTOR*, int))Field_FindCollisionCellAtPosition)(arg0, 0);

    if (ent == NULL) {
        return 0xFF;
    }

    h = Field_FloorHeightAt(arg0->vx, arg0->vz);

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

    los = Field_MovingPlatformContactHeight(arg0->vx, limit, arg0->vz, 0);

    if (los == 0) {
        arg0->pad = 0;
    } else {
        arg0->pad = Actors_supportingPlatformLayer;
        h = los;
    }

    arg0->pad |= Actors_supportingGimmickIndex << 8;

    if (sb->searchFloor < elev2 - h) {
        return 0xFF;
    }

    if (h < sb->searchCeiling + limit) {
        return 0xFF;
    }

    if (((int (*)(Field_GimmickInstance*, SVECTOR*))Field_IsActorPositionUnoccupied)(actor, arg0) == 0) {
        return 0xFF;
    }

    arg0->vy = h;
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
Field_MpdTileCell* Field_FindCollisionCellAtPosition(SVECTOR* arg0, int arg1)
{
    int temp_s1 = arg0->vx / 128;
    int temp_s0 = arg0->vz / 128;
    Field_MpdTileCell* temp_v0 = Field_CollisionCellAt(temp_s1, temp_s0, arg1);

    if (temp_v0 == NULL) {
        return NULL;
    }

    temp_s1 = arg0->vx - (temp_s1 << 7);
    temp_s0 = arg0->vz - (temp_s0 << 7);

    // BUG: potentially unhandled values
    switch (temp_v0->doorPassability) {
    case 0:
        return temp_v0;

    case 1:
        if ((temp_s1 + temp_s0) > 0x80) {
            return NULL;
        }
        return temp_v0;

    case 2:
        if ((temp_s1 - temp_s0) > 0) {
            return NULL;
        }
        return temp_v0;

    case 3:
        if ((temp_s1 + temp_s0) < 0x80) {
            return NULL;
        }
        return temp_v0;

    case 4:
        if ((temp_s1 - temp_s0) < 0) {
            return NULL;
        }
        return temp_v0;

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
int Field_GimmickSupportHeight(int arg0, int arg1)
{
    int var_t0;
    int i;

    var_t0 = 0xBB8;
    Actors_supportingGimmickIndex = 0;

    for (i = 0; i < 16; ++i) {

        Field_GimmickInstance* temp_v1 = Field_gimmickInstances[i];

        if (temp_v1 == NULL) {
            continue;
        }

        if (!temp_v1->isSkipped && (temp_v1->tileX == arg0) && (temp_v1->tileZ == arg1)
            && !temp_v1->motionState && (temp_v1->mode != 0xFE)
            && (temp_v1->positionY < var_t0)) {

            var_t0 = temp_v1->positionY;
            Actors_supportingGimmickIndex = i;
        }
    }

    var_t0 -= 128;

    if (var_t0 > 0) {
        var_t0 = 0;
    }

    return var_t0;
}

// decomp: BATTLE.PRG func_800A9378 (src/BATTLE/BATTLE.PRG/40564.c:377)
int Field_MovingPlatformContactHeight(int arg0, int arg1, int arg2, int arg3)
{
    Field_PlatformContactPoint sp10[4];
    int count;
    int i;

    (*(int (*))&Actors_supportingPlatformLayer) = 0;
    count = Field_CollectMovingPlatformPositions(sp10);

    if (count == 0) {
        return 0;
    }

    for (i = 0; i < count; ++i) {
        if ((arg0 >= sp10[i].x + 64) || (arg0 < sp10[i].x - 64)) {
            continue;
        }

        if ((arg2 >= sp10[i].z + 64) || (arg2 < sp10[i].z - 64)) {
            continue;
        }

        switch (arg3) {
        case 0:
            if (sp10[i].height < arg1 - 256) {
                continue;
            }
            break;

        case 1:
            if (sp10[i].height < arg1 - 256) {
                return sp10[i].height + 96;
            }
            continue;

        case 2:
            if ((sp10[i].height >= arg1) || (sp10[i].height < arg1 - 128)) {
                continue;
            }
            break;

        case 3:
            if ((sp10[i].height >= arg1) || (sp10[i].height < arg1 - Field_platformContactSearchDepth)) {
                continue;
            }
            return sp10[i].height + 96;
        }

        (*(int (*))&Actors_supportingPlatformLayer) = sp10[i].platformIndex + 2;
        return sp10[i].height;
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
int Field_GimmickSupportHeightBelow(int arg0, int arg1, int arg2)
{
    int i;
    int var_a3 = 0xBB8;

    for (i = 0; i < 16; ++i) {

        Field_GimmickInstance* temp_v1 = Field_gimmickInstances[i];

        if ((temp_v1 != NULL) && !(temp_v1->isSkipped) && !(temp_v1->objectInfo.isFalling)
            && (temp_v1->tileX == arg0) && (temp_v1->tileZ == arg1)) {

            int temp_v1_2 = temp_v1->positionY;

            if ((arg2 < temp_v1_2) && (temp_v1_2 < var_a3)) {
                var_a3 = temp_v1_2;
            }
        }
    }

    var_a3 -= 128;

    if (var_a3 > 0) {
        var_a3 = 0;
    }

    return var_a3;
}

// decomp: BATTLE.PRG func_800E42D4 (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Field_GroundHeightAtWorldPosition(int, int)
{
    VS_UNPORTED("Field::GroundHeightAtWorldPosition");
    return 0;
}

// decomp: BATTLE.PRG func_800E42E0 (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Field_GroundHeightAtPackedTile(unsigned int)
{
    VS_UNPORTED("Field::GroundHeightAtPackedTile");
    return 0;
}

// decomp: BATTLE.PRG func_800E42EC (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Field_GroundHeightAtTile(int, int)
{
    VS_UNPORTED("Field::GroundHeightAtTile");
    return 0;
}

// decomp: BATTLE.PRG func_800E4308 (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Field_GroundHeightFlagsAtPackedTile()
{
    VS_UNPORTED("Field::GroundHeightFlagsAtPackedTile");
    return 0;
}

// decomp: BATTLE.PRG func_800E4318 (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Field_GroundHeightFlagsAtWorldPosition()
{
    VS_UNPORTED("Field::GroundHeightFlagsAtWorldPosition");
    return 0;
}

// decomp: BATTLE.PRG func_800E4320 (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Field_GroundHeightFlagsAtTile(int, int)
{
    VS_UNPORTED("Field::GroundHeightFlagsAtTile");
    return 0;
}

// decomp: BATTLE.PRG func_800E4478 (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Field_IsPositionAboveGround()
{
    VS_UNPORTED("Field::IsPositionAboveGround");
    return 0;
}

// decomp: BATTLE.PRG func_800E44E8 (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Field_IsTerrainSegmentBlocked(SVECTOR*, SVECTOR*, int, int)
{
    VS_UNPORTED("Field::IsTerrainSegmentBlocked");
    return 0;
}

// decomp: BATTLE.PRG func_800E6BA0 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
int Field_IsPointOutsideSolidVolumes()
{
    VS_UNPORTED("Field::IsPointOutsideSolidVolumes");
    return 0;
}

// decomp: BATTLE.PRG func_800E6C34 (src/BATTLE/BATTLE.PRG/7E05C.s:0) [asm-file: not yet ported]
int Field_TraceActorAndTerrainCollision(SVECTOR*, SVECTOR*, SVECTOR*, int)
{
    VS_UNPORTED("Field::TraceActorAndTerrainCollision");
    return 0;
}

