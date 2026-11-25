# Native Source Port Plan: Data contracts and patterns

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md)

## 9. Data contracts & patterns

### 9.1 Def vs Instance (Type Object pattern)
The original mixes the two. For example, `vs_main_inventoryBlade` stores `u_char id` plus the mutable DP/PP and bitfields in one struct. We split them:

```cpp
struct BladeDef {                 // immutable, from catalog
    ItemId id;                    // "blade.scramasax"
    TextId name, help;
    BladeCategory category;       // Dagger, Sword, ...
    DamageType damageType;        // Blunt, Edged, Piercing
    Material defaultMaterial;
    int16_t str, intel, agl;
    int16_t range, risk;
    uint16_t maxDp, maxPp;
    std::vector<GripCategory> compatibleGrips;
    ModelRef model;
};

struct BladeInstance {            // mutable, lives in GameState / save
    Handle<BladeDef> def;
    Material material;
    uint16_t dp, pp;
    ClassAffinities classAffinity;   // human, beast, undead, ...
    ElementAffinities elementAffinity;
};
```

### 9.2 String IDs → runtime handles
- Catalogs and saves use **stable string IDs** (`"blade.scramasax"`, `"enemy.minotaur"`, `"room.wine_cellar.entrance"`).
- At load, a `Registry<T>` interns them into dense `Handle<T>` (a `uint32_t` index) for O(1) lookup. Code uses handles, and data and saves use strings.
- Phase 1 also records the **original numeric ID** (`legacyId`) on every Def, so that copied logic and `.mcd` import still work.

### 9.3 Catalogs (one schema each, `Native\Content\schemas\`)

| Catalog | Source on disc (to verify per CONTENT_LEDGER) |
|---|---|
| `blades`, `grips`, `gems`, `armor`, `shields`, `accessories` | `SMALL\*.DIS/*.ARM`, SLUS tables (the `MENU\*.SYD` files are workshop recipes, not item definitions) |
| `consumables`, `materials`, `misc-items` | SLUS/BATTLE tables |
| `item-text` | `MENU\ITEMNAME.BIN`, `ITEMHELP.BIN`, `SMALL\HELP*.HF0/HF1` |
| `species`, `enemies` (base + `extends` variants), `modelsets`, `bestiary` | ZND enemy sections (455 records in 49 zones), ZUD archives, `MON.BIN`. See [ENEMY_DATA_SCHEME.md (archived)](https://github.com/JEschete/VS-NativePort/blob/0e3ae7d8df169bc684c8155fa44b90e731bb4e69/Docs/ENEMY_DATA_SCHEME.md) |
| `loot` (shared drop tables) | Per-slot drop rates lifted out of the ZND records |
| `break-arts`, `battle-abilities` (chain/defense), `spells`, `status-effects` | BATTLE tables |
| `workshops`, `combination-<kind>-results`/`-pairs`/`-materials` | the blade, shield and armor combination recipes in `MENU\*.SYD`, converted (#499, ASSET_PIPELINE.md §6.8); the workshop tables are still to identify |
| `rooms` (incl. `spawns[]` with conditions), `zones`, `doors`, `keys`, `chests`, `save-points` | MPD/ZND + SLUS tables |
| `text` (dialog, UI) | event data, MCDATA, menu text |
| `music`, `sfx` | sound bank/sequence index tables |
| `effects` | EFFECT plugin parameters (480 template instantiations) |
| `events` | event script data |
| `title` | title-screen layout, timings (1000/100 tick timeouts), menu items |

### 9.4 Schema rules (from DW4 `schemas\README.md`)
- Every file has a schema version: a `"schema_version"` member in JSON, and the version in the table's `<table>.schema.json` for CSV tables. Loaders accept known versions only, and migrate forward (never downgrade).
- Validation at load: types, ranges, references (every `ItemId` resolves), uniqueness. Errors name the file, the location (JSON pointer, or CSV row and column) and the rule.
- After validation, data becomes **immutable** typed records (frozen registries).
- Converted payloads (`Native\data\`) are git-ignored. Only the schemas, the extractor and the patches are committed. See [ASSET_PIPELINE.md](../ASSET_PIPELINE.md).

### 9.5 Example catalog entry

Flat tables are CSV (D25), so they open in any spreadsheet. A list becomes its own join table instead of a nested
array. `catalogs\items\blades.csv`:

```csv
id,legacy_id,name,category,damage_type,str,int,agl,range,risk,max_dp,max_pp,model
blade.scramasax,2,text.item.blade.scramasax.name,dagger,piercing,3,0,0,5,1,50,50,model.wep.02
```

`catalogs\items\blade_grips.csv`:

```csv
blade,grip
blade.scramasax,grip.short_hilt
```

(Values illustrative. Real values come from VS.Extract.)

### 9.6 Patch layers (mods)
Load order: `Native\data\` (converted from the disc) → `Native\Content\patches\` (our fixes/enhancements) → `mods\<name>\` (user). Each layer mirrors the `Native\data\` layout. Mods may also replace converted assets (glTF, PNG, WAV, sequences, movies) by path.
Patch operations: `add`, `replace`, `merge` (field-level), `remove`, by ID. In a CSV layer, a row whose `id` already exists merges the columns that the layer's header lists, and a new `id` adds a row. An optional `_op` column selects `replace` or `remove` explicitly. A conflict report lists every ID that more than one layer touched. The merged layers are compiled into one SQLite database for queries; the CSV files remain the source.

### 9.7 Behaviors stay in code
Catalogs hold **parameters**. **Behaviors** (AI routines, effect kinds, event commands, break-art scripts) are C++ registered by name in a `BehaviorRegistry`. Data refers to behaviors by name (`"ai": "ai.minotaur"`). New behaviors are added in code in Phase 3a, or in the script layer in 3b.

### 9.8 Phase 1 parity rule
Phase 1 catalogs must reproduce the original **exactly**. A round-trip test decodes the disc table, serializes it to the catalog, loads the catalog and compares field-for-field with the original values. Enhancements go into patch layers, never into the base catalog.

### 9.9 Determinism
Fixed-step simulation, the original RNG sequence, no wall-clock in game logic, and no unordered-container iteration in gameplay. This enables replays (`--replay file`) and state hashes for verification.
