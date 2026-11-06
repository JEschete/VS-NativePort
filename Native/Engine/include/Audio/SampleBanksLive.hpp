#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Audio_SeqChannel;

// Live bindings for SampleBanks.hpp over the music and sound-effect bank ids
// (D_80036770, D_800377E0), the active music's flags, the mixer's music bank
// bit, the stream (D_80039BC8, D_80037850 and the driver's stream latch), the
// instrument table and sample memory. Generated and owned callers keep the
// original call shapes (copy_fixups.py OWNED_CALL_BINDINGS); a bank pointer
// is trusted for its header words, as func_800135D8 trusts it.
namespace VS::Audio::Samples {
void LiveLoadBank(void* bank, int wait);
int LiveTransferStatus();
int LiveClearMusicBankIds();
// Whether the bank went to the alternate music bank.
int LiveLoadMusicBank(void* bank, int wait);
int LiveLoadSoundEffectBank(void* bank, int slot, int wait);
// Sound_IsNotAkaoFile; a null sequence is refused as not AKAO.
int LiveSequenceMagicDifference(const void* data);
std::uint16_t LiveMapInstrumentToAlternateBank(unsigned music_flags,
                                               Audio_SeqChannel* channel);
std::uint16_t LiveMapInstrumentToBaseBank(unsigned music_flags,
                                          Audio_SeqChannel* channel);
int LiveSoundEffectBankSlot(int selector);
}  // namespace VS::Audio::Samples
