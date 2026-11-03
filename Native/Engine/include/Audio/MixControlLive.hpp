#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Audio_CommandParams;

// Live bindings for MixControl.hpp over the sound-effect and music channel
// tables, the active, secondary and saved music layers, the voice
// scheduler, the driver flags, the cutscene stream's voices and pause flags,
// the command arguments and parameters, the command table, the reverb unit
// and the voice output.
//
// The command handlers keep C-callable entries: Audio_Mixer_kCommandHandlers
// (a module image) stores their addresses (copy_fixups.py
// MODULE_BINDING_ENTRIES). Each reads its words from `params`; func_8001833C
// and func_80018434 read the low byte of `params->param1`.
namespace VS::Audio::Mixer {
void LiveSetSoundEffectPitchScale(Audio_CommandParams* params);
void LiveSlideSoundEffectPitchScale(Audio_CommandParams* params);
void LiveSetWorldSoundEffectPitchScale(Audio_CommandParams* params);
void LiveSlideWorldSoundEffectPitchScale(Audio_CommandParams* params);
void LiveSetTempoScale(Audio_CommandParams* params);
void LiveSlideTempoScale(Audio_CommandParams* params);
void LiveSetTempoScaleRamp(Audio_CommandParams* params);
void LiveSetMusicPitchScale(Audio_CommandParams* params);
void LiveSlideMusicPitchScale(Audio_CommandParams* params);
void LiveSetMusicPitchScaleRamp(Audio_CommandParams* params);
void LiveStopMusicAndCancelReplacement(Audio_CommandParams* params);
void LiveStopMatchingMusic(Audio_CommandParams* params);
void LiveStopWorldSoundEffects(Audio_CommandParams* params);
void LiveEnableStereoOutput(Audio_CommandParams* params);
void LiveEnableMonoOutput(Audio_CommandParams* params);
void LiveSetMusicMuteMask(Audio_CommandParams* params);
void LiveSetMusicJumpThreshold(Audio_CommandParams* params);
void LivePauseMusic(Audio_CommandParams* params);
void LiveResumeMusic(Audio_CommandParams* params);
void LivePauseWorldSoundEffects(Audio_CommandParams* params);
void LiveResumeWorldSoundEffects(Audio_CommandParams* params);
void LiveIgnoreCommand(Audio_CommandParams* params);

// Sound_SetReverbMode over the reverb unit.
void LiveSetReverbMode(int mode);
// func_80018C30 over D_800378C0 and Audio_Mixer_kCommandHandlers. A
// sequence argument is read only within the sequence memory
// (SeqRecordsLive.hpp LiveProgramRegion); D_800378C0.param2 holds the
// second sequence of command 0x16 as a 32-bit address.
unsigned LiveDispatchCommand(unsigned command);
}  // namespace VS::Audio::Mixer
