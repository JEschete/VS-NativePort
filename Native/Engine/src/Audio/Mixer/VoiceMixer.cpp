// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Audio/Mixer/VoiceMixer.hpp"

#include "Audio/MixControlLive.hpp"
#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Seq/AkaoFormat.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SeqMusicSequenceLive.hpp"
#include "Audio/SoundEffectProgramsLive.hpp"
#include "Audio/StreamControlLive.hpp"
#include "Legacy/Psx.hpp"

// decomp: g_Sound_StereoPanGainTableQ15
// (g_Sound_StereoPanGainTableQ15@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::int16_t VS::Audio::Mixer::kPanGains[VS::Audio::Mixer::kPanGainCount];

namespace VS::Audio::Mixer {
// decomp: D_800301E0 (D_800301E0@SLUS_010.40)
// contents: typed registration of the original table's handlers, one per
// command byte (each comment gives the original handler address);
// AudioDispatchTests.cpp checks its parity with the table in SLUS_010.40.
// Not const: tests swap entries to capture commands and restore them.
constinit CommandHandler kCommandHandlers[kCommandHandlerCount] = {
    &LiveIgnoreCommand,                           // 0x00: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x01: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x02: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x03: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x04: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x05: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x06: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x07: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x08: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x09: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x0A: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x0B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x0C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x0D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x0E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x0F: 0x80018BD8
    &Seq::LiveStartOrResumeMusic,                 // 0x10: 0x80016DF8
    &LiveStopMatchingMusic,                       // 0x11: 0x800185A0
    &Seq::LiveStartMusicWithLoopCounter,          // 0x12: 0x800171C8
    &LiveIgnoreCommand,                           // 0x13: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x14: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x15: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x16: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x17: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x18: 0x80018BD8
    &Seq::LiveStartMusicLayer,                    // 0x19: 0x80016F0C
    &Seq::LiveCrossfadeMusicLayers,               // 0x1A: 0x80016FF4
    &LiveIgnoreCommand,                           // 0x1B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x1C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x1D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x1E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x1F: 0x80018BD8
    &SoundEffects::LivePlaySharedProgram,         // 0x20: 0x800172D4
    &SoundEffects::LiveStopPrograms,              // 0x21: 0x8001746C
    &LiveIgnoreCommand,                           // 0x22: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x23: 0x80018BD8
    &SoundEffects::LivePlayBlobPrograms,          // 0x24: 0x8001733C
    &LiveIgnoreCommand,                           // 0x25: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x26: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x27: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x28: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x29: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x2A: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x2B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x2C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x2D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x2E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x2F: 0x80018BD8
    &SoundEffects::LivePlayInterfaceProgram,      // 0x30: 0x80017254
    &LiveIgnoreCommand,                           // 0x31: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x32: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x33: 0x80018BD8
    &SoundEffects::LivePlayInterfaceProgramPair,  // 0x34: 0x80017208
    &LiveIgnoreCommand,                           // 0x35: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x36: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x37: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x38: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x39: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x3A: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x3B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x3C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x3D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x3E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x3F: 0x80018BD8
    &Seq::LiveSaveCurrentMusic,                   // 0x40: 0x80016E74
    &LiveIgnoreCommand,                           // 0x41: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x42: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x43: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x44: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x45: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x46: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x47: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x48: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x49: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x4A: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x4B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x4C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x4D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x4E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x4F: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x50: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x51: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x52: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x53: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x54: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x55: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x56: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x57: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x58: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x59: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x5A: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x5B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x5C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x5D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x5E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x5F: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x60: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x61: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x62: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x63: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x64: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x65: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x66: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x67: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x68: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x69: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x6A: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x6B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x6C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x6D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x6E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x6F: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x70: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x71: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x72: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x73: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x74: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x75: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x76: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x77: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x78: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x79: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x7A: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x7B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x7C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x7D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x7E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x7F: 0x80018BD8
    &LiveEnableStereoOutput,                      // 0x80: 0x800186E4
    &LiveEnableMonoOutput,                        // 0x81: 0x80018744
    &LiveIgnoreCommand,                           // 0x82: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x83: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x84: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x85: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x86: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x87: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x88: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x89: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x8A: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x8B: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x8C: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x8D: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x8E: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x8F: 0x80018BD8
    &LiveSetMusicMuteMask,                        // 0x90: 0x800187A4
    &LiveIgnoreCommand,                           // 0x91: 0x80018BD8
    &LiveSetMusicJumpThreshold,                   // 0x92: 0x800187E4
    &LiveIgnoreCommand,                           // 0x93: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x94: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x95: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x96: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x97: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x98: 0x80018BD8
    &LiveIgnoreCommand,                           // 0x99: 0x80018BD8
    &LiveResumeMusic,                             // 0x9A: 0x800188E8
    &LivePauseMusic,                              // 0x9B: 0x800187F8
    &LiveResumeWorldSoundEffects,                 // 0x9C: 0x80018AA0
    &LivePauseWorldSoundEffects,                  // 0x9D: 0x80018984
    &Cutscene::ResumeLiveStream,                  // 0x9E: 0x80018B84
    &Cutscene::PauseLiveStream,                   // 0x9F: 0x80018B34
    &SoundEffects::LiveSetProgramVolume,          // 0xA0: 0x80017870
    &SoundEffects::LiveFadeProgramVolume,         // 0xA1: 0x80017958
    &SoundEffects::LiveSetProgramPan,             // 0xA2: 0x80017C1C
    &SoundEffects::LiveFadeProgramPan,            // 0xA3: 0x80017CFC
    &LiveSetSoundEffectPitchScale,                // 0xA4: 0x80017FB0
    &LiveSlideSoundEffectPitchScale,              // 0xA5: 0x80018090
    &LiveIgnoreCommand,                           // 0xA6: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xA7: 0x80018BD8
    &SoundEffects::LiveSetSceneVolume,            // 0xA8: 0x80017AD8
    &SoundEffects::LiveFadeSceneVolume,           // 0xA9: 0x80017B54
    &SoundEffects::LiveSetScenePan,               // 0xAA: 0x80017E74
    &SoundEffects::LiveFadeScenePan,              // 0xAB: 0x80017EEC
    &LiveSetWorldSoundEffectPitchScale,           // 0xAC: 0x80018218
    &LiveSlideWorldSoundEffectPitchScale,         // 0xAD: 0x80018270
    &LiveIgnoreCommand,                           // 0xAE: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xAF: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB0: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB1: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB2: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB3: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB4: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB5: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB6: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB7: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB8: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xB9: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xBA: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xBB: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xBC: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xBD: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xBE: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xBF: 0x80018BD8
    &Seq::LiveSetMusicVolume,                     // 0xC0: 0x80017498
    &Seq::LiveFadeMusicVolume,                    // 0xC1: 0x8001754C
    &Seq::LiveFadeMusicVolumeFrom,                // 0xC2: 0x8001766C
    &LiveIgnoreCommand,                           // 0xC3: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xC4: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xC5: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xC6: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xC7: 0x80018BD8
    &Seq::LiveSetDiscVolume,                      // 0xC8: 0x80017764
    &Seq::LiveFadeDiscVolume,                     // 0xC9: 0x80017798
    &Seq::LiveFadeDiscVolumeFrom,                 // 0xCA: 0x80017800
    &LiveIgnoreCommand,                           // 0xCB: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xCC: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xCD: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xCE: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xCF: 0x80018BD8
    &LiveSetTempoScale,                           // 0xD0: 0x8001833C
    &LiveSlideTempoScale,                         // 0xD1: 0x80018358
    &LiveSetTempoScaleRamp,                       // 0xD2: 0x800183C0
    &LiveIgnoreCommand,                           // 0xD3: 0x80018BD8
    &LiveSetMusicPitchScale,                      // 0xD4: 0x80018434
    &LiveSlideMusicPitchScale,                    // 0xD5: 0x80018450
    &LiveSetMusicPitchScaleRamp,                  // 0xD6: 0x800184B8
    &LiveIgnoreCommand,                           // 0xD7: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xD8: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xD9: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xDA: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xDB: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xDC: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xDD: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xDE: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xDF: 0x80018BD8
    &Cutscene::ReplaceLiveStream,                 // 0xE0: 0x8001D644
    &LiveIgnoreCommand,                           // 0xE1: 0x80018BD8
    &Cutscene::StopLiveStream,                    // 0xE2: 0x8001D698
    &LiveIgnoreCommand,                           // 0xE3: 0x80018BD8
    &Cutscene::SetLiveStreamVolume,               // 0xE4: 0x8001D6B8
    &Cutscene::SlideLiveStreamVolume,             // 0xE5: 0x8001D728
    &Cutscene::SetLiveStreamPan,                  // 0xE6: 0x8001D7A8
    &LiveIgnoreCommand,                           // 0xE7: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xE8: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xE9: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xEA: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xEB: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xEC: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xED: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xEE: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xEF: 0x80018BD8
    &LiveStopMusicAndCancelReplacement,           // 0xF0: 0x8001852C
    &LiveStopWorldSoundEffects,                   // 0xF1: 0x80018608
    &LiveIgnoreCommand,                           // 0xF2: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xF3: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xF4: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xF5: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xF6: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xF7: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xF8: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xF9: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xFA: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xFB: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xFC: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xFD: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xFE: 0x80018BD8
    &LiveIgnoreCommand,                           // 0xFF: 0x80018BD8
};
}  // namespace VS::Audio::Mixer

// decomp: g_SpuVoiceInfo (g_SpuVoiceInfo@SLUS_010.40)
VS::Audio::LegacyVoiceEnvelope
    VS::Audio::Mixer::voiceEnvelopes[VS::Audio::Mixer::kVoiceCount];

// decomp: D_80036778 (D_80036778@SLUS_010.40)
int VS::Audio::Mixer::musicPitchScaleStep;

// decomp: D_8003677C (D_8003677C@SLUS_010.40)
int VS::Audio::Mixer::tempoScaleStep;

// decomp: D_80036780 (D_80036780@SLUS_010.40)
std::int16_t VS::Audio::Mixer::musicPitchScaleSlideLength;

// decomp: D_80036782 (D_80036782@SLUS_010.40)
std::int16_t VS::Audio::Mixer::tempoScaleSlideLength;

// decomp: D_80037790 (D_80037790@SLUS_010.40)
VS::Audio::CommandParams VS::Audio::Mixer::commandParams;

// decomp: g_Sound_VoiceSchedulerState (g_Sound_VoiceSchedulerState@SLUS_010.40)
VS::Audio::VoiceScheduler VS::Audio::Mixer::voiceScheduler;

// decomp: D_800378E4 (D_800378E4@SLUS_010.40)
int VS::Audio::Mixer::mutedMusicChannels;

// decomp: g_Sound_MasterPitchScaleQ16_16
// (g_Sound_MasterPitchScaleQ16_16@SLUS_010.40)
int VS::Audio::Mixer::musicPitchScale;

// decomp: g_Sound_TempoMultiplier (g_Sound_TempoMultiplier@SLUS_010.40)
int VS::Audio::Mixer::tempoScale;

// decomp: g_Sound_VoiceChannelConfigs (g_Sound_VoiceChannelConfigs@SLUS_010.40)
VS::Audio::MusicConfig*
    VS::Audio::Mixer::voiceOwners[VS::Audio::Mixer::kVoiceCount];

// decomp: g_Sound_VoiceModeFlags (g_Sound_VoiceModeFlags@SLUS_010.40)
VS::Audio::VoiceModeMasks VS::Audio::Mixer::voiceModes;
