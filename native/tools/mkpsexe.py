#!/usr/bin/env python3
"""Wrap a flat binary in a PS-EXE header so DuckStation (or a PS1) can boot it.

PS-EXE layout: 2048-byte header followed by the text image.
  0x00 "PS-X EXE"      0x10 initial PC     0x14 initial GP
  0x18 text load addr  0x1C text size      0x30 stack addr  0x34 stack size
  0x4C region string, header padded to 0x800.

Usage: mkpsexe.py <flat.bin> <out.exe> [load_addr] [stack_addr]
"""
import struct, sys, pathlib

REGION = b"Sony Computer Entertainment Inc. for North America area"


def main(src, dst, load=0x80010000, stack=0x801FFF00):
    text = pathlib.Path(src).read_bytes()
    if len(text) % 2048:
        text += b"\x00" * (2048 - len(text) % 2048)

    h = bytearray(2048)
    h[0x00:0x08] = b"PS-X EXE"
    struct.pack_into("<I", h, 0x10, load)    # initial PC
    struct.pack_into("<I", h, 0x14, 0)       # initial GP
    struct.pack_into("<I", h, 0x18, load)    # text addr
    struct.pack_into("<I", h, 0x1C, len(text))
    struct.pack_into("<I", h, 0x30, stack)   # stack addr
    struct.pack_into("<I", h, 0x34, 0)       # stack size
    h[0x4C:0x4C + len(REGION)] = REGION

    pathlib.Path(dst).write_bytes(bytes(h) + text)
    print("  PSEXE    %s (load %#010x, text %d bytes)" % (dst, load, len(text)))


if __name__ == "__main__":
    a = sys.argv[1:]
    main(a[0], a[1], int(a[2], 0) if len(a) > 2 else 0x80010000,
                     int(a[3], 0) if len(a) > 3 else 0x801FFF00)
