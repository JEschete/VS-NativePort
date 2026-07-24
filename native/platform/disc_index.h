/* LBA -> extracted-file map (see native/tools/gen_disc_index.py). */
#ifndef NATIVE_DISC_INDEX_H
#define NATIVE_DISC_INDEX_H

typedef struct {
    int         lba;        /* first sector of the file on the disc */
    int         sectors;    /* length in 2048-byte sectors */
    const char* path;       /* path under data/ */
} psx_disc_entry;

extern const psx_disc_entry psx_disc_index[];
extern const int psx_disc_index_count;

#endif
