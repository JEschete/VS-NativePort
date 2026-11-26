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

**Event scripts (#520).** A bank's event script (§6.2.1) writes each event as an object: `frame` (the clip frame it runs at), `op` (its name) and the op's named fields, in the order the game reads its argument bytes (`Content/SeqEventForms.cpp`, after the handlers in `Actors/AnimationEvents.cpp`). Fields are bytes (0–255) unless a range is given. An op the game does not run keeps its number and ends the script, as `end` does. Arguments that do not fill an op's fields are written as `bytes`, as are those of `ignored` and `ignored-long`, which the game steps over. The reader refuses an unknown name, a named op written as its number, a field out of range, a member the op does not take and `bytes` beside a named field. "Reloads" means the script starts again from the (new) animation's first event.

| Op (number) | Fields | What the game does |
|---|---|---|
| `end` (0) | — | Ends the script. |
| `initialize-pose` (1) | — | Resets the pose and reloads. |
| `impact` (2) | — | Queues the action's impact effects, or else plays animation 1 and reloads. |
| `play-sound-pair` (4) | `sound` | Plays a sound pair below height 3000; sound 88 plays the drawn weapon's sound. |
| `play-actor-sound` (5), `play-effect-sound-at-volume` (8) | `sound`, `volume` | Plays an actor or effect sound at the actor's pan and this volume; 0 plays nothing. |
| `play-effect-sound` (7) | `sound` | Plays an effect sound at the actor's pan and volume. |
| `stop-actor-sound` (6), `stop-effect-sound` (9) | `sound` | Stops the sound. |
| `play-model-sound` (10), `play-surface-contact-sound` (11) | `sound`, — | Plays one of the model's sounds; the surface contact sound. |
| `start-effect-track` (12), `source-effect` (63) | `track`, — | Starts an effect track; spawns the source effect. Both do nothing while the event flag is set. |
| `restart-unless-advanced` (13) | — | While effects run and no advance is requested, acts as `initialize-pose`. |
| `start-weapon-trail` (14), `fade-weapon-trails` (15) | `bone`, `frames` | Starts a weapon trail; fades the trails over the frames. |
| `play-animation-13` (19), `play-animation-1` (20) | `blend_frames` | Plays animation 13 or 1, blending over the frames, and reloads. |
| `play-animation` (21), `play-blended-animation` (22) | `animation`, `blend_frames` (22 only) | Plays the animation and reloads. |
| `previous-animation` (23), `next-animation` (24) | — | Plays the animation before or after the current one and reloads. |
| `weapon-pose-on` (25), `weapon-pose-off` (64) | — | Sets or clears the actor's action lock. |
| `show-weapons` (26), `hide-weapons` (27) | `frames` | Scales the attached weapons in or out over the frames. |
| `gimmick-interaction` (28), `palette-fade` (29) | `command`, — | Applies a gimmick command; starts the palette fade. |
| `cell-sheet-0` (30), `cell-sheet-1` (31) | `overlay`, `source_x`, `source_y`, `columns` | Sets texture cell sheet 0 or 1. |
| `play-timeline-1` (32), `stop-timeline-1` (33), `play-timeline-0` (34), `stop-timeline-0` (35) | — | Plays or stops a texture animation timeline. |
| `texture-overlay` (44) | `overlay`, `source_x`, `source_y` | Uploads a texture overlay. |
| `move-vertex-x` (36), `-y` (37), `-z` (38) | `vertex` (0–511), `delta` (−64–63) | Moves the model vertex along the axis; unwinding reverses it. |
| `move-vertex` (39) | `vertex` (0–511), `x` (−64–63), `y`, `z` (−128–127) | Moves the vertex on all three axes. |
| `ignored` (40), `ignored-long` (41) | `bytes` | Stepped over. |
| `shadow-expand` (51), `shadow-shrink` (52) | `rate` (0–65535), `frames` | Blends that shadow radius rate to `rate` over the frames (0: at once). |
| `shadow-radius` (53) | `expand_rate`, `shrink_rate` (0–65535), `frames` | Blends both rates. |
| `shadow-level` (54) | `level` (0–65535, 4096 a turn), `frames` | Turns the shadow level the shorter way over the frames. |
| `shadow-point` (60) | `point` | Blends to a shadow point's rates and level. |
| `wait` (55) | `frames` | Holds the script for the frames. |
| `unlock-bone` (56), `lock-bone` (57) | `bone` | Unlocks or locks the bone's pose. |
| `finish-action` (58), `reset-movement` (59) | — | Marks the action animation finished; resets the actor's movement. |
| `normal-speed` (61), `fast-speed` (62) | — | Plays animations at normal or fast speed. |

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
