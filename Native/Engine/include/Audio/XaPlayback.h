#pragma once

int Audio_Native_StartXaStream(int sector, int volume);
void Audio_Native_StopXaStream();
// One while playing, zero at EOF, negative on an output failure.
int Audio_Native_XaStreamStatus();
void Audio_Native_UpdateXaControls();
