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

---

# Part 2: Globals, types and fields

The second naming pass covers the data. `tools\ledger\build_symbol_ledgers.py` generates three ledgers from the
libclang index, and the rules below apply to them. `tools\naming\merge_symbols.py` enforces the mechanical rules.

| Ledger | Row | Rows | What the pass fills |
|---|---|---|---|
| `DATA_LEDGER.csv` | `G#####`, one per **storage identity** (see §8.1) | 2,174 | `disposition`, `final_name`, `final_location`, `purpose`, `confidence`, `notes` |
| `TYPE_LEDGER.csv` | `T#####`, one per distinct type layout | 820 | `role`, `final_name`, `final_location`, `purpose`, `confidence`, `notes` |
| `FIELD_LEDGER.csv` | `T#####.NNN`, one per struct/union field or enumerator | 6,766 | `final_name`, `meaning`, `confidence`, `notes` |

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

## 10. Fields and enumerators (`FIELD_LEDGER.csv`)

- Struct/union fields: **lowerCamelCase** nouns (`currentHp`, `riskLevel`, `animationId`). Booleans read as
  predicates (`isVisible`, `hasShield`). Counts and indexes say so (`itemCount`, `slotIndex`).
- Enumerators: **PascalCase** without the enum prefix (`ItemCategory::Blade`); the C spelling is generated
  (`ItemCategory_Blade`).
- `unk*` is forbidden. If the code never reads a field and nothing explains it, name it `reserved<OffsetHex>`
  (`reserved1C`) with confidence `low`; padding the compiler would insert is `pad<OffsetHex>`.
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

After a complete batch passes `python tools\naming\merge_symbols.py --check <batch>`, merge it immediately with
`python tools\naming\merge_symbols.py --apply-batch <batch>`. This validates the batch against names already applied
in all three ledgers, rejects incomplete batches without writing, and leaves other rows unchanged.
The final gate remains `--check-all` followed by `--apply`; an individual batch does not satisfy that gate.
Regression tests: `python -m unittest discover -s tools\naming -p test_merge_symbols.py`.

### 11.2 Saved naming progress

The table below records validated outputs applied to the ledgers, **not native source migration**.
The function-name pass is separate. Naming globals/types/fields is still in progress; source copying and the
Phase 1a gate must not be marked complete from these results.

**Evidence backfill required (2026-10-16).** The evidence rule above was added after six batches were applied.
Rows applied before it that lack a citation are *unverified* and must be re-checked against the source and cited
before any of them is used in `Native\` code. At the time of the audit, 236 of 386 applied globals, 38 of 101 types
and 412 of 583 fields lacked a citation; `merge_symbols.py --check <batch>` lists them. The copy tool must refuse
names whose rows do not pass the evidence check.

| Batch | Globals | Types | Fields/enumerators | Output status |
|---|---:|---:|---:|---|
| Audio | 92 | 20 | 270 | Pending |
| Engine_01 | 151 | 38 | 252 | Pending |
| Engine_02 | 60 | 0 | 0 | Validated and applied |
| Game_Actors_01 | 0 | 55 | 768 | Pending |
| Game_Actors_02 | 48 | 8 | 108 | Pending |
| Game_Battle_01 | 0 | 92 | 575 | Pending |
| Game_Battle_02 | 0 | 66 | 689 | Pending |
| Game_Battle_03 | 0 | 78 | 648 | Pending |
| Game_Battle_04 | 58 | 58 | 526 | Pending |
| Game_Battle_05 | 114 | 0 | 0 | Pending |
| Game_Effects_01 | 0 | 78 | 655 | Pending |
| Game_Effects_02 | 239 | 9 | 53 | Pending |
| Game_Ending | 75 | 6 | 59 | Validated and applied |
| Game_Event | 69 | 17 | 160 | Validated and applied |
| Game_Field_01 | 63 | 59 | 500 | Pending |
| Game_Field_02 | 54 | 0 | 0 | Validated and applied |
| Game_Menu_01 | 0 | 95 | 557 | Pending |
| Game_Menu_02 | 38 | 63 | 582 | Pending |
| Game_Menu_03 | 262 | 0 | 0 | Pending |
| Game_Menu_04 | 262 | 0 | 0 | Pending |
| Game_Menu_05 | 262 | 0 | 0 | Pending |
| Game_Menu_06 | 199 | 0 | 0 | Pending |
| Game_Title | 121 | 21 | 82 | Validated and applied |
| Shared | 7 | 57 | 282 | Validated and applied |
| **Applied / inventoried** | **386 / 2,174** | **101 / 820** | **583 / 6,766** | **6 / 24 batches applied** |

Applied rows still include seven low-confidence globals, two low-confidence types and 65 low-confidence fields. Validation checks coverage,
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
- Event: T00219 is a merged-bitfield view of `gim_t` (5BF94.c); resolve both to one native splash-layer type.
  T00259's shake code reads its two oscillators from +0x1A6, two bytes after the declared field, and uses the first
  short as the active-shake count. G00442 is byte 3 of G00441. G00128/G00119 are command templates patched at
  runtime, so they are state, not constants.
- G02005/G02006 identify loading-image pixels/header through consumers, but their source definitions and extraction
  bindings are not located by the inventory. Converted asset paths in output notes remain proposed destinations.

The ending frame counter G01964 counts presented frames for XA resume seeking; it is **not** a frame duration.
The native audio player must own playback position rather than reading another module's timing globals.