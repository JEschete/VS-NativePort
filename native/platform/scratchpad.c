/* Host stand-in for the PS1 scratchpad (1KB fast RAM at 0x1F800000).
 *
 * Game code accesses the scratchpad by its literal hardware address
 * (e.g. `*(void**)0x1F800000 = …`), so we must place real, writable memory at
 * exactly that address rather than a buffer elsewhere. mmap(MAP_FIXED) does
 * this. A constructor runs it before any game code so even the earliest direct
 * access is valid.
 *
 * We map a full page (larger than the real 1KB) as slack; nothing should rely
 * on the region ending exactly at 0x1F800400.
 */
#include <psx_scratchpad.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

#define SCRATCH_ADDR ((void*)0x1F800000UL)
#define SCRATCH_SIZE 0x1000UL

static int mapped;

static void map_scratchpad(void)
{
    void* p;
    if (mapped) return;
    p = mmap(SCRATCH_ADDR, SCRATCH_SIZE, PROT_READ | PROT_WRITE,
             MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED || p != SCRATCH_ADDR) {
        fprintf(stderr, "[native] FATAL: could not map scratchpad at 0x1F800000\n");
        abort();
    }
    mapped = 1;
}

/* Map before main(), so hardcoded 0x1F800000 accesses are valid immediately. */
__attribute__((constructor))
static void _scratchpad_ctor(void) { map_scratchpad(); }

unsigned long* _psx_scratchpad(void)
{
    map_scratchpad();
    return (unsigned long*)SCRATCH_ADDR;
}
