#pragma once

namespace VS::Core {

// printf-style diagnostics at the Core/Log.hpp levels; formatting is skipped
// when the level/channel is disabled.
void Tracef(const char* channel, const char* format, ...);
void Debugf(const char* channel, const char* format, ...);
void Warnf(const char* channel, const char* format, ...);
void Errorf(const char* channel, const char* format, ...);

}  // namespace VS::Core
