#pragma once

namespace VS::Core {

void Debugf(const char* channel, const char* format, ...);
void Warnf(const char* channel, const char* format, ...);
void Errorf(const char* channel, const char* format, ...);

}  // namespace VS::Core
