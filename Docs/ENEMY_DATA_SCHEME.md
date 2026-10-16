# Enemy Data Scheme

Status: **Design v1.** Answers open question Q3 of [NATIVE_SOURCE_PORT_PLAN.md](NATIVE_SOURCE_PORT_PLAN.md).
It applies the patterns in plan §9: Def/Instance split, string IDs, versioned schemas, patch layers, and data for parameters with code for behaviors.

---

## 1. How the original stores enemies

There is **no global enemy table**. Enemy data is spread over five places and duplicated per zone.

| Where | What | Decomp type | Notes |
|---|---|---|---|
| `MAP\ZONEnnn.ZND`, enemy section | A list of ZUD files (LBA + size), followed by **N full enemy records of 0x464 bytes** | `_zndEnemy` / `vs_battle_charInitData` (`src\BATTLE\BATTLE.PRG\146C.h`) | Each record is a complete, self-contained character: `subType`, `reach`, name[24], HP/MP, STR/INT/AGI, walk/run speed, full **weapon** (blade + grip + 3 gems + material + drop rate + name), **shield** (+3 gems, material, drop rate), **accessory** (drop rate), **6 limbs** (HP, AGI defense bonus, chain evasion, 4 class types, 8 element affinities, 4 actions/skills, armor + drop rate, DP-damage split %), and a trailing `mpdIdentifier`. |
| `OBJ\Z###U##.ZUD` | Model archive per enemy: SHP (body) + WEP (weapon) + WEP (shield) + SEQ (common anim) + SEQ (battle anim) + effects | `zudHeader` (`146C.c`) | Loaded on demand by `func_800775C0` (the spawn function). Only one ZUD is cached at a time. |
| `MAP\*.MPD`, enemy section | Room spawns: which zone record to spawn, x/z/height/direction, flag/stat/scene conditions, up to 3 carried items with counts, variant, hidden | `_mpdEnemy` | This is the *placement*. Conditions gate spawns on story flags. |
| `BATTLE\MON.BIN` (MENU9) | Bestiary: 78 entries of name[28], `classId`, `zudId` (for the 3D preview), kill-flag range, prev/next links | `_monBinData_t` (`src\MENU\MENU9.PRG\238.c`) | Unlocked by `vs_main_scoredata.enemyKillFlags` (a bitset indexed by `subType` < 256). |
| Runtime | `vs_battle_actor` (0x50) + `vs_battle_actor2` (0x964): live stats, equipment, statuses/timers, loot | `146C.h` | Built by copying the zone record. `savedEnemyState` keeps HP/MP/limb HP/statuses/position when you leave and re-enter a room. Loot is rolled at death (`_dropLoot`) from the per-item drop rates. Kill score is tracked by `enemyClass` (3 bits) and `subType`. |

### 1.1 Measured on the disc (`SLUS-01040\MAP\*.ZND`)

| Metric | Value |
|---|---:|
| ZND files | 256 |
| ZNDs containing enemies | 49 |
| Enemy records in total | 455 |
| Unique records (excluding the trailing ID) | 405 |
| Distinct `subType`s | 168 |
| `subType`s with more than one variant | 91 (up to 15 variants each) |
| Bestiary (MON.BIN) entries | 78 |

**Conclusion:** the same creature appears in many zones as slightly different copies. Each copy differs in gear, stats or drops. There is no shared definition, the IDs are `u_char`/`u_short` (capped at 255 for kill flags), and the stats, loadout, loot, model and placement are all fused into one blob.

### 1.2 Problems this causes for modding and Phase 3

1. **Changing "all Minotaurs"** means editing up to 15 copies in different zone files.
2. **New enemies** need a free `subType`, a ZND slot, a ZUD slot and a bestiary slot, all with fixed-size tables.
3. **Loot** is baked into each equipment slot as a `dropRate` byte. There are no shared loot tables.
4. **The model, the AI and the stats** cannot be varied independently.
5. **Placement** is coupled to zone-local indices (`mpdIdentifier`).

---

## 2. Modern scheme

The scheme has four layers, each with its own catalog and schema. Every reference is a **string ID**, and a `Registry<T>` resolves each one to a `Handle<T>` at load time.

```
Species ──< EnemyDef ──> Loadout (equipment instances)
   │            │──────> LootTable
   │            └──────> Behavior (AI, by name, in code)
   └──> ModelSet (ZUD)        Room.spawns[] ──> EnemyDef  (placement + conditions)
Bestiary entry ──> Species
```

### 2.1 `species` catalog: what the creature *is*

Species data is shared by every variant.

```json
{
  "id": "species.minotaur",
  "legacy": { "subTypes": [42, 43], "bestiaryIndex": 17 },
  "name": "text.species.minotaur",
  "class": "beast",
  "bodyPlan": "bodyplan.quadruped_horned",
  "model": "modelset.z050u01",
  "defaultBehavior": "ai.minotaur",
  "bestiary": { "description": "text.bestiary.minotaur", "order": 17 }
}
```

- `class` is an enum string (human, beast, undead, phantom, dragon, evil). It replaces `enemyClass : 3`.
- `bodyPlan` names the limbs (`head`, `body`, `right_arm`, …), replacing the fixed `limbs[6]` with limb index meanings that differ per model. A plan may have **any number** of parts. Phase 1 plans have exactly the original 6.
- `model` points to a **`modelsets` catalog** entry generated from the ZUD: body SHP, animation SEQs, effect blocks. The weapon and shield models come from the *equipped items*, not from the ZUD.

### 2.2 `enemies` catalog: a concrete variant you can place

A variant is "a Minotaur as found in the Wine Cellar". It **extends** a base def and overrides only what differs, which is how the 405 unique records collapse.

```json
{
  "id": "enemy.minotaur",
  "species": "species.minotaur",
  "stats": { "hp": 250, "mp": 0, "str": 70, "int": 30, "agl": 45, "reach": 8 },
  "movement": { "walk": 12, "run": 20, "carrySpeed": 8 },
  "parts": {
    "head": { "hp": 60, "agilityDefense": 4, "chainEvasion": 0,
              "classTypes": { "human": 0, "beast": 0, "undead": 0, "phantom": 0 },
              "affinities": { "physical": 0, "air": 0, "fire": -10, "earth": 10, "water": 0, "light": 0, "dark": 0 },
              "damageShare": 20,
              "armor": { "item": "armor.horned_helm", "material": "iron" } }
  },
  "actions": [ { "skill": "skill.heavy_shot", "part": "body" } ],
  "loadout": {
    "weapon":    { "blade": "blade.bardiche", "grip": "grip.long_pole", "material": "iron", "gems": [] },
    "shield":    null,
    "accessory": null
  },
  "loot": "loot.minotaur",
  "behavior": "ai.minotaur"
}
```

```json
{
  "id": "enemy.minotaur.wine_cellar",
  "extends": "enemy.minotaur",
  "legacy": { "znd": 9, "index": 2, "subType": 42 },
  "stats": { "hp": 280 },
  "loadout": { "weapon": { "material": "hagane" } }
}
```

- `extends` uses a **field-level merge**, one level deep: maps merge, and arrays and scalars replace. Cycles are rejected at load.
- The loadout uses **the same `EquipmentInstance` types as the player** (`BladeInstance`, `GripInstance`, … from plan §9.1). Enemies and Ashley share the equipment system, so a dropped weapon is simply the enemy's instance moved into the player's inventory.
- `legacy` keeps the original indices for Phase 1 parity, `.mcd` import and verification. New content omits it.

### 2.3 `loot` catalog: what can drop

Loot is moved out of the per-slot `dropRate` bytes into shareable tables.

```json
{
  "id": "loot.minotaur",
  "rolls": [
    { "source": "equipped.weapon",    "chance": 0.0705 },
    { "source": "equipped.armor.*",   "chance": 0.0313, "each": true },
    { "source": "carried",            "chance": 1.0 },
    { "item": "consumable.cure_root", "chance": 0.25, "count": [1, 1] }
  ]
}
```

- `equipped.*` sources drop **the instance the enemy was actually using**, including its damage. This is the original behavior.
- Chances are stored as the original `n/255`, expressed as a decimal and kept exact by the extractor (`"chance": {"num": 18, "den": 255}` if bit-exactness is required).
- The RNG call order in `_dropLoot` is preserved in Phase 1 for replay parity.

### 2.4 Placement: `rooms[].spawns[]`

Spawns belong to the room catalog, not to the enemy.

```json
{
  "id": "spawn.wine_cellar.entrance.0",
  "enemy": "enemy.minotaur.wine_cellar",
  "position": { "x": 12, "z": 7, "height": 0 },
  "facing": 4,
  "variant": 0,
  "hidden": false,
  "when": { "all": [ { "flag": "story.minotaur_defeated", "is": false } ] },
  "carries": [ { "item": "key.iron_key", "count": 1 } ]
}
```

- `when` is a small condition expression. It replaces `flagTest/flagValue/statTest/statValue/sceneTest`. Phase 1 translates the original tests exactly.
- The **spawn ID is stable** and is the key for `savedEnemyState` (the room-revisit persistence) and for saves, instead of `spawn->index`.

### 2.5 Behaviors (code)

AI routines, special attacks and scripted deaths stay **C++, registered by name** in a `BehaviorRegistry` (`"ai.minotaur"` → `MinotaurAI`). They are copied from BATTLE.PRG in Phase 1. Data chooses a behavior and parameters; it never contains logic.

### 2.6 Runtime

| Concept | Native type | Replaces |
|---|---|---|
| Immutable def | `const EnemyDef&` via `Handle<EnemyDef>` | The 0x464 record copy |
| Live enemy | An `Actor` entity with the components `Transform`, `Model`, `Animator`, `Stats`, `BodyParts`, `Equipment`, `StatusEffects`, `Behavior`, `Collider`, `LootSource` | `vs_battle_actor` + `vs_battle_actor2` (0x964) |
| Room persistence | `RoomMemory[spawnId] → EnemySnapshot { hp, mp, partHp[], statuses, position, facing }` | `savedEnemyState` |
| Kill records | `Score.kills[speciesId]`, `Score.killStreak{species, count}` | `enemyKills[enemyClass]`, `enemyKillFlags[subType/32]` |
| Bestiary unlock | `Score.seenSpecies` set of `SpeciesId` | the kill-flag bit range in MON.BIN |
| Asset streaming | `Content::AsyncLoader` preloads every `ModelSet` referenced by the room's spawns when you enter the room | the single cached ZUD plus a synchronous wait loop |

### 2.7 Zones no longer own enemies

In the original, a ZND's enemy list exists to tell the game what to load. In the native port, a **zone's preload set is computed** from the spawns of its rooms. The zone catalog therefore has no enemy list, which also removes the "slots per zone" limit.

---

## 3. Extraction (VS.Extract) and Phase 1 parity

1. **Read** every ZND enemy section (455 records), every MPD enemy section, MON.BIN and the ZUD headers.
2. **Decode** each 0x464 record into a typed `LegacyEnemyRecord`. Do not keep it as raw bytes.
3. **Group** by `subType` → `species`. Use MON.BIN to name and class species; split a `subType` across species if the models differ.
4. **Dedupe:** byte-identical records become one `EnemyDef`. Within a species, pick the most common record as the base and emit the others as `extends` variants with minimal diffs.
5. **Lift** the per-slot drop rates into `loot` tables. Identical tables are shared.
6. **Emit** the catalogs (`species`, `modelsets`, `enemies`, `loot`, room `spawns`, `bestiary`) into `Native\data\catalogs\enemies\` (see [ASSET_PIPELINE.md](ASSET_PIPELINE.md)). This output is git-ignored.
7. **Round-trip test:** rebuild every original 0x464 record from catalog + `extends` + loot and compare it field-by-field with the disc. Rebuild every MPD spawn the same way. This is the Phase 1 gate for enemy data (plan §9.8).

Generated IDs are derived from names (`enemy.minotaur.zone009_2`). A hand-maintained `Native\Content\patches\enemy-names.json` can rename them to readable IDs without touching the extractor.

---

## 4. What this enables (Phase 2/3)

- **Balance mods** edit one `enemy.minotaur` entry and every variant inherits the change.
- **New enemies:**
  - add a `species` (new or existing model), an `enemy`, an optional `loot` table, and spawns;
  - no slot limits (IDs are strings and handles are 32-bit);
  - body plans are no longer fixed at 6 parts.
- **New loot tables** (shared, weighted, conditional on difficulty or New Game+).
- **Difficulty scaling** is a patch layer that multiplies `stats` and is applied at load.
- **Randomizers** are a mod that rewrites `spawns[].enemy` and `loot`.

---

## 5. Ledger hooks

- `DATA_LEDGER.csv`: `_zndEnemy`, `_mpdEnemy`, `_monBinData_t` and the related tables get `disposition = catalog`.
- `CONTENT_LEDGER.csv` (to be added): ZND (enemy section), MPD (enemy section), ZUD and MON.BIN map to their decoders and catalogs, with the round-trip test as the `verified` gate.
- `TYPE_LEDGER.csv` (to be added): `vs_battle_charInitData`, `_limbInit`, `vs_battle_weaponIntermediate`, `vs_battle_shieldIntermediate`, `_mpdEnemy`, `savedEnemyState`, `vs_battle_actor`, `vs_battle_actor2` map to the native types above.
