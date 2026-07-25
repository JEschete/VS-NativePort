/* libds: PSYQ's low-level CD-ROM driver, backed by the retail disc image.
 *
 * This is the layer the port has to own. Everything ABOVE it is decompiled game
 * code that runs unmodified: _processCdQueue -> func_800443CC (the disk state
 * machine) -> _diskInitRead. Everything BELOW it on hardware is the CD
 * controller. libds itself is undecompiled PSYQ library assembly, so natively
 * every Ds* entry point is ours to implement.
 *
 * The protocol the game drives, and that this reproduces:
 *
 *   DsIntToPos(lba, &loc)                      LBA -> BCD minute/second/sector
 *   DsPacket(mode, &loc, DslReadN, cmdcb, -1)  queue a read command
 *      -> cmdcb(DslComplete, result)           command accepted
 *         -> DsStartReadySystem(readycb, -1)   arm per-sector delivery
 *   ... once per sector ...
 *   readycb(DslDataReady, result)              a sector arrived
 *      -> DsGetSector(dest, 512)               game copies 2048 bytes out
 *   DsEndReadySystem()                         disarm
 *
 * Sectors are delivered from VSync() -- the frame boundary, which is where the
 * CD interrupt would land, and which the game already pumps via
 * vs_main_gametimeUpdate() -> VSync() -> _processCdQueue().
 *
 * WHERE THE BYTES COME FROM. The port does not require the disc image at
 * runtime. Reads are served from the extracted `data/` tree via disc_index.c
 * (generated from build/src/include/lbas.h + config/SLUS-01040.xml, which the
 * normal build already produces from the user's own disc): each LBA range maps
 * back to an ordinary file. Only if a sector falls outside that map -- and only
 * if the user has one -- do we fall back to the raw disc image.
 *
 * That keeps the PlayStation-accurate interface (the game asks by LBA, because
 * that is what the hardware does) without dragging the .bin along.
 *
 * Disc geometry, for the fallback path: the .bin is MODE2/2352. Sector N starts
 * at N*2352; the 2048 bytes of user data start 24 bytes in (12 sync + 4 header
 * + 8 subheader). Verified against the image: LBA 16 is the ISO9660 PVD
 * ("\x01CD001...PLAYSTATION") and LBA 23 is SYSTEM.CNF, matching lbas.h.
 */
#include "disc_index.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SECTOR_RAW  2352
#define SECTOR_DATA 2048
#define SECTOR_HDR  24
/* MODE2/FORM2 as extracted: subheader + 2324 data + EDC, no sync or header. */
#define SECTOR_RAW_F2 2336
#define SUBHEADER     8

/* From include/psx/libds.h -- the platform layer is built without the PSYQ
 * headers, so the handful of constants we need are restated here. */
#define DslDataReady 0x01
#define DslComplete  0x02
#define DslDiskError 0x05
#define DslNoResult  0x06
#define DslReady     1
#define DslReadN     0x06
#define DslReadS     0x1B
#define DslSeekL     0x15
#define DslPause     0x09

typedef struct {
    unsigned char minute, second, sector, track;
} DslLOC;

typedef void (*DslCB)(unsigned char intr, unsigned char* result);
typedef void (*DslRCB)(unsigned char intr, unsigned char* result);

/* Authentic 2x CD-ROM: 150 sectors/second = 2.5 per 60Hz frame. Overridable
 * with VS_CD_SPEED (sectors per frame) to skip load times while iterating. */
#define SECTORS_PER_FRAME_X2Q 10    /* quarter-sectors: 10/4 = 2.5 */

static FILE*  disc;
static long   disc_sectors;
static int    cur_lba;              /* head position */
static int    reading;              /* a DslReadN is streaming */
static DslRCB ready_sys_cb;         /* per-sector callback (ready system) */
static DslCB  ready_cb;             /* DsReadyCallback -- XA/pcm path */
static int    cmd_id = 1;
static unsigned char sector_buf[SECTOR_DATA];
static unsigned char result_buf[8];
static int    speed_q;              /* quarter-sectors per frame */
static int    accum_q;
static long   sectors_read;

/* Deferred command completion: DsPacket must not call back re-entrantly (the
 * game issues it from inside its own state machine), so completion is raised on
 * the next pump. */
static DslCB  pending_cmd_cb;
static int    pending_seek_only;

/* When the streaming layer is armed it consumes every delivered sector; see
 * psx_cd_set_stream_sink below. */
static void (*stream_sink)(const unsigned char*);

/* VS_CD_TRACE=1 logs every command and the sector flow. The CD is the spine of
 * the boot sequence, and a stalled read looks exactly like a hang from outside. */
static int trace;
#define CDTRACE(...) do { if (trace) printf(__VA_ARGS__); } while (0)

/* Optional raw-image fallback, only consulted for sectors the index does not
 * cover (and only if the user points us at an image). */
static void open_disc(void)
{
    const char* path = getenv("VS_DISC");
    if (disc || !path || !*path) return;
    disc = fopen(path, "rb");
    if (!disc) {
        printf("[libds] VS_DISC set but not readable: %s\n", path);
        return;
    }
    fseek(disc, 0, SEEK_END);
    disc_sectors = ftell(disc) / SECTOR_RAW;
    fseek(disc, 0, SEEK_SET);
    printf("[libds] raw disc image available: %s (%ld sectors)\n", path, disc_sectors);
}

static const char* data_dir(void)
{
    const char* d = getenv("VS_DATA_DIR");
    return (d && *d) ? d : "data";
}

/* Find the file containing this LBA: the last entry whose range starts at or
 * before it. The table is sorted and non-overlapping. */
static const psx_disc_entry* find_entry(int lba)
{
    int lo = 0, hi = psx_disc_index_count - 1, best = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (psx_disc_index[mid].lba <= lba) { best = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    if (best < 0) return 0;
    if (lba >= psx_disc_index[best].lba + psx_disc_index[best].sectors) return 0;
    return &psx_disc_index[best];
}

/* One open file, cached: reads within a load are sequential, so this turns the
 * whole transfer into one fopen and a stream of freads. */
static FILE*       cached_fp;
static const char* cached_path;

/* Stride of the cached file, and where its 2048 bytes of user data begin.
 *
 * Most files are plain MODE2/FORM1: one 2048-byte sector per 2048 bytes of
 * file. Streaming files (TITLE.STR) are MODE2/FORM2 and were extracted with
 * their raw 2336-byte sectors intact -- 8 bytes of CD-XA subheader, 2324 of
 * data, 4 of EDC -- because the subheader is what distinguishes video sectors
 * from the interleaved XA audio. The 2048 bytes the drive hands the streaming
 * layer are the 32-byte STR chunk header plus 2016 bytes of frame data, which
 * start immediately after the subheader.
 *
 * The two cases are told apart by the data rather than a hardcoded list. A
 * whole number of 2336-byte sectors is necessary but not sufficient, so the
 * deciding evidence is the subheader itself: CD-XA writes it twice, back to
 * back, precisely so it can be recovered from a damaged sector. Four bytes
 * repeated at the very start of the file, in a file that is an exact multiple
 * of the form 2 sector size, means form 2.
 *
 * Note the index cannot help here: it sizes each file by the gap to the next
 * one on the disc, not by its ISO length, so its sector count says nothing
 * about the stride. */
static int    cached_stride;
static int    cached_skip;

static int read_from_data(const psx_disc_entry* e, int lba, unsigned char* out)
{
    char path[512];
    size_t got;

    if (cached_path != e->path) {
        if (cached_fp) fclose(cached_fp);
        snprintf(path, sizeof path, "%s/%s", data_dir(), e->path);
        cached_fp = fopen(path, "rb");
        cached_path = cached_fp ? e->path : 0;
        if (!cached_fp) return 0;
        cached_stride = SECTOR_DATA;
        cached_skip = 0;
        if (fseek(cached_fp, 0, SEEK_END) == 0) {
            long size = ftell(cached_fp);
            unsigned char sub[SUBHEADER];
            if (size > 0 && size % SECTOR_RAW_F2 == 0
                && fseek(cached_fp, 0, SEEK_SET) == 0
                && fread(sub, 1, SUBHEADER, cached_fp) == SUBHEADER
                && memcmp(sub, sub + 4, 4) == 0) {
                cached_stride = SECTOR_RAW_F2;
                cached_skip = SUBHEADER;
                CDTRACE("[libds] %s is MODE2/FORM2 (%ld sectors)\n",
                        e->path, size / SECTOR_RAW_F2);
            }
        }
    }
    if (fseek(cached_fp, (long)(lba - e->lba) * cached_stride + cached_skip, SEEK_SET) != 0)
        return 0;
    got = fread(out, 1, SECTOR_DATA, cached_fp);
    /* The last sector of a file is partial on disc too; pad it. */
    if (got < SECTOR_DATA) memset(out + got, 0, SECTOR_DATA - got);
    return 1;
}

/* Pull one sector's user data: extracted file first, raw image as fallback. */
static int read_sector(int lba, unsigned char* out)
{
    const psx_disc_entry* e;

    if (lba < 0) return 0;
    e = find_entry(lba);
    if (e && read_from_data(e, lba, out)) return 1;

    if (disc && lba < disc_sectors) {
        if (fseek(disc, (long)lba * SECTOR_RAW + SECTOR_HDR, SEEK_SET) != 0) return 0;
        return fread(out, 1, SECTOR_DATA, disc) == SECTOR_DATA;
    }
    if (!e) {
        static int warned;
        if (warned++ < 8)
            printf("[libds] LBA %d is not in the extracted data index%s\n", lba,
                   disc ? "" : " (set VS_DISC to fall back to a disc image)");
    }
    return 0;
}

/* --- BCD position helpers. PSYQ's convention includes the 150-sector (2s)
 * lead-in, and DsPosToInt is its exact inverse. --- */
static unsigned char itob(int i) { return (unsigned char)(((i / 10) << 4) | (i % 10)); }
static int btoi(unsigned char b) { return ((b >> 4) * 10) + (b & 0x0F); }

DslLOC* DsIntToPos(int i, DslLOC* p)
{
    int t = i + 150;
    p->minute = itob(t / 4500);
    t %= 4500;
    p->second = itob(t / 75);
    p->sector = itob(t % 75);
    p->track  = 0;
    return p;
}

int DsPosToInt(DslLOC* p)
{
    return (btoi(p->minute) * 4500) + (btoi(p->second) * 75) + btoi(p->sector) - 150;
}

int DsInit(void)
{
    const char* e = getenv("VS_CD_SPEED");
    trace = getenv("VS_CD_TRACE") != 0;
    open_disc();
    speed_q = e && atoi(e) > 0 ? atoi(e) * 4 : SECTORS_PER_FRAME_X2Q;
    accum_q = 0;
    reading = 0;
    ready_sys_cb = 0;
    ready_cb = 0;
    pending_cmd_cb = 0;
    printf("[libds] %d files indexed under %s/\n", psx_disc_index_count, data_dir());
    return 0;
}

int  DsReset(void) { return DsInit(); }
void DsClose(void)
{
    if (disc) { fclose(disc); disc = 0; }
    if (cached_fp) { fclose(cached_fp); cached_fp = 0; cached_path = 0; }
}
void DsFlush(void) { pending_cmd_cb = 0; }
int  DsSystemStatus(void) { return DslReady; }
int  DsQueueLen(void) { return 0; }
unsigned char DsStatus(void) { return 0; }
int  DsShellOpen(void) { return 0; }
int  DsSetDebug(int level) { (void)level; return 0; }
int  DsMix(void* vol) { (void)vol; return 1; }        /* CD audio mix: Phase 3b */
int  DsDataSync(int mode) { (void)mode; return 0; }
int  DsReadySystemMode(int mode) { (void)mode; return 0; }
int  DsSync(int id, unsigned char* result) { (void)id; (void)result; return DslComplete; }
int  DsReady(int* result) { (void)result; return DslNoResult; }
unsigned char DsLastCom(void) { return 0; }

DslCB DsReadyCallback(DslCB func)
{
    DslCB old = ready_cb;
    ready_cb = func;
    return old;
}

DslCB DsSyncCallback(DslCB func) { (void)func; return 0; }
DslCB DsReadCallback(DslCB func) { (void)func; return 0; }

/* Arm per-sector delivery. Called by the game's command-complete callback. */
int DsStartReadySystem(DslRCB func, int count)
{
    (void)count;
    ready_sys_cb = func;
    reading = 1;
    accum_q = 0;
    CDTRACE("[libds] ready system armed at lba=%d\n", cur_lba);
    return 1;
}

void DsEndReadySystem(void)
{
    ready_sys_cb = 0;
    reading = 0;
}

/* Queue a command. Completion is raised on the next pump, never re-entrantly. */
int DsPacket(unsigned char mode, DslLOC* pos, unsigned char com, DslCB func, int count)
{
    (void)mode; (void)count;
    if (pos) cur_lba = DsPosToInt(pos);
    switch (com) {
    case DslReadN:
    case DslReadS:
        pending_seek_only = 0;
        break;
    case DslSeekL:
        pending_seek_only = 1;
        break;
    default:
        pending_seek_only = 1;
        break;
    }
    pending_cmd_cb = func;
    if (!func) reading = (com == DslReadN || com == DslReadS);
    CDTRACE("[libds] DsPacket com=%02X lba=%d cb=%p\n", com, cur_lba, (void*)func);
    return cmd_id++;
}

int DsCommand(unsigned char com, unsigned char* param, DslCB cbsync, int count)
{
    (void)param; (void)count;
    if (com == DslPause) reading = 0;
    pending_cmd_cb = cbsync;
    pending_seek_only = 1;
    return cmd_id++;
}

int DsControl(unsigned char com, unsigned char* param, unsigned char* result)
{
    (void)com; (void)param; (void)result;
    return 1;
}
int DsControlB(unsigned char com, unsigned char* param, unsigned char* result)
{
    return DsControl(com, param, result);
}
int DsControlF(unsigned char com, unsigned char* param)
{
    return DsControl(com, param, 0);
}

/* Copy the sector just delivered. `size` is in 32-bit words, as on hardware. */
int DsGetSector(void* madr, int size)
{
    int bytes = size * 4;
    if (bytes > SECTOR_DATA) bytes = SECTOR_DATA;
    if (madr) memcpy(madr, sector_buf, (size_t)bytes);
    return 1;
}
int DsGetSector2(void* madr, int size) { return DsGetSector(madr, size); }

int DsRead(DslLOC* pos, int sectors, unsigned long* buf, int mode)
{
    /* Synchronous bulk read -- not on the boot path, but cheap to support. */
    int i;
    (void)mode;
    if (pos) cur_lba = DsPosToInt(pos);
    for (i = 0; i < sectors; i++)
        if (read_sector(cur_lba + i, ((unsigned char*)buf) + i * SECTOR_DATA))
            sectors_read++;
    cur_lba += sectors;
    return sectors;
}
int DsRead2(DslLOC* pos, int mode)
{
    (void)mode;
    if (pos) cur_lba = DsPosToInt(pos);
    reading = 1;
    return 1;
}
int  DsReadSync(unsigned char* result) { (void)result; return 0; }
void DsReadBreak(void) { reading = 0; }

DslLOC* DsLastPos(DslLOC* p) { return DsIntToPos(cur_lba, p); }

/* --- sector delivery ---
 *
 * Paced by wall-clock time rather than by frames. That distinction is not
 * cosmetic: the drive spins at its own rate and raises an interrupt per sector
 * no matter what the CPU is doing, and the movie player leans on exactly that.
 * _getNextMovieFrame and _decodeNextMovieFrame busy-wait thousands of
 * iterations for the next frame WITHOUT calling VSync, so delivery tied to the
 * frame boundary starves them: _playMovie then restarts the read from the top
 * of the stream, and the first frame never finishes arriving. Charging sectors
 * against elapsed time, and servicing the drive from the streaming layer's
 * polling loop as well as from VSync, reproduces the hardware's behaviour and
 * keeps the authentic 150 sectors/second. */
static unsigned long long cd_last_ns;
static unsigned long long cd_ns_accum;   /* time owed to the drive, nanoseconds */

static unsigned long long cd_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec;
}

#define FRAME_NS 16683333ull

void psx_cd_service(void)
{
    unsigned long long now = cd_now_ns();
    unsigned long long elapsed;
    unsigned long long ns_per_sector;
    int budget;

    if (!cd_last_ns) cd_last_ns = now;
    elapsed = now - cd_last_ns;
    cd_last_ns = now;

    /* NB: do not gate this on `disc` -- reads normally come from the extracted
     * data index and the raw image is only an optional fallback. */
    if (!reading || (!ready_sys_cb && !stream_sink)) return;

    /* Accumulate TIME, not sectors. This function is called from the movie
     * player's polling loop, so `elapsed` is routinely tens of nanoseconds --
     * converting each interval to sectors independently would floor to zero
     * every time and the drive would never advance. */
    if (elapsed > 10 * FRAME_NS) elapsed = 10 * FRAME_NS;   /* cap catch-up */
    cd_ns_accum += elapsed;

    ns_per_sector = FRAME_NS * 4ull / (unsigned long long)(speed_q > 0 ? speed_q : 1);
    budget = 0;
    while (cd_ns_accum >= ns_per_sector) {
        cd_ns_accum -= ns_per_sector;
        budget++;
    }

    while (budget-- > 0) {
        if (!reading || (!ready_sys_cb && !stream_sink)) break;
        if (!read_sector(cur_lba, sector_buf)) {
            /* Off the end of the disc: report an error rather than spinning. */
            if (ready_sys_cb && !stream_sink) ready_sys_cb(DslDiskError, result_buf);
            reading = 0;
            break;
        }
        cur_lba++;
        sectors_read++;
        /* While a stream is set up the drive's sectors belong to it: on
         * hardware they are routed by the XA filter straight into the ring
         * buffer, never surfacing as ready-system callbacks. */
        if (stream_sink) {
            stream_sink(sector_buf);
        } else {
            memset(result_buf, 0, sizeof result_buf);
            ready_sys_cb(DslDataReady, result_buf);
        }
    }
}

/* Called once per frame from VSync(). Command completions are raised here and
 * only here: the game issues its next command from inside the callback, so
 * keeping that on the frame boundary preserves the ordering the disk state
 * machine expects. */
void psx_cd_pump(void)
{
    if (pending_cmd_cb) {
        DslCB cb = pending_cmd_cb;
        pending_cmd_cb = 0;
        memset(result_buf, 0, sizeof result_buf);
        CDTRACE("[libds] command complete -> callback\n");
        cb(DslComplete, result_buf);
    }

    if (trace) {
        static int idle_frames;
        if (!reading) {
            if (++idle_frames % 120 == 0)
                printf("[libds] idle %d frames (lba=%d, ready_cb=%p)\n",
                       idle_frames, cur_lba, (void*)ready_sys_cb);
        } else {
            idle_frames = 0;
        }
    }

    psx_cd_service();
}

/* --- streaming hook (native/platform/libstream.c) --- */
void psx_cd_set_stream_sink(void (*sink)(const unsigned char*))
{
    stream_sink = sink;
}

int psx_cd_cur_lba(void) { return cur_lba; }

long psx_cd_sectors_read(void) { return sectors_read; }

/* libcd's CD-audio mixer, the one libcd entry point the game still calls
 * directly (everything else goes through libds). Volume routing is meaningless
 * while the SPU is silent; recorded for when Phase 3b turns it on. */
int CdMix(void* vol) { (void)vol; return 1; }
