// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
void Field_ConfigureScreenTransition(int arg0, int arg1, int arg2)
{
    Field_screenTransitionEffect = arg0;
    Field_screenTransitionSpeed = arg1;
    Field_screenTransitionWipeAngle = arg2;
}

// decomp: BATTLE.PRG func_8007C460 (src/BATTLE/BATTLE.PRG/146C.c:8781)
void Field_SetTransitionWipeAngle(int arg0) { Field_screenTransitionWipeAngle = arg0; }

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
void Field_BeginScreenFadeIn(int effect, int arg1, int arg2)
{
    Field_screenTransitionStep = 7;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = arg1;
    Field_screenTransitionWipeAngle = arg2;
}

// decomp: BATTLE.PRG func_8007C4E0 (src/BATTLE/BATTLE.PRG/146C.c:8805)
int Field_BeginRoomTransition(Field_MpdDoorRecord* arg0, int effect, int speed)
{
    Field_pendingDestinationDoor = *arg0;
    Field_screenTransitionStep = 1;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = speed;
    Field_pendingDestinationDoor.destinationZoneId = Field_zoneContext.zndId;
    Field_QueueRoomLoad(arg0->destinationRoomId);
    Actors_RestoreActorIdlePose(0);
    Field_SetActorCollisionBypass(0, 1);
    Field_SetEntranceDoorRecord(NULL);

    return 1;
}

// decomp: BATTLE.PRG func_8007C580 (src/BATTLE/BATTLE.PRG/146C.c:8820)
void Field_BeginScriptedRoomTransition(Field_MpdDoorRecord* arg0, int arg1, int arg2)
{
    Actors_ActorLocationQuery sp10;

    Field_BeginRoomTransition(arg0, arg1, arg2);
    Actors_ActorPlacement(0, &sp10);
    Field_FindEntranceDoorNearTile(sp10.location.packedTile.target.tileX, sp10.location.packedTile.target.tileZ,
        sp10.location.packedTile.target.tileLayer);
}

// decomp: BATTLE.PRG func_8007C5C0 (src/BATTLE/BATTLE.PRG/146C.c:8830)
int Field_BeginZoneTransition(Field_MpdDoorRecord* arg0, int effect, int speed)
{
    Field_pendingDestinationDoor = *arg0;
    Field_screenTransitionStep = 2;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = speed;
    Field_QueueZoneLoad(arg0->destinationZoneId);
    Actors_RestoreActorIdlePose(0);
    Field_SetActorCollisionBypass(0, 1);
    Field_SetEntranceDoorRecord(NULL);
    return 1;
}

// decomp: BATTLE.PRG func_8007C654 (src/BATTLE/BATTLE.PRG/146C.c:8843)
void Field_BeginScriptedZoneTransition(Field_MpdDoorRecord* arg0, int arg1, int arg2)
{
    Actors_ActorLocationQuery sp10;

    Field_BeginZoneTransition(arg0, arg1, arg2);
    Actors_ActorPlacement(0, &sp10);
    Field_FindEntranceDoorNearTile(sp10.location.packedTile.target.tileX, sp10.location.packedTile.target.tileZ,
        sp10.location.packedTile.target.tileLayer);
}

// decomp: BATTLE.PRG func_8007C694 (src/BATTLE/BATTLE.PRG/146C.c:8853)
int Field_TeleportToLocation(int arg0, int arg1, int arg2, int effect, int speed)
{
    Actors_ActorLocationQuery sp10;
    int temp_v1_2;
    int var_a0;
    int zndId;
    int mpdId;

    zndId = Field_locationDestinations[arg0 * 2];
    mpdId = Field_locationDestinations[arg0 * 2 + 1];

    Field_pendingDestinationDoor.entryTileX = arg1;
    Field_pendingDestinationDoor.entryTileZ = arg2;
    Field_pendingDestinationDoor.tileLocation.entryDirection = 0;

    Actors_ActorPlacement(0, &sp10);

    temp_v1_2 = sp10.location.position.pad + 0x200;
    var_a0 = temp_v1_2 - ((temp_v1_2 / ONE) * ONE);
    if (var_a0 < 0) {
        var_a0 += 0x3FF;
    }

    Field_screenTransitionStep = 1;
    Field_screenTransitionEffect = effect;
    Field_screenTransitionSpeed = speed;
    Field_pendingDestinationDoor.entryElevation = var_a0 >> 10;
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
    RECT sp18;
    memset(&sp18, 0, sizeof sp18);

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

    SetDrawMode(Render_legacyTransitionDrawModes, 0, 1, getTPage(0, 2, 0, 0), &sp18);
    Render_legacyTransitionDrawModes[1] = Render_legacyTransitionDrawModes[0];

    Field_screenTransitionAlpha = 255;
}

