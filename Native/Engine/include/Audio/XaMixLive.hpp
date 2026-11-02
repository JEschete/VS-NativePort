#pragma once

#include <cstdint>

namespace VS::Audio::Control {
// Applies VS::Audio::Control::SetDiscMixVolume with the live output mode and
// publishes it to the native XA source. Returns false for a volume above 0xFF.
bool SetLiveDiscMixVolume(std::uint32_t volume);
}  // namespace VS::Audio::Control

namespace VS::Audio::Seq {
// Publishes the sequencer's disc volume and the current mix to the native XA
// source in place of the CD_VOL register writes.
void WriteLiveDiscVolume();
}  // namespace VS::Audio::Seq
