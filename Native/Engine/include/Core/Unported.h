#pragma once

#include "Core/Diagnostics.hpp"

// C entry point for code that is copied but not yet ported (asm-only functions,
// transitional shims). C++ code uses Core/Unported.hpp; both record into the
// same table.

void Core_RecordUnported(const char* site);

// printf-style diagnostics; formatting is skipped when the level/channel is
// disabled. Core_Tracef uses trace level and the "trace" channel;
// VS::Core::Debugf (Core/Diagnostics.hpp) uses debug level.
void Core_Tracef(const char* format, ...);

#ifndef VS_UNPORTED
#define VS_UNPORTED(site) Core_RecordUnported(site)
#endif
