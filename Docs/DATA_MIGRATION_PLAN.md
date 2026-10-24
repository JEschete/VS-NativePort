**Yes—this is a good time to establish the new content architecture, before item/spell work deepens the old dependencies.**

## What VS-Workshop actually does

It is **an independent parser/editor of the original files**, not a tool that runs the game and dumps its interpreted memory.

Its current paths are:

| Operation | What actually happens |
|---|---|
| Open a disc | Reads BIN/CUE/ISO sectors and parses their files into C# meshes, bones, images, fields, and placements. |
| Edit original content | Stages byte changes. Replacements must retain the original file length so offsets and LBAs remain valid. |
| Save a project | Saves source hashes and binary patches—not a fully independent native content package. |
| Extract a file | Writes the original file bytes, or raw sectors for streaming content. |
| Export an asset | Produces GLB with geometry, textures, and supported skeletal animation. Blender-based conversion can produce `.blend` or FBX. |

See [Workspace.cs](../../Modding/VS-Workshop/src/Workshop.Core/Workspace.cs#L28) and [ExportWorkflow.cs](../../Modding/VS-Workshop/src/Workshop.App/ExportWorkflow.cs#L16).

So **yes, it can dump and convert content—but merely opening it does not convert everything into a new authoring format.**

Important limits:

- Room exports contain static geometry/materials, **not runtime spawning or scripts**.
- Collision preview exposes floor/ceiling heights and modes; it explicitly is **not the game’s collision simulation**.
- Actor exports include sampled motion, but explicitly exclude game events, shaders, and IK.
- Its [glTF importer currently rejects skins and animations](../../Modding/VS-Workshop/src/Workshop.Core/GltfImporter.cs#L28). The complete Blender → edited rigged actor → game loop does not exist yet.
- `.vscene` supports extended geometry/object editing, but remains bound to a source disc/room hash. It is not yet the native game’s room definition.

### The native port is still mostly consuming original formats

The existing `Native\data\files.tsv` lists **5,160 raw files**, one converted movie, and one streaming entry. The current [bulk extractor](Native/Tools/Extract/src/main.cpp#L238) mostly copies files unchanged. The dirty audio work adds separate native audio conversion.

## What to break into

The right boundary is:

**Legacy files → validated import records → editable native content → typed runtime objects**

Not:

**Legacy files → differently named files → reconstruct the same binary layouts forever**

Your [existing asset plan](Docs/ASSET_PIPELINE.md) already points toward glTF, PNG, and structured catalogs. The work now is making those contracts real.
Define a contract for every type of content, specifying its source format, editable representation, and runtime usage.

### Recommended format split

| Content | Recommended representation |
|---|---|
| Items, spells, enemies, equipment definitions, loot | Relational catalogs with stable IDs and validated references. SQLite is a sensible editor-side database. |
| Room appearance | glTF 2.0 geometry/materials. Blender can be the authoring tool. |
| Room gameplay | Versioned room JSON: dimensions, coordinate system, collision cells/shapes, exits, triggers, spawns, cameras, puzzle state, and script references. |
| Actors and weapons | Skinned/static glTF 2.0 as appropriate; `.blend` as an editable source where useful. |
| Animation motion | glTF clips bound to an identified skeleton. |
| Action-to-animation relationships | Separate structured action/animation-set definitions, including timing and contextual selection. |
| Images | Lossless PNG, with palette/index and material metadata preserved where required. |

For databases, I would separate **relational authoring** from **runtime storage**. A practical fit with the existing plan is:

**Workshop’s SQLite catalog → deterministic JSON publication → native typed registries.**

The game need not execute SQL during combat. Nor should the database and exported JSON become competing manually maintained sources of truth. Introducing SQLite would be an explicit addition to the current JSON-catalog decision.

Also, **a WEP is not an item definition**. Weapon appearance should be referenced by equipment definitions; multiple weapons can share a mesh while differing in material, stats, components, or instance state.

## Rooms, tiles, and “Z maps”

Room geometry and gameplay dimensions must remain separate.

The visible mesh answers “what does this room look like?” The gameplay definition answers:

- Where can an actor stand?
- What are the floor, ceiling, slope, and blocking rules?
- Where are exits, interactions, and spawn conditions?
- What changes when a puzzle is cleared?
- What survives leaving and re-entering?

**The MPD tile grid is not necessarily a set of reusable visible tiles.** Workshop already distinguishes baked geometry from collision cells. Preserve that distinction rather than immediately replacing everything with a generic navmesh or deriving collision from the visible mesh.

On “Z maps”: I found decoded RGBA textures and separate room collision-height data, **not a general image-plus-depth-map pairing** in the inspected paths.

If you mean:

- **Room heights:** store those as exact gameplay data.
- **Actual per-pixel depth:** use an appropriate lossless numeric image representation—16-bit PNG where sufficient, or losslessly compressed floating-point EXR where necessary—with units and interpretation documented.
- **Rendered scene depth:** normally generate it from geometry and the camera; it is not an intrinsic companion to every texture.

PNG alone also does not preserve palette animation or PS1 blending behavior. Keep palette relationships, transparency flags, and blend modes alongside the pixels.

## Models, bones, and tying animations to actions

**glTF solves the motion container, not the action system.** The [glTF specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) deliberately leaves animation playback decisions to the application.

The native code already exposes why a simple “attack → clip 12” table is insufficient:

- [Battle animation selection](Native/Game/src/Battle/ActionAnimation.c#L336) chooses variants using animation group, target pitch, and side.
- [Actor animation playback](Native/Game/src/Actors/Animation.c#L669) further changes selection according to weapon-drawn and movement state.

The replacement should express:

**Action + actor animation set + weapon/stance/context → clip selection + gameplay timeline**

Keep these separate:

- **Motion:** bone transforms and root movement.
- **Bindings:** skeleton identity, weapon/effect sockets, and targetable body parts.
- **Action timing:** impact, effect/sound cues, movement restrictions, and interruption rules.
- **Behavior:** registered native gameplay code.

Recover those relationships from the loaders, action tables, and event/effect consumers—not filenames or matching bone counts. Workshop’s sampled 30-fps export is useful, but does not itself establish timing equivalence.

Likewise, “one Zombie” should mean **shared species/assets plus explicit variants**, not collapsing legitimate differences. The existing [enemy scheme](Docs/ENEMY_DATA_SCHEME.md) already separates species, enemy definitions, loadouts, loot, and room spawns.

## Is now the right point in the issue order?

The live [tracker #4](https://github.com/JEschete/VS-NativePort/issues/4) shows the maintained-code quality prerequisite complete; menu work and initial enemy/defeat work are closed. **#7 audio is active, followed by #8 items and #9 spells.**

There is already an appropriate architectural issue: [#47—stable asset IDs and decoded records](https://github.com/JEschete/VS-NativePort/issues/47).

My recommendation:

1. **Checkpoint the active audio work first.** There are substantial uncommitted changes; do not mix a content rewrite into them.
2. **Agree on the native contracts now.**
3. Explicitly schedule a **bounded #47 slice before deepening #8/#9**. That would be an intentional adjustment to the order, not an assumed authorization.
4. Prove it with **one room, one enemy variant, and one real combat action**.

The proof should include editing/reimporting an asset, changing a definition and placement without repacking legacy binaries, exercising actual gameplay, and leaving/re-entering correctly. Conversion equivalence and consumer adoption both matter.

