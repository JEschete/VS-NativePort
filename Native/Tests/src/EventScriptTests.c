#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Audio/Control/SoundControl.h"
#include "Audio/Cutscene/Stream.h"
#include "Audio/Driver/PlaybackReset.h"
#include "Audio/Driver/SoundDriver.h"
#include "Audio/Mixer/MixControl.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Music.h"
#include "Audio/MusicState.h"
#include "Audio/Playback/SoundPlayback.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Seq/Modulation.h"
#include "Audio/Seq/MusicSequence.h"
#include "Audio/Seq/Sequencer.h"
#include "Audio/SequencerOutput.h"
#include "Audio/Sfx.h"
#include "Audio/SoundBanks.h"
#include "Audio/SoundEffects/SoundEffects.h"
#include "Audio/Stream.h"
#include "Audio/StreamPlayback.h"
#include "Battle/Ai/RoomState.h"
#include "Boot/Session.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "DiscReader.h"
#include "Effects/EffectResources.h"
#include "Effects/Runtime.h"
#include "Event/Script.h"
#include "Event/ScriptActors.h"
#include "Event/ScriptVm.h"
#include "Field/CollisionSampling.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomEffects.h"
#include "Field/RoomLoading.h"
#include "Legacy/Psx.h"
#include "Menu/Sounds.h"
#include "Render/RoomLighting.h"

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

int TestsSavedMusicVoiceConflicts(void) {
  Audio_MusicConfig current = {0};
  Audio_MusicConfig saved = {0};
  Audio_SeqChannel channels[32] = {0};
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
        Audio_Seq_ReleaseSavedVoiceConflicts();
        for (channel = 0; channel < 32; ++channel) {
          const unsigned expected = channel < 2 && !conflict ? voice : 24;
          failures += channels[channel].voice.assignedVoice != expected;
        }
        failures += Audio_Mixer_voiceScheduler.pendingKeyOff !=
                    ((1U << 25) | (conflict && reservation == 0 ? bit : 0));
      }
    }
  }
  Audio_Seq_secondaryMusicConfig = NULL;
  Audio_Mixer_voiceScheduler.pendingKeyOff = 1;
  Audio_Seq_ReleaseSavedVoiceConflicts();
  failures += Audio_Mixer_voiceScheduler.pendingKeyOff != 1;
  Audio_Seq_activeMusicConfig = original_current;
  Audio_Seq_secondaryMusicConfig = original_saved;
  Audio_Seq_secondaryMusicChannels = original_channels;
  Audio_Mixer_voiceScheduler = original_scheduler;
  Audio_Cutscene_streamVoices = original_streams;
  return failures;
}

int TestsNativeMusicSnapshot(void) {
  const Audio_MusicConfig original_primary = Audio_Seq_primaryMusicConfig;
  const Audio_MusicConfig original_saved = Audio_Seq_savedMusicConfig;
  Audio_MusicConfig* original_active = Audio_Seq_activeMusicConfig;
  Audio_MusicConfig* original_secondary = Audio_Seq_secondaryMusicConfig;
  const Audio_VoiceScheduler original_scheduler = Audio_Mixer_voiceScheduler;
  const unsigned original_updates = Audio_Driver_controlLatches.updateFlags;
  Audio_SeqChannel original_channels[32];
  Audio_SeqChannel original_saved_channels[32];
  Audio_AkaoSequenceFile sequence = {0};
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
  Audio_Seq_SaveCurrentMusic();
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
  Audio_Seq_secondaryMusicConfig = NULL;
  for (swap = 0; swap < 2; ++swap) {
    int restored;
    memset(&Audio_Seq_savedMusicConfig, 0, sizeof(Audio_Seq_savedMusicConfig));
    memset(&Audio_Seq_primaryMusicConfig, 0,
           sizeof(Audio_Seq_primaryMusicConfig));
    Audio_Seq_primaryMusicConfig.timerTop = 0x1111;
    Audio_Seq_savedMusicConfig.timerTop = 0x2222;
    Audio_Seq_savedMusicConfig.sequenceBase = (uintptr_t)&sequence;
    Audio_Seq_savedMusicConfig.patchTable = &sequence;
    Audio_Seq_savedMusicConfig.keymapTable = &keymap;
    for (channel = 0; channel < 32; ++channel) {
      Audio_Seq_savedMusicChannels[channel].voice.dirtyFlags = 0x100 + channel;
      Audio_Seq_musicChannels[channel].voice.dirtyFlags = 0x200 + channel;
    }
    Audio_Seq_RestoreMusicSequence(&sequence, swap);
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
      Audio_Seq_SlideVibratoDepth(channel, 1);
      break;
    case 1:
      Audio_Seq_SlideTremoloDepth(channel, 1);
      break;
    case 2:
      Audio_Seq_SlideAutoPanDepth(channel, 1);
      break;
    default:
      fprintf(stderr, "unexpected modulation test kind: %d\n", kind);
      abort();
  }
  for (index = 1; index <= 4; ++index) {
    Audio_Mixer_UpdateChannelSlidesAndDelays(channel, 1, effect);
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
      Audio_Seq_EnableVibrato(channel, 1);
      break;
    case 1:
      Audio_Seq_EnableTremolo(channel, 1);
      break;
    case 2:
      channel->programCounter = enable + 1;
      Audio_Seq_EnableAutoPan(channel, 1);
      break;
    default:
      fprintf(stderr, "unexpected modulation test kind: %d\n", kind);
      abort();
  }
  if (effect) {
    Audio_Mixer_UpdateSoundEffectModulation(channel, 1);
  } else {
    Audio_Mixer_UpdateMusicChannelModulation(channel, 1);
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
  Audio_Driver_ResetPlaybackState();
  for (index = 0; index < 44; ++index) {
    channel = index < 32 ? &Audio_Seq_musicChannels[index]
                         : &Audio_SoundEffects_channels[index - 32];
    failures |= channel->vibratoRateCounter != 1 ||
                channel->tremoloRateCounter != 1 ||
                channel->autoPanRateCounter != 1;
    failures |= (channel->vibratoWave != NULL || channel->tremoloWave != NULL ||
                 channel->autoPanWave != NULL)
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
    Audio_Driver_ResetPlaybackState();
    failures |=
        (channel->vibratoWave != wave + 1 || channel->tremoloWave != wave + 1 ||
         channel->autoPanWave != wave + 1 || channel->vibratoRateCounter != 4 ||
         channel->tremoloRateCounter != 5 || channel->autoPanRateCounter != 6)
        << 4;
    channel->vibratoWave = NULL;
    channel->tremoloWave = NULL;
    channel->autoPanWave = NULL;
    channel->vibratoRateCounter = 0;
    channel->tremoloRateCounter = 0;
    channel->autoPanRateCounter = 0;
    Audio_Driver_ResetPlaybackState();
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
  const Audio_CommandHandler handlers[] = {Audio_Seq_SetDiscVolume,
                                           Audio_Seq_FadeDiscVolume,
                                           Audio_Seq_FadeDiscVolumeFrom};
  int index;
  savedXaDrive = Content_discDriveState;
  Audio_Driver_ResetPlaybackState();
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
      return Audio_BeginXaStream(first, second);
    case 1:
      Content_UpdateDiscCommands();
      return Content_discDriveState.state;
    case 2:
      return Audio_RequestXaStreamStop();
    case 3:
      Audio_Control_SetDiscVolume(first);
      break;
    case 4:
      Audio_Control_FadeDiscVolume(first, second);
      break;
    case 5:
      Audio_Control_FadeDiscVolumeFrom((void*)(intptr_t)first, second, third);
      break;
    case 6:
      return Audio_Seq_discVolumeLevel;
    case 7:
      Audio_Native_Reset();
      break;
    case 8:
      Content_ResetDiscDrive();
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
  Audio_CommandHandler handlers[] = {
      Audio_Cutscene_ReplaceStream,   Audio_Cutscene_StopStream,
      Audio_Cutscene_SetStreamVolume, Audio_Cutscene_SlideStreamVolume,
      Audio_Cutscene_SetStreamPan,    Audio_Cutscene_ResumeStream,
      Audio_Cutscene_PauseStream};
  int index;
  Audio_Driver_ResetPlaybackState();
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

int TestsBindEffectStream(void* bytes) {
  Effects_EffectPackage package = {0};
  Effects_EffectPackage* saved = Effects_package;
  Effects_package = &package;
  Effects_BindEffectPackageBlocks(bytes);
  Effects_package = saved;
  return package.auxiliarySoundHandle;
}

int TestsPlayStreamSlot(int slot, int pan, int volume) {
  return Audio_PlayCutsceneStreamSlot(slot, pan, volume);
}

void TestsAdvanceStreamTick(void) {
  Audio_Seq_UpdateGlobalSlides();
  Audio_Mixer_CommitPlaybackVoices();
  Audio_Native_Tick();
}

void TestsStreamControl(int command, int value, int target) {
  switch (command) {
    case 0:
      Audio_Control_SetCutsceneVolume(value);
      break;
    case 1:
      Audio_Control_FadeCutsceneVolume(value, target);
      break;
    case 2:
      Audio_Control_SetCutscenePan(value);
      break;
    case 3:
      Audio_Mixer_DispatchCommand(value ? 0x9F : 0x9E);
      break;
    case 4:
      Audio_Control_StopCutsceneStream();
      break;
    case 5:
      Audio_Driver_ResetPlaybackState();
      break;
    case 6:
      Audio_ResetSoundSlots();
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
  return Audio_soundState.sfxStreamData[2] == NULL;
}

static void CaptureMenuSound(Audio_CommandParams* params) {
  menuSoundObserved = *params;
  ++menuSoundCalls;
}

int TestsMenuSoundDispatch(void) {
  u_int offsets[12] = {0};
  u_short selectors[12] = {0};
  u_int* saved_offsets = Audio_Playback_soundEffectProgramOffsets;
  u_short* saved_selectors = Audio_Playback_soundEffectBankSelectors;
  const Audio_CommandHandler saved_handler = Audio_Mixer_kCommandHandlers[0x20];
  const Audio_CommandArguments saved_arguments = Audio_Driver_commandArguments;
  const Audio_CommandParams saved_params = Audio_Mixer_commandParams;
  const int saved_group = Audio_nextSfxChannelGroup7E;
  void (*const play[])(void) = {Menu_PlayOpenSound, Menu_PlayInvalidSound,
                                Menu_PlayLeaveSound, Menu_PlaySelectSound,
                                Menu_PlayChangeSound};
  const int ids[] = {10, 7, 6, 5, 4};
  int result = 0;
  int index;
  Audio_Playback_soundEffectProgramOffsets = offsets;
  Audio_Playback_soundEffectBankSelectors = selectors;
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
  Audio_Playback_soundEffectProgramOffsets = saved_offsets;
  Audio_Playback_soundEffectBankSelectors = saved_selectors;
  Audio_Mixer_kCommandHandlers[0x20] = saved_handler;
  Audio_Driver_commandArguments = saved_arguments;
  Audio_Mixer_commandParams = saved_params;
  Audio_nextSfxChannelGroup7E = saved_group;
  return result;
}

void TestsNativeNoiseChannel(void) {
  Audio_SeqChannel channel = {0};
  channel.voice.sampleStart = 0x1010;
  channel.voice.sampleRate = 4096;
  channel.voice.volume.left = 0x1000;
  channel.voice.volume.right = 0x1000;
  channel.voice.adsrLower = 0x00FF;
  channel.voice.adsrUpper = 0x5FC4;
  Audio_Driver_ResetPlaybackState();
  Audio_Native_ConfigureChannel(&channel);
  Audio_Native_SetNoiseClock(63);
  Audio_Driver_SetVoiceNoiseMask(1);
  Audio_Native_SetReverbMode(4);
  Audio_Native_SetReverbDepth(0x3FFF, 0x3FFF);
  Audio_Native_EnableReverb(1);
  Audio_Driver_SetVoiceReverbMask(1);
  Audio_Driver_KeyOnVoices(1);
  Audio_Native_Tick();
}

void TestsNativeMonoOutput(int enabled) {
  if (enabled) {
    Audio_Mixer_EnableMonoOutput(NULL);
  } else {
    Audio_Mixer_EnableStereoOutput(NULL);
  }
  Audio_Native_Tick();
}

int TestsZoneMusicStateAliases(void) {
  const int saved_id = Field_zoneContext.musicId;
  const int saved_slot = Field_zoneContext.musicSlot;
  const int saved_enabled = Field_zoneContext.isMusicFadeEnabled;
  const int saved_alias_id = Field_zoneSoundFileId;
  const int saved_alias_slot = Field_roomMusicSlot;
  const int saved_alias_enabled = Audio_zoneMusicSwitchEnabled;
  int passed = 0;
  if (&Field_zoneSoundFileId == &Field_zoneContext.musicId) passed |= 1;
  if (&Field_roomMusicSlot == &Field_zoneContext.musicSlot) passed |= 2;
  if (&Audio_zoneMusicSwitchEnabled == &Field_zoneContext.isMusicFadeEnabled) {
    passed |= 4;
  }
  Field_zoneContext.musicId = 16;
  Field_zoneContext.musicSlot = 3;
  Field_zoneContext.isMusicFadeEnabled = 1;
  if (Field_ZoneSoundFileId() == 16) passed |= 8;
  if (Field_roomMusicSlot == 3) passed |= 16;
  if (Audio_zoneMusicSwitchEnabled == 1) passed |= 32;
  Audio_SetZoneMusicSwitchEnabled(0);
  if (Field_zoneContext.isMusicFadeEnabled == 0) passed |= 64;
  Field_zoneSoundFileId = saved_alias_id;
  Field_roomMusicSlot = saved_alias_slot;
  Audio_zoneMusicSwitchEnabled = saved_alias_enabled;
  Field_zoneContext.musicId = saved_id;
  Field_zoneContext.musicSlot = saved_slot;
  Field_zoneContext.isMusicFadeEnabled = saved_enabled;
  return passed;
}

int Tests_RoomLightingSamples(void) {
  struct {
    unsigned ambient, floorCount, ceilingCount;
    struct {
      short bounds[4];
      short points[3][2];
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
  Render_SampleRoomLighting((32 << 16) | 32, 0, &color);
  int passed = color.red == 127 && color.green == 63 && color.blue == 63 &&
               color.code == 0x5A;
  Render_SampleRoomLighting((32 << 16) | 32, 1, &color);
  passed = passed && color.red == 64 && color.green == 96 && color.blue == 128;
  Render_SampleRoomLighting((200 << 16) | 200, 0, &color);
  passed = passed && color.red == 17 && color.green == 34 && color.blue == 51 &&
           color.code == 0;
  Field_roomData.lighting = NULL;
  Render_SampleRoomLighting(0, 0, &color);
  passed =
      passed && color.red == 128 && color.green == 128 && color.blue == 128;
  Field_roomData = saved;
  return passed;
}

_Static_assert(sizeof(Field_RoomEffectCommand) == 28,
               "room-effect command disk size");
_Static_assert(offsetof(Field_RoomEffectCommand, operands) == 4,
               "room-effect operands follow the disk header");
_Static_assert(sizeof(Field_RoomEffectObjectView) ==
                   sizeof(Field_RoomEffectObject),
               "room-effect runtime stride");
_Static_assert(offsetof(Field_RoomEffectObjectView, scriptCursor) ==
                   offsetof(Field_RoomEffectObject, scriptCursor),
               "room-effect runtime cursor view");

int Tests_RoomEffectCommandLayout(void) {
  int geometry[16] = {0};
  Field_RoomEffectObject object = {0};
  object.sceneEntry = geometry;
  int transform[7] = {28, 160, -320, 480, 1024, 2048, 3072};
  Field_SetRoomEffectTransform(&object, (void*)transform);
  int passed = object.position.vx == 160 && object.position.vy == -320 &&
               object.position.vz == 480 && geometry[1] == 10 &&
               geometry[2] == -20 && geometry[3] == 30 && geometry[4] == 1024 &&
               geometry[5] == 2048 && geometry[6] == 3072;
  unsigned velocity[5] = {20, 0x0020FFF0, 0x00070030, 0x00020001, 0x00040003};
  Field_SetRoomEffectVelocity(&object, (void*)velocity);
  passed = passed && object.velocity.vx == -16 && object.velocity.vy == 32 &&
           object.velocity.vz == 48 && object.velocity.pad == 7 &&
           object.angularVelocity.vx == 1 && object.angularVelocity.vy == 2 &&
           object.angularVelocity.vz == 3 && object.angularVelocity.pad == 4;
  return passed;
}

int Tests_CollisionHeightPatterns(void) {
  struct {
    Field_MpdCollisionHeader header;
    unsigned short floor[2];
    unsigned short ceiling[2];
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
  int passed = Field_SampleCollisionHeightCode(0, 0, 0) == 0x82 &&
               Field_SampleCollisionHeightCode(1, 0, 0) == 9 &&
               Field_SampleCollisionHeightCode(0, 1, 0) == 11 &&
               Field_SampleCollisionHeightCode(4, 0, 0) == 20 &&
               Field_SampleCollisionHeightCode(7, 3, 0) == 0xFC &&
               Field_SampleCollisionHeightCode(0, 0, 1) == 48 &&
               Field_SampleCollisionHeightCode(4, 0, 1) == 0xFA &&
               Field_SampleCollisionHeightCode(8, 0, 0) == 0x80 &&
               Field_SampleCollisionHeightCode(0, 4, 0) == 0x80 &&
               Field_SampleCollisionHeightCode(0, -1, 0) == 0x80 &&
               Field_SampleCollisionHeightCode(0, 0, 2) == 0x80;
  Field_roomData.header.collisionMapLength--;
  passed = passed && Field_SampleCollisionHeightCode(7, 3, 0) == 0x80;
  Field_roomData = saved;
  return passed;
}

int Tests_RoomAiMapStorage(void) {
  const Field_RoomRecord saved = Field_roomData;
  Field_MpdCollisionHeader dimensions = {2, 3, 0, 6};
  Battle_Ai_RoomAiMap map;
  memset(&Field_roomData, 0, sizeof Field_roomData);
  Field_roomData.collisionMap = &dimensions;
  int passed = Battle_Ai_BuildRoomAiMap(&map) == 1 && map.state.maxTileX == 2 &&
               map.state.maxTileZ == 3 && map.state.defaultActionDelay == 150 &&
               map.flags[0][0] == 0x8000 && map.flags[31][31] == 0x8000 &&
               (unsigned char*)map.flags >= (unsigned char*)(&map.state + 1) &&
               Battle_Ai_DecodeCollisionFlags(0xFFFFFFFFu) == 0x387F;
  for (unsigned bit = 0; bit < 32; ++bit) {
    const unsigned input = 1u << bit;
    const unsigned expected =
        ((input >> 23) & 15) | ((input >> 23) & 0x10) | ((input >> 12) & 0x20) |
        ((input >> 22) & 0x40) | ((input >> 20) & 0x800) |
        ((input >> 18) & 0x1000) | ((input >> 16) & 0x2000);
    passed = passed && Battle_Ai_DecodeCollisionFlags(input) == expected;
  }
  unsigned auxiliary[(0x498 + 6 + 3) / 4] = {0};
  auxiliary[0x450 / 4] = (2u << 29) | (99u << 21) | (1u << 20);
  auxiliary[0x494 / 4] = 0x03020A09;
  Field_roomData.section6 = auxiliary;
  Field_roomData.header.section6Length = 0x498 + 6;
  passed = passed && Battle_Ai_BuildRoomAiMap(&map) == 1 &&
           map.state.maxTileX == 9 && map.state.maxTileZ == 10 &&
           map.state.minTileX == 2 && map.state.minTileZ == 3 &&
           map.state.defaultActionDelay == 99 &&
           map.state.allowsNonSpellActions == 1;
  Field_roomData.header.section6Length--;
  passed = passed && Battle_Ai_BuildRoomAiMap(&map) == 0;
  Field_roomData.header.section6Length = 0x497;
  passed = passed && Battle_Ai_BuildRoomAiMap(&map) == 0;
  int samples[16];
  for (unsigned index = 0; index < 16; ++index) samples[index] = 128;
  samples[5] = 192;
  passed = passed && Battle_Ai_SelectTileHeight(samples, 128) == 192;
  const int invalid_sample = (int)0x80000080u;
  for (size_t index = 0; index < sizeof samples / sizeof samples[0]; ++index) {
    samples[index] = invalid_sample;
  }
  samples[0] = 256;
  passed = passed && Battle_Ai_SelectTileHeight(samples, 128) == samples[10];
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
  Audio_Seq_secondaryMusicConfig = NULL;
  memset(&Audio_Mixer_voiceScheduler, 0, sizeof Audio_Mixer_voiceScheduler);
  Audio_Cutscene_streamVoices = 0;
  Audio_Driver_controlLatches.updateFlags = 0x100;
  Audio_Mixer_CommitPlaybackVoices();
  const int passed = Audio_Mixer_voiceModes.noise == 0 &&
                     Audio_Mixer_voiceModes.reverb == 0 &&
                     Audio_Mixer_voiceModes.fm == 0 &&
                     (Audio_Driver_controlLatches.updateFlags & 0x100) == 0;
  Audio_Seq_activeMusicConfig = savedPrimary;
  Audio_Seq_secondaryMusicConfig = savedSecondary;
  Audio_Mixer_voiceScheduler = savedScheduler;
  Audio_Mixer_voiceModes = savedModes;
  Audio_Cutscene_streamVoices = savedStream;
  Audio_Driver_SetVoiceReverbMask(savedModes.reverb);
  Audio_Driver_SetVoiceNoiseMask(savedModes.noise);
  Audio_Driver_SetVoiceFrequencyModulationMask(savedModes.fm);
  Audio_Driver_controlLatches = savedDriver;
  return passed;
}

static int TestTitleSession(void) {
  ++sessionTitleCalls;
  if (sessionTitleCalls == 1) {
    Boot_JumpToTitle(NULL);
    ++sessionFallthrough;
  }
  if (sessionTitleCalls == 2) {
    Boot_JumpToGameLoop(NULL);
    ++sessionFallthrough;
  }
  return 17;
}

static int TestBattleSession(void) {
  ++sessionBattleCalls;
  Boot_JumpToTitle(NULL);
  ++sessionFallthrough;
  return 0;
}

int Tests_NativeSessionTransitions(void) {
  sessionTitleCalls = sessionBattleCalls = sessionFallthrough = 0;
  int passed = Boot_RunSessionLoop(TestTitleSession, TestBattleSession) == 17 &&
               sessionTitleCalls == 3 && sessionBattleCalls == 1 &&
               sessionFallthrough == 0;
  passed = passed &&
           Boot_RunSessionLoop(TestTitleSession, TestBattleSession) == 17 &&
           sessionTitleCalls == 4;
  return passed;
}

// 573B8.c:67 mixes control codes with addresses; every dispatch boundary must
// retain native pointer width.
typedef intptr_t (*TestOpcodeHandler)(u_char*, short);
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
  TestOpcodeHandler savedNormal[4];
  TestOpcodeHandler skipCopy[256];
  TestOpcodeHandler* savedSkipCopy = Event_legacySkipHandlerCopy;
  uintptr_t savedReturn = Event_subroutineReturnCursor;
  char savedSkip = Event_skipState;
  u_char savedYieldSize = Event_kOpcodeSizes[2];
  u_char script[] = {0, 4, 0, 0, 1, 0, 0, 0, 2, 3};
  u_char* cursor = script;
  int passed;
  memcpy(savedNormal, Event_kOpcodeHandlers, sizeof savedNormal);
  Core_Memory_CopyAlignedWords(skipCopy, Event_kSkipOpcodeHandlers,
                               sizeof skipCopy);
  passed = memcmp(skipCopy, Event_kSkipOpcodeHandlers, sizeof skipCopy) == 0;
  skipCopy[0] = Event_kOpcodeHandlers[0] = Event_ScriptJumpForward;
  skipCopy[1] = Event_kOpcodeHandlers[1] = Event_ScriptReturnToSavedAddress;
  skipCopy[2] = Event_kOpcodeHandlers[2] = Event_ScriptYieldFrame;
  skipCopy[3] = Event_kOpcodeHandlers[3] = Event_CommandStopScriptContext;
  Event_legacySkipHandlerCopy = skipCopy;
  Event_skipState = skip ? 2 : 0;
  Event_subroutineReturnCursor = (uintptr_t)&script[8];
  Event_kOpcodeSizes[2] = 1;
  // Forward jump, saved-address return, advance-and-yield, then thread
  // termination.
  passed &= Event_ExecuteScriptThread(&cursor, 1) == 0 && cursor == &script[9];
  passed &= Event_ExecuteScriptThread(&cursor, 1) == 0 && cursor == NULL;
  memcpy(Event_kOpcodeHandlers, savedNormal, sizeof savedNormal);
  Event_kOpcodeSizes[2] = savedYieldSize;
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
