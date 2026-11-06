#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// Live bindings for SoundBanks.hpp over the slot record (vs_main_soundData),
// the sound-effect and wave bank file tables, _soundFileMap, the disc request
// queue (CdDriveLive.hpp), the high end of the game heap and the playback and
// sample bank bindings. Generated and owned callers keep the original call
// shapes (copy_fixups.py OWNED_CALL_BINDINGS). A slot's buffers and requests
// stay the legacy pointers: the room effect bank's address and the failed
// request marker ((vs_main_CdQueueSlot*)-1) are kept as stored.
namespace VS::Audio {
int LiveLoadSfxBankSlot(int id, unsigned slot);
int LiveLoadSfxBankIntoFreeSlot(int id);
int LivePollSfxBankSlotLoad(unsigned slot);
int LivePollSfxBankLoads();
int LiveLoadSfxBankBlocking(int id);
int LiveSelectSfxBankSlot(int slot);
int LiveFreeSfxBankSlot(unsigned slot);
int LiveBindRoomSfxBank(std::uintptr_t bank_address);
int LiveUnbindRoomSfxBank();
void LiveLoadWaveBankFile(int file);
void LiveLoadMappedWaveBank(int id);
int LiveUpdateWaveBankLoad();
void LiveLoadWaveBankBlocking(int id);
void LiveSetMonoOutput(int mono_enabled);
void LiveInitializeMenuSoundBanks();
void LiveResetSoundSlots();
}  // namespace VS::Audio
