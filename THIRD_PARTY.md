# Third-party implementation references

The project's original contributions, including original modifications to adapted implementations, are
licensed under **GPL-3.0-or-later**, except where otherwise noted; see [LICENSE](LICENSE) and the
[project licensing notice](README.md#license). Upstream material retains its respective license or
public-domain dedication. These notices preserve provenance and do not revoke upstream permissions
or grant rights held by the original game's rights holders.

## rood-boi battle AI counterparts

The owned [AI planning primitives](Native/Game/src/Battle/Ai/PlanningPrimitives.cpp)
and [AI decision primitives](Native/Game/src/Battle/Ai/DecisionPrimitives.cpp)
along with [actor commands](Native/Game/src/Battle/Ai/ActorCommands.cpp),
[navigation primitives](Native/Game/src/Battle/Ai/NavigationPrimitives.cpp) and
[terrain height decoding](Native/Game/src/Field/TerrainHeight.cpp)
and the [engagement lifecycle](Native/Game/src/Battle/Ai/Engagement.cpp)
and [actor cleanup](Native/Game/src/Battle/Ai/ActorCleanup.cpp)
adapt the C counterparts in `pc/src/battle_helpers.c`,
`pc/src/battle_movement.c`, `pc/src/battle_gte.c`, `pc/src/actor_cleanup.c` and
`pc/src/actor_update.c` from
[rood-boi](https://github.com/noprompt/rood-boi), reference revision
`3be5ba38b008cad919a2b1ca196432c8d4817335`.

These reference implementations are dedicated to the public domain under
**CC0 1.0 Universal**; see the read-only
[reference license](resources/rood-boi/LICENSE) and
[CC0 legal code](https://creativecommons.org/publicdomain/zero/1.0/legalcode).
The native implementations use typed host structures rather than the donor's
guest-memory accessors. Original game symbols and assembly paths are recorded
above the implementations for independent behavior checks and future upstream
comparison.

## Native math comparison

The [packed tile distance](Native/Engine/src/Core/Math/TileDistance.cpp) and shared
wide-to-short normalization behavior were checked against `pc/src/battle_movement.c`
and `pc/src/boot_sdk.c` at the same pinned rood-boi revision and CC0 dedication.
Existing native math implementations are reused for compatibility symbols rather
than importing a second guest-memory/GTE-register implementation.
