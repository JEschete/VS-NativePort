# Native-build deviations ledger

Every place the **native x86 build** deviates from faithful game behaviour —
no-ops, no-draw stubs, synthetic-state shortcuts, and skipped subsystems.
This is the "come back and finish it" list. The **PSX matching build is never
affected** by anything here (all deviations are native-only code paths or
`NATIVE_BUILD`-guarded `src/` edits; the matching build stays 21/21).

Legend for **Restore when**: the condition that makes it worth replacing the
shortcut with the real thing.

---

## A. Render no-ops / no-draw stubs (visible gaps)

These make the native menu render *incompletely* vs. the real game. Compare
`native/build/menu.ppm` against a DuckStation capture of the same screen.

| # | Symbol / site | File | Native behaviour | Real behaviour | Restore when |
|---|---|---|---|---|---|
| A1 | `_renderAshley` | `src/MENU/_renderAshley.h:29` (`#ifdef NATIVE_BUILD` early return) | Skips the 3D character portrait entirely | Draws Ashley + equipped weapons as a rotating 3D model (GTE-transformed, lit) | 3D model data slots (`D_800F4538`) are populated — needs the model loader + a real save/inventory |
| A2 | `func_8007ACB0` | `native/platform/battle_asm.c` | No-op | Builds the GTE camera matrix for the submenu's 3D scene (rotating rune/cross backdrop + model) | Same as A1 — 3D scene is stood up |
| A3 | `_printFixedWidthFontChar` | `native/platform/battle_asm.c` | Draws nothing; returns a plausible pen x-advance so layout still flows | Emits a textured fixed-width glyph sprite | A screen that uses fixed-width text is brought up and can be diffed |
| A4 | INFORMATION box TEXT | **DONE** — `func_800CB83C` decompiled (`battle_textbox.c`) + text-box OT wired | Renders "Choose items to equip." (matches retail) | — | Complete |
| A4b | INFORMATION box FRAME | `func_800CD158` no-op (`battle_textbox.c`) | Text renders on plain background; no shaded backing bar | Shaded/bordered bar behind the info text | Diff the 146-instr per-segment sprite build against DuckStation |
| A4c | Dialogue/menu box frame | `func_800CD3E4` no-op (`battle_textbox.c`) | Box states 2 & 7 draw no frame | Full dialogue/menu box border+gradient | A dialogue/menu-box screen is brought up (566-instr renderer) |
| A5 | Per-category item list | needs real inventory | Blanks out on in-submenu navigation | Lists the items in the selected category | Real inventory from a loaded `.mcd` (save-system work) |

**Screenshot cross-check (ITEMS submenu):** our render has the category icon
row, the OPTIONS/EQUIP/SETUP list + cursor, STATUS and MENU labels — all
correct. Missing vs. the real screen: **A1** (3D Ashley), **A2** (animated
rune backdrop), **A4** (the "Choose items to equip." info box).

---

## B. Audio stubs (whole subsystem — Phase 3)

SPU is uninitialised natively, so every sound entry point is a safe no-op.
Calling the real `vs_main_playSfxDefault` faults.

| Symbol | File | Restore when |
|---|---|---|
| `vs_battle_playMenuChangeSfx` | `native/platform/battle_asm.c` | Phase 3 (native SPU) |
| `vs_battle_playMenuSelectSfx` | `native/platform/battle_asm.c` | Phase 3 |
| `vs_battle_playMenuLeaveSfx` | `native/platform/battle_asm.c` | Phase 3 |
| `vs_battle_playInvalidSfx` | `native/platform/battle_asm.c` | Phase 3 |
| `vs_battle_playSfx10` | `native/platform/battle_asm.c` | Phase 3 |

---

## C. Behavioural shortcuts (return synthetic values)

Correct *for the synthetic harness state*, but not general reimplementations.

| Symbol | File | Native behaviour | Note |
|---|---|---|---|
| `func_800AAD4C` | `native/platform/battle_menu.c` | Returns -1 when the queried `D_800F4538` slot is null (always, synthetically) | This IS the real behaviour for a null slot; becomes a partial reimpl once slots are populated |
| CD queue completion | `native/tests/menu_harness.c` | Marks every queued `_cdQueue` slot `Loaded` each frame | Overlays are statically linked natively, so "loads" just need to signal complete |
| `vs_overlay_wait` / `vs_overlay_getSp` | `native/platform/battle_asm.c` | No-op / returns a stack address | Overlay system is moot when everything is statically linked |
| Heap base | `native/tests/menu_harness.c` | `vs_main_initHeap` fed a native `heap_buf[0xF2000]` instead of PS1 addr `0x8010C000` | Correct; the literal PS1 address just isn't mapped natively |

---

## D. Faithful native decomps (NOT deviations — listed for provenance)

Reimplemented in native-functional C (PSX keeps the asm). These render/behave
correctly; noted so they're not mistaken for stubs.

| Symbol | File | Source of truth |
|---|---|---|
| `func_800C930C` | `native/platform/battle_menu.c` | `5BF94/func_800C930C.s` (main-menu tab handler) |
| `vs_battle_renderTextRawColor` | `native/platform/battle_asm.c` | `5BF94/vs_battle_renderTextRawColor.s` (var-width font string renderer) |
| `vs_battle_setSprite` / `setSpriteDefault` / `setSpriteDefaultTexPage` | `native/platform/battle_asm.c` | `573B8/*.s` (compound texpage+SPRT builders) |
| `vs_battle_memcpy` / `copyAligned` | `native/platform/battle_asm.c` | `573B8/*.s` |

---

## E. Auto-generated abort stubs (the "not yet reached" frontier)

`native/platform/stubs.c` currently holds **371** function stubs that `abort()`
with `UNIMPLEMENTED: <name>` if called. These are *not* silent — hitting one is
how the demand-driven bring-up finds the next function to implement. This file
is regenerated by `native/tools/gen_stubs.py`; the count shrinks as real code
lands. It is the authoritative list of what has NOT been touched yet.

To see them: `grep UNIMPLEMENTED native/platform/stubs.c` (or just run the
harness — the next one aborts by name).

---

## How to keep this current

- Adding a native no-op / no-draw stub / synthetic shortcut → add a row here.
- Replacing one with a faithful decomp → move it to section D (or delete the row).
- The running frontier (section E) is automatic — no manual upkeep.
