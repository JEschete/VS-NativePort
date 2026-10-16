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
