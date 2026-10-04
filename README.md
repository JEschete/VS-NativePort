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
| --- | --- | --- |
| Matching PSX build | Visual Studio `Decomp` project | Retail overlays, verified against the original disc when Windows historical compilers are supplied |
| Native x86 build | Visual Studio `NativePort` project | `build/windows/Debug/vagrant-native.exe`, a Win32 Windows binary |

The matching build is the correctness oracle. Every change to `src/` has to keep
it byte-identical, which is what stops the port drifting away from the real game.

## Relationship to rood-reverse

This is a fork of **[ser-pounce/rood-reverse](https://github.com/ser-pounce/rood-reverse)**,
the decompilation project this work stands on. All of the decompiled game code,
the splat configuration, the build system and the toolchain setup come from
there, and upstream remains the source of decompilation progress. This fork
pulls from it regularly.

The unmodified reference is under `resources/rood-reverse`. The maintained
Windows snapshot is under `resources/rood-reverseWin`; ser-pounce's source,
headers, assets and decompilation configuration always take precedence there.
Windows orchestration and generated native adapters are kept separately.
The root owns `native/`, Windows build tooling and the port documentation.
Please direct decompilation credit upstream.

## What you need

**A legally obtained copy of Vagrant Story (NTSC-U, `SLUS-01040`).**

No game code, assets, audio or text are distributed here, and none ever will be.
This repository contains only source code and build tooling. Everything the game
needs is extracted at build time from a disc image you supply yourself, dumped
from your own copy:

```text
disks/SLUS-01040.bin
```

The Windows native build extracts the disc contents into `data/`, which is gitignored.
Both builds read from there. The native port does not need the disc image at
runtime, because it serves data out of the extracted `data/` tree by LBA. Once
you have built once, the `.bin` can go back on the shelf.

## Development environment

The maintained build targets **Windows directly**, with a Visual Studio 2022
Win32 solution and the installed Clang compiler. WSL, Linux, Nix and Docker
are not build dependencies. The upstream reference remains unmodified.

### Requirements

- Visual Studio 2022 Desktop development with C++ and its Clang compiler component
- CMake 3.28 or newer and Python 3.10 or newer
- Your legally obtained NTSC-U disc image
- A 32-bit `SDL2.dll` beside the executable for a window, audio and controller input

Run `tools/Setup-Windows.ps1` from PowerShell. It creates an isolated Python
environment, fetches Windows tools and generates `build/windows/VagrantStory.sln`.
Open the solution and select Debug/Win32.

### SDL2 Runtime

Download the official [SDL2 2.32.10 Windows x86 runtime](https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10-win32-x86.zip)
and extract it into `resources/SDL2-2.32.10-win32-x86/` so that
`resources/SDL2-2.32.10-win32-x86/SDL2.dll` exists. Use x86, not x64 or SDL3.

Build `NativePort` again. Its `SDL2Runtime` dependency automatically copies the
DLL beside the executable for the selected configuration, including incremental
builds that do not relink. The Debug copy is `build/windows/Debug/SDL2.dll`.
Restart an already-running instance after installing it. Both the downloaded
runtime folder and build output are Git-ignored; do not install the DLL in System32.

See [Windows Migration](docs/WINDOWS_MIGRATION.md) for ownership, removed
dependencies, synchronization, verified behavior and outstanding prerequisites.

## Building

### Matching PSX build

Build the solution's `Decomp` project. It uses native Windows GNU Make and
Windows MIPS tools, and compares generated files against retail data.
It requires genuine Windows historical `cc1.exe` builds for 2.7.2-psx,
2.7.2-cdk and 2.8.1-psx. The old-gcc 0.17 release does not supply these.
Missing compilers cause an explicit error; Linux executables and WSL/Docker
fallbacks are rejected. Current Windows byte-matching verification is pending.

### Native build

Build `NativePort` in the generated solution. Native source/data/stub inputs
are generated under ignored `build/` from the authoritative source snapshot
and your retail data; they do not modify matching source files.

The stub layer is how the port finds its next task. Any game function not yet
reached becomes a stub that aborts with its own name, so bring-up is
demand-driven.

## Running the native port

Run this command from the repository root so relative `data/` and
`native/saves/` paths resolve correctly. Visual Studio uses that same working
directory when launching the `NativePort` project.

```powershell
build/windows/Debug/vagrant-native.exe
```

The Windows executable builds and links. Bounded SDL-window runs verify the
loading image and publisher logo with real, nonblack framebuffer pixels.
Full title/menu/movie and game-world execution have not yet been certified on
Windows. Historical Linux bring-up results below are not a claim of Windows
gameplay parity.

An initial Windows migration bug left image-header and palette symbols as
zero-filled stubs, producing a black screen despite successful SDL startup.
Native asset generation now preserves upstream's header/palette/pixel offsets,
and the build rescans object symbols before generating stubs. Rebuild `NativePort`
after updating these files; an older executable still contains the broken assets.
The splash sequence is not instantaneous, so allow time for the logos and movie.

### Environment variables

| Variable | Effect |
| --- | --- |
| `VS_HEADLESS=1` | Run with no window. Needed over plain SSH, and for regression runs |
| `VS_WATCHDOG=N` | Stop after N seconds and capture a Windows minidump |
| `VS_DATA_DIR=path` | Where the extracted game data lives (default `data`) |
| `VS_DISC=path` | Optional raw disc image, consulted only for sectors outside the extracted data |
| `VS_CD_SPEED=N` | Sectors per frame. The default is an authentic 2x (150/sec); raise it to skip load times |
| `VS_PAD_SCRIPT=…` | Scripted input for headless runs: `<frame>:<mask>` steps, e.g. `1000:0x0800` presses START at vblank 1000 |
| `VS_AUDIO_WAV=path` | Also capture the mix to a WAV file. Independent of playback; useful headless, and for checking output exactly |
| `VS_NO_AUDIO=1` | Disable sound output |
| `VS_SND_TRACE=1` | Log every command that reaches the sound driver, and dump each voice's registers at key-on |
| `VS_VRAM_RAW=path` | Dump raw VRAM alongside the PPM. Needed to inspect 24-bit regions such as the movie |
| `VS_CD_TRACE=1` | Log CD commands and sector flow |
| `VS_STR_TRACE=1` | Log STR video frame assembly |
| `VS_GPU_TRACE=1` | Log VRAM uploads and every `PutDispEnv` (rect, colour depth, interlace) |
| `VS_CARD_TRACE=1` | Log memory-card commands and directory hits |

### Controller

Windows SDL reads the keyboard and compatible controllers directly. The WSL
TCP controller bridge has been removed. Controller behavior still requires
interactive Windows verification.

### Memory card

The port reads and writes a real PS1 memory card image at
`native/saves/test_card.mcd`. Point it at a copy of your own card to load your
own saves. **Keep a pristine copy**, because save tests mutate it.
The Windows copy does not automatically import a card image from your WSL checkout.

## Tools and scripts

Generated native sources now live under ignored `build/native-data/` and
`build/native-inputs/`. The source tree contains their generators only.

| Script | Purpose |
| --- | --- |
| `native/tools/prepare_data.py` | Generates retail assets, string includes and the Windows LBA map |
| `native/tools/prepare_native.py` | Applies host-only compatibility to generated native inputs |
| `native/tools/coff_stubs.py` | Generates named aborts and data blocks from Win32 COFF symbols |
| `native/tools/harvest_data.py` | Copies read-only retail tables using the authoritative symbol maps |
| `tools/windows.py` | Windows bootstrap, upstream synchronization and matching-build operations |
| `tools/Probe-Native.ps1` | Hard-bounded startup check; add `-Windowed` to exercise SDL |
| `resources/rood-reverseWin/tools/legacy/` | Preserved root-only struct/image helpers |

### Tests

The solution contains `test_host`, `test_gte`, `test_gpu` and `test_inline_gte`.
Windows verification has passed the host test, 891 GTE checks, 18 GPU checks
and the inline-GTE adapter test. Python build regressions run with:

```powershell
build/python/Scripts/python.exe -m unittest discover -s native/tests -p 'test_*.py'
```

`mdectest` is the video regression. It demuxes `TITLE.STR` and checks that every
frame codes exactly the macroblocks its dimensions call for. A desynchronised
bitstream cannot land on the right count by accident, so **593/593** is a sharp
result.

## Status

**Historical matching build:** all 21 former targets byte-identical on Linux.
**Current Windows matching build:** historical Windows compilers not yet supplied.

**Decompilation:** about 72% of functions matched. Menus and the title sequence
are at 100%. `BATTLE.PRG` is the long pole.

**Historical native bring-up**, prior Linux results retained for reference:

- Boots the real retail sequence to the title menu, all game code, unmodified
- Plays the intro FMV using a software MDEC (VLC, dequantise, IDCT, YCbCr)
  decoding all 593 frames of `TITLE.STR`
- Software GTE, verified bit-exact against real COP2 hardware
- Virtual CD drive serving the extracted data by LBA at authentic 2x timing
- Memory card reads and writes a real `.mcd`, and the in-game LOAD screens
  enumerate genuine saves
- Live SDL window and real controller input at 60Hz
- **Sound**, out of the host's speakers, from two sources mixed as on the
  console: a software SPU (24 ADPCM voices, ADSR, stereo) driven by the game's
  own Akao sound driver, and the CD-XA soundtrack interleaved into the movie

**Not yet:** music (nothing on the boot path asks for any yet), reverb, the 3D
scene, and the game world itself. The last of those is gated on `BATTLE.PRG`,
which is the bulk of the remaining decompilation.

Every native-only shortcut, no-op and stub is tracked in
[`docs/NATIVE_DEVIATIONS.md`](docs/NATIVE_DEVIATIONS.md). Nothing is quietly
approximated.

## Documentation

| Document | Contents |
| --- | --- |
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
