# Vagrant Story: Native Port

A project to finish a Windows decompilation of **Vagrant Story** (PlayStation,
2000), with a native PC port as a later goal.

**Current status:** the decompilation is unfinished. The latest local objdiff
report (2026-10-14) measures **97.81017% matched code overall**
(1,894,184 / 1,936,592 bytes), **98.695786% for BATTLE.PRG**
(510,652 / 517,400), and **98.71365% for the broader BATTLE category**
(517,840 / 524,588), with 4,084 / 4,156 functions byte-matched. The two C
functions temporarily replaced by upstream assembly have now been restored and
freshly verified. Together with `func_8008AC78`, the BATTLE.PRG total is 1,412
bytes above the pre-import accepted baseline. The exact WSL build still
reproduces retail outputs; assembly-backed bytes are not counted as reconstructed
C progress. The native port has not started as an accepted implementation.
Existing native code and executables are legacy experiments, not a playable
port or completed milestones.

The approach is the one Ship of Harkinian popularised: decompile the game to C,
replace the console's hardware layer with a host implementation, and compile the
*same source* for x86. This is the intended architecture, not a claim that the
decompilation or the native hardware replacements are complete.

The repository contains build entry points for two intended targets:

| Target | Command | Output |
| --- | --- | --- |
| Matching PSX build | Visual Studio `Decomp` project | Requires historical Windows compilers and fresh byte-exact verification; not an accepted local result |
| Experimental native scaffold | Visual Studio `NativePort` project | Current build fails game compilation before linking; further native integration remains incomplete |

Fresh byte-exact matching is the acceptance gate for reconstructed game source.
Historical reference matches and host test passes do not satisfy that gate.

For the current native blockers and boot-to-combat acceptance gates, see the
[Native Port Blocker Audit](NATIVE_PORT_BLOCKER_AUDIT.md) (2026-10-15).

## Relationship to rood-reverse

This is a fork of **[ser-pounce/rood-reverse](https://github.com/ser-pounce/rood-reverse)**,
the decompilation project this work stands on. All of the decompiled game code,
the splat configuration, the build system and the toolchain setup come from
there, and upstream remains the source of decompilation progress. This fork
pulls from it regularly.

The unmodified reference is under `resources/rood-reverse`. The maintained
Windows snapshot is under `resources/rood-reverseWin`; ser-pounce's source,
headers, assets and configuration establish its imported baseline. Build
verification never synchronizes or replaces source. Explicit synchronization
refuses local additions, edits, deletions, or populated unmanifested source.
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
are not native-build dependencies. Current decompilation rules allow isolated
WSL historical-compiler verification only; the Linux checkout remains read-only
reference material, not Windows progress.

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

### Experimental Native Scaffold

The legacy `NativePort` target retains experimental source/data inputs. They
are generated under ignored `build/` from the authoritative source snapshot
and your retail data; they do not modify matching source files.

The symbol check now fails on missing game functions and data instead of
manufacturing a successful link. Generic 4 KB data arrays are forbidden.
The generator's explicit `--allow-function-stubs` option is diagnostic only;
it permits named aborts for known functions, never unknown data. Normal builds
do not enable it. Correct symbol resolution still does not prove game behavior.

## Legacy Probe Execution

Old executables are experimental artifacts and may contain unsafe historical
data buffers or stale source. They are not accepted builds and should not be
used to infer progress or test real saves. Native implementation must follow
accepted decompilation; this is not the project's current development stage.

Run this command from the repository root so relative `data/` and
`native/saves/` paths resolve correctly. Visual Studio uses that same working
directory when launching the `NativePort` project.

```powershell
build/windows/Debug/vagrant-native.exe
```

Earlier Windows experiments reported loading-image and publisher-logo pixels.
Those observations certify neither current source nor complete gameplay,
decompilation, or native-port progress. The stricter symbol gate intentionally
blocks the incomplete scaffold; an older executable is not a workaround.

An initial Windows migration bug left image-header and palette symbols as
zero-filled stubs, producing a black screen despite successful SDL startup.
Legacy asset generation preserves upstream's header/palette/pixel offsets.
This infrastructure correction did not implement the missing game or hardware
contracts. A startup watchdog or nonblack screenshot is not gameplay acceptance.

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

The experimental card backend uses `native/saves/test_card.mcd`, but creation,
BIOS rename, formatting, cache coherence and durable writes are incomplete.
Do not use real saves for validation. Future tests must use disposable fixtures
and prove the complete save/load transaction before any support claim.

## Tools and scripts

Generated native sources now live under ignored `build/native-data/` and
`build/native-inputs/`. The source tree contains their generators only.

| Script | Purpose |
| --- | --- |
| `native/tools/prepare_data.py` | Generates retail assets, string includes and the Windows LBA map |
| `native/tools/prepare_native.py` | Applies host-only compatibility to generated native inputs |
| `native/tools/coff_stubs.py` | Rejects unresolved game symbols; explicit diagnostic aborts only, never guessed data |
| `native/tools/harvest_data.py` | Copies read-only retail tables using the authoritative symbol maps |
| `tools/windows.py` | Windows bootstrap, upstream synchronization and matching-build operations |
| `tools/Probe-Native.ps1` | Hard-bounded startup check; add `-Windowed` to exercise SDL |
| `resources/rood-reverseWin/tools/legacy/` | Preserved root-only struct/image helpers |

### Tests

The solution contains `test_host`, `test_gte`, `test_gpu` and `test_inline_gte`.
These are infrastructure experiments, not decompilation or native-port
acceptance. Earlier reports of host/GTE/GPU passes are separate from fresh
byte-exact reports. Python build regressions run with:

```powershell
build/python/Scripts/python.exe -m unittest discover -s native/tests -p 'test_*.py'
```

The legacy MDEC harness checks VLC macroblock counts, not pixel equivalence,
full-frame coverage enforcement, or working Windows movie playback. Its
historical 593-frame report is reference research, not a completed feature.

## Status

**Linux decompilation:** reference only; inherited percentages and 21-target
matching reports are not results of this Windows project.

**Windows decompilation:** two accepted function-level matches beyond the
imported reference (func_800A2790 and func_800A25EC in BATTLE `38C1C.c`); no
source-unit or overlay match. The removed candidate and its 63-group report are not current
validation. See [Local Progress](resources/rood-reverseWin/DECOMP_PROGRESS.md).

**Native port:** not started as an accepted implementation. Legacy experimental
code remains for research; missing game code, data contracts, card operations,
and hardware behavior must not be hidden by a successful link.

**Historical Linux prototype reports**, retained only as reference:

- Boots the real retail sequence to the title menu, all game code, unmodified
- Plays the intro FMV using a software MDEC (VLC, dequantise, IDCT, YCbCr)
  decoding all 593 frames of `TITLE.STR`
- Selected GTE comparisons against captured emulator outputs, not proof of every operation or physical hardware fidelity
- Virtual CD drive serving the extracted data by LBA at authentic 2x timing
- Memory card reads and writes a real `.mcd`, and the in-game LOAD screens
  enumerate genuine saves
- Live SDL window and real controller input at 60Hz
- **Sound**, out of the host's speakers, from two sources mixed as on the
  console: a software SPU (24 ADPCM voices, ADSR, stereo) driven by the game's
  own Akao sound driver, and the CD-XA soundtrack interleaved into the movie

These reports do not establish any current Windows feature. The prerequisite
and risk list is in [Native Deviations](docs/NATIVE_DEVIATIONS.md); historical
"DONE" labels there describe experiments, not current acceptance.

## Documentation

| Document | Contents |
| --- | --- |
| [`docs/NATIVE_PORT_PLAN.md`](docs/NATIVE_PORT_PLAN.md) | Preserved design/backlog and historical prototype notes; not current progress |
| [`docs/NATIVE_DEVIATIONS.md`](docs/NATIVE_DEVIATIONS.md) | Current native prerequisites plus the historical experiment ledger |
| [`docs/PSX_NOTES.md`](docs/PSX_NOTES.md) | Hardware notes, including the GTE differential-validation method |
| [`docs/RAInfo.md`](docs/RAInfo.md) | RetroAchievements code notes: the persistent game-state layout |
| [`resources/RE_Docs/README.md`](resources/RE_Docs/README.md) | Source-linked reverse-engineering tools, REA host support, and cross-system disassembly/decompilation/porting research |

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
