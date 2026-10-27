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
| `Core` | `Native\Engine\src\Core\` | Fixed-point/vector/matrix math (GTE replacements), RNG, memory/heap helpers, string utils, generic containers |
| `Render` | `Native\Engine\src\Render\` | Generic rendering: primitive building, model/mesh drawing (SHP/WEP), texture/CLUT upload, sprite and font/glyph drawing, screen effects, camera projection, display setup |
| `Audio` | `Native\Engine\src\Audio\` | Sound driver, SEQ sequencer, SFX playback, voice allocation, music control, XA/streamed audio control |
| `Content` | `Native\Content\src\` | File loading, CD read queue, file tables, archive/format decoders, decompression |
| `Input` | `Native\Engine\src\Input\` | Pad processing, button repeat, vibration |
| `Movie` | `Native\Engine\src\Movie\` | FMV/STR playback, MDEC decode |
| `Save` | `Native\Engine\src\Save\` | Memory card I/O, save/load serialization, save menu data handling (not the menu UI) |
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
| Game_Menu_02 | 38 | 63 | 582 | Validated and applied |
| Game_Menu_03 | 262 | 0 | 0 | Validated and applied |
| Game_Menu_04 | 262 | 0 | 0 | Validated and applied |
| Game_Menu_05 | 262 | 0 | 0 | Validated and applied |
| Game_Menu_06 | 199 | 0 | 0 | Validated and applied |
| Game_Title | 121 | 21 | 82 | Validated and applied |
| Shared | 7 | 57 | 282 | Validated and applied |
| **Applied / inventoried** | **2,174 / 2,174** | **822 / 822** | **6,870 / 6,870** | **24 / 24 batches applied** |

Inventory changed on 2026-10-17 when 25 upstream layouts were adopted (15 merged layouts, 10 type-only retypes; two
new upstream-only types, `func_800D52A4_t`/`_t2`, the effect node's motion block). Applied rows kept their names;
four applied fields changed C type only (`AkaoSeqHeader.magic` `char[4]`→`int`, `_mpdEnemy.settings`
`u_char[8]`→`actorSpawnSettings`, `vs_battle_objectData.material`/`variant` `char`→`u_char`).

Applied rows still include 52 low-confidence globals, 77 low-confidence types and 2,246 low-confidence fields
(1,042 globals, 457 types and 3,227 fields are `med`; §11.3 records the low/med sanity pass). Validation checks coverage,
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
- Final locations of data rows and the function catalog disagreed in places (`Input\src\Controllers.c` vs
  `Controller.c`, `Graphics\src\ScreenTransition.c` vs `FramebufferEffects.c`, `Core\src\Memory.c` vs
  `Memory\Heap.c`, `Content\src\DiscQueue.c` vs `CdDrive.c`, `Video\src\Intro.c` vs `Intro\Playback.c`).
  Resolved 2026-10-17 by `tools\ledger\reconcile_locations.py` (run by `sanity_fixes.py`): 232 `state`/`constexpr`
  globals in files no function lives in moved to the catalog file their users live in, within the same namespace
  root. 62 stay in data-only files on purpose: per-plugin effect state (`Effects::PlgNNN`, see §6.8 templates),
  shared game state in `Game\src\State\`, the write-only reset words in the trace queue, and constants such as
  `Core::kNegativeYAxis` used from other roots. `catalog`, `asset` and `drop` rows produce no source file, so their
  locations do not matter. The 202 catalog rows proposed as `Triage` now carry the system of their final name.
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
- Game_Menu_02 (2026-10-17): nine more function-local state enums (named after their functions), 15 other enums and
  the remaining menu structs; four C-name collisions were renamed (`data` -> `items`, `MenuControlState`, `MenuRow`,
  `Menu::TextBoxState`). A receiver review (citations whose receiver differs from the type's usual one) moved 16
  auto-cited fields that had matched a same-named member of another type back to declaration-only.
- Game_Menu_03..06 (2026-10-17): 985 menu globals from `sym_gen/menu_globals.py` (rules) and
  `sym_gen/menu_globals_manual.py` (rows read by hand). Each global's uses are searched in its own scope from the
  ledger key (the declaring function for local statics, the file, or the including overlay for header statics).
  A meaningful fork name is kept at `med`, cited by its first use, in the namespace and file of its main user
  (local statics are prefixed with their function's context). Address names are named from recognised use patterns
  (cursor animation, OK blink, yes/no choice, selected row plus one, step switch, slide counters, item source and
  index from the argument) or by hand; three pattern names were wrong on reading (`D_80109A4D` is the sort category,
  `D_80109A7A` the browsed storage, `D_80109A7D` the controls panel mode, not the capacity panel) and were replaced.
  `_`/`_0`.. statics are layout padding (`__attribute__((unused))`, or only cleared) and are dropped. `items1.h`
  `extern` rows are `dup-of` the `_toggleUi.h` statics. Every row's citation names the global (checked).
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

#### 11.3 Low/med sanity pass (2026-10-17)

After every row was named, the low and med rows were re-checked against evidence that did not exist when they were
first named (all functions named, later batches' types and globals named, the refreshed upstream index).
`tools\naming\sanity_pass.py` writes candidates to `tools\naming\sanity\report.csv`; accepted corrections live in
`tools\naming\sanity_fixes.py` (each cites the code read) and are patched into the owning batch outputs and re-applied.
**Re-run `sanity_fixes.py` after regenerating any batch.**

- Upstream: for 3,114 low/med members whose struct also exists upstream, upstream uses the same member name as the
  fork at the same bits (6 differ, and there the fork name is the better one); upstream `symbol_addrs.txt` names
  equal the pinned ones. The upstream update reveals no new member or data names.
- Claims checked against all pinned and upstream references: 18 `reserved` members described as unreferenced are
  read. They are now named from those reads (`GimmickInstance.rotationDeltaX/Y/Z`, `SeqChannel.keyOnVolume` and
  `effectPitchScaleSlideStep`, `ShpBone.poseLockSlot`, `MpdTileCell.hasEventTrigger`/`hasRoomTrigger`/
  `blocksSphereTerrain`/`occlusionAndFloorFlags`, `MpdEnvironment.environmentFlags`/`fallDestinationZone`/
  `fallDestinationRoom`, `ActorLocationQuery.actorIndex`, `WeaponModel.boneWorldMatrices`/`ownerMotionState`), or
  become `opaque` where the meaning is still unknown. The other 45 reserved members that are referenced are only
  cleared or copied (their meaning says so). `D_800F22D0` (G00389) said "no reader" but frees the sequence buffer:
  `Content::legacySequenceBufferOwned`. T00269 said "not read" but is the effect texture load list:
  `Effects::TextureLoadList` with `assetCount`.
- Parameter names: an opaque/low member passed straight to a function whose parameter has a meaningful name.
  Accepted after checking the receiver type: `TargetVolume.kind/angle/scale/position`, `ActionCandidate.targetLimb`,
  the container screen's `storedContainer`/`transferPreview`/`stagedTransfer`/`gadgetBag`, and the camera shake
  step curves. Rejected: generic parameter names (`block`, `img`, `src`, `id`) and four PLG005 hits whose receiver
  is another type with the same member names.
- Evidence: 439 `declaration-only` members now cite a use. The citing function references this type's member and
  no other type with a member of that name, so the receiver cannot be another type. 113 remain declaration-only
  because every citing function is ambiguous.
- Sampled audit (random seeds 17, 23 and 31): 25 kept fork names, 20 other med globals, 30 med fields with uses, 30
  declaration-only fields, 20 med types, 25 low named fields and 15 opaque fields were each read against their
  citation. No wrong name was found. The only issues were six local-static names with a doubled word
  (`quitDelayDelay`), now merged by `menu_globals.py`.
- Totals after the pass: globals 1,080 high / 1,042 med / 52 low; types 288 / 457 / 77; fields 1,397 / 3,227 /
  2,246. Most low fields are `reserved` (never read) or `opaque` (read, meaning not established); they need behavior
  tracing, not renaming: see [RUNTIME_TRACING.md](RUNTIME_TRACING.md) and `tools\naming\trace_queue.py`.

The ending frame counter G01964 counts presented frames for XA resume seeking; it is **not** a frame duration.
The native audio player must own playback position rather than reading another module's timing globals.

#### 11.4 Catalog gaps closed and the reference index (2026-10-18)

Building the occurrence-level reference index (`tools\port\ref_index.py`, plan Step 4 item 9) resolved every
identifier in game code to a ledger row. That surfaced the last gaps, now closed:

- **Inline assembly and link-time names.** `tools\ledger\add_missing_functions.py --batch <name>` now also catalogs
  `glabel`/`alabel` entry points written as inline assembly in `.c` files, `jumpFunc NAME, TARGET` lines, and `.s`
  alternate entry points that C code references. These were batches `CATALOG_GAP_ASM` (22 rows, F04178-F04199:
  actor movement-request starters, halfword copies, sprite queueing, menu sounds), `CATALOG_GAP_ASM2`
  (`Boot::JumpToTitle`, `Boot::JumpToGameLoop`) and `CATALOG_GAP_ASM3` (`Battle::Ai::CanWalkInDirection`,
  `BATTLE/BATTLE.PRG/70D4C.s:458`, the ground wander check beside `CanFlyInDirection`). The script refuses to
  overwrite an existing batch.
- **Nested unnamed types.** `decomp_index.py` now recurses into struct/union/enum definitions nested two or more
  levels deep. That added 20 types and 56 fields, named in symbol batch `Nested_Types`.
- **Stale split file.** `EFFECT/PLG053.BIN/0.c` is not built: its `splat.yaml` has the single subsegment
  `[0x18, c]` (`18.c`), and upstream deleted the file. Its rows F01921-F01923 carry `not-linked`, which merge_names
  turns into `copy = no`. PLG054/055 `func_800FA368` (byte-identical images) are now `dup-of` the PLG053 closed-ring
  template instance F01925. A `dup-of` row may point at a template instance and use its `Templates\` location.
- **Link alias.** `_toVsStringPercentAlias` exists only as `--defsym` in `config/MENU/MENU9.PRG/Makefile`. In the
  shipped MENU9.PRG, address 0x80106D5C is the start of `_toVsStringPercent`, so the index maps the alias to that
  function.

Function rows to copy: 3,758, all named. Full index run: 138 translation units, 100,172 occurrences
(fields 46,749, variables 23,334, functions 19,616, types 6,831, enumerators 3,642, including 1,253 inside macro
bodies). Of these, 98,033 are direct ledger ids, 1,655 are `sdk:`, 301 are `plugin:` (effect entry points that
share a load address), 82 are member paths through upstream-merged layouts, 11 are widened views (`~w`), and 90
are multi-target (headers included by several overlays, e.g. per-instance statics). There are **0 unresolved and
0 text mismatches**. A full run takes about 10 minutes; rerun it after any ledger or naming change.

#### 11.5 Naming fixes found by the copy (2026-10-17)

Copying and compiling everything (plan Step 4) showed six naming defects; each is fixed at its source:

- **Effect template instances** were named from the bound macro alone, so a plugin binding the same macro for two
  templates got two functions with one name (PLG057 binds VS_TRAIL_* for trails9 and trails5). `merge_names.py`
  now names an instance after the template body it feeds: the first `#include` after the binding whose header
  defines that macro's function (`Effects::Plg057::TrailNineOrigin`, `ParticleLineRender`). A binding whose header
  does not define it (PLG065 rebinds its particle functions so particleDispatch.h can call them) is a reference:
  `template-binding;dup-of:<instance>`. 430 instances and 50 bindings; every copied C name is unique.
- **Stale dup-of target:** F02001 (PLG077) was dup-of F01922, the stale PLG053/0.c row that is not copied; it is
  dup-of the PLG053 closed-ring template instance F01925. `merge_names.py --apply` now replaces a row's old dup-of
  with the batch output's instead of keeping both.
- **Type placement:** `Battle::EffectInstanceTargets` (T00264) is only a member of `Effects::EffectInstance` and holds
  Effects types by value, so the two headers needed each other: it is `Effects::EffectInstanceTargets` in
  `Effects\Runtime.h`.
- **Function-local name collisions:** five functions had two unused layout-padding statics (decomp `_0`, `_1`) with
  one name; the second is `...LayoutPadding2`.
- **Nested types of same-as types:** TITLE's `primBuf_t` is same-as MENU7's, but the unions nested in it were named
  separately (`sprite` / `sprt`). The copy writes MENU7's, so `tools\port\copy_fixups.py` maps T00794 -> T00784 and
  T00838 -> T00836 (field references follow by index).
- **Same-as field names:** a field reference through a same-as type now uses the canonical type's field at the same
  index, since only the canonical type is written.

#### 11.6 Native semantic sweep (#148, 2026-10-27)

The owner requires individual source review of every remaining candidate before
#148 can close: apply a proven semantic name or record a specific justification
for retaining it. A lexical inventory, an automatically populated unresolved
list, a successful build, and a passing quality baseline do not meet that gate.
Existing function/data/type/field ledgers remain the sources of truth; naming
does not promote behavior, C++ ownership, or compatibility-retirement status.

The source-backed generator batch covers:

- F03449-F03460: 31 vibration bindings for port/motor indices, one-based slot
  IDs, owner tags and fixed-point motor envelopes. F03460 deliberately retains
  `var_a2_2` and `var_v0`: each is an unused declaration in original
  `SLUS_010.40/main.c:8701/8704`, with no native read, write or initializer.
  Inventing a runtime role for these two artifacts would be misleading.
- 180 opcode ledger identities: 353 instruction/thread parameter bindings
  (177 unique native names, 347 unique pairs). Both dispatch paths in original
  `BATTLE/BATTLE.PRG/573B8.c:67-142` establish the shared call contract.
- F01099/F00231: `ScriptClearRoomActorCache` and map/zone arguments follow
  original `4A0A8.c:1780`, `146C.c:8509` and `146C.c:786`. This is a cache clear,
  not the previously claimed room preload.
- F03463-F03467: 10 allocator bindings describe search direction, free-list
  predecessors, split remainders and native header units. They do not promise
  fixed 16-byte alignment.
- F01311/F03461: six further bindings name the raw stick-axis byte, preset
  index and little-endian preset-directory indices. Original
  `5BE50.c:82-91`, callers `5BF94.c:667-668`, and `main.c:139/8769-8785`
  establish the meanings. `directory_stride` is both the four-byte entry
  stride and the first offset-byte index after the count header; arithmetic
  and signedness remain unchanged.

Fresh local checks on base `804d1899` plus these edits and the pre-existing
unrelated dirty work:

- Debug and Release: `[input],[heap],[room-cache],[event]`, 41 cases and
  912 assertions each. New input boundary/preset controls also passed before
  the two input functions were renamed.
- 126 copier and 32 naming tests pass in the dirty tree. This includes the
  separately uncommitted title/owned-dispatch controls; it is not evidence that
  their associated issues are complete.
- The final input generation verifies exact expected identifier substitutions
  across all 742 manifest paths, preserves all 121 registered owned paths,
  and produces an identical manifest on the second generation.
- Scoped formatting/lint/compilation-based analysis has no new findings;
  the six existing event-test findings remain debt. The initially oversized
  preset test was split through a fixture assertion helper without dropping
  checks or changing the baseline.

Evidence is local under `build\quality\issue148-reviewed-*`,
`issue148-input-*`, and the earlier vibration/event/heap/room-name proof files.
That checkpoint's lexical snapshot has 24,710 occurrences, 2,145
file/identifier pairs and 368 files. Those are **not reviewed-unknown counts**:
the legacy regex also matches meaningful `speed`/`spec` and the standard-library
`temp_directory_path`, and file/name pairs can combine several distinct
bindings. Those matches need contextual classification, not cosmetic renaming
or blanket exclusion. Repository-wide individual review remains open.

The first batch is published as `aec9a7be`, with successful exact-revision
Source Quality run `40556560359`.

A second source-reviewed batch on that revision names 13 bindings in
F01273/F01401/F01404/F01455/F03434/F03435: source bytes, a backward-clear
destination/byte extent/word cursor, previous interface-RNG state and requested
bit count, unordered random bounds/modulo offset, and shuffle scaling/sample
locals. Original `573B8.c:146-152`, `5BF94.c:4476-4501/5810-5826` and
`SLUS_010.40/main.c:8251-8285` provide the contracts.

F03264 deliberately retains `a0`/`a1`: original `25AC.c:3993-4022` uses these
as the first two staged words from buffer A, paired with `b0`/`b1` from buffer B.
F03569 and its F03570 alias retain `arg0`/`arg1`: the original empty bodies
explicitly declare both unused. The ledger records each retention separately;
this is not a blanket exception for matching spellings elsewhere.

Before renaming, Debug memory/random controls pass 15 cases/31,343 assertions.
Afterward Debug and Release each pass 42 cases/719,587 assertions with
`[core][memory],[core][random],[loot] --reporter compact`. This checks signed
aligned/unaligned reads, positive word-multiple clear boundaries, exact shuffled
and averaged RNG sequences, reversed/equal bounds and draw consumption, and
1..32-bit interface requests. Zero-bit return semantics, unsupported ranges,
gameplay completion and compatibility retirement are not newly certified.
Tests restore all previous RNG state, including the underlying seed.

Both configurations build through
`MSBuild.exe Native\Tests\VS.Tests.vcxproj /m /p:Configuration=<Debug|Release>
/p:Platform=x64 /nologo /v:minimal`.
Two generations again preserve 121 owned paths and compare all 742 manifest
paths: only `Core\Random.c/.h` and `Core\Memory\Copy.c/.h` change, exactly by
the approved identifier substitutions. Dirty-tree copier tests are 127/127;
naming tests remain 32/32. Focused `quality-native.cmd --files` with `--tidy`
reports one existing finding and zero new findings, with no baseline edits.
New RNG tests are appended so Catch's generated function IDs do not relabel old
quality debt. Evidence is under `build\quality\issue148-core-*`.

The subsequent lexical snapshot has 24,643 occurrences, 2,131 pairs and 365
files, still subject to the same false-positive and binding-identity limits.
All candidates in the reviewed input controller, memory-copy, random and
runtime source/header files now have either verified replacements or specific
retention evidence. Other files are not implicitly marked reviewed.

The second batch is published as `5d439e3c`, with successful exact-revision
Source Quality run `40557984898`; its isolated staged copier suite is 124/124.
The final reference-index command is
`.venv-3\Scripts\python.exe tools\port\ref_index.py --out
build\quality\issue148-reviewed-refs.json`. On `5d439e3c` with unrelated dirty
work retained, it visits all 138 original translation units and resolves
100,172 occurrences across 215 source files: zero unresolved occurrences,
zero text mismatches, and exact occurrence-map equality with
`build\port\refs.json` used by generation. The 22 original error translation
units remain the documented old-C parsing limitations, with zero unresolved
references in them. Log: `build\quality\issue148-reference-index.log`.
Resolving an occurrence to its ledger ID is not a semantic review of every
local or parameter; #148's individual-review closure gate remains unmet.

The continuing math cohort reviews 20 functions in generated `FixedPoint`,
`Matrix`, `Trig` and `Vector` source/header pairs. Sixty-one bindings now name
vector sources/destinations, differently ordered output parameters, fixed-point
scales, component squares, signed magnitude selection, unordered clamp bounds,
packed coordinates, and orientation axes. Each function's ledger note and
copier rule cite its original body; the negative-Y look-at reference, short
narrowing, tie selection, matrix translation preservation and Q12 arithmetic
are unchanged. Existing meaningful names and the unused look-at padding remain.

On dirty base `a488d710`, nine new native controls pass 81 assertions before
renaming. Afterward Debug/Release each pass 39 cases/867 assertions using
`[core][math],[event] --reporter compact`; expected missing-actor diagnostics
come from the existing event negative controls. Dirty-tree copier tests are
128/128 and naming tests 32/32. Focused tidy reports one existing/zero new,
with unchanged baselines. Exact expected-output comparison covers all 742
manifest paths, restricts changes to the eight reviewed math files, preserves
121 owned paths and verifies a stable second generation.
Evidence: `build\quality\issue148-math-*`. The candidate inventory is now
24,342 occurrences/2,095 pairs/357 files; it still is not an all-reviewed count.
Work continues through the remaining individual source reviews rather than
treating this milestone as an issue-completion or stopping gate.

The math cohort is published in `baa0a27f`, exact green Source Quality
`40562493951`. The next reviewed cohort covers all remaining generic bindings
in generated `Core\Math\Interpolation.c/.h`: 15 functions and 68 bindings.
Original `5BF94.c:5805-5909/6382-6472` and its transform/camera callers establish
endpoint order, Q7/Q12/Q13 units, total/current step order, and the four
Lagrange control weights. No polynomial, divide/shift order, truncation,
extrapolation, callback, or ownership behavior changes.

On dirty base `baa0a27f`, five new controls first pass 65 assertions before
renaming. Debug/Release then each pass 44 cases/932 assertions with
`[core][math],[event] --reporter compact`. Controls distinguish negative
seven-bit shifts from thirteen-bit truncating division, ratio quantization,
aliasing and padding, quarter-step cosine/linear interpolation, and the
original rounded two-thirds Lagrange coefficients outside the central interval.
Copier tests pass 129/129 and naming tests 32/32. Final scoped compilation-based
quality has one existing/zero new findings; explicit fixed-width test types
repair the initial style findings without changing values or baselines.
Two generations again prove exact identifier-only changes to these two files,
all 742 manifest paths checked and all 121 owned paths preserved.
Evidence: `build\quality\issue148-interpolation-*`.
The lexical inventory is 24,102 occurrences/2,074 pairs/355 files; other
subsystems' candidates remain subject to individual review.

The interpolation cohort is published in `2f021fa5`, exact green
`40563858491`. The content cohort then reviews 28 bindings across 15 emitted
functions, the identical F00795 sequence-search alias, and the deliberately
unused F03468 callback parameter. Workspace owner tags, asset requests,
peer-model searches, bank-versus-actor selectors and block-relative timeline
indices follow their original bodies and callers; all decisions are in the
existing function ledger and copier rules.

F00797 is corrected to `Content::CopyWeaponSequence`: the original function
rebased two addresses, but existing native on-disc fixups intentionally remove
those assignments. The native operation copies exactly `0x7800` bytes and keeps
both relative offsets, not the entire `0x7900`-byte declared buffer. T00460 and
its first two field descriptions now distinguish this native contract from
original rebasing without changing layout, field names, confidence or status.
The symbol change reaches both copy callers and the module-address registry.

On dirty base `2f021fa5`, nine controls pass 47 assertions before the rename.
Debug/Release then each pass 91 cases/1,370,357 assertions with
`[content],[animation],[actors] --reporter compact`. Controls cover the exact
copy prefix and untouched tail, offset preservation, workspace contention and
repeat release, section/track index order, sequence selectors, peer filtering,
one-based shared banks, and special-model record cleanup. Copier tests pass
130/130; naming tests 32/32. Focused tidy has three existing/zero new findings.
All 742 manifest paths and 121 owned paths are checked; two generations differ
only by the approved identifier changes across eight generated files and are
stable on repeat. Evidence: `build\quality\issue148-content-*`.

The content cohort is published in `56cc936f`, exact green `40566216538`.
The complete audio-scope review covers 64 implementation/header files and all
188 lexical candidate pairs/2,446 occurrences. Original bodies, callers,
producers and reset/release paths establish 468 binding names in 175 copied
definitions; 287 header parameters link to those same reviewed definitions.
Compound names preserve decompiler-merged lifetimes instead of splitting
variables or claiming one role for a variable reused for different purposes.
Each mapping's original evidence is retained in the function ledger and
function-scoped copier rules, applied after structural fixups.

The sole retained audio match is `spec` in `AudioDevice::Open`
(`Native\Engine\src\Audio\AudioDevice.cpp`): it is the concrete `SDL_AudioSpec`
device/stream format specification, initialized from format/channels/rate and
passed in all three format-address arguments. All four occurrences are reviewed.
This is not a global exception for other `spec` bindings. No owned C++ source
is renamed or promoted by this cohort.

On dirty base `56cc936f`, the pre-change `[audio]` selection passes
84 cases/171,666 assertions. After applying the mappings, Debug/Release each
pass `[audio],[event] --reporter compact`, 105 cases/172,359 assertions.
Dirty-tree copier tests are 131/131 and naming contracts 32/32; focused
tooling quality reports zero findings/new findings with unchanged baselines.
Original and native review fingerprints are checked before application.
Exact code-token substitution is verified across all 742 manifest paths,
including unchanged comments, literals, types and operators; only 34 audio
source/header files change. All 121 owned paths are unchanged and the second
generation is identical. Evidence: `build\quality\issue148-audio-*`.
The refreshed whole-tree lexical count is 21,485 occurrences/1,868 pairs/
315 files; the remaining source reviews continue independently of this checkpoint.

The audio cohort is published in `08f718ec`, exact green `40568510803`.
The complete remaining event-scope review accounts for 995 occurrences in
36 scanned files: 193 approved mappings across 90 definitions and 33 linked
header bindings. All native and original scope fingerprints are checked before
application; registered-owned F01130 has no candidate mappings and stays intact.

Two bindings intentionally remain, with individual ledger evidence:

- F00972 `new_var`: original `40564.c:1282/1291/1293` declares it, assigns a
  scratch address, then overwrites it with `0x10`, without any read. The incompatible
  write-only roles do not justify inventing a runtime meaning or deleting code.
- F01277 `arg0`: original `573B8.c:223-229` never consumes the parameter;
  the file selection uses the current global event instead. The native body
  and sibling header preserve the unused call contract.

On dirty base `08f718ec`, `[event]` first passes 22 cases/694 assertions.
After naming, Debug/Release each pass `[event],[core][math] --reporter compact`,
44 cases/932 assertions. Copier tests pass 133/133 and naming tests 32/32;
focused tooling quality reports zero new findings and no baseline changes.
The original negative opcode-scope control still asserts unchanged helper
parameters when applying the two general opcode rules. Separately reviewed
helper-specific names now have positive controls; applying all naming rules
to that old negative fixture was an obsolete test-scope assumption, not a
reason to suppress either contract.
Exact expected code-token changes are confined to 14 generated event files;
all 742 manifest paths and 121 unchanged owned paths are verified across two
stable generations. Evidence: `build\quality\issue148-event-remaining-*`
and `issue148-event-verification.json`. The remaining event matches are exactly
the five occurrences of the two reviewed retentions above. Whole-tree lexical
counts are 20,495 occurrences/1,778 pairs/303 files; other scopes continue.
