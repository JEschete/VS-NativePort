# Current Handoff

## State

The last CI-verified published lifecycle revision is `ad5d4b96d`, including the identity implementation in `1cbddbb02`; [evidence is on #55](https://github.com/JEschete/VS-NativePort/issues/55#issuecomment-7233280568). Actor allocations have complete typed components, immutable creation definitions and generation identities. Roster links and retained decision/navigation/targeting cursors retain generations, and only native-owned allocation storage is freed.

## Work in progress

Continuing #55 with the owner's authorization through closure. The current component-ownership change fixes the [reproduced resource and selection failures](https://github.com/JEschete/VS-NativePort/issues/55#issuecomment-7233193672): component destinations are retained independently of mutable actor views; expected-record teardown preserves foreign bindings and handles partial equipment; selected actors are cleared on removal; room special targets retain generations.

## Next and open limits

Finish explicit actor registration and the remaining AI/persistence ownership checks, including failed publication when a slot changes. Then verify the complete create/use/remove/reset slice with fresh lifecycle and applicable controller evidence. Runtime model/animation/collision adapters and all EC-04/05 compatibility are not retired merely by changing instance ownership.

Keep #55 open until every acceptance criterion and exact-revision CI gate passes; do not advance to #58. #59 remains gated on #55. Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits or issue attachments.
