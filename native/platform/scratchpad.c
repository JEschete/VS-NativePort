/* Host stand-in for the PS1 scratchpad (1KB fast RAM at 0x1F800000).
 *
 * Game code accesses the scratchpad by its literal hardware address
 * (e.g. `*(void**)0x1F800000 = …`), so we must place real, writable memory at
 * exactly that address rather than a buffer elsewhere. VirtualAlloc does
 * this. Native startup initializes it before entering any game code.
 *
 * We map a full page (larger than the real 1KB) as slack; nothing should rely
 * on the region ending exactly at 0x1F800400.
 */
#include <psx_scratchpad.h>
#include <psx_host.h>
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
 * The Windows executable uses a fixed low image base and LARGEADDRESSAWARE
 * so this address is available to the 32-bit process on 64-bit Windows. */
#define RAM_ADDR ((void*)0x80000000UL)
#define RAM_SIZE 0x200000UL

static int mapped;

static void map_region(void* addr, unsigned long size, const char* what)
{
    void* p = psx_host_map_memory(addr, size);
    if (p != addr) {
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

void psx_scratchpad_init(void) { map_scratchpad(); }

unsigned long* _psx_scratchpad(void)
{
    map_scratchpad();
    return (unsigned long*)SCRATCH_ADDR;
}
