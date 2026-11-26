# Asset Pipeline: weapons and enemy archetypes

[Asset pipeline index](../ASSET_PIPELINE.md)

Weapons and ZUD bundles share the character model writer, reader and runtime rules of §6.2
([CHARACTER_MODELS.md](CHARACTER_MODELS.md)).

#### 6.2.4 Weapon models (WEP → glTF, #204)

A WEP is the SHP magic and counts, a model section laid out as an SHP's (flat polygons, no sound), a texture with one palette per blade material, and one rest rotation per bone (`Content/WepFile.hpp`). `VS.Extract models <datadir>` (and `extract`) converts every `OBJ\*.WEP` with converter `wep-gltf` version 1 (`Extract/WepModelConversion.hpp`, `Content/WepModelFiles.hpp`):

- Each weapon writes `models\weapons\<NN>.gltf` + `<NN>.bin`, its texture `<NN>.png` + `.image.json` + `.palettes.png` (§6.4), and six previews `<NN>.<material>.preview.png`. Its ids are `model.obj.<nn>.wep` and `image.obj.<nn>.wep`, so they do not collide with the character model of the same number.
- The texture is 8-bit and 32 rows tall. `.palettes.png` has seven rows in the order iron, bronze, hagane, silver, damascus, wood, leather; BATTLE.PRG's `weaponMaterialPaletteMap` gives each item material its row. Each row is the handle's colours then the material's. The PNG's own palette is row 0 (iron), and edits to any row are validated as §6.4 requires.
- The glTF has the character model layout, with each bone after the first turned by its rest rotation, so it shows the weapon as the game draws it. `asset.extras.vs` holds the payload (schema `vs.wep-model` version 1), including the stored rotations.
- **Material variants:** the glTF declares `KHR_materials_variants` with one variant per material row. Each primitive maps `iron` to its own material, and each other variant to a copy that shows that material's preview (the texture with that row's palette). Blender and other variant-aware viewers can switch blade materials.
- The previews are derived and only for viewers: the game and the reader never read them. After editing `.palettes.png`, convert again with `--force` to refresh them.
- At runtime, `Content::LoadConvertedWeapon` serves the weapon read (`ActorAssetsLive` `BeginFileRead`, `kWeapon`) as §6.2 describes for characters. The game picks the material row when it loads the weapon (`LoadWeaponMaterialPalette`), as the original did. `RequireConversions` refuses to start without the `wep-gltf` batch.

Open work: #204 (a weapon larger than its original allocation; a Blender re-export through the validator). ZUD bundles name their weapons and shields (both WEP files) by these outputs (§6.2.5).

#### 6.2.5 Enemy archetypes (ZUD → JSON, #200, #516)

A ZUD is the enemy bundle a room spawn reads (`Content/ZudFile.hpp`). It has a 0x30-byte header and then five sections packed in order with no gaps: body (an SHP), weapon (a WEP), shield (a WEP), common animation and battle animation (SEQ banks). The header holds 8 identity bytes (character, weapon, battle-sequence model, weapon material, shield, shield material, default material, and a zero pad) and then one offset and size per section. `VS.Extract models <datadir>` (and `extract`) converts every `MAP\*.ZUD` with converter `zud-json` version 2 (`Extract/ZudConversion.hpp`):

- Each bundle writes `models\zud\<ZUD>.json` (id `archetype.map.<zud>`) in schema `vs.actor-archetype` version 2 (`Content/ActorArchetype.hpp`, [actor-archetype.schema.json](../../Native/Content/schemas/actor-archetype.schema.json)). It holds the source, its hash, the identity bytes as named fields, and one entry per section (`null` when the section is empty).
- **A section is named, not copied, when it already has an output.** The converter matches each section's SHA-256 against the `OBJ` files (the identity byte that numbers the part breaks a tie). A match names that file's output: `{"model": "model.obj.<nn>"}` for a body, `{"model": "model.obj.<nn>.wep"}` for a weapon or shield, `{"animation": "animation.obj.<bank>"}` for a bank. An edit to that model, weapon or bank reaches every archetype that names it.
- **A section no `OBJ` file holds stays with the archetype.** A body becomes `models\zud\<ZUD>.gltf` + `.bin` in the §6.2 layout (model id `model.map.<zud>`; a texture of its own writes `<ZUD>.png`, `.image.json` and `.palettes.png` with image id `image.map.<zud>`, and a borrowed texture's image is `../characters/<NN>.png`), named as `{"file": "<ZUD>.gltf"}`. A bank is written inline as `{"bones": N, "bank": {...}}` in the `vs.actor-animation` bank form (§6.2.1). A weapon or shield must match a WEP. Four bundles have their own body (`Z049U06`, `Z060U01`, `Z063U00`, `Z063U01`).
- The output depends on each `OBJ` file it names, at the hash that file's output records, and on `BATTLE\BATTLE.PRG` when it writes its own body or bank (the event lengths). A change to any of them reconverts the archetype. A rerun leaves unchanged archetypes and their edits alone; `--force` rewrites them.
- The extractor refuses a bundle with no body, a non-zero pad byte, a section that does not start where the one before ends or runs past the file, bytes after the last section, a weapon or shield that matches no listed WEP, a named part that has no current output ("convert the models first"), and an own part when `files.tsv` does not list `BATTLE\BATTLE.PRG`.
- At runtime, `Content::LoadConvertedArchetype` serves the room spawn's ZUD read (`ActorSpawningLive` `BeginZudRead` through `ReadLiveConvertedArchetype`) and never reads the bundle from the disc. It finds the bundle by LBA, loads each named part as its own read would (`LoadConvertedModel`, `LoadConvertedWeapon`, `LoadConvertedSequence`), loads an own body from its glTF beside the JSON (which must name the same source), and rebuilds the bundle's bytes. An id that names no output of the right converter, an edit that fails validation, or a file that does not match its manifest is refused with the file and member named. A rebuilt bundle larger than the read's allocation is refused and the spawn fails as a failed read did. `RequireConversions` refuses to start without the `zud-json` batch.

Open work: #200 (the encyclopedia's enemy previews, which read ZUD files through their own path; editing the identity bytes against the character, weapon and material tables; a bundle larger than its original allocation).
