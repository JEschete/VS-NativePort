# Vagrant Story — Native Port Battle Plan

Working plan for turning the `rood-reverse` matching decompilation into a native
PC executable (the Ship of Harkinian model: decomp first, then replace the
hardware layer).

Baseline: upstream `c346ab20`, local branch `work`. Build environment is WSL2
Ubuntu 24.04; `make` is green and all **21/21 target binaries reproduce the
retail disc byte-for-byte**.

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

1. **Finish `libgte` (7 functions) first — cheap.** `SetGeomOffset`,
   `SetGeomScreen`, `SetFarColor`, `SetFogNear`, `DpqColor` are plain control
   register writes (OFX/OFY, H, FC, DQA/DQB) against the COP2 built in Phase 1.
   `rsin`/`rcos` are fixed-point trig lookups. Half a day, closes out libgte.
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
gamepad. This proves GPU + input + timing + asset loading end-to-end **without
needing the engine at all.**

**Guard rails:** PSX build stays 21/21; prefer `#ifdef NATIVE_BUILD` branches
inside existing headers (as done for `_insertTPage`) over edits that could
perturb matching.

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
