// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/ActorSound.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Effects/Weather.h"
#include "Field/Actors.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/GimmickMotion.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Room.h"
#include "Field/RoomEffects.h"
#include "Field/RoomRendering.h"
#include "Field/TriggerMarkers.h"
#include "Field/Zone.h"
#include "Items/Loot.h"
#include "Items/TreasureChests.h"
#include "Render/Display.h"
#include "Render/ModelDraw.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/RoomDraw.h"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"
#include "State/Timers.h"

// decomp: D_800E856C (D_800E856C@BATTLE/BATTLE.PRG)
MATRIX Field_triggerMarkerMatrix; /* contents: module image (ModuleData.c) */

// decomp: D_800E8594 (D_800E8594@BATTLE/BATTLE.PRG)
Field_TriggerMarkerVertex Field_triggerMarkerVertex; /* contents: module image (ModuleData.c) */

// decomp: D_800E85A0 (D_800E85A0@BATTLE/BATTLE.PRG)
char Field_triggerMarkerRecordTail[192]; /* contents: module image (ModuleData.c) */

// decomp: D_800E8FA8 (D_800E8FA8@BATTLE/BATTLE.PRG)
u_char Field_gimmickPaletteParameters[32]; /* contents: module image (ModuleData.c) */

// decomp: D_800E8FB4 (D_800E8FB4@BATTLE/BATTLE.PRG)
void* Field_gimmickModelTemplates[5]; /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_800E8FD0 (D_800E8FD0@BATTLE/BATTLE.PRG)
u_char Field_kGimmickRollOrientations[3424]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9030 (D_800E9030@BATTLE/BATTLE.PRG)
u_char Field_kGimmickOrientationRotations[3328]; /* contents: module image (ModuleData.c) */

// decomp: D_800E90A8 (D_800E90A8@BATTLE/BATTLE.PRG)
int Field_interactingGimmickId; /* contents: module image (ModuleData.c) */

// decomp: D_800E919C (D_800E919C@BATTLE/BATTLE.PRG)
u_char Field_gimmickTemplateIndices[2964]; /* contents: module image (ModuleData.c) */

// decomp: D_800E91AC (D_800E91AC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSpinHeightSteps[2948]; /* contents: module image (ModuleData.c) */

// decomp: D_800E91BC (D_800E91BC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSpinPitchSteps[2932]; /* contents: module image (ModuleData.c) */

// decomp: D_800E91CC (D_800E91CC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSpinYawSteps[2916]; /* contents: module image (ModuleData.c) */

// decomp: D_800E91DC (D_800E91DC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSpinRollSteps[2900]; /* contents: module image (ModuleData.c) */

// decomp: D_800E91EC (D_800E91EC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickBounceHeightSteps[2884]; /* contents: module image (ModuleData.c) */

// decomp: D_800E91F0 (D_800E91F0@BATTLE/BATTLE.PRG)
u_char Field_kGimmickTiltDepthSteps[2880]; /* contents: module image (ModuleData.c) */

// decomp: D_800E91FC (D_800E91FC@BATTLE/BATTLE.PRG)
u_char Field_kGimmickTiltAngleSteps[2868]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9208 (D_800E9208@BATTLE/BATTLE.PRG)
u_char Field_kGimmickRollHeightSteps[2856]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9218 (D_800E9218@BATTLE/BATTLE.PRG)
u_char Field_kGimmickRollDisplacementSteps[2840]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9228 (D_800E9228@BATTLE/BATTLE.PRG)
u_char Field_kGimmickRollAngleSteps[2824]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9238 (D_800E9238@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSquashHeightSteps[2808]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9248 (D_800E9248@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSquashScaleX[2792]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9258 (D_800E9258@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSquashScaleY[2776]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9268 (D_800E9268@BATTLE/BATTLE.PRG)
u_char Field_kGimmickSquashScaleZ[2760]; /* contents: module image (ModuleData.c) */

// decomp: vs_battle_roomData (vs_battle_roomData@BATTLE/BATTLE.PRG)
Field_RoomRecord Field_roomData;

// decomp: D_800F1CDC (D_800F1CDC@BATTLE/BATTLE.PRG)
u_short Field_roomAnimationTicks;

// decomp: D_800F1D28 (D_800F1D28@BATTLE/BATTLE.PRG)
Field_GimmickAnimationJob Field_gimmickAnimationQueue[8];

// decomp: D_800F1D68 (D_800F1D68@BATTLE/BATTLE.PRG)
short Field_platformFreezeFrames;

// decomp: D_800F1D6A (D_800F1D6A@BATTLE/BATTLE.PRG)
short Field_pendingGimmickAnimationCount;

// decomp: D_800F1D6C (D_800F1D6C@BATTLE/BATTLE.PRG)
u_short Field_triggerMarkerFrameTicks;

// decomp: D_800F1D6E (D_800F1D6E@BATTLE/BATTLE.PRG)
u_char Field_areMovingPlatformsPaused;

// decomp: D_800F1D6F (D_800F1D6F@BATTLE/BATTLE.PRG)
char Field_isPlatformFreezeTintActive;

// decomp: D_800F1D70 (D_800F1D70@BATTLE/BATTLE.PRG)
u_char Field_platformBounceFrames[1304];

// decomp: D_800F4448 (D_800F4448@BATTLE/BATTLE.PRG)
char Field_gimmickPaletteColors[240];

// decomp: D_800F45E0 (D_800F45E0@BATTLE/BATTLE.PRG)
Field_GimmickInstance* Field_gimmickInstances[16];

// decomp: D_800F49E8 (D_800F49E8@BATTLE/BATTLE.PRG)
int Field_gimmickInteractionDirection;

// decomp: D_80068BEC (D_80068BEC@src/BATTLE/BATTLE.PRG/146C.c)
short const Field_kPlatformBounceOffsets[] = { 8, 14, 16, 11, 7, 3, 1 };

// decomp: BATTLE.PRG func_8006CDD8 (src/BATTLE/BATTLE.PRG/146C.c:2151)
void Field_DisableTrapAtPosition(SVECTOR* arg0)
{
    Field_MpdFloorTrigger* section = Field_RoomTriggerAtTile(arg0->vx / 128, arg0->vz / 128, 0);

    if (section != NULL) {
        Field_SetRoomTriggerEnabled(section, 1);
        Battle_Ai_MarkAiTileDynamicObstacle(section->tileX, section->tileZ);
    }
}

// decomp: BATTLE.PRG func_8006CE50 (src/BATTLE/BATTLE.PRG/146C.c:2161)
void Field_FreezeRoomGimmicksTemporarily(void) { Field_FreezeMovingPlatforms(0xB4); }

// decomp: BATTLE.PRG func_8008A4FC (src/BATTLE/BATTLE.PRG/146C.c:15530)
int Field_CurrentSavePointId(void)
{
    Actors_ActorLocationQuery sp10;
    u_int temp_a2;
    u_int temp_v0_2;
    u_int var_a2;
    int var_a0;
    int var_a1;
    Actors_TileTarget* s0;

    Actors_ActorPlacement(0, &sp10);

    temp_v0_2 = Actors_ActorStateFlags(0);
    temp_a2 = temp_v0_2 & 0x70000;
    s0 = &sp10.location.packedTile.target;

    if (temp_a2 != 0) {
        var_a0 = 0;
        var_a1 = 0;
        var_a2 = temp_a2 >> 0x10;
    } else {
        var_a0 = s0->tileX;
        var_a1 = s0->tileZ;
        var_a2 = s0->tileLayer;
    }

    if ((Field_CollisionCellAt(var_a0, var_a1, var_a2)->hasRoomTrigger & 1)
        && (!(temp_v0_2 & 0x504000))) {
        Field_MpdFloorTrigger* room = Field_RoomTriggerAtTile(s0->tileX, s0->tileZ, s0->tileLayer);
        if ((room != NULL) && (room->kind == 16)) {
            return room->activation;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008B6B4 (src/BATTLE/BATTLE.PRG/146C.c:16052)
void Field_ResetGimmickAnimationQueue(void)
{
    Field_pendingGimmickAnimationCount = 0;
    Field_triggerMarkerFrameTicks = 0;
    Core_Memory_ZeroAlignedWords(Field_gimmickAnimationQueue, sizeof Field_gimmickAnimationQueue);
}

// decomp: BATTLE.PRG func_8008C070 (src/BATTLE/BATTLE.PRG/146C.c:16338)
void Field_QueueGimmickAnimation(int arg0, Field_MpdLockTrigger* arg1)
{
    int i;

    ++Field_pendingGimmickAnimationCount;

    for (i = 0; i < 8; ++i) {
        if (Field_gimmickAnimationQueue[i].jobKind == 0) {
            Field_gimmickAnimationQueue[i].jobKind = arg0;
            Field_gimmickAnimationQueue[i].trigger = arg1;
            return;
        }
    }
}

// decomp: BATTLE.PRG func_8008C0C0 (src/BATTLE/BATTLE.PRG/146C.c:16353)
void Field_UpdateGimmickAnimationQueue(void)
{
    Field_MpdLockTrigger* temp_a0;
    int i;

    if (Field_pendingGimmickAnimationCount <= 0) {
        return;
    }

    for (i = 0; i < 8; ++i) {
        if (Field_gimmickAnimationQueue[i].jobKind == 1) {
            if (Field_FinishDoorAnimation(Field_gimmickAnimationQueue[i].trigger) != 0) {
                Field_gimmickAnimationQueue[i].jobKind = 0;
                Field_gimmickAnimationQueue[i].trigger = NULL;
                --Field_pendingGimmickAnimationCount;
            }
        } else if (Field_gimmickAnimationQueue[i].jobKind == 2) {
            temp_a0 = Field_gimmickAnimationQueue[i].trigger;
            if ((--temp_a0->trigger.activation << 0x18) <= 0) {
                Field_ToggleRoomSwitch(temp_a0);
                Field_gimmickAnimationQueue[i].jobKind = 0;
                Field_gimmickAnimationQueue[i].trigger = NULL;
                --Field_pendingGimmickAnimationCount;
            }
        } else if (Field_gimmickAnimationQueue[i].jobKind == 3) {
            temp_a0 = Field_gimmickAnimationQueue[i].trigger;
            if (!(--temp_a0->delayFrames)) {
                Field_StartDoorAnimation(temp_a0);
                Field_gimmickAnimationQueue[i].jobKind = 0;
                Field_gimmickAnimationQueue[i].trigger = NULL;
                --Field_pendingGimmickAnimationCount;
            }
        }
    }
}

// decomp: BATTLE.PRG func_8008C49C (src/BATTLE/BATTLE.PRG/146C.c:16475)
int Field_SetMovingPlatformActor(int arg0, int arg1)
{
    if (Field_roomData.movingPlatforms != NULL) {
        Field_roomData.movingPlatforms[arg0].controlState = arg1;
        if ((arg1 >= 0) && (Field_platformFreezeFrames == 0)) {
            Field_platformBounceFrames[arg0] = 0;
        }
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_8008C518 (src/BATTLE/BATTLE.PRG/146C.c:16487)
void Field_ClearMovingPlatformActor(int arg0) { Field_SetMovingPlatformActor(arg0, -1); }

// decomp: BATTLE.PRG func_8008C538 (src/BATTLE/BATTLE.PRG/146C.c:16489)
void Field_InitializeMovingPlatforms(void)
{
    Field_MpdMovingPlatform* var_s2;
    int temp_s3;
    int i;
    int* temp_v0;
    u_int temp_a0;

    Field_areMovingPlatformsPaused = 0;
    Field_platformFreezeFrames = 0;
    Field_isPlatformFreezeTintActive = 0;

    for (i = 0; i < 4; ++i) {
        Field_platformBounceFrames[i] = 7;
    }

    if (Field_roomData.movingPlatforms != NULL) {
        temp_s3 = Field_roomData.header.movingPlatformsLength / 184;
        var_s2 = Field_roomData.movingPlatforms;
        for (i = 0; i < temp_s3; ++i, ++var_s2) {
            temp_v0 = ((int* (*)(int))Field_FindRoomGeometryGroup)(var_s2->sceneObjectId);
            if (temp_v0 != NULL) {
                if (var_s2->isHidden) {
                    temp_v0[0] |= 0x100;
                }

                temp_v0[1] = var_s2->positionX / ONE;
                temp_v0[2] = -(var_s2->positionY / ONE);
                temp_v0[3] = var_s2->positionZ / ONE;
            }
        }
    }

    temp_a0 = Field_entranceDoorPlatformFlags >> 0xD;
    if (temp_a0 >= 2) {
        Field_ResetMovingPlatformWaypoint(temp_a0 - 2, -1);
    }
}

// decomp: BATTLE.PRG func_8008C6B4 (src/BATTLE/BATTLE.PRG/146C.c:16528)
void Field_ResetMovingPlatformWaypoint(int arg0, int arg1)
{
    Field_MpdMovingPlatform* temp_s0;
    int* temp_v0_5;

    if (Field_roomData.movingPlatforms != NULL) {
        temp_s0 = &Field_roomData.movingPlatforms[arg0];
        if (arg1 == -1) {
            arg1 = temp_s0->terminalWaypoint;
        }
        if (arg1 >= 0) {
            temp_s0->isHidden = 0;
            temp_s0->controlState = 0;
            temp_s0->currentWaypoint = arg1;
            temp_s0->startWaypoint = arg1;
            temp_s0->currentSegment = arg1;
            temp_s0->motionTicks = 0;
            temp_s0->isWaiting = 0;

            temp_s0->positionX = temp_s0->waypoints[temp_s0->currentWaypoint].vx;
            temp_s0->positionY = temp_s0->waypoints[temp_s0->currentWaypoint].vy;
            temp_s0->positionZ = temp_s0->waypoints[temp_s0->currentWaypoint].vz;

            if (((temp_s0->startWaypoint)) == ((temp_s0->endWaypoint))) {
                temp_s0->isMovingForward = 0;
                --temp_s0->currentWaypoint;
            } else {
                temp_s0->isMovingForward = 1;
                ++temp_s0->currentWaypoint;
            }
            temp_v0_5 = ((int* (*)(int))Field_FindRoomGeometryGroup)(temp_s0->sceneObjectId);
            if (temp_v0_5 != NULL) {
                temp_v0_5[1] = temp_s0->positionX / ONE;
                temp_v0_5[2] = -(temp_s0->positionY / ONE);
                temp_v0_5[3] = temp_s0->positionZ / ONE;
            }
        }
    }
}

// decomp: BATTLE.PRG func_8008C8A8 (src/BATTLE/BATTLE.PRG/146C.c:16570)
int Field_UpdateMovingPlatforms(void)
{
    SVECTOR point;
    Field_MpdMovingPlatform* object;
    int count;
    int i;
    int tick;
    int* scene;
    u_int temp;

    if (Field_areRoomUpdatesPaused || Field_roomData.movingPlatforms == NULL || Field_areMovingPlatformsPaused) {
        return;
    }
    if (Field_platformFreezeFrames > 0) {
        Field_platformFreezeFrames -= State_tickSpeed >> 1;

        if (Field_platformFreezeFrames < 30 && Render_frameBuf) {
            Field_SetPlatformFreezeAppearance(Field_isPlatformFreezeTintActive == 0);
        }
        if (Field_platformFreezeFrames <= 0) {
            Field_SetPlatformFreezeAppearance(0);
            Field_platformFreezeFrames = 0;
        }

        return;
    }

    object = Field_roomData.movingPlatforms;
    count = Field_roomData.header.movingPlatformsLength / sizeof *object;

    for (i = 0; i < count; ++i, ++object) {
        if (!object->isHidden) {
            if (!object->isLooping || (*(u_int*)((char*)object + 4) & 0xFE) != 0xFE) {
                for (tick = 0; tick < (State_tickSpeed >> 1); ++tick) {
                    temp = Field_platformBounceFrames[i];

                    if (temp < 7) {
                        Field_platformBounceFrames[i] = temp + (State_tickSpeed >> 1);

                        if (Field_platformBounceFrames[i] >= 8) {
                            Field_platformBounceFrames[i] = 7;
                        }
                    }
                    if (!object->isWaiting) {
                        object->positionX += object->segmentVelocities[object->currentSegment].vx;
                        object->positionY += object->segmentVelocities[object->currentSegment].vy;
                        object->positionZ += object->segmentVelocities[object->currentSegment].vz;
                        point.vy = -(object->positionY / ONE);
                        point.vx = object->positionX / ONE;
                        point.vz = object->positionZ / ONE;

                        if (Field_CheckPlatformActorObstruction(i + 2, &point)) {
                            object->positionX -= object->segmentVelocities[object->currentSegment].vx;
                            object->positionY -= object->segmentVelocities[object->currentSegment].vy;
                            object->positionZ -= object->segmentVelocities[object->currentSegment].vz;
                        } else {
                            ++object->motionTicks;

                            if (object->motionTicks >= object->segmentVelocities[object->currentSegment].pad) {
                                object->motionTicks = 0;
                                object->isWaiting = 1;
                                object->positionX = object->waypoints[object->currentWaypoint].vx;
                                object->positionY = object->waypoints[object->currentWaypoint].vy;
                                object->positionZ = object->waypoints[object->currentWaypoint].vz;

                                if (object->currentWaypoint == object->terminalWaypoint) {
                                    object->tileCell.isPlatformDocked = 1;
                                }
                            }
                        }
                    } else {
                        ++object->motionTicks;

                        if (object->motionTicks >= object->waypoints[object->currentWaypoint].pad) {
                            object->motionTicks = 0;
                            object->isWaiting = 0;
                            object->startWaypoint = object->currentWaypoint;
                            ++object->currentSegment;
                            object->tileCell.isPlatformDocked = 0;

                            if (object->startWaypoint == 0) {
                                object->isMovingForward = 1;
                                object->currentSegment = 0;
                            } else if (object->startWaypoint == object->endWaypoint) {
                                object->isMovingForward = 0;
                            }
                            if (object->isMovingForward) {
                                ++object->currentWaypoint;
                            } else {
                                --object->currentWaypoint;
                            }
                        }
                    }
                }
            }

            scene = ((int* (*)(int))Field_FindRoomGeometryGroup)(object->sceneObjectId);

            if (scene != NULL) {
                int y;

                scene[1] = object->positionX / ONE;
                y = object->positionY / ONE;
                temp = Field_kPlatformBounceOffsets[Field_platformBounceFrames[i]] - y;
                scene[2] = temp;
                scene[3] = object->positionZ / ONE;
                scene[0] &= ~0x100;
            }
        } else {
            scene = ((int* (*)(int))Field_FindRoomGeometryGroup)(object->sceneObjectId);

            if (scene != NULL) {
                scene[0] |= 0x100;
            }
        }
    }
}

// decomp: BATTLE.PRG func_8008CEB0 (src/BATTLE/BATTLE.PRG/146C.c:16688)
void Field_DrawMovingPlatformShadows(void)
{
    Field_MpdMovingPlatform* object;
    int i;
    int count;

    Core_Gte_SetRotMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    Core_Gte_SetTransMatrix(&((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->viewMatrix);
    if (Field_roomData.movingPlatforms == NULL) {
        return;
    }
    object = Field_roomData.movingPlatforms;
    count = Field_roomData.header.movingPlatformsLength / sizeof(Field_MpdMovingPlatform);
    for (i = 0; i < count; ++i, ++object) {
        int _[8] __attribute__((unused));
        SVECTOR vertices[4];
        long projection;
        long flags;
        POLY_F4* polygon;
        int depth;
        int shade;
        Field_MpdTileCell* cell;
        int height;
        int coordinate;
        int y;

        if (object->isHidden || ((int* (*)(int))Field_FindRoomGeometryGroup)(object->sceneObjectId) == NULL) {
            continue;
        }
        cell = Field_CollisionCellAt(object->positionX / (ONE * 128), object->positionZ / (ONE * 128), 0);
        if (cell->isPit && (cell->surfaceKind < 15 || cell->surfaceKind >= 19)) {
            continue;
        }
        coordinate = object->positionX / ONE;
        height = Field_FloorHeightAt(coordinate, object->positionZ / ONE);
        height <<= 17;
        height >>= 17;
        coordinate = object->positionY;
        y = (coordinate /= -ONE);
        if (height <= y) {
            continue;
        }
        shade = (height - y) / 2;
        if (shade > 223) {
            shade = 223;
        }
        shade = 255 - shade;
        polygon = ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor;
        vertices[0].vx = (short)(object->positionX / ONE - 32) - shade / 8;
        vertices[0].vy = height;
        vertices[0].vz = (short)(object->positionZ / ONE - 32) - shade / 8;
        vertices[1].vx = (short)(object->positionX / ONE + 32) + shade / 8;
        vertices[1].vy = height;
        vertices[1].vz = (short)(object->positionZ / ONE - 32) - shade / 8;
        vertices[2].vx = (short)(object->positionX / ONE - 32) - shade / 8;
        vertices[2].vy = height;
        vertices[2].vz = (short)(object->positionZ / ONE + 32) + shade / 8;
        vertices[3].vx = (short)(object->positionX / ONE + 32) + shade / 8;
        vertices[3].vy = height;
        vertices[3].vz = (short)(object->positionZ / ONE + 32) + shade / 8;
        depth = Core_Gte_RotAverage4(&vertices[0], &vertices[1], &vertices[2], &vertices[3],
            (long*)&polygon->x0, (long*)&polygon->x1, (long*)&polygon->x2,
            (long*)&polygon->x3, &projection, &flags);
        if (depth <= 4 || depth >= 2048) {
            continue;
        }
        setPolyF4(polygon);
        setSemiTrans(polygon, 1);
        setRGB0(polygon, shade / 2 + 16, shade / 2 + 16, shade / 2 + 16);
        AddPrim((u_long*)((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->orderingTable + depth, polygon++);
        ((Render_FrameScratch*)((uintptr_t)Core_ScratchBase() + 0x000))->primitiveCursor = polygon;
        _insertTPage(depth, getTPage(0, 2, 0, 0));
    }
}

// decomp: BATTLE.PRG func_8008D2C0 (src/BATTLE/BATTLE.PRG/146C.c:16763)
int Field_CollectMovingPlatformPositions(Field_PlatformContactPoint arg0[])
{
    Field_MpdMovingPlatform* var_t0;
    int i;
    int var_t1;
    int temp_t3;

    Field_PlatformContactPoint* a0 = arg0;

    do {
        if (Field_roomData.movingPlatforms != NULL) {
            var_t0 = Field_roomData.movingPlatforms;
            temp_t3 = Field_roomData.header.movingPlatformsLength / 184;
            var_t1 = 0;

            for (i = 0; i < temp_t3; ++i, ++var_t0) {
                if (!((var_t0->isHidden))) {
                    do {
                        int v;
                        a0->x = var_t0->positionX / ONE;
                        v = var_t0->positionY / ONE;
                        a0->height = (Field_kPlatformBounceOffsets[Field_platformBounceFrames[i]] - v);
                        a0->z = (var_t0->positionZ / ONE);
                        a0->platformIndex = i;
                    } while (0);
                    ++a0;
                    ++var_t1;
                }
            }
            return var_t1;
        }
    } while (0);
    return 0;
}

// decomp: BATTLE.PRG func_8008D438 (src/BATTLE/BATTLE.PRG/146C.c:16810)
Field_MpdFloorTrigger* Field_RoomTriggerAtTile(int arg0, int arg1, int arg2)
{
    if (Field_roomData.floorTriggers != NULL) {
        Field_MpdFloorTrigger* var_t0 = Field_roomData.floorTriggers;
        int temp_t1 =
            Field_roomData.header.floorTriggersLength / sizeof *Field_roomData.floorTriggers;
        int i;
        for (i = 0; i < temp_t1; ++i, ++var_t0) {
            if ((var_t0->tileX == arg0) && (var_t0->tileZ == arg1)
                && (var_t0->floorLayer == arg2)) {
                if (((Audio_Seq_unusedStateFlags.opaque20 != 0) || (Audio_Seq_unusedStateFlags.opaque40 != 0))
                    && (var_t0->kind < 16)) {
                    return NULL;
                } else {
                    return var_t0;
                }
            }
        }
    }
    return NULL;
}

// decomp: BATTLE.PRG func_8008D594 (src/BATTLE/BATTLE.PRG/146C.c:16855)
void Field_SetMovingPlatformsPaused(int arg0) { Field_areMovingPlatformsPaused = arg0; }

// decomp: BATTLE.PRG func_8008D5A0 (src/BATTLE/BATTLE.PRG/146C.c:16857)
void Field_FreezeMovingPlatforms(int arg0)
{

    if ((Field_roomData.header.movingPlatformsLength / 184) != 0) {
        if (arg0 != 0) {
            Field_SetPlatformFreezeAppearance(1);
        } else {
            Field_SetPlatformFreezeAppearance(0);
        }
        Field_platformFreezeFrames = arg0;
    }
}

// decomp: BATTLE.PRG func_8008D5FC (src/BATTLE/BATTLE.PRG/146C.c:16870)
void Field_BeginSwitchToggle(Field_MpdLockTrigger* arg0)
{
    if ((signed char)arg0->trigger.markerFade == 0) {
        arg0->trigger.markerFade = 1;
        if (arg0->trigger.lockState.switchValue == 0) {
            arg0->trigger.activation = 0x14;
            Field_QueueGimmickAnimation(2, arg0);
            return;
        }
        Field_ToggleRoomSwitch(arg0);
    }
}

// decomp: BATTLE.PRG func_8008D658 (src/BATTLE/BATTLE.PRG/146C.c:16883)
void Field_ToggleRoomSwitch(Field_MpdLockTrigger* arg0)
{
    Field_MpdTileCell* temp_v0;

    if (arg0->trigger.lockState.switchValue == 0) {
        Audio_PlaySfxCentered(0x7E, 0x3B);
        Audio_PlaySfxCentered(0x7E, 0x3C);
    } else {
        Audio_PlaySfxCentered(0x7E, 0x3D);
        Audio_PlaySfxCentered(0x7E, 0x3E);
    }
    Field_StartRoomEffect(arg0->trigger.kind);
    arg0->trigger.markerFade = 0;
    arg0->trigger.lockState.switchValue = arg0->trigger.lockState.switchValue == 0;
    temp_v0 = Field_CollisionCellAt(arg0->trigger.tileX, arg0->trigger.tileZ, arg0->trigger.floorLayer);
    if (temp_v0 != NULL) {
        temp_v0->doorPassability = arg0->trigger.lockState.switchValue * 5;
    }
}

// decomp: BATTLE.PRG func_8008DEAC (src/BATTLE/BATTLE.PRG/146C.c:17147)
void Field_SetRoomTriggerEnabled(Field_MpdFloorTrigger* arg0, int arg1)
{
    if (arg0 == NULL) {
        Field_MpdFloorTrigger* var_a2 = Field_roomData.floorTriggers;
        if (var_a2 != NULL) {
            int i;
            int temp_a3 = Field_roomData.header.floorTriggersLength / 12;
            for (i = 0; i < temp_a3; ++i) {
                var_a2->activation = arg1;
                ++var_a2;
            }
        }
    } else {
        arg0->activation = arg1;
    }
}

// decomp: BATTLE.PRG func_8008DF14 (src/BATTLE/BATTLE.PRG/146C.c:17164)
void Field_DrawRoomTriggerMarkers(void)
{
    Field_MpdFloorTrigger* var_s3;
    int count;
    int j;
    int var_s2;
    int i;

    if (Field_roomData.floorTriggers != NULL) {
        var_s3 = Field_roomData.floorTriggers;
        var_s2 = 1;
        count =
            Field_roomData.header.floorTriggersLength / sizeof *Field_roomData.floorTriggers;

        for (i = 0; i < count; ++i, ++var_s3) {
            if ((Audio_Seq_unusedStateFlags.opaque20 == 0) && (Audio_Seq_unusedStateFlags.opaque40 == 0)
                && (var_s3->activation != 0)) {
                for (j = 0; j < (State_tickSpeed * 2); ++j) {
                    if ((signed char)var_s3->markerFade >= 0) {
                        ++var_s3->markerFade;
                    }
                }
            } else {
                for (j = 0; j < State_tickSpeed; ++j) {
                    if (var_s3->markerFade != 0) {
                        --var_s3->markerFade;
                    }
                }
            }

            if ((var_s3->markerFade != 0) && (var_s3->kind < 0x10)) {
                if (var_s2 != 0) {
                    Core_Memory_CopyAlignedWords(&Field_triggerMarkerMatrix, (void*)((uintptr_t)Core_ScratchBase() + 0x014), sizeof Field_triggerMarkerMatrix);
                    Field_triggerMarkerFrameTicks += State_tickSpeed;
                    if (Field_triggerMarkerFrameTicks >= 8) {
                        Field_triggerMarkerFrameTicks -= 8;
                        var_s2 = (Field_roomAnimationTicks >> 3) % 5;
                        Field_triggerMarkerTextureSource.x = (var_s2 * 8) + 0x48;
                        Field_triggerMarkerPaletteSource.x = (var_s2 * 4) + 4;
                        MoveImage(&Field_triggerMarkerTextureSource, 0x40, 0x180);
                        MoveImage(&Field_triggerMarkerPaletteSource, 0, 0x140);
                    }
                    var_s2 = 0;
                }
                j = Field_FloorHeightAt(var_s3->tileX << 7, var_s3->tileZ << 7) << 0x11;
                Field_DrawRoomTriggerMarker(
                    var_s3->tileX << 7, var_s3->tileZ << 7, j >> 0x11, var_s3->markerFade);
            }
        }
    }
}

// decomp: BATTLE.PRG func_8008E19C (src/BATTLE/BATTLE.PRG/146C.c:17216)
void Field_DrawRoomTriggerMarker(int arg0, int arg1, short arg2, u_int arg3)
{
    int i;
    char (*var_t0)[4];
    void* p;

    Field_triggerMarkerVertex.positionX = arg0 + 0x80;
    Field_triggerMarkerVertex.positionZ = arg1 + 0x80;
    Field_triggerMarkerVertex.height = arg2;

    var_t0 = Field_triggerMarkerVertex.vertexShades;
    for (i = 0; i < 3;) {
        ++i;
        (*var_t0)[0] = arg3;
        (*var_t0)[1] = arg3;
        (*var_t0)[2] = arg3;
        ++var_t0;
    }
    var_t0 += 3;

    (*var_t0)[0] = arg3;
    (*var_t0)[1] = arg3;
    (*var_t0)[2] = arg3;
    p = Field_triggerMarkerRecordTail;
    Render_DrawRoomGeometry(p - 0x58, Field_roomGeometryDrawMode);
}

// decomp: BATTLE.PRG func_8008E224 (src/BATTLE/BATTLE.PRG/146C.c:17243)
void Field_SetMovingPlatformVisibility(int arg0, int arg1)
{
    int* temp_v0;
    Field_MpdMovingPlatform* temp_s0 = Field_roomData.movingPlatforms;

    if (temp_s0 != NULL) {
        temp_s0 = &Field_roomData.movingPlatforms[arg0];
        temp_s0->isHidden = arg1 - 1;
        temp_v0 = ((int* (*)(int))Field_FindRoomGeometryGroup)(temp_s0->sceneObjectId);
        if (temp_v0 != NULL) {
            if (!temp_s0->isHidden) {
                *temp_v0 &= ~0x100;
            } else {
                *temp_v0 |= 0x100;
            }
        }
    }
}

// decomp: BATTLE.PRG func_8008E2D4 (src/BATTLE/BATTLE.PRG/146C.c:17262)
void* Field_MovingPlatformTargetWaypoint(int arg0)
{
    Field_MpdMovingPlatform* temp_v1 = Field_roomData.movingPlatforms;

    if (Field_roomData.movingPlatforms != NULL) {
        temp_v1 = &Field_roomData.movingPlatforms[arg0];
        return ((char*)temp_v1) + 8 + temp_v1->currentWaypoint * 16;
    }
    return NULL;
}

// decomp: BATTLE.PRG func_8008E320 (src/BATTLE/BATTLE.PRG/146C.c:17273)
u_int Field_MovingPlatformDwellTicks(int arg0)
{
    Field_MpdMovingPlatform* temp_v1 = Field_roomData.movingPlatforms;
    if (Field_roomData.movingPlatforms != NULL) {
        temp_v1 = &Field_roomData.movingPlatforms[arg0];
        if (temp_v1->isWaiting == 1) {
            return temp_v1->motionTicks;
        }
    }
    return -1;
}

// decomp: BATTLE.PRG func_8008E400 (src/BATTLE/BATTLE.PRG/146C.c:17307)
Field_MpdFloorTrigger* Field_ActiveRoomTriggers(int* arg0)
{
    if ((Audio_Seq_unusedStateFlags.opaque20 != 0) || (Audio_Seq_unusedStateFlags.opaque40 != 0)) {
        *arg0 = 0;
        return NULL;
    }
    if (Field_roomData.floorTriggers != NULL) {
        *arg0 = Field_roomData.header.floorTriggersLength / 12;
        return Field_roomData.floorTriggers;
    }
    *arg0 = 0;
    return NULL;
}

// decomp: BATTLE.PRG func_8008E4DC (src/BATTLE/BATTLE.PRG/146C.c:17333)
void Field_SetPlatformFreezeAppearance(int arg0)
{
    static u_short const Field_platformFreezePalette[0x10] = { 0x10, 1, 0x865, 0xC66, 0x869, 0xCA9, 0x88D,
        0x10AD, 0xCB1, 0x14D1, 0xCD5, 0x14F5, 0xCF9, 0x113A, 0x199B, 0x21FC };

    Field_MpdMovingPlatform* var_s0;
    int temp_s2;
    int i;

    if (Field_isPlatformFreezeTintActive != arg0) {
        if (arg0 != 0) {
            Render_LoadClutColors(Field_platformFreezePalette, 2, 0x30, 0x10);
        } else {
            Render_LoadClutColors(Field_roomPalettes + 0x230, 2, 0x30, 0x10);
        }

        var_s0 = Field_roomData.movingPlatforms;
        temp_s2 = Field_roomData.header.movingPlatformsLength / 184;

        for (i = 0; i < temp_s2; ++i) {
            if (var_s0->swapsColorsWhenFrozen) {
                Field_SwapRoomGeometryRedGreen(var_s0->sceneObjectId);
            }
            ++var_s0;
        }
        if (Field_roomData.environment != NULL) {
            if (Field_roomData.environment->freezeEffectId >= 0) {
                Field_ToggleRoomEffectUpdates(Field_roomData.environment->freezeEffectId);
            }
        }
        Field_isPlatformFreezeTintActive = !Field_isPlatformFreezeTintActive;
    }
}

// decomp: BATTLE.PRG func_80099900 (src/BATTLE/BATTLE.PRG/30DB0.c:273)
int Field_FindSharedGimmickModel(Content_ActorAssetRequest* arg0)
{
    int i;

    for (i = 0; i < 16; ++i) {
        if ((i != arg0->targetSlot)
            && ((Field_gimmickInstances[i] != NULL)
                && (Field_gimmickInstances[i]->objectInfo.objectId == arg0->subKind))) {
            arg0->dataAddress = i;
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_80099960 (src/BATTLE/BATTLE.PRG/30DB0.c:288)
void Field_SetGimmickPaletteTable(void* arg0) { Core_Memory_CopyAlignedWords(&Field_gimmickPaletteColors, arg0, 0xE0); }

// decomp: BATTLE.PRG func_8009998C (src/BATTLE/BATTLE.PRG/30DB0.c:290)
int Field_CreateGimmickInstance(Content_ActorAssetRequest* arg0)
{
    Field_GimmickInstance* temp_a0;
    int temp_s0;
    int j;
    int var_v0_2;
    u_short* var_s0;
    int i;
    Field_GimmickInstance* s1;
    Field_GimmickTemplate* temp_v1_2;

    if (Field_gimmickInstances[arg0->targetSlot] != NULL) {
        return -3;
    }

    s1 = Field_gimmickInstances[arg0->targetSlot] = arg0->destination;

    if (s1 == NULL) {
        return -2;
    }

    memset(s1, 0, offsetof(Field_GimmickInstance, reserved178));

    for (i = 0; i < 16; ++i) {
        if ((i != arg0->targetSlot) && (Field_gimmickInstances[i] != NULL)
            && (Field_gimmickInstances[i]->objectInfo.objectId == arg0->subKind)) {
            arg0->dataAddress = i;
            var_v0_2 = 1;
            goto exit;
        }
    }
    var_v0_2 = 0;
exit:
    if (var_v0_2 != 0) {
        Core_Memory_CopyAlignedWords(s1, Field_gimmickInstances[arg0->dataAddress], offsetof(Field_GimmickInstance, reserved178));
    } else {
        temp_v1_2 = Field_gimmickModelTemplates[Field_gimmickTemplateIndices[arg0->subKind]];
        *(int*)&s1->boneCount = temp_v1_2->instanceFlags;
        s1->reserved4 = temp_v1_2->instanceWord4;
        temp_s0 = temp_v1_2->recolorCount;
        s1->model = Content_CopyModelGeometry(NULL, temp_v1_2 + 1, temp_v1_2->modelDataSize);

        if (s1->model == NULL) {
            return -2;
        }

        temp_v1_2 = VS_ONDISC(s1->model, polygonSection, void *);
        j = arg0->subKind;

        if (j >= 6) {
            j = 5;
        }

        Content_OffsetModelTextureCoordinates(temp_v1_2, temp_s0, 1, Field_gimmickPaletteParameters[j * 2], Field_gimmickPaletteParameters[j * 2 + 1]);
    }

    var_s0 = s1->basePalette;

    s1->slot = arg0->targetSlot;
    s1->objectInfo.objectId = arg0->subKind;

    Core_Memory_CopyAlignedWords(var_s0, (s1->objectInfo.objectId * 32) + Field_gimmickPaletteColors, sizeof s1->basePalette);
    Render_LoadClutColors(var_s0, 0xD, s1->slot * 0x10, 0x10);

    s1->objectInfo.carrier = 0xFF;
    s1->parent = 0xFF;

    if (s1->objectInfo.objectId == 1) {
        s1->objectInfo.subtype = arg0->textureSlot;
    }

    s1->baseColor = 0x808080;
    s1->scaleX = 0x40;
    s1->scaleY = 0x40;
    s1->scaleZ = 0x40;
    s1->mode = 0;
    s1->palettePulse.kind = 0;
    s1->isSkipped = 1;
    s1->isVisible = 0;
    *(int*)&s1->tileX = *(int*)&arg0->placement;
    s1->reserved5F = 0;
    s1->positionX = (s1->tileX << 7) + 0x40;
    s1->positionZ = (s1->tileZ << 7) + 0x40;
    s1->positionY = ((int (*)(short*, int, int, int))Field_SampleActorSupportHeight)(&s1->positionX, 0, 0, 1) - (arg0->variant << 7);
    Field_SetGimmickOrientation(arg0->targetSlot, arg0->material);
    Battle_Ai_AddAiTileOccupant(s1);
    s1->objectInfo.interaction = 0;

    if (s1->objectInfo.objectId > 4) {

        for (j = 0; j < arg0->targetSlot; ++j) {
            temp_a0 = Field_gimmickInstances[j];
            if ((temp_a0 != NULL) && (*(int*)&temp_a0->tileX == *(int*)&s1->tileX)
                && (temp_a0->positionY == (s1->positionY + 0x80))
                && (temp_a0->objectInfo.objectId > 4)) {
                if (temp_a0->objectInfo.objectId != s1->objectInfo.objectId) {
                    s1->objectInfo.interaction = 1;
                } else {
                    s1->objectInfo.interaction = 2;
                    s1->positionY = s1->positionY - 0x80;
                    Battle_Ai_AddAiTileOccupant(s1);
                }
                break;
            }
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_80099D6C (src/BATTLE/BATTLE.PRG/30DB0.c:399)
int Field_ReleaseGimmickInstance(int arg0)
{
    Content_ActorAssetRequest sp10;
    Field_GimmickInstance* temp_s0;
    int i;
    int found;

    if (arg0 >= 16) {
        return -1;
    }

    temp_s0 = Field_gimmickInstances[arg0];

    if (temp_s0 == NULL) {
        return -1;
    }

    if (temp_s0->boneCount == 0) {
        return -1;
    }

    sp10.targetSlot = arg0;
    sp10.subKind = temp_s0->objectInfo.objectId;

    for (i = 0; i < 16; ++i) {
        Content_ActorAssetRequest* a2 = &sp10;
        if ((i != a2->targetSlot) && (Field_gimmickInstances[i] != NULL)
            && (Field_gimmickInstances[i]->objectInfo.objectId == a2->subKind)) {
            a2->dataAddress = i;
            found = 1;
            goto exit;
        }
    }

    found = 0;

exit:
    if (!found) {
        Core_Memory_FreeFromLowEnd(temp_s0->model);
    }

    Battle_Ai_RemoveAiTileOccupant(temp_s0);
    Field_gimmickInstances[arg0] = NULL;
    return 0;
}

// decomp: BATTLE.PRG func_8009E634 (src/BATTLE/BATTLE.PRG/30DB0.c:2695)
void Field_RefreshAshleyInteractionTile(Actors_ActorRecord* arg0)
{
    int temp_v1_2;
    int var_a0;
    int var_a2;
    int var_a3;

    var_a0 = 1;

    if (arg0->instance.slot == 0) {
        if ((arg0->animationId - 0x4F) >= 4U) {
            if (((arg0->animationId - 0x47) >= 4U) && (arg0->animationId != 0x4C)) {
                return;
            }
        } else {
            var_a0 = 2;
        }

        temp_v1_2 = arg0->instance.facing + 0x200;
        temp_v1_2 &= 0xFFF;
        temp_v1_2 /= 1024;

        var_a2 = 0;
        var_a3 = 0;

        if (temp_v1_2 < 2) {
            var_a0 = -var_a0;
        }

        if (temp_v1_2 & 1) {
            var_a2 = var_a0;
        } else {
            var_a3 = var_a0;
        }
        ((void (*)(int, int, int, int))Battle_Ai_ReleaseAiDestinationTile)(arg0->instance.currentTileX + var_a2, arg0->instance.currentTileZ + var_a3,
            var_a2, var_a3);
    }
}

// decomp: BATTLE.PRG func_8009F530 (src/BATTLE/BATTLE.PRG/30DB0.c:3256)
void Field_DropUnsupportedGimmickStack(int arg0)
{
    Actors_ActorRecord* temp_v0_2;
    int temp_s4;
    int temp_v0;
    int i;
    Field_GimmickInstance* temp_s0 = Field_gimmickInstances[arg0];

    temp_s0->objectInfo.isFalling = 1;
    temp_s4 = Field_FindTopGimmickAtTile(temp_s0->tileX, temp_s0->tileZ, 0);
    temp_v0 = Field_FindGimmickBelow(temp_s0);

    if (temp_v0 != -1) {
        Field_GimmickInstance* temp_s1 = Field_gimmickInstances[temp_v0];
        temp_s1->velocityY = 0;
        temp_s1->mode = 0xFC;
        Battle_Ai_RemoveAiTileOccupant(temp_s1);

        if (temp_s1->objectInfo.interaction == 2) {
            Battle_Ai_RemoveAiTileOccupant(temp_s1);
        }

        for (i = 0; i < 16; ++i) {
            temp_s0 = Field_gimmickInstances[i];
            if ((temp_s0 != NULL) && (temp_s0 != temp_s1) && !(temp_s0->isSkipped)) {
                temp_s0->objectInfo.carrier = 0xFF;
                if ((temp_s0->mode == 0) && !(temp_s0->objectInfo.isFalling)
                    && (temp_s0->parent == 0xFF) && (temp_s0->tileX == temp_s1->tileX)
                    && (temp_s0->tileLayer == temp_s1->tileLayer)
                    && (temp_s0->tileZ == temp_s1->tileZ)
                    && (temp_s0->positionY <= temp_s1->positionY)) {
                    temp_s0->objectInfo.carrier = temp_v0;
                }
            }
        }
    }

    if (Field_gimmickInstances[temp_s4] == NULL) {
        Core_Debugf("gimmick", "no eligible stack top while removing slot=%d", arg0);
        return;
    }

    i = 0;

    while ((temp_v0_2 = Field_FindActorAboveGimmick(Field_gimmickInstances[temp_s4], i))) {

        if ((temp_v0_2->instance.movementMode) != 3) {
            *(int*)&temp_v0_2->instance.rootMotion = 0;
            *(int*)&temp_v0_2->instance.rootMotion.vz = 0;
            temp_v0_2->instance.movementMode = 3;
            if (temp_v0_2->instance.slot != 0) {
                temp_v0_2->destinationMode = 1;
                temp_v0_2->destinationX = temp_v0_2->instance.currentTileX;
                temp_v0_2->destinationZ = temp_v0_2->instance.currentTileZ;
            }
            temp_v0_2->instance.jumpCountdown = 4;
        }

        i = temp_v0_2->instance.slot + 1;
        temp_v0_2->instance.locomotionState = 0;
        temp_v0_2->instance.movementPending = 0;
        temp_v0_2->supportGimmick = 0;

        if (i == 1) {
            i = 2;
        }
    }
}

// decomp: BATTLE.PRG func_8009F794 (src/BATTLE/BATTLE.PRG/30DB0.c:3320)
int Field_FindGimmickBelow(Field_GimmickInstance* arg0)
{
    Field_GimmickInstance* temp_v1;
    int temp_v1_2;
    int var_a3;
    int i;
    int var_t0;
    int t1;

    var_a3 = -0x7530;
    var_t0 = -1;
    t1 = *(int*)&arg0->tileX;

    for (i = 0; i < 16; ++i) {
        temp_v1 = Field_gimmickInstances[i];
        if ((temp_v1 != NULL) && !(temp_v1->isSkipped) && (temp_v1->mode == 0)
            && !(temp_v1->objectInfo.isFalling) && (temp_v1->parent == 0xFF)
            && (t1 == *(int*)&temp_v1->tileX)) {
            temp_v1_2 = temp_v1->positionY;
            if ((arg0->positionY >= temp_v1_2) && (var_a3 < temp_v1_2)) {
                var_a3 = temp_v1_2;
                var_t0 = i;
            }
        }
    }
    return var_t0;
}

// decomp: BATTLE.PRG func_8009F858 (src/BATTLE/BATTLE.PRG/30DB0.c:3348)
int Field_GimmickSubtype(int arg0)
{
    Field_GimmickInstance* temp_a0 = Field_gimmickInstances[arg0];

    if (temp_a0 == NULL) {
        return -1;
    }

    if (temp_a0->objectInfo.objectId != 1) {
        return -1;
    }

    return temp_a0->objectInfo.subtype;
}

// decomp: BATTLE.PRG func_800A0024 (src/BATTLE/BATTLE.PRG/30DB0.c:3654)
int Field_GimmickPlacement(int arg0, Field_GimmickSnapshot* arg1)
{
    Field_GimmickInstance* temp_s0;
    int var_v0_2;

    // BUG: Check performed on garbage
    if (temp_s0 == 0) {
        return -1;
    }

    temp_s0 = Field_gimmickInstances[arg0];

    if (temp_s0->parent != 0xFF) {
        return -1;
    }

    if ((temp_s0->objectInfo.isFalling)) {
        return -1;
    }

    if (temp_s0->mode != 0) {
        return -1;
    }

    *(int*)&arg1->placementByte0 = *(int*)&temp_s0->tileX;
    arg1->orientation = temp_s0->objectInfo.material;
    var_v0_2 = ((int (*)(short*, int, int, int))Field_SampleActorSupportHeight)(&temp_s0->positionX, 0, 0, 0) - temp_s0->positionY;
    if (var_v0_2 < 0) {
        var_v0_2 += 0x7F;
    }
    arg1->heightAboveGround = var_v0_2 >> 7;
    arg1->positionXY = *(int*)&temp_s0->positionX;
    arg1->positionZ = *(int*)&temp_s0->positionZ;
    return 0;
}

// decomp: BATTLE.PRG func_800A0104 (src/BATTLE/BATTLE.PRG/30DB0.c:3690)
int Field_SetGimmickOrientation(int index, int material)
{
    Field_GimmickInstance* obj = Field_gimmickInstances[index];

    if ((obj == NULL) || (obj->objectInfo.objectId >= 2)) {
        return -1;
    }

    obj->objectInfo.material = material;
    material *= 4;
    ((Render_ModelScratchpad*)((uintptr_t)Core_ScratchBase() + 0x35C))->rotation.vx = Field_kGimmickOrientationRotations[material] * (ONE / 4);
    ((Render_ModelScratchpad*)((uintptr_t)Core_ScratchBase() + 0x35C))->rotation.vy = Field_kGimmickOrientationRotations[material + 1] * (ONE / 4);
    ((Render_ModelScratchpad*)((uintptr_t)Core_ScratchBase() + 0x35C))->rotation.vz = Field_kGimmickOrientationRotations[material + 2] * (ONE / 4);
    Core_Gte_RotMatrixYXZ(&((Render_ModelScratchpad*)((uintptr_t)Core_ScratchBase() + 0x35C))->rotation, &obj->matrices[4]);
    obj->rotationTicks = 0;
    *(int*)&obj->rotationX = 0;
    obj->rotationZ = 0;
    return 0;
}

// decomp: BATTLE.PRG func_800A38E0 (src/BATTLE/BATTLE.PRG/3A1A0.c:522)
int Field_FindInteractableGimmickAhead(int actorId)
{
    Actors_ActorRecord* actor = Actors_actorRecords[actorId];
    Field_GimmickInstance* object;
    int direction;
    int x;
    int z;
    int farX;
    int farZ;
    int objectId;
    int found;
    int oldX;
    int oldZ;

    if (actor == NULL) {
        return -1;
    }

    if (actor->routePhase != 0) {
        return -1;
    }

    if (actor->instance.movementMode != 0) {
        return -1;
    }

    if (actor->instance.locomotionState != 0) {
        return -1;
    }

    direction = actor->instance.facing + ONE / 8;
    direction &= 0xFFF;
    direction /= ONE / 4;
    Field_gimmickInteractionDirection = direction;
    oldX = farX = x = actor->instance.currentTileX;
    oldZ = farZ = z = actor->instance.currentTileZ;
    found = direction < 2 ? -1 : 1;

    if (direction & 1) {
        x += found;
        farX = oldX + found * 2;
    } else {
        z += found;
        farZ = oldZ + found * 2;
    }

    if (x < 0 || z < 0) {
        return -1;
    }

    if (actor->actionPoseActive) {
        x = x * 128 + 64;
        objectId = actor->instance.attachBone;
        object = Field_gimmickInstances[objectId];
        z = z * 128 + 64;

        if (object->mode != 0) {
            return -1;
        }

        if (!Field_CanPlaceGimmickAtPoint(x, z, actor->instance.position.vy - 128)) {
            return -1;
        }

        direction = actor->instance.position.vy - 384;

        if (direction < Field_RoomCeilingHeight(actor->instance.position.vx, actor->instance.position.vz)) {
            return -1;
        }

        if (direction < Field_RoomCeilingHeight(x, z)) {
            return -1;
        }

        return objectId;
    }

    found = Field_FindTopGimmickAtTile(x, z, 0);

    if (found == 0) {
        return -1;
    }

    object = Field_gimmickInstances[found];
    objectId = found;

    if (object->objectInfo.interaction == 1) {
        return -1;
    }

    if (object->mode != 0) {
        return -1;
    }

    x = x * 128 + 64;
    z = z * 128 + 64;
    farX = farX * 128 + 64;
    farZ = farZ * 128 + 64;

    if (object->positionY < actor->instance.position.vy - 128) {
        return -1;
    }

    if (actor->instance.position.vy + 64 < object->positionY) {
        return -1;
    }

    if (object->objectInfo.objectId < 4) {
        if (!Field_CanPlaceGimmickAtPoint(farX, farZ, object->positionY)) {
            return -1;
        }
    } else {
        direction = actor->instance.position.vy - 384;

        if (direction < Field_RoomCeilingHeight(actor->instance.position.vx, actor->instance.position.vz)) {
            return -1;
        }

        if (direction < Field_RoomCeilingHeight(x, z)) {
            return -1;
        }
    }

    if (Field_FindActorAboveGimmick(object, 1) == 0) {
        return objectId;
    }

    return -1;
}

// decomp: BATTLE.PRG func_800A3C00 (src/BATTLE/BATTLE.PRG/3A1A0.c:661)
Actors_ActorRecord* Field_FindActorAboveGimmick(Field_GimmickInstance* arg0, u_int arg1)
{
    return Field_FindActorOccupyingTile(arg0->tileX, arg0->tileZ, arg0->positionY, arg1);
}

// decomp: BATTLE.PRG func_800A3DB4 (src/BATTLE/BATTLE.PRG/3A1A0.c:708)
int Field_CanPlaceGimmickAtPoint(int x, int z, int minimumHeight)
{
    SVECTOR origin;
    int height;

    *(int*)&origin.vx = 0;
    *(int*)&origin.vz = 0;
    height = Field_SampleActorSupportHeight(&origin, x, z, 1);

    if (height < minimumHeight) {
        return 0;
    }

    if ((*Field_collisionProbeTile >> 20) & 1) {
        return 0;
    }

    return Field_FindActorOccupyingTile(x / 128, z / 128, height, 1) == NULL;
}

// decomp: BATTLE.PRG func_800A3E6C (src/BATTLE/BATTLE.PRG/3A1A0.c:728)
int Field_BeginGimmickInteraction(int actorId)
{
    SVECTOR position;
    struct {
        u_char tileX;
        u_char tileY;
        u_char tileZ;
        u_char direction;
    } tile;
    Actors_ActorRecord* actor;
    Field_GimmickInstance* object;
    int objectId;
    int height;
    int step;
    int dx;
    int dz;

    actor = Actors_actorRecords[actorId];
    if (actor->weaponPoseActive) {
        return -1;
    }
    objectId = Field_FindInteractableGimmickAhead(actorId);
    if (objectId == -1) {
        return -1;
    }
    object = Field_gimmickInstances[objectId];
    object->orientation = Field_gimmickInteractionDirection;
    Field_interactingGimmickId = objectId;
    height = actor->instance.position.vy - object->positionY;
    height /= 64;
    height++;

    object->slideX = 0;
    object->slideZ = 0;
    switch (object->objectInfo.objectId) {
    case 0:
    case 1:
        object->objectInfo.material = Field_kGimmickRollOrientations[object->objectInfo.material * 4 + Field_gimmickInteractionDirection];
        // fallthrough
    case 2:
        step = 0x80;
        if (Field_gimmickInteractionDirection < 2) {
            step = -0x80;
        }
        if (Field_gimmickInteractionDirection & 1) {
            object->slideX = step;
        } else {
            object->slideZ = step;
        }
        goto move;
    case 3:
        step = 0x20;
        object->slideX = 0;
        object->slideZ = 0;
        if (Field_gimmickInteractionDirection < 2) {
            step = -0x20;
        }
        if (Field_gimmickInteractionDirection & 1) {
            object->slideX = step;
        } else {
            object->slideZ = step;
        }
    move:
        object->velocityY = 0;
        ((void (*)())Audio_PlayActorSoundPair)(actor, 0x2C, 0x2D);
        Actors_PlayActorAnimation(actorId, height + 0x4F, 0, 4);
        actor->weaponPoseActive = 1;
        Battle_Ai_ReserveAiDestinationTile(
            (object->positionX + object->slideX) / 128, (object->positionZ + object->slideZ) / 128);
        break;
    case 4:
    case 5:
    case 6:
        if (actor->actionPoseActive) {
            Actors_PlayActorAnimation(actorId, 0x4C, 0, 4);
            ((void (*)())Audio_PlayActorSoundPair)(actor, 0x2E, 0x2F);
            actor->weaponPoseActive = 1;
        } else {
            Actors_PlayActorAnimation(actorId, height + 0x47, 0, 4);
            ((void (*)())Audio_PlayActorSoundPair)(actor, 0x2C, 0x2D);
            actor->weaponPoseActive = 1;
        }
        dx = 0;
        dz = 0;
        step = 1;
        if (Field_gimmickInteractionDirection < 2) {
            step = -1;
        }

        if (Field_gimmickInteractionDirection & 1) {
            dx = step;
        } else {
            dz = step;
        }
        Battle_Ai_ReserveAiDestinationTile(actor->instance.currentTileX + dx, actor->instance.currentTileZ + dz);
        break;
    }
    step = 1;
    *(int*)&tile = *(int*)&actor->instance.currentTileX;
    if (Field_gimmickInteractionDirection < 2) {
        step = -1;
    }
    if (Field_gimmickInteractionDirection & 1) {
        tile.tileX += step;
    } else {
        tile.tileZ += step;
    }
    ((void (*)())Actors_TurnActorTowardTileOverTime)(actorId, (Actors_TileTarget*)&tile, 4);
    position.vx = (actor->instance.currentTileX << 7) + 0x40;
    position.vz = (actor->instance.currentTileZ << 7) + 0x40;
    position.vy = actor->instance.position.vy;
    ((void (*)())Actors_TranslateActorToOverTime)(actorId, &position, 4);
}

// decomp: BATTLE.PRG func_800A41D0 (src/BATTLE/BATTLE.PRG/3A1A0.c:842)
void Field_ApplyGimmickInteractionCommand(Actors_ActorRecord* actor, int mode)
{
    Field_GimmickInstance* object = Field_gimmickInstances[Field_interactingGimmickId];
    int direction;
    int step;
    int facing;

    if (mode == 2) {
        switch (object->objectInfo.objectId) {
        case 0:
        case 1:
            object->mode = 0xF9;
            break;

        case 2:
            object->mode = 0xFF;
            break;

        case 3:
            object->mode = 0xFE;
            break;
        }

        object->objectInfo.supportIndex = 0;
        Battle_Ai_RemoveAiTileOccupant(object);
        return;
    }

    if (mode == 1) {
        SVECTOR points[2];
        int x = actor->instance.currentTileX;
        int z = actor->instance.currentTileZ;
        step = Field_gimmickInteractionDirection < 2 ? -1 : 1;

        if (Field_gimmickInteractionDirection & 1) {
            x += step;
        } else {
            z += step;
        }

        step = Field_gimmickInteractionDirection;
        object->tileX = x;
        object->tileZ = z;
        object->positionX = x * 128 + 64;
        object->positionZ = z * 128 + 64;
        object->rotationY = (object->rotationY + step * (ONE / 4)) & 0xFFF;
        Actors_ActorTaggedBonePosition(object->parent, 0xF0, &points[0], 1);
        Actors_ActorTaggedBonePosition(object->parent, 0xF1, &points[1], 1);
        object->positionY = (points[0].vy + points[1].vy) / 2 + 60;
        object->mode = 0xFC;
        object->velocityY = 24;
        object->parent = 0xFF;
        actor->actionPoseActive = 0;
        return;
    }

    if (object == NULL || object->motionState) {
        Actors_PlayActorAnimation(actor->instance.slot, 1, 0, 8);
        return;
    }

    Battle_Ai_ReleaseAiDestinationTile(object->tileX, object->tileZ);
    Battle_Ai_RemoveAiTileOccupant(object);

    if (object->objectInfo.interaction == 2) {
        Battle_Ai_RemoveAiTileOccupant(object);
    }

    step = Field_gimmickInteractionDirection < 2 ? 128 : -128;

    if (Field_gimmickInteractionDirection & 1) {
        object->slideX += step;
    } else {
        object->slideZ += step;
    }

    facing = (object->rotationY + (4 - Field_gimmickInteractionDirection) * (ONE / 4)) & 0xFFF;
    direction = facing + ONE / 8;
    direction &= 0xFFF;
    object->rotationY = facing;
    direction /= ONE / 4;
    object->rotationY = direction * (ONE / 4);
    object->tileX = -1;
    object->mode = 8;
    object->parent = actor->instance.slot;
    actor->instance.attachBone = Field_interactingGimmickId;
    actor->actionPoseActive = 1;
}

// decomp: BATTLE.PRG func_800A47C4 (src/BATTLE/BATTLE.PRG/3A1A0.c:1022)
int Field_AreGimmicksMoving(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        Field_GimmickInstance* temp_v1 = Field_gimmickInstances[i];
        if ((Field_gimmickInstances[i] != NULL) && !Field_gimmickInstances[i]->isSkipped && !temp_v1->motionState
            && temp_v1->mode) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800A91DC (src/BATTLE/BATTLE.PRG/40564.c:317)
int Field_FindTopGimmickAtTile(int arg0, int arg1, int arg2)
{
    int _[2] __attribute__((unused));
    int i;

    int var_t2 = ONE;
    int var_t3 = 0;

    for (i = 0; i < 16; ++i) {
        Field_GimmickInstance* temp_v1 = Field_gimmickInstances[i];
        if ((temp_v1 != NULL) && !(temp_v1->isSkipped)
            && ((arg2 == 0) || (temp_v1->mode == 0)) && (temp_v1->parent == 0xFF)
            && (temp_v1->objectInfo.carrier == temp_v1->parent) && (temp_v1->tileX == arg0)
            && (temp_v1->tileZ == arg1) && (var_t2 >= temp_v1->positionY)) {
            var_t2 = temp_v1->positionY;
            var_t3 = i;
        }
    }

    if (var_t3 == 0) {
        return 0;
    }

    return var_t3;
}

// decomp: BATTLE.PRG func_800B0908 (src/BATTLE/BATTLE.PRG/44F14.c:1717)
void Field_UpdateGimmickMotion(Field_GimmickPhysicsView* object, int unused)
{
    Render_ModelScratchpadAnglesView* scratch = (void*)((uintptr_t)Core_ScratchBase() + 0x35C);
    int step, mode, other;
    Field_GimmickPhysicsView* child;
    if (object->delay) {
        --object->delay;
        return;
    }
    if (object->rotationTicks) {
        step = object->rotationRemainingX / object->rotationTicks;
        object->rotationRemainingX -= step;
        object->rotationX = (object->rotationX + step) & 0xFFF;
        step = object->rotationRemainingY / object->rotationTicks;
        object->rotationRemainingY -= step;
        object->rotationY = (object->rotationY + step) & 0xFFF;
        step = object->rotationRemainingZ / object->rotationTicks;
        object->rotationRemainingZ -= step;
        object->rotationZ = (object->rotationZ + step) & 0xFFF;
        --object->rotationTicks;
    }
    mode = object->mode;
    if (!mode) {
        return;
    }
    if (mode == 0xFF) {
        if (!object->slideX && !object->slideZ) {
            object->frame = 0;
            goto fall;
        }
        object->frame = 1;
        step = (short)(u_short)object->slideX / 2;
        object->slideX -= step;
        object->position.vx += step;
        if (object->slideX < 0) {
            if (object->slideX >= -1) {
                object->position.vx += object->slideX;
                object->slideX = 0;
            }
        } else if (object->slideX < 2) {
            object->position.vx += object->slideX;
            object->slideX = 0;
        }
        step = (short)(u_short)object->slideZ / 2;
        object->slideZ -= step;
        object->position.vz += step;
        if (object->slideZ < 0) {
            if (object->slideZ >= -1) {
                object->position.vz += object->slideZ;
                object->slideZ = 0;
            }
        } else if (object->slideZ < 2) {
            object->position.vz += object->slideZ;
            object->slideZ = 0;
        }
        return;
    } else if (mode == 0xFE) {
        if ((object->position.vx & 0x7F) == 0x40
            && (object->position.vz & 0x7F) == 0x40) {
            scratch->rotation.vx = object->position.vx;
            scratch->rotation.vz = object->position.vz;
            step = object->orientation;
            other = 128;
            if (step < 2) {
                other = -128;
            }
            if (step & 1) {
                scratch->rotation.vx += other;
            } else {
                scratch->rotation.vz += other;
            }
            if (Field_CanPlaceGimmickAtPoint(
                    scratch->rotation.vx, scratch->rotation.vz, object->position.vy)
                && Field_SampleActorSupportHeight(&object->position, 0, 0, 1) == object->position.vy) {
                object->tileX = 0xFF;
                object->tileZ = 0xFF;
            } else {
                goto fall;
            }
        }
        object->position.vx += object->slideX;
        object->position.vz += object->slideZ;
        return;
    } else if (mode == 0xFD) {
        if (!((int (*)())Field_UpdateGimmickReactionAnimation)(object)) {
            Battle_Ai_AddAiTileOccupant(object);
            Battle_Ai_AddAiTileOccupant(object);
            Battle_Ai_ReleaseAiDestinationTile(object->tileX, object->tileZ);
        }
        return;
    } else if (mode == 0xFC) {
    fall:
        object->tileX = object->position.vx / 128;
        object->tileZ = object->position.vz / 128;
        step = Field_SampleActorSupportHeight(&object->position, 0, 0, 2);
        if (object->position.vy < step) {
            object->velocityY += 3;
            object->position.vy += object->velocityY;
            if (object->position.vy < step) {
                object->mode = 0xFC;
                goto children;
            }
        }
        {
            int previousY = (u_short)object->position.vy;
            object->position.vy = step;
            object->velocityY += step - previousY;
        }
        other = object->material & 7;
        if (other >= 5) {
            *(int*)&object->material &= ~0x30;
            other = Field_FindTopGimmickAtTile(object->tileX, object->tileZ, 1);
            if (other) {
                other = *(u_char*)((char*)Field_gimmickInstances[other] + 0x16C) & 7;
                if (other >= 5) {
                    if (other == (object->material & 7)) {
                        object->mode = 0xFD;
                        object->frame = 0;
                        *(int*)&object->material =
                            (*(int*)&object->material & ~0x30) | 0x20;
                        Audio_PlayWorldSfx(0x7E, 0x30, &object->position);
                    } else {
                        *(int*)&object->material =
                            (*(int*)&object->material & ~0x30) | 0x10;
                        Audio_PlayWorldSfx(0x7E, 0x31, &object->position);
                        object->mode = 0;
                        Battle_Ai_AddAiTileOccupant(object);
                        Battle_Ai_ReleaseAiDestinationTile(object->tileX, object->tileZ);
                    }
                    goto children;
                }
            }
        }
        object->tileX = object->position.vx / 128;
        object->tileZ = object->position.vz / 128;
        Battle_Ai_AddAiTileOccupant(object);
        Battle_Ai_ReleaseAiDestinationTile(object->tileX, object->tileZ);
        if ((*(int*)&object->material & 7) == 1) {
            --object->durability;
            if (!object->durability) {
                object->mode = 0xF8;
                object->frame = 0;
                return;
            }
        }
        step = object->mode;
        if (step == 0xFE) {
            Audio_PlayWorldSfx(0x7E, 0x30, &object->position);
            object->mode = 0xFB;
            object->frame = 0;
        } else if (step == 0xFF || step == 0xF9) {
            object->mode = 0;
        } else {
            Audio_PlayWorldSfx(0x7E, 0x30, &object->position);
            object->mode = 0xFA;
            object->frame = 0;
        }
    children:
        step = 0;
        do {
            child = (void*)Field_gimmickInstances[step];
            if (child && !(child->flags & 1) && child->carrier == object->id) {
                child->position.vy += object->velocityY;
                if (!object->mode) {
                    child->carrier = 0xFF;
                }
            }
            ++step;
        } while (step < 16);
        return;
    } else if (mode == 0xFB) {
        goto animate;
    } else if (mode == 0xFA) {
        ((int (*)())Field_UpdateGimmickReactionAnimation)(object);
        goto children;
    } else if (mode == 0xF9) {
        if (((int (*)())Field_UpdateGimmickReactionAnimation)(object)) {
            return;
        }
        goto fall;
    } else if (mode == 0xF7) {
        goto animate;
    } else if (mode == 0xF8) {
    animate:
        ((int (*)())Field_UpdateGimmickReactionAnimation)(object);
        return;
    }
    step = object->slideX / object->mode;
    object->slideX -= step;
    object->position.vx += step;
    step = object->velocityY / object->mode;
    object->velocityY -= step;
    object->position.vy += step;
    step = object->slideZ / object->mode;
    object->slideZ -= step;
    object->position.vz += step;
    --object->mode;
    if (!object->mode && object->parent == 0xFF) {
        goto fall;
    }
}

// decomp: BATTLE.PRG func_800B101C (src/BATTLE/BATTLE.PRG/44F14.c:1919)
int Field_UpdateGimmickReactionAnimation(Field_GimmickMotionView* object)
{
    int frame = object->frame;
    int value;
    int value2;
    if (frame == 0) {
        frame = 1;
    }
    value = object->mode;
    if (value == 0xFD) {
        object->y += (signed char)Field_kGimmickSpinHeightSteps[frame];
        object->rotationX += (signed char)Field_kGimmickSpinPitchSteps[frame];
        object->rotationY += (signed char)Field_kGimmickSpinYawSteps[frame];
        object->rotationZ += (signed char)Field_kGimmickSpinRollSteps[frame];
        if (frame == 14) {
            goto complete;
        }
        goto next;
    } else if (value == 0xFB) {
        value = object->orientation;
        object->z += (signed char)Field_kGimmickTiltDepthSteps[frame];
        value2 = (signed char)Field_kGimmickTiltAngleSteps[frame];
        if (value == 0 || value == 3) {
            value2 = -value2;
        }
        if (value & 1) {
            object->rotationZ += value2;
        } else {
            object->rotationX += value2;
        }
        if (frame == 9) {
            goto complete;
        }
        goto next;
    } else if (value == 0xFA) {
        object->y += (signed char)Field_kGimmickBounceHeightSteps[frame];
        object->deltaY = (signed char)Field_kGimmickBounceHeightSteps[frame];
        if (frame == 3) {
            goto complete;
        }
        goto next;
    } else if (value == 0xF9) {
        int orientation;
        object->y += (signed char)Field_kGimmickRollHeightSteps[frame];
        value = (signed char)Field_kGimmickRollDisplacementSteps[frame];
        orientation = object->orientation;
        value2 = Field_kGimmickRollAngleSteps[frame];
        if (orientation >= 2) {
            value = -value;
        }
        if ((u_int)(orientation - 1) < 2) {
            value2 = -value2;
        }
        if (orientation & 1) {
            object->x += value;
            object->rotationZ += value2;
        } else {
            object->z += value;
            object->rotationX += value2;
        }
        if (frame == 14) {
            Field_SetGimmickOrientation(object->objectId, object->material);
            object->mode = 0;
            goto done;
        }
        goto next;
    } else if (value == 0xF8) {
        if (frame >= 9) {
            object->y += 3;
            if (frame == 9) {
                Core_Memory_CopyAlignedWords(object->state, object->savedState, 32);
                object->flags = (object->flags & ~0xF00) | 0xC00;
            }
            if (frame == 40) {
                Field_ReleaseGimmickInstance(object->objectId);
                object->mode = 0;
                goto done;
            }
        }
        goto next;
    } else if (value == 0xF7) {
        object->y += (signed char)Field_kGimmickSquashHeightSteps[frame];
        object->scaleX = (signed char)Field_kGimmickSquashScaleX[frame];
        object->scaleY = (signed char)Field_kGimmickSquashScaleY[frame];
        object->scaleZ = (signed char)Field_kGimmickSquashScaleZ[frame];
        if (frame == 14) {
            goto complete;
        }
        goto next;
    } else {
        goto next;
    }
next:
    object->frame = frame + 1;
    return 1;
complete:
    object->mode = 0;
done:
    return 0;
}

// decomp: MAINMENU.PRG func_800FA188 (src/MENU/MAINMENU.PRG/224.c:261)
int Field_MeasureTileStack(int arg0, int arg1, int* arg2)
{
    int i;
    int var_t2 = 0;
    int ret = 0;
    Field_GimmickInstance** var_t0 = Field_gimmickInstances;

    for (i = 0; i < 16; ++i) {
        Field_GimmickInstance* temp_v1 = var_t0[i];
        if ((var_t0[i] != 0)
            && ((((!var_t0[i]->isSkipped) && (!temp_v1->motionState)) && (temp_v1->mode == 0))
                && (temp_v1->tileX == arg0))
            && (temp_v1->tileZ == arg1)) {
            if (temp_v1->objectInfo.interaction == 2) {
                ret = 1;
            } else {
                var_t2 -= 0x80;
            }
        }
    }

    *arg2 = var_t2;
    return ret;
}

// decomp: MAINMENU.PRG func_800FA238 (src/MENU/MAINMENU.PRG/224.c:286)
int Field_IsGimmickWithinHeightBand(int arg0, int arg1, int arg2)
{
    int i;
    Field_GimmickInstance** var_t0 = Field_gimmickInstances;
    int new_var = arg2 + 0x80;

    for (i = 0; i < 16; ++i) {
        Field_GimmickInstance* temp_v1 = var_t0[i];

        if (var_t0[i] && !var_t0[i]->isSkipped && !temp_v1->motionState
            && (temp_v1->tileX == arg0) && (temp_v1->tileZ == arg1)
            && (temp_v1->positionY >= arg2) && (new_var >= temp_v1->positionY)) {
            return 1;
        }
    }

    return 0;
}

