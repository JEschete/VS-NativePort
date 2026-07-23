/* Native GPU/VRAM layer: PSYQ libgpu semantics on the host.
 *
 * Address model: the native binary is linked below 16MB (see Makefile.native,
 * -Ttext-segment=0x10000) so real host pointers fit in the 24-bit fields the
 * PS1 ordering table uses. That means AddPrim/DrawOTag can link primitives
 * exactly as the hardware does, with no translation layer.
 */
#ifndef NATIVE_PSX_GPU_H
#define NATIVE_PSX_GPU_H

#include <stdint.h>

#define PSX_VRAM_W 1024
#define PSX_VRAM_H 512

typedef struct { short x, y, w, h; } RECT;

/* First word of every primitive / OT entry: 24-bit next-address + 8-bit length. */
typedef struct {
    unsigned addr : 24;
    unsigned len  : 8;
    unsigned char r0, g0, b0, code;
} P_TAG;

typedef struct {
    RECT disp;
    RECT screen;
    unsigned char isinter, isrgb24, pad0, pad1;
} DISPENV;

typedef struct {
    RECT clip;
    short ofs[2];
    RECT tw;
    unsigned short tpage;
    unsigned char dtd, dfe, isbg, r0, g0, b0;
    unsigned long dr_env[16];   /* opaque tail (DR_ENV); never touched here */
} DRAWENV;

uint16_t* psx_vram(void);
void      psx_vram_reset(void);

/* Diagnostics: when enabled, DrawOTag prints each primitive it walks. Used to
 * validate ordering-table plumbing before any rasteriser exists. */
void psx_gpu_set_ot_dump(int enable);
int  psx_gpu_prims_drawn(void);
void psx_gpu_reset_stats(void);

/* Dump VRAM (or a sub-rect) to a binary PPM for eyeballing. */
int  psx_vram_write_ppm(const char* path, int x, int y, int w, int h);

#endif
