#pragma once

// Memory card surface (libapi file calls + libcard, plan §6.7, strategy T), translated into Engine\Save.
// The libapi file calls share names with the C runtime (open, read, rename...), so the copy tool renames them
// to LegacyApi_*.

#include "Legacy/Core/PsxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DIRENTRY {
    char name[20];
    long attr;
    long size;
    struct DIRENTRY* next;
    long head;
    char system[4];
} DIRENTRY;

long LegacyApi_open(char* path, unsigned long mode);
long LegacyApi_close(long fd);
long LegacyApi_read(long fd, void* buf, long n);
long LegacyApi_write(long fd, void* buf, long n);
long LegacyApi_erase(char* path);
long LegacyApi_rename(char* from, char* to);
long LegacyApi_cd(char* path);
struct DIRENTRY* LegacyApi_firstfile(char* pattern, struct DIRENTRY* dir);
struct DIRENTRY* LegacyApi_nextfile(struct DIRENTRY* dir);
void InitCARD(long val);
long StartCARD(void);
long _card_info(long chan);
long _card_clear(long chan);
long _card_load(long chan);
long _card_format(long chan);

#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR 0x0003
#define O_CREAT 0x0200
#define O_NOWAIT 0x8000

#ifdef __cplusplus
}
#endif
