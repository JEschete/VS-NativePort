/* libapi: BIOS kernel services the menu path needs.
 *
 * Events are a small fixed table, mirroring the PS1 kernel EvCB: an event has
 * a class, a spec, a mode and an optional callback, and sits in one of three
 * states. DeliverEvent marks matching enabled events as delivered; TestEvent
 * consumes that flag. Callback-mode events invoke the handler immediately.
 *
 * Memory-card calls are backed by host files under saves/ rather than a raw
 * 128KB block device, which is enough for the save/load menus.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Event states (kernel values). */
#define EvSt_UNUSED   0x0000
#define EvSt_WAIT     0x1000
#define EvSt_ACTIVE   0x2000
#define EvSt_ALREADY  0x4000

/* Event modes. */
#define EvMd_INTR     0x1000   /* invoke callback */
#define EvMd_NOINTR   0x2000   /* set a flag for TestEvent */

#define MAX_EVENTS 32

typedef struct {
    unsigned long cls;
    long spec;
    long mode;
    long (*func)(void);
    long state;
} event_t;

static event_t events[MAX_EVENTS];

long OpenEvent(unsigned long cls, long spec, long mode, long (*func)(void))
{
    int i;
    for (i = 0; i < MAX_EVENTS; i++) {
        if (events[i].state == EvSt_UNUSED) {
            events[i].cls = cls;
            events[i].spec = spec;
            events[i].mode = mode;
            events[i].func = func;
            events[i].state = EvSt_WAIT;      /* opened but disabled */
            return (long)(0xF1000000u | (unsigned)i);
        }
    }
    return -1;
}

static event_t* ev(long id)
{
    unsigned i = (unsigned)id & 0xFFFF;
    if (((unsigned long)id & 0xFF000000u) != 0xF1000000u) return 0;
    if (i >= MAX_EVENTS) return 0;
    return &events[i];
}

long CloseEvent(long id)
{
    event_t* e = ev(id);
    if (!e) return 0;
    memset(e, 0, sizeof *e);
    e->state = EvSt_UNUSED;
    return 1;
}

long EnableEvent(long id)
{
    event_t* e = ev(id);
    if (!e) return 0;
    if (e->state != EvSt_UNUSED) e->state = EvSt_ACTIVE;
    return 1;
}

long DisableEvent(long id)
{
    event_t* e = ev(id);
    if (!e) return 0;
    if (e->state != EvSt_UNUSED) e->state = EvSt_WAIT;
    return 1;
}

/* Returns non-zero once when the event has been delivered, then clears it. */
long TestEvent(long id)
{
    event_t* e = ev(id);
    if (!e) return 0;
    if (e->state == EvSt_ALREADY) {
        e->state = EvSt_ACTIVE;
        return 1;
    }
    return 0;
}

void DeliverEvent(unsigned long cls, unsigned long spec)
{
    int i;
    for (i = 0; i < MAX_EVENTS; i++) {
        event_t* e = &events[i];
        if (e->state != EvSt_ACTIVE) continue;
        if (e->cls != cls || (unsigned long)e->spec != spec) continue;
        if (e->mode == EvMd_INTR) {
            if (e->func) e->func();
        } else {
            e->state = EvSt_ALREADY;
        }
    }
}

void UnDeliverEvent(unsigned long cls, unsigned long spec)
{
    int i;
    for (i = 0; i < MAX_EVENTS; i++) {
        event_t* e = &events[i];
        if (e->state == EvSt_ALREADY && e->cls == cls
            && (unsigned long)e->spec == spec)
            e->state = EvSt_ACTIVE;
    }
}

/* ---- memory card ----
 * Host-backed: one file per card channel. Enough for the save/load menus;
 * a raw block-device model can replace this later if anything needs it. */

struct DIRENTRY {
    char name[20];
    long attr;
    long size;
    struct DIRENTRY* next;
    long head;
    char system[4];
};

#define CARD_DIR "saves"

long _card_info(long chan)  { (void)chan; return 1; }
long _card_clear(long chan) { (void)chan; return 1; }
long _card_load(long chan)  { (void)chan; return 1; }
long _card_format(long chan){ (void)chan; return 1; }

/* ---- raw frame I/O, host-file-backed ("virtual memory card") ----
 * A PS1 card is 0x400 frames of 128 bytes (128 KB). The BIOS primitives
 * _card_read/_card_write transfer one frame and complete asynchronously,
 * signalling via the card event classes. We back channel 0 (slot 1) with a
 * real .mcd image so VS's own save-load code reads genuine save data; the
 * transfer is synchronous here, so we deliver the completion event inline.
 *
 * Standard PSY-Q card event classes/specs:
 *   SwCARD 0xF4000001, HwCARD 0xF0000011; EvSpIOE(complete)=0x0004, error=0x8000. */
#define CARD_FRAME    128
#define CARD_FRAMES   0x400
#define CARD_PATH     "native/saves/test_card.mcd"
#define EvSwCARD      0xF4000001UL
#define EvHwCARD      0xF0000011UL
#define EvSpIOE       0x0004UL
#define EvSpERROR     0x8000UL

static FILE* card_file(void)
{
    static FILE* fp = NULL;
    static int   tried = 0;
    if (!fp && !tried) {
        tried = 1;
        fp = fopen(CARD_PATH, "r+b");   /* read/write, must exist */
        if (!fp) fprintf(stderr, "[memcard] no card image at %s\n", CARD_PATH);
    }
    return fp;
}

static void card_complete(int ok)
{
    unsigned long spec = ok ? EvSpIOE : EvSpERROR;
    DeliverEvent(EvHwCARD, spec);
    DeliverEvent(EvSwCARD, spec);
}

long _card_read(long chan, long sector, unsigned char* buf)
{
    FILE* fp = card_file();
    (void)chan;
    if (fp && sector >= 0 && sector < CARD_FRAMES
        && fseek(fp, sector * CARD_FRAME, SEEK_SET) == 0
        && fread(buf, 1, CARD_FRAME, fp) == CARD_FRAME) {
        card_complete(1);
        return 1;
    }
    card_complete(0);
    return 0;
}

long _card_write(long chan, long sector, unsigned char* buf)
{
    FILE* fp = card_file();
    (void)chan;
    if (fp && sector >= 0 && sector < CARD_FRAMES
        && fseek(fp, sector * CARD_FRAME, SEEK_SET) == 0
        && fwrite(buf, 1, CARD_FRAME, fp) == CARD_FRAME) {
        fflush(fp);
        card_complete(1);
        return 1;
    }
    card_complete(0);
    return 0;
}

/* Card present + not-new. Bit layout mirrors what StartCARD's poll expects:
 * 0 = connected & ready. Returns 0 when the image is missing so the menus
 * report "no card" instead of hanging. */
long _card_status(long chan) { (void)chan; return card_file() ? 0 : 1; }

/* ---- PS1 memory-card filesystem over the .mcd image ----
 * Block 0 is the directory: frame 0 is a header, frames 1..15 are one entry per
 * data block. Directory entry at 0x80*n: [0x00]=u32 state (0x51 first-in-use,
 * 0x52 middle, 0x53 last, 0xA*=free), [0x04]=u32 file size, [0x08]=u16 next
 * block (0xFFFF terminator), [0x0A]=NUL-terminated name. Data block n lives at
 * 0x2000*n. A file is a chain of 8 KB blocks. The whole image is cached in RAM
 * and flushed back on write. The game's open/read/write/close/lseek reach here
 * via the vs_bios_* asm labels in <libapi.h>; erase/firstfile/nextfile keep
 * their names (no POSIX clash). */
#define MCD_BLOCK   0x2000
#define MCD_BLOCKS  16
#define MCD_SIZE    (MCD_BLOCK * MCD_BLOCKS)

static unsigned char mcd_img[MCD_SIZE];
static int mcd_loaded = 0;

static unsigned char* mcd_image(void)
{
    FILE* fp;
    if (mcd_loaded) return mcd_img;
    fp = card_file();
    if (!fp) return 0;
    if (fseek(fp, 0, SEEK_SET) != 0 || fread(mcd_img, 1, MCD_SIZE, fp) != MCD_SIZE)
        return 0;
    mcd_loaded = 1;
    return mcd_img;
}

static void mcd_flush(void)
{
    FILE* fp = card_file();
    if (fp && mcd_loaded) {
        fseek(fp, 0, SEEK_SET);
        fwrite(mcd_img, 1, MCD_SIZE, fp);
        fflush(fp);
    }
}

static unsigned mcd_state(unsigned char* img, int b) { return *(unsigned*)(img + 0x80 * b); }
static unsigned mcd_fsize(unsigned char* img, int b) { return *(unsigned*)(img + 0x80 * b + 4); }
/* The directory "next block" field is a 0-based DATA-block index (data block 1
 * is stored as 0), so the next block NUMBER is raw + 1; 0xFFFF terminates. */
static int      mcd_next (unsigned char* img, int b)
{ int raw = *(unsigned short*)(img + 0x80 * b + 8); return raw == 0xFFFF ? 0xFFFF : raw + 1; }
static char*    mcd_name (unsigned char* img, int b) { return (char*)(img + 0x80 * b + 0x0A); }
static int      mcd_isfirst(unsigned char* img, int b)
{ unsigned s = mcd_state(img, b); return (s & 0xF0) == 0x50 && (s & 0xF) == 1; }

/* strip a "buNN:" (or any "dev:") device prefix */
static const char* mcd_basename(const char* path)
{
    const char* c = path;
    for (; *c; c++) if (*c == ':') return c + 1;
    return path;
}

#define MCD_MAXFD 8
static struct { int used, block; unsigned pos, size; } mcd_fd[MCD_MAXFD];

long vs_bios_open(char* name, unsigned long mode)
{
    unsigned char* img = mcd_image();
    const char* base;
    int b, fd;
    (void)mode;
    if (!img) return -1;
    base = mcd_basename(name);
    for (b = 1; b < MCD_BLOCKS; b++) {
        if (mcd_isfirst(img, b) && strcmp(mcd_name(img, b), base) == 0) {
            for (fd = 0; fd < MCD_MAXFD; fd++) {
                if (!mcd_fd[fd].used) {
                    mcd_fd[fd].used = 1;
                    mcd_fd[fd].block = b;
                    mcd_fd[fd].pos = 0;
                    mcd_fd[fd].size = mcd_fsize(img, b);
                    return fd + 3;               /* keep 0/1/2 for std streams */
                }
            }
            return -1;                            /* no free descriptor */
        }
    }
    return -1;                                    /* not found (new-file create: TODO) */
}

long vs_bios_read(long fd, void* buf, long n)
{
    unsigned char* img = mcd_image();
    int i = (int)fd - 3, blk, boff, got = 0;
    if (!img || i < 0 || i >= MCD_MAXFD || !mcd_fd[i].used) return -1;
    if ((unsigned)(mcd_fd[i].pos + n) > mcd_fd[i].size) n = (long)mcd_fd[i].size - mcd_fd[i].pos;
    if (n < 0) n = 0;
    blk = mcd_fd[i].block;
    boff = (int)mcd_fd[i].pos;
    while (boff >= MCD_BLOCK) { blk = mcd_next(img, blk); boff -= MCD_BLOCK;
                               if (blk <= 0 || blk >= MCD_BLOCKS) return got; }
    while (n > 0) {
        int chunk = MCD_BLOCK - boff;
        if (chunk > n) chunk = (int)n;
        memcpy((char*)buf + got, img + MCD_BLOCK * blk + boff, chunk);
        got += chunk; n -= chunk; mcd_fd[i].pos += chunk; boff += chunk;
        if (boff >= MCD_BLOCK && n > 0) { blk = mcd_next(img, blk); boff = 0;
                                          if (blk <= 0 || blk >= MCD_BLOCKS) break; }
    }
    return got;
}

long vs_bios_write(long fd, void* buf, long n)
{
    unsigned char* img = mcd_image();
    int i = (int)fd - 3, blk, boff, put = 0;
    if (!img || i < 0 || i >= MCD_MAXFD || !mcd_fd[i].used) return -1;
    blk = mcd_fd[i].block;
    boff = (int)mcd_fd[i].pos;
    while (boff >= MCD_BLOCK) { blk = mcd_next(img, blk); boff -= MCD_BLOCK;
                               if (blk <= 0 || blk >= MCD_BLOCKS) return put; }
    while (n > 0) {
        int chunk = MCD_BLOCK - boff;
        if (chunk > n) chunk = (int)n;
        memcpy(img + MCD_BLOCK * blk + boff, (char*)buf + put, chunk);
        put += chunk; n -= chunk; mcd_fd[i].pos += chunk; boff += chunk;
        if (boff >= MCD_BLOCK && n > 0) { blk = mcd_next(img, blk); boff = 0;
                                          if (blk <= 0 || blk >= MCD_BLOCKS) break; }
    }
    mcd_flush();
    return put;
}

long vs_bios_lseek(long fd, long off, long whence)
{
    int i = (int)fd - 3;
    if (i < 0 || i >= MCD_MAXFD || !mcd_fd[i].used) return -1;
    if (whence == 0)      mcd_fd[i].pos = (unsigned)off;              /* SEEK_SET */
    else if (whence == 1) mcd_fd[i].pos += (unsigned)off;            /* SEEK_CUR */
    else                  mcd_fd[i].pos = mcd_fd[i].size + off;      /* SEEK_END */
    return (long)mcd_fd[i].pos;
}

long vs_bios_close(long fd)
{
    int i = (int)fd - 3;
    if (i >= 0 && i < MCD_MAXFD) mcd_fd[i].used = 0;
    return 0;
}

/* Directory enumeration. firstfile(pattern, dir) matches the basename against
 * the leading, pre-'*' portion of the pattern; nextfile continues. VS uses this
 * to list its save files, then filters by name itself. */
static int  dir_blk;
static char dir_prefix[24];

static int mcd_prefix_match(const char* name)
{
    return dir_prefix[0] == 0 || strncmp(name, dir_prefix, strlen(dir_prefix)) == 0;
}

struct DIRENTRY* nextfile(struct DIRENTRY* dir)
{
    unsigned char* img = mcd_image();
    if (!img || !dir) return 0;
    for (; dir_blk < MCD_BLOCKS; dir_blk++) {
        if (mcd_isfirst(img, dir_blk) && mcd_prefix_match(mcd_name(img, dir_blk))) {
            memset(dir, 0, sizeof *dir);
            strncpy(dir->name, mcd_name(img, dir_blk), sizeof dir->name - 1);
            dir->size = (long)mcd_fsize(img, dir_blk);
            dir->head = dir_blk;
            dir_blk++;
            return dir;
        }
    }
    return 0;
}

struct DIRENTRY* firstfile(char* name, struct DIRENTRY* dir)
{
    const char* base = name ? mcd_basename(name) : "";
    const char* star = strchr(base, '*');
    size_t plen = star ? (size_t)(star - base) : strlen(base);
    if (plen >= sizeof dir_prefix) plen = sizeof dir_prefix - 1;
    memcpy(dir_prefix, base, plen);
    dir_prefix[plen] = 0;
    dir_blk = 1;
    return nextfile(dir);
}

long erase(char* name)
{
    unsigned char* img = mcd_image();
    const char* base;
    int b;
    if (!img) return 0;
    base = mcd_basename(name);
    for (b = 1; b < MCD_BLOCKS; b++) {
        if (mcd_isfirst(img, b) && strcmp(mcd_name(img, b), base) == 0) {
            int cur = b;
            while (cur > 0 && cur < MCD_BLOCKS) {
                int last = (mcd_state(img, cur) & 0xF) == 3;   /* read before clearing */
                int nxt  = mcd_next(img, cur);
                *(unsigned*)(img + 0x80 * cur) = 0xA0;         /* mark free */
                if (last || nxt == 0xFFFF) break;
                cur = nxt;
            }
            mcd_flush();
            return 1;
        }
    }
    return 0;
}
