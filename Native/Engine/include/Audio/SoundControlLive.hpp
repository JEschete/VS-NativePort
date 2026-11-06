#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

// Live bindings for SoundControl.hpp over the command arguments
// (D_800378C0), the mixer's dispatch, the driver flags, the voice scheduler
// and the sound-effect channels. Generated and owned callers keep the
// original call shapes (copy_fixups.py OWNED_CALL_BINDINGS); a request with
// no caller has no binding.
namespace VS::Audio::Control {
void LiveSetMusicBranchThreshold(void* threshold);
void LiveSetMusicVolume(std::uint32_t music_id, int volume);
void LiveFadeMusicVolume(std::uint32_t music_id, int fade_ticks,
                         int target_volume);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveFadeSoundEffectVolume(std::intptr_t effect, int tag_mask,
                               int fade_ticks, int target_volume);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveFadeSoundEffectPan(std::intptr_t effect, int tag_mask, int fade_ticks,
                            int target_pan);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveFadeSoundEffectPitch(std::intptr_t effect, int tag_mask,
                              int fade_ticks, int target_pitch_scale);
void LiveSetDiscVolume(int volume);
void LiveFadeDiscVolume(int fade_ticks, int target_volume);
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveFadeDiscVolumeFrom(int fade_ticks, int start_volume,
                            int target_volume);
void LiveFadeMusicTempoScale(int fade_ticks, int target_tempo_scale);
int LiveFadeMusicPitchScale(int fade_ticks, int target_pitch_scale);
void LiveStopAllMusic();
void LiveStopSceneSoundEffects();
void LiveStartCutsceneStream(void* stream, int pan, int reverb);
void LiveStopCutsceneStream();
void LiveSetCutsceneVolume(int volume);
void LiveFadeCutsceneVolume(int fade_ticks, int target_volume);
void LiveSetCutscenePan(int pan);
// Fills twelve effect ids; a null `effects` is refused and returns 0.
unsigned LiveSoundEffectChannelIds(std::intptr_t* effects);
}  // namespace VS::Audio::Control
