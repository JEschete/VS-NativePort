#pragma once

#include <cstdint>
#include <iterator>

#include "Audio/Seq/Sequencer.hpp"
#include "Core/Diagnostics.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Audio::Seq {

// decomp: SoundVM_B4/B8/BC, src/SLUS_010.40/25AC.c:5462/5568/5631.
inline std::int16_t* LfoWaveform(unsigned shape) {
  if (shape >= std::size(VS::Audio::Seq::kLfoWaveforms) ||
      VS::Audio::Seq::kLfoWaveforms[shape] == nullptr) {
    VS::Core::Errorf("audio.lfo", "unsupported source waveform selector=%u",
                     shape);
    return nullptr;
  }
  void* program = VS::Audio::Seq::kLfoWaveforms[shape];
  VS::Core::Debugf("audio.lfo", "source selector=%u program=%p", shape,
                   program);
  return VS::Audio::Seq::kLfoWaveforms[shape];
}

}  // namespace VS::Audio::Seq
