// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Control/SoundControl.h"

// decomp: _cdlAtv (_cdlAtv@SLUS_010.40)
CdlATV Audio_Control_legacyDiscMixAttenuation;

// decomp: g_Sound_CdVolumeFadeStep (g_Sound_CdVolumeFadeStep@SLUS_010.40)
int Audio_Seq_discVolumeFadeStep;

// decomp: g_Sound_CdVolumeFadeLength (g_Sound_CdVolumeFadeLength@SLUS_010.40)
short Audio_Seq_discVolumeFadeLength;

// decomp: g_CdVolume (g_CdVolume@SLUS_010.40)
int Audio_Seq_discVolume;

}  // extern "C"
