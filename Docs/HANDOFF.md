# Current Handoff

## State

Runtime revision `705b295a567dcda44b786e8a8b47fe08fb892b2b` adopts normal single-hit physical action 1 through owned combat inputs/results, generation-qualified delayed impacts and separate presentation consumers. The [#56 publication checkpoint](https://github.com/JEschete/VS-NativePort/issues/56#issuecomment-7238915005) records the verified scope and the hazard-ledger CI follow-up.

The [#55 actor ownership slice is complete](https://github.com/JEschete/VS-NativePort/issues/55#issuecomment-7235106314). Combat uses its expected components rather than replaceable borrowed stats, model or AI bindings.

## Work in progress

The owner authorized continued work until [#56](https://github.com/JEschete/VS-NativePort/issues/56) is closed. Controller/AI submission, previews, delayed impacts, camera/effect/animation consumers, completed AI plans and pre-heap cancellation are adopted. Final-source Debug controller runs cover a player attack, interruption of a pending action, natural player defeat and rendered re-entry. Release controller checks and the package regression gate remain in progress.

The published runtime's CI reported two namespace-token division candidates. Their divisor is the nonzero `Items::kUnarmedWeaponCategory` constant; the ledger now records that classification explicitly. Confirm the correcting revision's CI before treating this gate as resolved.

## Next

Finish Release controller/capture inspection and the Debug package regression, validate the Desktop timing option's boundaries, then publish the complete acceptance evidence on #56 and close it only after exact-revision CI succeeds. Do not stop at this partial checkpoint or start another issue in its place.

## Open limits

Only the normal single-hit physical rule shape is adopted. Other action families retain their explicit compatibility route. Broader AI/query, equipment/asset and effect ownership retirement remains under [#57](https://github.com/JEschete/VS-NativePort/issues/57), [#58](https://github.com/JEschete/VS-NativePort/issues/58) and [#61](https://github.com/JEschete/VS-NativePort/issues/61); this is not full combat or M8 completion.

Controlled comparisons use the opt-in stepped AI budget. Default wall-clock runs are not asserted deterministic, and accelerated captures do not certify real-time audio quality. The [cross-region script-offset diagnostic](https://github.com/JEschete/VS-NativePort/issues/59#issuecomment-7238503115) remains separate from combat behavior.

Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits or issue attachments. Evidence remains on the issues, not in this handoff.
