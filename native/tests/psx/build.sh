#!/usr/bin/env bash
# Build a PS-EXE from a single .S source. Usage: build.sh <src.S> <out.exe>
set -euo pipefail
SRC="$1"; OUT="$2"
DIR="$(cd "$(dirname "$0")" && pwd)"
ELF="$(mktemp -u /tmp/psx_XXXX.elf)"; BIN="${ELF%.elf}.bin"
mipsel-linux-gnu-gcc -march=mips1 -mabi=32 -msoft-float -nostdlib -fno-pic \
    -mno-abicalls -G0 -Wl,--build-id=none -Wl,-T,"$DIR/link.ld" -o "$ELF" "$SRC"
ENTRY=$(mipsel-linux-gnu-readelf -h "$ELF" | awk "/Entry point/{print \$4}")
mipsel-linux-gnu-objcopy -O binary "$ELF" "$BIN"
python3 "$DIR/../../tools/mkpsexe.py" "$BIN" "$OUT" "$ENTRY"
echo "  entry: $ENTRY"
