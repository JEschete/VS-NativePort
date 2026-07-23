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

/* Directory enumeration. The menus use firstfile/nextfile to list saves; with
 * no host save directory yet this reports an empty card, which is a valid
 * state the menus already handle. */
static struct DIRENTRY* dir_cursor;

struct DIRENTRY* firstfile(char* name, struct DIRENTRY* dir)
{
    (void)name;
    dir_cursor = 0;
    if (dir) memset(dir, 0, sizeof *dir);
    return 0;
}

struct DIRENTRY* nextfile(struct DIRENTRY* dir)
{
    (void)dir;
    return 0;
}

long erase(char* name) { (void)name; return 0; }
