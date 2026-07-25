/* libstream: PSYQ's CD streaming layer (St*), over the virtual drive.
 *
 * This is the piece that turns a stream of CD sectors back into whole video
 * frames. Like libds it is undecompiled PSYQ library assembly, so every St*
 * entry point is ours; everything above it -- _initIntroMovie, _playIntroMovie,
 * _getNextMovieFrame in src/TITLE/TITLE.PRG/22C.c -- is decompiled game code
 * that runs unmodified.
 *
 * The contract those callers rely on:
 *
 *   StSetRing(buf, nsectors)      hand the library a ring buffer
 *   StSetStream(mode, start, end, cb1, cb2)
 *   StGetNext(&addr, &header)     0 when a whole frame is ready; addr is the
 *                                 frame data, header its STR chunk header
 *   StFreeRing(addr)              release the frame's storage
 *   StUnSetRing()                 tear it all down
 *
 * A movie is stored as one 2048-byte STR chunk per sector: a 32-byte header
 * (magic, chunk index, chunks per frame, frame number, frame size, dimensions)
 * followed by 2016 bytes of frame data. Frames span several chunks -- nine for
 * TITLE.STR -- so the job here is to gather the chunks of a frame and hand back
 * its data in one piece.
 *
 * WHERE THIS DEPARTS FROM THE HARDWARE, and why it does not matter: the real
 * library fills the ring sector by sector from the CD interrupt, and callers
 * reach into it. Here the sectors arrive from the drive pump at the frame
 * boundary (native/platform/libds.c) and each frame is assembled into one
 * contiguous span of the caller's ring. Nothing above StGetNext can tell the
 * difference -- the game only ever uses `addr` as a frame pointer and hands it
 * straight back to StFreeRing -- and it means DecDCTvlc2 sees the bitstream as
 * the flat run of bytes it expects instead of one interrupted every 2016 bytes
 * by the next chunk's header.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define ST_CHUNK   2048   /* what the drive delivers per sector */
#define ST_HDR     32     /* STR chunk header */
#define ST_PAYLOAD (ST_CHUNK - ST_HDR)   /* 2016 bytes of frame data */

#define STR_ID   0x0160   /* header word 0 ... */
#define STR_TYPE 0x8001   /* ... and word 1: together the 0x80010160 magic */

typedef struct {
    unsigned char minute, second, sector, track;
} DslLOC;

/* Mirrors StHEADER in include/psx/libds.h. */
typedef struct {
    unsigned short id;
    unsigned short type;
    unsigned short secCount;    /* this chunk's index within the frame */
    unsigned short nSectors;    /* chunks per frame */
    unsigned long  frameCount;  /* frame number */
    unsigned long  frameSize;   /* frame data size, in bytes */
    unsigned short width;
    unsigned short height;
    unsigned long  dummy1;
    unsigned long  dummy2;
    DslLOC loc;
} StHEADER;

void psx_cd_set_stream_sink(void (*sink)(const unsigned char*));
void psx_cd_service(void);
int  psx_cd_cur_lba(void);
DslLOC* DsIntToPos(int i, DslLOC* p);

/* The CD interrupt flag the game polls (see _decDCToutCallback). Sectors reach
 * us from the drive pump rather than an interrupt handler, so there is never
 * deferred work outstanding and this stays clear. */
int StCdIntrFlag;

/* Frame assembly. Slots are carved out of the ring the caller gave us; four is
 * enough to keep the drive busy while a frame is being decoded, and TITLE.STR's
 * 0x20000-byte ring leaves 32KB per slot for frames that never exceed 18KB. */
#define ST_SLOTS 4

enum { SLOT_FREE = 0, SLOT_FILLING, SLOT_READY };

typedef struct {
    int state;
    int frameno;
    int nchunks;
    unsigned int have;     /* bitmask of chunks received */
    int len;               /* frame data size from the header */
    unsigned long seq;     /* arrival order, so frames come out in order */
} slot_t;

static unsigned char* ring;
static int ring_bytes;
static int slot_bytes;
static slot_t slots[ST_SLOTS];
static unsigned long seq_counter;
static int stream_on;
static int last_frameno;
static int trace = -1;

static int tracing(void)
{
    if (trace < 0) trace = getenv("VS_STR_TRACE") ? 1 : 0;
    return trace;
}

static unsigned char* slot_base(int i) { return ring + (size_t)i * slot_bytes; }
static unsigned char* slot_data(int i) { return slot_base(i) + ST_HDR; }

static void slots_reset(void)
{
    int i;
    for (i = 0; i < ST_SLOTS; i++) {
        slots[i].state = SLOT_FREE;
        slots[i].have = 0;
    }
    seq_counter = 0;
    last_frameno = 0;
}

/* One 2048-byte chunk straight off the drive. */
static void stream_sector(const unsigned char* sec)
{
    const StHEADER* h = (const StHEADER*)sec;
    int chunk, nchunks, frameno, i, slot;

    if (!ring || !stream_on) return;
    if (tracing()) {
        static int shown;
        if (shown < 6) {
            shown++;
            printf("[str] sector: id=%04X type=%04X chunk=%d/%d frame=%lu size=%lu %ux%u\n",
                   h->id, h->type, h->secCount, h->nSectors,
                   (unsigned long)h->frameCount, (unsigned long)h->frameSize,
                   h->width, h->height);
        }
    }
    if (h->id != STR_ID || h->type != STR_TYPE) return;   /* not a video chunk */

    chunk = h->secCount;
    nchunks = h->nSectors;
    frameno = (int)h->frameCount;
    if (nchunks <= 0 || nchunks > 32 || chunk < 0 || chunk >= nchunks) return;
    if (ST_HDR + (size_t)nchunks * ST_PAYLOAD > (size_t)slot_bytes) return;

    /* Find the slot already collecting this frame, else start one. */
    slot = -1;
    for (i = 0; i < ST_SLOTS; i++)
        if (slots[i].state == SLOT_FILLING && slots[i].frameno == frameno) { slot = i; break; }

    if (slot < 0) {
        if (chunk != 0) return;      /* joined mid-frame: wait for the next head */
        for (i = 0; i < ST_SLOTS; i++)
            if (slots[i].state == SLOT_FREE) { slot = i; break; }
        if (slot < 0) return;        /* ring full: the drive gets ahead, drop it */
        slots[slot].state = SLOT_FILLING;
        slots[slot].frameno = frameno;
        slots[slot].nchunks = nchunks;
        slots[slot].have = 0;
        slots[slot].len = (int)h->frameSize;
        memcpy(slot_base(slot), sec, ST_HDR);
    }

    memcpy(slot_data(slot) + (size_t)chunk * ST_PAYLOAD, sec + ST_HDR, ST_PAYLOAD);
    slots[slot].have |= 1u << chunk;

    if (slots[slot].have == (nchunks >= 32 ? 0xFFFFFFFFu : (1u << nchunks) - 1u)) {
        slots[slot].state = SLOT_READY;
        slots[slot].seq = ++seq_counter;
        last_frameno = frameno;
        if (tracing())
            printf("[str] frame %d complete (%d chunks, %d bytes, %dx%d)\n",
                   frameno, nchunks, slots[slot].len, h->width, h->height);
    }
}

void StSetRing(unsigned long* ring_addr, unsigned long ring_size)
{
    ring = (unsigned char*)ring_addr;
    ring_bytes = (int)(ring_size * ST_CHUNK);
    slot_bytes = ring_bytes / ST_SLOTS;
    slots_reset();
    if (tracing())
        printf("[str] ring %p, %lu sectors, %d bytes/slot\n",
               (void*)ring_addr, (unsigned long)ring_size, slot_bytes);
}

void StClearRing(void) { slots_reset(); }

void StUnSetRing(void)
{
    stream_on = 0;
    psx_cd_set_stream_sink(0);
    slots_reset();
    ring = 0;
}

void StSetStream(unsigned long mode, unsigned long start_frame, unsigned long end_frame,
    void (*func1)(void), void (*func2)(void))
{
    (void)mode; (void)start_frame; (void)end_frame; (void)func1; (void)func2;
    /* mode selects whether the library stops at end_frame or runs on; the movie
     * player asks for the whole stream and stops it itself, so there is nothing
     * to gate on here. */
    stream_on = 1;
    psx_cd_set_stream_sink(stream_sector);
}

void StSetMask(unsigned long mask, unsigned long start, unsigned long end)
{
    (void)mask; (void)start; (void)end;
}

/* 0 when a frame is ready. The oldest completed frame comes out first. */
unsigned long StGetNext(unsigned long** addr, unsigned long** header)
{
    int i, best = -1;

    /* Service the drive here as well as at the frame boundary. The movie player
     * polls this thousands of times without calling VSync while it waits for a
     * frame, and on hardware the sectors it is waiting for arrive on the drive's
     * own interrupt during exactly that wait. Delivery is rate-limited by
     * elapsed time inside libds, so polling harder does not read faster. */
    if (stream_on) psx_cd_service();

    for (i = 0; i < ST_SLOTS; i++)
        if (slots[i].state == SLOT_READY
            && (best < 0 || slots[i].seq < slots[best].seq))
            best = i;

    if (best < 0) return 1;
    if (addr) *addr = (unsigned long*)slot_data(best);
    if (header) *header = (unsigned long*)slot_base(best);
    return 0;
}

unsigned long StFreeRing(unsigned long* base)
{
    int i;
    for (i = 0; i < ST_SLOTS; i++) {
        if ((unsigned char*)base == slot_data(i)) {
            slots[i].state = SLOT_FREE;
            slots[i].have = 0;
            return 0;
        }
    }
    return 1;
}

/* The position the drive has reached. The movie player uses the return value to
 * notice a stream that has run away and to restart it from a known frame. */
int StGetBackloc(DslLOC* loc)
{
    if (loc) DsIntToPos(psx_cd_cur_lba(), loc);
    return last_frameno;
}

int StSetChannel(unsigned long channel) { (void)channel; return 0; }

/* On hardware this drains the drive's FIFO from the CD interrupt. Sectors here
 * are delivered by the drive pump instead, so there is nothing left to do. */
void StCdInterrupt(void) { StCdIntrFlag = 0; }
