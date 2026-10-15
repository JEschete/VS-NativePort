# Vagrant Story — Native Source Port Plan

Status: **Plan v1**. Supersedes the deleted emulated-RAM/CMake attempt.
Target: Windows desktop, built from a single top-level Visual Studio solution.

Companion documents:

- [TOOLS_INVENTORY.md](TOOLS_INVENTORY.md) — every tool and reference, with revision, license and reuse class.
- `resources\rood-reverseWin\AGENTS.md`, `DECOMP_PROGRESS.md` — rules and status of the decomp.

---

## 1. What we are building

A **native source port**: the game logic from the decomp (`resources\rood-reverseWin`) compiled as ordinary C for Windows. It is linked against a **host reimplementation of the parts of the PlayStation SDK the game actually calls** (libgpu, libgte, libspu/libsnd, libcd/libds, libpress, libpad, libcard, libetc, libapi), and driven by an SDL3 desktop host.

The model is the DW4 port (`D:\Dev\NativePort\DW4_NativePort2`): clear project boundaries, assets loaded from the user's own disc, and no emulation layer. The difference is that DW4 rewrote the logic in C++. Here, 97.81% of the logic already exists as matching C, so we keep that C and replace what sits beneath it.

### 1.1 Goals

1. Boot straight into `vs_main_exec` → title → game with no BIOS, no MIPS code, no emulated CPU.
2. **No guest address space.** No 2 MB array at `0x80000000`, no scratchpad at `0x1F800000`, no address aliasing of symbols. Every global is a real C object that the linker places.
3. All original assets are read at runtime from the user's disc image (`.bin/.cue`) or an extracted folder (`SLUS-01040\`). Nothing copyrighted is committed.
4. Behavior parity with the PS1 version at native resolution first. Enhancements (resolution scale, widescreen, frame interpolation) come only after parity.
5. A single `VagrantStory.sln` at the repo root that builds, runs and tests everything with F5.

### 1.2 Non-goals (for v1)

- Running the original `SLUS_010.40` or `*.PRG` machine code, whether by recompiler, dynarec or interpreter.
- Bit-exact GPU rasterization. The goal is visual parity, checked by image tolerance tests.
- Platforms other than Windows (the layout must not prevent them; see §15).
- Modifying `resources\rood-reverseWin` in place (see §6).

### 1.3 Lessons that shaped this plan

| Source | Lesson |
|---|---|
| Our previous attempt (deleted) | Compiling the decomp against fake PS1 RAM produced 1,459 unresolved symbols plus endless address-aliasing work. The emulated memory model was the real cost, not the C code. |
| Naming audit | About 50% of functions are still `func_XXXXXXXX` (BATTLE 78%, SLUS 31%, MENU 7%, TITLE 0%). There are 1,185 `D_` globals, 639 `unk` fields, about 11.7k `argN` uses and 93 `INCLUDE_ASM` blocks. Names are not needed to compile, but they are needed to port the hardware-facing parts. Rename work is prioritized by the milestone that touches the code (§13). |
| rood-boi (CC0, same decomp) | Proves the decomp can reach title, New Game and gameplay natively. It does this with 32-bit guest RAM and 941 by-address data imports, which is exactly what we are removing. **Its device models (CD, XA, MDEC, SPU, memory card, gamepad, overlay static reset) are CC0 and can be lifted.** Its `LP64_Audit.md` counts our 64-bit hazards: 347 pointer→32-bit casts, 874 int→pointer casts, 320 packed-OT/24-bit masks, 668 scratchpad literals, 79 KSEG0 literals. |
| Silent Hill PC port (GPL, reference only) | The same architecture on another decomp works: decomp C + PsyCross HAL + host shell. Two techniques worth copying as ideas: replacing inline GTE macros with C, and reformatting 32-bit on-disc structs for 64-bit hosts. |
| psx-vagrant-story / psxport | Dynarec hybrids. Rejected: they keep the PS1 machine as the runtime. |
| PsyCross (MIT) | A clean model of a host-side Psy-Q API (C GTE, GPU primitives → GL, SPU, CD). It can be vendored. |

---

## 2. Key decisions (with rationale)

| # | Decision | Choice | Why |
|---|---|---|---|
| D1 | Language of game code | **C17**, kept as close to the decomp as possible | Diffs stay reviewable against upstream; fixes can flow both ways. |
| D2 | Compiler | **clang-cl (ClangCL toolset)** for C projects; MSVC `cl` allowed for C++ host projects | The decomp uses GCC extensions (`__attribute__`, statement expressions, `asm` labels, zero-size arrays) that MSVC `cl` rejects. clang-cl accepts them inside a normal `.vcxproj`. |
| D3 | Architecture | **x64 only, from day one** (decided by the project owner). No Win32 configuration. | Avoids a later migration and a second ABI to maintain. Cost: the LP64 work (347 ptr→32-bit casts, 874 int→ptr casts, 320 OT/24-bit sites, every struct with an embedded pointer) moves onto the critical path. It is fixed **per milestone, only for the code that milestone executes**, and kept honest by layout asserts (§5.3), the audit (§9) and the pointer-slot rules below (§4.1, §8.2). Windows x64 is LLP64 (`long` = 4 bytes), so only pointer width changes, not `long`. |
| D4 | Bitfield layout | Force GCC/System V bitfield packing, enforced by `static_assert` on every on-disc struct | MSVC-ABI bitfield packing changes struct sizes (rood-boi needs `-mno-ms-bitfields` on MinGW). M0 spike: confirm that clang-cl honors `/clang:-mno-ms-bitfields` or `#pragma ms_struct off`. If neither works, rewrite the affected bitfields as accessor macros. |
| D5 | Overlays | **Statically link all overlays** into one executable. "Loading an overlay" becomes: reset its statics → call its entry point | No code is loaded at runtime, so there is no relocation and no DLL plumbing. Name collisions (e.g. `func_800F9800` in every EFFECT plugin) are solved by per-overlay symbol prefixing (§7.3). |
| D6 | Initialized data | **Generated at build time** from the user's original binaries plus the decomp's splat symbol layout, emitted as typed C with symbolic pointers | Game data tables are not committed (licensing). Pointers become real C relocations, not addresses. |
| D7 | Hardware boundary | Game code may only reach the hardware through the public SDK headers (`libgpu.h` …). Any literal hardware/KSEG address in game code is a **build-breaking** lint error | This is the "native boundary" rule DW4 enforces with `check-native-boundary.ps1`. |
| D8 | Renderer | Phase 1: a software "VRAM-accurate" GPU (1024×512×16 bpp VRAM, CPU rasterizer) presented via SDL3. Phase 2: a hardware renderer (D3D11/D3D12 or SDL_GPU) with upscaling | Phase 1 gives correctness quickly; many VS effects read back VRAM (`StoreImage`/`MoveImage`, e.g. the title menu uses `StoreImage`), so VRAM must exist as memory from day one. |
| D9 | Audio | Software SPU (24 voices, ADPCM, ADSR, reverb) driven by the **decomp's own AKAO sound driver**, plus the XA decoder | The game's music sequencer is already in the decomp; only the SPU hardware below it needs reimplementing. |
| D10 | Reuse | Vendor CC0/MIT code (rood-boi devices, PsyCross GTE, SDL3, Tracy) under `third_party\` with `THIRD_PARTY.md`. GPL/NC/unlicensed is reference only | See TOOLS_INVENTORY §4. |

---

## 3. Top-level Visual Studio solution

### 3.1 Repository layout (target)

```
VS-NativePort\
├─ VagrantStory.sln                 ← top-level solution (F5 runs VagrantStory.Desktop)
├─ Directory.Build.props            ← global: output dirs, warning policy, LangStandard
├─ Directory.Build.targets          ← global: boundary check, data generation hooks
├─ build\
│  └─ props\
│     ├─ VagrantStory.Common.props  ← Configs/Platforms, defines, /utf-8, /permissive-
│     ├─ VagrantStory.GameC.props   ← clang-cl flags for decomp C (§5.2)
│     └─ VagrantStory.Cpp.props     ← C++23 host code, /W4 /WX
├─ src\
│  ├─ Game\                         ← VagrantStory.Game (static lib, C) — ported decomp
│  │  ├─ SLUS\                      ← main.c, 25AC.c, overlay.c, zndFiles.c, …
│  │  ├─ TITLE\  BATTLE\  INITBTL\  MAINMENU\  MENU\  ENDING\  GIM\  EFFECT\
│  │  ├─ include\                   ← decomp headers (ported)
│  │  └─ PORT_MANIFEST.json         ← provenance of every imported file (§6)
│  ├─ Platform\                     ← VagrantStory.Platform (static lib, C/C++)
│  │  ├─ include\psx\               ← libgpu.h libgte.h libspu.h libsnd.h libcd.h libds.h
│  │  │                                libpress.h libpad.h libcard.h libetc.h libapi.h
│  │  ├─ gpu\  gte\  spu\  cd\  mdec\  pad\  card\  sys\
│  │  └─ host_api.h                 ← the only interface the Desktop host implements
│  ├─ Content\                      ← VagrantStory.Content (static lib, C++23)
│  │  ├─ disc\                      ← BIN/CUE + ISO9660 reader, extracted-folder reader
│  │  ├─ cdindex\                   ← LBA ↔ file table, sector-mode metadata (XA/Form2)
│  │  └─ formats\                   ← TIM, STR, XA, SEQ/SHP/WEP… (tools and tests only)
│  ├─ Desktop\                      ← VagrantStory.Desktop (exe, C++23, SDL3)
│  │  ├─ main.cpp  video.cpp  audio.cpp  input.cpp  config.cpp  paths.cpp
│  │  └─ resources\  (icon, manifest)
│  └─ GameData\                     ← VagrantStory.GameData (static lib, generated C)
│     └─ (empty in git; filled by build\gen during the build, see §8)
├─ tests\
│  ├─ VagrantStory.Tests\           ← unit/integration tests (GoogleTest or doctest)
│  └─ golden\                       ← hashes and tolerances only, never game data
├─ tools\
│  ├─ vsport\                       ← Python package: import, gen-data, audit, golden
│  ├─ VagrantStory.Tools.vcxproj    ← "Utility" project exposing tool commands in the IDE
│  ├─ Check-NativeBoundary.ps1
│  └─ Fetch-References.ps1          ← re-clones tools\PSX\* at the pinned revisions
├─ third_party\                     ← vendored code: SDL3, rood-boi devices, PsyCross GTE, Tracy
├─ Docs\
├─ resources\  disks\  SLUS-01040\  ← unchanged inputs (git-ignored where they hold game data)
```

### 3.2 Projects and dependency graph

```
VagrantStory.Desktop (exe)
 ├─ VagrantStory.Game      (lib)  ──► VagrantStory.Platform (headers only + link)
 ├─ VagrantStory.GameData  (lib)  ──► generated by VagrantStory.Tools (build dependency)
 ├─ VagrantStory.Platform  (lib)  ──► VagrantStory.Content
 └─ VagrantStory.Content   (lib)
VagrantStory.Tests (exe)  ──► Platform, Content, Game (headless host)
VagrantStory.Tools (utility) — runs tools\vsport\ (import / gen-data / audit)
```

Rules:

- `Game` includes only `src\Game\include` and `src\Platform\include\psx`. It never sees SDL, Windows headers or C++.
- `Platform` never includes game headers. It implements the SDK against `host_api.h`.
- `Desktop` is the only project that includes SDL3 or `<windows.h>`.
- `Tests` uses a headless `host_api` implementation (no window, deterministic clock).

### 3.3 Configurations

| Configuration | Purpose |
|---|---|
| `Debug` | `/Od`, asserts, Tracy off, AddressSanitizer optional (`/fsanitize=address` is supported by clang-cl). |
| `Release` | `/O2`, LTCG off for the C lib at first (overlay statics need to stay debuggable). |
| `Profile` | Release + Tracy client + frame markers in `VSync`. |

Platform: `x64` only.

### 3.4 Toolchain baseline

- Visual Studio 2026 (18.x) or 2022 (17.14), both with the "C++ Clang tools for Windows" component (already installed).
- Windows SDK 10.0.22621+.
- Python 3.11+ for `tools\vsport` (the decomp's `requirements.txt` already provides splat/spimdisasm).
- SDL3 pinned release under `third_party\SDL3` (zlib), x64. The SDL2 x86 copy in `resources\` is not used.

---

## 4. The runtime model (how the PS1 assumptions go away)

| PS1 concept | Native replacement |
|---|---|
| BIOS / kernel (`libapi`: events, root counters, `EnterCriticalSection`, `_sysInit` boot) | `Platform\sys`: the events/callbacks needed by libcd/libds/VSync are plain function-pointer tables. Critical sections are no-ops (single-threaded game loop). |
| `main()` / crt0 / `__main` | `Desktop\main.cpp` → `host_init()` → `vs_main_exec()`. `__main` becomes a no-op. |
| 2 MB main RAM | Gone. Globals are C objects. The game's own heap (`_sysReinit`: base `0x8010C000`, size `0xF2000`) becomes a static buffer passed to the decomp's allocator, **sized 2× the original** (`0x1E4000`) because structs that hold pointers are larger on x64. A debug counter records the peak use per scene, so the factor can be tuned with data. |
| Overlay slots (`vs_overlay_slots`: `0x80068800`, `0x800F9800`, `0x80102800`) | Gone as load addresses. The CD reads of `*.PRG` are intercepted (§7). Where the game uses a slot as a **data buffer** (e.g. loading a non-code file into slot memory), the slot becomes a static byte array of the original slot size. |
| `jumpToTitle` / `jumpToBattle` (asm stack resets) | `setjmp`/`longjmp` back to a dispatcher loop in `overlay.c`, or a restructured `while` loop in `vs_main_exec`. Both are acceptable; prefer the loop. |
| Scratchpad `0x1F800000` (1 KB) | `extern uint8_t vs_scratchpad[1024];` plus typed views (`#define D_1F800000 (*(SomeStruct*)&vs_scratchpad[0])`), generated from the audit (§9.2). |
| Hardware registers `0x1F801xxx` | Each access site is rewritten to a Platform call (`psx_spu_write`, `psx_gpu_status`…). These are concentrated in BATTLE (about 1,048 literal hits across the tree). |
| KSEG0/KSEG1 address arithmetic (`| 0x80000000`, `& 0x1FFFFFFF`) | Removed per site. Usually these mark uncached access or cache flushes, which are meaningless natively. |
| Ordering tables (24-bit next pointer + 8-bit length packed in one `u_long`) | `P_TAG`/OT macros are redefined to a native tag `{ void* next; u32 len_code; }` (pointers do not fit in 24 bits on x64). Every hand-rolled `& 0xFFFFFF` / `<< 24` site is rewritten to `addPrim`/`termPrim`/`setlen`-style helpers (320 sites per rood-boi's count). Primitive structs (`POLY_FT4`, …) grow by the tag change, so any game code that computes primitive sizes with literals instead of `sizeof` is fixed at the same time. |
| GTE (COP2 inline asm, `gte_*` macros, `libgte\*.s`) | C implementation of each GTE operation in `Platform\gte` (vendored from PsyCross, checked against ps1-tests). Inline macros in decomp headers are swapped for C macros by `inline_gte_c.h`. |
| VSync / frame pacing | `VSync(0)` = submit the frame to the host, then wait for the next 59.94 Hz tick. `VSync(-1)` returns a monotonic field counter. The title loop depends on this (timeouts of 1000 and 100 ticks). |
| CD-ROM (libcd, libds queues, LBAs) | Content provides an LBA-addressed **virtual disc**, backed by the `.bin` (exact) or by the extracted folder plus an index (§10.4). Reads complete either asynchronously at configurable speed (default 2× = 300 KB/s timing, for logic that polls) or instantly. |

### 4.1 Pointer slots on x64 (the core LP64 rule)

On the PS1, a pointer is 4 bytes. On x64 it is 8. Three kinds of memory hold pointers, and each gets one rule:

| Where the pointer lives | Rule |
|---|---|
| **Structs defined and filled only by code** (actors, menus, queues) | Native pointers. Struct sizes grow, which is fine because nothing outside the program reads these bytes. Any `sizeof`-free literal size or offset arithmetic over them is fixed when found. The audit flags `+ 0x`, `[N]` byte offsets on typed pointers. |
| **Structs read from disc** that contain 32-bit pointer or offset fields (files whose loader patches offsets into pointers in place, e.g. ZND/MPD/SHP/SEQ/WEP/EFFECT headers) | The on-disc struct keeps a `u32` field (`vs_ptr32`). The in-place "offset → pointer" patching is replaced by one of two methods. (a) **Base + offset accessors**: `VS_DPTR(base, field)` returns `(T*)((u8*)base + field)`; this is preferred, needs no copy, and keeps the file bytes unchanged. (b) **Reformat on load** into a native-layout struct (the Silent Hill `ipd_reformat.c` idea), only where (a) touches too many sites. Each format records which method it uses in `Docs\formats\`. |
| **Generated data tables** (function-pointer tables, string tables, linked records in `.data`) | The generator emits **typed** C with real pointer members (§8.2). A relocated word may never be emitted into a `u32`/byte array. |

Integer ↔ pointer casts are allowed only through `vs_ptr_to_u32()` / `vs_u32_to_ptr()` helpers that **assert** the value round-trips (for handles and offsets only). The raw C casts are compile errors in Game (`-Werror=pointer-to-int-cast -Werror=int-to-pointer-cast -Werror=int-conversion` on files marked `ported`; a warning budget elsewhere).

---

## 5. Building the decomp C natively

### 5.1 Compile surface

The decomp's PS1 build selects files per unit from splat YAML. The port uses the same unit lists, but:

- `INCLUDE_ASM(...)` → a host stub `PORT_UNIMPLEMENTED("unit/func")` that logs and asserts the first time it is called. The 93 remaining blocks are prioritized by milestone (§13).
- `INCLUDE_RODATA(...)` → resolved by the data generator (§8).
- SDK `lib*` folders under `src\SLUS_010.40\` are **not** compiled. Platform replaces them. The SDK `.s` files (libgte 16, libapi 2, libcard 2, libsn 2) disappear with them. The remaining game-side asm (93 `INCLUDE_ASM` blocks + 223 glabels across units) is mapped either to a Platform equivalent (GTE, memcpy-like helpers) or to a C rewrite (§14).

### 5.2 Compiler flags (`VagrantStory.GameC.props`)

```
clang-cl  /std:c17  (x64)  /W3  /utf-8
  /clang:-fno-strict-aliasing      ← decomp type-puns everywhere
  /clang:-fwrapv                   ← PS1 arithmetic relies on wraparound
  (no -funsigned-char)             ← MIPS GCC char is signed, same as clang default
  /clang:-fno-builtin-memcpy etc.  ← decomp defines its own libc subset
  -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast (files in state "ported"; see §4.1)
  /D VS_NATIVE=1 /D VS_PORT_X64=1
  /FI src\Game\include\port_prelude.h   ← maps GCC-only constructs, GTE macros, asm()
```

`port_prelude.h` handles:

- `__attribute__((section(...)))` and `asm("label")` names → empty.
- `INCLUDE_ASM` / `INCLUDE_RODATA` → port stubs.
- Integer type width checks (`static_assert(sizeof(long) == 4)`; on x64 Windows `long` is still 4 bytes, which conveniently matches the PS1).
- `D_8xxxxxxx` data symbols declared but undefined → link errors are **expected** until the generator provides them; the build lists them as a work queue.

### 5.3 Struct layout guards

Every struct that is (a) read from disc, (b) shared with Platform, or (c) annotated in DataCrystal gets a `PORT_ASSERT_SIZE(Type, size)` and `PORT_ASSERT_OFFSET(Type, field, off)` in `src\Game\include\layout_asserts.h`. They are generated initially from the decomp's headers and the DWARF-free size data in the splat symbol files. Failing asserts are the main way we detect bitfield or packing drift (D4). On x64, structs that hold native pointers are excluded from size asserts on purpose (§4.1). Asserts apply to **on-disc** layouts, which must keep their PS1 size exactly.

---

## 6. Source import strategy (owned fork with provenance)

The decomp stays **canonical and byte-matching** (`AGENTS.md`). The port owns a **copy** in `src\Game\`, managed by a tool rather than by hand.

### 6.1 `tools\vsport import`

1. Reads `resources\rood-reverseWin\upstream.json` and the splat YAMLs to learn the file set.
2. Copies each needed `.c`/`.h` into `src\Game\…`, rewriting only `#include` paths.
3. Records in `src\Game\PORT_MANIFEST.json`, for each file: source path, source SHA-256, decomp commit (`062096da30508e96fcb343cd7a4ce255632e1b9a` today), and state: `pristine | ported`.
4. Refuses to overwrite a `ported` file. Instead it writes `<file>.upstream` and a 3-way merge report.

### 6.2 Where changes go

| Kind of change | Goes to |
|---|---|
| Renames, types, struct fields, comments (matching-neutral) | **Upstream first** (decomp repo, verified with objdiff), then re-imported. This is how `func_`/`D_`/`unk` names improve without forking. |
| New decompilation of `INCLUDE_ASM` functions | Upstream first (m2c + permuter + objdiff), then import. |
| Hardware/PS1-ism removal (scratchpad, registers, OT masks, inline GTE) | Port fork only, wrapped in `#ifdef VS_NATIVE` **only when small**. Larger rewrites replace the function in the fork and set the file to `ported`. |
| Bug fixes found while porting | Both, if the bug exists in the original game logic description. |

### 6.3 Mechanical transforms (applied by the importer, idempotent)

- Hardware literal → `PORT_HW(addr)` macro (fails to compile until handled).
- Scratchpad literal → `vs_scratchpad` view.
- `(u_long)ptr & 0xFFFFFF` OT idioms → `PORT_OT_NEXT()` helper (compile error until reviewed).
- Overlay-collided symbol names → prefixed (§7.3).

Each transform is a Python pass with unit tests. Its output is reviewed in normal diffs.

---

## 7. Overlays

### 7.1 Inventory

| Overlay | Original slot | Contents |
|---|---|---|
| `TITLE.PRG` (`TITLE\TITLE.PRG\22C.c`, `titleData.c`, `libpress\`) | 0x80068800 | Intro movie, title menu, memory-card load, `vs_title_exec` (fully named). |
| `BATTLE.PRG` | 0x80068800 | Field/battle engine (78% generic names; most hardware literals). |
| `ENDING.PRG` | 0x80068800 | Ending sequence. |
| `INITBTL.PRG`, `MAINMENU.PRG` | 0x800F9800 | Battle init, main menu. |
| `MENU0`–`MENUF` | 0x80102800 | Sub-menus. |
| `GIM`, `SCREFF2` | various | Screen effects. |
| `EFFECT\PLG*.BIN` (72) | effect slot | Per-effect plugins with identical entry symbols. |

### 7.2 Loading model

`overlay.c` and `_loadTitlePrg`/`_loadBattlePrg` queue CD reads of the PRG into a slot and then jump into it. Natively:

```c
// src\Game\SLUS\overlay_native.c
typedef struct { const char* file; void (*reset)(void); void* entry; } vs_OverlayDesc;
extern const vs_OverlayDesc vs_overlays[];   // generated table, one row per PRG/BIN
void vs_overlay_activate(int id);            // reset statics, mark active, return entry
```

- The CD request for a PRG file (identified by LBA through the CD index) is **satisfied without copying code**. The request completes, and `vs_overlay_activate` runs.
- Each overlay's `reset()` re-initializes its `.data`/`.bss` from the generated initial image. This reproduces "fresh load" semantics, which the game relies on (statics such as `introMovieLastPlayed` above). rood-boi's `overlay_statics.c` is the model.
- Code that calls a slot address as a function pointer (e.g. `((void(*)(void))0x80068800)()`) is rewritten to call `vs_overlays[id].entry`.

### 7.3 Symbol collisions

All overlays link into one image, so identical names collide (every EFFECT plugin has `func_800F9800`). The importer applies a deterministic prefix to every non-`static` symbol defined in an overlay that also exists in another overlay: `PLG012_func_800F9800`. Cross-overlay references never happen in the original (overlays only call SLUS), so prefixing is purely local. Making such symbols `static` where possible is preferred.

---

## 8. Data generation (no addresses, no committed game data)

### 8.1 Problem

The decomp's C references about 1,185 `D_` globals plus named data. Many are defined only in splat-generated `.s` data files (`dlabel` + `.word`/`.byte`). These files include pointers written as symbols (`.word func_8001234C`, `.word D_800F0010`). The PS1 build assembles them. We have no assembler step and must not embed fixed addresses.

### 8.2 Solution: `tools\vsport gen-data`

Input:

- The user's original binaries: `SLUS-01040\SLUS_010.40` and `*.PRG`/`*.BIN` (exact bytes).
- Splat configs and symbol files from the decomp (segment boundaries, symbol names/addresses/sizes, and which words are relocations: spimdisasm's `.word sym` output in `build\splat-artifacts` for SLUS and BATTLE, regenerated by the tool for the other units).
- Optional type hints: a `datatypes.yaml` mapping symbols to C types (start with the decomp headers' `extern` declarations).

Output (to `$(IntDir)gen\`, compiled into `VagrantStory.GameData`, **never committed**):

```c
/* gen_SLUS_data.c — generated from SLUS_010.40 sha256=… */
#include "gen_prelude.h"
const unsigned char D_8004A2C0[0x40] = { 0x12, 0x34, … };          // raw bytes
void* D_8004A300[3] = { &func_80012340, &D_8004A2C0[0x10], 0 };      // relocated
vs_ItemDef vs_main_itemDefs[512] = { … };                            // typed, when hinted
```

Rules:

- Every word that splat marks as a symbol reference is emitted as `&symbol + offset`. Every other word is emitted as raw data.
- Untyped symbols **without relocations** are emitted as `unsigned char[]` with `__declspec(align(4))`. Symbols **containing relocations must be typed** (x64 pointers are 8 bytes, so a 4-byte slot cannot hold one). The generator refuses to emit them untyped and lists them in the report with a suggested struct taken from the word pattern (`ptr, u32, ptr, …`). The game's `extern` type declaration stays authoritative, and a `PORT_ALIAS` shim (an `#define` to a typed lvalue) is generated for the 1–2% of cases where the C declaration needs a different type.
- **Unknown or ambiguous words** (could be an address or an integer) are listed in `gen-data-report.md`. By default they are emitted as integers; overrides go into `datarelocs.yaml` (committed, since it has no game bytes).
- Overlay `.data`/`.bss` get both a live object and a `const` initial image for `reset()`.
- The generator validates that the input binary SHA-256 matches the known SLUS-01040 hashes. Other versions are rejected for now.

Build integration: `Directory.Build.targets` runs `python -m vsport gen-data` as a custom build step of `VagrantStory.GameData` with proper Inputs/Outputs, so it runs incrementally. The game data path comes from `VS_GAME_DATA` (environment variable or a `.user` props file), never hard-coded.

### 8.3 Runtime assets

Everything else (maps, models, music, movies, textures) is read at runtime by the game's own CD code through the virtual disc. **No offline asset conversion is needed for parity.** `VS.Content\formats` exists only for tools, tests and future enhancements (texture replacement).

---

## 9. Removing PS1-isms

### 9.1 Audit tool (`tools\vsport audit`, runs in CI)

Scans `src\Game` and produces counts per unit and category. Categories:

| Category | Pattern examples | Starting count (decomp) |
|---|---|---|
| HW register | `0x1F801xxx`, `0x1F802xxx` | about 1,048 hits including scratchpad (mostly BATTLE) |
| Scratchpad | `0x1F800xxx`, `getScratchAddr` | 668 (rood-boi count) |
| KSEG literal | `0x80xxxxxx` used as an address | 79 |
| OT/24-bit | `& 0xFFFFFF`, `<< 24` on tags, `addPrim` hand-rolled | 320 |
| Ptr→int | `(u_long)ptr`, `(int)ptr` | 347 |
| Int→ptr | `(T*)intExpr` | 874 |
| Inline GTE | `gte_*`, `__asm__` | per header |
| `INCLUDE_ASM` | | 93 blocks |
| Generic names | `func_`, `D_`, `unk`, `argN` | 1,662 / 1,185 / 639 / ~11.7k |

The audit output feeds the milestone checklists (§13). The first five categories must reach **zero in units included in a milestone** before that milestone closes.

### 9.2 `Check-NativeBoundary.ps1` (pre-build, fails the build)

- No hardware-address literal in `src\Game` outside `PORT_HW()` sites that are listed in an allowlist with a reason.
- No `#include` of anything except `src\Game\include` and `psx\` headers from Game.
- No SDL, Win32 or C++ header in Game or Platform/include.
- `vs_scratchpad` is the only scratchpad access path.

---

## 10. Platform subsystem designs

### 10.1 GPU (`Platform\gpu`) — libgpu API, VRAM-first

- State: 1024×512 16-bit VRAM, draw environment, display environment, texture page/CLUT cache.
- API surface: exactly the libgpu functions the decomp calls (`ResetGraph`, `SetDefDrawEnv`, `PutDrawEnv`, `PutDispEnv`, `DrawOTag`, `ClearOTagR`, `LoadImage`, `StoreImage`, `MoveImage`, `DrawSync`, `SetDispMask`, `LoadTPage`, `GetTPage`, `GetClut`, `SetVideoMode`…). The list is generated from undefined references after `lib*` removal.
- Phase 1 rasterizer: a CPU implementation of the GP0 primitives the game uses (flat/Gouraud/textured triangles and quads, sprites, tiles, lines). Semi-transparency modes, dithering, mask bit and texture windows behave as in psx-spx. Seed from rood-boi `gpu_hw.c` (CC0) where it fits. The display area is converted to RGBA and handed to the host every `VSync(0)`.
- Phase 2 (M5): a hardware backend with VRAM-as-texture, upscaling and optional PGXP-like subpixel precision (PsyCross has the reference design). It is enabled only when golden tests pass with tolerance.
- Validation: GPU primitive unit tests against DuckStation VRAM dumps captured with PCSX-Redux/DuckStation from the same scene (§12).

### 10.2 GTE (`Platform\gte`)

- C GTE with the register file as a per-process struct. Every operation (RTPS/RTPT, NCLIP, AVSZ3/4, MVMVA, NCDS/NCCS/NCT, OP, SQR, DPCS, INTPL, GPF/GPL, color/depth cue) is implemented with saturation and flag behavior from psx-spx.
- libgte C API (`RotTransPers`, `SetRotMatrix`, `RotMatrix*`, `ratan2`, `rsin`/`rcos`, `SquareRoot0`…) on top. The `libgte\*.s` routines in the decomp are replaced by these.
- Seed: PsyCross `gte` (MIT). Tested with ps1-tests GTE expected values plus randomized differential tests against Beetle's behavior (as reference data captured from runs, not code).

### 10.3 SPU / sound (`Platform\spu`)

- Software SPU: 24 voices, ADPCM decode, ADSR, pitch, noise, pitch modulation, reverb (PS1 reverb algorithm), SPU RAM 512 KB, DMA transfers via `SpuWrite`, IRQ unsupported unless used.
- libspu/libsnd API that the decomp's **AKAO driver** calls. The decomp's sound driver code (sequence playback, `vs_main_*Sound*`) runs unchanged on top, ticked from the host audio callback or from VSync at the original rate.
- XA: decode Form 2 XA ADPCM sectors (37.8/18.9 kHz, mono/stereo) and mix them in as CD audio. Seed: rood-boi `xa_audio.c` (CC0, verified against ffmpeg `adpcm_xa`).
- Host: SDL3 audio stream at 44.1 kHz or 48 kHz with resampling. The SPU runs on the audio thread, protected by a small lock-free command queue from the game thread.
- Validation: render sequences headlessly to WAV and compare spectrally with vgmstream/DuckStation captures (tolerance-based), and compare AKAO event streams against vgmtrans.

### 10.4 CD / streaming (`Platform\cd` + `Content\disc`)

- **Virtual disc**: an LBA → (file, offset, mode) map.
  - From `.bin/.cue`: read raw 2352-byte sectors (exact, includes XA subheaders). This is the preferred source.
  - From an extracted folder: build the LBA table from the ISO9660 directory (or DataCrystal's SLUS LBA list) and synthesize Form 1 sectors. Interleaved STR/XA files (`MOV\TITLE.STR`, `ENDING\*`) need their raw form, so the `.bin` is **required** for movies and XA unless dumpsxiso raw (2336-byte) extracts are provided. `disks\SLUS-01040.xml` marks them as `type="mixed"`.
- libcd/libds API: `CdInit`, `CdSearchFile`, `CdIntToPos`/`CdPosToInt`, `DsInit`, `DsControl`/`DsCommand` (SetLoc, ReadN/ReadS, Pause), `DsRead`/`DsReadSync`, `DsReadCallback`, `DsSyncCallback`, `StSetStream`, `StGetNext`, `StFreeRing`…
- Timing: reads complete after `bytes / (speed × 150 KB/s)` of **game time** (VSync ticks), with an "instant" option. Streaming (`StGetNext`) delivers sectors at the configured rate, so movie and XA sync behave as on hardware.
- Seed: rood-boi `cd_drive.c` (CC0).

### 10.5 MDEC / movies (`Platform\mdec` + decomp `libpress`)

- The title movie path in `22C.c` (`_initMoviePlayback` → `StGetNext` → `DecDCTvlc` → `DecDCTin`/`DecDCTout` → `LoadImage`) is kept **as is**.
- `DecDCTin`/`DecDCTout` are implemented with a software MDEC (IDCT, YUV→RGB15/24). `DecDCTvlc` comes from the decomp's `libpress` (C) or `VLC_C.s` → C translation (rood-boi has a CC0 one).
- `TITLE.STR`: 320×224 at 15 fps with interleaved 37.8 kHz stereo XA.
- Validation: frame hashes/PSNR versus jpsxdec decodes of `TITLE.STR`.

### 10.6 Input (`Platform\pad` + `Desktop\input.cpp`)

- `PadInitDirect`, `PadStartCom`, `PadGetState`, `PadInfoMode`, `PadSetAct`/`PadSetActAlign` (vibration) and the raw 34-byte pad buffers the game parses (`vs_main_processPadState`).
- Buffers are filled each VSync from SDL3 gamepad state (DualShock layout: digital + analog + vibration through `SDL_RumbleGamepad`), with keyboard fallback and configurable bindings in `config.ini`.
- Port 2 / multitap: reported as disconnected.

### 10.7 Memory card (`Platform\card`)

- libcard/libapi file calls (`InitCARD`, `StartCARD`, `_card_info`, `_card_load`, `open("bu00:…")`, `read`, `write`, `firstfile`/`nextfile`, `erase`) on a standard 128 KiB `.mcd` image per slot in the user's save folder. Compatible with MemcardRex and DuckStation.
- Seed: rood-boi `memory_card.c` (CC0).

### 10.8 System/timing (`Platform\sys`)

- `VSync`, `VSyncCallback`, `SetRCnt`/`GetRCnt`/root counters (as derived from the game-time clock), `ChangeClearPAD`, `ResetCallback`, `StopCallback`.
- One fixed-step game loop at 59.94 Hz. The host may run faster for presentation (M5 interpolation), but the game logic always sees exact ticks.
- Deterministic mode (tests and replays): the clock advances only on `VSync(0)`, pad input comes from a recording, and CD timing is instant or scripted.

---

## 11. Desktop host (`VagrantStory.Desktop`)

- SDL3 window and renderer (or GPU device), a texture upload of the display area each frame, and integer/aspect scaling.
- Audio device, gamepad hotplug, keyboard bindings.
- `config.ini` holds: game data path (`.cue` or folder), memory-card folder, window scale, fullscreen, vsync, CD speed, audio rate, bindings.
- First-run check: validates the disc by hash and shows a clear message box if data is missing. No silent failures.
- Logging (`vs.log`) plus Tracy zones in Profile builds.
- Debug overlay (Debug configuration only): FPS, current overlay, CD queue, VRAM viewer (dump to PNG on a hotkey).

---

## 12. Testing and validation

| Layer | Method | Tooling |
|---|---|---|
| Decomp correctness | Matching build stays 100% for changed functions | objdiff (upstream repo) |
| Data generator | Re-pack generated C into bytes and compare with the original segments (excluding relocations) | `vsport gen-data --verify` |
| Struct layout | Compile-time asserts (§5.3) | clang-cl |
| GTE | ps1-tests expected values plus captured differential vectors | `VagrantStory.Tests` |
| GPU | Per-primitive VRAM tests; scene VRAM dumps compared with emulator dumps (tolerance) | DuckStation/PCSX-Redux dumps, ImageMagick compare |
| MDEC/XA | Frame PSNR/hash and audio correlation versus jpsxdec/vgmstream | `tests\golden` (hashes only) |
| SPU/AKAO | Headless WAV renders versus emulator captures; event streams versus vgmtrans | vgmtrans, vgmstream |
| CD | LBA map compared with DataCrystal SLUS LBA list and the `.bin` directory | Content tests |
| Integration | Deterministic boot replays: "boot → title in N ticks", "New Game → first map" with screenshot checkpoints | headless host |
| Boundary | `Check-NativeBoundary.ps1`, `vsport audit` thresholds | CI |

Golden files hold **hashes and metrics only**, never game frames or audio.

---

## 13. Milestones

Every milestone ends with: the solution builds from a clean clone (plus user data), tests pass, the audit thresholds for the included units are met, and a short `Docs\milestones\Mx.md` report.

### M0 — Solution, tooling, spikes (foundation)

- [ ] `VagrantStory.sln`, the six projects, `Directory.Build.props/targets`, props files, x64 platform.
- [ ] `.gitignore` update (`build\`, `out\`, `*.user`, `src\GameData\gen\`, game data folders) and removal of stale entries.
- [ ] `tools\vsport` skeleton: `import`, `gen-data`, `audit`, `boundary`; `Fetch-References.ps1`.
- [ ] **Spike A**: bitfield layout under clang-cl (D4). Result recorded in `Docs\decisions\`.
- [ ] **Spike B**: compile all of `src\SLUS_010.40\*.c` (not `lib*`) with the prelude; count undefined symbols by category.
- [ ] **Spike C**: `gen-data` on SLUS only; link Spike B against it; the remaining undefined symbols should be SDK-only.
- [ ] Desktop host opens a window, plays a test tone, reads a pad, opens the `.cue` and lists the root directory.

Exit: `VagrantStory.Desktop.exe` links SLUS + generated data + stub Platform, and calls `vs_main_exec`, which reaches `_sysInit` before the first stub assert.

### M1 — Title slice (the original target)

The order follows the real code path (`vs_main_exec` → `_sysInit` → `vs_main_execTitle` → `_sysReinit` → `_loadTitlePrg` → `vs_title_exec`):

1. **Boot**: `_sysInit` (`SetVideoMode`, `ResetGraph`, `InitCARD`, `PadInitDirect`, `DsInit`) and `_sysReinit` (heap, CD queue, menu sound load) run against Platform.
2. **TITLE overlay**: imported and linked; `_loadTitlePrg` satisfied by `vs_overlay_activate(TITLE)`.
3. **Developer/company screens**: `_initEnvironment` / `_copyTitleBgData` → the logos in VRAM and on screen. GPU phase 1 (sprites, tiles, textured quads, `LoadImage`/`StoreImage`/`MoveImage`).
4. **Opening music**: the menu/title sound data loaded via CD into SPU RAM, and the AKAO driver ticking. Software SPU + SDL audio.
5. **Opening FMV**: `_initIntroMovie` / `_playIntroMovie` → `TITLE.STR` with StSetStream/StGetNext, `DecDCTvlc`/`DecDCTin`/`DecDCTout`, interleaved XA audio. Skippable with Start, as on PS1.
6. **Title art and menu**: `_initTitleScreen`, `_initMenu`, the fades (`_fadeInMenuCopyright`, `_fadeInMenu`), `_drawTitleMenu` showing **New Game / Continue / Vibration / Sound**.
7. **Controller**: `vs_main_processPadState` sees SDL gamepad and keyboard; up/down moves the cursor with the menu sound effects; Start/Circle selects; the idle timeout (1000 ticks) loops back to the movie.
8. **Memory card**: `_saveFileExists` reads `.mcd`, so Continue is enabled when a save exists.

Audit gate for TITLE + SLUS paths exercised: no HW/scratchpad/KSEG literals, no raw ptr↔int casts, OT sites converted, and no unhandled `INCLUDE_ASM` on the executed path.

Validation: a deterministic replay reaches the menu; screenshot metrics match DuckStation captures at the logo, mid-FMV and menu checkpoints; the audio correlation check passes for the title music.

### M2 — New Game → first playable map

- `_loadBattlePrg` / `vs_battle_exec`, INITBTL, the first event scripts (`EVENT\*.EVT`), the map loader (ZND/MPD/ZUD), actor models (SHP/SEQ/WEP), the camera, and the first cutscene.
- Here BATTLE's 78% generic naming and most hardware literals live. Rename upstream in the order of the execution trace (Ghidra + PCSX-Redux breakpoints), and decompile the BATTLE-path `INCLUDE_ASM`s upstream.
- GTE completeness and the GPU primitives used by 3D (Gouraud textured polys, semi-transparency).
- Exit: walk around the first map and trigger the first battle.

### M3 — Menus, saving, effects

- MAINMENU, MENU0–F, the memory-card save flow, GIM/SCREFF2, the EFFECT plugin set (72, with prefixing), and the in-battle UI.
- Exit: save in game, reboot, Continue → resume.

### M4 — Full game

- ENDING, all maps and bosses, and a long-play stability pass (deterministic replays of key segments).
- LP64 cleanup completes: audit ptr↔int / OT counts at zero across **all** units (not just executed paths); `-Werror` cast flags enabled tree-wide.
- Exit: complete playthrough on x64.

### M5 — Enhancements (post-parity, opt-in)

- Hardware renderer with internal resolution scale, widescreen hacks (careful culling), subpixel precision, texture filtering, frame-rate interpolation, and asset replacement through `VS.Content`.

---

## 14. Work breakdown for naming and asm (supporting all milestones)

| Work | Approach | Owner location |
|---|---|---|
| 93 `INCLUDE_ASM` blocks | Prioritize by trace coverage per milestone; m2c → permuter → objdiff in the decomp | Upstream |
| 223 asm glabels in `.s` | SDK ones replaced by Platform; game ones decompiled or hand-ported to C | Both |
| `func_` names (1,662) | Name when touched by porting or when the trace hits them; use DataCrystal, GodHands and vstrack structs as hints | Upstream |
| `D_` globals (1,185) | The data generator report lists types needed; name them with types | Upstream |
| `unk` fields (639) | Name when a Platform boundary or a layout assert touches them | Upstream |

Progress is reported by `vsport audit` per unit in each milestone report.

---

## 15. Risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| clang-cl cannot reproduce GCC bitfield layout (D4) | Wrong struct reads | Spike A in M0; fall back to accessor macros for affected structs. |
| Pointer-vs-integer ambiguity in data (`.word`) | Bad relocations, crashes | spimdisasm relocation info + `datarelocs.yaml` overrides + `--verify` repack. |
| Hidden reliance on memory layout (code that walks across adjacent globals, or assumes overlay slots overlap) | Subtle corruption | Generator keeps segment ordering within a unit (one array per segment if needed); ASan in Debug; layout-overlap list from splat. |
| Timing-dependent logic (CD polling, VSync counts, root counters) | Hangs or desyncs | Game-time clock, configurable CD speed, deterministic mode. |
| Self-modifying code / code read as data in overlays | Wrong behavior | Detect via generator (data refs into `.text`); hand-port such sites. |
| EFFECT plugin count and collisions | Link failures | Automatic prefixing and static-by-default. |
| Licensing drift (copying from GPL/NC repos) | Legal | TOOLS_INVENTORY reuse classes; `THIRD_PARTY.md`; review checklist. |
| Upstream decomp churn | Merge pain | Manifest + 3-way merge tool; renames done upstream first. |
| On-disc structs with 32-bit pointer slots patched in place (x64) | Crashes or corruption in map/model/effect loading | §4.1 rule: `vs_ptr32` fields + `VS_DPTR` accessors or reformat-on-load; per-format notes in `Docs\formats\`; ASan in Debug. |
| Heap exhaustion from larger x64 structs | Allocation failures mid-scene | 2× heap plus peak-use telemetry; grow per data. |
| LP64 fixes on the critical path slow M1/M2 | Schedule | Fix only executed paths per milestone (trace-driven), with tree-wide cleanup in M4. |

---

## 16. Immediate next actions

1. Commit the cleanup (deleted old build), and restore `resources\RE_Docs` if it should be kept (it shows as deleted in `git status`).
2. Scaffold M0: the solution, projects, props, `.gitignore`, `tools\vsport` skeleton, `Fetch-References.ps1`.
3. Run Spike A (bitfields) and Spike B (SLUS compile census) and record the results in `Docs\decisions\`.
4. Write `THIRD_PARTY.md` before vendoring SDL3, rood-boi devices and PsyCross GTE.
