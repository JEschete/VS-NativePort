#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int Audio_Native_StartXaStream(int sector, int volume);
void Audio_Native_StopXaStream(void);
// One while playing, zero at EOF, negative on an output failure.
int Audio_Native_XaStreamStatus(void);
void Audio_Native_UpdateXaControls(void);

#ifdef __cplusplus
}
#endif
