# Current Handoff

## Current state

Implementation revision `f2df0c31e064bb155eead95d3f76a8a653aa78fe` adopts owned segment-collision inputs under [#57](https://github.com/JEschete/VS-NativePort/issues/57). Both live segment entries capture actor bounds, moving platforms and decoded collision heights before querying. Captured geometry survives source changes and teardown; captured actor identities keep their original generation. Publication and verification evidence remains on the issue.

The adopted normal single-hit physical action 1 path in [#56](https://github.com/JEschete/VS-NativePort/issues/56) is complete. Controller and AI producers use owned combat inputs/results, generation-qualified delayed impacts and separate presentation consumers. [Acceptance evidence](https://github.com/JEschete/VS-NativePort/issues/56#issuecomment-7239339298) records that closure.

The [#55 actor ownership handoff](https://github.com/JEschete/VS-NativePort/issues/55#issuecomment-7235106314) remains complete. Combat reads and commits expected actor components rather than adopting replaceable borrowed bindings.

## Work in progress

[#57](https://github.com/JEschete/VS-NativePort/issues/57) remains open. Segment traces no longer read live globals while querying, but navigation/perception contexts, planner simulation ownership and persistent movement probes still require adoption. The planner still publishes its temporary buffers globally and modifies live simulated stats; movement still shares probe/support state.

## Next

Confirm the collision milestone's pushed CI, then thread owned probe/support state and explicit room/actor inputs through real movement, navigation/perception and planning callers. Preserve immediate platform-contact callbacks and source ordering. Publish verified milestones and continue through #57's full acceptance and exact-revision CI. The owner-authorized endpoint is #57 closure; do not start the next issue without further authorization.

## Open limits

Other action families retain their explicit compatibility route. Independent AI/navigation/planning ownership and current-worktree controller acceptance remain open under [#57](https://github.com/JEschete/VS-NativePort/issues/57). Equipment/asset and effect ownership retirement remains under [#58](https://github.com/JEschete/VS-NativePort/issues/58) and [#61](https://github.com/JEschete/VS-NativePort/issues/61); this is not full combat or M8 completion.

Unchanged image-conversion test tidy findings discovered by the expanded header scan are tracked separately in [#531](https://github.com/JEschete/VS-NativePort/issues/531); no quality baseline was refreshed.

Controlled comparisons use the opt-in stepped AI budget. Default wall-clock runs are not asserted deterministic, and accelerated captures do not certify real-time audio quality. The [cross-region script-offset diagnostic](https://github.com/JEschete/VS-NativePort/issues/59#issuecomment-7238503115) remains separate from combat behavior.

Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits or issue attachments. Evidence remains on the issues, not in this handoff.
