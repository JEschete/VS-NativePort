#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/Transitions.hpp"

struct Field_MpdDoorRecord;

// Binding from the screen transition globals (TransitionsLiveState.h), the
// pending door (Field_pendingDestinationDoor), the location table and Ashley
// to the typed transition rules (Transitions.hpp). Each adapter keeps the
// original argument order for the generated callers (copy_fixups.py
// OWNED_CALL_BINDINGS).
namespace VS::Field {
// The transition and pending door as the globals hold them, and back.
[[nodiscard]] RoomTransitionState LoadLiveRoomTransition();
void StoreLiveRoomTransition(const RoomTransitionState& state);

// decomp: BATTLE.PRG func_8007C398 (src/BATTLE/BATTLE.PRG/146C.c:8748)
int LiveScreenTransitionPhase();
// decomp: BATTLE.PRG func_8007C444 (src/BATTLE/BATTLE.PRG/146C.c:8774)
void LiveConfigureScreenTransition(int effect, int speed, int wipe_angle);
// decomp: BATTLE.PRG func_8007C460 (src/BATTLE/BATTLE.PRG/146C.c:8781)
void LiveSetTransitionWipeAngle(int wipe_angle);
// decomp: BATTLE.PRG func_8007C46C (src/BATTLE/BATTLE.PRG/146C.c:8783)
void LiveBeginScreenFadeOut(int effect, int speed, int wipe_angle);
// decomp: BATTLE.PRG func_8007C4AC (src/BATTLE/BATTLE.PRG/146C.c:8795)
void LiveBeginScreenFadeIn(int effect, int speed, int wipe_angle);
// decomp: BATTLE.PRG func_8007C4E0 (src/BATTLE/BATTLE.PRG/146C.c:8805)
// A null door is refused with a warning on the "field" channel; returns 0.
int LiveBeginRoomTransition(const Field_MpdDoorRecord* door, int effect,
                            int speed);
// decomp: BATTLE.PRG func_8007C580 (src/BATTLE/BATTLE.PRG/146C.c:8820)
void LiveBeginScriptedRoomTransition(const Field_MpdDoorRecord* door,
                                     int effect, int speed);
// decomp: BATTLE.PRG func_8007C5C0 (src/BATTLE/BATTLE.PRG/146C.c:8830)
int LiveBeginZoneTransition(const Field_MpdDoorRecord* door, int effect,
                            int speed);
// decomp: BATTLE.PRG func_8007C654 (src/BATTLE/BATTLE.PRG/146C.c:8843)
void LiveBeginScriptedZoneTransition(const Field_MpdDoorRecord* door,
                                     int effect, int speed);
// decomp: BATTLE.PRG func_8007C694 (src/BATTLE/BATTLE.PRG/146C.c:8853)
// A location outside the table is refused with a warning; returns 0.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveTeleportToLocation(int location_id, int tile_x, int tile_z, int effect,
                           int speed);
// decomp: INITBTL.PRG _initTransitionState (src/BATTLE/INITBTL.PRG/18.c:468)
void LiveInitializeRoomTransitionPrimitives();
}  // namespace VS::Field
