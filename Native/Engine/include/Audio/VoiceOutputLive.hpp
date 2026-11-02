#pragma once
// Hand-maintained (tools/port/owned.csv).

// Bindings from the generated sound driver to the typed voice writes and bank
// upload (VoiceOutput.cpp) over the native voice engine. None is stored in a
// module table, so each takes typed arguments. A voice outside the 24 SPU
// voices, a sample rate outside the 16-bit pitch register and a release rate
// or mode outside its ADSR field are refused with a diagnostic; mask bits
// above voice 23 are ignored with one.
namespace VS::Audio::Driver {
void KeyOnLiveVoices(unsigned mask);
void KeyOffLiveVoices(unsigned mask);
void SetLiveVoiceReverbMask(unsigned mask);
void SetLiveVoiceNoiseMask(unsigned mask);
void SetLiveVoiceFrequencyModulationMask(unsigned mask);
void SetLiveVoiceVolume(int voice, unsigned left, unsigned right,
                        unsigned scale);
void SetLiveVoiceSampleRate(int voice, int sample_rate);
void SetLiveVoiceReleaseRateAndMode(int voice, int rate, unsigned mode);
}  // namespace VS::Audio::Driver

namespace VS::Audio::Samples {
// Binds the decoded assets of the AKAO bank whose 32-byte header `bank`
// points at to instrument table slots from `first_instrument`, relocated by
// `sample_address`. Returns 0 and clears the sample transfer state, or
// returns -1 and marks the transfer failed. `wait` has no native transfer to
// wait for.
int UploadLiveBank(const void* bank, int wait, int first_instrument,
                   int sample_address);
}  // namespace VS::Audio::Samples
