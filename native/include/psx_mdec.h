/* psx_mdec.h -- the PlayStation's Motion Decoder (MDEC), in software.
 *
 * The MDEC is a fixed-function block that turns run/level coded DCT
 * coefficients into pixels. It is NOT the whole video decoder: the variable
 * length (Huffman) stage runs on the CPU, in PSYQ's DecDCTvlc2, and only its
 * output -- a stream of 16-bit run/level codes -- reaches the hardware. Both
 * halves live here, because both are ours to write natively:
 *
 *   - The VLC stage is `src/TITLE/TITLE.PRG/libpress/VLC_C.s`, hand-written
 *     MIPS assembly that never got decompiled, so it cannot run on x86.
 *   - The MDEC itself is hardware, reached over DMA channels 0 and 1.
 *
 * `native/platform/libpress.c` sits on top and presents the PSYQ libpress API
 * the game actually calls. Implementation notes are in native/platform/mdec.c.
 */
#ifndef NATIVE_PSX_MDEC_H
#define NATIVE_PSX_MDEC_H

/* Output depths, matching the MDEC command word's depth field. */
#define MDEC_DEPTH_4BIT  0
#define MDEC_DEPTH_8BIT  1
#define MDEC_DEPTH_24BIT 2
#define MDEC_DEPTH_15BIT 3

/* Decode one frame's VLC bitstream into MDEC run/level codes.
 *
 * `frame` points at the frame header the STR demuxer reassembled (u16 word
 * count, u16 0x3800 magic, u16 quantisation scale, u16 version), with the
 * bitstream immediately after it. `out` receives a buffer in the exact shape
 * DecDCTin expects: out[0] is the MDEC command word (including the count of
 * data words that follow), out[1..] the codes packed two per word.
 *
 * `table` is the decode table DecDCTvlcBuild produced -- the game builds it
 * itself and passes it down, so the port decodes with the retail table rather
 * than a reconstruction of it.
 *
 * Returns the number of 32-bit words written (command word included), or a
 * negative value if the frame header or the output buffer is unusable. */
int mdec_vlc_decode(const void* frame, unsigned long* out, int out_max_words,
    const unsigned short* table);

/* How many whole macroblocks the last mdec_vlc_decode() call coded. A frame
 * that decoded cleanly reports exactly the count its dimensions call for. */
int mdec_vlc_macroblocks(void);

/* Hardware-side state. mdec_reset() corresponds to the reset command
 * (MDEC1 bit 31) that DecDCTReset issues. */
void mdec_reset(void);

/* Load the inverse-quantisation tables (MDEC command 2). Both are 64 bytes in
 * zig-zag order. Passing NULL for either keeps the current table. */
void mdec_set_iq(const unsigned char* iq_y, const unsigned char* iq_c);

/* Feed a decode-macroblock command plus its data words: buf[0] is the command,
 * buf[1..nwords] the run/level codes. Decoding is synchronous here, so all of
 * the output is ready when this returns (see native/platform/libpress.c for why
 * that is safe given how the game drives DecDCTout). */
void mdec_in(const unsigned long* buf, int nwords);

/* Drain decoded pixels. Returns the number of 32-bit words actually written;
 * short reads mean the input did not code that many macroblocks. */
int mdec_out(void* dst, int nwords);

/* Words still waiting in the output FIFO. */
int mdec_out_avail(void);

#endif /* NATIVE_PSX_MDEC_H */
