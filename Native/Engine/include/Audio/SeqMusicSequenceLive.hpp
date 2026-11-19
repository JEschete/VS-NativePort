#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

namespace VS::Audio {
struct AkaoSequenceFile;
struct CommandParams;
struct MusicConfig;
struct SeqChannel;
}  // namespace VS::Audio

// Binds the typed music commands (MusicSequence.cpp in src\Audio\Seq) to the
// live driver: the primary, saved and secondary music layers, their channel
// tables, the voice scheduler, the master fade and the CD audio volume. A
// track's sequence is read only from the memory it lies in; a header or
// channel table that would read outside it is refused and reported.
namespace VS::Audio::Seq {
// func_80015C9C: points a sound-effect channel at `program_start`.
void LiveResetSoundEffectChannel(VS::Audio::SeqChannel* channel,
                                 std::uint8_t* program_start);
// Sound_LoadAkaoSequence: starts `sequence` on the active music layer.
void LiveLoadMusicSequence(VS::Audio::AkaoSequenceFile* sequence);
// Sound_KillMusicConfig: stops `config`'s track when `music_id_filter` is 0
// or names it, and releases the voices it owns.
void LiveReleaseMusicConfiguration(VS::Audio::MusicConfig* config,
                                   VS::Audio::SeqChannel* channels,
                                   unsigned music_id_filter);
// Sound_SetMusicSequence: resumes the restored track on `sequence`.
void LiveRestoreMusicSequence(VS::Audio::AkaoSequenceFile* sequence,
                              int swap_with_saved);

// The music driver commands (VS::Audio::Mixer::kCommandHandlers).
void LiveStartOrResumeMusic(VS::Audio::CommandParams* params);
// Takes no parameters; the pointer keeps the command handler signature.
void LiveSaveCurrentMusic(VS::Audio::CommandParams* params);
void LiveStartMusicLayer(VS::Audio::CommandParams* params);
void LiveCrossfadeMusicLayers(VS::Audio::CommandParams* params);
void LiveStartMusicWithLoopCounter(VS::Audio::CommandParams* params);
void LiveSetMusicVolume(VS::Audio::CommandParams* params);
void LiveFadeMusicVolume(VS::Audio::CommandParams* params);
void LiveFadeMusicVolumeFrom(VS::Audio::CommandParams* params);
void LiveSetDiscVolume(VS::Audio::CommandParams* params);
void LiveFadeDiscVolume(VS::Audio::CommandParams* params);
void LiveFadeDiscVolumeFrom(VS::Audio::CommandParams* params);
}  // namespace VS::Audio::Seq
