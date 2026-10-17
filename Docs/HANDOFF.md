# Handoff: title attract scenes (Phase 1b follow-up)

Last updated at commit after `d9664700`. Read with `.github/copilot-instructions.md` (rules, machine paths),
[BUILDING.md](BUILDING.md), [PORTING_WATCHLIST.md](PORTING_WATCHLIST.md) §2/§5/§9 and
[NATIVE_SOURCE_PORT_PLAN.md](NATIVE_SOURCE_PORT_PLAN.md) §6.8.1.

## Rules (owner)

- Never attribute AI in commits (no `Co-authored-by`, no tool names). Committing without attribution is allowed.
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
  `Platform_WatchWrites`, null-call recovery), `--speed`.
- After it: `Render::QueueSprite`, `QueueSpriteWithTexturePage`, `QueueDefaultSprite`, `LoadFrameViewMatrix`
  hand-ported from inline assembly (owned, `Native\Engine\src\Render\Sprites.c`). This fixed the
  `Battle_DrawBattleHud` crash (a stub returned null).

## Where it stops now

Run: `tools\test\run_native.ps1 -NoCopy -SkipIntro -Speed 8 -Frames 8000` (the demo starts about tick 5300).
Loads Ashley (SHP/SEQ/WEP/battle SEQs), ZONE020.ZND, MAP213.MPD, runs the game loop, then crashes:

```
access violation reading 0x7FF4061558A0
#0 Actors_ActorTaggedBonePosition   Actors\Skeleton.c:381   (arg1 == 0xFA: Actors_actorRecords[arg0])
#1 Battle_Ai_CreateActorAiState     Battle\Ai\ActorState.c:228
#2 Actors_QueueScriptedActorSpawn   Actors\ActorLoading.c:799
#3 Event_ScriptSpawnActor           Event\ScriptActors.c:89
```

Suspects in `Battle_Ai_CreateActorAiState` (`Battle\Ai\ActorState.c`, decomp `7CD68.c:934`), check in order:

0. **Most likely:** `Battle_Ai_AiActorView` (and `Battle_Ai_AiCreationView`) are generated *views* with PS1
   padding (`reserved0[4]`, `reserved20[16]`, ... then `stats`/`model` pointers). The real object is a
   `Battle_Actor`, whose x64 layout differs (its pointer members are 8 bytes), so `actor->actorId`, `->stats`,
   `->model`, `->aiControlFlags` may read the wrong bytes. Compare member offsets of the view with `Battle_Actor`
   (clang-cl `offsetof`, see PORTING_WATCHLIST §2), and replace the view with `Battle_Actor` (or rebuild the view
   from it) in the copy tool. Audit the other `*View` types over pointer-holding records the same way: it is the
   same class of bug in every one of them.

1. `Actors_actorRecords[id]` may not be set (or set to a stale value) when the AI state is created for a scripted
   spawn: check where the spawn fills `Actors_actorRecords` vs `Battle_actors`, and what `actor->actorId` reads.
   `Platform_WatchWrites(&Actors_actorRecords[id], 8)` will show the writer.
2. `Core_Memory_ClearAlignedWords` (`Engine\src\Core\Memory\Copy.c`) is an unported stub, so the new state is
   never zeroed. Port it (zero `size` bytes, word-aligned), also used by `ActionPlanning.c:463`.
3. `Core_Memory_AllocateFromLowEnd(0x484)` / `ClearAlignedWords(state, 0x484)`: size literal of
   `Battle_Ai_AiCreationView`, which holds pointers (`actor`, `model`, `stats`), so it becomes `sizeof` through a
   FIXES entry.
4. `((int*)Battle_Ai_roomAiState)[10 + i] = (int)actor`: truncates a pointer through a raw offset. Use
   `field_at.py` to find the member and make it a pointer (TYPE_FIXES + FIXES).
5. The cast call `((void (*)(unsigned char, int, void*, int))Actors_ActorTaggedBonePosition)(..., result, 0)` with
   `unsigned short result[4]`: check the real output type (SVECTOR vs VECTOR) so it does not overflow.

## Stubs the attract scenes reach (port next, in this order)

From the log (`[warn] unported:`). Assembly lives in `resources\rood-reverseWin\src\...` (`.s` files) or the
`INCLUDE_ASM` splat output under `resources\rood-reverseWin\build\splat-artifacts\`.

| Stub | Catalog | Assembly | Size |
|---|---|---|---|
| `Core::Memory::ClearAlignedWords` | SLUS | — | tiny |
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

Room/actor geometry will not appear until `DrawActorModel` and the room-geometry functions are ported (the shared
path for every actor and room, not only the attract scene). Raw offsets still in code reached later are listed in
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
