# Vagrant Story: Native Port

A native PC port of **Vagrant Story** (PlayStation, 2000), built on top of a
matching decompilation of the original game.

The approach is the one Ship of Harkinian popularised: decompile the game to C,
replace the console's hardware layer with a host implementation, and compile the
*same source* for x86. The game's own decompiled code runs natively, with no
emulation and no binary translation. Only the PlayStation-specific libraries
underneath it are reimplemented.

This repository builds **two targets from one source tree**:

| Target | Command | Output |
|---|---|---|
| Matching PSX build | `make` | The retail overlays, verified **byte-identical** to the original disc |
| Native x86 build | `make -f Makefile.native` | `native/build/vagrant-native`, a Linux binary |

The matching build is the correctness oracle. Every change to `src/` has to keep
it byte-identical, which is what stops the port drifting away from the real game.

## Relationship to rood-reverse

This is a fork of **[ser-pounce/rood-reverse](https://github.com/ser-pounce/rood-reverse)**,
the decompilation project this work stands on. All of the decompiled game code,
the splat configuration, the build system and the toolchain setup come from
there, and upstream remains the source of decompilation progress. This fork
pulls from it regularly.

What this fork adds is everything under `native/`, plus the `docs/NATIVE_*`
files: the host hardware layer that turns the decompilation into a running
program. Please direct decompilation credit upstream.

## What you need

**A legally obtained copy of Vagrant Story (NTSC-U, `SLUS-01040`).**

No game code, assets, audio or text are distributed here, and none ever will be.
This repository contains only source code and build tooling. Everything the game
needs is extracted at build time from a disc image you supply yourself, dumped
from your own copy:

```
disks/SLUS-01040.bin
```

The first `make` extracts the disc contents into `data/`, which is gitignored.
Both builds read from there. The native port does not need the disc image at
runtime, because it serves data out of the extracted `data/` tree by LBA. Once
you have built once, the `.bin` can go back on the shelf.

## Development environment

The project is developed and tested on **Linux**, and on **Windows via WSL2**
(Ubuntu 24.04). The matching build needs a MIPS cross-compiler and a 1997-era
GCC, so a Unix environment is required.

### Packages

| Package | Minimum version |
|---|---|
| clang-format | 18.1.3 |
| cmake | 3.28.3 |
| docker-buildx | 29.1.3 |
| g++ | 13.3.0 |
| gcc-mipsel-linux-gnu | 12.4.0 |
| make | 4.3 |
| python3 / python3-venv | 3.12.3 |
| rustup | 1.26.0 |

Then:

- `sudo usermod -aG docker $USER`, and log out and back in
- `rustup default stable` if you have no Rust toolchain yet
- Put your disc image at `disks/SLUS-01040.bin`
- `make -j` to configure the remaining tooling under `tools/`, extract the disc,
  and perform the first build

The native build additionally wants a 32-bit host toolchain, and SDL2 at runtime
if you want a window. SDL2 is loaded dynamically, so it is not needed to build.

A **Nix flake** is provided as an alternative to installing the packages above.
Run `nix develop` for a pinned shell, then `make -j`.

### WSL specifics

Three things will bite you on WSL, all learned the hard way:

- **Keep the working tree inside the WSL filesystem** (`~/rood-reverse-work`,
  not `/mnt/c/...`). Copying a Windows git checkout into WSL rewrites line
  endings, and the matching build then fails in ways that look like code
  problems.
- **Be careful with `gcc-multilib`.** On Ubuntu, installing it can silently
  remove `gcc-mipsel-linux-gnu` and break the matching build. Verify the
  cross-compiler survived any multilib install.
- **WSL2 exposes no `/dev/input` at all.** Neither USB nor Bluetooth gamepads
  are passed through to the VM, so a controller plugged into Windows is
  invisible to a Linux binary. See [Controller](#controller) for the bridge that
  solves this.

## Building

### Matching PSX build

```sh
make -j              # incremental build + verify
make remake -j       # full re-split and rebuild, then verify
make format          # format sources and symbol addresses
make clean           # drop build/ and nonmatchings/
make clean-all       # full reset, including extracted data (keeps the disc image)
```

A successful run ends with **`✔ All files match`**, meaning all 21 target
binaries reproduce the retail disc byte for byte. If it prints `✘` for a file,
the change under test does not match.

Decompilation helpers:

```sh
make decompme TARGET=path/to/nonmatchings/source.s   # upload to decomp.me
make permute  TARGET=path/to/nonmatchings/source.s   # run the permuter
```

While matching a function, a full build is a slow feedback loop, because it only
tells you which `.PRG` differs. Read the object directly instead:

```sh
mipsel-linux-gnu-objdump -d --no-show-raw-insn build/src/<unit>.o
```

and compare against `build/src/.../nonmatchings/<unit>/<func>.s`.

### Native build

```sh
make -f Makefile.native -j       # build native/build/vagrant-native
make -f Makefile.native stubs    # regenerate the abort-stub layer
make -f Makefile.native run      # build and run
```

The stub layer is how the port finds its next task. Any game function not yet
reached becomes a stub that aborts with its own name, so bring-up is
demand-driven.

## Running the native port

```sh
./native/build/vagrant-native
```

It boots the real retail sequence: publisher and developer screens, the intro
FMV, then the title menu. Be patient. The two logo screens run 364 frames each
and the movie is about 38 seconds, so roughly a minute passes before the title
settles.

### Environment variables

| Variable | Effect |
|---|---|
| `VS_HEADLESS=1` | Run with no window. Needed over plain SSH, and for regression runs |
| `VS_WATCHDOG=N` | Bail out after N seconds with a backtrace and a VRAM dump. WSL blocks ptrace-attach, so the binary traps for itself |
| `VS_DATA_DIR=path` | Where the extracted game data lives (default `data`) |
| `VS_DISC=path` | Optional raw disc image, consulted only for sectors outside the extracted data |
| `VS_CD_SPEED=N` | Sectors per frame. The default is an authentic 2x (150/sec); raise it to skip load times |
| `VS_PAD_SCRIPT=…` | Scripted input for headless runs: `<frame>:<mask>` steps, e.g. `1000:0x0800` presses START at vblank 1000 |
| `VS_AUDIO_WAV=path` | Capture the SPU mix to a WAV file |
| `VS_VRAM_RAW=path` | Dump raw VRAM alongside the PPM. Needed to inspect 24-bit regions such as the movie |
| `VS_CD_TRACE=1` | Log CD commands and sector flow |
| `VS_STR_TRACE=1` | Log STR video frame assembly |
| `VS_GPU_TRACE=1` | Log VRAM uploads and every `PutDispEnv` (rect, colour depth, interlace) |
| `VS_CARD_TRACE=1` | Log memory-card commands and directory hits |

### Controller

A DualSense or DualShock over SDL works directly on native Linux, and the
keyboard always works.

On WSL, run the bridge **on the Windows side**, because WSL cannot see the
device at all:

```powershell
python native/tools/pad_bridge.py
```

It reads the pad over raw HID using pure `ctypes`, with no pip installs, and
streams state packets to the game's TCP listener. `VS_PAD_PORT` changes the
port; `VS_NO_PAD_BRIDGE=1` disables it.

### Memory card

The port reads and writes a real PS1 memory card image at
`native/saves/test_card.mcd`. Point it at a copy of your own card to load your
own saves. **Keep a pristine copy**, because save tests mutate it.

## Tools and scripts

Generated sources under `native/platform/` are never committed. They are rebuilt
from your own disc dump, so a fresh clone just works.

| Script | Purpose |
|---|---|
| `native/tools/gen_disc_index.py` | LBA to extracted-file map, so the port needs no disc at runtime (`make -f Makefile.native discindex`) |
| `native/tools/gen_assets.py` | Embeds the per-overlay assets splat extracts separately (`make -f Makefile.native assets`) |
| `native/tools/gen_stubs.py` | Generates the abort-stub layer from unresolved symbols |
| `native/tools/harvest_data.py` | Copies read-only tables out of the retail overlays (`make -f Makefile.native harvest`) |
| `native/tools/pad_bridge.py` | Windows-side controller bridge for WSL |
| `tools/dev/` | Upstream helpers: struct dumping, string tables, tpage/UV maths, objdiff config |

### Tests

```sh
make -f Makefile.native test       # software GTE against golden data, plus a GPU test
make -f Makefile.native mdectest   # decode every frame of the intro movie
make -f Makefile.native menu       # headless 130-frame menu render, dumps a PPM
make -f Makefile.native window     # the same, in a real window
```

`mdectest` is the video regression. It demuxes `TITLE.STR` and checks that every
frame codes exactly the macroblocks its dimensions call for. A desynchronised
bitstream cannot land on the right count by accident, so **593/593** is a sharp
result.

## Status

**Matching build:** ✔ all 21 targets byte-identical.

**Decompilation:** about 72% of functions matched. Menus and the title sequence
are at 100%. `BATTLE.PRG` is the long pole.

**Native port**, what runs today:

- Boots the real retail sequence to the title menu, all game code, unmodified
- Plays the intro FMV using a software MDEC (VLC, dequantise, IDCT, YCbCr)
  decoding all 593 frames of `TITLE.STR`
- Software GTE, verified bit-exact against real COP2 hardware
- Virtual CD drive serving the extracted data by LBA at authentic 2x timing
- Memory card reads and writes a real `.mcd`, and the in-game LOAD screens
  enumerate genuine saves
- Live SDL window and real controller input at 60Hz
- Software SPU (24 ADPCM voices, ADSR, stereo mix), present but **silent**
  pending the Akao instrument-bank upload

**Not yet:** audio output, the 3D scene, and the game world itself. The last of
those is gated on `BATTLE.PRG`, which is the bulk of the remaining
decompilation.

Every native-only shortcut, no-op and stub is tracked in
[`docs/NATIVE_DEVIATIONS.md`](docs/NATIVE_DEVIATIONS.md). Nothing is quietly
approximated.

## Documentation

| Document | Contents |
|---|---|
| [`docs/NATIVE_PORT_PLAN.md`](docs/NATIVE_PORT_PLAN.md) | The working plan, phase by phase, with a "start here" status section and the traps each phase cost |
| [`docs/NATIVE_DEVIATIONS.md`](docs/NATIVE_DEVIATIONS.md) | Every deviation from faithful behaviour, and what would retire it |
| [`docs/PSX_NOTES.md`](docs/PSX_NOTES.md) | Hardware notes, including the GTE differential-validation method |
| [`docs/RAInfo.md`](docs/RAInfo.md) | RetroAchievements code notes: the persistent game-state layout |

## Credits

The decompilation is the work of **[rood-reverse](https://github.com/ser-pounce/rood-reverse)**
and its contributors. This fork would not exist without it.

Upstream's acknowledgements, which apply equally here:

- The [Chrono Cross Decompilation](https://github.com/jdperos/chrono-cross-decomp),
  for much of the sound library code
- The [sotn-decomp](https://github.com/Xeeynamo/sotn-decomp) team and the
  [PS1/PS2 decompilation](https://discord.gg/hKx3FJJgrV) Discord server
- Data Crystal's [Vagrant Story](https://datacrystal.tcrf.net/wiki/Vagrant_Story) page

And the tooling all of this depends on: [splat](https://github.com/ethteck/splat),
[maspsx](https://github.com/mkst/maspsx), [decomp.me](https://github.com/decompme/decomp.me),
[decomp-permuter](https://github.com/simonlindholm/decomp-permuter),
[old-gcc](https://github.com/decompals/old-gcc), [mkpsxiso](https://github.com/Lameguy64/mkpsxiso),
[objdiff](https://github.com/encounter/objdiff) and [decomp.dev](https://decomp.dev/).

Vagrant Story is © 2000 Square. This project is not affiliated with or endorsed
by Square Enix, and distributes none of its copyrighted material.
