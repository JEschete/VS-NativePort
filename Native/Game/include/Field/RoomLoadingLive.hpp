#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// Binding from the zone context (Field_zoneContext), the loaded room record
// (Field_roomData), the pending door and the session's heap, disc queue,
// actors, events, camera and music to the typed zone and room loading rules
// (RoomLoading.hpp). Each adapter keeps the original call shape for the
// generated callers (copy_fixups.py OWNED_CALL_BINDINGS). Refusals are
// reported on the "field" channel.
namespace VS::Field {
// decomp: BATTLE.PRG _loadZnd (src/BATTLE/BATTLE.PRG/146C.c:14803)
void LiveQueueZoneLoad(int zone_id);
// decomp: BATTLE.PRG _finishLoadZnd (src/BATTLE/BATTLE.PRG/146C.c:14826)
void LiveFinishZoneLoad(int zone_id);
// decomp: BATTLE.PRG func_80089098 (src/BATTLE/BATTLE.PRG/146C.c:14886)
void LiveReleaseZoneData();
// decomp: BATTLE.PRG func_80089104 (src/BATTLE/BATTLE.PRG/146C.c:14899)
int LiveZoneSoundFileId();
// decomp: BATTLE.PRG func_80089114 (src/BATTLE/BATTLE.PRG/146C.c:14901)
void LiveSpawnRoomActors();
// decomp: BATTLE.PRG _loadMpd (src/BATTLE/BATTLE.PRG/146C.c:15254)
void LiveQueueRoomLoad(int room_id);
// decomp: BATTLE.PRG func_80089DC0 (src/BATTLE/BATTLE.PRG/146C.c:15272)
void LiveFinishRoomLoad(int room_id);
// decomp: BATTLE.PRG func_8008A3A0 (src/BATTLE/BATTLE.PRG/146C.c:15489)
void LiveUnloadCurrentRoom();

// The loaded room's environment; each is 0 without a whole environment
// section, which the original read through a null pointer.
// decomp: BATTLE.PRG vs_battle_inWorkshop (src/BATTLE/BATTLE.PRG/146C.c:16798)
int LiveIsCurrentRoomWorkshop();
// decomp: BATTLE.PRG vs_battle_getCurrentSceneId
// (src/BATTLE/BATTLE.PRG/146C.c:16802)
int LiveCurrentSceneId();
// decomp: BATTLE.PRG func_8008D418 (src/BATTLE/BATTLE.PRG/146C.c:16804)
int LiveRoomFallDestination();
// decomp: BATTLE.PRG func_8008E470 (src/BATTLE/BATTLE.PRG/146C.c:17321)
int LiveIsRoomAudioSequenceAbsent();
// decomp: BATTLE.PRG func_8008E4AC (src/BATTLE/BATTLE.PRG/146C.c:17329)
std::int16_t LiveRoomEnvironmentParameter();
// decomp: BATTLE.PRG _getRoomTrapLevel (src/BATTLE/BATTLE.PRG/146C.c:17331)
int LiveRoomTrapLevel();
}  // namespace VS::Field
