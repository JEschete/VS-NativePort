# Vagrant Story: Native Source Port Plan

Status: **Plan v3: implementation and retirement roadmap.** The native-system destination from v2 remains the
target; existing compatibility implementations are documented rather than counted as already modernized.
Target: Windows x64 desktop, built from one Visual Studio solution at the repo root (`VagrantStory.Native.sln`).

**How to use this plan:** this index holds the decision log (§0), the target and baseline (§0.1, §0.2) and the hard
rules (§2); every other section lives in a topic file under [plan/](plan/), listed in the section map below. §0 and §6
define the destination, §14 the executable work packages and acceptance gates, and §15 the EC-01–EC-09
emulation-compatibility register. §5 and §13.1 are history, not proof that a subsystem runs correctly. Code comments
cite "plan §N", "plan section N" or "plan Dn"; resolve them through the map. No runtime verification or owner
approval is implied by this document.

Evidence and history removed in the 2026-11-24 split remain in Git: git show 215cdd4d9:Docs/NATIVE_SOURCE_PORT_PLAN.md.

## Section map

| Sections | File | Topic |
|---|---|---|
| 0, 0.1, 0.2, 2, 2.1, 2.2 | this file | Decision log, "not an emulator" target, inspected baseline, hard rules and shims |
| 1 (Phase 1, Phase 2, Phase 3), 13, 13.1, 13.2, 13.3 | [plan/ROADMAP.md](plan/ROADMAP.md) | Phases and exit gates; foundation record, next work, open questions |
| 3 | [plan/LAYOUT.md](plan/LAYOUT.md) | Solution layout, including the generated project-boundary block |
| 4, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6 | [plan/LEDGERS.md](plan/LEDGERS.md) | Ledgers: function catalog, SDK, retired source-file ledger, data, coverage |
| 5, Step 1–Step 7, Phase 1b status | [plan/PHASE1_HISTORY.md](plan/PHASE1_HISTORY.md) | Phase 1a workflow and the retired copier (history) |
| 6, 6.1, 6.2, 6.3, 6.4, 6.5 | [plan/SYSTEMS.md](plan/SYSTEMS.md) | Per-system designs: Core, Render, Audio, Video, Input |
| 6.6, 6.7, 6.8, 6.8.1, 6.8.2 | [plan/SYSTEMS_2.md](plan/SYSTEMS_2.md) | Content, Save, Game modules, actor assets, state ownership (#45) |
| 7, 8, 8.1, 8.2 | [plan/RETIREMENT_AND_CPP.md](plan/RETIREMENT_AND_CPP.md) | R/T/D retirement strategies; why C++; promotion checklist |
| 9, 9.1–9.9 (each) | [plan/DATA_CONTRACTS.md](plan/DATA_CONTRACTS.md) | Def vs Instance, IDs, catalogs, schema rules, patch layers, determinism |
| 10, 10.1, 11, 12 | [plan/VERIFICATION_AND_RISKS.md](plan/VERIFICATION_AND_RISKS.md) | Verification methods, evidence rules, licensing, risks |
| 14, 14.1, M0, M1, M2, M3, M4 | [plan/WORK_PACKAGES.md](plan/WORK_PACKAGES.md) | Work-package contract, dependency table, M0–M4 |
| M5, M6, M7, M8, M9, M10 | [plan/WORK_PACKAGES_2.md](plan/WORK_PACKAGES_2.md) | Audio, movies, field, combat, menus/saves, full coverage |
| 15, 15.1, 15.2, 15.3, EC-01, EC-02, EC-03 | [plan/EC_REGISTER.md](plan/EC_REGISTER.md) | EC policy, native foundation, issue register; GPU, SPU, GTE |
| EC-04, EC-04 asset T-shim ledger | [plan/EC_REGISTER_2.md](plan/EC_REGISTER_2.md) | Resource addresses, module relocation, asset T-shims |
| EC-05, EC-06, EC-07, EC-08, EC-09, 15.4 | [plan/EC_REGISTER_3.md](plan/EC_REGISTER_3.md) | Overlays, scheduling, controller, movies, saves; tracking and closure |

Companion documents: [TOOLS_INVENTORY.md](TOOLS_INVENTORY.md) (tools and references, with license and reuse class),
[ledgers/SUMMARY.md](ledgers/SUMMARY.md) (current ledger counts), [FUNCTION_CATALOG.csv](ledgers/FUNCTION_CATALOG.csv)
(master per-function list), [SDK_API_LEDGER.md](ledgers/SDK_API_LEDGER.md) (SDK symbols and retirement strategy),
[DATA_LEDGER.csv](ledgers/DATA_LEDGER.csv) (global data) and the archived
[ENEMY_DATA_SCHEME.md](https://github.com/JEschete/VS-NativePort/blob/0e3ae7d8df169bc684c8155fa44b90e731bb4e69/Docs/ENEMY_DATA_SCHEME.md)
(original enemy storage and the Species / EnemyDef / Loot / Spawn scheme). Ledger maintenance rules: [§4](plan/LEDGERS.md).

## 0. Decision log

| # | Decision | Choice | Decided by |
|---|---|---|---|
| D1 | What the port is | A **native source port**: game logic re-homed into modern C++ systems. No emulator, BIOS, guest RAM, MIPS code or stand-in PS1 SDK. | Owner |
| D2 | Role of the decomp | A **read-only reference**: nothing in the solution includes, links or loads anything from `resources\`. Code was *copied* into `Native\` once, then owned there. Since 2026-11-18 the reference is the upstream submodule `resources\rood-reverse` (the pinned `rood-reverseWin` copy was removed in `6b20016f0`) and the copier is retired (D24). | Owner |
| D3 | Matching | **Not a goal.** No byte or register matching and no original memory layout; types may change, IDs widen and content grow. | Owner |
| D4 | Architecture | **x64 only.** Windows is LLP64 (`long` = 4 bytes), so pointers are the only width change. | Owner |
| D5 | Language | **C++23** for everything in `Native\`, with no C or assembly (2026-10-31; enforced by `tools\ledger\gates.py`). Copied code compiles as C++ with relaxed settings (C linkage retired in #441); each item is then **promoted** per [§8](plan/RETIREMENT_AND_CPP.md#82-promotion-checklist-sets-cpp--yes). The original C and assembly stay under `resources\` as reference. | Owner |
| D6 | Layout | Mirrors the DW4 port: root `.sln`, `Native\` with one solution folder per system, versioned schemas, generated assets git-ignored ([§3](plan/LAYOUT.md)). | Owner (agreed) |
| D7 | Phasing | **Phase 1:** run like the original. **Phase 2:** enhancement. **Phase 3:** go wild. Each has sub-phases ([§1](plan/ROADMAP.md)). | Owner |
| D8 | SDK replacement | Every PS1 SDK call is retired by **R** (rewrite at copy), **T** (temporary translator shim, then migrate and delete) or **D** (delete), tracked per symbol in the SDK ledger. Shims translate into the *modern* system API and never model hardware (§2.2). | Owner (agreed, "Adjustment B") |
| D9 | Function placement | A function catalog classifies every function to a system; every function is copied, named, understood and placed, tracked per function ("Adjustment A"). | Owner (agreed) |
| D10 | Naming speed | The naming/understanding pass is done **up front in Phase 1a** with parallel subagents per overlay, not spread across milestones. | Owner |
| D11 | Data contracts | Def/Instance split, string IDs → runtime handles, versioned schemas validated at load, extract-then-patch layers, data for parameters / code for behaviors, composition for actors ([§9](plan/DATA_CONTRACTS.md)). | Owner (agreed) |
| D12 | Dropped references | `psx-spx` and `beetle-psx-libretro` are **not used** and were removed from `tools\`. Hardware documentation is never used to build hardware; at most to understand what a data format means. | Owner |
| D13 | Compiler | **ClangCL** toolset in `.vcxproj` (accepts the GCC extensions in copied code, `/std:c++latest`). MSVC `cl` stays allowed for pure C++ projects. | Proposed |
| D14 | Renderer backend | **SDL3 GPU** (D3D12/Vulkan/Metal; HLSL via SDL_shadercross) behind our own `Engine\Render` API. A direct D3D12 backend may be added later behind the same API if Phase 3 needs ray tracing, mesh shaders or bindless. | Owner (2026-10-15) |
| D15 | Third-party | SDL3 (zlib), vendored. CC0/MIT code may be lifted with attribution (rood-boi device logic, PsyCross GTE math). GPL / non-commercial / unlicensed code is reference only. | Proposed |
| D16 | Upstream contributions | Renames are **not** sent back to rood-reverse. | Owner |
| D17 | Enemy data | Species / EnemyDef (`extends` variants) / Loot / Spawn catalogs, behaviors in code ([ENEMY_DATA_SCHEME.md, archived](https://github.com/JEschete/VS-NativePort/blob/0e3ae7d8df169bc684c8155fa44b90e731bb4e69/Docs/ENEMY_DATA_SCHEME.md)). | Owner asked to modernize; design v1 |
| D18 | Asset pipeline | `VS.Extract` converts **every** disc file **up front** into `Native\data\` (git-ignored); the game never reads the disc. Executable tables → JSON (generated typed decoders), text → UTF-8 string catalog, models → glTF 2.0 (matching `resources\Sydney Losstarot (Dying)\`), rooms → glTF + room JSON, images → PNG, samples → WAV, music/SFX stay **sequenced** (JSON event lists played by our ported sequencer), XA → FLAC, movies → **AV1** in Matroska (ffmpeg at extract time only, dav1d at runtime). See [ASSET_PIPELINE.md](ASSET_PIPELINE.md); packaging refined by D25. | Owner (2026-10-15) |
| D19 | Build instructions | [BUILDING.md](BUILDING.md) is the single source of truth for clone → build → extract → play. Every change to prerequisites, scripts, dependencies, output layout or extraction updates it **in the same commit**; planned steps are marked ⏭ until they land. | Owner (2026-10-15) |
| D20 | 1b data path | **Extract now.** The game reads only VS.Extract output (`Native\data`); files not converted yet are copied as-is and listed `raw` in `files.tsv`, so each converter replaces one form at a time. Module data is filled from the extracted module files at start-up. | Owner (2026-10-17) |
| D21 | Texture/VRAM model for 1b | **Page image:** one 1024×512 16-bit image that uploads, copies and drawing act on with the original semantics (`Render::PageSpace`). Drawing is rasterized on the CPU for parity and the GPU only presents; the §6.2 GPU renderer replaces the rasterizer in 2a. | Owner (2026-10-17) |
| D22 | Movies | **AV1** in Matroska with FLAC, converted by FFmpeg at extract time; dav1d + libwebm + dr_flac at run time. | Owner (2026-10-17) |
| D23 | Instruments | **sfizz** (SFZ + WAV) driven by the ported AKAO sequencer ([§6.3](plan/SYSTEMS.md#63-audio-engineaudio-audioseq)). | Owner (2026-10-17) |
| D24 | C++ headers | Every header becomes a real C++ header (`.hpp`, `#pragma once`, namespaces, C++ linkage, no C idioms); a rename or language override is not enough. The copier is retired (its decomp input was removed in `6b20016f0`): generated files are edited directly, and a migrated generated header leaves `copy_manifest.json`. Tracked in [CPP_HEADERS.md](CPP_HEADERS.md). | Owner (2026-11-18) |
| D25 | Native asset formats | Chosen for **ease of modding** without losing exactness. Stable IDs everywhere; LBAs and offsets are provenance only. Flat gameplay tables are **CSV** (one table per file, references by ID), merged with patch and mod layers and compiled into **SQLite**. Images are **PNG** at true depth plus a `.image.json` sidecar holding only what PNG cannot (STP, provenance, material). Models are `.gltf` + `.bin` + shared PNG. Events, effect programs and layouts are schema-validated JSON. Unknown bytes are kept; valid unused content is converted and marked `unreferenced`. Layout: [ASSET_PIPELINE.md](ASSET_PIPELINE.md) §3, details §1.1. | Owner (2026-11-21) |
| D26 | Semantic native authoring | **ASSET_PIPELINE §4 is the target**, not positional/filename IDs with labels added: semantic definition IDs, readable text, schema-backed named enums/flags, meaningful column families and named instruction/track fields. Original IDs/bytes stay only as explicit compatibility/provenance; do not silently merge variants or guess unknown meanings. Early `display_name` metadata is preparation, not completion. Apply the readability gate in [ASSET_PIPELINE.md](ASSET_PIPELINE.md) §7.2 to existing and future formats; #520 tracks it. | Owner (2026-11-23: "we are going to go with §4") |

### 0.1 Target meaning of "not an emulator"

The decision log records the intended end state, not a claim that today's runtime meets every rule. The current
page-image renderer, SPU memory/register blocks and original-address adapters are deviations described in
[§15](plan/EC_REGISTER.md#15-emulation-compatibility-register). Preserve the retirement goal unless an explicit
decision changes it; [§13.3](plan/ROADMAP.md#133-open-questions) lists the unresolved choices.

| Excluded from the target shipping runtime | Replaced by |
|---|---|
| A byte array representing PS1 RAM, scratchpad, VRAM or SPU RAM | Typed C++ objects owned by each system |
| Addresses such as `0x80xxxxxx`, `0x1F80xxxx` or KSEG literals | Pointers, references, handles |
| Ordering tables, GPU packets, `DR_*` primitives as the render API | `RenderQueue` with sort keys and typed draw items |
| GTE registers or `gte_*` macros | `VS::Math` fixed-point vectors/matrices (bit-accurate where gameplay depends on it) |
| SPU voices addressed by register, SPU RAM transfers | `VS::Audio` voices, banks, mixer |
| CD sectors, LBAs, `CdlLOC`, `DsRead` callbacks | `VS::Content` file IDs, async loader, decoded assets |
| `VSync()` / vblank callbacks / root counters | `VS::Core::FrameClock` fixed-step tick (Phase 1: 60 Hz field timing reproduced) |
| Overlay loading of `*.PRG` files | Statically linked modules with explicit `Init/Reset/Update` |
| `StoreImage` / `MoveImage` readback of "VRAM" | `Engine\Render` render-target capture API (§6.2) |

### 0.2 Inspected baseline and two progress tracks

The baseline is a source snapshot taken for the compatibility audit (now §15), not a fresh build/test result.
Refresh it when closing a package; do not copy old log results into a new gate.

| Area | Implemented foundation | Still not established by that foundation |
|---|---|---|
| Execution/platform | `Boot::RunSession` runs boot, title, gameplay and ending modes on a cancellable fiber that carries exceptions to the host; SDL event loop, crash diagnostics | Full title/gameplay parity |
| Graphics | GP0/ordering-table compatibility renderer, SDL GPU presentation | Typed native draw queue and geometry/material path |
| Input/verification | Native pad state, `--press`/`--scripted`, bounded captures | Complete replay, canonical state comparison, legacy pad-view retirement |
| Content | Extracted files, asset/audio catalogs, module data/pointer initialization | Validated relocations, typed assets, disc-independent startup |
| Audio | SDL output, copied sequencer, sfizz voices, SPU-shaped transfers/registers | Reference-verified output, register-free voice API |
| Movie/save | AV1/FLAC movie decoding; native schema3 save/load | A/V parity; imports and migration |

Track **behavior** (`implemented → integrated → reference-verified`) separately from **architecture**
(`copied → compatibility-backed → native API → legacy dependency retired`). A compatibility-backed screen can
be verified without being modernized. A new API or `.cpp` file can be modern-looking without verified behavior.
Original RNG, fixed-point rules, event scripts and AKAO sequencing are game semantics, not automatically emulation.

## 2. Hard rules

### 2.1 Code rules

1. **No `#include` into `resources\`.** A CI check greps every `.vcxproj` and source file for `resources` and `rood-reverse`.
2. **No guest addresses in modernized runtime code.** Boundary lint checks address/scratchpad patterns including
   `0x80[0-9A-F]{6}`, `0x1F80`, `0xA0[0-9A-F]{6}`, `getScratchAddr` and `D_80`. M0 distinguishes provenance,
   extraction-only source metadata, non-address constants and the exact existing compatibility exceptions. Each
   runtime exception needs an EC issue and retirement package; never disable the check for an entire project.
3. **Provenance comments.** Every copied function carries one line:
   `// decomp: BATTLE.PRG func_8006B57C (src/BATTLE/BATTLE.PRG/…c:123)`; the ledger links back the same way.
4. **No raw struct I/O in the target runtime.** Every disc format has an explicit decoder that produces typed
   objects (§6.6). Existing module-image copies are EC-04 bridge work, not compliance. Original layout offsets may be
   used by extraction decoders, never assumed equal to native pointer-bearing layouts.
5. **System ownership.** A system's state lives in that system. Cross-system calls go through its public header (`Native\Engine\include\<System>\…`, `Native\Game\include\<Module>\…`, `Native\Content\include\Content\…`), enforced by lint inside one project.
6. **Warnings are errors** for promoted code. Copied code gets a relaxed warning set that is tracked and tightened on promotion.

### 2.2 Transitional shims (strategy T)

Shims make copied code run *before* every call site is rewritten. These rules describe the desired boundary; §15
records existing departures, including hardware-shaped helpers outside `Legacy`, without approving their retention.

- They live only in `Native\Engine\Legacy\<System>\` (or `Native\Content\Legacy\`) and are named after the SDK function.
- They **translate** into the modern system API (e.g. `AddPrim(ot, polyFT4)` → `RenderQueue::Submit(SortKey{layer, z}, TexturedQuad{…})`) and never keep a hardware model (no VRAM array, no OT walk at draw time, no SPU RAM).
- Each shim symbol has a row in `SDK_API_LEDGER.md` with its live use count; at 0 the shim is deleted and the row goes `[x]`.
- New code may never call a shim. A CI check counts shim uses, and the count may only go down.

New native gameplay code uses the public system API. Bridge repairs needed to unblock a verified slice must identify
their EC issue, affected consumers, tests and removal package; any temporary increase requires explicit review and
must not be hidden by renaming SDK calls. Missing behavior is classified as required or optional per checkpoint:
required data/dispatch failure stops before use, while an optional omission is reported and cannot pass its feature gate.
