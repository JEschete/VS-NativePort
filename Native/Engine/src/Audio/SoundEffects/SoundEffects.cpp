// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Mixer/VoiceMixer.hpp"
#include "Audio/Seq/Channel.hpp"
#include "Audio/SoundEffects/SoundEffects.hpp"

// decomp: D_80035910 (D_80035910@SLUS_010.40)
VS::Audio::SeqChannel VS::Audio::SoundEffects::channels[VS::Audio::SoundEffects::kChannelSlots];

