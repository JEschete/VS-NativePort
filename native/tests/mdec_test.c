/* mdec_test.c -- offline check of the software MDEC against the real movie.
 *
 * Demuxes data/MOV/TITLE.STR the way the streaming layer will, decodes frames
 * with native/platform/mdec.c, and reports whether each one codes exactly the
 * macroblocks its dimensions call for. That count is the sharp test: a single
 * wrong entry in the variable-length table desynchronises the bitstream, and a
 * desynchronised frame cannot land on the right macroblock count by accident.
 *
 *   make -f Makefile.native mdectest
 *   native/build/mdec_test [str-file] [frames-to-dump]
 *
 * Dumped frames are written as native/build/mdec_NNN.ppm.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psx_mdec.h>

#define SECTOR   2336
#define SUBHDR   8      /* CD-XA subheader, kept by the extraction */
#define STRHDR   32     /* STR chunk header */
/* An STR chunk carries 2048 bytes -- the 32-byte header plus 2016 bytes of
 * frame data -- even though the form 2 sector it rides in has room for 2324.
 * The remainder of the sector is padding. Taking the whole sector instead
 * splices junk into the bitstream partway through the first chunk, which
 * decodes as plausible-looking coefficients rather than failing outright. */
#define PAYLOAD  2016

#define STR_MAGIC 0x80010160UL

static unsigned int rd16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static unsigned long rd32(const unsigned char* p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8)
         | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static unsigned char frame_buf[64 * 1024];
static unsigned long mdec_words[64 * 1024];
static unsigned char strip[16 * 512 * 3];
static unsigned char image[640 * 512 * 3];

/* The decode table is the game's own, built by retail code (BUILD.c). */
extern void DecDCTvlcBuild(unsigned short* table);
static unsigned short vlc_table[34816];

static void write_ppm(const char* path, const unsigned char* rgb, int w, int h)
{
    FILE* f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * h * 3, f);
    fclose(f);
}

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "data/MOV/TITLE.STR";
    int dump_frames = argc > 2 ? atoi(argv[2]) : 6;

    FILE* f = fopen(path, "rb");
    unsigned char sec[SECTOR];
    long nsec = 0, isec = 0;
    int frames = 0, good = 0, bad = 0, dumped = 0;
    int cur_frame = -1, cur_len = 0, cur_chunks = 0, seen_chunks = 0;
    int width = 0, height = 0, declared = 0;

    if (!f) { perror(path); return 1; }
    fseek(f, 0, SEEK_END);
    nsec = ftell(f) / SECTOR;
    fseek(f, 0, SEEK_SET);
    printf("%s: %ld sectors\n", path, nsec);

    DecDCTvlcBuild(vlc_table);
    mdec_reset();

    for (isec = 0; isec < nsec; isec++) {
        int chunkno, nchunks, frameno, fsize, w, h;
        if (fread(sec, 1, SECTOR, f) != SECTOR) break;
        if (sec[2] & 0x04) continue;                       /* audio sector */
        if (rd32(sec + SUBHDR) != STR_MAGIC) continue;     /* not a video chunk */

        chunkno = (int)rd16(sec + SUBHDR + 4);
        nchunks = (int)rd16(sec + SUBHDR + 6);
        frameno = (int)rd32(sec + SUBHDR + 8);
        fsize   = (int)rd32(sec + SUBHDR + 12);
        w       = (int)rd16(sec + SUBHDR + 16);
        h       = (int)rd16(sec + SUBHDR + 18);

        if (chunkno == 0) {
            cur_frame = frameno;
            cur_len = 0;
            cur_chunks = nchunks;
            seen_chunks = 0;
            width = w;
            height = h;
            declared = fsize;
        }
        if (frameno != cur_frame) continue;                /* lost the head */
        if (cur_len + PAYLOAD <= (int)sizeof frame_buf) {
            memcpy(frame_buf + cur_len, sec + SUBHDR + STRHDR, PAYLOAD);
            cur_len += PAYLOAD;
        }
        if (++seen_chunks != cur_chunks) continue;

        /* Whole frame in hand. */
        {
            int want_mb = ((width + 15) / 16) * ((height + 15) / 16);
            int words = mdec_vlc_decode(frame_buf, mdec_words,
                                        (int)(sizeof mdec_words / sizeof mdec_words[0]),
                                        vlc_table);
            int got_mb = mdec_vlc_macroblocks();

            frames++;
            if (words < 0 || got_mb != want_mb) {
                bad++;
                if (bad <= 8) {
                    printf("frame %3d: FAIL rc=%d macroblocks %d (want %d) "
                           "%dx%d declared %d bytes\n",
                           cur_frame, words, got_mb, want_mb, width, height, declared);
                }
                continue;
            }
            good++;

            if (dumped < dump_frames) {
                int cols = width / 16;
                int strip_words = 16 * height * 3 / 4;
                int c, y, x;
                char name[128];

                /* 24-bit output, matching DecDCTin(buf, 3) in 22C.c. */
                mdec_words[0] = (mdec_words[0] & ~0x18000000UL) | 0x10000000UL
                              | 0x02000000UL;
                mdec_in(mdec_words, words - 1);

                for (c = 0; c < cols; c++) {
                    mdec_out(strip, strip_words);
                    for (y = 0; y < height; y++) {
                        for (x = 0; x < 16; x++) {
                            const unsigned char* s = strip + (y * 16 + x) * 3;
                            unsigned char* d = image + ((size_t)y * width + c * 16 + x) * 3;
                            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                        }
                    }
                }
                sprintf(name, "native/build/mdec_%03d.ppm", cur_frame);
                write_ppm(name, image, width, height);
                printf("frame %3d: ok, %d macroblocks, %d words -> %s\n",
                       cur_frame, got_mb, words, name);
                dumped++;
            }
        }
    }

    fclose(f);
    printf("\n%d frames: %d decoded cleanly, %d failed\n", frames, good, bad);
    return bad ? 1 : 0;
}
