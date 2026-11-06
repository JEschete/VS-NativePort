#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Audio/SoundPlayback.hpp"

// Live bindings for SoundPlayback.hpp over the sound driver, the mixer's
// command arguments and dispatch, the music layers, the voice scheduler, the
// sound-effect channels, the cutscene stream's voices and the shared
// sound-effect tables. Generated callers keep the original call shapes
// (copy_fixups.py OWNED_CALL_BINDINGS); a program or sequence is read only
// within the sequence memory (SeqRecordsLive.hpp LiveProgramRegion).
namespace VS::Audio::Playback {
int LiveInitialize();
int LiveShutdown();
int LiveBindSoundEffectPrograms(void* blob);

int LiveMusicVolume(unsigned music_id);
int LivePackedMusicSampleBankIds();
int LiveMusicPosition();

unsigned LiveStartMusic(void* sequence);
void LiveStopMusic(unsigned music_id);
void LiveSaveMusicState();
int LiveInvalidateSavedMusicId();
unsigned LiveCrossfadeMusic(void* sequence, unsigned fade_ticks,
                            int target_volume);
unsigned LiveQueueMusicTransition(void* sequence);

int LivePlaySoundEffect(std::uint32_t effect_id, int effect_tag, int pan,
                        int volume);
// The program, or (uintptr_t)-1 when it is not AKAO.
std::uintptr_t LivePlaySoundEffectBlob(void* program, int effect_tag, int pan,
                                       int volume);
void LiveStopSoundEffects(std::intptr_t selector, int effect_tag_mask);
std::intptr_t LiveSoundEffectIdForTag(int effect_tag);
// func_800123C8 for a shared effect id, and for an effect program.
int LiveSharedEffectUsesTags(std::int32_t effect_id);
int LiveProgramUsesTags(void* program);
void LiveSetMonoOutput(int mono_enabled);

// Writes `arguments` to the mixer's command arguments (D_800378C0) ahead of a
// dispatch; an absent argument leaves its word as it was.
void LiveWriteCommandArguments(const CommandArguments& arguments);

// The shared sound-effect tables. The binding is their source of truth; it
// also stores them in the legacy table pointers the sound-effect commands
// still read. Tables bound one entry per effect id they cover.
struct LiveEffectTables {
  std::span<std::uint32_t> program_offsets;
  std::span<std::uint16_t> bank_selectors;
  std::span<char> program_data;
};
void BindLiveEffectTables(const LiveEffectTables& tables);
// The bound tables; empty when none are bound, or when the legacy table
// pointers no longer match the binding.
std::optional<LiveEffectTables> BoundLiveEffectTables();

// Binds `tables` for its lifetime and restores the earlier binding and legacy
// table pointers after it (tests and tools).
class ScopedLiveEffectTables {
 public:
  explicit ScopedLiveEffectTables(const LiveEffectTables& tables);
  ~ScopedLiveEffectTables();
  ScopedLiveEffectTables(const ScopedLiveEffectTables&) = delete;
  ScopedLiveEffectTables& operator=(const ScopedLiveEffectTables&) = delete;
  ScopedLiveEffectTables(ScopedLiveEffectTables&&) = delete;
  ScopedLiveEffectTables& operator=(ScopedLiveEffectTables&&) = delete;

 private:
  std::optional<LiveEffectTables> saved_binding_;
  std::uint32_t* saved_offsets_;
  std::uint16_t* saved_selectors_;
  char* saved_program_data_;
};
}  // namespace VS::Audio::Playback
