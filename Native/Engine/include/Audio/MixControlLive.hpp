#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Audio {
struct CommandParams;
}  // namespace VS::Audio

// Live bindings for MixControl.hpp over the sound-effect and music channel
// tables, the active, secondary and saved music layers, the voice
// scheduler, the driver flags, the cutscene stream's voices and pause flags,
// the command arguments and parameters, the command table, the reverb unit
// and the voice output.
//
// The command handlers keep C-callable entries:
// VS::Audio::Mixer::kCommandHandlers (a module image) stores their addresses
// (copy_fixups.py MODULE_BINDING_ENTRIES). Each reads its words from `params`;
// func_8001833C and func_80018434 read the low byte of `params->param1`.
namespace VS::Audio::Mixer {
void LiveSetSoundEffectPitchScale(VS::Audio::CommandParams* params);
void LiveSlideSoundEffectPitchScale(VS::Audio::CommandParams* params);
void LiveSetWorldSoundEffectPitchScale(VS::Audio::CommandParams* params);
void LiveSlideWorldSoundEffectPitchScale(VS::Audio::CommandParams* params);
void LiveSetTempoScale(VS::Audio::CommandParams* params);
void LiveSlideTempoScale(VS::Audio::CommandParams* params);
void LiveSetTempoScaleRamp(VS::Audio::CommandParams* params);
void LiveSetMusicPitchScale(VS::Audio::CommandParams* params);
void LiveSlideMusicPitchScale(VS::Audio::CommandParams* params);
void LiveSetMusicPitchScaleRamp(VS::Audio::CommandParams* params);
void LiveStopMusicAndCancelReplacement(VS::Audio::CommandParams* params);
void LiveStopMatchingMusic(VS::Audio::CommandParams* params);
void LiveStopWorldSoundEffects(VS::Audio::CommandParams* params);
void LiveEnableStereoOutput(VS::Audio::CommandParams* params);
void LiveEnableMonoOutput(VS::Audio::CommandParams* params);
void LiveSetMusicMuteMask(VS::Audio::CommandParams* params);
void LiveSetMusicJumpThreshold(VS::Audio::CommandParams* params);
void LivePauseMusic(VS::Audio::CommandParams* params);
void LiveResumeMusic(VS::Audio::CommandParams* params);
void LivePauseWorldSoundEffects(VS::Audio::CommandParams* params);
void LiveResumeWorldSoundEffects(VS::Audio::CommandParams* params);
void LiveIgnoreCommand(VS::Audio::CommandParams* params);

// Sound_SetReverbMode over the reverb unit.
void LiveSetReverbMode(int mode);
// func_80018C30 over D_800378C0 and VS::Audio::Mixer::kCommandHandlers. A
// sequence argument is read only within the sequence memory
// (SeqRecordsLive.hpp LiveProgramRegion); D_800378C0.param2 holds the
// second sequence of command 0x16 as a 32-bit address.
unsigned LiveDispatchCommand(unsigned command);
// func_80018C30 with the second sequence (command 0x16's) passed as a
// pointer rather than as D_800378C0.param2's 32-bit address.
unsigned LiveDispatchCommand(unsigned command, std::uint8_t* second_sequence);
}  // namespace VS::Audio::Mixer
