#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live bindings for Music.hpp over the slot record (vs_main_soundData), the
// music file table (_musicLBAs, _musicFileSizes), the disc request queue,
// the high end of the game heap, the playback front end, the sound control
// requests and the zone context's music switch. Generated and owned callers
// keep the original call shapes (copy_fixups.py OWNED_CALL_BINDINGS). A
// slot's buffer and request stay the legacy pointers, the failed request
// marker ((vs_main_CdQueueSlot*)-1) told apart by equality only.
namespace VS::Audio {
int LiveSetZoneMusicSwitchEnabled(int enabled);
int LiveMusicBeatPosition();
void LiveSaveMusicPlaybackState();
void LiveSetMusicJumpThreshold(int threshold);
int LiveStopMusicSlotPlayback(int id);
int LiveResumeMusicSlot(int id);
int LiveCrossfadeMusicSlot(int id, int target_volume, unsigned fade_ticks);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LivePlayMusicSlot(int id, int target_volume, int fade_ticks);
int LiveCurrentMusicSlot();
int LiveCurrentMusicId();
int LiveLoadMusicSlot(int id, int target_slot);
int LiveLoadMusicIntoFreeSlot(int id);
int LivePollMusicSlotLoad(unsigned slot);
int LivePollMusicLoads();
int LiveLoadMusicSlotBlocking(int id, int target_slot);
int LiveLoadFreeMusicSlotBlocking(int id);
int LiveFreeMusicSlot(int id);
int LiveMusicSlotVolume(int id);
int LiveQueryMusicSlotVolume(int id);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveFadeMusicSlotVolume(int id, int target_volume, int fade_ticks);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveFadeMusicSlotTempo(int id, int target_scale, int fade_ticks);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveFadeMusicSlotPitch(int id, int target_scale, int fade_ticks);
void LiveStopAndFreeCurrentMusic();
}  // namespace VS::Audio
