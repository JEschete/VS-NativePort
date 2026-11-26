# Asset Pipeline: SHP layout and bone fields

[Asset pipeline index](../ASSET_PIPELINE.md)

These are the named members of a character model's `asset.extras.vs` payload (§6.2,
[CHARACTER_MODELS.md](CHARACTER_MODELS.md)). The meanings come from the native code that reads them.

#### 6.2.6 SHP layout and bone fields (#520)

**Units.** Lengths are world units, the same integer units as the model's positions (§6.2). Y points down, so a
height is measured up from the actor's feet (`position.y`) and a negative y offset is above them. Unsigned halfword
fields hold 0 to 65535; `anchor_height` holds -32768 to 32767. The reader refuses a value outside its field's
range, naming the member.

**Layout block (`layout`).**

| Member | Meaning (consumer) |
|---|---|
| `collision_radius` | Body radius against terrain and in ground navigation (`Field/ActorCollision.cpp`, `Battle/Ai/GroundNavigation.cpp`). Large values also change the head-tracking step (`Skeleton.cpp` `HeadTrackingSpeed`) and double animation blend frames (`AnimationControl.cpp`). |
| `collision_height` | Body height against ceilings: a probe fails when `y - collision_height` is above the ceiling (`Actors/ActorPlacement.cpp`). |
| `occupancy_radius`, `occupancy_height` | The box one actor keeps from another: ± radius on x and z, and from the feet up by the height (`Field/ActorCollision.cpp` overlap tests). Independent of `collision_*`. |
| `target_point_height` | Height above the feet of the actor's target point, the point that point selector 0xFB names (`Skeleton.cpp` `TargetPoint`). |
| `anchor_height` | Signed y offset of the anchor point (selector 0xFA), scaled by the actor's scale (4096 = 1): `y + anchor_height × scale / 4096` (`Skeleton.cpp` `AnchorPoint`). Negative values are above the feet. |
| `floor_lit` | When true, the model is tinted by the room's floor light under it (`ModelDrawingLive.cpp`). |
| `shadow_radius` | Ground shadow radius. The drawn size also follows the actor's height over the floor and is never below 20 (`Render/ModelGeometry.cpp` `BuildShadowTransform`). |
| `shadow_radius_shrink_rate`, `shadow_radius_expand_rate` | Resting scale of the shadow along its x (`shrink`) and z (`expand`) axes, 8192 for full size (`ModelGeometry.cpp` `ShadowColumnScale`). Animation event 60 blends the shadow to other values and back (`Appearance.cpp` `BeginActorShadowBlend`). |
| `menu_camera_distance` | Camera distance when menus and the status screen show the model (`Menu/Status/TerminalControllersLive.cpp`, `Menu/Framework/FrameworkLive.cpp`). |
| `battle_sequence_lbas`/`_sizes`, `special_sequence_lbas`/`_sizes` | Source location of the owned SEQ banks, sizes in bytes; edit `sequence_bank_ids` instead (§6.2). |

**Texture animation records.** Positions and sizes are texels of the model's texture.

- `texture_overlays[8]` (`x`, `y`, `width`, `height`): destination rectangles that texture animation copies frames
  into. Event and cell-sheet `overlay` numbers name them (§6.2.2).
- `texture_cell_origins[2]` (`x`, `y`, `columns`): the default frame sheet for animation channels 0 and 1, used when
  an event sets no sheet. Frames run `columns` per row, each cell the size of the overlay it fills. `columns` 0
  disables the default.
- `texture_scroll_bounds[4]` (`x`, `y`, `width`, `height`, `initial_step_x`, `initial_step_y`, `bounces`,
  `interval`): a scrolling window for overlay layer n. `width` 0 disables the layer. A step moves the source by
  the signed step in texels. At an edge the window returns to `x`/`y`, or, with `bounces`, reverses. Each update
  adds half the game speed to a timer; the window steps when the timer passes `interval`
  (`TextureAnimation.cpp` `StepTextureScrolling`).

**Bones (`bones[]`).**

| Member | Meaning (consumer) |
|---|---|
| `length` | A child's origin is its parent's origin moved by minus the parent's `length` along the parent's x axis. The glTF node translation shows it. Origins must stay within ±2^23 (`ShpModelFiles.cpp` `ShpBoneOrigins`). |
| `parent` | A lower-numbered bone; the reader refuses any other. Bone 0 is the root and its value is not read. |
| `tag` | A number that actions and effects use to find a bone, e.g. an action range standing on the bone tagged 250 (`Skeleton.cpp` `FindBoneTag`). |
| `limb_ids` | Two limb groups, high and low nibble, 0 for none. A lookup for group g matches a nibble of g + 1 (`Skeleton.cpp` `FindBoneTag`). |
| `auxiliary_tag` | Drawing rule: 1–2 turn the bone by the actor's look yaw and pitch, 3–6 spin it while the actor is frozen, 7–9 face it to the camera (`ModelDrawingLive.cpp`). It is also a bone lookup key. |
| `pose_lock_slot` | 0 draws the bone from its animation. 1 or 2 draw it from saved matrix 1 or 2, which animation events also set at run time. A model with a bone above 2 is not drawn. |
| `joint_init` | No native code reads it; keep the source value. |

**Weapons.** WEP `bone_rotations` are 4096 to a turn (`WepModelFiles.cpp`), like the SEQ rotation keys.

Remaining: which battle and special slot serves which weapon category or role, and the meaning of the header's
`sound`, `sequence_word` and `palette_words` values.
