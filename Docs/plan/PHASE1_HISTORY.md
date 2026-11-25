# Native Source Port Plan: Phase 1a workflow and 1b record (history)

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md)

## 5. Phase 1a workflow (in order)

History, not current proof that a subsystem runs correctly. Phase 1a built the solution, named every function and
copied all game code into `Native\`. The copier and its regeneration workflow were retired by D24 (2026-11-18):
generated files are edited in place and keep their `// decomp:` provenance. Current work is ordered by §14.

### Step 1: Baseline ledgers (**done**)

Ran `python tools\ledger\build_ledgers.py` and committed the output. Legacy; never rerun it (§4).

### Step 2: Solution skeleton

Done: `VagrantStory.Native.sln`, `Directory.Build.props` and the projects of §3, vendored SDL3, a `VS.Desktop` window
that pumps input, and the CI scripts (`build-native.cmd`, `test-native.cmd`, boundary lint).

### Step 3: Naming & understanding pass (parallel subagents)

Done (D10). Batches of about 150–250 functions per overlay/source file (BATTLE.PRG split by source file and call-graph
clusters) ran in the order SLUS (shared engine), TITLE/MAINMENU/MENU*, BATTLE, EFFECT, then ENDING and the rest. Each
agent read the decomp source, catalog callers/callees, DataCrystal (`resources\DataCrystal`), GodHands, vstrack, the
Unity VS parser and Ghidra for asm-only functions, and returned `final_name`, `purpose`, `final_location`, struct-field
names and confidence. One merge script applied the results and re-queued duplicate names and low-confidence rows.
**Gate:** every row has `final_name` + `final_location` + `purpose`; `Triage` = 0. The record is in
[NAMING_CONVENTIONS.md](../NAMING_CONVENTIONS.md) §11; runtime tracing of the remaining low-confidence rows is #347.

### Step 4: Copy tool (`tools\port\copy_to_native.py`)

Retired by D24: its decomp input was removed, so it can no longer regenerate `Native\`; do not rerun or revive it. Per
row it extracted the function body, renamed symbols by reference (libclang `tools\port\ref_index.py`), applied the
**R** rewrites (`gte_*`/`RotTransPers` → `VS::Math`, `PadRead` → `VS::Input::Pad`, `memcpy`/`memset`/`strcpy` →
`std::`, `rand` → `VS::Core::Random`, `VSync` → `FrameClock`, `getScratchAddr(n)` → owned scratch state), deleted
**D** calls, left **T** calls to `Legacy\` shims, wrote the result with a provenance comment and marked `copied`.
Effect template instantiations (`copy = template`) share one body per template (`Effects\Templates\<Name>.inl`); each
plugin instance is a block that binds its parameters (§6.8). Hand ownership went through `tools\port\own.py`,
`owned.csv` and `copy_manifest.json`; fixes now go directly into the files.

What the copy left in the tree (still current):
- **Declaration differences:** where decomp files declared their own view of a global or prototype, uses go through a
  cast to that declaration or a `ret (*)()` call. Both preserve what the original compiled; they are modernization
  targets.
- **Legacy SDK layer** (`Native\Engine\Legacy\`, `Native\Content\Legacy\`, umbrella `Legacy/Psx.hpp`): PS1 type
  layouts, packet macros and prototypes for the SDK names in use. libgte became `Core/Gte.hpp` (EC-03), `rand`/`srand`
  the BIOS LCG (`Core/RandomLive.hpp`); the scratch block was retired by #45 (§6.8.2). T shims record
  `VS::Core::RecordUnported` until they translate into their system.
- **Disc-derived constants:** LBAs/sizes in `Content/DiscLayout.hpp` (`VS::Content::disc`) and string-table IDs in
  `Content/StringTables.hpp` (`VS::Content::text`, named per overlay), hand-maintained since #441.
- **Module reload** ([PORTING_WATCHLIST.md](../PORTING_WATCHLIST.md) §1): function-local statics with an initial value
  are hoisted to file scope; per-file resets and one `Boot_Reset<Overlay>Statics()` per overlay
  (`Boot/OverlayReset.cpp`) restore initialised statics.
- **Assembly-only functions** are `VS_UNPORTED` stubs with their real prototypes (PORTING_WATCHLIST.md §1, §5, §6).
- **Link check:** VS.Tests links the libraries in full (`/WHOLEARCHIVE`), so an unresolved or duplicate symbol anywhere
  fails the build even before anything calls it.

### Step 5: Data disposition and asset extraction

Fill in `disposition` in the data ledger and generate the Tier 1 table decoders for `catalog` rows. Convert
`constexpr` rows. Fold `state` rows into system state structs. `VS.Extract` and `CONTENT_LEDGER.csv` convert disc
files in the order the slices need them; every disc file is converted up front by Phase 1 exit, with per-sub-phase
asset gates in [ASSET_PIPELINE.md](../ASSET_PIPELINE.md) §9.

`VS.Extract` stays C++ (owner, 2026-10-17). Its decoders are ported from VS-Workshop's `Workshop.Core`
(`D:\Dev\Modding\VS-Workshop`), which decodes BIN/CUE, ISO9660, MPD/ZND/SHP/WEP/ZUD/SEQ/ESQ/TIM, palettes and P-file
effects. AKAO, XA and STR converters are ours to write ([PORTING_WATCHLIST.md](../PORTING_WATCHLIST.md) §7).

### Step 6: Compile everything

Done (2026-10-17): the solution builds in Debug and Release x64 and every copied row is marked `compiles`. Compilation
does not prove x64 runtime safety; M1/EC-04 validates the remaining data and pointer assumptions before their consumers
run. Destination fixes:

| Hazard | Fix |
|---|---|
| pointer casts | proper types |
| 24-bit OT links | sort keys |
| KSEG literals | handles |
| inline asm | C++ |

Before 1b the copy fixed hazards that break at run time regardless of modernization order (PORTING_WATCHLIST.md §2):
ordering-table links are 24-bit handles, pointer values stored in integers land in `uintptr_t` where the destination is
a member, global or local, the game heap is a native arena, struct-size literals of pointer structs are `sizeof`, and
every pointer-free type is checked against its PS1 layout. Remaining pointer/integer warnings (returns, parameters,
stores through casts, on-disc relocations) are fixed as each function is modernized (owner, 2026-10-17).

### Step 7: Gate review

Confirm that the ledger percentages match §1, that the CI lint is clean and that the tests are green.

### Phase 1b status (as built, 2026-10-17)

History; current state is §0.2, §14 and §15. The boot code (`VS::Boot::RunLiveGame`, the session from
`vs_main_exec`) runs on a game fiber (`Platform::GameFiber`): the game's wait for the next frame hands control to the
host loop, which feeds the pad, resumes the game once per tick and presents the page space. The loading screen, logos,
intro movie with soundtrack, title art and menu ring were compared with DuckStation captures, and the functions
reachable from `vs_title_exec` passed `tools\port\understood.py`.

What the slice needed beyond the shims, still in place:
- **Data:** `Content::DataStore` reads `Native\data` (D20); the disc drive (`Content\src\DiscReader.cpp`) serves the
  copied CD queue from it. Generated `Game\src\Boot\ModuleData.cpp` fills module-data globals and `ModulePointers`
  relocates pointer tables through a symbol map built from `splat_symbols.csv` (EC-04).
- **Views:** `view-of` globals are macros over their base (element, field or byte offset), so both names reach the same
  storage as on the PS1.
- **x64 and compiler:** the heap unit is the block header's size; overlay slots live in a native block at their
  original offsets (EC-05); copied code is compiled with unsigned `char` (`/J`, as the original `-funsigned-char`).
