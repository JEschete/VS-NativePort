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
