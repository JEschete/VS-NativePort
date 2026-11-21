#pragma once
// A sequencer channel and the music configuration it plays under, and the
// music channels and configurations: the active, primary, saved and secondary
// layers and the pending music command.
#include <cstddef>
#include <cstdint>

#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/AkaoFormat.hpp"
#include "Audio/SeqControlCommands.hpp"
#include "Audio/SeqSequencer.hpp"

namespace VS::Audio {
// decomp: FSoundChannel (src/include/sound.h:410)
struct SeqChannel {
  // The program pointers address bytecode bytes.
  std::uint8_t* program_counter;
  std::uint8_t* loop_starts[Seq::kLoopDepthCount];
  std::uint8_t* return_address;
  std::uint8_t* keymap;
  std::int16_t* vibrato_wave;
  std::int16_t* tremolo_wave;
  std::int16_t* auto_pan_wave;
  std::int32_t effect_tags;
  std::int32_t pitch_base;
  std::int32_t pitch_slide;
  std::uint32_t update_flags;
  std::int32_t effect_voice_mask;
  std::uintptr_t effect_id;
  std::int32_t effect_pitch_scale;
  std::uint32_t effect_pitch_scale_slide_step;
  std::int32_t volume;
  std::int32_t volume_slide_step;
  std::int32_t pitch_slide_step;
  std::int32_t fine_pitch_delta;
  std::int32_t priority;
  std::int32_t key_on_volume;
  std::int32_t key_on_volume_slide_step;
  std::uint16_t type;
  std::uint16_t note_length;
  std::uint16_t gate_length;
  std::uint16_t instrument_index;
  std::uint16_t effect_pan;
  std::uint16_t effect_pan_slide_length;
  std::uint16_t opcode_step_counter;
  std::uint16_t loop_iterations[Seq::kLoopDepthCount];
  std::int16_t loop_step_snapshots[Seq::kLoopDepthCount];
  std::uint16_t volume_balance;
  std::uint16_t volume_balance_slide_length;
  std::uint16_t effect_pitch_scale_slide_length;
  std::uint16_t volume_slide_length;
  std::uint16_t key_on_volume_slide_length;
  std::uint16_t effect_volume_slide_length;
  std::uint16_t pan;
  std::uint16_t pan_slide_length;
  std::uint16_t pitch_slide_ticks_left;
  std::uint16_t octave;
  std::uint16_t pitch_bend_slide_length;
  std::uint16_t portamento_key;
  std::uint16_t portamento_steps;
  std::uint16_t play_modes;
  std::int16_t vibrato_delay;
  std::uint16_t vibrato_delay_left;
  std::uint16_t vibrato_rate;
  std::uint16_t vibrato_rate_counter;
  std::uint16_t vibrato_shape;
  std::uint16_t vibrato_base;
  std::uint16_t vibrato_depth;
  std::uint16_t vibrato_depth_slide_length;
  std::uint16_t vibrato_depth_slide_step;
  std::int16_t tremolo_delay;
  std::uint16_t tremolo_delay_left;
  std::uint16_t tremolo_rate;
  std::int16_t tremolo_rate_counter;
  std::uint16_t tremolo_shape;
  std::uint16_t tremolo_depth;
  std::uint16_t tremolo_depth_slide_length;
  std::int16_t tremolo_depth_slide_step;
  std::uint16_t auto_pan_rate;
  std::uint16_t auto_pan_rate_counter;
  std::uint16_t auto_pan_shape;
  std::uint16_t auto_pan_depth;
  std::uint16_t auto_pan_depth_slide_length;
  std::int16_t auto_pan_depth_slide_step;
  std::uint16_t noise_timer;
  std::uint16_t fm_timer;
  std::uint16_t loop_depth;
  std::uint16_t random_pitch_depth;
  std::int16_t stored_note_length;
  std::int16_t fixed_note_length;
  std::int16_t volume_balance_slide_step;
  std::int16_t effect_pan_slide_step;
  std::int16_t effect_volume;
  std::int16_t effect_volume_slide_step;
  std::int16_t pan_slide_step;
  std::int16_t transpose;
  std::int16_t fine_tune;
  std::int16_t key;
  std::int16_t pitch_bend_target;
  std::int16_t portamento_transpose;
  std::int16_t vibrato_pitch;
  std::int16_t tremolo_volume;
  std::int16_t auto_pan_offset;
  VoiceParams voice;
};

// decomp: FSoundMusicContext (src/include/sound.h:446-492; was
// FSoundChannelConfig)
struct MusicConfig {
  std::int32_t status_flags;
  std::int32_t active_channels;
  std::uint32_t keyed_voices;
  std::int32_t allocated_voices;
  std::int32_t pending_key_on;
  std::uint32_t active_notes;
  std::int32_t pending_key_off;
  std::uint32_t paused_channels;
  std::int32_t tempo;
  std::int32_t tempo_slide_step;
  std::uint32_t tick_accumulator;
  std::uint32_t resume_key_on_mask;
  std::int32_t push_release_mask;
  std::int32_t sequence_control_value;
  std::uintptr_t sequence_base;
  AkaoSequenceFile* patch_table;
  std::uint16_t* keymap_table;
  KeymapEntry* note_remap_table;
  std::int32_t voice_search_start;
  std::uint32_t noise_channels;
  std::uint32_t reverb_channels;
  std::uint32_t fm_channels;
  std::int32_t reverb_depth;
  std::int32_t reverb_depth_slide_step;
  std::int32_t master_volume;
  std::int32_t master_volume_step;
  std::int16_t master_volume_slide_length;
  std::int16_t reverb_depth_slide_length;
  std::uint16_t tempo_slide_length;
  std::uint16_t music_id;
  std::uint16_t jump_threshold;
  std::uint16_t noise_clock;
  std::uint16_t timer_upper;
  std::uint16_t timer_upper_count;
  std::uint16_t timer_lower;
  std::uint16_t timer_lower_count;
  std::uint16_t timer_top;
  std::int32_t reserved80;
  std::int32_t reserved84;
  std::int32_t reserved88;
  std::int32_t reserved8c;
  std::int32_t reserved90;
  std::int32_t reserved94;
  std::int32_t reserved98;
  std::int32_t reserved9c;
};
}  // namespace VS::Audio

namespace VS::Audio::Seq {
// decomp: g_ActiveMusicChannels (g_ActiveMusicChannels@SLUS_010.40)
extern SeqChannel musicChannels[kMusicChannelCount];
// Its music_id is the decomp's D_800358FE view.
// decomp: g_PushedMusicConfig (g_PushedMusicConfig@SLUS_010.40)
extern MusicConfig savedMusicConfig;
// decomp: g_pSecondaryMusicChannels (g_pSecondaryMusicChannels@SLUS_010.40)
extern SeqChannel* secondaryMusicChannels;
// decomp: g_pSavedMousicConfig (g_pSavedMousicConfig@SLUS_010.40)
extern MusicConfig* secondaryMusicConfig;
// decomp: D_800366F0 (D_800366F0@SLUS_010.40)
extern MusicConfig primaryMusicConfig;
// decomp: D_80037788 (D_80037788@SLUS_010.40)
extern CommandParams pendingMusicData;
// decomp: g_Music_LoopCounter (g_Music_LoopCounter@SLUS_010.40)
extern std::uint32_t musicLoopsRemaining;
// decomp: g_pActiveMusicConfig (g_pActiveMusicConfig@SLUS_010.40)
extern MusicConfig* activeMusicConfig;
// decomp: D_800378E8 (D_800378E8@SLUS_010.40)
extern SeqChannel savedMusicChannels[kMusicChannelCount];
}  // namespace VS::Audio::Seq
