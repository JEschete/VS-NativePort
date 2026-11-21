#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// Binding from the zone context (VS::Field::zoneContext), the loaded room
// record (VS::Field::roomData), the pending door and the session's heap, disc
// queue, actors, events, camera and music to the typed zone and room loading
// rules (RoomLoading.hpp). Each adapter keeps the original call shape for the
// generated callers (copy_fixups.py OWNED_CALL_BINDINGS). Refusals are
// reported on the "field" channel.
namespace VS::Field {
// decomp: BATTLE.PRG loadZnd (src/BATTLE/BATTLE.PRG/146C.c:14674; was _loadZnd)
void LiveQueueZoneLoad(int zone_id);
// decomp: BATTLE.PRG finishLoadZnd (src/BATTLE/BATTLE.PRG/146C.c:14697; was
// _finishLoadZnd)
void LiveFinishZoneLoad(int zone_id);
// decomp: BATTLE.PRG func_80089098 (src/BATTLE/BATTLE.PRG/146C.c:14757)
void LiveReleaseZoneData();
// decomp: BATTLE.PRG func_80089104 (src/BATTLE/BATTLE.PRG/146C.c:14770)
int LiveZoneSoundFileId();
// decomp: BATTLE.PRG func_80089114 (src/BATTLE/BATTLE.PRG/146C.c:14901)
void LiveSpawnRoomActors();
// decomp: BATTLE.PRG loadMpd (src/BATTLE/BATTLE.PRG/146C.c:15124; was _loadMpd)
void LiveQueueRoomLoad(int room_id);
// decomp: BATTLE.PRG func_80089DC0 (src/BATTLE/BATTLE.PRG/146C.c:15272)
[[nodiscard]] bool LiveFinishRoomLoad(int room_id);
// decomp: BATTLE.PRG func_8008A3A0 (src/BATTLE/BATTLE.PRG/146C.c:15359)
void LiveUnloadCurrentRoom();

// The loaded room's environment; each is 0 without a whole environment
// section, which the original read through a null pointer.
// decomp: BATTLE.PRG vs_battle_inWorkshop (src/BATTLE/BATTLE.PRG/146C.c:16672)
int LiveIsCurrentRoomWorkshop();
// decomp: BATTLE.PRG vs_battle_getCurrentSceneId
// (src/BATTLE/BATTLE.PRG/146C.c:16676)
int LiveCurrentSceneId();
// decomp: BATTLE.PRG func_8008D418 (src/BATTLE/BATTLE.PRG/146C.c:16678)
int LiveRoomFallDestination();
// decomp: BATTLE.PRG func_8008E470 (src/BATTLE/BATTLE.PRG/146C.c:17200)
int LiveIsRoomAudioSequenceAbsent();
// decomp: BATTLE.PRG func_8008E4AC (src/BATTLE/BATTLE.PRG/146C.c:17208)
std::int16_t LiveRoomEnvironmentParameter();
// decomp: BATTLE.PRG getRoomTrapLevel (src/BATTLE/BATTLE.PRG/146C.c:17210; was
// _getRoomTrapLevel)
int LiveRoomTrapLevel();
}  // namespace VS::Field
