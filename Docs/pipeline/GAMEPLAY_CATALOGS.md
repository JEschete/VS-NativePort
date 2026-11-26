# Asset Pipeline: gameplay catalogs

[Asset pipeline index](../ASSET_PIPELINE.md)

The shared catalog conventions (commands, `display_name`, schemas, import provenance, layered SQLite/cache,
refusal) are in §4 ([OUTPUT_AND_TABLES.md](OUTPUT_AND_TABLES.md)); each section below states only its own rules.

### 4.3 Native encyclopedia definitions (#499/#500)

`VS.Extract bestiary <datadir>` imports `SMALL\MON.BIN` into `catalogs\enemies\bestiary.csv`
([schema](../../Native/Content/schemas/catalogs/enemies/bestiary.schema.json)). `bestiary.*` IDs identify
encyclopedia entries, **not spawned enemies**. `name` and `description` are IDs in the shared native text
catalog (§5.2). The layered SQLite compiler owns validation and immutable runtime records.

- **Records:** the file holds 150 44-byte records and a 150-entry description offset table. Only records
  **0..77** enter the original list, selection and previous/next navigation; the detail viewer can only select
  those. The other **72 records are out-of-list provenance**, not enemy definitions: their zero fields,
  Japanese-name bytes and empty descriptions are kept without assigned meaning. `catalogs\enemies\import\bestiary.json`
  retains all 150 records and description extents, including initial UI values, reserved/name-suffix bytes and padding.
- **Rules beyond the schema:** `legacy_id` is the visible record 0..77, or -1 for native-only entries.
  `display_order` is dense in the resulting list; the three-digit view admits at most 999 entries.
  `preview_table_index` (0..255) is an explicit **MENU9 preview-table adapter** selecting that table's model,
  equipment and palette policy, not an actor/species identity; #200/#55 own its replacement. The
  `defeated-subtype-range` unlock tests any saved defeat bit in `[first_defeated_subtype, first + count)`; those are
  **actor subtypes** (the original producer writes subtypes below 256), not MON record numbers or preview indices.
  `always` exposes a native information entry with both range fields zero. Neither policy writes progress or
  implies a spawn.
- **Sources:** upstream MENU9 `238.c` (definitions 372, unlock 1921, list 1977, detail 2074), preview `592C.c`
  (table 28) and BATTLE `146C.c:1437` (defeat-bit producer). Extending actor identities or the persistent defeat
  model remains #199/#200/#203, not an inferred field meaning.
- **Ownership:** the Gazette owns its snapshot, selection/highlight/link state and native layouts separately from
  the definitions; cancel, re-entry, MENU9 replacement, cold-session and pre-heap boundaries release them. Native
  cursor/link indices are not bytes; the old BATTLE byte cursor is a limited compatibility bookmark. Native text has
  no 28-byte name limit. List text uses a 220-pixel content box and a wider background; the viewer gives the name its
  own lower-left position. These view bounds and the eight-row/999-entry policy are deliberate, not container limits.
- **Reruns and refusals:** normal imports preserve authored CSV/text and foreign text contributors;
  `bestiary --force` reimports only this family, not save prompts or neighboring tables. Missing definitions, bad
  references, invalid ranges or layouts refuse entry rather than falling back to a legacy read.

Open work: #499, #500 (model preview, actor assets, progress storage and the GPU page/packet backend are tracked dependencies).

### 4.4 Native learning requirements (#499)

`VS.Extract learning <datadir>` imports two immutable tables. `learning-csv` v1 owns `catalog.battle.learning`:
`catalogs\battle\break-art-learning.csv`, `battle-ability-learning.csv`
([schema](../../Native/Content/schemas/catalogs/battle/break-art-learning.schema.json),
[schema](../../Native/Content/schemas/catalogs/battle/battle-ability-learning.schema.json)) and
`catalogs\battle\import\learning.json`. Layers publish checked `Content::LearningCatalog` definitions; the battle
and both menu readers use them directly, with no reconstructed halfword table, module copy or address binding.

| Table | Identity and meaningful order | Editable quantity |
|---|---|---|
| `break-art-learning.csv` | `learning.whistle-sting`, etc.; named weapon category, `first`..`fourth` tier, named action reference | `required_weapon_kills`: cumulative qualifying kills in that category, **not** an incremental price |
| `battle-ability-learning.csv` | `learning.first-battle-ability-choice`, etc.; `choice_order` 1..22 is the next opportunity to choose an unlearned chain/defense ability, **not** an action ID | `required_ability_points`: cumulative successful offensive/defensive timing credit, despite the retained save name `total_kills` |

- **Categories:** bare hands, dagger, sword, great sword, axe/mace, great axe, staff, heavy mace, polearm and
  crossbow, reusing `GameplayDomains.hpp`'s blade-category domain with only category zero renamed. The schema's
  enum codes adapt the retained equipment/progress indices. Equipment category 10 maps modulo 10 to **bare hands**,
  not material `none`; the zero equipment alias normalizes to category 10 for the action and notification name
  while reading progress category zero. Action codes 184..219 belong to weapon categories 1..9; 220..223 are the
  four bare-hands arts. Action references, category/tier and identities must agree.
- **Refusals:** unsupported identities, missing definitions, duplicate keys/orders, bad values and mismatched
  references refuse publication with file/row/column diagnostics. General mod-defined action/category/progress
  identities await #499/#58; no extra compatible action slots are invented.
- **Thresholds:** nonnegative signed 32-bit native quantities, including values above 65535. No monotonic rule:
  production checks the next tier, so a lower later threshold can become eligible immediately. Both next-points
  displays use safe decimal arithmetic and `max(0, requirement-progress)`; more than five remaining digits use one
  right-aligned `NEXT <value>PT` run instead of the original label/value slots (stock positioning unchanged).
- **Progress stays separate:** mutable kills, learned counts, chosen actions, notification phases and saves.
  #183's owner policy still banks successful ability timing, including lethal hits and pending rewards, saturating
  at 65535; only a qualifying death starts learning. A weapon-kill award the uint16 counter cannot hold is refused
  and rolls back its kill-counted mark; it does not truncate the threshold or add a clamp. Changing persistent
  progress width is separate work. Threshold edits are authored content, not earned progress.
- **Original extent:** `src/SLUS_010.40/main.c:722,735,751,755` and the executable agree on 80 requirement bytes at
  `0x405E4`, 40 unused `artsActionIds` bytes at `0x40634`, 44 requirement bytes at `0x4065C`, then 256
  `actorExtraShp` bytes at `0x40688` (the old G01916/G01917 extents wrongly included the gaps). Only **40 plus 22
  requirements** become definitions. Original CSV snapshots and the two neighboring byte arrays reload independently
  of edits and reconstruct all 420 bytes **offline**. G01918's actor-shape consumer is unmigrated; the placeholder
  actor's name no longer reads a fake threshold tail (native padding is zero, original bytes stay offline).
- **Reruns:** edited files and timestamps survive; missing owned schemas are repaired without replacing owned CSVs.
  Foreign output files are refused even with `--force`; a changed source requires explicit force before replacing
  owned content. Material, bestiary, text/font, FAR and save assets keep their ownership.

### 4.5 Native status rules and shared timing (#499)

`VS.Extract status <datadir>` imports **only G01906**, the 32 sixteen-byte records at SLUS offset `0x3F6CC`, as
bundle `status-csv` (`catalog.battle.status`):

- `catalogs\battle\status-rules.csv` ([schema](../../Native/Content/schemas/catalogs/battle/status-rules.schema.json)):
  semantic rule IDs and named `effect_channels`, `replaces`, `blocked_by` sets.
- `catalogs\battle\status-timings.csv` ([schema](../../Native/Content/schemas/catalogs/battle/status-timings.schema.json)):
  six named shared duration/pulse roles.
- The schemas define named flag members with explicit original bit mappings and must equal the generated schemas
  (a test compares them). Sets use `|`, or `none`; numeric masks, unnamed bits, duplicate members and unknown names
  are not native authoring syntax.
- `catalogs\battle\import\status.json` keeps **every word of every original record**, including unused/duplicate durations.

Timing is measured in **eligible status updates**, not seconds: the vitals loop calls the status updater at counter
multiples of 60, only for an eligible living actor. AI estimates the same poison/regeneration intervals when planning.

| Authored role | Actual state affected | Original source |
|---|---|---|
| `timing.attribute-modifiers` | Strength, Intelligence and Agility up/down share timer 0 | Row 5 duration low byte |
| `timing.movement-modifiers` | Quicken and impaired legs share timer 1; recovered legs can clear earlier | Row 11 duration low byte |
| `timing.equipment-and-elements` | Equipment up/down and all eight elemental enchantments share timer 2 | Row 19 duration low byte |
| `timing.paralysis-and-numbness` | Separate timers 3/4, both initialized from this one definition | Row 13 duration low byte |
| `timing.poison-pulse` | HP pulse timer 5; poison wins if both effects are present | Row 14 interval |
| `timing.regeneration-pulse` | Same HP pulse timer 5, not a separate regeneration timer | Row 17 interval |

The other row durations are **not independently effective**: equipment-up/element durations and numbness's own
row duration are preserved offline, not offered as per-rule controls. The retained actor/save timers are bytes:
definitions above 255 refuse; zero duration means no expiry countdown; HP pulse intervals must be positive.
Original AI reads the full interval halfword but lifecycle uses its low byte; non-stock originals where these
disagree reconstruct offline but refuse native adoption (no silent narrowing or new actor sidecar).

The last three bits were resolved by **consumers and producers**, not position:

- 29 is **Analyze**. Original `func_80083D7C` queues it and independently marks the target analyzed on execution,
  even if the rule has no effect.
- 30 is the **instant-defeat channel** checked by the undead/living prerequisites and removed by the effect that
  requests all current HP. It is not a timed “Spirit Drain” ailment.
- 31 is **spell immunity**, not a reserved flag. Enemy initialization copies this resistance bit into active
  statuses; `doesAttackHit` zeroes a spell's threshold and `func_80085390` suppresses spell dispatch. No fixed
  original `applyStatus(31)` caller exists. The retained rule record does not author enemy spawn resistance.

Sources: pinned decomp `c9a869b8919efc561894fb6a851b300791553bd3` `main.c:524`, `BATTLE/BATTLE.PRG/146C.c:6085`,
`:10072`, `:10429`, `:11129`, `:12287`, `:12432`, `:12858`, `:13697`, `:14243`, `:14565` and `74088.c:591`,
checked against the original binary. Legacy callback names containing “Banish” remain call-boundary names; they
do not define the catalog's meaning.

Definitions are independent of mutable actor statuses/timers and module resets. G01906's module copy/address/storage
is removed; neighboring globals are unchanged. This is **not** action, enemy-resistance, actor/save-width or
whole-executable adoption.

### 4.6 Native action categories (#499)

`VS.Extract action-categories <datadir>` imports the sixteen category records BATTLE.PRG stores as five parallel
byte tables (needs-selection `0x7F984`, area `0x7F994`, includes-user `0x7F9B4`, body-part `0x7F9C4`, target-kind
masks `0x7F9D4`; the sixteen bytes at `0x7F9A4` are zero padding). Bundle `action-category-csv`
(`catalog.battle.action-categories`) writes `catalogs\battle\action-categories.csv` with `action-category.<slug>`
rows. Columns, enums and flag bits are in
[action-categories.schema.json](../../Native/Content/schemas/catalogs/battle/action-categories.schema.json); the
committed schema must equal the generated one. `target_kinds` is a `|` set (or `none`).

`catalogs\battle\import\action-categories.json` holds the five original bytes of every record. The importer refuses
a source whose padding bytes are not zero or whose mask uses the unnamed `0x40` bit, and authoring cannot spell either.

Live targeting, camera selection and action-result code read the checked catalog through
`RequireLiveActionCategory`. A missing catalog logs and aborts like the status catalog; it never yields an all-zero
rule. Module globals G00019..G00023 and their address bindings are removed.

Actions name their category by identity (§4.7). The retained action definition array keeps a four-bit category
field, so at most sixteen categories exist. AI category sets, action preview lists and the damage distribution
policy remain keyed by category number in code.

### 4.7 Native actions (#499)

`VS.Extract actions <datadir>` imports the 256 fifty-two-byte action records of G01904 at SLUS offset `0x3C1DC`.
Bundle `action-csv` (`catalog.battle.actions`) needs the action categories published too: opening a root with
actions but no action categories fails and says so. Columns, enums and flags are in
[actions.schema.json](../../Native/Content/schemas/catalogs/battle/actions.schema.json) and
[action-hits.schema.json](../../Native/Content/schemas/catalogs/battle/action-hits.schema.json); the committed copies
must equal the generated ones. Numeric spellings of an enum are accepted as legacy aliases and read back as the name.
Rules beyond the schemas:

- `catalogs\battle\actions.csv`: one row per action. `id` is `action.<slug>` from the in-game name, with `-2`, `-3`
  after a repeated name and `untitled-NNN` for placeholder records. `name` allows only the `{level}` markup.
  `legacy_id` is the index saves and scripts still use. `category` references an `action-category.<slug>`.
  `behavior` is a `|` set of the twelve named AI bits plus `menu-action`, `hits-first-obstruction` and
  `starts-unlocked` (or `none`).
- `catalogs\battle\action-hits.csv`: one row per hit, keyed by `action` and `slot` (`first`/`second`). Every column
  but the factor is a name. An action with no second hit has no second row; an all-zero row is refused. `effect`
  names the battle effect table slot the hit dispatches: `apply-*` and `cure-*` add or remove that status,
  `recover-hp-on-*` heal only when the answered hit matches, the `mark-success-N` slots behave alike, and the
  `unused-N` slots, empty in the original, are refused at run time. What every value does, with the formulas, is in
  [§4.7.1](ACTION_HITS.md#471-action-hit-values-520). `VS.Extract authoring` spells numeric codes in an existing tree
  by name and refreshes both schemas.
- `catalogs\battle\import\actions.json` holds the bits the catalog does not keep: the four unread AI bits, the
  reserved flag nibble, the reserved word and the bytes after each name's terminator. The importer refuses a source
  whose hit codes have no name (effects 70-127, prerequisite codes 40-63, stat calculators 27-63).

**Bridge (EC-04):** `Content::LoadModuleData` calls `LoadCatalogBackedGlobals`, which fills the
`Battle::actionDefinitions` array from the catalog through `Battle::BuildActionTable`. This is a **temporary
bridge**: code still reads the 52-byte bitfield record, and the table retires with those readers under #499/#58.
A missing catalog logs and leaves the table unchanged; it never yields an all-zero rule. The module data entry for
G01904 is removed.

Open work: #499, #58 (the action's own `effect_id` is still an unnamed index; identically behaving hit prerequisites
and effects keep separate names; Learning's `action` column names break arts by slug rather than action identity).
