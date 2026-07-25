/* psx_spu.h -- the PlayStation's Sound Processing Unit, in software.
 *
 * WHY THIS IS THE WHOLE JOB. Vagrant Story's sound driver is Square's Akao
 * engine, and it is *already decompiled* -- src/SLUS_010.40/25AC.c is ~5300
 * lines of C with only a handful of assembly stubs left. It therefore runs
 * natively, unmodified, exactly like the rest of the game: it walks its
 * sequences, computes pitches and envelopes, and writes the SPU's registers.
 *
 * It writes them by literal address (src/include/hw.h: voice registers at
 * 0x1F801C00 + n*0x10, key-on at 0x1F801D88, ...), and native/platform/
 * scratchpad.c backs that page with real memory -- so those writes already
 * land somewhere real. Nothing was listening. This is the listener.
 *
 * The register block is read rather than trapped: no MMU games, no rewritten
 * call sites. The one thing that cannot be recovered by reading alone is a
 * write to key-on/key-off, which is an event rather than a state -- so those
 * two registers are consumed and cleared once acted on. The driver only ever
 * writes them, so it cannot tell.
 */
#ifndef NATIVE_PSX_SPU_H
#define NATIVE_PSX_SPU_H

#define SPU_RAM_SIZE   0x80000     /* 512 KB */
#define SPU_VOICES     24
#define SPU_RATE       44100

/* Power-on state: silence, all voices off, sample memory cleared. */
void spu_reset(void);

/* Sample-RAM transfers, as issued by SpuWrite/SpuRead. `addr` is a byte
 * address into SPU RAM. */
void spu_ram_write(unsigned addr, const void* data, unsigned len);
void spu_ram_read(unsigned addr, void* data, unsigned len);

/* Mix `frames` stereo sample pairs into `out` (interleaved L,R signed 16-bit).
 * Reads the register block for voice state, and consumes any pending key-on /
 * key-off. */
void spu_render(short* out, int frames);

/* Voices currently sounding -- for tracing and tests. */
int spu_active_voices(void);

#endif /* NATIVE_PSX_SPU_H */
