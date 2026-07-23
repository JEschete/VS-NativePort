/* Host stand-in for PS1 VRAM: 1024x512 16-bit framebuffer.
 * Pixel format is 5551 (BGR order, MSB = semi-transparency bit), same as hw. */
#include <psx_gpu.h>
#include <stdio.h>
#include <string.h>

static uint16_t vram[PSX_VRAM_W * PSX_VRAM_H];

uint16_t* psx_vram(void) { return vram; }
void psx_vram_reset(void) { memset(vram, 0, sizeof vram); }

int psx_vram_write_ppm(const char* path, int x, int y, int w, int h)
{
    FILE* f;
    int i, j;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
        x + w > PSX_VRAM_W || y + h > PSX_VRAM_H)
        return -1;
    f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint16_t p = vram[(y + j) * PSX_VRAM_W + (x + i)];
            unsigned char rgb[3];
            rgb[0] = (unsigned char)(((p >>  0) & 0x1F) << 3);
            rgb[1] = (unsigned char)(((p >>  5) & 0x1F) << 3);
            rgb[2] = (unsigned char)(((p >> 10) & 0x1F) << 3);
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    return 0;
}
