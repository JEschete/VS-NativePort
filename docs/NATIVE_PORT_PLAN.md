# Vagrant Story — Native Port Battle Plan

Working plan for turning the `rood-reverse` matching decompilation into a native
PC executable (the Ship of Harkinian model: decomp first, then replace the
hardware layer).

Baseline: upstream `c346ab20`, local branch `work`. Build environment is WSL2
Ubuntu 24.04; `make` is green and all **21/21 target binaries reproduce the
retail disc byte-for-byte**.

---

## 0. Current status — START HERE (resume point)

> **Every native-only no-op, no-draw stub, and synthetic shortcut is tracked in
> [`docs/NATIVE_DEVIATIONS.md`](NATIVE_DEVIATIONS.md).** When you add or remove
> one, update that ledger. The running "not yet implemented" frontier is the 371
> auto-generated abort stubs in `native/platform/stubs.c` (self-maintaining).

### ✅ ITEMS SUBMENU ENTERED + RENDERS (tip `b380fbe8`)
`make -f Makefile.native menu` now confirms into the ITEMS submenu and renders
it natively: category icon row, OPTIONS/EQUIP/SETUP list + cursor, and real
text. Deviations vs. the real screen (all logged in the ledger): no 3D Ashley
portrait (A1), no animated rune backdrop (A2), no INFORMATION box yet (A4).
In-submenu category nav needs real inventory (a loaded `.mcd`) — deferred to
the save system. See the deviations ledger for the full bring-up chain.

### ✅ PHASE 2 EXIT CRITERION MET (tip `20f1becf`)
`make -f Makefile.native menu` renders the **full Vagrant Story command menu**
natively — all tabs (MAGIC, BREAK ARTS, BATTLE ABILITIES, ITEMS, STATUS, MAP,
DATA, OPTIONS, SCORE, QUICK MANUAL) as real text with cursor + gradient
highlights — and it **responds to input** (injected presses move the cursor;
95 primitives, ~16k pixels). That is Phase 2: *the menu renders natively and
responds to a gamepad.*

The unlock was decompiling **`func_800C930C`** (the 300-instruction main-menu
tab input/build handler) to native-functional C in
`native/platform/battle_menu.c` — it was assembly, stubbed, aborting at
`currentState=0`. PSX build keeps the asm; native gets the C.

**Immediate polish / next items:**
- **Cursor cosmetic:** multiple selection highlights can persist as the cursor
  moves (a cursor-clear nuance in the native `func_800C930C`, or prim-buffer
  reuse between frames). Cosmetic; tidy first.
- **Audio (Phase 3):** the menu SFX dispatchers in `battle_asm.c` are no-oped
  because the SPU voice state is uninitialised (`vs_main_playSfxDefault` faults).
  Restore real dispatch when audio lands.
- **Submenu content:** entering a tab (confirm) should open its item list
  (`vs_mainMenu_renderItemsList`, already real, reads `vs_main_buttonsPressed`).
  Inventory is populated; wire the confirm → submenu path.
- **Real save:** optionally load a memory-card save instead of synthetic state.

---

**Branch `work`, tip `20f1becf`.** Both builds green:
- **PSX matching build:** `make -j"$(nproc)"` → 21/21 byte-identical (the oracle; must never break).
- **Native build:** `make -f Makefile.native stubs && make -f Makefile.native` → `native/build/vagrant-native` links and runs (32-bit x86 ELF).
- **Tests:** `make -f Makefile.native test` → GTE 891/0 (hardware-verified), GPU 18/0.
- **`make -f Makefile.native menu`** runs the equipment menu and renders **real Vagrant Story menu text** ("MENU", "MAGIC") to `native/build/menu.ppm`.

**🎉 The equipment menu renders natively with real glyphs.** The full path
executes and rasterises: `func_800C97BC → vs_mainMenu_exec → renderScreen →
_renderMenuItems → vs_battle_renderMenuItem → printVariableWidthFontChar`, the
selected-menu bar slides in from the right and settles, and the font (loaded
from `SYSTEM.DAT`) draws as readable text.

**What is done (Phases 0–1b + Phase 2 render pipeline COMPLETE):**
- `libgte` complete and **verified bit-exact vs real COP2** (DuckStation capture ROMs).
- `libetc` (60Hz pacer), `libapi` (events, card). Binary linked at `0x10000` so PS1 24-bit OT addresses work natively; scratchpad `mmap(MAP_FIXED)`'d at real `0x1F800000`.
- **`libgpu` — full GP0 command-list rasteriser:** walks each OT packet as a GP0 word stream (texpage `0xE1`, polygons `0x20-0x3F`, rects/sprites `0x60-0x7F`), with **CLUT texture sampling** (4/8-bit + 15-bit) for sprites AND **affine-UV textured triangles** (POLY_FT4/POLY_GT4). Flat + Gouraud + textured all work.
- **Font/UI textures load via the real game loaders:** `menu_harness.c load_system_dat()` reads `data/BATTLE/SYSTEM.DAT` from the extracted file (not disc) and drives `LoadImage` / `vs_main_loadClut` / `func_800CA9C0` → font at VRAM (832,0), tpage 0x2D.

**RESUME POINT — render is done; the menu shows the TAB BAR, and displaying a
submenu item-list needs driving the menu's cursor navigation.**

Findings this session:
- **Inventory data path works.** `menu_harness.c` now populates `vs_main_inventory`
  (2 weapons + 10 grimoires) using the verified layout — weapons[8]@0x000 stride
  0x20, present iff `.blade`(+1)!=0; misc[64]@0xE00 stride 8, present iff
  `.id`(u16@0)!=0; grimoires are misc IDs 0x82+. `vs_main_inventory` is a writable
  0x1000 blob (exact struct size). This alone changes nothing on screen, because
  the current render is the top-level **tab bar** (`vs_mainMenu_exec` → the 10
  `vs_battle_renderMenuItem`s), not a submenu list.
- **Navigation is analog-stick, not discrete buttons.** Tab/row selection goes
  through `vs_mainmenu_getSelectedRow()` (reads the stick position buffer), and
  `func_800C930C` maps the cursor to a `currentState` that opens a submenu. So to
  render an item list you must script the cursor: set the stick-position state
  (`vs_main_stickPosBuf` / the pad analog buffer) so `getSelectedRow` returns a
  row, then confirm. `vs_main_buttonsPressed.all` (offset 0;
  `PADRright=0x20 left=0x80 up=0x10 down=0x40`, `PADn=0x04` etc.) is the discrete
  side; the write path is verified.

**Next = script menu navigation into a submenu → item list renders → that IS the
Phase 2 exit criterion (render + input).** Then optionally load a real save
(memory card) instead of synthetic inventory. Minor: confirm textured POLY
panels sample their art.

**Root-cause fix landed this session (the real reason `scratchpad[0..2]` read null):**
`D_1F800000` and `_camera` both *alias* the scratchpad at `0x1F800000` in the
retail build (per-overlay `--defsym`s), but the native stub generator gave each
its own separate 4KB blob — so `D_1F800000[i]`, `*(void**)0x1F80000{0,4,8}` and
`_camera.unk{0,4,8}` touched three different memories and the render pointers
looked null to half the code. Fixed by excluding both in `gen_stubs.py`
(`SCRATCHPAD_ALIASES`) and adding the identical `--defsym`s to `Makefile.native`
`LDFLAGS`. The `mmap` was necessary but not sufficient; this is the true fix.
Details in [`PSX_NOTES.md`](PSX_NOTES.md) → "Demand-driven menu bring-up".

**The equipment-menu render path now runs end-to-end natively** (was: faulted /
ran away). Four fixes got it there this session:
- **`-funsigned-char`** — MIPS `char` is unsigned; x86 defaults to signed, so
  string terminators (`0xE7`/`0xFF`) sign-extended and terminator checks failed,
  running the glyph loop away. Load-bearing like `-D_LANGUAGE_C`.
- **Data harvest** — `native/tools/harvest_data.py` copies real read-only tables
  (menu strings, glyph widths, animation steps) from the retail overlays at their
  known symbol addresses into `native/platform/harvested_data.c`
  (`make -f Makefile.native harvest`; curated via `HARVEST_SYMS`).
- **Synthetic actor state** — the equipment panel derefs
  `vs_battle_actors[i]->unk3C` (a `vs_battle_actor2` stat block); the harness
  wires a zeroed but non-null pointer chain (`actors[i] → actor → actor2`).
- **Faithful sprite builders** — `vs_battle_setSprite*` in `battle_asm.c`
  reimplemented from the 573B8 asm (len-5 compound texpage+SPRT), replacing the
  no-draw stubs.

`func_800C97BC` now executes to completion for 3 frames, emitting 7–8 real
primitives, **no fault**.

**Exactly where it stops now (the next task): nothing is visible yet.** VS
threads **GP0 command-list** primitives — each OT packet prepends a texpage
command (`0xE1…`) before the real SPRT/poly — and the native `draw_one` decodes a
fixed struct layout, so it reads the `0xE1` as the primitive code and skips the
packet. Making the menu visible needs a rasteriser that walks each packet as a
GP0 command stream, the **font texture in VRAM** (tpage 0x2D → VRAM (832,0),
4-bit, CLUT (0,222)) loaded via the asset path the harness bypasses, and
**CLUT-textured sampling**. This is a scoped rendering sub-project; the geometry
pipeline itself is proven (harness driving `vs_battle_addTile`: 4 prims, 22200
non-black pixels). Then Phase 2 step: **libpad** input to meet the exit
criterion. Detailed traps in [`PSX_NOTES.md`](PSX_NOTES.md) → "Demand-driven menu
bring-up".

**Working rule:** every change keeps PSX 21/21. Diverge freely in `native/`, `docs/`, tooling; keep `src/` edits minimal and guarded with `#ifndef NATIVE_BUILD` so upstream merges stay cheap.

---

## 1. Where we actually stand

From `progress.json` at `c346ab20`:

| Category | Matched code | Size (bytes) | Port readiness |
|---|---:|---:|---|
| `menus` (MAINMENU + MENU0..F) | **100.00%** | 318,340 | Ready |
| `title` (TITLE.PRG) | **100.00%** | 40,524 | Ready |
| `battle.INITBTL.PRG` | **100.00%** | 4,864 | Ready |
| `ending` | 85.17% | 23,336 | Nearly |
| `battle.SCREFF2.PRG` | 77.28% | 2,324 | Nearly |
| `lib` (SLUS_010.40, PSYQ) | **45.94%** | 157,720 | Half — this is the HAL boundary |
| `battle.BATTLE.PRG` (engine) | **35.28%** | **517,400** | The long pole |

Overall: 60.16% code matched, 34.38% "complete" (fully linked), 2667/3702
functions matched, 31/244 units complete.

**The honest headline:** a *playable* native port is gated on `BATTLE.PRG` — the
actual game engine, the largest module by far, and only ~1/3 matched. No amount
of platform-layer work changes that. But the menu subsystem is **100% matched and
already fully relocatable**, which gives us a real beachhead to build and prove
the entire platform layer against, long before the engine is done.

---

## 2. Architecture decision: dual-target, one source tree

**Do not fork the repo into a "port" branch that drifts.** Adopt the model
proven by `sotn-decomp` and friends:

```
one source tree
├── target: psx    (mipsel + PSYQ cc1)  -> must stay byte-matching. Ground truth.
└── target: native (clang/gcc x86)      -> links a host platform layer instead of PSYQ
```

The matching PSX build stays the **regression gate**. Every change must keep
`make` green and 21/21 byte-identical. The native target is additive: same C, a
different implementation of the PSYQ libraries underneath.

Why this matters: the moment the two diverge, you lose the ability to prove your
C is still faithful to the original game. The matching build is not overhead —
it is the correctness oracle that makes the port trustworthy.

---

## 3. The hardware boundary to reimplement

The abstraction seam is already in the tree: `src/SLUS_010.40/lib*` and the
headers in `include/psx/`. That is exactly what a native target must replace.

| Layer | What it is | Native strategy | Difficulty |
|---|---|---|---|
| `libgte` | Geometry Transformation Engine (COP2 fixed-point 3D coprocessor) | Pure C reimplementation. Deterministic, no I/O, unit-testable. | Medium — must replicate saturation/overflow quirks exactly |
| `libgpu` | Primitive/display-list rasteriser | Translate primitives to a modern API (SDL3 GPU / OpenGL) | High |
| `libspu` | ADPCM voices, reverb, mixing | Software mixer → SDL audio | High — reverb is fiddly |
| `libcd` / `libds` | CD sector reads, XA streaming | Virtual FS over extracted disc data | Medium |
| `libpad` | Controller | SDL gamepad | Low |
| `libetc` | VSync / interrupt callbacks | Host frame timing loop | Medium — drives game pacing |
| `libapi` / kernel | BIOS syscalls, events, malloc | Host runtime shims | Medium |
| `libcard` / `libmcrd` | Memory card | Save files on disk | Low |
| `libc` | string/mem/math | Host libc | Trivial |

`libgte` first, deliberately: it is pure math with **no host dependencies**, so
it can be verified by differential testing against the PSX build before any
rendering exists.

---

## 4. Known hazards (decide these early — they are expensive later)

1. **32-bit pointer assumptions.** PS1 pointers are 32-bit; the decompiled
   structs assume that. On x86-64 every pointer-bearing struct changes size and
   layout, silently breaking anything that memcpy's or casts structs.
   *Options:* (a) build the native target as **32-bit** (`-m32`) to sidestep it
   entirely — what several PS1 ports do; (b) audit and fix structs for 64-bit
   cleanliness. **Recommendation: start with `-m32`.** Revisit only if a 64-bit
   build becomes a hard requirement.

   Decision: **32-bit native build**. The port is a long-term project; we can
   revisit 64-bit later once the port is fully functional and the structs are
   audited.

2. **Absolute PS1 addresses.** The code and data tables contain baked-in
   `0x800xxxxx` addresses (we saw them in MAINMENU's tables). Anything treating
   an address as a pointer must be rebased or indirected for the native target.

   Decision: **rebase all absolute addresses** to a virtual base (e.g. `0x80000000`),
   and use a `native_ptr` type to wrap them. The native target will then map
   the virtual base to a host pointer. This is a one-time change to the decompiled
   code, and it is **not** a runtime cost — the compiler will optimise away the
   wrapper.

3. **Overlay loading model.** The game loads `.PRG` overlays from disc **by
   sector** (`BATTLE.PRG` does sector-based loading; the README notes rebuilding
   the disc requires preserving dummy-sector counts). Native either keeps the
   overlay model over a virtual disc, or statically links everything and stubs
   the loader. Keeping the overlay model is closer to the original and less
   invasive early on.

   Decision: **keep the overlay model**. It is a one-time cost to implement a virtual disc loader, 
   and it is the only way to preserve the original disc layout for modding and patching. 
   Statically linking everything is a one-time cost to implement, but it is invasive and makes modding harder.

4. **Undefined behaviour.** Decompiled C matched against a 1997 compiler is full
   of constructs modern compilers treat as UB (type punning, strict aliasing,
   signed overflow). The native target will need `-fno-strict-aliasing`
   `-fwrapv` and friends.

   Decision: **compile the native target with `-fno-strict-aliasing -fwrapv`**. 
   This is a one-time cost to the build flags, and it is the only way to preserve
   the original semantics of the decompiled code.

5. **Frame pacing.** The original is locked to VSync-driven timing. Decoupling
   logic from a 60Hz assumption is what enables high-framerate later — but do
   *not* attempt it before the port runs at all.

   Decision: **keep the original 60Hz pacing**. The native target will implement a
   host frame loop that sleeps to maintain 60Hz, and the game logic will remain
   locked to that. Later, we can add a framerate-independent mode, but only after
   the port is fully functional.

---

## 5. Phased plan

### Phase 0 — Native build scaffolding — ✅ **COMPLETE**
- Add a `native` target to the build that compiles the same `src/` with host
  gcc/clang, `-m32`, `-fno-strict-aliasing -fwrapv`.
- Stub every `libgte/libgpu/libspu/libcd/libpad/libetc/libapi` entry point so it
  links (abort-on-call bodies).
- **Exit criterion:** something links natively and runs to the first stub abort.
  Nothing renders. That is fine and expected.
- **Guard rail:** PSX build still green, 21/21 byte-identical.

**Outcome.** `native/build/vagrant-native` is a 32-bit x86 ELF that links and
runs. Build it with:

```sh
make -f Makefile.native stubs && make -f Makefile.native
```

`Makefile.native` is deliberately **standalone** — it cannot disturb the
matching build. Both verified green: PSX **21/21 byte-identical**, native links
and runs.

Added:

| Path | Purpose |
|---|---|
| `Makefile.native` | standalone native target |
| `native/include/psx_compat.h` | native-only compatibility shim |
| `native/include/psx_scratchpad.h` | host stand-in for the PS1 scratchpad |
| `native/platform/{stubs,unimpl,scratchpad,main}.c` | stub/platform layer |
| `native/tools/gen_stubs.py` | regenerates stubs from undefined symbols |

**Flags that make 1997 game code build on modern GCC:**
`-m32 -std=gnu89 -nostdinc -DNATIVE_BUILD -D_LANGUAGE_C -fno-builtin
-fno-strict-aliasing -fwrapv -fno-pic -fno-stack-protector`, link `-no-pie`.

> `-D_LANGUAGE_C` is essential and non-obvious: most of `include/psx/kernel.h`
> (including `struct DIRENTRY`) lives inside
> `#if defined(_LANGUAGE_C)||...`, which the PSYQ compiler predefines and modern
> GCC does not — without it, half the header silently disappears.

**Coverage:** 25 of 28 fully-decompiled MENU+TITLE TUs compile natively.
Stub layer is 268 abort-on-call functions + 21 data blobs. Symbols host libc
already provides (`memset`, `strlen`, `sprintf`, …) are deliberately left
unstubbed so they bind to the real implementation.

**Deferred (`NATIVE_EXCLUDE` in `Makefile.native`)** — 3 TUs needing `static`
vs non-`static` linkage fixes, which change symbol linkage and so must each be
verified against the byte-matching build before being touched:
`src/MENU/MENUB.PRG/260.c`, `src/MENU/MENUF.PRG/3B8.c`,
`src/TITLE/TITLE.PRG/libpress/LIBPRESS.c`.

**Source changes made, each verified to keep PSX at 21/21:** moved the
`textHeader_t` typedef from `58EC.c` into `58EC.h`; corrected `sizeof int` to
`sizeof(int)` in `TITLE.PRG/22C.c`; added an `#ifdef NATIVE_BUILD` C
reimplementation of the MIPS-asm `_insertTPage` macro in
`src/include/insertTPage.h` (the PSX branch is untouched, so it cannot affect
matching).

> ⚠️ **Toolchain trap:** `gcc-multilib` and `gcc-mipsel-linux-gnu` cannot coexist
> on Ubuntu 24.04 — installing the former silently removes the latter. Only the
> *driver* disappears (`cpp`/`as`/`ld`/`objcopy` survive), so the matching build
> keeps compiling and fails later in `dumpStruct.py` with a confusing asset
> error. Keep `gcc-13-multilib` (it provides 32-bit `libgcc`) and reinstall
> `gcc-mipsel-linux-gnu`; both then coexist.

### Phase 1 — `libgte` in C — ✅ **core complete** (validation partial)

**Scope discovery.** `src/SLUS_010.40/libgte/` is *not* decompiled — every `.c`
there is a ~100-byte `INCLUDE_ASM` wrapper around hand-written MIPS. But the
fully-decompiled MENU + TITLE subset only reaches **four** GTE entry points, so
Phase 1 is bounded by what those need, not by all of libgte.

Semantics were read directly off the original assembly in
`build/src/SLUS_010.40/nonmatchings/libgte/` — not guessed:

| Entry point | COP2 behaviour |
|---|---|
| `SetRotMatrix` | `ctc2` MATRIX words 0..4 → control regs 0..4 (R11R12…R33) |
| `SetTransMatrix` | `ctc2` MATRIX+0x14/18/1C → regs 5..7 (TRX/TRY/TRZ) |
| `ApplyMatrix` | load V0, `mvmva 1,0,0,3,0` (sf=1, rot matrix, V0, **no** translation), store MAC1..3 |
| `RotTransPers` | load V0, `rtps`, store SXY2→`sxy`, IR0→`p`, FLAG→`flag`; **returns SZ3>>2** |

**Delivered.**

| Path | Purpose |
|---|---|
| `native/include/psx_gte.h` | COP2 register indices, FLAG bit definitions, API |
| `native/platform/gte_core.c` | software COP2: `RTPS`, `MVMVA`, saturation, FLAG, UNR divide |
| `native/platform/libgte.c` | the four PSYQ wrappers (MATRIX layout pinned by static assert) |
| `native/tests/test_gte.c` | spec test harness (`make -f Makefile.native test`) |

Implemented to the documented hardware behaviour: 44-bit MAC intermediates with
per-component overflow flags, IR saturation honouring `lm`, SZ/SXY FIFO shifts,
`SX2`/`SY2` clamped to -400h..3FFh, `IR0` to 0..1000h, FLAG bit 31 as the sticky
OR of bits 30..23 and 18..13, and the hardware **UNR reciprocal** (generated
table, Newton-Raphson refinement, 1FFFFh saturation with FLAG bit 17).

**Result:** the 4 GTE stubs are gone — stub layer dropped 268 → 264 functions.
Test suite: **25 checks, 0 failures**. PSX build still **21/21 byte-identical**;
Phase 1 required **no game-source changes at all**.

### Phase 1b — differential validation against real COP2 — ✅ **COMPLETE**

The original exit criterion ("bit-exact against the PSX build") is now **met**.
Earlier there was no oracle available; that changed once DuckStation + a real
BIOS were on hand.

**Method.** Rather than trusting the spec, we run the *actual COP2 instructions*
on emulated hardware and diff:

1. `native/tests/psx/gte_capture.S` — a PS-EXE that walks a table of GTE cases,
   issues real `rtps` / `mvmva`, and prints inputs + outputs over BIOS TTY.
2. Built by `native/tests/psx/build.sh` (+ `native/tools/mkpsexe.py`).
3. Run headless-ish under DuckStation 0.1-11580 with the SCPH1001 BIOS;
   `I/TTY:` lines scraped from `duckstation.log`.
4. Captured to `native/tests/gte_golden.txt`.
5. `test_gte.c` replays every captured input through our C GTE and requires all
   11 output registers to match exactly.

The emitted opcodes were verified byte-for-byte against the game's own `libgte`
assembly (`rtps` = `0x4A180001`, `mvmva 1,0,0,3,0` = `0x4A486012`), so the ROM
exercises the identical instruction stream the game does.

**Result: 16 cases × 11 registers = 176 hardware comparisons, 0 mismatches.**
Full suite: **201 checks, 0 failures.** Coverage includes identity and rotated
matrices, translation, negative vectors, IR saturation, `SZ3 = 0` divide
overflow, `H >= 2*SZ3` overflow, non-zero `OFX/OFY` and `DQA/DQB`, and MVMVA.

Two findings worth keeping (details in [`PSX_NOTES.md`](PSX_NOTES.md)):

- **MVMVA does not write `IR0` / `SXY2` / `SZ3` / `MAC0`.** Those retain the
  previous op's values. A harness that resets GTE state per case will report
  false failures against a back-to-back hardware capture — model the state
  carry instead. Our implementation was correct; the *test* was wrong.
- The UNR sweep's `max deviation 1` versus the ideal quotient is **hardware
  behaviour**, not error. The golden vectors confirm our reciprocal matches the
  real thing.

Regenerate after any GTE change:
```sh
native/tests/psx/build.sh native/tests/psx/gte_capture.S native/tests/psx/gte_capture.exe
# boot under DuckStation, scrape I/TTY lines into native/tests/gte_golden.txt
make -f Makefile.native test
```

The GTE is now **verified, not merely plausible** — which matters because errors
here are silent: geometry drifts rather than crashing.

### Phase 2 — Beachhead: render a menu *(the motivating milestone)*
The menu subsystem is 100% matched and relocatable — the ideal first real target.

**Exactly what is left to implement**, measured from the live stub layer
(`native/platform/stubs.c`, 264 functions) rather than estimated:

| Library | Stubs remaining | Notes |
|---|---:|---|
| `libgpu` | **18** | `AddPrim` `DrawOTag` `DrawPrim` `DrawSync` `PutDisp` `LoadImage` `LoadImage2` `ClearImage` `FntLoad` `FntOpen` … — the bulk of Phase 2 |
| `libapi` | 14 | events, `EnterCriticalSection`/`ExitCriticalSection` |
| `libds` | 11 | CD-XA streaming (overlaps `libcd`) |
| `libgte` | 7 | `SetGeomOffset` `SetGeomScreen` `SetFarColor` `SetFogNear` `DpqColor` `rsin` `rcos` |
| `libcd` | 7 | `StSetStream` `StGetNext` … |
| `libetc` | 1 | `VSync` |
| `libspu` | 1 | `SpuSetCommonMasterVolume` |
| *(non-PSYQ)* | **207** | game symbols from `BATTLE.PRG`/`SLUS` — **not** platform work; they resolve as the decomp advances (Phase 5) |

That 207 is the important number: **most remaining stubs are not port work at
all.** Only ~59 are actual platform surface.

**Ordered steps:**

1. ~~**Finish `libgte` (7 functions).**~~ DONE. It turned out to be three tiers,
   not one: four plain control-register writes (`SetFarColor`, `SetGeomOffset`,
   `SetGeomScreen`, `SetFogNear`); a table-lookup pair (`rsin`/`rcos`, folding
   onto the 513 sin/cos pairs in `_trig_table`); and `DpqColor`, which issues
   the **DPCS** GTE op and so required implementing depth-cue interpolation,
   the colour FIFO and RGB saturation.

   **`libgte` now has zero stubs.** Verified: `rsin` matches `round(sin*4096)`
   with max deviation 0 across all 4096 angles; DPCS verified bit-exact against
   real COP2 (12 cases x 8 registers, 0 mismatches). Suite: 891 checks, 0
   failures. Stubs 264 to 257; PSX build still 21/21.

   Side effect worth knowing: `INCLUDE_ASM` is now a no-op under `NATIVE_BUILD`
   (the live guard is in `src/include/common.h`), so assembly-bearing TUs can be
   opted into the native build via `NATIVE_EXTRA` to harvest their data tables
   and already-decompiled functions. That is how `_trig_table` was obtained.
2. **`libetc` VSync + frame loop.** Per §4 decision, keep 60Hz: host loop that
   paces frames and drives the callback the game expects.
3. **`libgpu` in two stages.**
   a. *Display-list plumbing first, no pixels*: `AddPrim`, `DrawOTag`,
      `DrawPrim`, `DrawSync`, `PutDisp` walking the ordering table. Validate by
      **dumping the OT to text** and eyeballing primitive order/types — this
      shakes out the scratchpad/OT addressing (already stubbed via
      `_psx_scratchpad`) before any renderer exists.
   b. *Then rasterise*: back the primitives with SDL3 (or OpenGL). Start with
      flat quads, then textured quads/sprites, then CLUT handling.
4. **VRAM model.** `LoadImage`/`LoadImage2`/`ClearImage`/`StoreImage` need a
   1024×512 16-bit VRAM buffer with the PS1's texture-page/CLUT addressing.
   Get this right early — most texture bugs trace back to it.
5. **`libpad`.** SDL gamepad → the PS1 button bitfield the menus read.
6. **Asset loading.** Menus pull from `MENU/*.BIN` etc. Serve them from the
   already-extracted `data/` tree behind a minimal `libcd`; full sector-accurate
   streaming (`libds`) is not needed for menus and should be deferred.

**Exit criterion:** the equipment/status menu renders natively and responds to a
gamepad. This proves GPU + input + timing + asset loading end-to-end.

**Guard rails:** PSX build stays 21/21; prefer `#ifdef NATIVE_BUILD` branches
inside existing headers (as done for `_insertTPage`) over edits that could
perturb matching.

---

#### Phase 2 status (updated) — platform layer DONE, exit criterion BLOCKED on decomp

**The platform layer is complete for the menu path.** Steps 1–6 above are done:

| Component | State |
|---|---|
| `libgte` | complete, hardware-verified (Phase 1 + 1b) |
| `libetc` | VSync + 60Hz monotonic frame pacer |
| `libgpu` | ordering table, DISP/DRAW env, VRAM (1024×512, 5551), TILE + POLY_F4 flat + POLY_G4 Gouraud raster, VRAM transfers; SPRT/CLUT texturing still to come |
| `libapi` | events (EvCB model), memory-card + directory calls |
| `libpad` | already real (its code was decompiled and is now linked) |

`INCLUDE_ASM`, `BIOS_STUB` and `SYSCALL` are all no-ops under `NATIVE_BUILD`, so
**206 of 217 TUs compile** and **1319 real functions** are linked. The binary is
linked at `0x10000` so the PS1's 24-bit ordering-table addresses work natively
with no translation layer (heap/static/bss fit under 16MB; the **stack does
not** — `check_24bit()` guards against stack-allocated OTs). **No PSYQ function
the menu references is stubbed any more.** Tests: GTE 891 checks / 0 fail, GPU
18 checks / 0 fail; PSX still 21/21.

**Why the exit criterion is blocked — and the plan's beachhead premise was
wrong.** Measured from the object files, the menu is *not* independent of the
engine. It references **408 external symbols; 205 are real, 203 missing**:

| Missing | count | nature |
|---|---:|---|
| Functions | **118** | 78 `vs_battle_*`, 19 `func_*`, plus `vs_main_*` — still MIPS assembly |
| Data | **85** | 18 have known addresses; `.bss` is zero-correct, `.rodata`/`.data` is harvestable from the retail binary |

So "menu renders" is gated on **~118 `BATTLE.PRG` functions** — decompilation,
not port work. And the *static* closure is large: those 118 reference ~131 more
new symbols one level down, i.e. several hundred total. Blanket-decompiling the
closure is the long pole in disguise.

**Revised strategy: demand-driven ("abort-driven") bring-up.** The static
closure is large, but the *runtime* init+render path is a small subset of it,
and the stub layer's `UNIMPLEMENTED: <name>` abort is a perfect oracle for
exactly which functions that path touches, in call order. So: run the menu,
satisfy the function it aborts on, repeat — decompiling only the hot path
(tens of functions), never the whole closure. Sequence:

1. **Harvest data (extraction, not decomp).** Copy the 85 data symbols from the
   retail `SLUS`/`BATTLE` binaries at known addresses (`.rodata`/`.data`); leave
   `.bss` as zero-blobs. Removes a class of null-deref crashes for near-zero
   effort.
2. **Menu render harness + a stub "continue" mode** (log-and-return-0 instead of
   abort) so one run surfaces the whole missing hot-path set at once.
3. **Demand-driven decompile/implement** each function the harness actually
   hits — real upstreamable `BATTLE.PRG` progress, or a minimal honest stub if
   it is off the render path (save I/O, inventory mutation).
4. **Synthetic game state from the RA struct layouts** (see Phase 2.5) so the
   menu renders a real loadout instead of an empty screen, skipping the
   population logic entirely.

**Risk:** if the render path bottoms out in the battle actor/item system despite
a synthetic state, that is itself a definitive answer — the menu genuinely can't
be decoupled — and it redirects effort to `BATTLE.PRG` proper with no waste.

---

#### Phase 2 progress — the TU-unblock lever (partly spent)

A large accelerator surfaced: the 11 TUs in `NATIVE_EXCLUDE` are excluded only
because each contains a *few* hand-written inline-MIPS functions, yet they hold
**hundreds of already-matched functions**. `5BF94.c` alone is 212 functions
gated by one asm block (`_printFixedWidthFontChar`); `573B8.c` is 9 more.

Unblocking pattern (same as `_insertTPage`): wrap each file-scope
`__asm__("glabel …")` in `#ifndef NATIVE_BUILD` and provide C in
`native/platform/battle_asm.c`. Trivial functions (memcpy, sfx dispatch) get
faithful C; sprite/glyph builders get memory-SAFE stubs (bump a zeroed,
non-drawing primitive from scratchpad) marked `TODO(render)` for
DuckStation-verified reimplementation once the render path runs.

Done: `573B8.c` + `5BF94.c`. Result: **+197 real functions** (1319 → 1516),
menu dependency completion **50% → 57%**, PSX still 21/21.

Then `main.c` (356KB, 178 functions) — only 3 `__asm__` spots (`_asmNop`,
`vs_main_memcpy`, `vs_main_bzero`), guarded and given self-contained C. It alone
held 21 menu deps. Needed a no-op `__main` runtime shim.

`4A0A8.c` (236 fns) and `38C1C.c` (16 fns) stay excluded: between them they
define only **1** of the menu's remaining missing symbols — battle-engine code,
not menu code, and their inline-asm-inside-functions form is harder. Deferred.

**Cumulative: +296 real functions (1319 → 1615), menu deps 50% → 62%.**

#### Phase 2 progress — demand-driven bring-up reaches the render path

Two correctness fixes made the harness possible:
- **`gen_stubs.py` data/function classification.** It emitted any non-`D_`
  symbol as a *function*, so named data globals landed in read-only `.text` and
  writing them segfaulted. Now: a symbol is a function only if the disassembly
  defines it with `glabel`/`alabel`; else a writable 4KB blob. (~216 symbols
  reclassified.)
- **Scratchpad at `0x1F800000`.** Game code touches it by literal address, so
  it is `mmap(MAP_FIXED)`'d there by a constructor.

`native/tests/menu_harness.c` (`make -f Makefile.native menu`) drives the real
per-frame menu function `func_800C97BC`; the stub layer's `UNIMPLEMENTED` abort
names the next needed function in call order. It supplies synthetic backing
buffers for the pointer-globals the game allocates at init
(`vs_battle_menuItems/characterState/rowTypeBuf`).

Execution now reaches real menu render code and stops at the per-frame render
init gap — see §0 "Current status" for the exact call chain and next step.

---

### Phase 2.1 — "See something" milestone *(do this next; independent of the menu)*

Right now rendering goes to a VRAM buffer dumped as PPM. Before the menu works,
stand up a **live window** that presents the VRAM framebuffer, so there is a
real running application to look at and iterate against:

- SDL3 (available; DuckStation ships `SDL3.dll`, and WSLg gives WSL a display)
  window that blits the 1024×512 VRAM, or just the display-area sub-rect.
- Drive it with the 60Hz `libetc` pacer already written.
- Present the existing GPU test scene live; later, whatever the game renders
  appears in the same window with zero extra work.

**Exit criterion:** `vagrant-native` opens a window and shows moving/rendered
content at 60fps. This proves window + present + timing end-to-end and gives
every subsequent step a visible target — valuable even though no game logic
runs yet. It also de-risks the eventual menu render: the pipeline that will
display it is already proven.

### Phase 2.5 — RA-data structs *(parallel track, needs no decomp)*
`docs/RAInfo.md` (RetroAchievements code notes) documents the persistent
game-state layout — inventory at `0x60168` (weapon slots, stride `0x20`), item
/armor/material IDs, the full map-visited bitfield. Turn it into typed,
`static_assert`-checked structs and named symbols. This is independent of the
decomp, cross-corroborated by our own symbol map (`vs_main_inventory`,
`vs_main_scoredata`, `vs_main_mapStatus` all land on the documented addresses),
and it is what makes the synthetic game state in Phase 2 step 4 possible — and
the eventual foundation for the weapon-loadout hotkey mod.

### Phase 3 — Audio
- `libspu`: ADPCM decode, voice mixing, then reverb. SDL audio backend.
- **Exit criterion:** menu SFX, then title-screen music.

### Phase 4 — Title + ending sequences
- `title` is 100%, `ending` 85%. With Phases 1–3 done these should largely fall
  out, exercising streaming/video paths.

### Phase 5 — The long pole: finish the decomp
- `BATTLE.PRG` 35% → 100% and `lib` 46% → 100%. This is the bulk of remaining
  work and is **where contributing upstream pays off** — it is shared effort, and
  every matched function benefits the whole community, not just this port.
- **Exit criterion:** full playable native port.

**Phases 0–4 are genuinely useful work available today.** Phase 5 is the gate,
and it is a multi-person, long-horizon effort.

---

## 6. Working practices

- **Never break the matching build.** `make` green + 21/21 byte-identical is the
  gate for every commit. Use `make remake -j` before proposing anything.
- **Upstream what is upstreamable.** Decomp progress and genuine bugs (e.g. the
  missing `MAINMENU_PRG.py` fixup + the unanchored `data/` gitignore rule) belong
  upstream. Port-specific scaffolding stays local.
- **Keep the native layer behind the existing headers.** Do not rewrite call
  sites in game code to suit the host; implement `include/psx/*.h` contracts.
  Game code should not know it has been ported.
- Edit inside WSL (`~/rood-reverse-work`, VS Code Remote-WSL). Do not copy the
  tree back to Windows — that reintroduces CRLF corruption.

---

## 7. Immediate next actions

1. Land the `MAINMENU_PRG.py` fixup upstream (+ `.gitignore` anchor fix).
2. Phase 0: stand up the `native` build target with stubbed PSYQ entry points.
3. Phase 1: `libgte` in C + differential test harness.
4. Pick a first `BATTLE.PRG` function to decompile, to keep Phase 5 moving in
   parallel with port work.
