#pragma once

#include <cstdint>
#include <iterator>

#include "Audio/Seq/Sequencer.h"
#include "Core/Diagnostics.h"
#include "Legacy/Psx.h"

namespace VS::Audio::Seq {

// decomp: SoundVM_B4/B8/BC, src/SLUS_010.40/25AC.c:5462/5568/5631.
inline std::int16_t* LfoWaveform(unsigned shape) {
  if (shape >= std::size(Audio_Seq_kLfoWaveforms) ||
      Audio_Seq_kLfoWaveforms[shape] == nullptr) {
    Core_Errorf("audio.lfo", "unsupported source waveform selector=%u", shape);
    return nullptr;
  }
  void* program = Audio_Seq_kLfoWaveforms[shape];
  Core_Debugf("audio.lfo", "source selector=%u program=%p", shape, program);
  return Audio_Seq_kLfoWaveforms[shape];
}

}  // namespace VS::Audio::Seq
