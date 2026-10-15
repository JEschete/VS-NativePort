# Tools Inventory — Vagrant Story Native Source Port

This file lists every external tool, reference repository and documentation source used by the native port. For each one it gives the location, the pinned revision, the license, and whether we may copy its code (**reuse class**).

Reuse classes:

| Class | Meaning |
|---|---|
| **VENDOR** | Permissive license (CC0, MIT, zlib, BSD). Code may be copied into `src\` with attribution in `THIRD_PARTY.md`. |
| **REFERENCE** | GPL, LGPL, non-commercial, no-derivatives, or **no license file**. Read it to understand behavior; do **not** copy code. Write a clean-room reimplementation instead. |
| **TOOL** | Run as an external program on developer machines only. Never linked into or shipped with the port. |

The backbone sources of truth are, in priority order:

1. `resources\rood-reverseWin` — the 97.81%-matching C decompilation (code semantics).
2. `SLUS-01040\` and `disks\SLUS-01040.bin` — the original game data (assets).
3. `resources\DataCrystal\` — file format and RAM documentation.
4. `C:\Tools\RE_Docs\` — prior research notes (`VS_NATIVEPORT_CONTEXT.md`, `Native_Ports\VAGRANT_STORY_TOOLING.md`, `SDL3.md`, `TRACY.md`, `RENDERDOC.md`, `DW4_TOOLING.md`).

---

## 1. Repositories downloaded in this session (`tools\PSX\`)

All are shallow clones. `tools\PSX\` is git-ignored, so re-create them with `tools\Fetch-References.ps1` (planned in M0) using the revisions below.

### 1.1 Native ports (closest precedents)

| Path | Upstream | Revision | License | Class | Why it matters |
|---|---|---|---|---|---|
| `Ports\rood-boi` | https://github.com/noprompt/rood-boi | `dec91e56b6ff` | CC0 | **VENDOR** | Native Linux/Windows port built on the **same** Vagrant Story decomp. It reaches the intro movie, the title screen, New Game and gameplay. It uses a guest-RAM model (which we reject), but its subsystem code can be lifted: `pc\src\cd_drive.c`, `xa_audio.c`, `mdec.c` (libpress / software MDEC), `audio_spu.cpp` and `audio_libspu.c` (software SPU), `gpu_hw.c`, `memory_card.c`, `boot_gamepad.c`, `overlay_statics.c`. Its docs `pc\docs\LP64_Audit.md` and `Platform_Support.md` give our 64-bit hazard counts. |
| `Ports\PsyCross` | https://github.com/SlickAmogus/PsyCross (REDRIVER2 lineage) | `20a2fb4798be` | MIT | **VENDOR** | Psy-Q SDK reimplemented on host: libgte (C GTE), libgpu (primitive → GL), libspu, libcd and libetc, plus PGXP. Best model for a replacement SDK that has the same API shape. |
| `Ports\silent-hill-decomp-pc-port` (branch `pc-port`) | https://github.com/SlickAmogus/silent-hill-decomp | `ee18d6916004` | GPL-3.0 | REFERENCE | Decomp → native port of Silent Hill using PsyCross. Good patterns: `gpu_gte_pc.h` (inline GTE macro → C), `ipd_reformat.c` and `dms_reformat.c` (32→64-bit struct reformatting of on-disc data), `map_registry.c` (overlays as DLL or static). Do not copy. |
| `Ports\psx-vagrant-story` | https://github.com/SomeoneIsWorking/psx-vagrant-story | `93ffb40a4a30` | none | REFERENCE | Hybrid port: decomp plus a Lightrec dynarec for the rest. This is the approach we rejected; useful only for spotting problem functions. |
| `Ports\psxport` | https://github.com/SomeoneIsWorking/psxport | `6709b7df81bd` | none | REFERENCE | The generic framework behind the repo above. |

### 1.2 Vagrant Story format tools

| Path | Upstream | Revision | License | Class | Use |
|---|---|---|---|---|---|
| `VagrantStory\GodHands` | https://github.com/collinsmichael/GodHands | `d4523ec83e7c` | MIT | **VENDOR** | Win32 C editor for disc data: ZND/MPD/ZUD, items, text tables. Its struct layouts are a second check against the decomp. |
| `VagrantStory\vstrack` | https://github.com/tomasz-rozanski/vstrack | `5343bfe17ea2` | MIT | **VENDOR** | Tracker that reads live game state; documents RAM structures for actors, inventory and maps. |
| `VagrantStory\VagrantStoryImporter` | https://github.com/scurest/VagrantStoryImporter | `4fffb39e221c` | none | REFERENCE | SHP/SEQ/WEP/ZUD → glTF. The clearest written spec for model and animation decoding. |
| `VagrantStory\blender-vagrant-story` | https://github.com/korobetski/blender-vagrant-story | `94ade1ff11dd` | none | REFERENCE | Blender importer (MPD, SHP, WEP, SEQ, TIM). |
| `VagrantStory\Vagrant-Story-Unity-Parser` | https://github.com/korobetski/Vagrant-Story-Unity-Parser | `529fd41a32e2` | none | REFERENCE | The most complete format parser set (ARM, MPD, ZND, SHP, WEP, SEQ, AKAO, P.DAT/SE, FBT/FBC fonts, EFFECT). It is the main oracle for checking `VS.Content`. |
| `VagrantStory\VSViewer` | https://github.com/MercurialForge/VSViewer | `c9d2147af8a2` | none | REFERENCE | Model viewer; an extra check on SHP rendering. |
| `VagrantStory\VagrantStory-Font` | https://github.com/HilltopWorks/VagrantStory-Font | `6e4288747696` | none | REFERENCE | Font and character table extraction; used to check the text decoder. |

`C:\Tools\Systems\PSX\vstools` (morris, MIT) — **VENDOR/TOOL**. Python parsers for most VS formats; a good starting point for `tools\vsx\` extractors.

### 1.3 Audio

| Path | Upstream | Revision | License | Class | Use |
|---|---|---|---|---|---|
| `Audio\vgmtrans` | https://github.com/vgmtrans/vgmtrans | `33673176917c` | zlib-like | **VENDOR** (with care) | AKAO sequence and instrument parser and a PSX SPU ADSR model. Used as an independent oracle to check the decomp's sound driver output. |
| `Audio\chrono-cross-decomp` | https://github.com/jdperos/chrono-cross-decomp | `7dcadfc36421` | none | REFERENCE | A Square AKAO driver close in age to Vagrant Story's. Cross-checks names and opcodes in the decomp's sound code. |

`C:\Tools\Base\Audio\vgmstream` — **TOOL**. Decodes XA and VAG for golden audio comparisons.

### 1.4 Hardware references

| Path | Upstream | Revision | License | Class | Use |
|---|---|---|---|---|---|
| `Reference\duckstation-src` | https://github.com/stenzek/duckstation | `66888ba0d448` | CC BY-NC-ND 4.0 | REFERENCE (strict) | Read-only behavior reference. **Never** copy or adapt its code. |

Removed by decision (2026-10-15): `psx-spx` and `beetle-psx-libretro`. The port does not model PS1 hardware, so hardware specifications and emulator cores are out of scope. Format math (XA ADPCM, MDEC/IDCT, fixed-point transforms) comes from the decomp itself and the VENDOR code in rood-boi and PsyCross.

---

## 2. Existing local tooling

### 2.1 `C:\Tools\Systems\PSX`

| Tool | Class | Use in this port |
|---|---|---|
| `jpsxdec` (+ v2.1 release) | TOOL (non-commercial) | Reference decode of `MOV\TITLE.STR` and ending STRs into PNG frames and WAV, to check our MDEC and XA output frame by frame. Its STR format document is MIT. |
| `vstools` | VENDOR/TOOL (MIT) | Format parsers (see §1.2). |
| `maspsx` | TOOL | Decomp build only (matching). |
| `mkpsxiso` / `dumpsxiso` | TOOL | Produced `disks\SLUS-01040.xml` and the extraction in `SLUS-01040\`. |
| `DuckStation` (binary) | TOOL | Golden runs: screenshots, VRAM dumps, GPU dumps, save states for side-by-side comparison. |
| `PCSX-Redux` (source + binary) | TOOL | Lua-scriptable debugger: break on `vs_*` entry points, trace CD reads, dump the SPU state. |
| `MemcardRex` | TOOL | Inspect and convert `.mcd` saves created by the port. |
| `PSn00bSDK` | REFERENCE | Open-source PS1 SDK; documents what each SDK call the game makes is *supposed* to do, so the modern replacement covers the same behavior. Never reimplemented as an API. |
| `ps1-tests` | TOOL/oracle | Expected results for fixed-point transform math; used only to check `VS.Core` math against the original numbers. |

Mirrors in `tools\PSX\Asset-Tools\jpsxdec`, `tools\PSX\Emulators\{DuckStation,PCSX-Redux}` and `tools\PSX\Toolchains\PSn00bSDK`.

### 2.2 `C:\Tools\Base` (`tools\Base\` mirror)

| Tool | Class | Use |
|---|---|---|
| Ghidra (+ MCP) | TOOL | Resolve the 93 `INCLUDE_ASM` functions and 223 asm glabels; check data layouts. |
| objdiff 3.8.2 | TOOL | Keep the decomp byte-matching when we push renames upstream. |
| m2c, asm-differ, decomp-permuter | TOOL | Decompile the remaining asm functions in the decomp repo. |
| Tracy 0.14.1 | VENDOR (BSD-3) | Frame profiler in Debug and Profile builds of `VS.Desktop`. |
| RenderDoc (see `RE_Docs\RENDERDOC.md`) | TOOL | Debug the renderer. |
| Noesis, ImageMagick | TOOL | Image and model inspection, comparing PNG frames. |

### 2.3 Repo-local tools (`tools\`)

| File | Purpose |
|---|---|
| `Setup-Windows.ps1`, `windows.py` | Existing environment bootstrap scripts from the decomp workflow. |
| `Probe-Native.ps1` | Old probe script for the removed emulated-RAM build. Retire it in M0. |
| `.clang-format`, `permuter_settings.toml` | Decomp formatting and permuter configuration. |

---

## 3. Documentation sources

### 3.1 `resources\DataCrystal\` (saved pages; the live site returns HTTP 403, Wayback works)

| Page | Used by |
|---|---|
| Executables / Main exe | Overlay load addresses (TITLE/BATTLE/ENDING at 0x80068800; MAINMENU/INITBTL at 0x800F9800; MENUx at 0x80102800). Used to map the decomp's overlay sections. |
| RAM map, Data Structures | Struct layouts for `VS.Game` headers and the boundary audit. |
| ARM / MPD / SEQ / SHP / WEP / ZND / ZUD file pages | `VS.Content` decoders and tests. |
| Script Opcodes | Event script (`EVENT\*.EVT`) interpreter checks. |
| SLUS LBA List | Order and LBA of disc files. Used to build the CD file index the game queries by LBA. |
| TBL, item and skill lists | Text decoding and data table tests. |

### 3.2 Web references (not downloaded)

| Source | Use |
|---|---|
| ffmpeg `libavcodec/mdec.c`, `adpcm.c` (`adpcm_xa`), `libavformat/psxstr.c` | LGPL, **REFERENCE**. A second implementation of MDEC and XA for golden comparisons. |
| SDL3 (https://github.com/libsdl-org/SDL) | zlib, **VENDOR**. Desktop host (window, gamepad, audio). Fetched as a pinned release in M0. |

---

## 4. Licensing rules (enforced in code review)

1. Only **VENDOR** code may appear under `src\`, and only with an entry in `THIRD_PARTY.md` naming the upstream URL, revision and license.
2. For **REFERENCE** code, cite the file and line in commit messages or comments as "behavior reference". Never paste its code.
3. Game data (`SLUS-01040\`, `disks\`, anything extracted from them) is never committed. The port loads it from a user-provided path at runtime.
4. The decomp (`resources\rood-reverseWin`) keeps its own license. Ported files under `src\Game\` keep a provenance header (see the plan, §6).
