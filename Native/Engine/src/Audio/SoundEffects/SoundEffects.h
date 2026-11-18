// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


// decomp: SfxSoundChannels (SfxSoundChannels@SLUS_010.40): a view into another global
#define Audio_SoundEffects_programPairChannels (Audio_SoundEffects_channels + 10)

#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"

extern Audio_SeqChannel Audio_SoundEffects_channels[22];
