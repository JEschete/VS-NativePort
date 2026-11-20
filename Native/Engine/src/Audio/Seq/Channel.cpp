// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Audio/Seq/Channel.hpp"

#include "Audio/Mixer/MixControl.hpp"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/AkaoFormat.hpp"
#include "Legacy/Psx.hpp"

// decomp: g_ActiveMusicChannels (g_ActiveMusicChannels@SLUS_010.40)
VS::Audio::SeqChannel
    VS::Audio::Seq::musicChannels[VS::Audio::Seq::kMusicChannelCount];

// decomp: g_PushedMusicConfig (g_PushedMusicConfig@SLUS_010.40)
VS::Audio::MusicConfig VS::Audio::Seq::savedMusicConfig;

// decomp: g_pSecondaryMusicChannels (g_pSecondaryMusicChannels@SLUS_010.40)
VS::Audio::SeqChannel* VS::Audio::Seq::secondaryMusicChannels;

// decomp: g_pSavedMousicConfig (g_pSavedMousicConfig@SLUS_010.40)
VS::Audio::MusicConfig* VS::Audio::Seq::secondaryMusicConfig;

// decomp: D_800366F0 (D_800366F0@SLUS_010.40)
VS::Audio::MusicConfig VS::Audio::Seq::primaryMusicConfig;

// decomp: D_80037788 (D_80037788@SLUS_010.40)
VS::Audio::CommandParams VS::Audio::Seq::pendingMusicData;

// decomp: g_Music_LoopCounter (g_Music_LoopCounter@SLUS_010.40)
std::uint32_t VS::Audio::Seq::musicLoopsRemaining;

// decomp: g_pActiveMusicConfig (g_pActiveMusicConfig@SLUS_010.40)
VS::Audio::MusicConfig* VS::Audio::Seq::activeMusicConfig;

// decomp: D_800378E8 (D_800378E8@SLUS_010.40)
VS::Audio::SeqChannel
    VS::Audio::Seq::savedMusicChannels[VS::Audio::Seq::kMusicChannelCount];
