# Asset Pipeline: rooms and zones

[Asset pipeline index](../ASSET_PIPELINE.md)

### 6.3 Rooms (MPD → glTF + room JSON)
- **Geometry:** one glTF per room. Each texture page/CLUT group is one primitive with an unlit, nearest-sampled PNG. Vertex colors are kept, and the same `VS_UNIT_SCALE` applies.
- **Everything that is not drawing** goes in `<MAP>.room.json` (schema `room.schema.json`):
  - collision / floor-height tile grid;
  - doors and exits (target room IDs, not LBAs);
  - camera areas;
  - trap, lock and treasure markers;
  - room script triggers;
  - lighting/fog;
  - enemy spawn references → `catalogs\enemies` (ENEMY_DATA_SCHEME).
- Room IDs are `room.<zone>.<n>` with the original `legacyId` (zone/room index).

### 6.9 Zones (ZND → CSV tables, #199, #517)

A `MAP\ZONE*.ZND` is what the room loader reads when the zone changes (`Content/ZndFile.hpp`; the game's `_loadZnd`). Its 0x1C-byte header holds the room table, enemy section and texture section (each as offset and size) and the zone's music id. A reserved word follows, zero in every file. The sections are packed back to back to the end of the file:
- **Room table:** (sector, read size) per slot; an empty slot is (0, 0).
- **Enemy section:** a count, that many ZUD (sector, read size) entries, then that many 0x464-byte templates (`vs_CharInitData`, `Actors/CharacterInit.hpp`).
- **Texture section:** either 32 zero bytes, or four table sizes of which only the first is used, followed by that table: a count and that many TIMs, each after its byte size.

`VS.Extract tables <datadir>` (and `extract`) converts every zone with converter `znd-tables` version 3
(`Extract/ZndConversion.hpp`, `Content/ZoneTables.hpp`). `tables` runs after `models`, because the zones name the
enemy archetypes (§6.2.5). It writes six CSV tables, each with a committed schema that is written beside it and must
match (a test checks this). Columns are described in the schemas; the rules they do not express are:

| Table ([schema](../../Native/Content/schemas/catalogs/)) | Rows and identities |
|---|---|
| `world\zones.csv` ([schema](../../Native/Content/schemas/catalogs/world/zones.schema.json)) | `id` `zone.map.zone009`; `room_slots` is the room table's length, used or not |
| `world\zone-rooms.csv` ([schema](../../Native/Content/schemas/catalogs/world/zone-rooms.schema.json)) | one row per used slot, naming the room by id (`room.map.map009`) |
| `world\zone-textures.csv` ([schema](../../Native/Content/schemas/catalogs/world/zone-textures.schema.json)) | the texture table's images by id, in upload order |
| `enemies\zone-enemies.csv` ([schema](../../Native/Content/schemas/catalogs/enemies/zone-enemies.schema.json)) | one row per template: `id` `enemy.map.zone009.00`, `zone`, `slot`, the `archetype` it loads (`archetype.map.z009u00`); the two 24-byte names stay hexadecimal in the game's encoding (type `bytes`) until the string table (#500) gives them text ids |
| `enemies\zone-enemy-limbs.csv` ([schema](../../Native/Content/schemas/catalogs/enemies/zone-enemy-limbs.schema.json)) | one row per limb, except its armour part |
| `enemies\zone-enemy-parts.csv` ([schema](../../Native/Content/schemas/catalogs/enemies/zone-enemy-parts.schema.json)) | one row per equipment part: `weapon.blade`, `weapon.grip`, `weapon.gem1-3`, `shield.base`, `shield.gem1-3`, `accessory`, `limb0-5.armor` |

All six tables receive early informational `display_name` columns through §1.2. `zone-enemies.csv` shows the
decoded enemy name while its `name` and `weapon_name` hex fields continue to preserve every source byte.

**Named fields and domains (versions 2-3).** Enemy class, equipment materials (the shared material domain),
physical types, class ratings and affinities use the named domains in the schemas. Rules beyond the schemas:
- Version 3 names the parts' `damage_type`, `cost_type` (the resource the base attack spends) and `material`, and
  splits `shape_bits` into `attack_shape` (the shared volume-shape domain) and `attack_angle` (bits 3-7), as upstream
  `vs_Range`. Part `category` stays an integer: its domain depends on the part slot, which needs per-kind part tables.
- The remaining array tail bytes are explicit `storage_6`/`storage_7` fields, not invented classes/elements. They
  still need separation into import provenance when native template consumers are adopted.
- DP splash columns identify their zero-based destination limb and percent units. A limb index is topology, not a
  universal head/arm/leg name; actual body-part names depend on the model.
- The old enemy `material` column becomes `movement_class`. It feeds both gait/movement and body-load selection,
  **not** the equipment material enum. Its packed semantics remain an explicit blocker rather than guessed names.
- Mappings follow `StringTables.hpp`'s status indices, `Targeting.cpp`, `EquipmentStats.cpp`,
  `EquipmentAffinity.cpp` and `ActorSpawning.cpp`, with upstream structures/consumers in `BATTLE.PRG/charInit.h`
  and `146C.c`.

Schemas expose each renamed column's `legacy_name`. Readers accept either name, but refuse both names for one
column; enum numeric aliases remain supported. `VS.Extract authoring <datadir>` upgrades existing headers and enum
spellings without replacing IDs, labels, record values or row order. It validates all six tables and compares every
decoded native record before writing, then publishes the new converter version last. It splits a version-2
`shape_bits` cell into its two columns, refusing a value they cannot hold or a table with both forms. A normal
`tables` run upgrades version-1 and version-2 tables before considering source reconversion, preserving existing
native edits. Unknown enum codes remain preservable by the raw ZND codec, but are refused as native authoring values.
This does not resolve the remaining part categories, flags, strings, semantic identities or relational enemy
definitions.

**Exactness.**
- Every bit of a template is in exactly one column, and a test checks the coverage. Unnamed bytes are `raw_<offset>` columns (§4.1).
- A limb or part whose bytes are all zero, and an empty room slot, has no row; a missing row reads as zeros.
- Rows name files by id, never by sector. The rebuild takes each named file's sector and sector-rounded size from `files.tsv` as it is now.

**Zone textures.** The texture table's TIMs become `textures\zone\<ZONE>\<nn>.png` with their sidecars (image id
`image.map.zone009.00`), using the shared TIM rules (§6.4). Provenance names the zone, its hash and the TIM's byte
offset. They are 16-bit: room palette rows and texture pages, kept as the words the zone uploads. A page's packed 4-
or 8-bit indices become an indexed image when the rooms (#198) bind each page's depth and palette.

**Manifest and reruns.** The manifest has one output per zone (`zone.map.zone009`), listing the twelve table files it shares with every zone and its own images. The tables hold every zone, so a run that converts any zone writes them all. A run whose zones are all unchanged writes nothing, so edits survive; `--force` rewrites everything.

**Refusals.** The converter writes nothing unless every zone's rows and images rebuild the source file byte for byte and the tables read back to the same rows. It refuses:
- a room or ZUD entry whose sector starts no extracted MPD or ZUD, or whose size is not that file's read;
- an empty texture table;
- an archetype that the `zud-json` converter has not recorded ("convert the models first").

**Runtime.** The room loader's zone read (`RoomLoadingLive`, `BeginConvertedZoneRead`) fills the block the disc read
would have filled from `Content::LoadConvertedZone` and never reads the zone file. It completes in the requesting
update. This is **EC-04-T-ZND**, not completion of #199: the native input is still repacked in memory for an
original-layout consumer. Replace that path with typed zone/enemy definitions; do not make its byte-sized block the
native authoring or allocation limit.
- The tables are read once and kept while their files are unchanged.
- Each texture image must name its zone and that zone's manifest hash, and is rebuilt into its TIM (`Content::TimFromImage` and `EncodeTim`, shared with the importer).
- A table error names the file, line and column. An unknown room, archetype or image, a stale image, or a zone rebuilt larger than the read's block is refused with the cause logged. The loader then refuses the zone as a failed read.
- `RequireConversions` refuses to start without a current `znd-tables` output for every zone.

Open work: #199 (relational species, variant, loadout and loot tables; part references to the item catalog; SQLite
and patch/mod layers; production spawn, combat, drop and bestiary evidence for edited enemies; a zone rebuilt larger
than its original read), #198 (texture page depth and palette binding).
