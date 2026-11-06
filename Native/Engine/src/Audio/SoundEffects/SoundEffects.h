// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_TYPES
#define VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_TYPES
#include "Legacy/Psx.h"


// decomp: SfxSoundChannels (SfxSoundChannels@SLUS_010.40): a view into another global
#define Audio_SoundEffects_programPairChannels (Audio_SoundEffects_channels + 10)

#endif // VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_DECLS)
#define VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_DECLS
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"

extern Audio_SeqChannel Audio_SoundEffects_channels[22];

#endif // VS_AUDIO_SOUNDEFFECTS_SOUNDEFFECTS_H_DECLS
