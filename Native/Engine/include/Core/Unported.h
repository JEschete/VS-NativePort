#pragma once

// C entry point for code that is copied but not yet ported (asm-only functions, transitional shims).
// C++ code uses Core/Unported.hpp; both record into the same table.

#ifdef __cplusplus
extern "C" {
#endif

void Core_RecordUnported(const char* site);

#ifdef __cplusplus
}
#endif

#ifndef VS_UNPORTED
#define VS_UNPORTED(site) Core_RecordUnported(site)
#endif
