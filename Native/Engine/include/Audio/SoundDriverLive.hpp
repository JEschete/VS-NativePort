#pragma once
// Hand-maintained (tools/port/owned.csv).

// Live bindings for SoundDriver.hpp over the legacy SPU registers, the SPU
// transfer state, the SDK root counter and event calls and the voice output.
// Generated callers keep the original call shapes (copy_fixups.py
// OWNED_CALL_BINDINGS).
namespace VS::Audio::Driver {
void LiveStart();
void LiveStop();
// The SPU transfer callback _writeSpu and _readSpu install.
void LiveCompleteSampleTransfer();
void LiveArmSampleTransferCallback();
void LiveWriteSampleMemory(char* data, unsigned size);
void LiveReadSampleMemory(char* data, unsigned size);
void LiveWaitForSampleTransfer();
void LiveSetVoiceStartAddress(int voice, unsigned address);
void LiveSetVoiceLoopAddress(int voice, unsigned address);
void LiveSetVoiceAttackRateAndMode(int voice, int attack_step,
                                   unsigned attack_mode);
void LiveSetVoiceDecayRate(int voice, int decay_rate);
void LiveSetVoiceSustainLevel(int voice, int sustain_level);
void LiveSetVoiceSustainRateAndMode(int voice, int sustain_rate,
                                    unsigned sustain_direction);
}  // namespace VS::Audio::Driver
