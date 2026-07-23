/* Loud failure for any PSYQ entry point the native layer does not implement. */
#include <stdio.h>
#include <stdlib.h>

void _native_unimplemented(const char* name)
{
    fprintf(stderr, "[native] UNIMPLEMENTED: %s()", name);
    fputc(10, stderr);
    fflush(stderr);
    abort();
}

/* GCC emits a call to __main() to run global constructors. The decompiled
 * game entry references it; there are no C++ constructors here, so it is a
 * no-op. Provided in the platform layer because gen_stubs skips __-prefixed
 * symbols. */
void __main(void) { }
