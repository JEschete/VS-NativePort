#ifndef NATIVE_GAME_SRC_EFFECTS_TARGETREQUIREMENTS_H_
#define NATIVE_GAME_SRC_EFFECTS_TARGETREQUIREMENTS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Read-only proof for an original single-primary-track package entered at
// world-script offset zero, without a parent or preattached renderer. One means
// mandatory execution does not need target/emitTo. The guarded kind-7 colour
// track is optional only for the original absent actor sentinel; this does not
// authorize another actor.
// Unsupported or malformed sources return zero; malformed sources are logged.
int Effects_ScriptPackageAllowsAbsentTarget(const void* bytes,
                                            size_t byte_count);

// Single-primary-request bridge across asynchronous loading. Record only an
// original model selector whose lookup returned the absent sentinel. Validation
// consumes the record before logging/proof, including on failure or reentry.
// actual_owner_model: -1 genuinely absent, -2 unprovable existing owner, or the
// nonnegative model of a real, fully bound owner. Reset before every new
// request.
void Effects_ResetDeferredScriptTarget(void);
void Effects_RecordDeferredScriptTarget(unsigned effect_id, unsigned selector,
                                        unsigned token);
int Effects_ValidateDeferredScriptTarget(unsigned effect_id, unsigned token,
                                         int actual_owner_model,
                                         const void* bytes, size_t byte_count);

#ifdef __cplusplus
}
#endif

#endif  // NATIVE_GAME_SRC_EFFECTS_TARGETREQUIREMENTS_H_
