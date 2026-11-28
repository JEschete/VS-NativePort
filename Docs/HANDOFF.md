# Current Handoff

## State

The last CI-verified published lifecycle revision is `b905975c8`; [integration and controller evidence is on #55](https://github.com/JEschete/VS-NativePort/issues/55#issuecomment-7234706517). Actor allocations retain immutable creation definitions, generation identities, independent component bindings and expected AI allocations.

## Work in progress

Continuing #55 with the owner's authorization through closure. Final boundary verification reproduced actor identities and pending loads surviving actual heap replacement, followed by late scripted cleanup against the new heap. The in-flight repair cancels or drains owned loads and invalidates instances before replacing the heap. [The failing cases and repair scope are recorded on #55](https://github.com/JEschete/VS-NativePort/issues/55#issuecomment-7234807777).

## Next and open limits

Finish Debug/Release pre-heap and dependent lifecycle verification, repeat interrupted-load and rendered re-entry checks on the repair, then confirm exact-revision CI. Semantic snapshots, canonical retained targets and terminal AI cleanup are already integrated. The extended controller run reached the first enemy encounter and ordinary player defeat; it is not a claim of completing the full prologue or defeating those enemies.

Runtime model/animation/collision adapters and all EC-04/05 compatibility are not retired merely by changing instance ownership.

Keep #55 open until every acceptance criterion and exact-revision CI gate passes; do not advance to #58. #59 remains gated on #55. Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits or issue attachments.
