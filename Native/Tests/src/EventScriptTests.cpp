#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <span>

#include "Audio/Control/SoundControl.hpp"
#include "Audio/Cutscene/Stream.hpp"
#include "Audio/Driver/SoundDriver.hpp"
#include "Audio/MixControlLive.hpp"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/MusicState.hpp"
#include "Audio/MusicVoiceConflicts.hpp"
#include "Audio/PlaybackResetLive.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/Seq/Sequencer.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqSequencerLive.hpp"
#include "Audio/SeqVoiceCommandsLive.hpp"
#include "Audio/SfxChannelGroups.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Audio/SoundState.hpp"
#include "Audio/StreamControlLive.hpp"
#include "Audio/StreamFormat.hpp"
#include "Audio/StreamLive.hpp"
#include "Audio/VoiceMixerLive.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "DiscQueue.h"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/Runtime.hpp"
#include "Event/Script.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Field/CollisionSampling.hpp"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLightingLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Sounds.hpp"
#include "Render/Palette.hpp"
#include "TestSupport/ScriptSources.hpp"

static int sessionTitleCalls;
static int sessionBattleCalls;
static int sessionFallthrough;
static VS::Audio::CommandParams menuSoundObserved;
static int menuSoundCalls;
static int16_t savedStreamPan[256];
static VS::Audio::CommandHandler savedStreamCommands[7];
static VS::Audio::CommandHandler savedXaCommands[3];
static Content_DiscDrive savedXaDrive;
static const int kStreamCommandIds[] = {0xE0, 0xE2, 0xE4, 0xE5,
                                        0xE6, 0x9E, 0x9F};

int TestsCancelQueuedMusic(int queued, int secondary) {
  const VS::Audio::CommandParams original_pending =
      VS::Audio::Seq::pendingMusicData;
  VS::Audio::MusicConfig* original_active = VS::Audio::Seq::activeMusicConfig;
  VS::Audio::MusicConfig* original_secondary =
      VS::Audio::Seq::secondaryMusicConfig;
  VS::Audio::MusicConfig current = {0};
  VS::Audio::MusicConfig saved = {0};
  VS::Audio::AkaoSequenceFile sequence = {0};
  const VS::Audio::CommandParams pending = {
      queued ? reinterpret_cast<uintptr_t>(&sequence) : 0, 2, 3, 4, 5, 6};
  int result;
  current.status_flags = 0x10062;
  VS::Audio::Seq::activeMusicConfig = &current;
  VS::Audio::Seq::secondaryMusicConfig = secondary ? &saved : nullptr;
  VS::Audio::Seq::pendingMusicData = pending;
  VS::Audio::Mixer::LiveStopMusicAndCancelReplacement(nullptr);
  result = VS::Audio::Seq::pendingMusicData.param1 == 0;
  result |= (current.status_flags == 0x62) << 1;
  result |=
      (VS::Audio::Seq::pendingMusicData.param2 == pending.param2 &&
       VS::Audio::Seq::pendingMusicData.param3 == pending.param3 &&
       VS::Audio::Seq::pendingMusicData.param4 == pending.param4 &&
       VS::Audio::Seq::pendingMusicData.ext_param1 == pending.ext_param1 &&
       VS::Audio::Seq::pendingMusicData.ext_param2 == pending.ext_param2)
      << 2;
  result |= (!queued || reinterpret_cast<uintptr_t>(&sequence) > UINT32_MAX)
            << 3;
  VS::Audio::Mixer::LiveStopMusicAndCancelReplacement(nullptr);
  result |= (VS::Audio::Seq::pendingMusicData.param1 == 0) << 4;
  VS::Audio::Seq::pendingMusicData = original_pending;
  VS::Audio::Seq::activeMusicConfig = original_active;
  VS::Audio::Seq::secondaryMusicConfig = original_secondary;
  return result;
}

int TestsSavedMusicVoiceConflicts(void) {
  VS::Audio::MusicConfig current = {0};
  VS::Audio::MusicConfig saved = {0};
  VS::Audio::SeqChannel channels[32] = {nullptr};
  VS::Audio::MusicConfig* original_current = VS::Audio::Seq::activeMusicConfig;
  VS::Audio::MusicConfig* original_saved = VS::Audio::Seq::secondaryMusicConfig;
  VS::Audio::SeqChannel* original_channels =
      VS::Audio::Seq::secondaryMusicChannels;
  VS::Audio::VoiceScheduler original_scheduler =
      VS::Audio::Mixer::voiceScheduler;
  int original_streams = VS::Audio::Cutscene::streamState.voice_mask;
  int failures = 0;
  unsigned flags, voice, reservation, channel;
  VS::Audio::Seq::activeMusicConfig = &current;
  VS::Audio::Seq::secondaryMusicConfig = &saved;
  VS::Audio::Seq::secondaryMusicChannels = channels;
  for (flags = 0; flags < 16; ++flags) {
    const int conflict =
        (flags & 1) && (!(flags & 4) || ((flags & 2) && !(flags & 8)));
    for (voice = 0; voice < 24; ++voice) {
      const int bit = 1 << voice;
      current.allocated_voices = flags & 1 ? bit : 0;
      current.keyed_voices = flags & 2 ? bit : 0;
      saved.allocated_voices = flags & 4 ? bit : 0;
      saved.keyed_voices = flags & 8 ? bit : 0;
      for (reservation = 0; reservation < 4; ++reservation) {
        for (channel = 0; channel < 32; ++channel) {
          channels[channel].voice.assigned_voice = channel < 2 ? voice : 24;
        }
        VS::Audio::Mixer::voiceScheduler.active_voices =
            reservation & 1 ? bit : 0;
        VS::Audio::Cutscene::streamState.voice_mask = reservation & 2 ? bit : 0;
        VS::Audio::Mixer::voiceScheduler.pending_key_off = 1U << 25;
        VS::Audio::Seq::ReleaseLiveSavedVoiceConflicts();
        for (channel = 0; channel < 32; ++channel) {
          const unsigned expected = channel < 2 && !conflict ? voice : 24;
          failures += channels[channel].voice.assigned_voice != expected;
        }
        failures += VS::Audio::Mixer::voiceScheduler.pending_key_off !=
                    ((1U << 25) | (conflict && reservation == 0 ? bit : 0));
      }
    }
  }
  VS::Audio::Seq::secondaryMusicConfig = nullptr;
  VS::Audio::Mixer::voiceScheduler.pending_key_off = 1;
  VS::Audio::Seq::ReleaseLiveSavedVoiceConflicts();
  failures += VS::Audio::Mixer::voiceScheduler.pending_key_off != 1;
  VS::Audio::Seq::activeMusicConfig = original_current;
  VS::Audio::Seq::secondaryMusicConfig = original_saved;
  VS::Audio::Seq::secondaryMusicChannels = original_channels;
  VS::Audio::Mixer::voiceScheduler = original_scheduler;
  VS::Audio::Cutscene::streamState.voice_mask = original_streams;
  return failures;
}

namespace {
// A zeroed track in a game heap block; the restore reads the track only from
// the block it lies in.
struct HeapSequenceFile {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 16> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  VS::Audio::AkaoSequenceFile* file = static_cast<VS::Audio::AkaoSequenceFile*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(
          sizeof(VS::Audio::AkaoSequenceFile)));

  HeapSequenceFile() { memset(file, 0, sizeof(*file)); }
};
}  // namespace

int TestsNativeMusicSnapshot(void) {
  const VS::Audio::MusicConfig original_primary =
      VS::Audio::Seq::primaryMusicConfig;
  const VS::Audio::MusicConfig original_saved =
      VS::Audio::Seq::savedMusicConfig;
  VS::Audio::MusicConfig* original_active = VS::Audio::Seq::activeMusicConfig;
  VS::Audio::MusicConfig* original_secondary =
      VS::Audio::Seq::secondaryMusicConfig;
  const VS::Audio::VoiceScheduler original_scheduler =
      VS::Audio::Mixer::voiceScheduler;
  const unsigned original_updates =
      VS::Audio::Driver::controlLatches.update_flags;
  VS::Audio::SeqChannel original_channels[32];
  VS::Audio::SeqChannel original_saved_channels[32];
  HeapSequenceFile sequence;
  u_short keymap = 0;
  unsigned channel;
  int result, swap;
  memcpy(original_channels, VS::Audio::Seq::musicChannels,
         sizeof(original_channels));
  memcpy(original_saved_channels, VS::Audio::Seq::savedMusicChannels,
         sizeof(original_saved_channels));
  memset(&VS::Audio::Seq::primaryMusicConfig, 0,
         sizeof(VS::Audio::Seq::primaryMusicConfig));
  memset(&VS::Audio::Seq::savedMusicConfig, 0,
         sizeof(VS::Audio::Seq::savedMusicConfig));
  memset(VS::Audio::Seq::musicChannels, 0,
         sizeof(VS::Audio::Seq::musicChannels));
  memset(VS::Audio::Seq::savedMusicChannels, 0,
         sizeof(VS::Audio::Seq::savedMusicChannels));
  VS::Audio::Seq::activeMusicConfig = &VS::Audio::Seq::primaryMusicConfig;
  VS::Audio::Seq::primaryMusicConfig.active_channels = 1;
  VS::Audio::Seq::primaryMusicConfig.music_id = 0x1234;
  VS::Audio::Seq::primaryMusicConfig.timer_top = 0x4321;
  for (channel = 0; channel < 32; ++channel) {
    VS::Audio::Seq::musicChannels[channel].voice.assigned_voice = channel % 24;
    VS::Audio::Seq::musicChannels[channel].voice.dirty_flags =
        0x123400 + channel;
  }
  VS::Audio::Seq::LiveSaveCurrentMusic(nullptr);
  result = VS::Audio::Seq::savedMusicConfig.active_channels == 1 &&
           VS::Audio::Seq::savedMusicConfig.music_id == 0x1234 &&
           VS::Audio::Seq::savedMusicConfig.timer_top == 0x4321;
  {
    int channels_saved = 1;
    for (channel = 0; channel < 32; ++channel) {
      channels_saved &=
          VS::Audio::Seq::savedMusicChannels[channel].voice.assigned_voice ==
              channel % 24 &&
          VS::Audio::Seq::savedMusicChannels[channel].voice.dirty_flags ==
              0x123400 + channel;
    }
    result |= channels_saved << 1;
  }
  VS::Audio::Seq::secondaryMusicConfig = nullptr;
  for (swap = 0; swap < 2; ++swap) {
    int restored;
    memset(&VS::Audio::Seq::savedMusicConfig, 0,
           sizeof(VS::Audio::Seq::savedMusicConfig));
    memset(&VS::Audio::Seq::primaryMusicConfig, 0,
           sizeof(VS::Audio::Seq::primaryMusicConfig));
    VS::Audio::Seq::primaryMusicConfig.timer_top = 0x1111;
    VS::Audio::Seq::savedMusicConfig.timer_top = 0x2222;
    VS::Audio::Seq::savedMusicConfig.sequence_base =
        reinterpret_cast<uintptr_t>(sequence.file);
    VS::Audio::Seq::savedMusicConfig.patch_table = sequence.file;
    VS::Audio::Seq::savedMusicConfig.keymap_table = &keymap;
    for (channel = 0; channel < 32; ++channel) {
      VS::Audio::Seq::savedMusicChannels[channel].voice.dirty_flags =
          0x100 + channel;
      VS::Audio::Seq::musicChannels[channel].voice.dirty_flags =
          0x200 + channel;
    }
    VS::Audio::Seq::LiveRestoreMusicSequence(sequence.file, swap);
    restored = VS::Audio::Seq::primaryMusicConfig.timer_top == 0x2222;
    for (channel = 0; channel < 32; ++channel) {
      restored &= VS::Audio::Seq::musicChannels[channel].voice.dirty_flags ==
                  0x100 + channel;
      if (swap) {
        restored &=
            VS::Audio::Seq::savedMusicChannels[channel].voice.dirty_flags ==
            0x200 + channel;
      }
    }
    if (swap) restored &= VS::Audio::Seq::savedMusicConfig.timer_top == 0x1111;
    result |= restored << (swap + 2);
  }
  VS::Audio::Seq::primaryMusicConfig = original_primary;
  VS::Audio::Seq::savedMusicConfig = original_saved;
  VS::Audio::Seq::activeMusicConfig = original_active;
  VS::Audio::Seq::secondaryMusicConfig = original_secondary;
  VS::Audio::Mixer::voiceScheduler = original_scheduler;
  VS::Audio::Driver::controlLatches.update_flags = original_updates;
  memcpy(VS::Audio::Seq::musicChannels, original_channels,
         sizeof(original_channels));
  memcpy(VS::Audio::Seq::savedMusicChannels, original_saved_channels,
         sizeof(original_saved_channels));
  return result;
}

static int CheckPendingModulationSlides(VS::Audio::SeqChannel* channel,
                                        int effect, int kind, int16_t* wave) {
  std::uint8_t slide[] = {4, 64};
  std::uint8_t enable[] = {0, 2, 0};
  int failures = 0;
  int index;
  channel->program_counter = slide;
  channel->volume = 0x40000000;
  channel->volume_balance = 0x7F00;
  channel->pitch_base = 0x1000;
  VS::Audio::Seq::kLfoWaveforms[0] = wave;
  switch (kind) {
    case 0:
      VS::Audio::Seq::LiveSlideVibratoDepth(channel, 1);
      break;
    case 1:
      VS::Audio::Seq::LiveSlideTremoloDepth(channel, 1);
      break;
    case 2:
      VS::Audio::Seq::LiveSlideAutoPanDepth(channel, 1);
      break;
    default:
      fprintf(stderr, "unexpected modulation test kind: %d\n", kind);
      abort();
  }
  for (index = 1; index <= 4; ++index) {
    VS::Audio::Mixer::LiveUpdateChannelSlidesAndDelays(
        channel, 1,
        effect ? VS::Audio::Driver::ChannelKind::kSoundEffect
               : VS::Audio::Driver::ChannelKind::kMusic);
    const unsigned depth = kind == 0   ? channel->vibrato_depth
                           : kind == 1 ? channel->tremolo_depth
                                       : channel->auto_pan_depth;
    const unsigned length = kind == 0   ? channel->vibrato_depth_slide_length
                            : kind == 1 ? channel->tremolo_depth_slide_length
                                        : channel->auto_pan_depth_slide_length;
    failures |=
        (depth != static_cast<unsigned>(index * (kind == 2 ? 2048 : 4096)) ||
         length != static_cast<unsigned>(4 - index) ||
         channel->vibrato_pitch != 0 || channel->tremolo_volume != 0 ||
         channel->auto_pan_offset != 0 || channel->update_flags != 0)
        << 2;
  }
  channel->program_counter = enable;
  switch (kind) {
    case 0:
      VS::Audio::Seq::LiveEnableVibrato(channel, 1);
      break;
    case 1:
      VS::Audio::Seq::LiveEnableTremolo(channel, 1);
      break;
    case 2:
      channel->program_counter = enable + 1;
      VS::Audio::Seq::LiveEnableAutoPan(channel, 1);
      break;
    default:
      fprintf(stderr, "unexpected modulation test kind: %d\n", kind);
      abort();
  }
  if (effect) {
    VS::Audio::Mixer::LiveUpdateSoundEffectModulation(channel);
  } else {
    VS::Audio::Mixer::LiveUpdateMusicChannelModulation(channel);
  }
  failures |=
      ((kind == 0 &&
        (channel->vibrato_wave != wave + 1 || channel->vibrato_pitch != 60)) ||
       (kind == 1 && (channel->tremolo_wave != wave + 1 ||
                      channel->tremolo_volume != 4064)) ||
       (kind == 2 &&
        (channel->auto_pan_wave != wave + 1 || channel->auto_pan_offset != 16)))
      << 3;
  return failures;
}

int TestsPendingModulationSlides(int effect, int kind) {
  VS::Audio::SeqChannel saved_music[32];
  VS::Audio::SeqChannel saved_effects[12];
  int16_t wave[] = {16384, 8192, 0, 0, -2};
  int16_t* saved_wave = VS::Audio::Seq::kLfoWaveforms[0];
  VS::Audio::SeqChannel* channel;
  int failures = 0;
  int index;
  memcpy(saved_music, VS::Audio::Seq::musicChannels, sizeof(saved_music));
  memcpy(saved_effects, VS::Audio::SoundEffects::channels,
         sizeof(saved_effects));
  memset(VS::Audio::Seq::musicChannels, 0, sizeof(saved_music));
  memset(VS::Audio::SoundEffects::channels, 0, sizeof(saved_effects));
  VS::Audio::Driver::ResetLivePlaybackState();
  for (index = 0; index < 44; ++index) {
    channel = index < 32 ? &VS::Audio::Seq::musicChannels[index]
                         : &VS::Audio::SoundEffects::channels[index - 32];
    failures |= channel->vibrato_rate_counter != 1 ||
                channel->tremolo_rate_counter != 1 ||
                channel->auto_pan_rate_counter != 1;
    failures |=
        (channel->vibrato_wave != nullptr || channel->tremolo_wave != nullptr ||
         channel->auto_pan_wave != nullptr)
        << 1;
  }
  channel = effect ? VS::Audio::SoundEffects::channels
                   : VS::Audio::Seq::musicChannels;
  if (failures == 0) {
    failures |= CheckPendingModulationSlides(channel, effect, kind, wave);
    channel->vibrato_wave = wave + 1;
    channel->tremolo_wave = wave + 1;
    channel->auto_pan_wave = wave + 1;
    channel->vibrato_rate_counter = 4;
    channel->tremolo_rate_counter = 5;
    channel->auto_pan_rate_counter = 6;
    VS::Audio::Driver::ResetLivePlaybackState();
    failures |= (channel->vibrato_wave != wave + 1 ||
                 channel->tremolo_wave != wave + 1 ||
                 channel->auto_pan_wave != wave + 1 ||
                 channel->vibrato_rate_counter != 4 ||
                 channel->tremolo_rate_counter != 5 ||
                 channel->auto_pan_rate_counter != 6)
                << 4;
    channel->vibrato_wave = nullptr;
    channel->tremolo_wave = nullptr;
    channel->auto_pan_wave = nullptr;
    channel->vibrato_rate_counter = 0;
    channel->tremolo_rate_counter = 0;
    channel->auto_pan_rate_counter = 0;
    VS::Audio::Driver::ResetLivePlaybackState();
    failures |= (channel->vibrato_rate_counter != 1 ||
                 channel->tremolo_rate_counter != 1 ||
                 channel->auto_pan_rate_counter != 1)
                << 5;
  }
  VS::Audio::Seq::kLfoWaveforms[0] = saved_wave;
  memcpy(VS::Audio::Seq::musicChannels, saved_music, sizeof(saved_music));
  memcpy(VS::Audio::SoundEffects::channels, saved_effects,
         sizeof(saved_effects));
  return failures;
}

void TestsPrepareXaPlayback(void) {
  const VS::Audio::CommandHandler handlers[] = {
      VS::Audio::Seq::LiveSetDiscVolume, VS::Audio::Seq::LiveFadeDiscVolume,
      VS::Audio::Seq::LiveFadeDiscVolumeFrom};
  int index;
  savedXaDrive = Content_discDriveState;
  VS::Audio::Driver::ResetLivePlaybackState();
  memset(&Content_discDriveState, 0, sizeof(Content_discDriveState));
  for (index = 0; index < 3; ++index) {
    savedXaCommands[index] = VS::Audio::Mixer::kCommandHandlers[0xC8 + index];
    VS::Audio::Mixer::kCommandHandlers[0xC8 + index] = handlers[index];
  }
}

void TestsRestoreXaPlayback(void) {
  int index;
  VS::Audio::Driver::ResetVoiceOutput();
  Content_discDriveState = savedXaDrive;
  for (index = 0; index < 3; ++index) {
    VS::Audio::Mixer::kCommandHandlers[0xC8 + index] = savedXaCommands[index];
  }
}

int TestsXaCommand(int command, int first, int second, int third) {
  switch (command) {
    case 0:
      return VS::Audio::LiveBeginXaStream(first, second);
    case 1:
      VS::Content::UpdateLiveDiscCommands();
      return Content_discDriveState.state;
    case 2:
      return VS::Audio::LiveRequestXaStreamStop();
    case 3:
      VS::Audio::Control::LiveSetDiscVolume(first);
      break;
    case 4:
      VS::Audio::Control::LiveFadeDiscVolume(first, second);
      break;
    case 5:
      VS::Audio::Control::LiveFadeDiscVolumeFrom(first, second, third);
      break;
    case 6:
      return static_cast<std::int16_t>(VS::Audio::Seq::discVolume >> 16);
    case 7:
      VS::Audio::Driver::ResetVoiceOutput();
      break;
    case 8:
      VS::Content::ResetLiveDiscDrive();
      break;
    case 9:
      return Content_discDriveState.failureCount;
    default:
      fprintf(stderr, "unexpected XA test command: %d\n", command);
      abort();
  }
  return Content_discDriveState.state;
}

void TestsPrepareStreamPlayback(void) {
  VS::Audio::CommandHandler handlers[] = {
      VS::Audio::Cutscene::ReplaceLiveStream,
      VS::Audio::Cutscene::StopLiveStream,
      VS::Audio::Cutscene::SetLiveStreamVolume,
      VS::Audio::Cutscene::SlideLiveStreamVolume,
      VS::Audio::Cutscene::SetLiveStreamPan,
      VS::Audio::Cutscene::ResumeLiveStream,
      VS::Audio::Cutscene::PauseLiveStream};
  int index;
  VS::Audio::Driver::ResetLivePlaybackState();
  memcpy(savedStreamPan, VS::Audio::Mixer::kPanGains, sizeof(savedStreamPan));
  memset(VS::Audio::Mixer::kPanGains, 0, sizeof(savedStreamPan));
  VS::Audio::Mixer::kPanGains[0] = 32767;
  VS::Audio::Mixer::kPanGains[127] = 23170;
  VS::Audio::Mixer::kPanGains[128] = 23170;
  for (index = 0; index < 7; ++index) {
    savedStreamCommands[index] =
        VS::Audio::Mixer::kCommandHandlers[kStreamCommandIds[index]];
    VS::Audio::Mixer::kCommandHandlers[kStreamCommandIds[index]] =
        handlers[index];
  }
}

void TestsRestoreStreamPlayback(void) {
  int index;
  VS::Audio::Driver::ResetVoiceOutput();
  memcpy(VS::Audio::Mixer::kPanGains, savedStreamPan, sizeof(savedStreamPan));
  for (index = 0; index < 7; ++index) {
    VS::Audio::Mixer::kCommandHandlers[kStreamCommandIds[index]] =
        savedStreamCommands[index];
  }
}

int TestsBindEffectStream(void* bytes, size_t size) {
  VS::Effects::LiveEffectPackage package = {0};
  VS::Effects::LiveEffectPackage* saved = VS::Effects::package;
  VS::Effects::package = &package;
  static_cast<void>(VS::Effects::BindLiveEffectPackage(
      std::span(static_cast<std::byte*>(bytes), size)));
  VS::Effects::package = saved;
  return package.auxiliary_sound_handle;
}

int TestsPlayStreamSlot(int slot, int pan, int volume) {
  return VS::Audio::LivePlayCutsceneStreamSlot(slot, pan, volume);
}

void TestsAdvanceStreamTick(void) {
  VS::Audio::Seq::LiveUpdateGlobalSlides();
  VS::Audio::Mixer::LiveCommitPlaybackVoices();
  VS::Audio::Driver::TickVoiceOutput();
}

void TestsStreamControl(int command, int value, int target) {
  switch (command) {
    case 0:
      VS::Audio::Control::LiveSetCutsceneVolume(value);
      break;
    case 1:
      VS::Audio::Control::LiveFadeCutsceneVolume(value, target);
      break;
    case 2:
      VS::Audio::Control::LiveSetCutscenePan(value);
      break;
    case 3:
      VS::Audio::Mixer::LiveDispatchCommand(value ? 0x9F : 0x9E);
      break;
    case 4:
      VS::Audio::Control::LiveStopCutsceneStream();
      break;
    case 5:
      VS::Audio::Driver::ResetLivePlaybackState();
      break;
    case 6:
      VS::Audio::LiveResetSoundSlots();
      break;
    default:
      fprintf(stderr, "unexpected stream test command: %d\n", command);
      abort();
  }
}

unsigned TestsStreamVoiceMask(void) {
  return VS::Audio::Cutscene::streamState.voice_mask;
}

unsigned TestsStreamVolume(void) {
  return VS::Audio::Cutscene::streamState.volume;
}

int TestsStreamSlotIsEmpty(void) {
  return VS::Audio::soundState.sfx_stream_data[2] == nullptr;
}

static void CaptureMenuSound(VS::Audio::CommandParams* params) {
  menuSoundObserved = *params;
  ++menuSoundCalls;
}

int TestsMenuSoundDispatch(void) {
  u_int offsets[12] = {0};
  u_short selectors[12] = {0};
  const VS::Audio::CommandHandler saved_handler =
      VS::Audio::Mixer::kCommandHandlers[0x20];
  const VS::Audio::LegacyCommandArguments saved_arguments =
      VS::Audio::Driver::commandArguments;
  const VS::Audio::CommandParams saved_params = VS::Audio::Mixer::commandParams;
  const int saved_group = VS::Audio::nextSfxChannelGroup7E;
  void (*const play[])(void) = {
      VS::Menu::PlayOpenSound, VS::Menu::PlayInvalidSound,
      VS::Menu::PlayLeaveSound, VS::Menu::PlaySelectSound,
      VS::Menu::PlayChangeSound};
  const int ids[] = {10, 7, 6, 5, 4};
  int result = 0;
  int index;
  const VS::Audio::Playback::ScopedLiveEffectTables tables{
      {.program_offsets = offsets,
       .bank_selectors = selectors,
       .program_data = {}}};
  VS::Audio::Mixer::kCommandHandlers[0x20] = CaptureMenuSound;
  VS::Audio::nextSfxChannelGroup7E = 2;
  for (index = 0; index < 5; ++index) {
    selectors[ids[index]] = 0x8000;
    menuSoundCalls = 0;
    play[index]();
    if (menuSoundCalls == 1 &&
        menuSoundObserved.param1 == static_cast<uintptr_t>(ids[index]) &&
        menuSoundObserved.param2 == (2 << index) &&
        menuSoundObserved.param3 == 0x80 && menuSoundObserved.param4 == 0x7F) {
      result |= 1 << index;
    }
  }
  VS::Audio::Mixer::kCommandHandlers[0x20] = saved_handler;
  VS::Audio::Driver::commandArguments = saved_arguments;
  VS::Audio::Mixer::commandParams = saved_params;
  VS::Audio::nextSfxChannelGroup7E = saved_group;
  return result;
}

void TestsNativeNoiseChannel(void) {
  VS::Audio::SeqChannel channel = {nullptr};
  channel.voice.sample_start = 0x1010;
  channel.voice.sample_rate = 4096;
  channel.voice.volume.left = 0x1000;
  channel.voice.volume.right = 0x1000;
  channel.voice.adsr_lower = 0x00FF;
  channel.voice.adsr_upper = 0x5FC4;
  VS::Audio::Driver::ResetLivePlaybackState();
  VS::Audio::Driver::ConfigureOutputChannel(&channel);
  VS::Audio::Driver::SetOutputNoiseClock(63);
  VS::Audio::Driver::SetLiveVoiceNoiseMask(1);
  VS::Audio::Driver::SetOutputReverbMode(4);
  VS::Audio::Driver::SetOutputReverbDepth(0x3FFF, 0x3FFF);
  VS::Audio::Driver::EnableOutputReverb(1);
  VS::Audio::Driver::SetLiveVoiceReverbMask(1);
  VS::Audio::Driver::KeyOnLiveVoices(1);
  VS::Audio::Driver::TickVoiceOutput();
}

void TestsNativeMonoOutput(int enabled) {
  if (enabled) {
    VS::Audio::Mixer::LiveEnableMonoOutput(nullptr);
  } else {
    VS::Audio::Mixer::LiveEnableStereoOutput(nullptr);
  }
  VS::Audio::Driver::TickVoiceOutput();
}

int TestsZoneMusicCanonicalState(void) {
  const int saved_id = VS::Field::zoneContext.music_id;
  const int saved_slot = VS::Field::zoneContext.music_slot;
  const int saved_enabled = VS::Field::zoneContext.music_fade_enabled;
  int passed = 0;
  VS::Field::zoneContext.music_id = 16;
  VS::Field::zoneContext.music_slot = 3;
  VS::Field::zoneContext.music_fade_enabled = 1;
  if (VS::Field::LiveZoneSoundFileId() == 16) passed |= 1;
  VS::Field::zoneContext.music_id = 17;
  if (VS::Field::LiveZoneSoundFileId() == 17) passed |= 2;
  if (VS::Field::zoneContext.music_slot == 3) passed |= 4;
  VS::Audio::LiveSetZoneMusicSwitchEnabled(0);
  if (VS::Field::zoneContext.music_fade_enabled == 0) passed |= 8;
  VS::Audio::LiveSetZoneMusicSwitchEnabled(1);
  if (VS::Field::zoneContext.music_fade_enabled == 1) passed |= 16;
  if (VS::Field::zoneContext.music_id == 17 &&
      VS::Field::zoneContext.music_slot == 3) {
    passed |= 32;
  }
  VS::Field::zoneContext.music_id = saved_id;
  VS::Field::zoneContext.music_slot = saved_slot;
  VS::Field::zoneContext.music_fade_enabled = saved_enabled;
  return passed;
}

int Tests_RoomLightingSamples(void) {
  struct {
    unsigned ambient, floorCount, ceilingCount;
    struct {
      int16_t bounds[4];
      int16_t points[3][2];
      unsigned colors[3];
    } triangles[2];
  } lighting = {0x00332211,
                1,
                1,
                {{{0, 0, 128, 128},
                  {{0, 0}, {0, 128}, {128, 0}},
                  {0x0000FF, 0xFF0000, 0x00FF00}},
                 {{0, 0, 128, 128},
                  {{0, 0}, {0, 128}, {128, 0}},
                  {0x806040, 0x806040, 0x806040}}}};
  const Field_RoomRecord saved = Field_roomData;
  Field_roomData.isLoaded = 1;
  Field_roomData.lighting = &lighting;
  Field_roomData.header.lightingLength = sizeof lighting;
  VS::Render::ColorCode color = {0, 0, 0, 0x5A};
  VS::Field::SampleLiveRoomLighting((32 << 16) | 32, 0, &color);
  int passed = color.red == 127 && color.green == 63 && color.blue == 63 &&
               color.code == 0x5A;
  VS::Field::SampleLiveRoomLighting((32 << 16) | 32, 1, &color);
  passed = passed && color.red == 64 && color.green == 96 && color.blue == 128;
  VS::Field::SampleLiveRoomLighting((200 << 16) | 200, 0, &color);
  passed = passed && color.red == 17 && color.green == 34 && color.blue == 51 &&
           color.code == 0;
  Field_roomData.lighting = nullptr;
  VS::Field::SampleLiveRoomLighting(0, 0, &color);
  passed =
      passed && color.red == 128 && color.green == 128 && color.blue == 128;
  Field_roomData = saved;
  return passed;
}

int Tests_CollisionHeightPatterns(void) {
  struct {
    Field_MpdCollisionHeader header;
    uint16_t floor[2];
    uint16_t ceiling[2];
    signed char patterns[2][16];
  } collision = {{2, 1, 0, 2}, {0x0800, 0x1801}, {0x5001, 0xF800}, {{0}}};
  collision.patterns[0][0] = -2;
  collision.patterns[0][1] = 5;
  collision.patterns[0][4] = 7;
  collision.patterns[1][0] = 8;
  collision.patterns[1][15] = -16;
  const Field_RoomRecord saved = Field_roomData;
  memset(&Field_roomData, 0, sizeof Field_roomData);
  Field_roomData.collisionMap = &collision.header;
  Field_roomData.header.collisionMapLength = sizeof collision;
  const auto sample = [](uint32_t x, int z, int layer) {
    return VS::Field::SampleCollisionHeightCode(
        VS::Field::LiveRoomCollisionMap(), x, z, layer);
  };
  int passed = sample(0, 0, 0) == 0x82 && sample(1, 0, 0) == 9 &&
               sample(0, 1, 0) == 11 && sample(4, 0, 0) == 20 &&
               sample(7, 3, 0) == 0xFC && sample(0, 0, 1) == 48 &&
               sample(4, 0, 1) == 0xFA && sample(8, 0, 0) == 0x80 &&
               sample(0, 4, 0) == 0x80 && sample(0, -1, 0) == 0x80 &&
               sample(0, 0, 2) == 0x80;
  Field_roomData.header.collisionMapLength--;
  passed = passed && sample(7, 3, 0) == 0x80;
  Field_roomData = saved;
  return passed;
}

int Tests_AudioModesWithoutSavedMusic(void) {
  VS::Audio::MusicConfig primary = {0};
  VS::Audio::MusicConfig* saved_primary = VS::Audio::Seq::activeMusicConfig;
  VS::Audio::MusicConfig* saved_secondary =
      VS::Audio::Seq::secondaryMusicConfig;
  const VS::Audio::DriverFlags saved_driver = VS::Audio::Driver::controlLatches;
  const VS::Audio::VoiceScheduler saved_scheduler =
      VS::Audio::Mixer::voiceScheduler;
  const VS::Audio::VoiceModeMasks saved_modes = VS::Audio::Mixer::voiceModes;
  const std::int32_t saved_stream = VS::Audio::Cutscene::streamState.voice_mask;
  VS::Audio::Seq::activeMusicConfig = &primary;
  VS::Audio::Seq::secondaryMusicConfig = nullptr;
  memset(&VS::Audio::Mixer::voiceScheduler, 0,
         sizeof VS::Audio::Mixer::voiceScheduler);
  VS::Audio::Cutscene::streamState.voice_mask = 0;
  VS::Audio::Driver::controlLatches.update_flags = 0x100;
  VS::Audio::Mixer::LiveCommitPlaybackVoices();
  const int passed =
      VS::Audio::Mixer::voiceModes.noise == 0 &&
      VS::Audio::Mixer::voiceModes.reverb == 0 &&
      VS::Audio::Mixer::voiceModes.fm == 0 &&
      (VS::Audio::Driver::controlLatches.update_flags & 0x100) == 0;
  VS::Audio::Seq::activeMusicConfig = saved_primary;
  VS::Audio::Seq::secondaryMusicConfig = saved_secondary;
  VS::Audio::Mixer::voiceScheduler = saved_scheduler;
  VS::Audio::Mixer::voiceModes = saved_modes;
  VS::Audio::Cutscene::streamState.voice_mask = saved_stream;
  VS::Audio::Driver::SetLiveVoiceReverbMask(saved_modes.reverb);
  VS::Audio::Driver::SetLiveVoiceNoiseMask(saved_modes.noise);
  VS::Audio::Driver::SetLiveVoiceFrequencyModulationMask(saved_modes.fm);
  VS::Audio::Driver::controlLatches = saved_driver;
  return passed;
}

static int TestTitleSession(void) {
  ++sessionTitleCalls;
  if (sessionTitleCalls == 1) {
    VS::Boot::JumpToLiveTitle();
    ++sessionFallthrough;
  }
  if (sessionTitleCalls == 2) {
    VS::Boot::JumpToLiveGameLoop();
    ++sessionFallthrough;
  }
  return 17;
}

static int TestBattleSession(void) {
  ++sessionBattleCalls;
  VS::Boot::JumpToLiveTitle();
  ++sessionFallthrough;
  return 0;
}

int Tests_NativeSessionTransitions(void) {
  sessionTitleCalls = sessionBattleCalls = sessionFallthrough = 0;
  int passed =
      VS::Boot::RunLiveSessionLoop(TestTitleSession, TestBattleSession) == 17 &&
      sessionTitleCalls == 3 && sessionBattleCalls == 1 &&
      sessionFallthrough == 0;
  passed =
      passed &&
      VS::Boot::RunLiveSessionLoop(TestTitleSession, TestBattleSession) == 17 &&
      sessionTitleCalls == 4;
  return passed;
}

// 573B8.c:67 mixes control codes with addresses; every dispatch boundary must
// retain native pointer width.
using TestOpcodeHandler = intptr_t (*)(u_char*, int16_t);
static_assert(_Generic(VS::Event::opcodeHandlers[0],
                  TestOpcodeHandler: 1,
                  default: 0),
              "normal opcode ABI");
static_assert(_Generic(VS::Event::skipOpcodeHandlers[0],
                  TestOpcodeHandler: 1,
                  default: 0),
              "skip opcode ABI");
static_assert(sizeof(VS::Event::skipOpcodeHandlers) ==
                  256 * sizeof(TestOpcodeHandler),
              "complete skip table");

int Tests_EventScriptNativeBranches(int skip) {
  const VS::test_support::LiveScriptRuntimeScope runtime;
  std::array<TestOpcodeHandler, 4> saved_normal{};
  std::array<TestOpcodeHandler, 4> saved_skip_handlers{};
  std::array<u_char, 4> saved_sizes{};
  constexpr std::array<u_char, 4> kOpcodeSizes{3, 1, 1, 1};
  TestOpcodeHandler skipCopy[256];
  char saved_skip = VS::Event::skipState;
  const auto script =
      VS::test_support::MountLiveEventScript({0, 4, 0, 0, 1, 0, 0, 0, 2, 3});
  auto& host = VS::Event::LiveScriptVm();
  int passed;
  std::copy_n(VS::Event::opcodeHandlers, saved_normal.size(),
              saved_normal.begin());
  std::copy_n(VS::Event::skipOpcodeHandlers, saved_skip_handlers.size(),
              saved_skip_handlers.begin());
  std::copy_n(VS::Event::opcodeSizes, saved_sizes.size(), saved_sizes.begin());
  VS::Core::Memory::CopyLiveAlignedWords(
      skipCopy, VS::Event::skipOpcodeHandlers, sizeof skipCopy);
  passed = std::equal(std::begin(skipCopy), std::end(skipCopy),
                      VS::Event::skipOpcodeHandlers);
  VS::Event::skipOpcodeHandlers[0] = VS::Event::opcodeHandlers[0] =
      VS::Event::ScriptJumpForwardOpcode;
  VS::Event::skipOpcodeHandlers[1] = VS::Event::opcodeHandlers[1] =
      VS::Event::ScriptReturnToSavedAddressOpcode;
  VS::Event::skipOpcodeHandlers[2] = VS::Event::opcodeHandlers[2] =
      VS::Event::ScriptYieldFrameOpcode;
  VS::Event::skipOpcodeHandlers[3] = VS::Event::opcodeHandlers[3] =
      VS::Event::CommandStopScriptContextOpcode;
  VS::Event::skipState = skip ? 2 : 0;
  host.SetSubroutineReturn({script.region, 8, script.generation});
  host.SetThreadCursor(1, script);
  std::copy(kOpcodeSizes.begin(), kOpcodeSizes.end(), VS::Event::opcodeSizes);
  // Forward jump, saved-address return, advance-and-yield, then thread
  // termination.
  passed &= !VS::Event::ExecuteLiveScriptThread(1) &&
            host.ThreadCursor(1) ==
                VS::Event::ScriptLocation{script.region, 9, script.generation};
  passed &=
      !VS::Event::ExecuteLiveScriptThread(1) && !host.ThreadCursor(1).Present();
  std::copy(saved_normal.begin(), saved_normal.end(),
            VS::Event::opcodeHandlers);
  std::copy(saved_skip_handlers.begin(), saved_skip_handlers.end(),
            VS::Event::skipOpcodeHandlers);
  std::copy(saved_sizes.begin(), saved_sizes.end(), VS::Event::opcodeSizes);
  VS::Event::skipState = saved_skip;
  return passed;
}

int Tests_EventScriptBranchHandlers(void) {
  const VS::test_support::LiveScriptRuntimeScope runtime;
  std::int8_t saved_flag = VS::Event::cachedFlagValue;
  const char saved_skip = VS::Event::skipState;
  std::array<TestOpcodeHandler, 2> saved_handlers{};
  std::array<u_char, 2> saved_sizes{};
  std::copy_n(VS::Event::opcodeHandlers + 0xE0, saved_handlers.size(),
              saved_handlers.begin());
  std::copy_n(VS::Event::opcodeSizes + 0xE0, saved_sizes.size(),
              saved_sizes.begin());
  u_char conditional[] = {0, 7, 4, 0, 0};
  u_char backward[] = {0, 0, 0, 0, 0, 0xFF, 4, 0};
  int passed;
  auto& host = VS::Event::LiveScriptVm();
  VS::Event::cachedFlagValue = 7;
  passed = VS::Event::ScriptBranchOnCachedFlag({conditional, {}, 1}, host) ==
           VS::Event::ScriptFlow::JumpBy(4);
  VS::Event::cachedFlagValue = 6;
  passed &=
      VS::Event::ScriptBranchOnCachedFlag({conditional, {}, 1}, host).kind ==
      VS::Event::ScriptFlowKind::kAdvance;
  passed &=
      VS::Event::ScriptLoopBackward({std::span{backward}.subspan(4), {}, 1},
                                    host) == VS::Event::ScriptFlow::JumpBy(-4);
  const auto context =
      VS::test_support::MountLiveEventScript({0xE0, 2, 5, 0, 0xF0, 0xE1, 2});
  VS::Event::skipState = 0;
  VS::Event::opcodeHandlers[0xE0] = VS::Event::CommandStartScriptContextOpcode;
  VS::Event::opcodeHandlers[0xE1] = VS::Event::CommandWaitScriptContextOpcode;
  VS::Event::opcodeSizes[0xE0] = 4;
  VS::Event::opcodeSizes[0xE1] = 2;
  host.SetThreadCursor(1, context);
  passed &= !VS::Event::ExecuteLiveScriptThread(1) &&
            host.ThreadCursor(1) == VS::Event::ScriptLocation{
                                        context.region, 5, context.generation};
  passed &= host.ThreadCursor(2) ==
            VS::Event::ScriptLocation{context.region, 4, context.generation};
  std::copy(saved_handlers.begin(), saved_handlers.end(),
            VS::Event::opcodeHandlers + 0xE0);
  std::copy(saved_sizes.begin(), saved_sizes.end(),
            VS::Event::opcodeSizes + 0xE0);
  VS::Event::skipState = saved_skip;
  VS::Event::cachedFlagValue = saved_flag;
  return passed;
}
