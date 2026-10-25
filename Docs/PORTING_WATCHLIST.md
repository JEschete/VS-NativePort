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

**How:** the copy tool generates the reset. Initialised statics of each reloadable module (284 rows) are restored
from `static const` copies of their initial values; the 158 function-local ones are hoisted to file scope so the
reset reaches them. `Boot/OverlayReset.c` has one `Boot_Reset<Overlay>Statics()` per module (16); Boot calls it at
the points where the original loads that module (menu overlay load, title/battle/ending entry, effect plugin load).
That wiring is the first 1b task. Four statics are not reset because their initial value names something local to
the function: `Menu::Status::animateActorSwitchState` and `Menu::BreakArts::weaponCategoriesState` (local enum
values) and the two inventory sort tables (built from other local statics, never written). During development, any
bug that shows up "the second time a screen opens" should first be checked against this list.

Native session boundaries also invalidate borrowed scene UI before the next
title/battle initialization replaces the game heap. This covers all eight
textboxes, formatting-string pointers, selection/wait state and notification
storage; it does not free that storage individually or change ordinary in-scene
closing animations. The original interface initializer clears only textbox0
(`5BF94.c:3484`, matching800CAA9C), which is insufficient after an interrupted
native scene retains a higher-numbered textbox. Do not replace that lifetime
repair with a font null-pointer fallback or assume original zero-initialized
globals are reloaded.

## 2. 64-bit and layout assumptions (handled during modernization)

The copied code compiles for x64, but some of it assumes 32-bit pointers or fixed struct sizes:

- **Opaque views can hide pointer widening.** F00979's palette view omitted a
  gimmick model pointer and kept PS1 offsets300/332 while the native owner used
  304/336. Shifting source and destination together made part of the CPU array
  appear correct, but the CLUT upload used the wrong indices and missed the
  final two colors. Use the owning native record, not extra padding on a PS1
  compatibility view; verify actual upload rows, neighbors and reentry (#172).
- **Keep fixed-step ray semantics.** Original F01768 samples16 arithmetic
  delta>>4 steps after advancing, not an exact endpoint. Preserve unsigned16 X,
  signed Y/Z, extent subtraction, equality-as-blocked, the signed15 ground
  decoder and special bit0x2000 ground0. Checked native arithmetic must not turn
  unavailable terrain or source overflow into a fabricated clear path (#42/#178).
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

As copied (2026-10-17): the build shows about 520 pointer-to-int casts, 520 incompatible-pointer conversions and 100
int-to-pointer casts. Ordering-table links (`addPrim`, `setaddr`, the game's `vs_getTag`, `_insertTPage`) still
store 24-bit addresses in packet tags, which truncates x64 pointers: the first render work in 1b replaces them with
sort keys (plan Step 6). The size-literal asserts are not generated yet.

**Fixed before 1b (2026-10-17)**, by the copy tool from `tools\port\hazard_index.py` (libclang over the decomp:
every pointer-to-4-byte-integer cast and where its value goes):

- **Links:** ordering-table links are 24-bit handles (`LegacyGpu_Handle`/`LegacyGpu_Pointer`,
  `Legacy/Render/LibGpu.h`), so packets anywhere in 64-bit memory link correctly. `addPrim`, `setaddr` (pointer or
  copied link word), `nextPrim`, `ClearOTag(R)`, `AddPrim(s)`, `DrawOTag`/`DrawPrim` (which walk the list and hand
  each packet to the renderer) use them; 37 `(u_long)p & 0xFFFFFF` sites become `LegacyGpu_Handle(p)`, and
  `_insertTPage` and `Render::SplicePrimitiveList` were fixed by review. Unit-tested with packets 16 MiB apart.
- **Pointer storage:** casts whose value lands in something made pointer-sized become `(uintptr_t)`: three runtime
  members (`Content::ActorAssetRequest.dataAddress`, `Audio::CommandParams.param1`, `Audio::MusicConfig.sequenceBase`),
  one global (`Event::subroutineReturnCursor`), 16 locals, plus round trips and comparisons.
- **Left visible on purpose:** 33 stores through a dereference, 14 returns, 8 parameters and the on-disc members
  (`Actors::WeaponModelData`, `Actors::ShpModel`, `Content::SequenceBuffer` relocations) keep their original casts,
  so the compiler still reports them (127 pointer/integer warnings, from 315). They need signature or decoder work.
- **Struct sizes:** 5 literal sizes of structs that hold pointers became `sizeof` (e.g. `allocHeapR(0x30)` for the
  particle state). Every pointer-free struct/union (398) carries `_Static_assert(sizeof(T) == <PS1 size>)`, and
  `tools\port\layout_check.py` compares every member offset with the PS1 layout under the real x64 MSVC rules.
  That found two real bugs, now fixed in the copy: MSVC bitfield packing (7 runs given the storage type that keeps
  their PS1 offsets, e.g. `Actors::CharacterInit`, `Field::ActorPlacement`) and `size_t` members (8 bytes on x64) in
  the `SYSTEM.DAT` header, written as `u_int`. Result: 0 pointer-free layout differences.
- **Game heap:** the three start-up paths that placed the game heap at PS1 address 0x8010C000 use Core's arena
  (`Core/Arena.h`, 16 MiB). The heap's block headers hold pointers, so its 16-byte block size assumptions are a 1b
  runtime check.

Types that hold pointers (111) differ from the PS1 layout by design. Code that reaches their members by literal
offsets, or reads them from disc, is where the remaining §2 work is.

**Option B (owner, 1b attract path):** per-site fixes, keeping 64-bit pointer members (not `__ptr32` with
`/LARGEADDRESSAWARE:NO`). Mechanisms, all in the copy tool (`tools\port\copy_fixups.py`), so they hold for every
actor and room:

- **On-disc offsets stay offsets.** `ONDISC_OFFSETS` lists members of file images that the PS1 relocated in place
  (SHP, SEQ, SEQ animation bank, WEP, MPD section E). They stay 32-bit `u_int` offsets; every use becomes
  `VS_ONDISC(base, member, T)` (`Legacy/OnDisc.h`), which resolves base + offset when read, and the in-place
  relocation loops are deleted.
- **Signatures that carry pointers.** `SIG_RETYPE` (plus hazard `return:`/`param:` rows) makes such returns and
  parameters `intptr_t`/`uintptr_t`, in definitions, stubs and view casts (e.g. `Actors::FindActorAnimationHeader`,
  `Field::RoomAuxiliaryData`).
- **Dispatch must agree end to end.** Event opcodes return either control codes or script pointers. Retyping only
  branch functions left `int` in the dispatch tables and interpreter, truncating the address at tick 5485.
  All 179 distinct normal/skip handlers (182 catalog rows) now return `intptr_t` with compatible arguments, as do their table slots;
  `ExecuteScriptThread` stores the full result. The skip-table copy uses `sizeof`, not 256 PS1 words.
  `check_event_dispatch.py` audits the target set and all signatures, including normal-only handlers with
  formerly omitted arguments; changing the return type alone is not enough.
- **Type text fixes.** `TYPE_FIXES` corrects copied type text (pointer slots in `Field::RoomRecord`, padding in
  `T00634`); the layout writer uses the field ledger's scalar type when it fits the gap.
  `Native\Tests\src\LayoutAsserts.c` statically asserts the layouts code depends on (actor allocation span, stats
  placement, SHP/SEQ headers).
- **Partial runtime views.** `native_views.py` exposes reviewed pointer slots hidden inside byte padding for
  actor/AI/model views. `check_native_views.py` compares 532 selected field offsets against canonical native
  records; guarded tests cover AI storage and pointer round trips. This is not a generic ABI or bounds proof.
  Allocate the full canonical AI state, never `sizeof` its partial creation view. `layout_check.py` separates
  these explicitly rebased runtime records from fixed file layouts. The AI-state table has 16 slots (the
  next symbol is `tileFlagMap`, not `roomAiState`); generator fixups must reach extern-only globals too.
- **Native record strides.** Effect timelines allocate `sizeof(header) + count * sizeof(track)` and index their
  `tracks` array. The PS1 stride was 16 bytes; native tracks are 24. A two-track regression catches
  incorrect playback strides; allocation is also exercised by the attract run.
- **Literal offsets and sizes.** `(char*)x + 0xNN` views and `0x1900`-style sizes become named members / `sizeof`
  (`python tools\port\field_at.py <Type> 0xOFF[:bit]` maps a PS1 offset to the member). Still raw, to check when
  reached: ActionPlanning 0x9CC/0xD8C/0x1D8C, Behavior +12, Timeline +0x14, ScriptCamera +0x1DA, Gimmicks +0x16C,
  Appearance 0x55-0x64; the audio `>= 0x80000000` KSEG pointer test.
- **Debug aids.** `Core_Tracef` (C log), `Platform::LogCallStack` (also on bad data reads),
  `Platform_WatchWrites(addr, bytes)` (hardware write watchpoint), and the crash handler recovers the caller after a
  call through a null function pointer.

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

## 4. Audio: owner-selected modern approach (D23; implementation remains open)

The asset pipeline already keeps music sequenced (ASSET_PIPELINE.md §6.5: samples to WAV, AKAO streams to event
JSON) and plan §6.3 already avoids sound-chip registers. Owner decision D23 selects sfizz/SFZ+WAV instruments,
driven by the existing AKAO sequencer. M5 still has to define fidelity and unsupported-mode behavior:

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
- **Reverb:** the PS1 had a fixed set of reverb presets. M5 must implement and test the native mapping and document
  its parity limits; no hardware model runs at runtime.
- **Parity switches:** optional PS1-style 4-tap interpolation and envelope curves, for side-by-side comparison in
  1b. Phase 2 defaults to the high-quality settings.
- **Modding for free:** music and instruments become standard MIDI-like events plus SFZ+WAV, so new songs and
  replacement instruments need no custom tools. ASSET_PIPELINE's round-trip rule still applies
  (decode → event JSON + SFZ → encode == original bytes).

Risk to check first: whether SFZ envelopes can match the PS1 envelope curves closely enough. If not, document the
supported sfizz behavior and the remaining parity gap rather than adding a competing voice engine.

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

As copied: 244 stubs. 10 of them are C functions whose body is raw MIPS assembly, and 70 are never declared in C;
those are stubbed as `int f()`, the implicit declaration their callers compiled with. One assembly row (F01920,
PLG052) is written from its C twin (F01998, PLG077).

## 6. Scratchpad memory

1,061 uses in 266 functions (Menu 81, Battle 52, Field 40, Effects 37) of the PS1's 1 KiB fast scratchpad, which the
code uses as shared temporary memory.

**How (owner, 2026-10-17):** removed during modernization: the values become locals or module state. For the copy
only, the copy tool maps `getScratchAddr(n)` and `vs_scratch` to a per-module scratch struct (or one `Core` scratch
area), so the code compiles and runs unchanged until each function is modernized.

As copied: one `Core` scratch block (`Core/Scratch.h`, 1 KiB). `getScratchAddr(n)`, the game's scratch struct
(`Render::frameScratch`, a view of the block's start) and scratch literals (`(T*)0x1F8003EC`) all reach the same
bytes. Note that the block's struct holds pointers, which are 8 bytes on x64, so its later members sit at other
offsets than raw literals assume (§2).

**GIM follow-up:** F01393 (`Render::DrawGimTiles`, BATTLE `func_800CBBCC`, `5BF94.c:4056`) formed coordinate
arrays backward from `0x1F800400`. Its copy-tool fix uses independent native `short[256]` arrays, sufficient for
each byte-sized dimension plus its final boundary, and checks the tile-table capacity. Tests verify packet
coordinates and that the rest of Core's shared scratch remains untouched; do not rebase this workspace into it.
The same function's scaled-quad links now use native handles rather than truncated pointers.

## 7. Extractor (deferred)

**Owner, 2026-10-17:** `VS.Extract` stays C++. Its decoders will be ported from VS-Workshop's `Workshop.Core`
(`D:\Dev\Modding\VS-Workshop`, C#), which already decodes BIN/CUE, ISO9660, MPD/ZND/SHP/WEP/ZUD/SEQ/ESQ/TIM, palettes
and P-file effects. Workshop is still in development, so the port waits until it is further along. It is part of
Phase 1a step 5 (data disposition and asset extraction). AKAO, XA and STR are not in Workshop and are ours to write.

**Workshop headless API (available 2026-10-17).** `VS.Workshop.Headless.exe` serves the same readers over local
HTTP (127.0.0.1, bearer token from `WORKSHOP_API_TOKEN`, never committed or printed) or MCP over stdio; see
`D:\Dev\Modding\VS-Workshop\API.md`. Verified against `disks\SLUS-01040.cue` (39 operations; assets list, bytes,
fields). Where it helps this port, without replacing the C++ extractor decision:

- **Decoder oracle:** `asset_read`, `asset_texture` (PNG per palette), `asset_inspect` and the GLB export are the
  reference that `VS.Extract`'s decoders and the round-trip tests (ASSET_PIPELINE.md §7) compare against.
- **Naming evidence:** `asset_fields` returns verified field identities for on-disc formats (ZND enemy records,
  MPD, SHP...), which can confirm or correct low/opaque `OnDisc` fields in FIELD_LEDGER.csv with a citation to the
  Workshop field, alongside runtime tracing (RUNTIME_TRACING.md).
- **Development data for 1b:** extracting the few files the title slice needs before the converters exist; such
  output stays under `build\` or `Native\data\` (git-ignored), never in the repo.

## 8. Left by the copy (Step 4), for 1b and modernization

What the mechanical copy kept as it was, so that each item gets a deliberate fix rather than a guess:

- **Shims and stubs.** T shims (`Native\Engine\Legacy\`, `Native\Content\Legacy\`) and the 244 assembly-only
  functions record `VS_UNPORTED` and do nothing yet. `Core::DumpUnportedToLog` lists the ones a run reaches, which
  orders the work. libpad, VSync and the root counters (strategy R in the SDK ledger) are still shims with the SDK
  names because Engine\Input and Core::FrameClock have no matching API yet.
- **GTE math.** `Core/Gte.h` follows the documented GTE arithmetic; its rotation order, `rsin`/`ratan2` rounding,
  projection divide and fog setup need the parity tests of plan ?6.1 before gameplay depends on them.
- **Casts for per-file declarations.** 98 uses go through a cast to the view a decomp file declared and 30 calls go
  through an unprototyped pointer (copy report `view-cast`, `implicit-call-cast`). Each marks a place where two
  files disagreed about a type or an argument list; modernization picks the real type.
- **Disc positions.** `Content/DiscLayout.h` keeps real LBAs/sizes because the code does sector arithmetic; Content
  must map a sector to its converted file. `MENUBGSZ` is the ledger size of the title background asset.
- **String ids.** `Content/StringTables.h` holds the decomp's per-overlay string-table ids. Offsets are read from
  the user's extracted tables by `gen_decomp_headers.py` (`--data`), as the decomp's build reads them from the disc;
  the strings themselves stay in the extracted files.
- **Disc data.** Module data and pointer tables are filled at start-up (plan "Phase 1b status"). 88 arrays whose
  size neither the ledger nor the splat configs give stay tentative one-element definitions (84 are effect-plugin
  arrays; reported as `module-data-unsized`).
- **Helper macros.** The game's packet macros (`vs_getXY`, `vs_getRGB0`, `vs_getTag`...) keep their decomp names
  (they are not ledger rows) until the primitive code moves to the render queue.
- **Review list.** `tools\port\copy_fixups.py` holds the hand-reviewed text fixes (merged-layout member access, the
  save copies, `_insertTPage` in C, the list splice, the game heap base, two nested same-as types); the copy fails if
  one stops matching.
- **Hand work.** The copy regenerates everything from the decomp, so output is never edited in place:
  `tools\port\own.py <id> --to <new .c>` moves an item to a hand-maintained file listed in `tools\port\owned.csv`
  (ledger `modernized` = `wip`), and the copy then includes that file's header instead of emitting the item. The
  committed `tools\port\copy_manifest.json` hashes every generated file; a hand edit to one, or a file in the way,
  stops the copy before it writes anything.
- **Lint noise.** `native_boundary.py`'s `ps1-address` rule also matches sign-bit flags (`0x80000000`,
  `0x80000001`); of its 36 hits only the overlay slot table and one KSEG pointer test in Audio are addresses.

## 9. Found while running the title slice (1b)

Each of these broke the running game silently; the fix is in the copy tool or the shims, so it holds for every
module, not only the title.

- **`char` is unsigned.** The original compiler ran with `-funsigned-char` (`tools\verification\matching.py`);
  copied C now builds with `/J`. Signed `char` turned the pad bytes into negative numbers and lost buttons.
- **Signed pointer-sized integers.** A pointer stored through `(long)`/`(int)` is `intptr_t`, not `uintptr_t`:
  `(long)result < 0` is an error sentinel (`_loadFileMenu`), and unsigned made a failed card read look like a loaded
  save.
- **Views of globals.** `view-of` rows (34) were separate variables; they are macros over their base now. Watch for
  new ones when the ledger changes: the copy report lists `view-macro`, `view-unresolved` and `view-negative-offset`
  (3 left: `Field::triggerMarkerMatrix`, `Field::triggerMarkerVertex`, `Actors::sequenceTexturePagesOneBased`).
- **Assets cut in two.** Splat may split one C array into several images (`_fontTable[]` = `_fontTable` +
  `_fontTable1`); an asset now runs to the next subsegment the C code itself declares.
- **Data next to its header.** `(u_long*)&header + 1` reaches pixels stored after a size header (`_nowLoading`);
  natively they are two globals, so such uses get a reviewed fixup (F03421).
- **Reads span files.** The game reads MCDATA.BIN and MCMAN.BIN in one request; Content reads on through
  consecutive files as the drive did.
- **Event no-ops.** Copied code retries `EnableEvent`/`SetRCnt`/... until they return 1, so the D no-ops return
  success. Interrupt-completed transfers (`SpuWrite`) call their completion callback synchronously.
- **Pad stick bytes.** `Input_ReadControllerPort` stores receive-buffer bytes 4/5 as the left stick and 6/7 as the
  right; the controller sends the right stick first. The shim writes the hardware order, so the names in
  `Input\Controller.c` (from the decomp) are swapped; check against analog movement in 1c before renaming.
- **Palette-first images.** A splat `indexed` image without `cluts_after` stores its palette first, so the C symbol
  starts 512 bytes into the segment (`_uiTable`, the load screen's CAUTION label).
- **Overlay names.** Module pointer tables match the catalog overlay by file name (`BATTLE.PRG` vs
  `BATTLE/BATTLE.PRG`); before that, BATTLE function tables stayed unresolved.
- **Attract scenes (title idle) reach assembly-only rendering.** Loading (actors, SHP/SEQ/WEP, ZONE020/MAP213) and
  the game loop run; drawing reaches unported stubs: `Actors::DrawActorModel` (~1150 lines of assembly),
  `Render::SetRoomPolygonNearClip`, `Render::BuildRoomGeometryViewMatrix`, `Render::DrawSkyDome`,
  `Render::QueueSpriteWithTexturePage`, `Core::Math::MultiplyMatrixIntoRight`, `Field::SampleCollisionHeightCode`,
  `Battle::Ai::InitializeRoomAiMap`. These come first in the §5 rewrite order.
