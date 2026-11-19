#pragma once
// The disc audio mix: the libcd attenuation the original applied, and the
// disc volume with its fade.
#include <cstdint>

#include "Legacy/LibDs.h"

namespace VS::Audio::Control {
// decomp: _cdlAtv (_cdlAtv@SLUS_010.40)
extern CdlATV legacyDiscMixAttenuation;
}  // namespace VS::Audio::Control

namespace VS::Audio::Seq {
// decomp: g_Sound_CdVolumeFadeStep (g_Sound_CdVolumeFadeStep@SLUS_010.40)
extern std::int32_t discVolumeFadeStep;
// decomp: g_Sound_CdVolumeFadeLength (g_Sound_CdVolumeFadeLength@SLUS_010.40)
extern std::int16_t discVolumeFadeLength;
// Its high halfword is the decomp's D_800378E2 view.
// decomp: g_CdVolume (g_CdVolume@SLUS_010.40)
extern std::int32_t discVolume;
}  // namespace VS::Audio::Seq
