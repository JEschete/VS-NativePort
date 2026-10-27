#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifdef __cplusplus
#include <chrono>

namespace VS::Core {
int FrameCounterTicks(std::chrono::nanoseconds elapsed);
}

extern "C" {
#endif
void Core_ResetFrameTimingBaseline(void);
int Core_ElapsedFrameCounterTicks(void);
#ifdef __cplusplus
}
#endif
