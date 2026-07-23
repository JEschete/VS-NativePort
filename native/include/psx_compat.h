/* Native-target compatibility shim.
 *
 * Force-included (-include) for the native build ONLY; the matching PSX build
 * never sees this file. Its job is to paper over constructs the 1997 PSYQ
 * compiler (GCC 2.7.2) accepted but modern GCC rejects, WITHOUT modifying game
 * source that the byte-matching build depends on.
 */
#ifndef NATIVE_PSX_COMPAT_H
#define NATIVE_PSX_COMPAT_H

/* Several TUs (memory-card code in MENU7/TITLE) use struct DIRENTRY without
 * including the header that defines it; GCC 2.7.2 tolerated the incomplete
 * type. kernel.h is guarded, so including it here is idempotent. */
#include <kernel.h>

#endif /* NATIVE_PSX_COMPAT_H */
