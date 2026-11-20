// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Audio/Mixer/VoiceMixer.hpp"

#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Seq/AkaoFormat.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Legacy/Psx.hpp"

// decomp: g_Sound_StereoPanGainTableQ15
// (g_Sound_StereoPanGainTableQ15@SLUS_010.40)
// contents: module image (ModuleData.cpp)
std::int16_t VS::Audio::Mixer::kPanGains[VS::Audio::Mixer::kPanGainCount];

// decomp: D_800301E0 (D_800301E0@SLUS_010.40)
// contents: module image, relocated (ModulePointers.cpp)
VS::Audio::CommandHandler
    VS::Audio::Mixer::kCommandHandlers[VS::Audio::Mixer::kCommandHandlerCount];

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
