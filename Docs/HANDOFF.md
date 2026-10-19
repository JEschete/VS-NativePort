# Handoff: prologue entry and shared rendering

## Current checkpoint

Prologue combat entry and remaining intro regressions are the active priority. The older
tick-5751 stopping point below is historical, not the current runtime baseline.

- Debug and Release desktop/test targets build. Both full suites pass **66 tests / 30505 assertions**.
- Required module initialization reports **334 data entries, 19 pointer tables, 2991 symbols,
  50 modules, zero failures**. Copies and relocations validate source extents, destination capacities,
  and reviewed native element strides; unresolved required data prevents startup.
- Native views: **590 fields, zero mismatches**. Layout check: **514 types, zero pointer-free
  mismatches, 78 pointer-bearing differences, zero parse errors**. Event dispatch: **179 signatures,
  zero errors**, including both 256-entry tables. A second copier run leaves all generated content
  hashes unchanged.
- Shared room/sky and normal actor geometry now render. Owned scalar/matrix math, native model
  workspace, bone transforms/projection, encoded text, and native ordering links have focused tests.
  Packet decoding rejects truncated commands and cyclic lists instead of reading beyond packets.
- Facial ETM loading/release now uses `Actors_sharedSequenceBankTexturePages[slot - 1]`. Original
  `D_800F244F[slot]` and `D_800F2450[slot - 1]` name the same byte; independent native arrays broke
  the loader-to-animation hand-off. Public slots are checked before the one-based conversion.
  Tests cover 4-bit/8-bit frame rectangles, uploaded texels, a nonzero blink frame, frame-zero
  restoration on bank switching/release, and page retirement. Both hand-off and bank-switch tests
  failed before their respective corrections.
- Shared-bank selection now reads named actor fields, not PS1 offsets. The three row bytes read
  by `func_8009CE9C` at actor+0x183E are represented by a union: the third byte retains its original
  overlap with `momentumDistance` at +0x1840. The actor extent and subsequent field offsets are
  unchanged; regression coverage switches to both alternate banks and checks the overlap.
- The second-demo sword-draw stall was an audio-volume wait. Counter 2's callback was discarded by
  the removed `OpenEvent` implementation. It now runs at 240 Hz from logical frames; fades advance
  every fourth callback. The newly reached mixer path also handles an absent saved music layer.
- Room AI allocation and its native-sized flag/height tables are initialized before enemy metadata.
  The two-layer collision sampler now decodes real 4-by-4 patterns and surface flags. The later
  `ActorStatusConditionScore` fault came from the stubbed condition snapshot: the owned producer now
  fills actor IDs, HP, MP, statuses, and equipment DP before scoring. A poisoned-table fixture checks it.
- The first basic attack reproduced a null-menu access in `Battle_CommitSelectedAction` at tick 12621.
  G01904 (`Battle_actionDefinitions`) was excluded from startup loading by incorrect pointer metadata
  and the anonymous hit-record spelling. Its reviewed 52-byte records now load from SLUS offset
  `0x3C1DC`; basic attack 1 has category 14 and body-part targeting, not zero-filled environment targeting.
  Native layout assertions and the real-disc module test check its decoded fields and targeting policy.
- G00229 (`Effects_emptyPackage`, the existing label for the embedded default package) had only its
  four-byte header allocated. The parser walked into unrelated native memory; a relink exposed a
  watchdog stall at tick 1372. It now owns all `0x5200` bytes from BATTLE offset `0x83CBC`: nine tagged
  blocks and the zero terminator at `+0x51FC`. The module regression binds the real package and checks
  texture, animation, timeline, curve, particle, program, parameter, transform, and sound-table data.
- GIM rendering and event views now share the two-byte flag header and native pointer stride.
  `ENKEI.GIM` fills the manor splash rather than displaying a small image over the room. Captures
  at ticks 1600/1800 in `build/new-game-actors` match the supplied full-illustration composition.
- On-disc room-effect command headers remain four bytes, fixing shifted motion/rotation operands.
  Gimmick views expose their native model pointer and use the canonical room scale. Room triangle/quad
  subdivision, adaptive special-model quads, GPU mask packets, and room-light color sampling now have
  focused tests. Actor drawing preserves the update-owned flag and applies signed tint adjustments.
- Debug and Release New Game replays reach **19000 ticks** with dialogue, basic-attack, and directional
  button inputs without the reproduced crash. The final Debug log is `build/Debug/prologue-debug-replay.log`;
  captures and the preserved user playthrough log are in `build/new-game-actors/prologue-input`.
  Release also completes **43000 ticks** across both demos and returns to title. Its event trace is
  `build/Debug/window-event-replay.log`, with captures in `build/facial-textures/window-pause`.
  These accelerated runs are not proof of complete combat or normal-speed timing parity.
- The null-actor movement dump in `Pasted text.md` matched a detached room-spawn alias. The shared
  caller now reads `Field_zoneContext.mpdEnemyCount`; both current binaries contain that correction.

## Reference update review

- Reviewed `resources/rood-reverse` at `2a589e01`, including the latest pull after `c37e3eb2`.
  That batch adds shared trail/ring implementations, effect relinking, ending refactors, and image tools.
  Its battle-path changes rename screen dimensions without changing behavior.
- Actor dispatch already has the native label `Actors_ExecuteQueuedActorCommands` (F00161), and the
  pinned snapshot already includes C implementations of it and shared trail routines. Do not duplicate
  these implementations or replace established native types with another snapshot's layouts.
- `RunActorAiUpdateStages`, `SetActorVerticalMovementSteps`, and `SetDirectMovement` remain assembly-backed
  in the updated upstream on the inspected paths. The local `rood-reverse-noprompt` snapshot also retains
  assembly for those entry points. No reference snapshot was changed or bulk-imported during this review.

## Remaining limits

- The user confirmed that lighting fixes resolved the doubled faces and restored title cards resolved
  the apparent early staging/repeated entrance. Door motion and the full wyvern sequence remain unverified.
  The companion rendering pass is enabled. Material overrides, full lighting/shadows, and broad
  camera/pose parity remain incomplete. The model workspace is sequential and not reentrant.
- The reported extended Guildenstern window pause was not reproduced as a hard stall in the accelerated
  demo trace; the window scene advances and the demos finish. Normal-speed duration and the recording's
  exact interval remain unverified. Do not shorten original script delays or claim this pause fixed.
- `func_800B3A68` does not read its third argument in the original assembly either. The unused native
  `nearOffset` parameter alone is not evidence of a missing clipping operation. Texture scrolling's
  incoming-register comparison also exists in the original assembly; its intended semantics remain
  unresolved and were not guessed during the facial fix.
- The title/battle boundary uses C `setjmp`/`longjmp` after existing cleanup. It must not bypass live
  C++ destructors. Earlier runs observed title re-entry, but this is not complete lifecycle ownership.
- The two human encounters and D'tok are interactive prologue gameplay, not skippable demo segments.
  Validation must drive actual controller actions and check combat/scene state. No enemy defeat,
  HP edit, event-flag bypass, or combat completion was used or claimed in these replays. Higher-level
  navigation, planning, and combat routines still include stubs. The live handoff reaches the unported
  `RunActorAiUpdateStages`, `SetDirectMovement`, and `DrawSphereVolumeWireframe` paths; completing these
  and validating actual target selection/action execution is still required for a playable encounter.
- No full playthrough, collision/combat completion, save support, audio/timing parity, or final
  asset-format conversion is claimed. VS Workshop informs asset layouts but does not execute ETM
  facial overlays or establish runtime visual parity.

## Diagnostics

- `--capture <dir> --at <ticks> --capture-pages` also writes a 1024-by-512 page-space image and raw
  little-endian 16-bit words for each presented capture. `tools/test/decode_pages.ps1` decodes a selected
  4-bit, 8-bit, or direct-color region with its runtime CLUT. Coordinates X/Y are page-space words/rows;
  Width/Height are decoded pixels.
- Optional `texture` and `event-step` debug channels report facial copy selections and event commands.
  Frame presentation may happen on alternating ticks; inspect adjacent render ticks when correlating logs.
- Portable DuckStation captures were collected under `build/ds-render-parity-20261018`; the installed
  emulator and BIOS were not modified. These do not constitute a complete matched-sequence comparison.

## Historical checkpoint

The notes below describe the earlier native-layout/dispatch follow-up based on `218a9b5f`.
Read with `.github/copilot-instructions.md` (rules, machine paths),
[BUILDING.md](BUILDING.md), [PORTING_WATCHLIST.md](PORTING_WATCHLIST.md) §2/§5/§9 and
[NATIVE_SOURCE_PORT_PLAN.md](NATIVE_SOURCE_PORT_PLAN.md) §6.8.1.

## Rules (owner)

- Never edit generated files. Fixes go into `tools\port\copy_to_native.py` or `tools\port\copy_fixups.py`; hand
  ports go through `python tools\port\own.py <id> --to Native\<Project>\src\<Area>\<File>.c`.
- Never commit a BIOS. x64 layouts: **option B** (per-site fixes, 64-bit pointer members kept).
- Actor/sequence/room work stays on the shared path (plan §6.8.1): Content `ActorAssets.c` (SHP/SEQ/WEP loader),
  `Actors\ActorLoading.c`/`ActorState` (actor handler), `Actors\Animation.c` (sequence handler). Every fix is a
  copy-tool rule on those shared functions, so it applies to every actor.

## Done

- 1b title slice: logos, intro movie, title, menu, load screen (CAUTION label fixed), controller input.
- Tools and `C:\Tools` in the solution (Tools folder); DuckStation/BIOS paths logged; test scripts in `tools\test\`.
- Attract path (commit `d9664700`): on-disc offsets via `VS_ONDISC`, `SIG_RETYPE`, `TYPE_FIXES`, actor record and
  allocation fixes, module pointer tables resolve, debug aids (`Core_Tracef`, `Platform::LogCallStack`,
  `Platform_WatchWrites`, null-call stack reporting), `--speed`.
- After it: `Render::QueueSprite`, `QueueSpriteWithTexturePage`, `QueueDefaultSprite`, `LoadFrameViewMatrix`
  hand-ported from inline assembly (owned, `Native\Engine\src\Render\Sprites.c`). This fixed the
  `Battle_DrawBattleHud` crash (a stub returned null).
- Toggleable logging: `--log-level trace|debug|info|warn|error|off`, `--log-channels ai,actors,...`, and logical
  tick stamps. C diagnostics use `Core_Debugf`; existing `Core_Tracef` is trace-level. See BUILDING §6.
- `Core_Memory_ClearAlignedWords` (F01758 / BATTLE `func_800E4288`, `7B5DC.s:344-367`) is now an owned port in
  `Engine\src\Core\Memory\WordCopy.c`, with assembly provenance and bounds/full-size tests. Unlike SLUS's
  `ZeroAlignedWords`, it does not truncate the size to 16 bits. Nonpositive sizes are a documented native no-op.
- Shared actor/AI views: `tools\port\native_views.py` exposes native pointer slots hidden in PS1 padding,
  including the canonical AI state's position/tile sources and decision callback. Corrected the linked-actor
  byte's MSVC bitfield storage, model-pointer view, and two misleadingly named view families. File layouts are
  unchanged. `check_native_views.py` checks 532 selected non-padding field offsets: **0 mismatches**.
- AI creation (F01818): allocates/clears the full `Battle_Ai_ActorAiState` (1264 native bytes, not the partial
  creation view), uses `RoomAiState.specialActors`, and calls the real tagged-bone signature with `SVECTOR`.
  G00579 has **16 slots** in both definition and header: the next BSS symbol is `tileFlagMap` at 0x800F58B8,
  0x40 bytes after the table at 0x800F5878 (`7E05C.s:541-575`). `0x10` is a linked-actor sentinel, not a slot.
  The copier now applies reviewed fixups to globals synthesized from extern declarations too.
- Effect timelines (F01557/F01589): allocate native-sized tracks and index `timeline->tracks[i]`, instead of
  assuming 16-byte PS1 records. Native stride is 24 bytes; the old sound-track crash at tick 5483 is cleared.
- Event dispatch: all 179 distinct normal/skip handlers (182 catalog rows), tables and interpreter result use `intptr_t`. This includes
  saved-address returns and control-code-only handlers; no incompatible function-pointer casts were added.
  The skip table allocation/copy uses its complete native size (256 pointers), not 0x400 bytes. Actual branches
  at tick 5485 now retain their full native addresses. Debug channels `effects` and `event` log these boundaries.
- Layout/dispatch baseline: Debug and Release each passed **19 tests / 30141 assertions**. Tests cover guarded full AI
  storage and cross-view pointers, two-track timeline indexing, normal/skip branch dispatch, saved returns,
  conditional/backward jumps and thread starts. Release build log: `build\ai-release-build.log`.
- `check_event_dispatch.py` verifies the complete normal/skip handler set and compatible argument/return types
  against the extracted BATTLE.PRG and generated skip initializer: 179 distinct signatures, zero errors.
- Final 1800-tick scripted title/menu run with Down at 1200 and Circle at 1300 exits 0 and produced captures at
  1500/1700 (`build\ai-final-title`); log: `build\ai-final-title.log` (git-ignored).
- CLI smoke checks reject invalid/missing logging values with exit 1; `off` leaves an empty log and still
  captures tick 30; `trace`/`frame` emits begin/end records through tick 30 without info-channel leakage.
- GIM tiles (F01393 / BATTLE `func_800CBBCC`, `5BF94.c:4056`): copy-tool fixes replace the computed PS1 scratch
  address with two native 256-entry coordinate arrays (byte dimensions plus the final boundary), reject grids
  exceeding the 250-entry tile payload, and use `LegacyGpu_Handle` for scaled-quad ordering links. Fixed-point
  coordinate formulas and the shared tile/fade API are unchanged. No generated source was patched by hand.
- GIM follow-up: Debug and Release builds and **25 tests / 30178 assertions** pass. Six new tests exercise
  coordinate rounding, clipping, dimension/tile-table bounds, shared-scratch isolation, packet-buffer guards,
  native links, framebuffer fades and wipe/crossfade fast paths. Regeneration is stable; native-view, layout and
  event-dispatch checks pass. Release build log: `build\gim-release-build.log`.
- Fresh 1800-tick title/menu regression exits 0 with captures at 1500/1700 in
  `build\gim-title-20261018-011630`; log: `build\gim-title-20261018-011630.log`.

## Historical stopping point (superseded)

Run: `tools\test\run_native.ps1 -NoCopy -SkipIntro -Speed 8 -Frames 8000 -At '1500,5400,5500,7900' -LogLevel debug -LogChannels 'ai,actors,effects,event,boot,input,content,render'`
(the demo starts about tick 5300).
Latest log: `build\gim-attract-20261018-011630.log`; captures at 1500/5400/5500 in
`build\gim-attract-20261018-011630`. The former tick-5487 GIM fault is cleared, with no GIM grid-rejection
diagnostics in this run. The tick-7900 capture is still not reached.
Ashley initialization returns at tick 5363; scripted actors 2/3/4 report matching native/view IDs at
5371/5381/5391. They now take the intended no-AI path according to their actor flags. Loads ZONE020.ZND,
MAP213.MPD, HEADQUAT.GIM and E229.P; playback now advances to **tick 5751**:

```
access violation reading 0xFFFFFFFFFFFFFFFF
#0 Actors_FindActorAnimationHeader Game\src\Actors\Animation.c:1247
#1 Actors_PlayActorAnimation       Game\src\Actors\Animation.c:759
#2 Event_ScriptPlayActorAnimation  Game\src\Event\ScriptActors.c:116
#3 Event_ExecuteScriptThread       Game\src\Event\ScriptVm.c:964
```

**Next investigation:** the shared actor/SEQ animation-header lookup. The failing read is
`VS_ONDISC(p, animationMap, u_char *)[idx]`; trace the selected sequence pointer, on-disc offset and index before
choosing a fix. This later animation failure was not repaired as part of the GIM work.

The runner correctly rejects this run with exit `-1073741819` (0xC0000005); the requested tick-7900 capture is
not reached. Wrappers that catch this expected exception may exit 0: that is **not** gameplay success.
The earlier actor-ID, effect-stride, event-return and GIM scratch failures are cleared; **the attract run still fails**.

Validation limits: the 532-field checker audits selected offsets, not all widths/strides or every runtime view.
The model-family slot list covers only the reviewed early model/sequence region, not the entire ActorRecord.
`layout_check.py` reports 519 checked types, zero pointer-free mismatches, 80 expected pointer-bearing layout
differences and zero parse errors. No full AI simulation, visual-fidelity comparison or real-time A/V validation
is claimed. AI room/relations/locomotion prerequisites remain stubs; downstream planning workspace offsets
remain unreviewed. The initialization log's height 65397 is the creation view's unsigned representation of -139,
not proof of correct collision behavior. No further assembly functions were ported in this layout slice.

## Stubs reached at the historical checkpoint

From the log (`[warn] [tick=N] unported:`). Assembly lives in `resources\rood-reverseWin\src\...` (`.s` files) or the
`INCLUDE_ASM` splat output under `resources\rood-reverseWin\build\splat-artifacts\`.

| Stub | Catalog | Assembly | Size |
|---|---|---|---|
| `Core::ElapsedFrameCounterTicks` | SLUS | root counter (hardware): modernize to a frame timer; returns 0 now = always within AI budget | tiny |
| `Core::Math::MultiplyMatrixIntoRight` | F03415 | `SLUS_010.40\32154.s` | small (GTE) |
| `Field::SampleCollisionHeightCode` | F00772 | `BATTLE\BATTLE.PRG\2EA3C.s` | small |
| `Render::SetRoomPolygonNearClip` | F00753 | `2EA3C.s` | small |
| `Render::BuildRoomGeometryViewMatrix` | F00759 | `2EA3C.s` | medium |
| `Render::DrawSkyDome` | F00757 | `2EA3C.s` | medium |
| `Battle::Ai::InitializeRoomAiMap` | F01838 | `BATTLE\BATTLE.PRG\7E05C.s` | medium |
| `Battle::Ai::InitializeActorAiExtensionHook` | F01835 | `7E05C.s` | small |
| `Battle::Ai::UpdateActorNavigationOverrides` | — | `Battle\Ai\Navigation.c:856` stub | ? |
| `Actors::DrawActorModel` | F01023 | `func_800B28A8.s` (INCLUDE_ASM) | ~1150 lines |

Room/actor geometry was not implemented at that checkpoint. The current shared normal-material path and its
remaining limitations are described above. Raw offsets still in code reached later are listed in
PORTING_WATCHLIST §2 (Option B).

## Reference

- DuckStation reference of the attract scene: `tools\test\duckstation_capture.ps1` (demo at about 100 s, VKP
  Headquarters / Valnain); earlier captures in `build\ds_attract`.
- Regression before each commit:
  - Debug and Release builds:
    `& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' VagrantStory.Native.sln /m /nologo /v:minimal /p:Configuration=<Debug|Release> /p:Platform=x64`
  - Tests: `test-native.cmd Debug`
  - Title, menu and load screen: `tools\test\run_native.ps1 -NoCopy -SkipIntro -Frames 1600 -At 1590 -Press '1200:Down,1300:Circle'`
- After new files: `python tools\scaffold\gen_solution.py`. After copy-tool changes: `python tools\port\copy_to_native.py --report`.
- Layout gates: `python tools\port\check_native_views.py`, `python tools\port\layout_check.py`, and
  `python tools\port\check_event_dispatch.py` (requires the extracted US BATTLE.PRG).
  Visual Studio currently does not discover this Catch2 suite; run `build\Debug\VS.Tests.exe` and
  `build\Release\VS.Tests.exe` (or `test-native.cmd <configuration>`).
