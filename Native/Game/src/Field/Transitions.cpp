// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Lifecycle.h"
#include "Actors/Location.h"
#include "DiscQueue.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Locations.h"
#include "Field/Mpd.h"
#include "Field/RoomLoading.h"
#include "Field/Transitions.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Render/ScreenTransition.h"

// decomp: vs_battle_screenTransitionStep (vs_battle_screenTransitionStep@BATTLE/BATTLE.PRG)
u_int Field_screenTransitionStep;

// decomp: vs_battle_screenTransitionEffect (vs_battle_screenTransitionEffect@BATTLE/BATTLE.PRG)
int Field_screenTransitionEffect;

// decomp: vs_battle_screenTransitionWipeAngle (vs_battle_screenTransitionWipeAngle@BATTLE/BATTLE.PRG)
int Field_screenTransitionWipeAngle;

// decomp: vs_battle_screenTransitionSpeed (vs_battle_screenTransitionSpeed@BATTLE/BATTLE.PRG)
int Field_screenTransitionSpeed;

// decomp: BATTLE.PRG func_8007C398 (src/BATTLE/BATTLE.PRG/146C.c:8748)
int Field_ScreenTransitionPhase(void)
{
    switch (Field_screenTransitionStep) {
    case 1:
    case 2:
        return 1;
    case 3:
        return 2;
    case 4:
        return 3;
    case 5:
        return 4;
    case 6:
        return 5;
    case 7:
        return 6;
    case 0:
    default:
        return 0;
    }
}

// decomp: BATTLE.PRG func_8007C444 (src/BATTLE/BATTLE.PRG/146C.c:8774)
void Field_ConfigureScreenTransition(int transition_effect, int transition_speed, int wipe_angle)
{
    Field_screenTransitionEffect = transition_effect;
    Field_screenTransitionSpeed = transition_speed;
    Field_screenTransitionWipeAngle = wipe_angle;
}

// decomp: BATTLE.PRG func_8007C460 (src/BATTLE/BATTLE.PRG/146C.c:8781)
void Field_SetTransitionWipeAngle(int wipe_angle) { Field_screenTransitionWipeAngle = wipe_angle; }

// decomp: BATTLE.PRG func_8007C46C (src/BATTLE/BATTLE.PRG/146C.c:8783)
void Field_BeginScreenFadeOut(int effect, int speed, int angle)
{
    if (Field_screenTransitionStep == 1 || Field_screenTransitionStep == 2) {
        return;
    }

    Field_screenTransitionStep = 5;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = speed;
    Field_screenTransitionWipeAngle = angle;
}

// decomp: BATTLE.PRG func_8007C4AC (src/BATTLE/BATTLE.PRG/146C.c:8795)
void Field_BeginScreenFadeIn(int effect, int transition_speed, int wipe_angle)
{
    Field_screenTransitionStep = 7;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = transition_speed;
    Field_screenTransitionWipeAngle = wipe_angle;
}

// decomp: BATTLE.PRG func_8007C4E0 (src/BATTLE/BATTLE.PRG/146C.c:8805)
int Field_BeginRoomTransition(Field_MpdDoorRecord* destination_door, int effect, int speed)
{
    Field_pendingDestinationDoor = *destination_door;
    Field_screenTransitionStep = 1;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = speed;
    Field_pendingDestinationDoor.destinationZoneId = Field_zoneContext.zndId;
    Field_QueueRoomLoad(destination_door->destinationRoomId);
    Actors_RestoreActorIdlePose(0);
    Field_SetActorCollisionBypass(0, 1);
    Field_SetEntranceDoorRecord(NULL);

    return 1;
}

// decomp: BATTLE.PRG func_8007C580 (src/BATTLE/BATTLE.PRG/146C.c:8820)
void Field_BeginScriptedRoomTransition(Field_MpdDoorRecord* destination_door, int transition_effect, int transition_speed)
{
    Actors_ActorLocationQuery ashley_placement;

    Field_BeginRoomTransition(destination_door, transition_effect, transition_speed);
    Actors_ActorPlacement(0, &ashley_placement);
    Field_FindEntranceDoorNearTile(ashley_placement.location.packedTile.target.tileX, ashley_placement.location.packedTile.target.tileZ,
        ashley_placement.location.packedTile.target.tileLayer);
}

// decomp: BATTLE.PRG func_8007C5C0 (src/BATTLE/BATTLE.PRG/146C.c:8830)
int Field_BeginZoneTransition(Field_MpdDoorRecord* destination_door, int effect, int speed)
{
    Field_pendingDestinationDoor = *destination_door;
    Field_screenTransitionStep = 2;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = speed;
    Field_QueueZoneLoad(destination_door->destinationZoneId);
    Actors_RestoreActorIdlePose(0);
    Field_SetActorCollisionBypass(0, 1);
    Field_SetEntranceDoorRecord(NULL);
    return 1;
}

// decomp: BATTLE.PRG func_8007C654 (src/BATTLE/BATTLE.PRG/146C.c:8843)
void Field_BeginScriptedZoneTransition(Field_MpdDoorRecord* destination_door, int transition_effect, int transition_speed)
{
    Actors_ActorLocationQuery ashley_placement;

    Field_BeginZoneTransition(destination_door, transition_effect, transition_speed);
    Actors_ActorPlacement(0, &ashley_placement);
    Field_FindEntranceDoorNearTile(ashley_placement.location.packedTile.target.tileX, ashley_placement.location.packedTile.target.tileZ,
        ashley_placement.location.packedTile.target.tileLayer);
}

// decomp: BATTLE.PRG func_8007C694 (src/BATTLE/BATTLE.PRG/146C.c:8853)
int Field_TeleportToLocation(int location_id, int destination_tile_x, int destination_tile_z, int effect, int speed)
{
    Actors_ActorLocationQuery ashley_placement;
    int rounded_facing_angle;
    int wrapped_facing_for_quadrant;
    int zndId;
    int mpdId;

    zndId = Field_locationDestinations[location_id * 2];
    mpdId = Field_locationDestinations[location_id * 2 + 1];

    Field_pendingDestinationDoor.entryTileX = destination_tile_x;
    Field_pendingDestinationDoor.entryTileZ = destination_tile_z;
    Field_pendingDestinationDoor.tileLocation.entryDirection = 0;

    Actors_ActorPlacement(0, &ashley_placement);

    rounded_facing_angle = ashley_placement.location.position.pad + 0x200;
    wrapped_facing_for_quadrant = rounded_facing_angle - ((rounded_facing_angle / ONE) * ONE);
    if (wrapped_facing_for_quadrant < 0) {
        wrapped_facing_for_quadrant += 0x3FF;
    }

    Field_screenTransitionStep = 1;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = speed;
    Field_pendingDestinationDoor.entryElevation = wrapped_facing_for_quadrant >> 10;
    Field_pendingDestinationDoor.reserved06Bit15 = 0;

    if (zndId != Field_pendingDestinationDoor.destinationZoneId) {
        Field_screenTransitionStep = 2;
        Field_QueueZoneLoad(zndId);
    } else if (mpdId != Field_pendingDestinationDoor.destinationRoomId) {
        Field_screenTransitionStep = 1;
        Field_QueueRoomLoad(mpdId);
    }

    Field_pendingDestinationDoor.destinationZoneId = zndId;
    Field_pendingDestinationDoor.destinationRoomId = mpdId;
    Actors_FreezeAllActorAnimations();
    Field_SetActorCollisionBypass(0, 1);
    Field_SetEntranceDoorRecord(NULL);

    return 1;
}

// decomp: INITBTL.PRG _initTransitionState (src/BATTLE/INITBTL.PRG/18.c:468)
void Field_InitializeRoomTransitionPrimitives(void)
{
    RECT texture_window_rect;
    memset(&texture_window_rect, 0, sizeof texture_window_rect);

    SetPolyF4(&Render_legacyTransitionFadePackets[0]);
    setRGB0(&Render_legacyTransitionFadePackets[0], 0, 0, 0);
    setXY4(&Render_legacyTransitionFadePackets[0], 0, 0, 320, 0, 0, 240, 320, 240);
    SetSemiTrans(&Render_legacyTransitionFadePackets[0], 1);
    Render_legacyTransitionFadePackets[1] = Render_legacyTransitionFadePackets[0];

    SetPolyF4(&Render_legacyTransitionWipePackets[0].coveredQuad);
    setRGB0(&Render_legacyTransitionWipePackets[0].coveredQuad, 0, 0, 0);
    SetSemiTrans(Render_legacyTransitionWipePackets, 0);
    setRGB0(&Render_legacyTransitionWipePackets[0].coveredQuad, 0, 0, 1);
    Render_legacyTransitionWipePackets[0].edgeQuad =
        Render_legacyTransitionWipePackets[0].coveredQuad;
    Render_legacyTransitionWipePackets[1] = Render_legacyTransitionWipePackets[0];

    SetPolyG4(&Render_legacyTransitionEdgePackets[0]);
    setRGB0(&Render_legacyTransitionEdgePackets[0], 255, 255, 255);
    setRGB2(&Render_legacyTransitionEdgePackets[0], 255, 255, 255);
    setRGB1(&Render_legacyTransitionEdgePackets[0], 0, 0, 0);
    setRGB3(&Render_legacyTransitionEdgePackets[0], 0, 0, 0);
    setXY4(&Render_legacyTransitionEdgePackets[0], 0, 0, 320, 0, 0, 240, 320, 240);
    SetSemiTrans(Render_legacyTransitionEdgePackets, 1);
    Render_legacyTransitionEdgePackets[1] = Render_legacyTransitionEdgePackets[0];

    SetDrawMode(Render_legacyTransitionDrawModes, 0, 1, getTPage(0, 2, 0, 0), &texture_window_rect);
    Render_legacyTransitionDrawModes[1] = Render_legacyTransitionDrawModes[0];

    Field_screenTransitionAlpha = 255;
}

}  // extern "C"
