#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Audio {
struct CommandParams;
struct MusicConfig;
struct SeqChannel;
}  // namespace VS::Audio

// Live bindings for SoundEffectPrograms.hpp over the sound-effect channels,
// the voice scheduler, the cutscene stream's voices and pause flags, the
// driver update flags, the music channels' voices and the active layer's
// notes, the voice owners, the voice output, the shared sound-effect tables
// and the sequence memory.
//
// The command handlers keep C-callable entries:
// VS::Audio::Mixer::kCommandHandlers (a module image) stores their addresses
// (copy_fixups.py MODULE_BINDING_ENTRIES). Generated and owned callers of the
// other entries keep the original call shapes (OWNED_CALL_BINDINGS).
namespace VS::Audio::SoundEffects {
void LiveReleaseMatchingVoices(std::intptr_t selector, int flags);
// A null program is no program; a null `params` is refused.
void LivePlayProgramPair(VS::Audio::CommandParams* params,
                         std::uint8_t* first_program,
                         std::uint8_t* second_program, int skip_release);
// `channels` is the layer's 32-entry channel table; null arguments are
// refused.
void LiveInvalidateMusicChannelVolumes(VS::Audio::MusicConfig* layer,
                                       VS::Audio::SeqChannel* channels);
void LiveInvalidateSoundEffectVolumes();

void LivePlayInterfaceProgramPair(VS::Audio::CommandParams* params);
void LivePlayInterfaceProgram(VS::Audio::CommandParams* params);
void LivePlaySharedProgram(VS::Audio::CommandParams* params);
void LivePlayBlobPrograms(VS::Audio::CommandParams* params);
void LiveStopPrograms(VS::Audio::CommandParams* params);
void LiveSetProgramVolume(VS::Audio::CommandParams* params);
void LiveFadeProgramVolume(VS::Audio::CommandParams* params);
void LiveSetSceneVolume(VS::Audio::CommandParams* params);
void LiveFadeSceneVolume(VS::Audio::CommandParams* params);
void LiveSetProgramPan(VS::Audio::CommandParams* params);
void LiveFadeProgramPan(VS::Audio::CommandParams* params);
void LiveSetScenePan(VS::Audio::CommandParams* params);
void LiveFadeScenePan(VS::Audio::CommandParams* params);
}  // namespace VS::Audio::SoundEffects
