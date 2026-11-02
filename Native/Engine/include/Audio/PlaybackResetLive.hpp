#pragma once

namespace VS::Audio::Driver {
// Applies VS::Audio::Driver::ResetPlaybackState to the live sound driver: the
// native output, the music layers and channel tables, the voice scheduler and
// mixer, the sample bank selections, the XA stream and the driver latches.
void ResetLivePlaybackState();
}  // namespace VS::Audio::Driver
