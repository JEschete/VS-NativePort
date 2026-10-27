#pragma once

#include "Core/Diagnostics.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "Audio/Seq/Sequencer.h"

// decomp: SoundVM_B4/B8/BC, src/SLUS_010.40/25AC.c:5462/5568/5631.
static inline int16_t* Audio_Seq_LfoWaveform(unsigned shape) {
  if (shape >=
          sizeof Audio_Seq_kLfoWaveforms / sizeof Audio_Seq_kLfoWaveforms[0] ||
      !Audio_Seq_kLfoWaveforms[shape]) {
    Core_Errorf("audio.lfo", "unsupported source waveform selector=%u", shape);
#ifdef __cplusplus
    return nullptr;
#else
    return NULL;
#endif
  }
  void* program = Audio_Seq_kLfoWaveforms[shape];
  Core_Debugf("audio.lfo", "source selector=%u program=%p", shape, program);
  return Audio_Seq_kLfoWaveforms[shape];
}

#ifdef __cplusplus
}
#endif
