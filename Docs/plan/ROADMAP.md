# Native Source Port Plan: Phases and immediate actions

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md)

## 1. Phases and sub-phases

Every sub-phase has an **exit gate**. A gate is passed only when the listed ledger state is reached *and* the listed behavior is demonstrated.

These are product-level gates. Execute the smaller packages in [§14](WORK_PACKAGES.md#14-implementation-work-packages):
title closure requires M3 plus the title deliverables of M4/M5 and M6; first-room/encounter closure requires M7/M8.
Do not label the entire title slice done because logos or menu captures work. Compatibility retirement proceeds
alongside each slice, not only at Phase 2e.

### Phase 1: Run like the original

| Sub-phase | Scope | Exit gate |
|---|---|---|
| **1a Foundation & catalog** | Solution skeleton; Core, Platform, Content (disc + ISO9660); ledgers; **full naming pass**; final locations assigned; copy tool (since retired, D24); **all game code copied into `Native\` and compiling** with R-rewrites applied and T-shims in place. | `FUNCTION_CATALOG.csv`: 100 % `final_name`, `final_location`, `copied`, `compiles`; 0 rows in `Triage`. Solution builds clean in Debug/Release x64. Unit tests for Core.Math pass. |
| **1b Title slice** | Boot → dev credits (Square logo/legal) → opening music → intro FMV (`MOV\TITLE.STR`, converted to AV1) → title art → title menu **New Game / Continue / Vibration / Sound** with **controller input recognized**. Idle timeout (1000 ticks without input, 100 after the movie) leaves the title for one of two in-engine attract demos, chosen by the title count's parity (22C.c:4560, 4625); the demos are gameplay and run in 1c, and the title (with its movie) returns after them. | Side-by-side capture vs DuckStation reference for each screen. Pad and keyboard both drive the menu. Every function reachable from `vs_title_exec` is `understood`. |
| **1c New Game → first room → first battle** | Opening event (Ashley/Lea Monde intro), field rendering (MPD rooms, ZND zones), actor rendering (SHP/SEQ/WEP), first battle (risk, chains, damage). | Replay of a scripted input file reaches the first save point with identical state hashes to a reference log. |
| **1d Menus, saves, effects** | All MENU0–F modules, workshops, inventory, Save/Load to our own format (+ `.mcd` import), all effect templates and plugins. | Every menu screen reachable; save → load round-trip is lossless; every effect plugin ID renders. |
| **1e Full parity** | All zones, bosses, ending (ENDING.PRG), New Game+. Remaining T-shims reduced as far as possible. | Full playthrough on replays and by hand; `verified` = yes on all rows reachable from gameplay. `CONTENT_LEDGER.csv` 100 % converted + round-trip pass, manifest 0 `unconverted` (ASSET_PIPELINE §9). |

### Phase 2: Enhancement

| Sub-phase | Scope |
|---|---|
| **2a Presentation** | Internal resolution scale, widescreen (with per-scene safe framing), 60 fps interpolation for the 30/20 fps paths, texture filtering options, perspective-correct mapping, no wobble. |
| **2b Audio** | High-quality resampling, optional reverb improvements, per-channel volume, streaming music replacement hooks. |
| **2c QoL & input** | Full remapping, rumble mapping, UI prompts per device, fast text, quick save slots, accessibility options. |
| **2d Developer tooling** | In-game inspector (actors, rooms, items), catalog hot-reload, replay recorder UI, effect previewer. |
| **2e Retirement closure** | Final audit of retirement performed throughout M3–M10: **all T-shims deleted**, **all `.c` promoted to `.cpp`**, all `Legacy\` folders empty and removed. SDK ledger 100 % `[x]`, with no renamed hardware model outside `Legacy`. Any approved retained backend requires an explicit amendment to this gate. |

### Phase 3: Go wild

| Sub-phase | Scope |
|---|---|
| **3a Content expansion** | New weapons, armor, gems, enemies, drops, rooms through the catalogs (IDs are strings, no 255 cap). |
| **3b Scripting** | Event scripting exposed as data plus a small, sandboxed script layer for new behaviors. |
| **3c Editors** | Catalog editor, room/door/key editor, effect editor (built on Phase 2d tools). |
| **3d Mods** | Patch-layer mod packages with load order, validation and conflict reports ([§9.6](DATA_CONTRACTS.md#96-patch-layers-mods)). |

## 13. Immediate actions

### 13.2 Next executable work

1. **M0:** refresh the baseline and exception inventory; resolve or explicitly defer the decisions in §13.3.
2. **M1:** validate module data/dispatch bounds and failure handling for the boot/title paths before widening coverage.
3. **M2:** extend existing scripted captures into reproducible checkpoint/state verification.
4. **M3:** close interactive title/input/reset behavior, recording remaining graphics/audio/movie dependencies.
5. Deliver **M4–M6** against that baseline; close their title gates before claiming full Phase 1b parity.
6. Continue **M7–M10**, retiring compatibility per slice rather than postponing it to Phase 2e.

### 13.3 Open questions
- ~~**Q1** Renderer backend~~: **SDL3 GPU**, optional direct D3D12 later behind `Engine\Render` (D14).
- ~~**Q2** Contribute renames upstream~~: **No** (owner, 2026-10-15). Renames live only in our ledgers and `Native\`.
- ~~**Q3** Enemy data location~~: **Answered.** ZND (records), ZUD (models), MPD (spawns), MON.BIN (bestiary); see [ENEMY_DATA_SCHEME.md (archived)](https://github.com/JEschete/VS-NativePort/blob/0e3ae7d8df169bc684c8155fa44b90e731bb4e69/Docs/ENEMY_DATA_SCHEME.md).
- **Q4 Compatibility renderer lifetime:** reconcile the page-image decision recorded in `PageSpace.hpp` with
  D1/D8 and §2.2. Working assumption: retain it for comparison while replacing callers, then retire it from the shipping
  path. An optional fidelity backend needs explicit approval, isolated dependencies and its own verification coverage.
- **Q5 Audio backend:** resolved by D23 (sfizz with SFZ+WAV instruments driven by the existing AKAO sequencer).
  M5 still defines the supported fidelity rules and unsupported modes; no competing synthesis backend is authorized.
- **Q6 Save encoding and migration:** the initial native-only milestone uses versioned JSON with semantic
  Game-owned fields, strict validation and explicit version handling. Engine storage remains payload-agnostic.
  Supported PS1 import variants and migration fixtures remain undecided and block import implementation,
  not the native-only milestone; no memory-card-image runtime or native-struct serialization is authorized.
- **Q7 Reference evidence:** select extractable canonical fields and reference checkpoints in M2. Do not assume a
  comparable original-game state hash already exists because the roadmap names it.
