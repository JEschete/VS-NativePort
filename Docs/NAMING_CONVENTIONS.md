# Naming & Placement Conventions (Phase 1a, Step 3)

The original naming campaign and C++ promotion (#276) are complete. Historical
batch inputs/outputs, the batch-specific generators, namespace consolidation and
sanity-fix replay scripts were retired in #440. Their last version is retained in
[Git history](https://github.com/JEschete/VS-NativePort/tree/d654bad79/tools/naming).
Accepted names and evidence remain in the canonical ledgers, not those old batches.
The generic review/merge tools below remain available for new, scoped reviews;
their batch directories and reports are ignored scratch outputs.
Shared layout and citation support lives in
[`symbol_support.py`](../tools/naming/symbol_support.py). Historical campaign
examples below do not authorize replaying retired scripts over current ledgers.

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
  (`Menu::Files::*` lives in `Game\src\Menu\Files.cpp`, `Effects::Plg074::*` in `Game\src\Effects\Plugins\Plg074.c`,
  one grouping folder allowed). Root-namespace functions live directly in the root folder. `merge_names.py` enforces
  this.
- One concept, one namespace. Don't create near-synonyms (`Seq` / `Sequence` / `Driver\Sequencer`,
  `Random.c` / `Random\Generator.c`). The first-pass consolidation rules are archived
  with the completed campaign; current placements are in the canonical ledgers.
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

- The decomp (read-only): `resources\rood-reverse\src\…`, plus `include\` for struct definitions. The pinned copy
  `resources\rood-reverseWin` that older rows cite was removed in `6b20016f0`.
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
- Native namespace-scope variables follow the same rule. `Native\.clang-tidy` enforces it (`GlobalVariableCase:
  camelBack`, #453) and exempts only the copied `System_name` C spellings until #441 retires them.

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
  used across projects, or `Native\<ProjectDir>\src\<Area>\<File>.h` for types private to one module. Once the native
  type exists it is that type's file; `gates.py --check` rejects a live row naming a file that does not exist (#471),
  so a type whose file is deleted is pointed at its native replacement or retired.
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

### 10.1 Native evidence and trace results (#445)

The native tree is the source of truth (D24). `tools\naming\trace_queue.py` builds `TRACE_QUEUE.csv` from the
ledgers, the libclang index of the native tree (`tools\naming\native_refs.py`) and the read-only upstream
`resources\rood-reverse` symbol files; it does not use `rood-reverseWin`, the copier or the name-merge pipeline.
Settle a row by editing its ledger row directly, then regenerate the queue. Each settled row cites its evidence in
`notes`:

- `native-refs:<class>`: how production native code uses the item (`Native\Tests\` and layout assertions do not
  count; a `= default` body, such as a defaulted comparison, counts only when production code uses it, #470).
  `none`: never used; `write-only`: only written; `copy-only`: only copied into members that are themselves
  never read; `read`: read. `trace_queue.py --resolve` records the first three at `med` with the meanings it sets;
  a meaning that already says what the item is in the original is kept, with the native finding appended.
- `source:native:<path>:<line>`: the native line behind that class (the first writer or copier, else the
  declaration). Every `trace_queue.py` run recomputes the line from the index, so header edits do not leave it
  stale, and lists recorded classes that native code no longer matches for review (#457). It also lists field rows
  whose `final_name` matches the native member only when letter case is ignored: `final_name` gives the code's
  exact spelling (#458). The run refreshes a citation only when the `native-refs:` part starts `notes`, so
  `gates.py --check` rejects one placed later in a live row (#471).
- `trace:<scenario>:<observation>`: a runtime result. `<scenario>` names the log
  `tools\naming\traces\<scenario>.md`, `<observation>` is a short kebab-case result
  (`trace:map009-door:read-on-open`). A trace that establishes the meaning raises the row to `med` or `high`.
- `disc-refs:` or a free-text disc citation: accesses found in the disc executables with
  `tools\naming\disc_refs.py <Gid|0xADDR:size>`, which reports access-width overlaps. A base address formed with
  `addiu` and used with an offset reaches a neighbour, not the target: check the surrounding range and disassemble
  the sites before calling an item unread.

A global that the disc shows is only written is retired like G01961 (91f560147): remove the hooks that exist only
for the write, leave a provenance comment at the original write and mark the row `copied=retired` with a
`retired-by:` note. A global that is really a member of another record is retired with `retired-by:<owner>
<member>`. A `reserved` member confirmed unread in its scenarios becomes padding or a drop candidate.

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
  row cites at least one line naming the original member. Maintained tools use
  `tools\naming\symbol_support.py` (`Gen`): `c(("file", "text"))` cites either tree
  (`upstream:` prefix for resources\rood-reverse), `T(...)` auto-cites a field's first access through a receiver
  pattern, `g.decl()` cites a declaration explicitly, and `write()` refuses a field whose cited lines do not name it.
- Member uses are searched first in functions whose libclang field references (`build\port\index.json` `refs.field`)
  contain the exact `Type.member`, so `actor->unk0.x` is found for the header type even when the function never
  names that type. `evidence.py` and `symbol_support.Gen` both do this. Auto-citations still land on reset lines (`= 0`) that
  prove identity but not meaning; pin meaning-bearing lines explicitly for every name above `low`.

After a complete batch passes `python tools\naming\merge_symbols.py --check <batch>`, merge it immediately with
`python tools\naming\merge_symbols.py --apply-batch <batch>`. This validates the batch against names already applied
in all three ledgers, rejects incomplete batches without writing, and leaves other rows unchanged.
The final gate remains `--check-all` followed by `--apply`; an individual batch does not satisfy that gate.
Regression tests: `python -m unittest discover -s tools\naming -p test_merge_symbols.py`.

### 11.2 Rules carried forward from the naming and copy passes

The per-batch progress table and evidence-backfill log (former §11.2) are in
`git show b0383a255:Docs/NAMING_CONVENTIONS.md`; accepted names and evidence are in the canonical ledgers.
The dated logs for the low/med sanity pass (2026-10-17), the catalog-gap and reference-index work (2026-10-18), the
copy-time naming fixes (2026-10-17) and the #148 native semantic sweep (2026-10-27) are in the
[archived version of this file](https://github.com/JEschete/VS-NativePort/blob/8db4f46e292aa19c382807757786d6f07f65c71c/Docs/NAMING_CONVENTIONS.md#113-lowmed-sanity-pass-2026-10-17)
(§11.3-§11.6). These rules from those passes still apply:

- **Sanity review.** `tools\naming\sanity_pass.py` writes candidates to the ignored
  `tools\naming\sanity\report.csv`. Accepted campaign corrections are already in the
  canonical ledgers. Review new findings against source and update the applicable
  ledger rows; do not replay the retired sanity-fix script or historical batches.
- **Low and opaque rows need tracing, not renaming.** Most low fields are `reserved` (never read) or `opaque` (read, but
  the meaning is not established). See §10.1, #445 and `tools\naming\trace_queue.py`.
- **Catalog additions.** `tools\ledger\add_missing_functions.py --batch <name>` catalogs functions the scanner missed:
  `glabel`/`alabel` inline-assembly entry points in `.c` files, `jumpFunc NAME, TARGET` lines and `.s` alternate entry
  points that C code references. It never overwrites an existing batch. Do not re-run `build_ledgers.py`, which
  renumbers every function ID.
- **Reference index.** `tools\port\ref_index.py` resolves every identifier occurrence in game code to a ledger row. A
  full run takes about 10 minutes. Re-run it after any ledger or naming change; the expected result is 0 unresolved
  occurrences and 0 text mismatches.
- **Stale split file.** `EFFECT/PLG053.BIN/0.c` is not built (upstream deleted it). Its rows F01921-F01923 carry
  `not-linked`, which `merge_names.py` turns into `copy = no`.
- **Link alias.** `_toVsStringPercentAlias` exists only as a `--defsym` in `config/MENU/MENU9.PRG/Makefile`; the index
  maps it to `_toVsStringPercent`.
- **Effect template instances** are named after the template body they feed: the first `#include` after the binding
  whose header defines that macro's function. A binding whose header does not define it is a reference,
  `template-binding;dup-of:<instance>`. A `dup-of` row may point at a template instance and use its `Templates\`
  location. `merge_names.py --apply` replaces a row's old `dup-of` with the batch output's.
- **Function-local collisions.** When one function has two unused layout-padding statics with the same name, the second
  is `...LayoutPadding2`.
- **Same-as types.** The copy writes only the canonical type, so `tools\port\copy_fixups.py` maps nested types of a
  same-as type to the canonical ones (for example T00794 -> T00784, T00838 -> T00836). A field reference through a
  same-as type uses the canonical type's field at the same index.
- **Ending frame counter.** G01964 counts presented frames for XA resume seeking; it is **not** a frame duration. The
  native audio player must own playback position rather than reading another module's timing globals.
- **Native symbol inventory.** `python -m tools.naming.native_symbol_inventory` lists short or opaque native identifiers
  as candidates for review. It grants no approvals. The reviewed ledger from #148 (`NATIVE_SYMBOL_REVIEW.csv`) has
  been retired (#273), so there is no maintained `--review --check` gate.
