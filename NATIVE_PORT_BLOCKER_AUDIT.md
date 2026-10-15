# Native Port Blocker Audit

**Audit date:** 2026-10-15  
**Target:** Windows Win32/Clang native port, not the historical PS1 matching build.  
**Result:** The current native target does not compile. No fresh playable
executable, title-to-game transition, room, movement, or combat milestone was
verified in this audit.

## Executive summary

The last **2.18983% of measured PS1 matching is not the native-port backlog**.
Most of those unmatched bytes are console-library implementations whose
contracts can be provided by Windows services. Conversely, already matched
game code can still be excluded, incorrectly adapted, missing its data, or
connected to an incomplete host implementation.

The native scaffold contains substantial work: Windows presentation/input,
extracted-disc reads, a software GPU/GTE, SPU mixing, movie decoding, and a
memory-card backend. These are real implementations, but not a verified game.
Several deliberately incomplete paths remain, and old comments claiming that
functions are still assembly no longer agree with the current source.

The shortest useful objective is:

> Build a fresh executable without diagnostic stubs, boot through the real title
> code, start or load a game, render one real room and Ashley, move/collide,
> complete one combat action, and save/reload on a disposable card.

Do not spend native-port effort reproducing a Sony SDK routine's exact MIPS
instruction scheduling unless it is needed to establish a behavior contract.
Keep historical matching and native functional acceptance as separate tracks.
This audit proposes that priority; it does **not** silently change the existing
decompilation acceptance rules.

## 1. Evidence and limits

Labels used below:

- **Observed:** reproduced by an audit command.
- **Source-confirmed:** directly visible in current source/build selection;
  not necessarily reached in a fresh executable.
- **Unverified:** an implementation exists, but the relevant gameplay behavior
  was not demonstrated in this audit.

### Fresh build result

From the repository root, the audit invoked:

```powershell
cmake --build .\build\windows --config Debug --target NativePort --parallel 2
```

The command exited **1** while compiling generated native game inputs. The
local diagnostic log is `build/native-audit-build.log` (ignored build output).
The first reported errors are:

| Generated location | Failure | Canonical source/context |
| --- | --- | --- |
| `146C.c:5441` and `146C.c:5444` | Integer-to-pointer assignment from an implicitly declared `func_8008BAC8` | The later definition returns `D_800F1D08_t*`; the caller needs a consistent declaration before use. |
| `146C.c:7650` | Integer passed where `u_char*` is declared | The `func_800BEC58(4, 0, i, 0)` call conflicts with the third parameter in [4A0A8.h](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/4A0A8.h). Determine the actual contract, not an arbitrary cast. |
| `146C.c:16185` | Conflicting type for `func_8008BAC8` | Consequence of the earlier implicit declaration, not an independent missing implementation. |

The source is [146C.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/146C.c).
The build prepared native inputs without rewriting canonical game sources.
Reference/snapshot revision differences were warnings, not this build's
terminating error. **The current failure precedes a fresh symbol gate/link**;
there is no verified complete current unresolved-symbol count.

### What was not used as proof

- Existing native/test executables have 2026-10-04 timestamps. They were not
  launched as substitutes for the failed current build.
- A read-only scan of existing COFF objects found unresolved-symbol leads, but
  those objects mix stale and partial compilation. It is not a current link
  inventory and must not be reported as a count of missing game functions.
- CMake Tools test discovery returned no tests in the active extension context.
  The generated build does register WindowsHost, GTE, GPU and InlineGTE tests,
  but none was freshly run here. Their existing binaries are not acceptance
  evidence.
- Old screenshots, watchdog runs, audio captures and "DONE" labels in the
  historical docs are research history, not verification of current inputs.
- This was a native-integration audit, not a new decompilation, full call-graph
  proof, security review, or exhaustive hardware-conformance review.

## 2. Ranked blockers

P0 = obtaining a trustworthy native build; P1 = first playable slice;
P2 = broader functionality/fidelity. These are implementation priorities, not
security severities.

| ID | Priority / evidence | Blocker | Player impact and required action |
| --- | --- | --- | --- |
| B01 | P0 / observed | Current game input fails Clang compilation | No fresh executable. Resolve the declaration/contracts above without suppressing type checking or changing game semantics. |
| B02 | P0 / source-confirmed | Game and host function ownership overlap | Establish one provider per symbol. Do not link both legacy replacements and their current reconstructed implementations. |
| B03 | P0 / source-confirmed | Whole gameplay/menu units are excluded | Reintegrate ordinary C functions and isolate genuine hardware/assembly seams. Matching progress in excluded units currently does not reach the native executable. |
| B04 | P0 / source-confirmed | Overlay jump/return adapters are absent | Title/game/reset/ending transitions need explicit host control flow, not a saved pointer to an expired local variable or execution of MIPS bytes. |
| B05 | P0 / source-confirmed gap | Complete native data/alias ownership is not established | Reconstruct correctly sized data, offsets, initialization and native function-pointer tables. Linking arbitrary zero buffers is forbidden. |
| B06 | P1 / source-confirmed | Four remaining game-engine routines have no native implementation identified | Needed for model rendering, movement checks, battle-result application and scripted actor actions. Implement/verify behavior, retaining separate matching status. |
| B07 | P1 / source-confirmed gap | Banked effect symbol identities are not fully translated | A generic address-based name is not a unique effect identity. Build an overlay-qualified function/data registry and wire the dispatcher. |
| B08 | P1 / source-confirmed | Native preparation deliberately removes visible/game-state behavior | Restore Ashley/menu 3D rendering, fixed-width glyphs/dialogue frames, and live-state save capture. |
| B09 | P1 / source-confirmed | GPU state/rendering is incomplete; address representation is inconsistent | Rooms/UI/effects may render incorrectly or encounter invalid primitive addresses. Complete required commands and resolve PS1/host address translation. |
| B10 | P1 / source-confirmed | GTE supports only selected matrix/translation modes | Correct-looking simple transforms do not establish real actor animation/lighting. Cover modes exercised by the first room and model renderer. |
| B11 | P1 / source-confirmed | Disc APIs contain success-shaped placeholders and incomplete queue semantics | Loading can falsely finish, lose callbacks or use incomplete results. Validate real requests and propagate failures. |
| B12 | P1 / source-confirmed | Save backend cannot create new files and packaging skips live-state capture | Existing-file reads are not a complete new-game/save/load loop. Implement safe persistence and test on disposable images only. |
| B13 | P1-P2 / source-confirmed + unverified integration | Audio pipeline is partial | Validate actual music sequencing/sample loading; reverb/noise/modulation and some API contracts remain missing. |
| B14 | P2 / source-confirmed + unverified integration | Input, timing and movie services have restricted contracts | Verify gameplay input and callback ordering; distinguish supported single-pad behavior from missing rumble/multitap and streaming modes. |

### B02-B03: source selection and ownership need reconciliation

[CMakeLists.txt](CMakeLists.txt) recursively includes generated game C, but excludes
entire units by basename, including
[38C1C.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/38C1C.c) and
[4A0A8.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/4A0A8.c).
These contain actor/model geometry helpers and actor/script handlers, not just
replaceable PS1 driver code. It also excludes:

- [MENUB/260.c](resources/rood-reverseWin/src/MENU/MENUB.PRG/260.c):
  loot/inventory-menu control.
- [MENUF/3B8.c](resources/rood-reverseWin/src/MENU/MENUF.PRG/3B8.c):
  congratulations/time-attack/cube-puzzle-related menu control.
- Interrupt, counter, sound-library, movie-library and overlay units, some of
  which intentionally belong on the host side but still need full contracts.

Current target source inspection also found both providers for these examples:

| Symbol | Game provider | Host provider |
| --- | --- | --- |
| `PadGetState` | [PADENTRY.c](resources/rood-reverseWin/src/SLUS_010.40/libpad/PADENTRY.c) | [libpad.c](native/platform/libpad.c) |
| `SetDefDispEnv` | [E04.c](resources/rood-reverseWin/src/SLUS_010.40/libgpu/E04.c) | [libgpu.c](native/platform/libgpu.c) |
| `func_800C930C` | [5BF94.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/5BF94.c) | [battle_menu.c](native/platform/battle_menu.c) |

These are source-level provider conflicts; this audit did not reach a fresh
linker diagnostic for them. The generator preserves an upstream C function,
but that alone does not remove its legacy replacement from platform objects.
Choose the provider explicitly and verify ABI/data compatibility.

**Important correction to old documentation:** `vs_battle_exec` now has a C
definition in [146C.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/146C.c)
and is selected for the native target. Likewise, `func_8001733C` has a C
definition in [25AC.c](resources/rood-reverseWin/src/SLUS_010.40/25AC.c).
Neither should be described as a proven missing implementation merely because
old comments or stale objects say so. Their integration remains unverified.

### B04-B05: addresses, overlays and data are not optional plumbing

[main.c](resources/rood-reverseWin/src/SLUS_010.40/main.c) drives the real sequence:
system initialization, title loading/execution, battle loading, then
`vs_battle_exec`. It also calls `vs_overlay_jumpToTitle` and
`vs_overlay_jumpToBattle` for transitions.

The original [overlay.c](resources/rood-reverseWin/src/SLUS_010.40/overlay.c)
uses MIPS stack-restoring jumps and is excluded. No host definitions of those
two jump functions were identified. The host
[battle_asm.c](native/platform/battle_asm.c) provides only a no-op wait and a
`vs_overlay_getSp` that stores the address of a local variable. That is not a
portable continuation implementation.

[scratchpad.c](native/platform/scratchpad.c) maps PS1 RAM and register addresses
as host memory. This prevents some access faults; it does not emulate hardware
side effects or turn retail MIPS function pointers into host functions.
[kernel.c](native/platform/kernel.c) allows overlay binaries to be loaded at
their PS1 addresses even though native functions are statically linked.
Code pointers, shared overlay data, lifetimes and resets must be handled
explicitly.

[prepare_data.py](native/tools/prepare_data.py) emits retail asset ranges and
aliases; [harvest_data.py](native/tools/harvest_data.py) extracts additional
data. Those mechanisms do not by themselves establish every required BSS
object, shared-state alias or relocated dispatch table.
[coff_stubs.py](native/tools/coff_stubs.py) correctly rejects unresolved
non-CRT symbols and does not fabricate generic data. Retain that gate.

### B06-B07: game behavior that actually remains

| Required behavior | Routine / location | Native status |
| --- | --- | --- |
| Actor/model root and bone transforms, scaling, clipping and render dispatch | `func_800B28A8`, [49268.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/49268.c) | Canonical assembly inclusion; no host definition identified. |
| Movement-step/terrain checking used by surrounding movement logic | `func_800E3600`, [771A8.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/771A8.c) | Canonical assembly inclusion; no host definition identified. |
| Apply queued ability effects to targets and select responses | `func_8006CB04`, [146C.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/146C.c) | Canonical assembly inclusion; ignored candidates are not native implementations. |
| Start/wait/cancel scripted actor actions | `func_800B9DB4`, [4A0A8.c](resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/4A0A8.c) | Assembly inclusion, and its whole source unit is excluded. |
| Additional effect behavior | Assembly-backed routines in PLG052/053/054/055/074 | Outside the cited measured matching report; required paths must be inventoried separately rather than called complete. |

The generated native `INCLUDE_ASM` and `INCLUDE_RODATA` macros expand to nothing.
An assembly reference is therefore not a working host implementation or a
definition of its associated tables.

Effect adaptation currently renames every `func_800F9800` identifier within
each effect source to that directory's entry name:
[prepare_native.py](native/tools/prepare_native.py).
But [EFFPURGE/0.c](resources/rood-reverseWin/src/EFFECT/EFFPURGE.BIN/0.c)
contains a dispatch table with repeated generic names from different banks.
Renaming references according to the dispatcher's directory does not resolve
them to their owning plugins. Other reused address-based names also need
overlay-qualified identities. Static linking alone is not sufficient.

### B08-B10: concrete visual and geometry gaps

- **Ashley is deliberately removed from the menu portrait path.**
  [prepare_native.py](native/tools/prepare_native.py) replaces `_renderAshley`
  with an empty body. That is not a missing SDK dependency.
- **Other visible no-ops:** [battle_asm.c](native/platform/battle_asm.c) omits
  the submenu 3D camera setup and fixed-width glyph drawing;
  [battle_textbox.c](native/platform/battle_textbox.c) leaves
  `vs_battle_renderDialogBalloon` empty. Some sprite/text builders do have
  implementations despite stale file-header descriptions.
- **GPU command/state coverage:** [libgpu.c](native/platform/libgpu.c)
  rasterizes flat/Gouraud/textured triangles and sprites, but ignores GP0
  E2-E6 state commands. `SetDrawArea`, `SetDrawMode`, `SetDrawMove` and
  `SetDrawTPage` are empty. Semi-transparency flags are set in primitives,
  but the pixel paths shown overwrite pixels instead of applying the PS1
  blend modes. Line rendering is not implemented by the command executor.
- **Address-contract conflict:** the GPU requires OT/primitive host pointers
  to fit in 24 bits and dereferences masked addresses directly. The game's
  heap starts at mapped `0x8010C000`. Such pointers do not fit that contract.
  Determine actual primitive allocation paths and use a consistent address
  translation/arena model. A low executable image base does not make the
  mapped PS1 heap low-addressed. This is a source-confirmed architectural
  conflict, not a reproduced first-room crash.
- **GTE modes:** [gte_core.c](native/platform/gte_core.c) always selects the
  rotation matrix in `gte_op_mvmva`; nonzero matrix selection is ignored,
  and only translation selector zero adds a base. Contrary to its comment,
  unsupported modes do not assert there. Existing simple-mode tests do not
  establish all model/lighting paths.

### B11-B14: host services exist, but their contracts are incomplete

| Service | Implementation present | Concrete gap / acceptance needed |
| --- | --- | --- |
| Disc/loading | [libds.c](native/platform/libds.c) indexes extracted files, reads sectors and pumps callbacks. | `DsSync` always reports completion; several status/callback APIs ignore inputs. There is one pending command-callback slot. `DsRead` returns the requested count even when individual sector reads fail. Validate queue concurrency, result buffers, callback order and missing/short-file errors. |
| Streaming/FMV | [libstream.c](native/platform/libstream.c), [libpress.c](native/platform/libpress.c), [mdec.c](native/platform/mdec.c), [cdxa.c](native/platform/cdxa.c) implement substantial software paths. | Stream start/end/mode/callback arguments and masks are ignored; full rings drop frames. General VLC variant is unsupported. Prove current intro completion, skip, A/V timing and return to title; historical decoded-frame counts are not a fresh visual comparison. |
| Saves/cards | [libapi.c](native/platform/libapi.c) opens an existing writable `native/saves/test_card.mcd`, exposes raw frames and caches its filesystem. | New-file creation is TODO; format reports success without formatting. Raw writes do not update the cached filesystem image. Flush operations do not consistently propagate persistence failure. Channel behavior is restricted. Never test against a valuable original save. |
| Live save state | [prepare_native.py](native/tools/prepare_native.py) removes `func_80042CB0()` inside `_packageGameSaveData`; [battle_asm.c](native/platform/battle_asm.c) returns a constant for `func_800E6694`. | Restoring a file is not enough: save capture must reflect current battle/character state before serialization. |
| Audio | [sound_asm.c](native/platform/sound_asm.c), [sound_tables.c](native/platform/sound_tables.c), [libspu.c](native/platform/libspu.c), [spu_core.c](native/platform/spu_core.c), [audio_sink.c](native/platform/audio_sink.c) implement sequencing adapters, sample transfers, voices and output. | Reconcile legacy adapters with current game providers. Reverb/noise/modulation are omitted; interpolation differs. SPU allocation, voice-attribute and IRQ APIs contain placeholders. Establish which are actually required, then test real music, transitions and combat SFX. |
| Input | [libpad.c](native/platform/libpad.c), [present.c](native/platform/present.c) publish PS1-format packets from keyboard/SDL input. | Port zero is always reported stable/analog; communication toggles and mode requests are simplified, rumble is discarded, and multitap is not faithfully implemented. Validate game-derived pressed/released/repeat and analog behavior, not just host key polling. |
| Timing/events | [libetc.c](native/platform/libetc.c), [kernel.c](native/platform/kernel.c), [libapi.c](native/platform/libapi.c) provide pacing, callbacks and events. | Multiple hardware services are pumped from `VSync`; CD additionally services polling. Verify progress when game code waits without VSync, callback reentrancy/order, counter catch-up and pause/resume. Mapped register RAM alone cannot provide those semantics. |

## 3. Boot-to-combat milestone gates

No stage below is newly certified by this audit. Fixing the first compilation
error does not certify the next stage.

| Gate | What blocks or needs proof | Required pass evidence |
| --- | --- | --- |
| G0: trustworthy build | B01-B05; stale generated/object inputs can obscure missing providers. | Fresh Debug native build; exactly one provider per required symbol; typed data/alias ownership; no diagnostic stubs or guessed buffers. Record source/build identity. |
| G1: boot and interactive title | Disc, timers, graphics, input and movie integration. | Fresh executable reaches title using real extracted assets; input selects items; intro completes and skips correctly; no watchdog termination counted as success. |
| G2: new game and continue | Host overlay transitions, initialization/data ownership, save-loading semantics. | Both paths reach real battle initialization. Continue loads a disposable known save and verifies character/inventory/scene state. Record transition traces, not just a loading image. |
| G3: first room and Ashley | Excluded actor helpers, model renderer, GTE/GPU modes, asset pointers. | Load real room/model/texture data; visible correctly posed Ashley and environment; camera changes and frames remain stable; compare a controlled reference scene. |
| G4: movement and room transition | Movement helper, terrain/collision, scripts and asynchronous loading. | Walk/turn, collide with walls, traverse relevant height changes, use a door and enter a second room without broken state or timing. |
| G5: one real combat action | Battle effects, actor-action handler, effect dispatcher, UI and audio. | Select a target, execute an action, verify hit/miss and HP/status changes, animation/effect completion and return to control. No substitution of a synthetic menu harness. |
| G6: persistence and return loop | Save capture, creation/overwrite/error handling, title/game continuations. | Save changed live state to a disposable card, exit/restart, reload and verify it. Then return to title and start/load again without stale overlay state. |

G1-G5 are the first-playable sequence. G6 is a separate safety/completeness
gate: a game that moves but cannot safely preserve state is not a finished port.

## 4. Recommended implementation order

1. **Repair native compilation with verified declarations.** Keep generated
   outputs disposable; fix the owning declaration or explicit native adapter,
   never just edit generated C or suppress errors.
2. **Reconcile source ownership and exclusions.** Prefer current game C over
   obsolete replacements; select host libraries intentionally instead of
   compiling conflicting PS1 and host providers. Reintegrate non-hardware
   functions from excluded gameplay units.
3. **Produce a fresh unresolved-symbol/data ledger.** Only after all current
   selected objects compile. Separate functions, storage, aliases, relocated
   pointers and intentionally replaced hardware contracts. Do not reuse the
   stale-object diagnostic as that ledger.
4. **Implement host overlay/scene transitions and bank-qualified dispatch.**
   Preserve game state and control-flow semantics; never jump into retail MIPS
   data. Verify repeated transitions, not just first entry.
5. **Close G1-G3.** Establish one reproducible title-to-first-room scenario.
   Resolve primitive addressing and required model/GTE/GPU behavior using
   reference captures and state checks.
6. **Close G4-G5.** Implement missing gameplay routines in dependency order
   and validate movement/combat outcomes. Expand effect coverage explicitly.
7. **Close G6 and fidelity gaps.** Safe saves, music/effects, richer GPU/SPU
   behavior, controller edge cases and later-game scenes.

The exact order inside steps 4-6 should follow the first verified blocker in
the fresh executable, not a guess based on a function's size.

## 5. Progress reporting from here

Track three independent dimensions:

| Track | Valid claim | Not a valid substitute |
| --- | --- | --- |
| Historical reconstruction | Fresh accepted function/unit/binary matches under the decomp rules | Native compilation or a similar-looking screenshot |
| Native implementation | Real provider/contract, tests, correct data and error behavior | Linked abort stubs, generic storage or success-returning no-ops |
| Playable integration | G0-G6 evidence tied to a fresh build and reproducible scenario | An old executable, watchdog exit, nonblack pixels or source matching percentage |

Do not invent a new native completion percentage from line counts or from
the 97.81017% historical code score. Report the last passing gate, next concrete
failure, unsupported behaviors and test coverage.

## Related records

- [Root project status and build instructions](README.md)
- [Windows migration and historical observations](docs/WINDOWS_MIGRATION.md)
- [Legacy deviations ledger](docs/NATIVE_DEVIATIONS.md)
- [Older native-port design/backlog](docs/NATIVE_PORT_PLAN.md)
- [Historical decompilation progress](resources/rood-reverseWin/DECOMP_PROGRESS.md)

Older records contain useful evidence but also obsolete function inventories and
completion labels. This audit's dated findings distinguish current source from
those historical claims; it does not certify or erase the older experiments.
