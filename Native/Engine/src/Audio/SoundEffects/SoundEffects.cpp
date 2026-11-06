// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Mixer/VoiceMixer.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundEffects/SoundEffects.h"

// decomp: D_80035910 (D_80035910@SLUS_010.40)
Audio_SeqChannel Audio_SoundEffects_channels[22];

}  // extern "C"
