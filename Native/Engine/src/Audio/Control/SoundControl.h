// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_AUDIO_CONTROL_SOUNDCONTROL_H_TYPES
#define VS_AUDIO_CONTROL_SOUNDCONTROL_H_TYPES
#include "Legacy/Psx.h"


// decomp: D_800378E2 (D_800378E2@SLUS_010.40): a view into another global
#define Audio_Seq_discVolumeLevel (*(short*)((u_char*)&Audio_Seq_discVolume + 2))

#endif // VS_AUDIO_CONTROL_SOUNDCONTROL_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_AUDIO_CONTROL_SOUNDCONTROL_H_DECLS)
#define VS_AUDIO_CONTROL_SOUNDCONTROL_H_DECLS

extern CdlATV Audio_Control_legacyDiscMixAttenuation;
extern int Audio_Seq_discVolumeFadeStep;
extern short Audio_Seq_discVolumeFadeLength;
extern int Audio_Seq_discVolume;

#endif // VS_AUDIO_CONTROL_SOUNDCONTROL_H_DECLS
