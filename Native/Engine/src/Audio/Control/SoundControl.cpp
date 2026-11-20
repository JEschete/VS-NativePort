// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.hpp"

// decomp: _cdlAtv (_cdlAtv@SLUS_010.40)
VS::Legacy::CdlATV VS::Audio::Control::legacyDiscMixAttenuation;

// decomp: g_Sound_CdVolumeFadeStep (g_Sound_CdVolumeFadeStep@SLUS_010.40)
int VS::Audio::Seq::discVolumeFadeStep;

// decomp: g_Sound_CdVolumeFadeLength (g_Sound_CdVolumeFadeLength@SLUS_010.40)
short VS::Audio::Seq::discVolumeFadeLength;

// decomp: g_CdVolume (g_CdVolume@SLUS_010.40)
int VS::Audio::Seq::discVolume;

