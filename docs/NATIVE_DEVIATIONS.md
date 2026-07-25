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
| A4b | INFORMATION box FRAME | **DONE** — `func_800CD158` decompiled (`battle_textbox.c`) | Renders the shaded backing bar (semi-transparent, visible over a non-black background) | — | Complete |
| A4c | Dialogue/menu box frame | `func_800CD3E4` no-op (`battle_textbox.c`) | Box states 2 & 7 draw no frame | Full dialogue/menu box border+gradient | A dialogue/menu-box screen is brought up (566-instr renderer) |
| A5 | Per-category item list | **DONE** | Renders the real loaded inventory (e.g. WEAPONS 3/8: Fandango, Tovarisch, Seventh Heaven) | — | Complete — needed the save load (`.mcd`) + item-name text (`load_item_text` reads ITEMNAME/ITEMHELP/MENU12.BIN) |
| A6 | Intro FMV | **DONE** — software MDEC (`native/platform/mdec.c`, `libpress.c`, `libstream.c`) | Plays the opening MDEC video stream (`TITLE.STR`), all 593 frames | — | Complete. **The `src/` tree now has no `NATIVE_BUILD` edits at all** — the two early returns in `22C.c` are gone. Video only: the interleaved XA audio track is still discarded (see B) |
| A7 | Title background / menu items | **NOT A DEFECT** — renders correctly | Full title art, logo, rune backdrop, and the SOUND / NEW GAME / CONTINUE items | — | Was a false alarm: every VRAM snapshot had been taken during the ~12s publisher/developer sequence that precedes the title. `_copyTitleBgData` uploads real data (`LoadImage (480,0 544x512)` checksum `2C23F9CD`). **Boot takes ~17s of wall time before the title appears** — two 364-frame screens, plus loading |

**Screenshot cross-check (ITEMS submenu):** our render has the category icon
row, the OPTIONS/EQUIP/SETUP list + cursor, STATUS and MENU labels — all
correct. Missing vs. the real screen: **A1** (3D Ashley), **A2** (animated
rune backdrop), **A4** (the "Choose items to equip." info box).

---

## B. Audio stubs (whole subsystem — Phase 3)

SPU is uninitialised natively, so every sound entry point is a safe no-op.
Calling the real `vs_main_playSfxDefault` faults.

The whole of `native/platform/libspu.c` is inert-but-present: the boot sequence
calls it, so it must exist, but it makes no sound. One entry point there is
load-bearing rather than cosmetic — `SpuWrite` **must** invoke the callback
registered by `SpuSetTransferCallback`, because the sound driver sets
`_isSpuTransfer = 1` and then spins in `_waitTransferAvailable()` until the
completion callback clears it. A no-op there hangs the boot before the first
frame.

**Status:** the SPU itself now exists (`native/platform/spu_core.c`): 24 ADPCM
voices, ADSR, pitch and volume, mixing to 44.1kHz stereo. It works by *reading*
the register block the Akao driver writes — the driver is decompiled C
(`src/SLUS_010.40/25AC.c`, ~5300 lines, 17 `INCLUDE_ASM`) and already runs
natively, so nothing above the hardware needed changing.

Two of the three things it needed are now done, and the state is measured with
`VS_AUDIO_WAV=<path>` plus the counters it prints:

1. **The instrument bank uploads.** `func_800135D8` is implemented in
   `sound_asm.c` (transcribed from `25AC/func_800135D8.s`). SPU RAM went from
   **64 bytes** (just `StartSound`'s flush buffer) to **204,096 bytes**.
2. **The sequencer ticks.** `StartSound` arms root counter 2 at `0x44E8`, which
   is exactly 240Hz, and registers `func_80019A58` as an `EvMdINTR` handler.
   The kernel could read counters but never delivered their interrupt, so the
   Akao tick had never once run. `psx_rcnt_pump()` (kernel.c) now raises it from
   VSync, verified with `VS_RCNT_TRACE=1`.
3. **Still 0 key-ons.** Neither the title sequence nor menu SFX reach the SPU,
   across runs of 80s with scripted input. So the driver has instruments and a
   clock, and nothing asks it to play.

**Next place to look:** the path from `vs_main_playSfxDefault` (main.c:9877) ->
`vs_main_playSfx` down into the driver, and whether an SFX/sequence bank is ever
loaded. Music files are separate: `_musicLBAs[]` in main.c indexes
`MUSIC000.DAT` and friends by LBA, so something has to load one and start it.
Check whether that load ever happens on the title path before assuming the
driver is at fault.

| Symbol | File | Restore when |
|---|---|---|
| `SpuSetVoiceAttr`, reverb, IRQ | `native/platform/libspu.c` | Phase 3b. Reverb, noise voices and pitch/amplitude modulation are requested through registers `spu_core.c` ignores; interpolation is linear, not the hardware's four-tap Gaussian |
| Audio output | `native/platform/libspu.c` | **No sound card yet** — the mix is only written to a WAV. Needs an SDL audio sink (SDL2 is already `dlopen`'d by present.c) |
| XA audio in `TITLE.STR` | `native/platform/libstream.c` | Phase 3b. The intro's sectors interleave video with CD-XA ADPCM audio (1401 of 11216 sectors, submode `0x64`). The streaming layer drops anything that is not an STR video chunk, so the intro plays silent |
| `func_800135D8` | `native/platform/sound_asm.c` | **DONE** natively. Uploads an Akao instrument bank to SPU RAM; verified by SPU RAM going 64 -> 204,096 bytes. Three callees in 25AC.c are file-static and are restated in the native file; `Sound_CopyAndRelocateInstruments` and `g_InstrumentInfo` are global, so the real driver code does the relocation. Still `INCLUDE_ASM` for the PSX build: a matching decomp reached 49/49 instructions with the right structure and failed only on register allocation, which is permuter work |
| `CdMix` | `native/platform/libds.c` | Phase 3b — CD-audio volume routing |
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
| `PadSetAct` / `PadSetActAlign` | `native/platform/libpad.c` | Rumble commands accepted and ignored; `PadSetActAlign` returns success | Restore when host haptics are wired (the DualSense supports them) |
| `PadInfoMode` / `PadGetState` | `native/platform/libpad.c` | Port 0 always reports a connected analog pad (id `0x73`, mode 7); port 1 always empty; `PadInitMtap` falls through to `PadInitDirect` | Single-controller assumption. Restore for multitap / hot-plug reporting |
| `DecDCTin` / `DecDCTout` | `native/platform/libpress.c` | Decode is synchronous; the DMA completion callback is queued and run from a trampoline instead of an interrupt | Observably identical — one transfer finishes, its callback runs and starts the next. Direct recursion is what would be wrong: the callback issues the next `DecDCTout` *before* uploading its strip, so recursing runs every transfer before any upload and the game's two decode buffers end up holding only the last two strips |
| Frame assembly | `native/platform/libstream.c` | Each frame is gathered into one contiguous span of the caller's ring, rather than left as ring sectors with a header every 2016 bytes | The game only uses `StGetNext`'s `addr` as a frame pointer and hands it back to `StFreeRing`, so nothing above can tell. Restore only if something starts walking the ring itself |

### Known issues (found by playing the windowed build)

| # | Issue | Detail |
|---|---|---|
| C1 | Closing the menu ends the session | Triangle, or Cross at the top level, closes the menu (`currentState` `00→1F→7F→3F`). Correct behaviour — it hands back to the game world — but no world exists, so the harness exits cleanly with a message instead of showing a black window. **Auto-reopen investigated and not shipped:** it works at the state level (needs `func_800CACD0`'s flag resets *and* the synthetic buffers re-wired first, or it takes the faulting `menuItems == NULL` heap path), but ~20 frames later the reopened menu corrupts the C heap and kills the process inside SDL. Root cause is the menu's close/rebuild path against synthetic state. |
| C2 | BREAK ARTS submenu can't be exited | With a save that has no break arts unlocked, backing out of the submenu appears to loop; only Triangle (which closes the whole menu) escapes. Not yet diagnosed — likely an empty list with a cancel handler gated on a valid selection. |

---

## D. Faithful native decomps (NOT deviations — listed for provenance)

Reimplemented in native-functional C (PSX keeps the asm). These render/behave
correctly; noted so they're not mistaken for stubs.

| Symbol | File | Source of truth |
|---|---|---|
| `func_8001369C` | `native/platform/sound_asm.c` | `25AC/func_8001369C.s` (167-instruction sound-driver reset; on the boot path via `StartSound`) |
| `func_800C930C` | `native/platform/battle_menu.c` | `5BF94/func_800C930C.s` (main-menu tab handler) |
| `vs_battle_renderTextRawColor` | `native/platform/battle_asm.c` | `5BF94/vs_battle_renderTextRawColor.s` (var-width font string renderer) |
| `vs_battle_setSprite` / `setSpriteDefault` / `setSpriteDefaultTexPage` | `native/platform/battle_asm.c` | `573B8/*.s` (compound texpage+SPRT builders) |
| `vs_battle_memcpy` / `copyAligned` | `native/platform/battle_asm.c` | `573B8/*.s` |

---

## E. Auto-generated abort stubs (the "not yet reached" frontier)

`native/platform/stubs.c` currently holds **313** function stubs that `abort()`
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
