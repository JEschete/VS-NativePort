# Naming & Placement Conventions (Phase 1a, Step 3)

This is the contract for the naming pass. Every row in `Docs\ledgers\FUNCTION_CATALOG.csv` with `copy = yes` gets a
`final_name`, a `final_location` and a `purpose`, following the rules below. `tools\naming\merge_names.py` enforces
the mechanical rules. The judgement calls (good names, sensible grouping) are on the author.

The goal is **readable, modern, organized source**. The port does not keep byte or register parity, and these names
are the ones the code will carry for the rest of the project.

---

## 1. `final_name`

**Format:** `Namespace::Name`, where `Namespace` is the system or module (optionally one sub-namespace) relative to
`VS::`.

| Part | Rule | Examples |
|---|---|---|
| Namespace | One of the roots in §3, optionally plus one PascalCase sub-namespace | `Battle`, `Battle::Ai`, `Audio::Seq`, `Render`, `Menu::Status` |
| Name | PascalCase, verb first for actions, noun for getters/queries | `ApplyDamage`, `UpdateRiskGauge`, `DrawWindowFrame`, `IsActorDead`, `FindItemSlot` |
| Length | ≤ 40 characters for `Name`; no abbreviations except the glossary ones (§6) | `CalcChainDamage`, not `CalcChnDmg` |
| Forbidden | `func`, `sub`, `unk`, `Unknown`, `Helper`, `Do`, `Process` alone, address digits, `vs_` prefixes | |

**C spelling (used while the code is still `.c`):** replace `::` with `_`. For example `Battle::Ai::PickTarget`
becomes `Battle_Ai_PickTarget`. The C spelling must be **unique across the whole port**, because every static library
links into one executable.

**Already-named functions** (`vs_main_*`, `vs_battle_*`, `_static` helpers, …) are converted to this convention, and
the name is improved if the decomp name is wrong or vague. Example: `vs_main_getRand` → `Core::Rng::Next` (§3 says it
is replaced by `VS::Core::Random`, so note `replaced-by:Core::Random::Next`).

**Duplicates:** if two rows have identical code (for example the same helper in several MENU overlays, or the same
function in two plugins), give them the **same** `final_name` and `final_location`. Put `dup-of:<id of first row>` in
`notes` on every row except the first. That code is then copied once.

- A `dup-of` collapses to **one native copy with one set of state**. Without overlays, two overlays' private
  file-static state would silently merge. So identical code that touches **mutable file-static state** in different
  overlays is *not* a `dup-of`. It gets its own name (e.g. the TITLE screen's copy of the MENU7 file menu is
  `Title::SaveFiles::*` / `Title::SaveCard::*`) and a `variant-of:<id>` note so Phase 2 can consolidate it
  deliberately.
- `tools\naming\resolve_dups.py` finds every cross-batch C-name collision and compares bodies (from the libclang
  index, with callees resolved to final names and locals canonicalized). Identical, stateless bodies get `dup-of`
  automatically. Every other collision needs a reviewed row in `tools\naming\dup_decisions.csv` (`dup` when only the
  spelling differs, e.g. `0x1000` vs `ONE`; `rename` otherwise). The script exits 1 while any collision is undecided.

**Methods:** Phase 1a code is C, so use free functions. Where a function clearly operates on one object, choose a
namespace that will become the class later (e.g. `Battle::RiskGauge::Update`). The C++ promotion (plan §8.2) then
turns the namespace into a class without renaming call sites.

## 2. `final_location`

**Format:** `Native\<ProjectDir>\src\<Area>\<File>.c`, Windows separators. Files are copied as `.c` and promoted
to `.cpp` later.

- `<ProjectDir>` and the allowed `<Area>` roots are listed in §3.
- `<File>` is a PascalCase feature noun: `Damage.c`, `RiskGauge.c`, `WindowFrame.c`, `SoundDriver.c`.
- At most **one** extra sub-folder below the module (e.g. `Native\Game\src\Battle\Ai\Targeting.c`).
- Aim for **10–60 functions per file**, grouped by feature. One large function may have its own file. Do not create
  a file per function, and do not create a 400-function dumping ground.
- The namespace in `final_name` and the folder path must agree. Either the sub-namespaces equal the folders below
  the root (`Battle::Ai::*` lives in `Game\src\Battle\Ai\*.c`), or the sub-namespace is the file name
  (`Menu::Files::*` lives in `Game\src\Menu\Files.c`, `Effects::Plg074::*` in `Game\src\Effects\Plugins\Plg074.c`,
  one grouping folder allowed). Root-namespace functions live directly in the root folder. `merge_names.py` enforces
  this.
- One concept, one namespace. Don't create near-synonyms (`Seq` / `Sequence` / `Driver\Sequencer`,
  `Random.c` / `Random\Generator.c`). `tools\naming\consolidate.py` holds the reviewed merges from the first pass.
- **Never** `Native\Triage\…`. If you truly cannot place a function, pick the best guess and set confidence `low`.

## 3. Systems and modules (namespace → location)

| Namespace root | Location root | What goes there |
|---|---|---|
| `Core` | `Native\Core\src\` | Fixed-point/vector/matrix math (GTE replacements), RNG, memory/heap helpers, string utils, generic containers |
| `Render` | `Native\Graphics\src\` | Generic rendering: primitive building, model/mesh drawing (SHP/WEP), texture/CLUT upload, sprite and font/glyph drawing, screen effects, camera projection, display setup |
| `Audio` | `Native\Audio\src\` | Sound driver, SEQ sequencer, SFX playback, voice allocation, music control, XA/streamed audio control |
| `Content` | `Native\Content\src\` | File loading, CD read queue, file tables, archive/format decoders, decompression |
| `Input` | `Native\Input\src\` | Pad processing, button repeat, vibration |
| `Movie` | `Native\Video\src\` | FMV/STR playback, MDEC decode |
| `Save` | `Native\Save\src\` | Memory card I/O, save/load serialization, save menu data handling (not the menu UI) |
| `Boot` | `Native\Game\src\Boot\` | Startup, main loop, mode switching (title ⇄ game ⇄ battle ⇄ menu), reset |
| `Title` | `Native\Game\src\Title\` | Dev credits, intro movie control, title screen, title menu, attract/demo |
| `Field` | `Native\Game\src\Field\` | Rooms/zones (MPD/ZND/ZUD), map loading, collision, doors, traps, camera control, minimap, room transitions |
| `Battle` | `Native\Game\src\Battle\` | Combat: targeting sphere, damage, risk, chains, defense, break arts, spells, status effects, enemy AI (`Battle::Ai`), battle HUD |
| `Actors` | `Native\Game\src\Actors\` | Actor lifecycle, Ashley/NPC/enemy objects, animation (SEQ) state, movement, equipment visuals |
| `Items` | `Native\Game\src\Items\` | Inventory, equipment stats, workshop combine/repair, item use, loot/chests |
| `Menu` | `Native\Game\src\Menu\` | Menu framework (windows, cursors, text boxes) and every menu screen: `Menu::Status`, `Menu::Inventory`, `Menu::Map`, `Menu::Options`, … |
| `Event` | `Native\Game\src\Event\` | Event/script VM, cutscene commands, dialog boxes, scripted cameras |
| `Effects` | `Native\Game\src\Effects\` | Visual effect framework, plugin (PLG*.BIN) effects, particles, effect templates |
| `Ending` | `Native\Game\src\Ending\` | Ending sequence and staff roll |
| `State` | `Native\Game\src\State\` | Global game state: flags, timers, play time, score, party/progress data |

**Rendering split:** generic "how to draw" code belongs to `Render`. Game-specific "what to draw" code (the risk
gauge HUD, menu windows) stays in its game module and calls `Render`.

## 4. `purpose`

- One sentence, present tense, ≤ 160 characters: **what it does, and for whom/when**.
  - Good: "Applies a chain-ability bonus hit to the current target and queues its damage numbers."
  - Bad: "Does stuff with actor", "Unknown", "Helper function".
- Mention the original concept where useful (e.g. "Risk", "Break Art", "ZUD").
- If you are unsure, write your best inference and set confidence to `med` or `low`. Do not leave it blank.

## 5. Output format (per batch)

The output is a UTF-8 CSV at `tools\naming\out\<batch>.csv` with this exact header:

```
id,final_name,final_location,purpose,confidence,notes
```

- **One row per input row.** Same `id`s, none missing, none extra.
- `confidence`: `high` (behaviour clear from code/callers), `med` (good inference), `low` (guess).
- `notes` (optional, `;`-separated tokens):
  - `dup-of:F01234`
  - `replaced-by:<native API>` (functions superseded by an existing native system, e.g. RNG, pad edge detection)
  - `fields:<Type>.<field>=<meaning>` (struct-field names you worked out)
  - `asm` (named from assembly)
  - free text

## 6. Glossary (use these words consistently)

| Term | Meaning |
|---|---|
| Ashley | Ashley Riot, the (male) player character |
| Actor | Any character in a room (Ashley, NPCs, enemies) |
| Risk | Risk meter. Rises with attacks and lowers accuracy/defense |
| Chain / ChainAbility | Timed follow-up hits during an attack |
| DefenseAbility | Timed defensive reactions |
| BreakArt | Weapon-specific special attacks paid for with HP |
| BattleAbility | Learned abilities (chain/defense) |
| Spell / Grimoire | Magic and the books that teach it |
| Sphere | The targeting range sphere shown in battle mode |
| BattleMode | Weapon-drawn mode (the "battle" overlay is also the field engine) |
| Zone / Room | Map hierarchy: ZND (zone data), MPD (room geometry/scripts), ZUD (unit/model data) |
| SHP / SEQ / WEP | Character model / animation sequence / weapon model formats |
| DP / PP | Weapon Damage Points / Phantom Points |
| Affinity / Class | Weapon class-and-element affinities |
| Workshop | Forge/combine/repair locations |
| Gimmick | Interactive room objects (crates, switches, traps) |
| Plugin (PLG) | Effect overlay loaded per spell/art (`EFFECT\PLGnnn.BIN`) |
| OT | Ordering table (draw-order list) on PS1. Natively this is a sort key |

Abbreviations allowed in names: `Hp`, `Mp`, `Dp`, `Pp`, `Ui`, `Hud`, `Fx`, `Rgb`, `Vram`, `Clut`, `Xa`, `Fmv`,
`Ai`, `Id`, `Pos`, `Rot`, `Vec`, `Mat`.

## 7. Sources to consult

- The decomp (read-only): `resources\rood-reverseWin\src\…`, plus `include\` for struct definitions.
- DataCrystal pages: `resources\DataCrystal\` (formats, lists, mechanics, executables).
- GodHands: `tools\PSX\VagrantStory\GodHands`, plus the other VS tools under `tools\PSX\VagrantStory\`.
- Callers and callees as listed in the batch file.
- Upstream pull-request history of `ser-pounce/rood-reverse` (read-only; never post). `tools\naming\upstream_prs.py`
  caches PR titles and bodies in `build\port\upstream_prs.json`, and `tools\naming\evidence.py --upstream N` prints
  PR sentences that mention a symbol, type or referencing function by exact name. PR text is a **hint about where
  to look**, never evidence by itself: a row still needs a cited source line. PRs whose text says they were
  written with an AI model are tagged `[AI-declared]` (all of noprompt's PRs say so) and deserve the same scrutiny
  as our own agents' output; e.g. PR#206 calls `func_8007D734` a map/compass overlay, but its five 64-pixel
  sprite strips cover the full 320-pixel screen (`146C.c:9385`). Maintainer review threads are fetched on demand
  through the GitHub MCP server when a symbol is ambiguous. The cache also holds the PR review and conversation
  comments (957 as of #251); refresh it after each upstream update.
- Upstream main, `resources\rood-reverse` (read-only). The pinned reference `rood-reverseWin` is noprompt's fork at
  `062096da` (`upstream.json`); upstream main merges that work in reviewed batches, so its struct layouts are the
  maintainer-accepted ones. **Layouts are adopted from upstream where it is finer** (decision 2026-10-17):
  `tools\naming\upstream_layout.py` compares every type in the two decomp indexes and merges per byte range (the
  finer side wins; on equal members upstream C types win and the more meaningful name stays). Adopted types are listed
  in `Docs\ledgers\UPSTREAM_LAYOUTS.csv`, and `tools\ledger\build_symbol_ledgers.py` emits their fields from the merge.
  A field that exists only upstream carries the hint `layout:upstream` and is cited from upstream source as
  `source:upstream:<path>:<line>`; `evidence.py` prints those lines. Each adopted row lists the pinned functions
  that touch replaced or retyped members; the copy step must adapt them (take upstream's reviewed body where it
  exists, e.g. `func_800DD344`; otherwise edit the pinned body to the merged layout).
- Upstream `config\**\symbol_addrs.txt` adds nothing over the pinned reference: the two trees agree on all
  ~2,340 address names (upstream adds only the SDK's `DS_close`); new names arrive through C declarations.

---

# Part 2: Globals, types and fields

The second naming pass covers the data. `tools\ledger\build_symbol_ledgers.py` generates three ledgers from the
libclang index, and the rules below apply to them. `tools\naming\merge_symbols.py` enforces the mechanical rules.

| Ledger | Row | Rows | What the pass fills |
|---|---|---|---|
| `DATA_LEDGER.csv` | `G#####`, one per **storage identity** (see §8.1) | 2,174 | `disposition`, `final_name`, `final_location`, `purpose`, `confidence`, `notes` |
| `TYPE_LEDGER.csv` | `T#####`, one per distinct type layout | 822 | `role`, `final_name`, `final_location`, `purpose`, `confidence`, `notes` |
| `FIELD_LEDGER.csv` | `T#####.NNN`, one per struct/union field or enumerator | 6,870 | `final_name`, `meaning`, `confidence`, `notes` |

Every row gets a name, including ones that already look fine (`vs_main_*`, `_portInfo`): they are converted to these
conventions the same way functions were.

## 8. Globals (`DATA_LEDGER.csv`)

### 8.1 Identity

The ledger key is the storage, not the spelling: `name@overlay` (an overlay's extern), `name@file` (file `static`),
`name@file:function` (function-local `static`) and `name@header` with `instances` (a table defined in a shared
header and instantiated once per including overlay). Two rows that share a decomp name are **different storage** and
get different final names unless they are proven identical constant data (then `dup-of:G#####`, as for functions).

### 8.2 `disposition`

Choose one (plan §4.4). The generator's `proposed_disposition` is a size/segment heuristic only; check it.

| Disposition | Use when | Name form |
|---|---|---|
| `catalog` | Game content: item/weapon/enemy/spell stats, text, effect parameters, anything a modder would want to edit | lowerCamelCase plural noun: `Items::bladeDefs`, `Battle::breakArtDefs`. Add `catalog:<name>` (the JSON catalog file, e.g. `catalog:blades`) to `notes` |
| `constexpr` | Engine constants: sin/cos tables, layout coordinates, colour tables, fixed lookup tables | `k` + PascalCase: `Core::kSinTable`, `Menu::kWindowFrameColors` |
| `state` | Mutable runtime state | lowerCamelCase noun: `Battle::riskGauge`, `Title::cursorIndex`. Add `state:<Struct>` naming the system state struct it will fold into (e.g. `state:Battle::BattleState`) |
| `asset` | Images, CLUTs, fonts, sound or model data embedded in the executable/overlays | lowerCamelCase: `Title::titleScreenBg`. Add `asset:<path under Native\data>` |
| `drop` | PS1-only: ordering tables, primitive packet pools, DMA/GPU/SPU buffers, CD sector buffers, the scratchpad, SDK work areas | Still give a lowerCamelCase name (the copied code needs one until the shim removing it lands) and add `replaced-by:<native API>` |

### 8.3 `final_name`

- Same `Namespace::name` form and namespace roots as §1/§3. Variables are **lowerCamelCase** (constants `kPascalCase`),
  so they can never collide with PascalCase functions.
- C spelling: `::` → `_` (`Battle::riskGauge` → `Battle_riskGauge`). It must be unique across the port for every
  `extern`, `file-static` and `per-instance` row. A `local-static` only has to be unique within its function, and its
  namespace is that function's namespace.
- `per-instance` rows name the template once. Natively each including plugin owns its copy; if the data is mutable,
  the copy tool prefixes the instance namespace (`Effects::Plg149::ringVertices`).
- Forbidden: `D_`, `B_`, address digits, `unk`, `temp`, `data`/`buf`/`table` on their own, Hungarian `g_`/`s_`
  prefixes, `vs_`.
- Units belong in the name when they are not obvious: `fadeTimerFrames`, `cameraDistanceFixed12`.

### 8.4 `final_location`

The `.c` file (same rules as §2) where the definition lives: normally the file that holds most of its referencing
functions (the batch lists them). `state` rows go to the file of the system that owns the state; shared engine state
used by several systems goes to that system's `State.c` (e.g. `Native\Game\src\Battle\State.c`).

## 9. Types (`TYPE_LEDGER.csv`)

### 9.1 `role`

| Role | Meaning | Name form |
|---|---|---|
| `Def` | Immutable content definition (one per item/enemy/spell kind) | `BladeDef`, `EnemyDef`, `SpellDef` |
| `Instance` | Mutable per-object data that refers to a Def | `BladeInstance`, `Actor`, `InventorySlot` |
| `State` | A system's runtime state block | `BattleState`, `TitleMenuState` |
| `OnDisc` | Exact layout of bytes on the disc / in a file (decoder input only) | `ZndEnemyRecord`, `MpdHeader`, `SeqHeader` (`Record`/`Header` suffix) |
| `Packet` | PS1 GPU/SPU/CD packet or primitive layout | `PolyFt4Packet`. These are replaced by the backend, so add `replaced-by:` |
| `Enum` | Enumeration | Singular PascalCase: `ItemCategory`, `Element` |
| `Value` | Small value types: vectors, colours, rects, fixed-point, ids | `Vec3s`, `Rgb8`, `RectS16` |
| `Misc` | Anything else (callbacks tables, script contexts) | Descriptive PascalCase noun |

### 9.2 `final_name` and `final_location`

- `Namespace::PascalCase` (`Battle::ActionState`, `Items::BladeDef`). C spelling `Battle_ActionState`, unique across
  the port. `Value` types normally live in `Core` (`Core::Vec3s`).
- `final_location` is the **header** that will declare it: `Native\<ProjectDir>\include\<Area>\<File>.h` for types
  used across projects, or `Native\<ProjectDir>\src\<Area>\<File>.h` for types private to one module.
- Rows with `layouts > 1` share a decomp name but have different layouts. Give each its own name, unless one is a
  partial view of the other (then `same-as:T#####` in `notes`, and the merged layout is resolved when copying).
- The generic decomp names (`D_800F19CC_t`, `func_8006B57C_t`, `unk_t`) must all go. The `aliases` column lists the
  folded typedef spellings, which all map to the one final name.
- **Fork-only view types.** About 176 types exist only in the pinned fork, most of them per-unit views over one
  shared struct (e.g. `actionReactionState` where upstream's reviewed `func_800DD344` takes `func_800E78F4_t*`;
  #247: "per-unit views ... are not brought over"). They are named like any other type, **but only where the cited
  code supports a meaningful name**; a view whose uses do not explain it keeps no invented meaning (name it after the
  struct it views, with `same-as:`/`view-of` notes, or leave its fields `reserved<Off>` at `low`). List each
  view's canonical struct in `notes` so the copy step can fold views back into it.

## 10. Fields and enumerators (`FIELD_LEDGER.csv`)

- Struct/union fields: **lowerCamelCase** nouns (`currentHp`, `riskLevel`, `animationId`). Booleans read as
  predicates (`isVisible`, `hasShield`). Counts and indexes say so (`itemCount`, `slotIndex`).
- Enumerators: **PascalCase** without the enum prefix (`ItemCategory::Blade`); the C spelling is generated
  (`ItemCategory_Blade`).
- `unk*` is forbidden. If the code never reads a field and nothing explains it, name it `reserved<OffsetHex>`
  (`reserved1C`) with confidence `low`; padding the compiler would insert is `pad<OffsetHex>`. A field that is read
  but only copied somewhere whose meaning is not established is `opaque<OffsetHex>` (`opaque2C`), confidence `low`,
  with the copy cited, so it is not mistaken for an unused one. Unread bitfields are `reserved<Offset>Bit<n>` /
  `reserved<Offset>Bits<n>`.
- `hints` holds what the function-naming agents worked out (`fields:` notes in `FUNCTION_CATALOG.csv`). Use them, and
  resolve any disagreement between hints.
- `meaning`: one short phrase (≤ 100 characters), units included (`HP lost this frame`, `fixed 4.12 angle`).
- Fields that are on-disc offsets or raw pointers in the original become typed natively: note `handle:<Type>` (a
  pointer that becomes a `Handle<T>`) or `index:<Type>` where it applies.

## 11. Output format (symbol batches)

`tools\naming\make_symbol_batches.py` writes `tools\naming\sym_batches\<batch>\{globals,types,fields}.csv` plus
`context.md` (referencing functions). Agents write `tools\naming\sym_out\<batch>\` with exactly these headers, one row
per input row:

```
globals.csv  id,disposition,final_name,final_location,purpose,confidence,notes
types.csv    id,role,final_name,final_location,purpose,confidence,notes
fields.csv   id,final_name,meaning,confidence,notes
```

`confidence` is `high` / `med` / `low` as in §5. `notes` uses the tokens above (`catalog:`, `state:`, `asset:`,
`replaced-by:`, `dup-of:`, `same-as:`, `handle:`, `index:`) plus free text, `;`-separated.

### 11.1 Execution and recovery

- Default to direct work. Do not dispatch all generated batches at once. If delegation is needed, start with one
  bounded assignment and verify saved, validated output before adding another; use at most two concurrent agents.
- The generated batches are inventories, not an instruction to research hundreds of symbols before writing.
  Work in checkpoints of at most ten globals, or one type and its fields. Save evidence-backed rows immediately.
  An incomplete output is a checkpoint, not a completed batch; the validator must still reject missing rows.
- Use the existing Python interpreter for CSV tooling. Do not configure Python, invoke Pylance, or install packages
  merely to read and write CSV files. These naming tasks concern C source, not Python environment setup.
- A delegated assignment must stop after ten tool calls or ten minutes, whichever comes first, saving completed
  rows and reporting unresolved IDs. Do not invent names to meet the limit. If no output is saved by that boundary,
  stop expanding the work and diagnose the blocked assignment.
- Judge progress by files and validation results, not an agent's `running` status. A cancelled agent with zero
  completed turns may still have executed tools; inspect the session evidence before claiming it did no work.
- Never mark the naming pass complete until every input ID has a validated output and cross-batch checks pass.
- **Evidence is mandatory.** Every saved row's `notes` must cite the code actually read: `source:<file>:<line>`
  (or `DataCrystal` for documented file formats, `same-as:T#####` for duplicate type rows, `view-of:G#####+off` for
  aliased storage). A name copied from the decomp spelling without reading its uses is not evidence. The validator
  rejects rows without a citation in `--check`, `--check-partial`, `--apply-batch` and `--apply`.
- Subagents gather read-only evidence (uses, writes, values) and return it; names are written and validated only
  after that evidence is read. Agent-written rows are reviewed against the source before being kept.
- Research starts from `python tools\naming\evidence.py <batch> --globals|--types [--max N] [--upstream N]`, which
  prints exact definition and use lines without any model involved. Its member search matches `.member`/`->member`
  by name inside referencing functions, so a hit can belong to another struct with the same member name (e.g.
  `actor->unkC` for `vs_battle_objectData.unkC`); confirm the receiver's type before citing a hit.
- Write citations by searching for exact source text rather than typing line numbers, and check that each field
  row cites at least one line naming the original member (Engine_01's generator does both). From Game_Actors_01 on,
  generators import `tools\naming\sym_gen\common.py` (`Gen`): `c(("file", "text"))` cites either tree
  (`upstream:` prefix for resources\rood-reverse), `T(...)` auto-cites a field's first access through a receiver
  pattern, `g.decl()` cites a declaration explicitly, and `write()` refuses a field whose cited lines do not name it.
- Member uses are searched first in functions whose libclang field references (`build\port\index.json` `refs.field`)
  contain the exact `Type.member`, so `actor->unk0.x` is found for the header type even when the function never
  names that type. `evidence.py` and `common.Gen` both do this. Auto-citations still land on reset lines (`= 0`) that
  prove identity but not meaning; pin meaning-bearing lines explicitly for every name above `low`.
- Upstream refresh, after `resources\rood-reverse` moves: `python tools\port\decomp_index.py --root
  resources\rood-reverse --out build\port\index_upstream.json`, then `python tools\naming\upstream_layout.py report
  --out build\port\upstream_layout\all.md` (classes: same / type-only / layout / fork-only; identical struct source
  counts as same, since shared effect headers differ only by per-overlay macros such as `TRAIL_SAMPLE_COUNT`),
  `upstream_layout.py adopt --all` (refuses a type whose filled rows would move), `python
  tools\ledger\build_symbol_ledgers.py`, and `python tools\naming\make_symbol_batches.py --refresh`, which rewrites
  only the batches not yet applied and keeps their membership.

After a complete batch passes `python tools\naming\merge_symbols.py --check <batch>`, merge it immediately with
`python tools\naming\merge_symbols.py --apply-batch <batch>`. This validates the batch against names already applied
in all three ledgers, rejects incomplete batches without writing, and leaves other rows unchanged.
The final gate remains `--check-all` followed by `--apply`; an individual batch does not satisfy that gate.
Regression tests: `python -m unittest discover -s tools\naming -p test_merge_symbols.py`.

### 11.2 Saved naming progress

The table below records validated outputs applied to the ledgers, **not native source migration**.
The function-name pass is separate. Naming globals/types/fields is still in progress; source copying and the
Phase 1a gate must not be marked complete from these results.

**Evidence backfill (2026-10-16).** The evidence rule above was added after six batches were applied. At the
audit, 236 of 386 applied globals, 38 of 101 types and 412 of 583 fields lacked a citation. Each was then
reviewed against the exact source lines it cites (`tools\naming\cite_globals.py` resolves every storage identity's
own definition through the decomp index and its first use inside an actual referencing function;
`tools\naming\cite_declarations.py` cites type and member declarations). **All applied rows now carry evidence.**
412 fields are tagged `declaration-only`: their recorded evidence is the member declaration (layout and the original
authors' name), not a cited use, so their confidence is capped at `med`; cite a use before raising it. The copy tool
must refuse names whose rows do not pass the evidence check.

| Batch | Globals | Types | Fields/enumerators | Output status |
|---|---:|---:|---:|---|
| Audio | 92 | 20 | 270 | Validated and applied |
| Engine_01 | 151 | 38 | 252 | Validated and applied |
| Engine_02 | 60 | 0 | 0 | Validated and applied |
| Game_Actors_01 | 0 | 55 | 771 | Validated and applied |
| Game_Actors_02 | 48 | 8 | 108 | Validated and applied |
| Game_Battle_01 | 0 | 92 | 583 | Validated and applied |
| Game_Battle_02 | 0 | 66 | 689 | Validated and applied |
| Game_Battle_03 | 0 | 78 | 737 | Validated and applied |
| Game_Battle_04 | 58 | 58 | 526 | Validated and applied |
| Game_Battle_05 | 114 | 0 | 0 | Validated and applied |
| Game_Effects_01 | 0 | 80 | 659 | Validated and applied |
| Game_Effects_02 | 239 | 9 | 53 | Validated and applied |
| Game_Ending | 75 | 6 | 59 | Validated and applied |
| Game_Event | 69 | 17 | 160 | Validated and applied |
| Game_Field_01 | 63 | 59 | 500 | Validated and applied |
| Game_Field_02 | 54 | 0 | 0 | Validated and applied |
| Game_Menu_01 | 0 | 95 | 557 | Validated and applied |
| Game_Menu_02 | 38 | 63 | 582 | Pending |
| Game_Menu_03 | 262 | 0 | 0 | Pending |
| Game_Menu_04 | 262 | 0 | 0 | Pending |
| Game_Menu_05 | 262 | 0 | 0 | Pending |
| Game_Menu_06 | 199 | 0 | 0 | Pending |
| Game_Title | 121 | 21 | 82 | Validated and applied |
| Shared | 7 | 57 | 282 | Validated and applied |
| **Applied / inventoried** | **1,151 / 2,174** | **759 / 822** | **6,288 / 6,870** | **19 / 24 batches applied** |

Inventory changed on 2026-10-17 when 25 upstream layouts were adopted (15 merged layouts, 10 type-only retypes; two
new upstream-only types, `func_800D52A4_t`/`_t2`, the effect node's motion block). Applied rows kept their names;
four applied fields changed C type only (`AkaoSeqHeader.magic` `char[4]`→`int`, `_mpdEnemy.settings`
`u_char[8]`→`actorSpawnSettings`, `vs_battle_objectData.material`/`variant` `char`→`u_char`).

Applied rows still include 51 low-confidence globals, 78 low-confidence types and 2,158 low-confidence fields. Validation checks coverage,
names, locations and collisions; it does not prove semantics or close migration hazards. In particular:

- G00805/G00806: the ending atlas declaration and a shared-header per-overlay declaration overlap. Resolve their
  ENDING identity without collapsing the separate MENUF instance.
- G00323 is a view into G00322 at byte offset two. G00335 has a scalar pause-flag declaration and an array
  declaration whose platform indices reach G00337. Replace these with explicit native fields, not independent
  copies or out-of-bounds indexing.
- G00603 has seven declared bounce offsets but is indexed at seven. G00820's declared ripple grid also needs
  boundary/extent verification. Resolve the original values before copying; do not carry these assumptions into
  native arrays. Unsized-array inventory gap sizes are not validated capacities.
- G01961/G01963/G01970 have only room-load reset writes in the indexed C references. Their names describe that
  observed behavior, not established gameplay meanings; assembly/cross-reference review remains required.
- Lexical normalization of anonymous-type declaration paths reveals 17 exact duplicate layout groups
  (67 redundant type rows). Each duplicate row carries `same-as:T#####` naming its canonical row and uses the
  identical final name and field names; the validator rejects a `same-as` target that does not exist, a final name
  that differs from a named target, and a field name that differs from the target's field at the same index.
  Do not manufacture different native types from include-path spelling.
- T00581 `Input::VibrationPatternSlot`: the pattern interpreter uses `channels` as `int[2][6]` (48 bytes) but only
  40 bytes remain in the 56-byte element, and only slot 0's `motorLevels` is used as the summed output. The real
  element most likely starts 8 bytes later with the motor levels stored once before the array. Lay out the native
  type from the interpreter, not the declaration.
- Audio layouts come from the binary, not the decomp declarations: `Audio::MusicConfig` is 0x80 bytes (configs are
  copied with `Sound_memcpy32(..., 0x80)`; `MusicId` is read at +0x6E in the matched asm; the Chrono Cross AKAO
  driver's `FSoundMusicContext` is also 0x80), so its `reserved80`-`reserved9C` members do not exist.
  The sound-effect channels are one 12-entry array starting at G01847 (declared `[10]`), and G01848 aliases
  entries 10-11. The binary labels `g_Sound_GlobalFlags` as one word; its C struct's second and third words are
  the separate globals G01891/G01892, and the stereo/mono commands overwrite G01891 whole. Several unsized Audio
  tables have inventory sizes that overlap the next symbol; their capacities come from address gaps (noted per row).
- Final locations of data rows and the function catalog disagree in places (`Input\src\Controllers.c` vs
  `Controller.c`, `Graphics\src\ScreenTransition.c` vs `FramebufferEffects.c`, `Core\src\Memory.c` vs
  `Memory\Heap.c`, `Content\src\DiscQueue.c` vs `CdDrive.c`, `Video\src\Intro.c` vs `Intro\Playback.c`).
  Reconcile file placement once, before copying, instead of per batch.
- T00595.003 (`iconHeaderBytes`) receives `CardUiResources.fileIconPalettes`; the save header written at
  `260.c:753` (icon flag 0x11) matches the standard PS1 card layout with the icon palette at 0x60, so it can be
  renamed `iconPalette` when Shared/Title rows are next revised.
- Event: T00219 is a merged-bitfield view of `gim_t` (5BF94.c, now `Render::GimLayer`); resolve both to one native
  type. T00259's shake code reads its two oscillators from +0x1A6, two bytes after the declared field, and uses the first
  short as the active-shake count. G00442 is byte 3 of G00441. G00128/G00119 are command templates patched at
  runtime, so they are state, not constants.
- G02005/G02006 identify loading-image pixels/header through consumers, but their source definitions and extraction
  bindings are not located by the inventory. Converted asset paths in output notes remain proposed destinations.
- Upstream layout adoption (2026-10-17): `vs_battle_movementPosition` is a union (`raw` + `p.x/y/z/status`) in the
  pinned fork and a plain byte struct upstream; the pinned union is kept because the copied code uses its members,
  but the native type should be the struct and the `.raw` word compares become an explicit packed compare. Shared
  effect headers (`trails.h`, `grids.h`, `rings.h`, …) are instantiated per overlay with macros such as
  `TRAIL_SAMPLE_COUNT`; port them as one parameterised type, not one type per overlay. Adopted layouts that retype
  members still used by pinned-only bodies (e.g. `func_800D2904_t.unk5C`, `int` in `func_800D2ADC`, `char[4]`
  upstream, where `func_800D2ADC` is still asm) must be reconciled per function when copying.
- Game_Actors_01 (2026-10-17): `Field::GimmickInstance` (T00255) begins with the `Actors::ModelInstance` header
  laid out as gimmick motion fields (the same bytes `Field::GimmickPhysicsView`, `Field::GimmickMotionView` and
  `Actors::PalettePulseView` read); its tail from +0x178 is actor-record space gimmicks never use (named
  `reserved<Off>`). Native code should give gimmicks a header plus their own fields, not a cast of the actor record.
  `Actors::RoomActorRecordWrite` (T00431) is the write-side view of `Actors::RoomActorRecord` (T00597), and
  `Battle::Ai::AiActorView` (T00383) is a fork-only view of `vs_battle_actor` (T00630, upstream callers pass
  `vs_battle_actor*`): fold views into their canonical types when copying. Placement word byte 1 (applied as
  `direction`) only keeps bit 0 at spawn and feeds tile lookups as a layer (`Actors::TileTarget.tileLayer`,
  `ModelInstance.currentTileLayer`); revisit T00444's name when Field rows are next revised.
  346 of the 771 fields are `low` (239 `reserved`, 13 `opaque`); most actor-record flag bits are only reset and
  tested, and need behavior tracing before their names can be raised.
- Game_Battle_01 (2026-10-17): 41 of its 92 types are per-overlay copies of anonymous header types
  (`146C.h`, `5BF94.h`, `6E644.h`, `func_8006EBF8_t.h` included from INITBTL, MENU3 and SLUS) or of the AI's
  repeated tile word, written as `same-as` rows by `common.Gen.D()`. `Battle::Ai::TileWord` (x, tag, z, search
  status) is declared nine times across the fork's AI files and once upstream as `vs_battle_movementPosition`; its
  byte 1 is a gate index in routes and a layer in placements, so it is named `tag`. `Battle::TargetSphereSetupView`
  (573B8.c) and `Battle::TargetSphere` (58578.c) are two declarations of one global (D_800EB9B8) with different
  detail; `Battle::Ai::TargetRelationReactionView`/`Bytes` are other splits of the relation word in
  `Battle::Ai::TargetRelationBits` (upstream `func_800DA1D4_t`). Merge each set into one native type when copying.
  `Battle::ActorTargetWord` (vs_battle_actor +0x10) is used both as a pending Z movement offset and as the last
  targeted actor/limb; resolve that overlap from the assembly before porting.
- Game_Battle_02 (2026-10-17): the fork's AI files declare a separate struct per function for one object, the AI
  actor state (upstream `func_800E78F4_t`, T00532, allocated 0x484 bytes); upstream's reviewed functions take that one
  type. `tools\naming\sym_gen\ai_state.py` names each byte offset once, from the uses gathered across all views and
  upstream (`tools\naming\ai_offsets.py` prints the per-offset evidence), and every view takes the
  name at its offset when the member has the same size as upstream's member there (untyped `char[N]` fillers
  upstream do not constrain). Settings bytes are named after the settings-word bit they are decoded from
  (`settingsBit26`), which is what the code shows. Other views use `common.Gen.generic_specs`: a used member keeps a
  meaningful fork name (cited by its use) or becomes `opaque<Off>`; an unread member is `reserved<Off>` (bitfields
  `reserved<Off>Bit<n>`) even when the fork gave it a plausible name. Battle_03 must name T00532 from the same table.
  421 of 689 fields are `low` (294 reserved, 81 opaque).
- Game_Battle_03 (2026-10-17): T00532 is `Battle::Ai::ActorAiState`, named from the same offset table as its 28 fork
  views (15 more views here, including the small prefix views `DirectionCacheView`, `WaypointSearchView` and
  `ObstructionView`). Canonical entry types whose fork views were already named by use take those names where the
  bit offset and width match (`common.Gen.names_from_views`): `ActionCandidate` (T00517) from
  `CandidateEntryView`, `ActionSlotMetadata` (T00520) from `ActionMetadataEntry`, and `RelationWord` (T00515,
  upstream's bit split) from `TargetRelationBits`. 506 of 737 fields are `low` (287 reserved, 176 opaque); most
  opaque members are AI state bytes that are set and tested without a behavior that explains them.
- Game_Battle_04 (2026-10-17): the core battle types are named member by member from their read uses:
  `Battle::Actor` (vs_battle_actor: a queued command `commandState`/`commandOpcode`/`commandTarget`/`commandArg0-2`,
  `kind`, spawn data, `stats`, `model`), `Battle::ActorStats` (vs_battle_actor2: speeds, mobility limbs, ranges,
  `limbActions`, `statusImmunities`, `actionRecoveryTimer`, drops), `Battle::ActionDefinition` (vs_action_t:
  `category` indexes the targeting tables, `recoveryDelay`, `areaRange*`), `Battle::StatusEffectParams` and
  `Render::FrameScratch` (the scratchpad frame context). Seven more AI state views use the offset table; two views of
  D_800F5920 take `Battle::Ai::ArenaOrbitState` names by offset. `class` members become `classes` (C++ keyword).
  Globals: the three status-string globals are `statusStrings` entries addressed directly (u16 offsets 142, 167 and
  246 = `targetTemplate`, `line23`, `empty`), dropped in favour of the table; G00147 sits in rodata but is the
  target-menu step counter; G00017/G00018 are unreferenced. Corrections: T00273 offset 0x14 (Battle_01) is
  `healthPhase` (upstream 6E644.c:4112 reads it). Gap found: `func_800AD714` (44F14.c:168, the animation event
  interpreter) is missing from FUNCTION_CATALOG.csv. 195 of 526 fields are `low` (137 reserved, 30 opaque).
- Game_Battle_05 (2026-10-17): 114 globals named from read uses. `D_800EBDDC` is the `doorStrings` asset (splat
  offset 0x835DC). The puzzle timer text `D_80069998` is declared const but rewritten every frame through the pointer
  `D_800EC258` (initial value read from BATTLE.PRG), so it is state and the pointer is dropped. `D_800F56A8` is the
  effect renderer table (G00574) under its address name. G01907-G01912 are single bytes of `statusEffectParams`
  entries 5, 11, 13, 14, 17 and 19 (address arithmetic from 0x8004EECC), which shows offset 4 is a duration:
  T00709 `unk4` is now `duration` (Battle_04 had it as unread). Room AI tables (tile flags, special terrain heights,
  waypoints, patrol routes, route gates) are filled by code that is not decompiled; their names rest on reads only.
  `_cameraMode` is `Battle::actionFlowMode` (it selects action-flow stages, not only the camera). 11 globals are `low`.
- Game_Effects_01 (2026-10-17): the effect runtime is named from InitEffectInstance, SpawnSecondaryEffects and the
  timeline ticks (`Effects::EffectInstance`, `EffectSpawnRequest`, `EffectTimeline`, `TimelineTrack`). The particle
  definition (`ParticleDefinition`) and the scratchpad emitter (`ParticleEmitterScratch`) are named from
  EmitParticleEvent (5BF94.c:6940-7500): each definition curve byte samples one range into one scratch member
  (`positionCurve` -> `positionRange` -> `position`, ... `lifetimeCurve` -> `lifetimeRange`). The particle node
  (`Effects::Particle`) uses upstream's merged `motion` group; `common.Gen.layout` now builds offsets for merged
  layouts member by member (pinned names first, then upstream). T00489 and T00493 are two declarations of one
  208-byte transform slot; members only cleared in InitEffectInstance (upstream 5BF94.c) are `reserved`. Correction
  (Game_Battle_03): T00418 `EffectDefinitionView` members are now `flags`, `attachment`, `rotationXAndTarget`,
  `rotationY`, `rotationZ` (5BF94.c:5947-6014), which also names T00494. 186 of 659 fields are `low`
  (102 reserved, 45 opaque).
- Game_Effects_02 (2026-10-17): 155 plugin globals are named by `sym_gen/Game_Effects_02.py` from each plugin's
  template binding (`#define VS_<MEMBER> D_...` followed by the template include, or an inline `rings.h`-style index)
  found in the global's own overlay directory: plugin overlays share an address range, so one `D_` name can be a
  different object in several plugins. Names are `Effects::PlgNNN::<templateMember>`, numbered when a plugin binds
  the same template twice; three template `extern` rows are views of the plugin's own definition. The BATTLE effect
  runtime globals (primary/secondary instance lists, phase, tracks, tokens, particle pool, camera motion, package
  loading) and the EFFPURGE load tables are named from their read uses. 5 globals are `low`.
- Game_Menu_01 (2026-10-17): 49 function-local `enum state` types are named after the function that declares
  them (`Menu::Files::SaveFilesState`, `Save::CardReadinessState` for the save code shared by MENU7 and TITLE) and
  placed in that function's header; enumerators drop the prefix all of a type's enumerators share
  (`fileMenuElementStateAnimateX` -> `AnimateX`). `tempFileCreated` becomes `TemporaryFileCreated` (forbidden token).
  T00746 takes the names earlier same-as rows expect (`Input::PressedButtonsView`: `allButtons`, `ports`); T00124 and
  T00187 are same-as T00192 (`Input::ButtonBytePair`), written against the applied row.
- Correction (Game_Field_01): T00223 was `Battle::PendingAbilityState` with "unidentified" shorts, but it is the menu
  result returned by `Menu::Framework::ActionSelectionState` (D_800F4E98): `menuStatus` (146C.c:7771) and
  `selectionKind` (146C.c:4693); renamed `Menu::Framework::ActionSelection`.
- Function catalog gap (2026-10-17): 56 functions defined in `.c` files of the pinned index are not in
  FUNCTION_CATALOG.csv (14 in SLUS main.c, CLUT transforms; 10 MAINMENU C48.c; 7 MENU5 4D8.c; 6 MENUE 494.c; 4 MENU9
  238.c; 6 BATTLE 146C.c at lines 8280-8317; 4 in 44F14.c including the animation event interpreter; 4 in 5BF94.c;
  1 ENDING D4.c). The other 372 indexed functions missing by file live in shared EFFECT headers and are catalogued
  under the overlays that include them. The 56 need catalog rows named from read code before copying starts.
  Resolved: `tools\ledger\add_missing_functions.py` appends the 57 functions the scanner missed (the count above
  left out `func_800AD714`) as F04121-F04177 with columns from `build_ledgers.py`'s own helpers; re-running
  `build_ledgers.py` would renumber every id, so do not. They were named from the read functions in batch
  `CATALOG_GAP` (`Menu::*::UpdateScreen` for the MENU5/9/E entry points, the reward pause pair in `Battle::`,
  the CLUT/background-gradient/tint transforms in `Render::`, `Actors::RunAnimationEvents`). Coverage is 3736/3736.

The ending frame counter G01964 counts presented frames for XA resume seeking; it is **not** a frame duration.
The native audio player must own playback position rather than reading another module's timing globals.