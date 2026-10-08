# Native-build deviations ledger

## Current Status And Prerequisites

Status updated on 2026-10-05: decompilation is unfinished, Linux is reference
only, Windows has two accepted function-level matches beyond that reference
(no unit or overlay match), and native-port implementation has not started as
an accepted deliverable.
The sections below record legacy experiments. Their "DONE", "Complete",
"21/21", and runtime claims are historical, not current Windows acceptance.
This ledger is not an exhaustive fidelity certificate.

The following work remains explicit; it has not been implemented by the status
cleanup or by making the symbol generator stricter:

| Prerequisite | Current Gap | Evidence Required Before Acceptance |
| --- | --- | --- |
| Matching decompilation | Two accepted function-level matches; checker controls pass at function scope; no unit or overlay match | Continued per-function canonical matches, then source-unit and overlay comparisons under the decomp rules |
| Game entrypoints and overlays | Battle entry, excluded menu controllers, music routines and overlay return/jump paths are incomplete | Real verified implementations with end-to-end control-flow tests; no callable data arrays or implicit function aborts |
| Data storage and aliases | The old generator invented 4 KB blocks without type, bounds, initialization or alias provenance | Correctly sized typed data, verified symbol ownership/offsets, initialization and consumer boundary tests |
| Cards and persistence | Creation flags, BIOS rename semantics, formatting, coherent raw/filesystem writes and durable error reporting are incomplete; native packaging removes live-state capture | Disposable-card tests for create/load/save/overwrite/rename/format, full-image readback, failure paths, cache coherence and current-state capture |
| GTE/GPU | Only selected GTE modes and GPU commands work; other modes/state, blending and draw paths are unsupported or approximated | Unsupported paths fail loudly; supported operations and render workflows pass appropriate differential checks before a fidelity claim |
| Audio/video | Music/IRQ paths, noise, reverb, modulation, interpolation and pixel-exact decode are incomplete or approximate | Complete sequencing/transfer tests and source-bound audio/video comparisons, not merely linked symbols or macroblock counts |
| Native integration | Legacy screenshots, watchdog exits and isolated host tests do not demonstrate a game | Accepted source first, then complete tested vertical slices with explicit behavior and data contracts |

Normal symbol generation now rejects unresolved functions and data. It never
emits generic data buffers. Explicit `--allow-function-stubs` permits known
function aborts for diagnostics only; unknown data still fails. Old generated
sources/executables containing guessed buffers are stale research artifacts,
not accepted builds and not safe substitutes for the new gate.

No game, card, or hardware implementation is being silently substituted for
these requirements. Keep original assets and saves untouched during bring-up.

## Historical Experiment Ledger

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
| A1 | `_renderAshley` | `src/MENU/_renderAshley.h:29` (`#ifdef NATIVE_BUILD` early return) | Skips the 3D character portrait entirely | Draws Ashley + equipped weapons as a rotating 3D model (GTE-transformed, lit) | 3D model data slots (`D_800F4538`, now `vs_battle_actorModels`) are populated — needs the model loader + a real save/inventory |
| A2 | `func_8007ACB0` | `native/platform/battle_asm.c` | No-op | Builds the GTE camera matrix for the submenu's 3D scene (rotating rune/cross backdrop + model) | Same as A1 — 3D scene is stood up |
| A3 | `_printFixedWidthFontChar` | `native/platform/battle_asm.c` | Draws nothing; returns a plausible pen x-advance so layout still flows | Emits a textured fixed-width glyph sprite | A screen that uses fixed-width text is brought up and can be diffed |
| A4 | INFORMATION box TEXT | **DONE** — `func_800CB83C` (now `vs_battle_renderTextBoxes`) decompiled (`battle_textbox.c`) + text-box OT wired | Renders "Choose items to equip." (matches retail) | — | Complete |
| A4b | INFORMATION box FRAME | **DONE** — `func_800CD158` decompiled (`battle_textbox.c`) | Renders the shaded backing bar (semi-transparent, visible over a non-black background) | — | Complete |
| A4c | Dialogue/menu box frame | `func_800CD3E4` (now `vs_battle_renderDialogBalloon`) no-op (`battle_textbox.c`) | Box states 2 & 7 draw no frame | Full dialogue/menu box border+gradient | A dialogue/menu-box screen is brought up (566-instr renderer) |
| A5 | Per-category item list | **DONE** | Renders the real loaded inventory (e.g. WEAPONS 3/8: Fandango, Tovarisch, Seventh Heaven) | — | Complete — needed the save load (`.mcd`) + item-name text (`load_item_text` reads ITEMNAME/ITEMHELP/MENU12.BIN) |
| A6 | Intro FMV | **DONE** — software MDEC (`native/platform/mdec.c`, `libpress.c`, `libstream.c`) plus CD-XA audio (`cdxa.c`) | Plays the opening MDEC video stream (`TITLE.STR`), all 593 frames, **with its soundtrack** | — | Complete. **The `src/` tree now has no `NATIVE_BUILD` edits at all** — the two early returns in `22C.c` are gone |
| A7 | Title background / menu items | **NOT A DEFECT** — renders correctly | Full title art, logo, rune backdrop, and the SOUND / NEW GAME / CONTINUE items | — | Was a false alarm: every VRAM snapshot had been taken during the ~12s publisher/developer sequence that precedes the title. `_copyTitleBgData` uploads real data (`LoadImage (480,0 544x512)` checksum `2C23F9CD`). **Boot takes ~17s of wall time before the title appears** — two 364-frame screens, plus loading |

**Screenshot cross-check (ITEMS submenu):** our render has the category icon
row, the OPTIONS/EQUIP/SETUP list + cursor, STATUS and MENU labels — all
correct. Missing vs. the real screen: **A1** (3D Ashley), **A2** (animated
rune backdrop), **A4** (the "Choose items to equip." info box).

---

## B. Audio (Phase 3b: the port has sound)

**The port plays through the host's sound card.** SFX come from the game's own
Akao driver over the software SPU, and the intro movie's soundtrack comes from
its interleaved CD-XA track. A full boot with scripted input at the title:

```
$ VS_AUDIO_WAV=/tmp/a.wav VS_SND_TRACE=1 \
  VS_PAD_SCRIPT=2400:0x0800,2520:0x1000,2600:0x4000,2700:0x0020 \
  ./native/build/vagrant-native
[audio] 44100Hz stereo via SDL "pulseaudio"
[cdxa] 190 sectors, 446885 frames, ring 2352
[snd] cmd 20  00000004 00000000 00000080 0000007F
[spu] key-on v22 start=077B0 repeat=077D0 pitch=1BB6 vol=0273/0535 adsr=5FC700FF
$ # /tmp/a.wav: 95s, peak 23883, 31% non-silent
```

Output is on by default and needs nothing configured; `VS_NO_AUDIO=1` turns it
off. `VS_AUDIO_WAV` still captures the same mix to a file, and the two work
together.

Much of `native/platform/libspu.c` is still inert-but-present: the boot sequence
calls it, so it has to exist, but most of it does nothing. Two entry points there
are load-bearing. `SpuWrite` **must** invoke the callback
registered by `SpuSetTransferCallback`, because the sound driver sets
`_isSpuTransfer = 1` and then spins in `_waitTransferAvailable()` until the
completion callback clears it. A no-op there hangs the boot before the first
frame. `SpuGetVoiceEnvelope` is the other: the driver reclaims any voice whose
envelope reads back as 0, so a stub returning 0 kills every voice on the tick
after it started.

**Status:** the SPU itself now exists (`native/platform/spu_core.c`): 24 ADPCM
voices, ADSR, pitch and volume, mixing to 44.1kHz stereo. It works by *reading*
the register block the Akao driver writes — the driver is decompiled C
(`src/SLUS_010.40/25AC.c`, ~5300 lines, 17 `INCLUDE_ASM`) and already runs
natively, so nothing above the hardware needed changing.

Getting from "the SPU exists" to "you can hear it" took eight separate fixes,
every one of which had to be right before any of them could be observed:

1. **The instrument bank uploads.** `func_800135D8` is implemented in
   `sound_asm.c` (transcribed from `25AC/func_800135D8.s`). SPU RAM went from
   **64 bytes** (just `StartSound`'s flush buffer) to **204,096 bytes**.
2. **The sequencer ticks.** `StartSound` arms root counter 2 at `0x44E8`, which
   is exactly 240Hz, and registers `func_80019A58` as an `EvMdINTR` handler.
   The kernel could read counters but never delivered their interrupt, so the
   Akao tick had never once run. `psx_rcnt_pump()` (kernel.c) now raises it from
   VSync, verified with `VS_RCNT_TRACE=1`.
3. **The driver's front door was a no-op.** Every sound request in the game
   arrives at `func_80018C30` as a numbered command; it was stubbed in
   `battle_asm.c`, so nothing behind it was ever called. It is now transcribed
   in `sound_asm.c`, together with the rest of the SFX path that was still
   assembly (see section D).
4. **Three dispatch tables were arrays of zeroes.** `D_800301E0` (driver
   commands), `D_800305E0` and `D_80030760` (sound-VM opcodes) hold PSX code
   addresses in `.data`; the stub generator, seeing data symbols nobody
   defines, emitted zero blobs, so the first dispatch jumped to address 0. They
   are rebuilt as real C in `native/platform/sound_tables.c`, along with
   `g_Sound_LfoTable`, which is sixteen pointers to waveform tables.
5. **`.bss` structs the C code aliases were split into unrelated blobs.** See
   section F below. That is the one worth reading.

`VS_SND_TRACE=1` prints every command that reaches the front door and dumps the
register state of every voice at key-on. It is the first thing to reach for when
the game is silent, because it separates "the game never asked for a sound" from
"the driver was asked and did nothing".

Three more things were needed to get from "a voice keys on" to "you can hear
it", and each one is a class of mistake worth recognising again:

6. **The mix was gated on `VS_AUDIO_WAV`.** `spu_render` is what consumes the
   key-on register, so an ordinary run was not merely silent, it was inert: no
   voice ever started and the counters read zero, which looks exactly like a
   broken driver.
7. **The SPU was rendering per `VSync` call, not per unit of time.** The game
   does not call VSync 60 times a second -- during the intro it calls it once
   per movie frame, about fifteen. A fixed 735 frames per call therefore
   produced audio at a quarter of real time. This is the same mistake the vblank
   counter once had, in a different place.
8. **The volume registers were decoded wrong, twice.** Voice and main volume
   hold *half* the level in a signed 15-bit field, so the effective value is
   double the register; returning the raw field cost 6dB per multiply, 12dB
   across the mix. Worse, the CD input registers are not in that format at all
   -- they are plain signed 16-bit -- and reading the 0x7FFF the driver writes
   for "full" as a 15-bit field gives -1. That is why the movie's soundtrack
   first came out at a peak amplitude of exactly 1.

**Still not done:** music. No run has ever driven it, and that is structural
rather than a driver problem: nothing on the boot path loads a `MUSIC0xx.DAT`
(`_musicLBAs[]` in main.c indexes them by LBA), and the one place that starts
music from a menu, `src/MENU/MENUF.PRG/3B8.c`, is in `NATIVE_EXCLUDE`. A
95-second boot with scripted title input issues commands 0x80 and 0x20 only:
never 0x10/0x12/0x16/0x19/0x1A. Those are all implemented and dispatch
correctly, but until something asks for music they cannot be exercised.

| Symbol | File | Restore when |
|---|---|---|
| `SpuSetVoiceAttr`, reverb, IRQ | `native/platform/libspu.c` | Phase 3b. Reverb, noise voices and pitch/amplitude modulation are requested through registers `spu_core.c` ignores; interpolation is linear, not the hardware's four-tap Gaussian |
| `SpuSetReverbModeDepth`, `SpuSetNoiseClock` | `native/platform/libspu.c` | Accept their arguments and drop them, because `spu_core.c` models neither reverb nor noise. `func_80015220` calls both every time the driver raises the matching update flag |
| Audio output | `native/platform/audio_sink.c` | **DONE.** SDL2 is `dlopen`'d independently of present.c, so a headless run still has sound, and `SDL_QueueAudio` is pushed from the game's own thread once per pump rather than using SDL's callback thread (nothing else here is threaded). The queue is held between 25ms and 200ms. `VS_NO_AUDIO=1` disables it. Under WSL, WSLg's PulseAudio socket carries it to the Windows device with no configuration, given 32-bit libpulse and libSDL2 |
| `SpuGetVoiceEnvelope` | `native/platform/libspu.c` | **DONE.** Reads the voice's `ADSR_VOL` register, which `spu_core` keeps current as it mixes. Had to be real: the driver reclaims any voice whose envelope reads 0, so a stub returning 0 would kill every voice on the tick after it started |
| XA audio in `TITLE.STR` | `native/platform/cdxa.c` | **DONE.** 1402 of the 11216 sectors are CD-XA ADPCM (submode `0x64`, coding `0x01` = stereo/37800Hz/4-bit, all on channel 1 so no filter is needed). Decoded behind libds, not libstream, because that is where the hardware does it: the drive decodes matching sectors itself and feeds the SPU's CD input, which is why the game's own `UpdateCdVolume` fades apply to the soundtrack with no special-casing. The sound-group header layout was established by measuring which of the 16 header bytes are always equal across all 25,236 groups in the real file, not from a remembered spec |
| `func_800135D8` | `native/platform/sound_asm.c` | **DONE** natively. Uploads an Akao instrument bank to SPU RAM; verified by SPU RAM going 64 -> 204,096 bytes. Three callees in 25AC.c are file-static and are restated in the native file; `Sound_CopyAndRelocateInstruments` and `g_InstrumentInfo` are global, so the real driver code does the relocation. Still `INCLUDE_ASM` for the PSX build: a matching decomp reached 49/49 instructions with the right structure and failed only on register allocation, which is permuter work |
| `CdMix`, `SpuSetCommonCDMix` | `native/platform/libds.c`, `libspu.c` | Muting the CD input is not modelled. Nothing in the game calls either; the volume path it would gate works, through the CD volume registers |
| `vs_battle_play*Sfx` (five) | `native/platform/battle_asm.c` | **DONE.** Restored to the real `vs_main_playSfxDefault(0x7E, id)` calls the PSX build's hand-written assembly makes (ids 4, 5, 6, 7, 10). Confirmed live: title-screen navigation now issues `cmd 20` with ids 4 and 5 |

---

## C. Behavioural shortcuts (return synthetic values)

Correct *for the synthetic harness state*, but not general reimplementations.

| Symbol | File | Native behaviour | Note |
|---|---|---|---|
| `func_800AAD4C` | `native/platform/battle_menu.c` | Returns -1 when the queried `D_800F4538` (now `vs_battle_actorModels`) slot is null (always, synthetically) | This IS the real behaviour for a null slot; becomes a partial reimpl once slots are populated |
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
| `func_80018C30` | `native/platform/sound_asm.c` | `25AC/func_80018C30.s`. The driver's command front door: marshals arguments into `D_80037790` and dispatches through `D_800301E0` |
| `func_8001653C` | `native/platform/sound_asm.c` | `25AC/func_8001653C.s`. Arms one SFX voice channel from an Akao program |
| `func_800161C4` | `native/platform/sound_asm.c` | `25AC/func_800161C4.s`. Releases voices by owner mask, by channel index, or by priority (the voice stealer) |
| `func_80019FC4` | `native/platform/sound_asm.c` | `25AC/func_80019FC4.s`. Scans ahead in a channel's bytecode for the next note, following jumps/loops/calls without executing |
| `func_80015220` | `native/platform/sound_asm.c` | `25AC/func_80015220.s`. The per-tick flush: works out which voices to start, pushes their parameters to the SPU, and issues the key-on |
| `func_80015080` | `native/platform/sound_asm.c` | `25AC/func_80015080.s`. Reaps voices whose envelope has run out |
| `func_80015970` | `native/platform/sound_asm.c` | `25AC/func_80015970.s`. Collapses a channel mask to a voice mask, for reverb/noise/FM |
| `func_800147CC` | `native/platform/sound_asm.c` | `25AC/func_800147CC.s`. Per-channel vibrato, tremolo, auto-pan, then the SPU volume and sample rate. Runs for every keyed channel every tick |
| `D_800301E0`, `D_800305E0`, `D_80030760`, `g_Sound_LfoTable` | `native/platform/sound_tables.c` | `1FD44.data.s`. The three dispatch tables and the LFO waveform pointer array, transcribed as real C because the retail bytes are PSX addresses |
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

## F. The generated data layer, and the two ways it lies

Section E's stubs are loud: they abort with a name. The *data* half of the stub
generator is the opposite, and it is where the hard bugs have come from. Both
failure modes below are silent, produce plausible-looking behaviour, and are
worth checking whenever a subsystem "runs but does nothing".

### F1. Split `.bss` labels break struct aliasing

splat starts a new `dlabel` at every address something references. A struct that
the C code treats as one object therefore arrives from the disassembly as
several independent symbols, and `gen_stubs.py`, which knows only that they are
data symbols nobody defines, backs each with its own zero blob. Code reaching a
field through the struct and code reaching the same field through the split
label then land in unrelated memory.

The instance that cost the most to find: `func_80019A58` decides whether to
flush pending voice state to the SPU with

```c
if ((D_80039B00 | ...->ActiveNoteMask | D_80037898) != 0) func_80015220();
```

where `D_80039B00` *is* `g_Sound_GlobalFlags.UpdateFlags` and `D_80037898` *is*
`g_Sound_VoiceSchedulerState.KeyedFlags`. The sound VM sets both fields through
the structs. The test read two unrelated blobs that were always zero, so the
flush never ran and not one voice ever keyed on, with no error anywhere.

The fix is to define the container once and equate each split label to
base + its true offset. `native/platform/sound_asm.c` does this with an `ALIAS`
macro for the whole sound driver (scheduler state, global flags, cutscene stream
state, the music configs, and `SfxSoundChannels`, which is the tail of
`D_80035910` at +0xAA0 and which the driver walks *backwards* into).

To find more: compare the `.bss` addresses in `build/src/**/*.bss.s` against the
struct sizes in `src/include/*.h` and look for a symbol whose address falls
inside another symbol's extent. Everything found this way in the sound driver is
fixed; other subsystems have not been audited.

### F2. Initialised `.data` harvested to zero, or from the wrong offset

A data symbol that no decompiled C defines gets a zero blob even when the retail
binary has real bytes there. For the sound driver that meant every lookup table
(operand lengths, note lengths, pitch and envelope curves, the pan law) read
as zeroes. `SOUND_HARVEST` in `Makefile.native` now copies them out of the
retail image; the audit that found them is a set comparison between the
`unsigned char X[4096]` definitions in `stubs.c` and the `dlabel`s in the
`*.data.s` / `*.rodata.s` listings.

Worse, `harvest_data.py` had the file offset wrong for `SLUS_010.40`. The
overlays are raw blobs that load at file offset 0, but the main executable is a
PS-EXE whose code segment starts 0x800 bytes in, past the header. Its
`splat.yaml` says `start: 0x800`, and the tool was ignoring it. Every
main-executable symbol was harvested 0x800 bytes early. That is not a crash: it
is a table full of the wrong numbers. It surfaced as the semitone pitch table
`D_8002F63C` having 0 as its first entry, so every note played at pitch 0: a
voice keying on correctly, mixing correctly, and emitting silence. Fixed in
`overlays()`; anything harvested from the main executable before that fix was
wrong.

**Rule of thumb:** a harvested table should be eyeballed once. The semitone
table reads `0x1000, 0x10F3, 0x11F5, ...`, obviously a ratio table. If a
harvested array does not look like what its name says it is, suspect the offset
before suspecting the code that reads it.

---

## How to keep this current

- Adding a native no-op / no-draw stub / synthetic shortcut → add a row here.
- Replacing one with a faithful decomp → move it to section D (or delete the row).
- The running frontier (section E) is automatic — no manual upkeep.
