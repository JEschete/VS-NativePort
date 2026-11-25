# Asset Pipeline: workshop recipes

[Asset pipeline index](../ASSET_PIPELINE.md)

### 6.8 Workshop recipes (SYD → CSV tables, #499)

`MENU\BLADE.SYD`, `SHIELD.SYD` and `ARMOR.SYD` are the workshop's combination recipes. Each one starts with a
12-byte header of three offsets: the pair matrix (the result record by both ingredients' record indices), the
material matrix (the material two ingredients' materials give; blades and armor also index it by category) and the
result records (16 bytes for a blade, 8 for a shield or armor piece). `VS.Extract tables <datadir>` (and `extract`)
converts each file to three CSV tables under `catalogs\menu\` (`Extract/WorkshopRecipeConversion.hpp`,
`Content/WorkshopRecipes.hpp`). Columns are described by the committed schemas in
[`catalogs/menu/`](../../Native/Content/schemas/catalogs/menu/) (e.g.
[combination-blade-results.schema.json](../../Native/Content/schemas/catalogs/menu/combination-blade-results.schema.json)):

- `combination-<kind>-results.csv`: one row per result definition, with a semantic `id` (`blade.battle-knife`) and
  its compatibility `legacy_id` (the record index the original matrices use; record 0 of each kind is the empty
  one). Record bytes the original never reads are not columns (see reserved bytes below).
- `combination-<kind>-pairs.csv`: result ids, one row per ingredient pair that makes something.
- `combination-<kind>-materials.csv`: one row per combination that gives a material.

Empty matrix entries have no row. Each table's schema is written beside it and must equal the committed copy (a
test checks this). The manifest lists each kind as one output (`catalog.menu.combination-<kind>`, converter
`syd-recipe-csv`) holding its six files and the source hash.

Conversion keeps edits. A rerun leaves the tables of an unchanged SYD alone, reconverts those whose SYD changed and
with `--force` rewrites them all. The converter only writes tables that parse back to the decoded recipes and
rebuild the original file byte for byte, so a disc that does not convert exactly is an error rather than a lossy
output. At startup the data store reads and checks every table (types and ranges, every reference names a result,
ingredients fit the pair matrix, `legacy_id`s run from 0 without gaps) and refuses to open on the first error,
naming the file, line and column and telling the user to rerun `VS.Extract tables`. `RequireConversions` refuses to
start when a SYD has no tables or its source hash is stale. The workshop builds its recipe book from the loaded
tables (`Menu::Setup::BuildRecipeBook`, which rebuilds the original's dense matrices) and never reads the SYD.

Open work: #58, #499 (shared item-definition references, retirement of the recipe matrix bridge, an in-game
combination replay).

#### Semantic recipe identities and import-only records

`VS.Extract authoring <datadir>` upgrades generated `<kind>-NNN` result keys and every directed pair reference in
place, without reconverting source data. `tables` and full `extract` also run this step. Existing custom identities,
labels and value edits are preserved. The importer uses the source item names; ambiguous or unencodable names
require an explicit identity, not a numeric suffix or an invented meaning.

The two Footman's Maces use their verified categories: `blade.footmans-mace.axe-mace` and
`blade.footmans-mace.heavy-mace`. The same rule disambiguates duplicate names only when a known category
distinguishes them. Remaining collisions fail explicitly.

The five stock `untitled` tail records have no recipe references. They move to
`catalogs/menu/import/combination-blade.json` as original record bytes keyed by `legacy_id`, described by
[workshop-import.schema.json](../../Native/Content/schemas/workshop-import.schema.json). This is **import
provenance**, not another item catalog. The game does not read these records; the offline
`EncodeWorkshopAuthoring` export restores them for exact legacy reconstruction. An edited/referenced record or
custom caption is not discarded: it needs an explicit authoring identity. Explicitly authoring a former unused slot
takes precedence over its archived original.

Result records also have reserved bytes: blade bytes 7 and 11 and shield/armor byte 7. `combineBlades`,
`combineShields` and `combineArmor` (`MENUC.PRG/168.c`) never read them, and they are zero on the US disc, so they
are not table columns. A non-zero value is kept in the same import file's `reserved_bytes` list (schema version 2) by `legacy_id`
and offset. Only the offline export writes it back. `authoring` moves the former `raw.byte7`/`raw.byte11` columns
there, dropping zeros, and upgrades version 1 import files. A tree that still has those columns refuses to load
until `authoring` runs.

Result schemas describe their old-ID alias pattern. Readers resolve aliases through `legacy_id`, normalize pairs to
current IDs and reject ambiguous aliases or duplicate resolved directed pairs. Old numeric identifiers do not remain
the primary authoring keys, and renaming a display label does not rename an ID.

The same pass writes verified domains as names: materials (`bronze`, `iron`, `hagane`, etc.), blade categories
(`dagger`, `axe-mace`, `heavy-mace`, etc.), damage (`blunt`, `edged`, `piercing`), resource cost (`mp`, `risk`,
`hp`, `weapon-pp`, or `none` with zero cost), attack shapes (`ellipsoid`, `cone`, `rectangle`, etc., the targeting
volumes that actions' `range_shape` also uses) and armor categories (`shield`, `helm`, `breast-armor`, `leggings`,
`glove`; armor material rules accept only the last four). Each CSV schema carries its code-to-name `enum` map.
Generated material-rule captions name the categories (`Bronze + Iron -> Hagane (helm + glove)`); `labels` replaces
the old `(categories N + M)` captions it generated but keeps edited ones.

`model` is the blade or shield WEP number (`OBJ/<hex>.WEP`, converted to `models/weapons/<hex>.gltf`); armor
results keep 0. It stays a number until item definitions reference model assets (#204, #58). Numeric aliases
remain explicitly accepted for legacy imports; the authoring pass emits canonical names, and key comparisons
normalize aliases so numeric/named spellings cannot create duplicate rules.

`cost_type` is the weapon resource selector, not `ActionDefinition::type`: `EquipmentStats` passes it through
`WeaponLoadout.action_type` to `ActionCostsLive` and the resource mapping in `ActionCosts`. Unsupported selectors
remain byte-preservable by the legacy codec but are refused as native authoring values. Numeric quantities remain
numeric. Other enum/flag families still require their own verified mappings.

The shield/armor overlap is not byte-identical: some shield records have gem slots where the corresponding armor
import byte is zero. Original and native `combineArmor` do not read the imported model or gem-slot bytes, whereas
`combineShields` does. Those context-specific values remain exact. Shared IDs must agree on the category, stats and
legacy identity both paths consume; divergent authored variants require distinct IDs. A single shared item
definition with all import-only fields separated is still part of #499.

#### Layered compiled workshop catalogs (#499)

The data store loads the nine workshop CSV tables and the material definitions (§4.2) when their outputs are listed,
applies the ordered roots in optional `catalog-layers.json`
([schema](../../Native/Content/schemas/catalog-layers.schema.json)), validates their values/references and compiles
them into a private SQLite database. Production recipe books are built from queries of that database, not a separate
unpatched CSV path. The importer keeps its base-only path so offline preservation/reconversion does not bake mods
into the source catalog.

Layers implement plan section 9.6: partial-field merge by schema key, complete add/replace, and explicit remove.
Directed pair keys preserve order. Enum codes and legacy result aliases normalize before matching; ambiguous
identities and duplicate resolved keys fail. Later roots win with a deterministic conflict report. Numeric values,
readable enum names and row source locations survive SQLite compilation. Foreign keys are enabled; compilation is
transactional and the published database is read-only.

Validated catalogs persist as `.cache/catalog.sqlite`. The input fingerprint includes actual CSV and schema contents,
the serialized compiler schema contract, compiler version, layer configuration and every selected layer file in
deterministic root/file order. CSV timestamps are never an authority. Unchanged input opens the cache read-only and
snapshots it into the owned immutable catalog; releasing its file handle permits atomic replacement on Windows while
a previous store remains alive. Conflict provenance is preserved.

Compilation reads an immutable snapshot of the same input bytes that were fingerprinted, then checks the current
fingerprint again. Only successfully typed consumer records may publish the serialized SQLite file. Failed
validation or changed inputs leave the old cache intact; missing/corrupt/stale caches rebuild. An unavailable cache
does not make a read-only asset tree unusable. Base files and import provenance are never changed by loading layers,
and a SQLite cache does not retire EC-04-T-SYD's remaining recipe/item layout constraints. See
[BUILDING.md section 5.1](../BUILDING.md#51-recipe-patch-and-mod-layers) for the configuration and patch syntax.
Other catalog families and asset-path overrides are not adopted; these bounded slices do not complete #499 or #520.
