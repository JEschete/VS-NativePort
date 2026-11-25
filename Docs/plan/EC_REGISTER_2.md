# Native Source Port Plan: Emulation-compatibility register (EC-04)

[Plan index](../NATIVE_SOURCE_PORT_PLAN.md) · §15 policy and EC-01–EC-03: [EC_REGISTER.md](EC_REGISTER.md) ·
EC-05–EC-09 and §15.4: [EC_REGISTER_3.md](EC_REGISTER_3.md)

#### EC-04: Original resource addresses and module relocation

**Evidence:** `Native/Content/src/DataStore.cpp` (`FindBySector`, `Content_ReadDiscBytes`), `ModuleLoader.cpp`
(`LoadData`, `SymbolMap::ResolveIn`, `RelocatePointers`), `Native/Game/src/Boot/ModuleData.cpp`, `ModulePointers.cpp`,
`Native/Tools/Extract/src/main.cpp`, and `Native/Desktop/src/main.cpp`.

Regular disc files are extracted into a file tree and indexed by original LBA/size. The game still requests sectors;
module data is copied into globals and original addresses are resolved into native symbols. Desktop startup still opens
a disc image when one is located, but runtime sector reads use extracted files through `DataStore`. LibDs keeps two
compatibility stubs and three consumed layouts; that is not retirement of the sector-addressed loading state machine.
STR converts to Matroska (AV1/FLAC); effect streams convert to WAV/SFZ and ending XA to FLAC under `audio/streams`.
The original XA `files.tsv` row remains `stream` for sector-addressed lookup, separately from the native asset index.

**Issue:** extraction is not yet conversion to the typed/versioned assets in plan D18. Interior-pointer resolution adds
an original byte offset to a native object; that is valid only where layout identity is established. Pointer-table extents,
aliases and function signatures require evidence. Unresolved pointers currently become null and boot logs a warning.

**Retain:** source provenance, stable original IDs for import/comparison, and source addresses inside extraction metadata
where needed to decode the original format. These are not runtime guest addresses after conversion.
**Replace:** sector-facing runtime APIs with asset IDs/handles; decode fields explicitly; map behavior tables to typed
registrations rather than serialized function addresses. Separate disk layout from native object layout.

**Retirement gate:** required tables resolve with validated bounds before their consumers run; malformed inputs produce
precise errors. Ultimately the shipping game runs from versioned converted assets with the original disc unavailable and
has no module-image copying, sector arithmetic or original-address relocation dependency.

**#47 status (first migrated slice):** `VS.Extract catalogs` imports `catalogs/assets.json` (a stable `asset.*` ID per
raw extracted file, with LBA/path kept only as provenance) and `catalogs/audio.json` (music, SFX-bank, wave-bank and
menu-sound tables read from the user's `SLUS_010.40`). `DataStore` validates both with precise `file: pointer: rule`
errors, and the hosts refuse to start without them. The audio slice requests catalog handles through
`DiscFileExtent::asset`; an extent naming both a sector and an asset, or a handle from an earlier catalog generation,
is rejected. `LegacyFiles` is deleted, and the sound command and AKAO opcode tables are constinit typed
registrations. Actor/model, room/zone, menu, effect and movie consumers still request sectors; `MOV/TITLE.STR` and
`ENDING.XA` have no asset IDs; other module-image copies and relocated tables remain. Open work: #48, #502.

##### EC-04 asset T-shim ledger

These are **active migration debt**, not completed native adoption. This register uses the existing EC-04 owner and
issue structure rather than a second status ledger. Preservation, production behavior and retirement remain separate
claims.

| T-shim ID | Existing runtime bridge / constraint | Retirement issue(s) | Required typed destination |
|---|---|---|---|
| EC-04-T-ZND | `ConvertedZones.cpp::LoadConvertedZone` calls `ZndFileOf` / `EncodeZndFile`; `ReadLiveConvertedZone` copies into a sector-sized block. Templates retain fixed name/limb layouts. | #199, #58 | Zone membership and environment definitions; relational species/enemy/loadout/loot definitions and native spawn instances. Room loading must not rebuild or parse a ZND. |
| EC-04-T-ZUD | `ConvertedModels.cpp::LoadConvertedArchetype` rebuilds SHP/WEP/SEQ components and calls `EncodeZudFile`; `ActorAssetsLive` consumes the rebuilt container. | #200, #55, #48 | Archetype references resolve directly to owned/shared model, equipment and animation definitions. No synthetic ZUD container or section-offset dispatch. |
| EC-04-T-SHP | `LoadConvertedModel` calls `EncodeShp` through `RebuiltBytes`, then the legacy-shaped actor loader parses it. Model validation also retains source-format constraints. | #203, #55, #48 | Typed mesh/skeleton/attachment definitions consumed directly. Any D21 renderer adapter is isolated; glTF authoring is not bounded by SHP exportability. |
| EC-04-T-WEP | `LoadConvertedWeapon` calls `EncodeWep` through `RebuiltBytes`; equipment appearance follows the old model loader. | #204, #58 | Typed equipment appearances/material variants, independent of WEP records and narrow item indices. |
| EC-04-T-SEQ | `LoadConvertedSequence` calls `EncodeSeqFile`; actor animation loading consumes the packed bank. | #205, #55 | Native clip/track definitions with explicit timing, interpolation and event semantics. No SEQ byte stream between the native loader and playback. |
| EC-04-T-ESQ | `LoadConvertedAnimationSet` calls `EncodeEsqFile`; set selection still feeds the original-layout loader. | #206, #55, #48 | Typed shared animation sets and explicit skeleton/selector bindings, with owned playback state. |
| EC-04-T-ETM | `LoadConvertedActorTexture` calls `EtmSheetOfImages` / `EncodeEtmFile` before actor texture loading. | #207, #55, #48 | Native texture-region/frame/palette definitions and instance bindings. PNG pixels are not repacked into ETM for playback. |
| EC-04-T-SYD | Not a whole SYD-file rebuild: `BuildRecipeBook` calls `DenseWorkshopTables` and retains byte-valued results/material matrices and legacy-indexed equipment records. SQLite compilation does not retire this layout bridge. | #499, #58 | Semantic-ID recipe rules/lookups and item definitions/instances. Results are not limited to byte IDs; directed behavior remains exact without the SYD matrix layout. |
| EC-04-T-ACTION | `Battle::BuildActionTable` (called by `LoadCatalogBackedGlobals` from `LoadModuleData`) rebuilds the 256 fifty-two-byte `ActionDefinition` bitfield records that about 110 references in 62 files still read; names, ids and categories stay within the original field widths. | #499, #58 | Battle code reads `Content::ActionRule` directly: named kinds, categories by identity, hits as rules, no 256-entry or four-bit-category limit. Mediator and top-level effect ids become named effect definitions. |

Partial typed adoptions that do **not** retire any row above:

- **Bestiary (#499/#500):** the Gazette builds an owned encyclopedia session from immutable SQLite definitions and the
  shared native text catalog instead of parsing `SMALL/MON.BIN`; original reconstruction stays offline. The 72
  out-of-list records are provenance, not invented enemies; native-only entries have explicit identity, preview and
  unlock policies. The preview-table index, BATTLE defeat/cursor storage and model/actor loading remain
  #199/#200/#203/#55 dependencies, and native glyphs still use the isolated page/packet backend (ASSET_PIPELINE §4.3/§5.2).
- **Native text (#54/#500):** the file-picker status/notice and Yes/No consent slice consumes semantic IDs, owned
  Unicode/control tokens, font metrics and layout, with no runtime encoded footer or choice offsets. Only MCMAN entries
  36/37 and the TITLE G02022 base English font image/metrics are imported; room summaries and other encoded menus are
  unchanged, and scaled glyph quads still use the isolated page/palette/packet backend. Not EC-01/EC-04, #54, #500,
  #498 or whole-module/font-source retirement (ASSET_PIPELINE §5.1).
- **G01905 materials (#499):** raw module-data storage and address mapping are removed; the workshop reads
  `MaterialCatalog` definitions from the layered SQLite cache with no runtime material encoder. Native modifiers are
  signed 32-bit and sums signed 64-bit; the eight-kind compatibility map and final inventory-stat byte storage remain
  explicit #58 constraints (unsupported kinds/results refuse, not truncate). Reserved bytes and exact reconstruction
  exist only in the offline importer. Does not retire EC-04-T-SYD (ASSET_PIPELINE section 4.2).
- **G01916/G01917 learning thresholds (#499):** semantic Break Art and battle-ability thresholds feed the battle/menu
  readers through the layered SQLite `LearningCatalog`; raw arrays, module copies and address bindings are removed.
  Native int32 thresholds may exceed uint16 progress storage; missing definitions refuse, and unrepresentable weapon
  credit refuses without marking or wrapping. #183 timing banking is unchanged; action/category adapters, mutable
  progress/saves and general definitions remain #499/#58 scope (ASSET_PIPELINE §4.4).
- **G01904 actions (#499):** the 256 action records are authored as a named `Content::ActionCatalog` (name, kind,
  category identity, behavior flags, one row per hit) and the raw module-data entry is gone, but
  `LoadCatalogBackedGlobals` still fills `Battle::actionDefinitions` through `BuildActionTable` (EC-04-T-ACTION), so
  G01904's readers, their byte-width limits and #58 remain. Hit effect mediators and the action's own effect id are
  still unnamed (ASSET_PIPELINE §4.7).

For every row, retirement requires:
1. Trace and migrate producers, consumers, callbacks, persistence and cleanup to
   the typed destination; remove the runtime encoder/layout adapter and its
   source-format validation requirement.
2. Keep exact reconstruction of **imported originals** in offline converter
   tests. Those tests remain mandatory and are not weakened by native adoption.
3. Exercise a native authoring case beyond the old format's representational
   limit through the actual consumer. Keep separately documented gameplay
   policies, not accidental byte/count limits.
4. Verify behavior/lifecycle with the format issue's real gameplay and
   reference requirements; publish exact-revision CI and mark the row retired
   only when the migrated path has no remaining bridge consumers.

Caching these adapters, their validation or their derived tables is a launch
optimization only. It cannot satisfy a retirement gate or make legacy source
files a permanent requirement of the shipping native-data tree.
