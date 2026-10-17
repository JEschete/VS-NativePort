#pragma once

// On-disc pointers (tools/port/copy_fixups.py ONDISC_OFFSETS). The PS1 loaded a file into memory and turned each
// 32-bit offset in it into a pointer in place. Natively the member keeps the offset (the file layout, 4 bytes), and
// each use computes the pointer from the struct's address. Replaced by decoded content formats (plan D18).

#define VS_ONDISC(base, member, type) ((type)((char*)(base) + (base)->member))
#define VS_ONDISC_DOT(base, member, type) ((type)((char*)&(base) + (base).member))
