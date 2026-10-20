# Third-party implementation references

## rood-boi battle AI counterparts

The owned [AI planning primitives](Native/Game/src/Battle/Ai/PlanningPrimitives.cpp)
adapt the C counterparts in `pc/src/battle_helpers.c` and
`pc/src/battle_movement.c` from
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
