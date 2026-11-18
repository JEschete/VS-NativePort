#pragma once

namespace VS::Audio {

int StartXaStream(int sector, int volume);
void StopXaStream();
// One while playing, zero at EOF, negative on an output failure.
int XaStreamStatus();
void UpdateXaControls();

}  // namespace VS::Audio
