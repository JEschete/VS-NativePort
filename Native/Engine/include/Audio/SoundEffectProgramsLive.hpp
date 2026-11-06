#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

struct Audio_CommandParams;
struct Audio_MusicConfig;
struct Audio_SeqChannel;

// Live bindings for SoundEffectPrograms.hpp over the sound-effect channels,
// the voice scheduler, the cutscene stream's voices and pause flags, the
// driver update flags, the music channels' voices and the active layer's
// notes, the voice owners, the voice output, the shared sound-effect tables
// and the sequence memory.
//
// The command handlers keep C-callable entries: Audio_Mixer_kCommandHandlers
// (a module image) stores their addresses (copy_fixups.py
// MODULE_BINDING_ENTRIES). Generated and owned callers of the other entries
// keep the original call shapes (OWNED_CALL_BINDINGS).
namespace VS::Audio::SoundEffects {
void LiveReleaseMatchingVoices(std::intptr_t selector, int flags);
// A null program is no program; a null `params` is refused.
void LivePlayProgramPair(Audio_CommandParams* params, char* first_program,
                         char* second_program, int skip_release);
// `channels` is the layer's 32-entry channel table; null arguments are
// refused.
void LiveInvalidateMusicChannelVolumes(Audio_MusicConfig* layer,
                                       Audio_SeqChannel* channels);
void LiveInvalidateSoundEffectVolumes();

void LivePlayInterfaceProgramPair(Audio_CommandParams* params);
void LivePlayInterfaceProgram(Audio_CommandParams* params);
void LivePlaySharedProgram(Audio_CommandParams* params);
void LivePlayBlobPrograms(Audio_CommandParams* params);
void LiveStopPrograms(Audio_CommandParams* params);
void LiveSetProgramVolume(Audio_CommandParams* params);
void LiveFadeProgramVolume(Audio_CommandParams* params);
void LiveSetSceneVolume(Audio_CommandParams* params);
void LiveFadeSceneVolume(Audio_CommandParams* params);
void LiveSetProgramPan(Audio_CommandParams* params);
void LiveFadeProgramPan(Audio_CommandParams* params);
void LiveSetScenePan(Audio_CommandParams* params);
void LiveFadeScenePan(Audio_CommandParams* params);
}  // namespace VS::Audio::SoundEffects
