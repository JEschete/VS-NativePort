#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Audio_AkaoSequenceFile;
struct Audio_CommandParams;
struct Audio_MusicConfig;
struct Audio_SeqChannel;

// Binds the typed music commands (MusicSequence.cpp in src\Audio\Seq) to the
// live driver: the primary, saved and secondary music layers, their channel
// tables, the voice scheduler, the master fade and the CD audio volume. A
// track's sequence is read only from the memory it lies in; a header or
// channel table that would read outside it is refused and reported.
namespace VS::Audio::Seq {
// func_80015C9C: points a sound-effect channel at `program_start`.
void LiveResetSoundEffectChannel(Audio_SeqChannel* channel,
                                 char* program_start);
// Sound_LoadAkaoSequence: starts `sequence` on the active music layer.
void LiveLoadMusicSequence(Audio_AkaoSequenceFile* sequence);
// Sound_KillMusicConfig: stops `config`'s track when `music_id_filter` is 0
// or names it, and releases the voices it owns.
void LiveReleaseMusicConfiguration(Audio_MusicConfig* config,
                                   Audio_SeqChannel* channels,
                                   unsigned music_id_filter);
// Sound_SetMusicSequence: resumes the restored track on `sequence`.
void LiveRestoreMusicSequence(Audio_AkaoSequenceFile* sequence,
                              int swap_with_saved);

// The music driver commands (Audio_Mixer_kCommandHandlers).
void LiveStartOrResumeMusic(Audio_CommandParams* params);
// Takes no parameters; the pointer keeps the command handler signature.
void LiveSaveCurrentMusic(Audio_CommandParams* params);
void LiveStartMusicLayer(Audio_CommandParams* params);
void LiveCrossfadeMusicLayers(Audio_CommandParams* params);
void LiveStartMusicWithLoopCounter(Audio_CommandParams* params);
void LiveSetMusicVolume(Audio_CommandParams* params);
void LiveFadeMusicVolume(Audio_CommandParams* params);
void LiveFadeMusicVolumeFrom(Audio_CommandParams* params);
void LiveSetDiscVolume(Audio_CommandParams* params);
void LiveFadeDiscVolume(Audio_CommandParams* params);
void LiveFadeDiscVolumeFrom(Audio_CommandParams* params);
}  // namespace VS::Audio::Seq
