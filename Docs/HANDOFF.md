# Current Handoff

## State

The verified runtime revision is `a49383bf1828a568e2655b2829c0a3575c157dd4`. The actor identity and instance/resource lifecycle slice in [#55 is complete](https://github.com/JEschete/VS-NativePort/issues/55#issuecomment-7235106314), including the #48 request-to-publication handshake and pre-heap cancellation/invalidation. Actor allocations retain immutable creation definitions, generation identities, independent component bindings and expected AI allocations.

## Work in progress

No actor-lifecycle implementation or verification remains in progress for #55. Work on #58 has not started.

## Next

The next adoption item is [#58](https://github.com/JEschete/VS-NativePort/issues/58), subject to [#4's priorities](https://github.com/JEschete/VS-NativePort/issues/4) and [#217's integration order](https://github.com/JEschete/VS-NativePort/issues/217). The #55 ownership handoff is also available to #59; completing #55 does not complete those later work items.

## Open limits

Runtime model/animation/collision adapters, synthetic asset-format bridges and the remaining EC-04/05 gates retain their existing work items. The adopted actor lifecycle is not an ECS rewrite or blanket compatibility retirement.

Controller evidence covers original event progression, the first enemy encounter, ordinary player defeat, reset and rendered re-entry. It does not claim enemy defeat or completion of the full prologue/game.

Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits or issue attachments. Commands and evidence remain on #55, not in this handoff.
