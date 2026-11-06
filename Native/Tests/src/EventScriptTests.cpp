#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include "Audio/MixControlLive.hpp"
#include "Audio/MusicLive.hpp"
#include "Audio/MusicVoiceConflicts.hpp"
#include "Audio/PlaybackResetLive.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SeqSequencerLive.hpp"
#include "Audio/SeqVoiceCommandsLive.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/SoundControlLive.hpp"
#include "Audio/SoundPlaybackLive.hpp"
#include "Audio/StreamControlLive.hpp"
#include "Audio/StreamLive.hpp"
#include "Audio/VoiceMixerLive.hpp"
#include "Audio/VoiceOutputLive.hpp"
#include "Boot/SessionLive.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Field/CollisionSampling.hpp"
#include "Field/RoomLightingLive.hpp"
#include "Field/RoomLoadingLive.hpp"
#include "Menu/Sounds.hpp"
#include "TestSupport/ScriptSources.hpp"

extern "C" {

#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/MusicState.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SequencerOutput.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"
#include "DiscQueue.h"
#include "Effects/Runtime.h"
#include "Event/Script.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptVm.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomLoadingLiveState.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

static int sessionTitleCalls;
static int sessionBattleCalls;
static int sessionFallthrough;
static Audio_CommandParams menuSoundObserved;
static int menuSoundCalls;
static int16_t savedStreamPan[256];
static Audio_CommandHandler savedStreamCommands[7];
static Audio_CommandHandler savedXaCommands[3];
static Content_DiscDrive savedXaDrive;
static const int kStreamCommandIds[] = {0xE0, 0xE2, 0xE4, 0xE5,
                                        0xE6, 0x9E, 0x9F};

int TestsCancelQueuedMusic(int queued, int secondary) {
  const Audio_CommandParams original_pending = Audio_Seq_pendingMusicData;
  Audio_MusicConfig* original_active = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* original_secondary = Audio_Seq_secondaryMusicConfig;
  Audio_MusicConfig current = {0};
  Audio_MusicConfig saved = {0};
  Audio_AkaoSequenceFile sequence = {0};
  const Audio_CommandParams pending = {
      queued ? (uintptr_t)&sequence : 0, 2, 3, 4, 5, 6};
  int result;
  current.statusFlags = 0x10062;
  Audio_Seq_activeMusicConfig = &current;
  Audio_Seq_secondaryMusicConfig = secondary ? &saved : nullptr;
  Audio_Seq_pendingMusicData = pending;
  VS::Audio::Mixer::LiveStopMusicAndCancelReplacement(nullptr);
  result = Audio_Seq_pendingMusicData.param1 == 0;
  result |= (current.statusFlags == 0x62) << 1;
  result |= (Audio_Seq_pendingMusicData.param2 == pending.param2 &&
             Audio_Seq_pendingMusicData.param3 == pending.param3 &&
             Audio_Seq_pendingMusicData.param4 == pending.param4 &&
             Audio_Seq_pendingMusicData.extParam1 == pending.extParam1 &&
             Audio_Seq_pendingMusicData.extParam2 == pending.extParam2)
            << 2;
  result |= (!queued || (uintptr_t)&sequence > UINT32_MAX) << 3;
  VS::Audio::Mixer::LiveStopMusicAndCancelReplacement(nullptr);
  result |= (Audio_Seq_pendingMusicData.param1 == 0) << 4;
  Audio_Seq_pendingMusicData = original_pending;
  Audio_Seq_activeMusicConfig = original_active;
  Audio_Seq_secondaryMusicConfig = original_secondary;
  return result;
}

int TestsSavedMusicVoiceConflicts(void) {
  Audio_MusicConfig current = {0};
  Audio_MusicConfig saved = {0};
  Audio_SeqChannel channels[32] = {nullptr};
  Audio_MusicConfig* original_current = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* original_saved = Audio_Seq_secondaryMusicConfig;
  Audio_SeqChannel* original_channels = Audio_Seq_secondaryMusicChannels;
  Audio_VoiceScheduler original_scheduler = Audio_Mixer_voiceScheduler;
  int original_streams = Audio_Cutscene_streamVoices;
  int failures = 0;
  unsigned flags, voice, reservation, channel;
  Audio_Seq_activeMusicConfig = &current;
  Audio_Seq_secondaryMusicConfig = &saved;
  Audio_Seq_secondaryMusicChannels = channels;
  for (flags = 0; flags < 16; ++flags) {
    const int conflict =
        (flags & 1) && (!(flags & 4) || ((flags & 2) && !(flags & 8)));
    for (voice = 0; voice < 24; ++voice) {
      const int bit = 1 << voice;
      current.allocatedVoices = flags & 1 ? bit : 0;
      current.keyedVoices = flags & 2 ? bit : 0;
      saved.allocatedVoices = flags & 4 ? bit : 0;
      saved.keyedVoices = flags & 8 ? bit : 0;
      for (reservation = 0; reservation < 4; ++reservation) {
        for (channel = 0; channel < 32; ++channel) {
          channels[channel].voice.assignedVoice = channel < 2 ? voice : 24;
        }
        Audio_Mixer_voiceScheduler.activeVoices = reservation & 1 ? bit : 0;
        Audio_Cutscene_streamVoices = reservation & 2 ? bit : 0;
        Audio_Mixer_voiceScheduler.pendingKeyOff = 1U << 25;
        VS::Audio::Seq::ReleaseLiveSavedVoiceConflicts();
        for (channel = 0; channel < 32; ++channel) {
          const unsigned expected = channel < 2 && !conflict ? voice : 24;
          failures += channels[channel].voice.assignedVoice != expected;
        }
        failures += Audio_Mixer_voiceScheduler.pendingKeyOff !=
                    ((1U << 25) | (conflict && reservation == 0 ? bit : 0));
      }
    }
  }
  Audio_Seq_secondaryMusicConfig = nullptr;
  Audio_Mixer_voiceScheduler.pendingKeyOff = 1;
  VS::Audio::Seq::ReleaseLiveSavedVoiceConflicts();
  failures += Audio_Mixer_voiceScheduler.pendingKeyOff != 1;
  Audio_Seq_activeMusicConfig = original_current;
  Audio_Seq_secondaryMusicConfig = original_saved;
  Audio_Seq_secondaryMusicChannels = original_channels;
  Audio_Mixer_voiceScheduler = original_scheduler;
  Audio_Cutscene_streamVoices = original_streams;
  return failures;
}

namespace {
// A zeroed track in a game heap block; the restore reads the track only from
// the block it lies in.
struct HeapSequenceFile {
  alignas(VS::Core::Memory::kHeapUnitBytes)
      std::array<std::byte, VS::Core::Memory::kHeapUnitBytes * 16> arena{};
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  Audio_AkaoSequenceFile* file = static_cast<Audio_AkaoSequenceFile*>(
      VS::Core::Memory::AllocateLiveFromLowEnd(sizeof(Audio_AkaoSequenceFile)));

  HeapSequenceFile() { memset(file, 0, sizeof(*file)); }
};
}  // namespace

int TestsNativeMusicSnapshot(void) {
  const Audio_MusicConfig original_primary = Audio_Seq_primaryMusicConfig;
  const Audio_MusicConfig original_saved = Audio_Seq_savedMusicConfig;
  Audio_MusicConfig* original_active = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* original_secondary = Audio_Seq_secondaryMusicConfig;
  const Audio_VoiceScheduler original_scheduler = Audio_Mixer_voiceScheduler;
  const unsigned original_updates = Audio_Driver_controlLatches.updateFlags;
  Audio_SeqChannel original_channels[32];
  Audio_SeqChannel original_saved_channels[32];
  HeapSequenceFile sequence;
  u_short keymap = 0;
  unsigned channel;
  int result, swap;
  memcpy(original_channels, Audio_Seq_musicChannels, sizeof(original_channels));
  memcpy(original_saved_channels, Audio_Seq_savedMusicChannels,
         sizeof(original_saved_channels));
  memset(&Audio_Seq_primaryMusicConfig, 0,
         sizeof(Audio_Seq_primaryMusicConfig));
  memset(&Audio_Seq_savedMusicConfig, 0, sizeof(Audio_Seq_savedMusicConfig));
  memset(Audio_Seq_musicChannels, 0, sizeof(Audio_Seq_musicChannels));
  memset(Audio_Seq_savedMusicChannels, 0, sizeof(Audio_Seq_savedMusicChannels));
  Audio_Seq_activeMusicConfig = &Audio_Seq_primaryMusicConfig;
  Audio_Seq_primaryMusicConfig.activeChannels = 1;
  Audio_Seq_primaryMusicConfig.musicId = 0x1234;
  Audio_Seq_primaryMusicConfig.timerTop = 0x4321;
  for (channel = 0; channel < 32; ++channel) {
    Audio_Seq_musicChannels[channel].voice.assignedVoice = channel % 24;
    Audio_Seq_musicChannels[channel].voice.dirtyFlags = 0x123400 + channel;
  }
  VS::Audio::Seq::LiveSaveCurrentMusic(nullptr);
  result = Audio_Seq_savedMusicConfig.activeChannels == 1 &&
           Audio_Seq_savedMusicConfig.musicId == 0x1234 &&
           Audio_Seq_savedMusicConfig.timerTop == 0x4321;
  {
    int channels_saved = 1;
    for (channel = 0; channel < 32; ++channel) {
      channels_saved &=
          Audio_Seq_savedMusicChannels[channel].voice.assignedVoice ==
              channel % 24 &&
          Audio_Seq_savedMusicChannels[channel].voice.dirtyFlags ==
              0x123400 + channel;
    }
    result |= channels_saved << 1;
  }
  Audio_Seq_secondaryMusicConfig = nullptr;
  for (swap = 0; swap < 2; ++swap) {
    int restored;
    memset(&Audio_Seq_savedMusicConfig, 0, sizeof(Audio_Seq_savedMusicConfig));
    memset(&Audio_Seq_primaryMusicConfig, 0,
           sizeof(Audio_Seq_primaryMusicConfig));
    Audio_Seq_primaryMusicConfig.timerTop = 0x1111;
    Audio_Seq_savedMusicConfig.timerTop = 0x2222;
    Audio_Seq_savedMusicConfig.sequenceBase = (uintptr_t)sequence.file;
    Audio_Seq_savedMusicConfig.patchTable = sequence.file;
    Audio_Seq_savedMusicConfig.keymapTable = &keymap;
    for (channel = 0; channel < 32; ++channel) {
      Audio_Seq_savedMusicChannels[channel].voice.dirtyFlags = 0x100 + channel;
      Audio_Seq_musicChannels[channel].voice.dirtyFlags = 0x200 + channel;
    }
    VS::Audio::Seq::LiveRestoreMusicSequence(sequence.file, swap);
    restored = Audio_Seq_primaryMusicConfig.timerTop == 0x2222;
    for (channel = 0; channel < 32; ++channel) {
      restored &=
          Audio_Seq_musicChannels[channel].voice.dirtyFlags == 0x100 + channel;
      if (swap) {
        restored &= Audio_Seq_savedMusicChannels[channel].voice.dirtyFlags ==
                    0x200 + channel;
      }
    }
    if (swap) restored &= Audio_Seq_savedMusicConfig.timerTop == 0x1111;
    result |= restored << (swap + 2);
  }
  Audio_Seq_primaryMusicConfig = original_primary;
  Audio_Seq_savedMusicConfig = original_saved;
  Audio_Seq_activeMusicConfig = original_active;
  Audio_Seq_secondaryMusicConfig = original_secondary;
  Audio_Mixer_voiceScheduler = original_scheduler;
  Audio_Driver_controlLatches.updateFlags = original_updates;
  memcpy(Audio_Seq_musicChannels, original_channels, sizeof(original_channels));
  memcpy(Audio_Seq_savedMusicChannels, original_saved_channels,
         sizeof(original_saved_channels));
  return result;
}

static int CheckPendingModulationSlides(Audio_SeqChannel* channel, int effect,
                                        int kind, int16_t* wave) {
  char slide[] = {4, 64};
  char enable[] = {0, 2, 0};
  int failures = 0;
  int index;
  channel->programCounter = slide;
  channel->volume = 0x40000000;
  channel->volumeBalance = 0x7F00;
  channel->pitchBase = 0x1000;
  Audio_Seq_kLfoWaveforms[0] = wave;
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
    const unsigned depth = kind == 0   ? channel->vibratoDepth
                           : kind == 1 ? channel->tremoloDepth
                                       : channel->autoPanDepth;
    const unsigned length = kind == 0   ? channel->vibratoDepthSlideLength
                            : kind == 1 ? channel->tremoloDepthSlideLength
                                        : channel->autoPanDepthSlideLength;
    failures |= (depth != (unsigned)(index * (kind == 2 ? 2048 : 4096)) ||
                 length != (unsigned)(4 - index) ||
                 channel->vibratoPitch != 0 || channel->tremoloVolume != 0 ||
                 channel->autoPanOffset != 0 || channel->updateFlags != 0)
                << 2;
  }
  channel->programCounter = enable;
  switch (kind) {
    case 0:
      VS::Audio::Seq::LiveEnableVibrato(channel, 1);
      break;
    case 1:
      VS::Audio::Seq::LiveEnableTremolo(channel, 1);
      break;
    case 2:
      channel->programCounter = enable + 1;
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
        (channel->vibratoWave != wave + 1 || channel->vibratoPitch != 60)) ||
       (kind == 1 &&
        (channel->tremoloWave != wave + 1 || channel->tremoloVolume != 4064)) ||
       (kind == 2 &&
        (channel->autoPanWave != wave + 1 || channel->autoPanOffset != 16)))
      << 3;
  return failures;
}

int TestsPendingModulationSlides(int effect, int kind) {
  Audio_SeqChannel saved_music[32];
  Audio_SeqChannel saved_effects[12];
  int16_t wave[] = {16384, 8192, 0, 0, -2};
  int16_t* saved_wave = Audio_Seq_kLfoWaveforms[0];
  Audio_SeqChannel* channel;
  int failures = 0;
  int index;
  memcpy(saved_music, Audio_Seq_musicChannels, sizeof(saved_music));
  memcpy(saved_effects, Audio_SoundEffects_channels, sizeof(saved_effects));
  memset(Audio_Seq_musicChannels, 0, sizeof(saved_music));
  memset(Audio_SoundEffects_channels, 0, sizeof(saved_effects));
  VS::Audio::Driver::ResetLivePlaybackState();
  for (index = 0; index < 44; ++index) {
    channel = index < 32 ? &Audio_Seq_musicChannels[index]
                         : &Audio_SoundEffects_channels[index - 32];
    failures |= channel->vibratoRateCounter != 1 ||
                channel->tremoloRateCounter != 1 ||
                channel->autoPanRateCounter != 1;
    failures |=
        (channel->vibratoWave != nullptr || channel->tremoloWave != nullptr ||
         channel->autoPanWave != nullptr)
        << 1;
  }
  channel = effect ? Audio_SoundEffects_channels : Audio_Seq_musicChannels;
  if (failures == 0) {
    failures |= CheckPendingModulationSlides(channel, effect, kind, wave);
    channel->vibratoWave = wave + 1;
    channel->tremoloWave = wave + 1;
    channel->autoPanWave = wave + 1;
    channel->vibratoRateCounter = 4;
    channel->tremoloRateCounter = 5;
    channel->autoPanRateCounter = 6;
    VS::Audio::Driver::ResetLivePlaybackState();
    failures |=
        (channel->vibratoWave != wave + 1 || channel->tremoloWave != wave + 1 ||
         channel->autoPanWave != wave + 1 || channel->vibratoRateCounter != 4 ||
         channel->tremoloRateCounter != 5 || channel->autoPanRateCounter != 6)
        << 4;
    channel->vibratoWave = nullptr;
    channel->tremoloWave = nullptr;
    channel->autoPanWave = nullptr;
    channel->vibratoRateCounter = 0;
    channel->tremoloRateCounter = 0;
    channel->autoPanRateCounter = 0;
    VS::Audio::Driver::ResetLivePlaybackState();
    failures |=
        (channel->vibratoRateCounter != 1 || channel->tremoloRateCounter != 1 ||
         channel->autoPanRateCounter != 1)
        << 5;
  }
  Audio_Seq_kLfoWaveforms[0] = saved_wave;
  memcpy(Audio_Seq_musicChannels, saved_music, sizeof(saved_music));
  memcpy(Audio_SoundEffects_channels, saved_effects, sizeof(saved_effects));
  return failures;
}

void TestsPrepareXaPlayback(void) {
  const Audio_CommandHandler handlers[] = {
      VS::Audio::Seq::LiveSetDiscVolume, VS::Audio::Seq::LiveFadeDiscVolume,
      VS::Audio::Seq::LiveFadeDiscVolumeFrom};
  int index;
  savedXaDrive = Content_discDriveState;
  VS::Audio::Driver::ResetLivePlaybackState();
  memset(&Content_discDriveState, 0, sizeof(Content_discDriveState));
  for (index = 0; index < 3; ++index) {
    savedXaCommands[index] = Audio_Mixer_kCommandHandlers[0xC8 + index];
    Audio_Mixer_kCommandHandlers[0xC8 + index] = handlers[index];
  }
}

void TestsRestoreXaPlayback(void) {
  int index;
  Audio_Native_Reset();
  Content_discDriveState = savedXaDrive;
  for (index = 0; index < 3; ++index) {
    Audio_Mixer_kCommandHandlers[0xC8 + index] = savedXaCommands[index];
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
      return Audio_Seq_discVolumeLevel;
    case 7:
      Audio_Native_Reset();
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
  Audio_CommandHandler handlers[] = {VS::Audio::Cutscene::ReplaceLiveStream,
                                     VS::Audio::Cutscene::StopLiveStream,
                                     VS::Audio::Cutscene::SetLiveStreamVolume,
                                     VS::Audio::Cutscene::SlideLiveStreamVolume,
                                     VS::Audio::Cutscene::SetLiveStreamPan,
                                     VS::Audio::Cutscene::ResumeLiveStream,
                                     VS::Audio::Cutscene::PauseLiveStream};
  int index;
  VS::Audio::Driver::ResetLivePlaybackState();
  memcpy(savedStreamPan, Audio_Mixer_kPanGains, sizeof(savedStreamPan));
  memset(Audio_Mixer_kPanGains, 0, sizeof(savedStreamPan));
  Audio_Mixer_kPanGains[0] = 32767;
  Audio_Mixer_kPanGains[127] = 23170;
  Audio_Mixer_kPanGains[128] = 23170;
  for (index = 0; index < 7; ++index) {
    savedStreamCommands[index] =
        Audio_Mixer_kCommandHandlers[kStreamCommandIds[index]];
    Audio_Mixer_kCommandHandlers[kStreamCommandIds[index]] = handlers[index];
  }
}

void TestsRestoreStreamPlayback(void) {
  int index;
  Audio_Native_Reset();
  memcpy(Audio_Mixer_kPanGains, savedStreamPan, sizeof(savedStreamPan));
  for (index = 0; index < 7; ++index) {
    Audio_Mixer_kCommandHandlers[kStreamCommandIds[index]] =
        savedStreamCommands[index];
  }
}

int TestsBindEffectStream(void* bytes, size_t size) {
  Effects_EffectPackage package = {0};
  Effects_EffectPackage* saved = Effects_package;
  Effects_package = &package;
  static_cast<void>(VS::Effects::BindLiveEffectPackage(
      std::span(static_cast<std::byte*>(bytes), size)));
  Effects_package = saved;
  return package.auxiliarySoundHandle;
}

int TestsPlayStreamSlot(int slot, int pan, int volume) {
  return VS::Audio::LivePlayCutsceneStreamSlot(slot, pan, volume);
}

void TestsAdvanceStreamTick(void) {
  VS::Audio::Seq::LiveUpdateGlobalSlides();
  VS::Audio::Mixer::LiveCommitPlaybackVoices();
  Audio_Native_Tick();
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
  return Audio_Cutscene_streamState.voiceMask;
}

unsigned TestsStreamVolume(void) { return Audio_Cutscene_streamState.volume; }

int TestsStreamSlotIsEmpty(void) {
  return Audio_soundState.sfxStreamData[2] == nullptr;
}

static void CaptureMenuSound(Audio_CommandParams* params) {
  menuSoundObserved = *params;
  ++menuSoundCalls;
}

int TestsMenuSoundDispatch(void) {
  u_int offsets[12] = {0};
  u_short selectors[12] = {0};
  const Audio_CommandHandler saved_handler = Audio_Mixer_kCommandHandlers[0x20];
  const Audio_CommandArguments saved_arguments = Audio_Driver_commandArguments;
  const Audio_CommandParams saved_params = Audio_Mixer_commandParams;
  const int saved_group = Audio_nextSfxChannelGroup7E;
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
  Audio_Mixer_kCommandHandlers[0x20] = CaptureMenuSound;
  Audio_nextSfxChannelGroup7E = 2;
  for (index = 0; index < 5; ++index) {
    selectors[ids[index]] = 0x8000;
    menuSoundCalls = 0;
    play[index]();
    if (menuSoundCalls == 1 &&
        menuSoundObserved.param1 == (uintptr_t)ids[index] &&
        menuSoundObserved.param2 == (2 << index) &&
        menuSoundObserved.param3 == 0x80 && menuSoundObserved.param4 == 0x7F) {
      result |= 1 << index;
    }
  }
  Audio_Mixer_kCommandHandlers[0x20] = saved_handler;
  Audio_Driver_commandArguments = saved_arguments;
  Audio_Mixer_commandParams = saved_params;
  Audio_nextSfxChannelGroup7E = saved_group;
  return result;
}

void TestsNativeNoiseChannel(void) {
  Audio_SeqChannel channel = {nullptr};
  channel.voice.sampleStart = 0x1010;
  channel.voice.sampleRate = 4096;
  channel.voice.volume.left = 0x1000;
  channel.voice.volume.right = 0x1000;
  channel.voice.adsrLower = 0x00FF;
  channel.voice.adsrUpper = 0x5FC4;
  VS::Audio::Driver::ResetLivePlaybackState();
  Audio_Native_ConfigureChannel(&channel);
  Audio_Native_SetNoiseClock(63);
  VS::Audio::Driver::SetLiveVoiceNoiseMask(1);
  Audio_Native_SetReverbMode(4);
  Audio_Native_SetReverbDepth(0x3FFF, 0x3FFF);
  Audio_Native_EnableReverb(1);
  VS::Audio::Driver::SetLiveVoiceReverbMask(1);
  VS::Audio::Driver::KeyOnLiveVoices(1);
  Audio_Native_Tick();
}

void TestsNativeMonoOutput(int enabled) {
  if (enabled) {
    VS::Audio::Mixer::LiveEnableMonoOutput(nullptr);
  } else {
    VS::Audio::Mixer::LiveEnableStereoOutput(nullptr);
  }
  Audio_Native_Tick();
}

int TestsZoneMusicCanonicalState(void) {
  const int saved_id = Field_zoneContext.musicId;
  const int saved_slot = Field_zoneContext.musicSlot;
  const int saved_enabled = Field_zoneContext.isMusicFadeEnabled;
  int passed = 0;
  Field_zoneContext.musicId = 16;
  Field_zoneContext.musicSlot = 3;
  Field_zoneContext.isMusicFadeEnabled = 1;
  if (VS::Field::LiveZoneSoundFileId() == 16) passed |= 1;
  Field_zoneContext.musicId = 17;
  if (VS::Field::LiveZoneSoundFileId() == 17) passed |= 2;
  if (Field_zoneContext.musicSlot == 3) passed |= 4;
  VS::Audio::LiveSetZoneMusicSwitchEnabled(0);
  if (Field_zoneContext.isMusicFadeEnabled == 0) passed |= 8;
  VS::Audio::LiveSetZoneMusicSwitchEnabled(1);
  if (Field_zoneContext.isMusicFadeEnabled == 1) passed |= 16;
  if (Field_zoneContext.musicId == 17 && Field_zoneContext.musicSlot == 3) {
    passed |= 32;
  }
  Field_zoneContext.musicId = saved_id;
  Field_zoneContext.musicSlot = saved_slot;
  Field_zoneContext.isMusicFadeEnabled = saved_enabled;
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
  Render_ColorCode color = {0, 0, 0, 0x5A};
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
  Audio_MusicConfig primary = {0};
  Audio_MusicConfig* savedPrimary = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* savedSecondary = Audio_Seq_secondaryMusicConfig;
  const Audio_DriverFlags savedDriver = Audio_Driver_controlLatches;
  const Audio_VoiceScheduler savedScheduler = Audio_Mixer_voiceScheduler;
  const Audio_VoiceModeMasks savedModes = Audio_Mixer_voiceModes;
  const unsigned savedStream = Audio_Cutscene_streamVoices;
  Audio_Seq_activeMusicConfig = &primary;
  Audio_Seq_secondaryMusicConfig = nullptr;
  memset(&Audio_Mixer_voiceScheduler, 0, sizeof Audio_Mixer_voiceScheduler);
  Audio_Cutscene_streamVoices = 0;
  Audio_Driver_controlLatches.updateFlags = 0x100;
  VS::Audio::Mixer::LiveCommitPlaybackVoices();
  const int passed = Audio_Mixer_voiceModes.noise == 0 &&
                     Audio_Mixer_voiceModes.reverb == 0 &&
                     Audio_Mixer_voiceModes.fm == 0 &&
                     (Audio_Driver_controlLatches.updateFlags & 0x100) == 0;
  Audio_Seq_activeMusicConfig = savedPrimary;
  Audio_Seq_secondaryMusicConfig = savedSecondary;
  Audio_Mixer_voiceScheduler = savedScheduler;
  Audio_Mixer_voiceModes = savedModes;
  Audio_Cutscene_streamVoices = savedStream;
  VS::Audio::Driver::SetLiveVoiceReverbMask(savedModes.reverb);
  VS::Audio::Driver::SetLiveVoiceNoiseMask(savedModes.noise);
  VS::Audio::Driver::SetLiveVoiceFrequencyModulationMask(savedModes.fm);
  Audio_Driver_controlLatches = savedDriver;
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
typedef intptr_t (*TestOpcodeHandler)(u_char*, int16_t);
_Static_assert(_Generic(Event_kOpcodeHandlers[0],
                   TestOpcodeHandler: 1,
                   default: 0),
               "normal opcode ABI");
_Static_assert(_Generic(Event_kSkipOpcodeHandlers[0],
                   TestOpcodeHandler: 1,
                   default: 0),
               "skip opcode ABI");
_Static_assert(_Generic(Event_legacySkipHandlerCopy[0],
                   TestOpcodeHandler: 1,
                   default: 0),
               "copied skip opcode ABI");
_Static_assert(sizeof(Event_kSkipOpcodeHandlers) ==
                   256 * sizeof(TestOpcodeHandler),
               "complete skip table");

int Tests_EventScriptNativeBranches(int skip) {
  const VS::test_support::LiveScriptRuntimeScope runtime;
  TestOpcodeHandler savedNormal[4];
  std::array<TestOpcodeHandler, 4> saved_skip_handlers{};
  std::array<u_char, 4> saved_sizes{};
  constexpr std::array<u_char, 4> kOpcodeSizes{3, 1, 1, 1};
  TestOpcodeHandler skipCopy[256];
  TestOpcodeHandler* savedSkipCopy = Event_legacySkipHandlerCopy;
  uintptr_t savedReturn = Event_subroutineReturnCursor;
  char savedSkip = Event_skipState;
  const auto script =
      VS::test_support::MountLiveEventScript({0, 4, 0, 0, 1, 0, 0, 0, 2, 3});
  auto& host = VS::Event::LiveScriptVm();
  int passed;
  memcpy(savedNormal, Event_kOpcodeHandlers, sizeof savedNormal);
  std::copy_n(Event_kSkipOpcodeHandlers, saved_skip_handlers.size(),
              saved_skip_handlers.begin());
  std::copy_n(Event_kOpcodeSizes, saved_sizes.size(), saved_sizes.begin());
  VS::Core::Memory::CopyLiveAlignedWords(skipCopy, Event_kSkipOpcodeHandlers,
                                         sizeof skipCopy);
  passed = memcmp(skipCopy, Event_kSkipOpcodeHandlers, sizeof skipCopy) == 0;
  skipCopy[0] = Event_kSkipOpcodeHandlers[0] = Event_kOpcodeHandlers[0] =
      Event_ScriptJumpForward;
  skipCopy[1] = Event_kSkipOpcodeHandlers[1] = Event_kOpcodeHandlers[1] =
      Event_ScriptReturnToSavedAddress;
  skipCopy[2] = Event_kSkipOpcodeHandlers[2] = Event_kOpcodeHandlers[2] =
      Event_ScriptYieldFrameOpcode;
  skipCopy[3] = Event_kSkipOpcodeHandlers[3] = Event_kOpcodeHandlers[3] =
      Event_CommandStopScriptContext;
  Event_legacySkipHandlerCopy = skipCopy;
  Event_skipState = skip ? 2 : 0;
  host.SetSubroutineReturn({script.region, 8, script.generation});
  host.SetThreadCursor(1, script);
  std::copy(kOpcodeSizes.begin(), kOpcodeSizes.end(), Event_kOpcodeSizes);
  // Forward jump, saved-address return, advance-and-yield, then thread
  // termination.
  passed &= !VS::Event::ExecuteLiveScriptThread(1) &&
            host.ThreadCursor(1) ==
                VS::Event::ScriptLocation{script.region, 9, script.generation};
  passed &=
      !VS::Event::ExecuteLiveScriptThread(1) && !host.ThreadCursor(1).Present();
  memcpy(Event_kOpcodeHandlers, savedNormal, sizeof savedNormal);
  std::copy(saved_skip_handlers.begin(), saved_skip_handlers.end(),
            Event_kSkipOpcodeHandlers);
  std::copy(saved_sizes.begin(), saved_sizes.end(), Event_kOpcodeSizes);
  Event_subroutineReturnCursor = savedReturn;
  Event_skipState = savedSkip;
  Event_legacySkipHandlerCopy = savedSkipCopy;
  return passed;
}

int Tests_EventScriptBranchHandlers(void) {
  char savedFlag = Event_cachedFlagValue;
  u_char* savedThread = Event_threadCursors[2];
  u_char conditional[] = {0, 7, 4, 0, 0};
  u_char backward[] = {0, 0, 0, 0, 0, 0xFF, 4, 0};
  u_char context[] = {0, 2, 5, 0, 0, 0};
  int passed;
  Event_cachedFlagValue = 7;
  passed = Event_ScriptBranchOnCachedFlag(conditional, 1) ==
           (intptr_t)&conditional[4];
  Event_cachedFlagValue = 6;
  passed &= Event_ScriptBranchOnCachedFlag(conditional, 1) == 0;
  passed &= Event_ScriptLoopBackward(&backward[4], 1) == (intptr_t)backward;
  passed &=
      Event_CommandStartScriptContext(context, 1) == (intptr_t)&context[5];
  passed &= Event_threadCursors[2] == &context[4];
  Event_threadCursors[2] = savedThread;
  Event_cachedFlagValue = savedFlag;
  return passed;
}
}  // extern "C"
