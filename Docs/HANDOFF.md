# Current Handoff

## State

- Published source baseline on `main`: `62ba5955ee6a5103404424d3f7ed38ee82976478`.
- Active work: final integration and publication of [#48](https://github.com/JEschete/VS-NativePort/issues/48).
  Ownership implementation is complete; issue closure still requires the pushed revision's successful CI.
- The production Status entry-ordering reproduction is recorded
  [on #48](https://github.com/JEschete/VS-NativePort/issues/48#issuecomment-7217239279).
  Its correction defers the first render until the switcher acquires the owned preview, preserving the load timing.

## Integrated ownership contract

Actor, weapon, sequence and auxiliary requests resolve destination records through their owners. Queued loads and
previews retain immutable model definitions and animation banks; mutable geometry and playback state are private.
Auxiliary publication follows successful completion, and cancellation precedes discard. Partial failure, foreign
slots, occupancy, repeated entry and reset have explicit cleanup. The obsolete shared-record-slot request field is gone.

Shared-model instances explicitly enter the initial queued-animation boundary without copying a peer's pose.
Status initialization no longer relies on a previous entry's preview record. Gimmick room changes invalidate cache
selection without freeing transition geometry; actor removal and model/session reset retire the instance bindings.

## Next

Publish the verified #48 integration, confirm exact-revision CI, post the final acceptance evidence on the issue,
then check the remaining criterion and close it. Do not stop at this publication checkpoint.

## Open limits

Compiled SHP/WEP/SEQ rendering and playback layouts remain compatibility adapters. ETM and global material/shared-bank
producers remain their existing integration work (#207 and the EC register); ownership does not retire their formats.
File streams remain serialized. Remaining format and actor-lifecycle work stays under #200/#203/#205/#207/#55 and the
handoffs in #217.

Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits.
