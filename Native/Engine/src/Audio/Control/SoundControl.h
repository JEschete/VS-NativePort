// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


// decomp: D_800378E2 (D_800378E2@SLUS_010.40): a view into another global
#define Audio_Seq_discVolumeLevel (*(short*)((u_char*)&Audio_Seq_discVolume + 2))

extern CdlATV Audio_Control_legacyDiscMixAttenuation;
extern int Audio_Seq_discVolumeFadeStep;
extern short Audio_Seq_discVolumeFadeLength;
extern int Audio_Seq_discVolume;
