# Current Handoff

## State

- Implementation revision on `main`: `e577bde4e87d96525bcaa7a802894b0f942c0399`.
- [#48](https://github.com/JEschete/VS-NativePort/issues/48) is complete: owned request inputs/results, cancellation,
  immutable resource sharing and producer-to-publication/use/reset/removal adoption are verified.
- The fixing revision's CI and all acceptance evidence are recorded
  [on #48](https://github.com/JEschete/VS-NativePort/issues/48#issuecomment-7217495764).
  There is no remaining #48 implementation work.

## Current contract

Actor, weapon, sequence and auxiliary requests resolve destination records through their owners. Definitions and
animation banks are immutable; mutable geometry and playback are private. Successful completion precedes auxiliary
publication, and cancellation precedes discard. Failure and foreign-slot cleanup preserve unrelated ownership.

Shared-model instances explicitly enter the initial queued-animation boundary without borrowing a peer's pose.
Status renders only after its switcher acquires the owned preview. Gimmick room changes invalidate cache selection
without freeing transition geometry; actor removal and model/session reset retire instance bindings.

## Next and open limits

Use the owner-authorized priorities in [#4](https://github.com/JEschete/VS-NativePort/issues/4) and the integration
handoffs in [#217](https://github.com/JEschete/VS-NativePort/issues/217) for subsequent work.

Compiled SHP/WEP/SEQ rendering/playback adapters and ETM/global material/shared-bank producers are not retired.
File streams remain serialized. Remaining format and actor-lifecycle work stays under #200/#203/#205/#207/#55 and the
EC register; ownership completion does not close those separate limits.

Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits.
