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

/* Scratchpad (1KB at 0x1F800000) plus the hardware I/O register block that
 * follows it at 0x1F801000. Decompiled code writes SPU/GPU/CD registers by
 * literal address -- the sound driver's init, for instance, pokes the SPU main
 * volume at 0x1F801D80 and SPUCNT at 0x1F801DAA. Backing that page with plain
 * memory turns those writes into harmless stores instead of segfaults, and lets
 * such code run verbatim. Reads see whatever was last written, which is the
 * right answer for the volume/control registers the game reads back. */
#define SCRATCH_ADDR ((void*)0x1F800000UL)
#define SCRATCH_SIZE 0x2000UL

/* PS1 main RAM, 2MB at the KSEG0 base. The retail boot path hands the game's
 * allocator a literal address inside it -- _sysReinit does
 * `vs_main_initHeap((vs_main_HeapHeader*)0x8010C000, 0xF2000)` -- and other code
 * addresses RAM directly too. Mapping the region where the hardware has it lets
 * all of that run unmodified, exactly as with the scratchpad above, instead of
 * rewriting call sites to use host buffers.
 *
 * 0x80000000 is comfortably inside the 3GB user address space of 32-bit Linux,
 * well away from the binary (linked at 0x10000), the C heap and the stack. */
#define RAM_ADDR ((void*)0x80000000UL)
#define RAM_SIZE 0x200000UL

static int mapped;

static void map_region(void* addr, unsigned long size, const char* what)
{
    void* p = mmap(addr, size, PROT_READ | PROT_WRITE,
                   MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED || p != addr) {
        fprintf(stderr, "[native] FATAL: could not map %s at %p\n", what, addr);
        abort();
    }
}

static void map_scratchpad(void)
{
    if (mapped) return;
    mapped = 1;
    map_region(SCRATCH_ADDR, SCRATCH_SIZE, "scratchpad + I/O registers");
    map_region(RAM_ADDR, RAM_SIZE, "PS1 main RAM");
}

/* Map before main(), so hardcoded PS1 addresses are valid immediately. */
__attribute__((constructor))
static void _scratchpad_ctor(void) { map_scratchpad(); }

unsigned long* _psx_scratchpad(void)
{
    map_scratchpad();
    return (unsigned long*)SCRATCH_ADDR;
}
