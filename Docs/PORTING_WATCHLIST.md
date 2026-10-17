# Porting watchlist

Things to check for during the copy, compile and modernization work. Each entry says what to look for, why it
matters, and how it is meant to be handled. Owner decisions are dated.

## 1. Module reload resets statics (check during development)

On the PS1, menus, battle, title, ending and effect plugins are separate modules (overlays) loaded into shared
memory slots on demand. In the native build they are all linked into one program, so their statics keep their
values for the whole run.

- **Statics with an initial value are reset on every reload**, because the module's data is read from disc again.
  Example: `Menu::Framework::QuitDelay` (`MAINMENU.PRG/C48.c:305`) does
  `static int delay = 0; int ret = ++delay; ... return ret == 5;`. Nothing in code resets `delay`; it only works
  because the module is reloaded before the next quit.
- **Zero-initialised statics are not cleared.** `Menu::Framework::LoadMenuOverlay` (`vs_battle_loadMenuPrg`)
  only reads the file into the slot. The original code must write them before reading them; a read-before-write is
  a latent original bug, not something to reproduce.
- Scale: 1,296 `state` globals live in reloadable modules (MENU 775, BATTLE 356, TITLE 76, EFFECT 47, ENDING 32).
  285 of them have an initial value (MENU 274, TITLE 8).

**How:** the copy tool groups each module's statics (with their declared initial values) into one state struct per
module and generates `<Module>::ResetOnLoad()`. Boot calls it at the points where the original loads that module
(menu overlay load, title/battle/ending entry, effect plugin load). During development, any bug that shows up
"the second time a screen opens" should first be checked against this list.

## 2. 64-bit and layout assumptions (handled during modernization)

The copied code compiles for x64, but some of it assumes 32-bit pointers or fixed struct sizes:

- 286 pointer/integer casts in 151 functions;
- literal sizes for structs that contain pointers (`memset(s1, 0, 0x178)`, `vs_main_memcpy(..., 0x7800)`,
  `vs_main_allocHeapR(0x178)`). On x64 those structs are larger.
- On-disc data whose pointers are fixed up in place after loading (e.g. `Content::CopyRelocatedWeaponSequence`,
  `BATTLE.PRG/30DB0.c:1608`; types `Actors::ShpModel`, `Content::SequenceBuffer`, `Field::RoomEffectCommand`,
  `Field::MpdSectionE`). The plan forbids raw struct I/O, so these become decoders.

**How (owner, 2026-10-17):** fixed as part of modernizing each function, not as a separate pass. Phase 1a only needs
it to compile. Keep the clang casting warnings (`-Wpointer-to-int-cast`, `-Wint-to-pointer-cast`,
`-Wshorten-64-to-32`) visible and counted for copied `.c`, and make a size literal next to a struct an assert
(`static_assert(sizeof(T) == 0x178)`) so the first run on x64 points at it.

## 3. Texture and palette references by video-memory coordinates (AI rewrite tool)

The game addresses textures and palettes by PS1 video-memory coordinates (`getTPage` ×49, `setClut`, `LoadImage`
×28, `ClearImage` ×11), reads video memory back (`StoreImage` ×8, `MoveImage` ×2), and animates palettes by
rewriting them (the CLUT transform code in `SLUS_010.40/main.c`). Plan §6.2 already gives `Engine\Render` a
logical texture-page lookup (rect → texture + palette, not emulated memory) and a capture API, which is enough to
make copied code run.

**How (owner, 2026-10-17):** the real rewrite to modern texture/material handles is not done by hand. It is done by
AI at high speed through a **rewrite tool** that loops until the work is approved:

1. Job = one function (or one call graph cluster) plus its ledger names, the rules, and the target API.
2. The AI rewrites it.
3. Automatic checks: it compiles; the boundary lint passes (no video-memory coordinates, no PS1 GPU calls);
   unit tests and golden captures pass where they exist; the shim use count does not go up.
4. A second AI reviews the diff against the original and the rules, and either approves it or returns findings.
5. Repeat 2–4 until approved, then mark the catalog row `modernized` and commit.

The same tool serves items 2, 5 (sequencer call sites), 6 and 7. It is worth building early in 1b.

## 4. Audio: proposed modern approach (for owner review)

The asset pipeline already keeps music sequenced (ASSET_PIPELINE.md §6.5: samples to WAV, AKAO streams to event
JSON) and plan §6.3 already avoids sound-chip registers. The open question is what plays the notes. Proposal:

- **Instruments as SFZ.** The extractor writes each instrument bank as an SFZ file (plain text: sample, key range,
  root key, tuning, loop points, envelope, volume, pan) next to the decoded WAVs, instead of a custom JSON schema.
  SFZ is a standard sampler format, so any DAW or SFZ player can audition, remix or replace the instruments.
- **Synthesis by sfizz** (BSD-2-licensed C++ SFZ sampler), embedded in `Engine\Audio`. It provides high-quality
  resampling, envelopes, filters, loops, and MIDI-style controls. We don't write or maintain a voice engine.
- **The game's sequencer stays ours.** The AKAO interpreter is ported from the decomp (`Audio::Seq`) because it is
  game logic: tempo changes, volume slides, battle music transitions and SFX priorities. It reads the event JSON
  and drives sfizz with standard events: note on/off, program change, volume (CC7), pan (CC10), reverb send
  (CC91), pitch bend for slides, and LFO depth for vibrato.
- **Mixing and output** through SDL3's audio stream (already a dependency), with a float mix bus at 48 kHz.
- **Reverb:** the PS1 had a fixed set of reverb presets. Two options:
  - a modern algorithmic reverb tuned per preset (default);
  - convolution with impulse responses that the extractor computes from the documented reverb algorithm (parity
    mode). No hardware model runs at runtime either way.
- **Parity switches:** optional PS1-style 4-tap interpolation and envelope curves, for side-by-side comparison in
  1b. Phase 2 defaults to the high-quality settings.
- **Modding for free:** music and instruments become standard MIDI-like events plus SFZ+WAV, so new songs and
  replacement instruments need no custom tools. ASSET_PIPELINE's round-trip rule still applies
  (decode → event JSON + SFZ → encode == original bytes).

Risk to check first: whether SFZ envelopes can match the PS1 envelope curves closely enough. If not, sfizz still
handles playback and resampling, and our envelope replaces its built-in one for parity mode.

## 5. Assembly-only and inline-assembly functions

215 functions exist only as assembly (Battle 112, Core 33, Render 31, Effects 20). Another 49 functions use inline
assembly, mostly the 3D math coprocessor macros.

**How (owner, 2026-10-17):** they are replaced over time, through the rewrite tool (item 3), hopefully within about
a month. Anything that is only hardware interaction is not ported; it is replaced as part of modernization. To keep
the 1a compile gate reachable meanwhile, an unported function compiles as a stub that calls
`VS_UNPORTED("<catalog name>")` (`Core/Unported.hpp`). The log shows which stubs a slice actually reaches, which
sets the order.

Before stubbing, the copy tool follows `dup-of:` in the catalog notes. An assembly row whose bytes equal a function
that does exist as C uses that C code instead of a stub. Example: PLG054/055 `func_800FA368` (F01927, F01929) are
byte-identical to PLG053's, which is the closed-ring effect template instance F01925, so they become effects-catalog
rows of that template.

## 6. Scratchpad memory

1,061 uses in 266 functions (Menu 81, Battle 52, Field 40, Effects 37) of the PS1's 1 KiB fast scratchpad, which the
code uses as shared temporary memory.

**How (owner, 2026-10-17):** removed during modernization: the values become locals or module state. For the copy
only, the copy tool maps `getScratchAddr(n)` and `vs_scratch` to a per-module scratch struct (or one `Core` scratch
area), so the code compiles and runs unchanged until each function is modernized.

## 7. Extractor (deferred)

**Owner, 2026-10-17:** `VS.Extract` stays C++. Its decoders will be ported from VS-Workshop's `Workshop.Core`
(`D:\Dev\Modding\VS-Workshop`, C#), which already decodes BIN/CUE, ISO9660, MPD/ZND/SHP/WEP/ZUD/SEQ/ESQ/TIM, palettes
and P-file effects. Workshop is still in development, so the port waits until it is further along. It is part of
Phase 1a step 5 (data disposition and asset extraction). AKAO, XA and STR are not in Workshop and are ours to write.
