# Asset Pipeline: character models

[Asset pipeline index](../ASSET_PIPELINE.md)

### 6.2 Character models (SHP + SEQ/ESQ + ETM → glTF), matching the Sydney reference

Runtime read errors (#518): missing, stale or invalid converted SHP, WEP, SEQ/ESQ and ETM data fails the
actor-asset request. The loader frees its unpublished file allocation and does not parse cleared bytes or fall back
to the extracted legacy file. Only a full legacy disc-request queue permits the existing synchronous fallback, and
that fallback's failure is checked too.

`resources\Sydney Losstarot (Dying)\6F.SHP.gltf` is the standard-tool preview baseline. Preserve that structure where compatible with the source; it does not override the lossless metadata or renderer requirements below:

| Element | Rule (from the reference) |
|---|---|
| asset | glTF 2.0, one scene, written as `.gltf` + `.bin` with textures as shared image files (§6.4) referenced by relative URI (D25). Blender and standard viewers open it directly, and textures can be edited without touching the model. A self-contained base64 `.gltf` or `.glb`, like the reference, is an export option only |
| mesh | one skinned mesh; attributes `POSITION`, `NORMAL`, `TEXCOORD_0`, `COLOR_0`, `JOINTS_0`, `WEIGHTS_0` + indices |
| skeleton | one node per SHP bone named `bone<N>` (N = SHP bone index), with rotation + translation; one `skin` with `inverseBindMatrices` |
| animations | **every** applicable SEQ/ESQ skeletal clip, one clip per animation, named `<SHP>_<bank>_<n>` (e.g. `6F_BT2_0`); texture/event tracks follow §6.2.2 |
| material | baseline: `KHR_materials_unlit`, `alphaMode: BLEND`, `metallicFactor: 0`, `roughnessFactor: 0.9`; split primitives/materials where source blend or lighting state differs |
| texture | the shared image files of §6.4 (PNG + `.image.json`), referenced by the material; sampler `magFilter/minFilter = 9728` (NEAREST), wrap `10497` (REPEAT) |

Additions the reference does not have, required by P4 (lossless). The initial contract uses `extras`; standard tools can display the baseline without interpreting VS behavior:
- `asset.extras.vs.schemaVersion`: version of the VS payload across the file's `extras.vs` objects (§6.2.1).
- `extras.vs.shp`: SHP header values the game uses (collision/size, shadow, overlay/attachment points for weapons and effects, magic points, bone groups, body-part mapping used by targeting).
- `extras.vs.seq` on each animation: the SEQ key data the game reads at runtime (frame count, per-key flags, sound/effect triggers, root-motion flags).
- `asset.extras.vs.textureAnimation`: actor-local atlas bindings, overlay regions, frame definitions and palette data; animation-local texture/event tracks live with `extras.vs.seq` (§6.2.2).
- Vertex colors and per-polygon flags (semi-transparency, double-sided, lighting mode) go in `COLOR_0` plus a per-primitive `extras.vs.polyFlags`. Group polygons by compatible flags rather than collapsing different states into one primitive.
- **Which SEQ/ESQ clips and ETM regions belong to which SHP** comes from the decomp's loaders, event commands and ZUD bundles, not file-name guessing. A matching prefix/stem is a candidate association, not proof.
- **Fixed-point scale:** positions are SHP units divided by a constant `VS_UNIT_SCALE`, recorded in `asset.extras.vs.unitScale` so the loader can return exact integer values. The Sydney reference's scale is measured and matched.

**Runtime target:** typed `ModelDef` / `AnimationDef` consumers read glTF and native metadata directly. The current
SHP-shaped reconstruction is EC-04-T-SHP/SEQ migration debt, not the target `ModelLoader` contract. D21's temporary
renderer does not require limiting glTF authoring to SHP layouts. Section 7's offline comparison proves imported
fields survive; it does not require the production loader to emit SHP or SEQ bytes.

**Also emitted** (optional, `--posed`): `<SHP>_posed.obj/.mtl/_0.png`, like the reference folder. This is for quick previews only; the game never reads it.

**SHP conversion (#203, #205).** `VS.Extract models <datadir>` (and `extract`) converts every `OBJ\*.SHP` with
converter `shp-gltf` version 3 (`Extract/ShpModelConversion.hpp`, `Content/ShpModelFiles.hpp`,
`Content/ShpFile.hpp`), and carries every `OBJ\*.SEQ` bank in its owner model's glTF (§6.2.1):

- Each model writes `models\characters\<NN>.gltf` + `<NN>.bin`; id `model.obj.<nn>`. A model with its own texture
  also writes `<NN>.png`, `.image.json` and `.palettes.png` (§6.4), image id `image.obj.<nn>`. A model that borrows
  another model's texture writes only the glTF and buffer; its glTF shows the lender's PNG and its manifest entry
  lists the lender's SHP as a dependency.
- The glTF has one skinned mesh, a `root` node turned 180° about X and one `bone<N>` node per SHP bone. Positions
  are the SHP's integer units (scale 1) with each bone's rest origin added. Primitives are grouped by polygon flags
  0x71 (double-sided, semi-transparent, blend mode). Each corner carries the custom `_VS_VERTEX` attribute (its SHP
  vertex number); each primitive's `extras.vs.polygons` lists its polygons in triangle order.
- `asset.extras.vs` holds the payload (schema `vs.shp-model` version 3): source and hash, bones, polygon counts,
  limb ranges and the 0xE8-byte layout block as named fields (e.g. editable `collision_radius`). A buffer view holds
  the sound bytes. `sequence` is the SHP's embedded bank and `sequences` lists the `OBJ\*.SEQ` banks the model owns,
  both as `vs.actor-animation` JSON (§6.2.1).
- **The payload and the geometry are authoritative.** Materials, textures and images only preview the polygon flags.
  The reader rebuilds the SHP from those two sources and checks it against every value the payload records. Output
  must pass the Khronos glTF validator with no errors or warnings.
- A rerun leaves an unchanged SHP's files alone, so edits survive; `--force` rewrites every model.
- At runtime, `Content::LoadConvertedModel` (`Content/ConvertedModels.hpp`) serves the actor model read
  (`ActorAssetsLive` `BeginFileRead`, `kActorModel`) and never reads the SHP from the disc. It matches the read's LBA
  and sector-rounded size to the extracted SHP and rebuilds the bytes from that model's files. An edit is validated
  and loaded, or refused with the file and member named. `RequireConversions` refuses to start without the
  `shp-gltf` batch. The load completes in the same update that requests it (the original waited for the CD). An
  edited model must still fit the original sector-rounded allocation; a larger one is refused.

Open work: #205, #206 (glTF preview clips for SEQ banks and ESQ sets), #207 (ETM references and texture tracks); a
model larger than its original allocation and a Blender re-export through the validator are unsupported. Weapons are
§6.2.4 and ZUD enemy archetypes §6.2.5.

#### 6.2.1 Container, schema and optional extension

- **First delivery:** keep the existing `extras.vs` design and define its versioned contract in [actor-animation.schema.json](../../Native/Content/schemas/actor-animation.schema.json). VS.Content and Workshop must agree on IDs, units, track semantics and fixtures; do not build incompatible per-tool interpretations. CI checks the writer fixtures in `Native\Tests\data\animation\` against the schemas (`tools\test\test_content_schemas.py`).
  - **SEQ banks (#205):** schema `vs.actor-animation` version 2 (`Content/ActorAnimation.hpp`) writes one bank as its source, source hash, bone count and bank.
    - The bank lists its clips, the animation-to-clip map and the motion pool. Each pool element is a track (an optional initial key and its commands as one flat list of frames, x, y, z groups), an event script (one object per event: its frame, its op by name and the op's named fields, §6.2.2) or unread bytes (lowercase hex).
    - Clips name pool elements by number, so a track can be lengthened without recomputing offsets.
    - The JSON Schema checks types and ranges. The native reader also checks references, clip flags and event lengths (`EncodeSeqBank`). Banks read back from JSON must rebuild their exact bytes.
    - Version 1 wrote each event as `[frame, op, arguments...]`. `VS.Extract authoring <datadir>`, or the next `models` run without `--force`, rewrites such outputs (`shp-gltf` 2, `seq-gltf` 1, `esq-json` 1, `zud-json` 1) in place with named events and keeps every other edit (`Extract/AnimationAuthoring.hpp`).
  - **SEQ banks in their models (#205):** each bank is stored once, in `asset.extras.vs.sequences` of the one model that loads it.
    - Ownership comes from what the game reads, not from file names. A model owns the banks its layout block names (`battle_sequence_lbas`/`_sizes` and `special_sequence_lbas`/`_sizes`, LBA and sector count). `OBJ\00.SHP` and `OBJ\02.SHP` also own the material and material-model banks that `BATTLE.PRG`'s tables name (`ShpModelConversion.cpp` `BankOwners`).
    - The extractor refuses a bank that no model or two models load, and a layout entry that names no listed SEQ.
    - Every bank gets a `seq-gltf` output (id `animation.obj.<bank>`) that lists its owner's `.gltf` and `.bin`. Its dependencies are `BATTLE\BATTLE.PRG` (the event lengths) and the owner SHP at the hash the model output records. A bank with a changed source or owner reconverts its owner.
    - At runtime, `Content::LoadConvertedSequence` serves the animation bank read (`ActorAssetsLive` `BeginFileRead`, `kSequence`) of an `OBJ\*.SEQ` and never reads the bank from the disc. It finds the bank by LBA and sector-rounded size, reads its owner's glTF and rebuilds the bank's bytes. The last four models decoded are cached, keyed by their files' size and time, so one model's banks decode once.
    - An edited bank is validated and loaded, or refused with the file and member named. It must fit the original sector-rounded read; a larger one is refused and leaves an empty bank. `RequireConversions` refuses to start without the `seq-gltf` batch or with a bank that has no current output.
  - **ESQ shared animation sets (#206):** converter `esq-json` writes each `OBJ\E*.ESQ` as `models\animation-sets\<ESQ>.json` (id `animation-set.obj.<stem>`) in schema `vs.animation-set` version 2 (`Content/AnimationSet.hpp`, [animation-set.schema.json](../../Native/Content/schemas/animation-set.schema.json)).
    - A set holds up to 15 entries in the order the game searches them. Each entry has a selector (a model number or a model kind), a texture row or `null`, the bone count and its bank in the `vs.actor-animation` form. A set is its own file because several models play it.
    - Which models play an entry comes from `BATTLE.PRG`'s tables, not file names (`EsqConversion.cpp`). A model plays the first entry whose selector is its number or its kind (the kind table), and the set tables name each set's sectors.
    - The extractor refuses an entry no listed model plays, a model whose bone count differs from its entry's, a texture row for a selector that is not a listed model number, a repeated selector, and a set the tables do not name or a table entry naming no listed set. The output depends on `BATTLE.PRG` and on every model the set binds; a change to any of them reconverts the set.
    - Each entry's bank includes the padding to a word that follows it on disc, as the game has it in memory.
    - Two quirks of the original sets, absent from the standalone SEQ files, are kept rather than repaired:
      - A track's last command can be cut off by the end of its bank (or by the next element). Every clip that plays such a track ends before the cut, so the game never reads it. The cut bytes stay as unread bytes, and the decoder and encoder refuse a clip that would play into them.
      - Two entries map an animation to the clip one past the last (`E02F` entry 4, `E054` entry 2). Like the game, the reader accepts a clip number whose header still fits in the bank, and refuses one whose header would end past it.
    - At runtime, `Content::LoadConvertedAnimationSet` serves an ESQ read of the animation bank stream (`ActorAssetsLive` `BeginFileRead`, `kSequence`) and never reads the set from the disc. An edited set is validated and loaded, or refused with the file and member named. It must fit the original sector-rounded read; a larger one is refused and leaves an empty set. `RequireConversions` refuses to start without the `esq-json` batch.
  - **ETM actor texture sheets (#207, #515):** converter `etm-png` writes each `OBJ\E*.ETM` as one indexed PNG plus `.image.json` (§6.4) per model it textures, `textures\actors\<ETM>\<MODEL>.png` (output id `actor-texture.obj.<stem>`, image id `image.obj.<stem>.<model>`; `Content/EtmFile.hpp`, `Extract/EtmConversion.hpp`).
    - A sheet is a 4-byte header (marker and height) and lines of 64 halfwords that the game uploads below the actor textures. Which lines belong to which model comes from the ESQ set `BATTLE.PRG`'s tables pair with the sheet: each entry with a texture row starts a model's region, which runs to the next entry's row or the end of the sheet. The game binds a model to the first entry whose selector is its model number, so a texture row's selector is that number.
    - A region's depth is its model's SHP texture depth, and its preview palette is the model's own palette; a model that borrows another's texture uses the lender's. The sidecar names the palette's source, and the indices are authoritative.
    - Indices past the model's palette are drawn at run time from the rest of the model's CLUT row (a weapon's palette shares the row from column 160); the preview shows them transparent and the sidecar's note says so.
    - The extractor refuses a sheet entry that names no listed ETM or the wrong sector count, a sheet named by no entry or by two, an entry naming a sheet but no set, a set whose texture rows give no model line 0, repeat a model or a line or pass the sheet's end, and a region whose model is not a listed SHP. The output depends on `BATTLE.PRG`, the set and each region's model and lender; a change to any of them reconverts the sheet. A rerun leaves unchanged sheets and their edits alone; `--force` rewrites them.
    - At runtime, `Content::LoadConvertedActorTexture` serves the effect texture read (`ActorAssetsLive` `BeginFileRead`, `kEffectTexture`) and never reads the sheet from the disc. It rebuilds the sheet from the region images, which must be the sheet's indexed images, one line wide and in contiguous lines, each at the depth its model (named by the sidecar's `texture` dependency and loaded from its own glTF) draws its texture at. An edit that breaks this is refused with the image named; a rebuilt sheet larger than the original read is refused and uploads nothing. `RequireConversions` refuses to start without the `etm-png` batch.
- **Extension adoption gate:** once the payload is exercised by both consumers, assess packaging it as one documented optional vendor extension. Choose a project/vendor namespace, publish its schema and fallback behavior, and test both readers before switching the writer. Do not invent a `KHR_` or `EXT_` standard. The extension must be in `extensionsUsed`, but not `extensionsRequired`, only while the asset has a valid standard preview without it. See the [glTF extension rules](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#specifying-extensions) and [extension registry guidance](https://github.com/KhronosGroup/glTF/tree/main/extensions).
- There is **one authoritative VS payload** per asset. A packaging change requires a schema/converter-version change and an explicit migration; do not emit independently editable copies in both `extras` and an extension. A native loader rejects missing required VS data or unsupported versions with an actionable error, even when a generic viewer can still show the baseline.
- Ordinary viewers need not execute facial tracks, event commands or VS rendering rules. Ignoring custom data is not a promise to preserve it on re-export. Before accepting an edited model, validate the VS payload and references; missing or altered metadata must be reported, not silently defaulted.
- Keep scene-wide scripts, room logic and gameplay catalogs in their existing JSON contracts. Model-local metadata belongs with the model, but glTF extensibility is not a reason to move all game data into it.

Open work: #205, #206 (glTF preview clips, playback/timing evidence, and a load-time check of an edited ESQ entry's bone count against the models that play it), #207 (glTF region references, texture tracks, edited-region pixel evidence).
