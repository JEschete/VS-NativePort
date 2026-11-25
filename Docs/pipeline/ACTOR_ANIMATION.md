# Asset Pipeline: actor texture animation

[Asset pipeline index](../ASSET_PIPELINE.md)

Character model conversion (§6.2, §6.2.1) is in [CHARACTER_MODELS.md](CHARACTER_MODELS.md).

#### 6.2.2 Event and facial texture animation contract

Treat skeletal pose, texture overlays and event synchronization as related but separate data. A correct ESQ bone pose alone does not prove facial playback, and texture-based expressions do not prove that every facial movement excludes bone animation.

The [original ETM loader](../../resources/rood-reverse/src/BATTLE/BATTLE.PRG/30DB0.c) skips a 16-bit word, reads a 16-bit height and uploads 128 bytes per row. [Native texture animation](../../Native/Game/src/Actors/TextureAnimation.cpp) selects rectangular frames and handles packed texture addressing. The sheet-to-model binding (which lines each model uses, at which depth and with which palette) is established and converted (§6.2.1, `etm-png`); the frame, palette-animation and event bindings below are not yet, and AP1 must establish them before the glTF carries texture tracks.

| Contract component | Required content |
|---|---|
| Provenance and association | Source paths/hashes, legacy model/bank/clip IDs and evidence for the ESQ/ETM/SHP association. Preserve source meaning without using executable addresses as native IDs. |
| Atlas binding | Stable binding ID, glTF image reference, actor/model association, index depth and palette selection. Resolve 4-bit/8-bit interpretation per binding; do not assume an entire ETM uses one depth or carries its own palette. |
| Texture patches | Logical destination texture/region, source and destination rectangles in integer pixel coordinates, atlas columns/frame ordering and overlay-slot identity. Validate bounds and packing; keep original layout/offset provenance separate from native texture ownership. |
| Tracks and clock | Integer frame durations, declared tick rate, clip/event associations, start/stop commands, looping and synchronization with skeletal clips. Derive boundary/order semantics from the original scheduler rather than inferring them from filenames or a viewer's frame rate. |
| Lifecycle | Base-texture restoration, bank/clip switching, actor-local mutable texture state and cleanup on cancellation, scene exit and repeat entry. Shared immutable images must not make one actor's expressions change another actor. |

The character glTF references the bound images and carries the actor-local contract. The images are written once as `textures\actors\<ETM>\<binding>.png` plus `.image.json` (§6.4) and shared by the runtime, Workshop and editors; there is no second copy. Preserve palette indices, all consumed palette entries and transparency/STP information, not only a baked RGBA appearance. A chosen-palette RGBA PNG or individual expression-frame PNG is a convenient additional export, not a lossless replacement for the indexed data.

Resolve colors from the verified character palette binding; never guess a palette or interpret index bytes as RGB. Missing/ambiguous bindings remain explicit errors or `unconverted` entries with diagnostics, not plausible-looking successful exports. Inventory unresolved regions and aliases until their consumption is understood. Multiple characters may reference the same source bank without duplicating meshes; §8 tracks every dependency that contributes to each output.

#### 6.2.3 Bounded implementation packages

All packages below are **planned**, not completed by this document. Their prerequisites describe technical dependencies, not permission to reorder tracker #4. They are incremental implementation slices within the up-front conversion architecture; runtime lazy conversion remains prohibited.

| Package | Prerequisite | Deliverable and acceptance gate |
|---|---|---|
| AP1: establish source bindings | Asset work reaches its place in #4 | Trace original loaders, event producers and texture-frame consumers. Record exact SHP/ESQ/ETM/CLUT/region/clip mappings and scheduler/reset behavior for at least two actors covering 4-bit and 8-bit bindings. Keep observed header relations and same-stem filenames separate from verified pixel/timing evidence. |
| AP2: typed decode and schema | AP1 | Extend the existing content decoders and define the §6.2.1 schema. Fixtures cover indices/palettes, rectangle addressing, duration boundaries and association rules. Reject truncated data, invalid references, unsupported depth, out-of-bounds patches and ambiguous palettes without partially publishing an asset. Keep reference code read-only; native implementations belong in existing content/actor systems. |
| AP3: inspection and PNG export | AP2 | Extend Workshop's existing viewer/export path: show source IDs, actor/bank/clip/region/palette selection, decoded atlas and individual frames; provide readable playback/pause/step controls for skeletal and texture tracks together. Export palette-correct atlas/frame PNGs, label unsupported event/render behavior, and distinguish manual frame preview from verified event playback. |
| AP4: conversion and native loading | AP2 | Implement versioned glTF/PNG writers and VS.Content readers from the same typed contract, reusing Workshop decoder findings rather than a second guessed parser. Pass exact decoded-data comparisons and schema/glTF validation. Two unchanged extraction runs produce byte-identical outputs with no unnecessary rebuild; changing any consumed SHP/SEQ/ESQ/ETM/palette dependency rebuilds affected outputs. |
| AP5: production playback and fidelity | AP4 | Connect typed tracks to the existing native actor/event lifecycle, not a new hardware-shaped bridge. Exercise normal scene/event flow with synchronized body and expression playback, loops, cancellation, bank switching, two actors sharing source data and repeated scene entry. Prove patch pixels and unchanged pixels outside the target, then compare named scene/event states against the reference. Record inspection, preservation and rendering results separately (§7.1). |

AP3 and AP4 can proceed independently after AP2. The two-actor slice establishes the contract, not full coverage: before the content gate passes, inventory every ESQ/ETM and settle each consumed binding or record a justified skip. Do not claim universal viewer support or facial fidelity from a single successful clip.

**Rendering boundary:** the unlit glTF baseline and baked preview colors are not the final lighting implementation. Preserve vertex colors, per-primitive lighting/blend flags, palette changes and scene lighting inputs for the native renderer. Standard `alphaMode: BLEND` does not express every PS1 blend operation. For a reported cutscene rim/backlight effect, trace the specific shot's texture, lighting, vertex-color and event inputs before choosing its native representation; do not assume it is an animated texture.
