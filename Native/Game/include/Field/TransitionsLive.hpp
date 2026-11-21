#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/Transitions.hpp"

namespace VS::Field {
struct MpdDoorRecord;
}  // namespace VS::Field

// Binding from the screen transition globals (TransitionsLiveState.hpp), the
// pending door (VS::Field::pendingDestinationDoor), the location table and
// Ashley to the typed transition rules (Transitions.hpp). Each adapter keeps
// the original argument order for the generated callers (copy_fixups.py
// OWNED_CALL_BINDINGS).
namespace VS::Field {
// The transition and pending door as the globals hold them, and back.
[[nodiscard]] RoomTransitionState LoadLiveRoomTransition();
void StoreLiveRoomTransition(const RoomTransitionState& state);

// decomp: BATTLE.PRG func_8007C398 (src/BATTLE/BATTLE.PRG/146C.c:8702)
int LiveScreenTransitionPhase();
// decomp: BATTLE.PRG setScreenTransition (src/BATTLE/BATTLE.PRG/146C.c:8728;
// was func_8007C444)
void LiveConfigureScreenTransition(int effect, int speed, int wipe_angle);
// decomp: BATTLE.PRG setScreenTransitionWipeAngle
// (src/BATTLE/BATTLE.PRG/146C.c:8735; was func_8007C460)
void LiveSetTransitionWipeAngle(int wipe_angle);
// decomp: BATTLE.PRG func_8007C46C (src/BATTLE/BATTLE.PRG/146C.c:8740)
void LiveBeginScreenFadeOut(int effect, int speed, int wipe_angle);
// decomp: BATTLE.PRG func_8007C4AC (src/BATTLE/BATTLE.PRG/146C.c:8752)
void LiveBeginScreenFadeIn(int effect, int speed, int wipe_angle);
// decomp: BATTLE.PRG func_8007C4E0 (src/BATTLE/BATTLE.PRG/146C.c:8762)
// A null door is refused with a warning on the "field" channel; returns 0.
int LiveBeginRoomTransition(const VS::Field::MpdDoorRecord* door, int effect,
                            int speed);
// decomp: BATTLE.PRG func_8007C580 (src/BATTLE/BATTLE.PRG/146C.c:8777)
void LiveBeginScriptedRoomTransition(const VS::Field::MpdDoorRecord* door,
                                     int effect, int speed);
// decomp: BATTLE.PRG func_8007C5C0 (src/BATTLE/BATTLE.PRG/146C.c:8786)
int LiveBeginZoneTransition(const VS::Field::MpdDoorRecord* door, int effect,
                            int speed);
// decomp: BATTLE.PRG func_8007C654 (src/BATTLE/BATTLE.PRG/146C.c:8799)
void LiveBeginScriptedZoneTransition(const VS::Field::MpdDoorRecord* door,
                                     int effect, int speed);
// decomp: BATTLE.PRG func_8007C694 (src/BATTLE/BATTLE.PRG/146C.c:8808)
// A location outside the table is refused with a warning; returns 0.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveTeleportToLocation(int location_id, int tile_x, int tile_z, int effect,
                           int speed);
// decomp: INITBTL.PRG initTransitionState (src/BATTLE/INITBTL.PRG/18.c:481; was
// _initTransitionState)
void LiveInitializeRoomTransitionPrimitives();
}  // namespace VS::Field
