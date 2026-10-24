#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void Core_Debugf(const char* channel, const char* format, ...);
void Core_Warnf(const char* channel, const char* format, ...);
void Core_Errorf(const char* channel, const char* format, ...);

#ifdef __cplusplus
}
#endif
