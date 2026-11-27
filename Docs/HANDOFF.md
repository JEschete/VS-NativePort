# Current Handoff

## State

- Native resource revision on `main`: `fba58a09d02f87df1aca097a21f9f07326f5b503`.
- Current work: complete #48, actor/model request ownership, cancellation and completion. The issue remains open;
  the owner requested completion, not a checkpoint-only stop.
- Current reproduction and CI repair:
  [#48 evidence](https://github.com/JEschete/VS-NativePort/issues/48#issuecomment-7215309875).
  The source guard now follows the immutable weapon binding; its pushed CI must pass before this milestone is accepted.

## In progress: #48

Queued actor/weapon inputs and menu previews retain immutable compiled models and animation banks. Mutable geometry
is private to each instance; resource leases and bank extents retire at reset. Preview records and requests are owned,
with cancellation before cleanup and foreign-record protection.

Next: adopt owned requests in the auxiliary/gimmick producer, require successful completion before publication, and
replace mutable peer-prefix sharing with immutable definitions and private geometry. Regressions already reproduce
publication after failed loading, mutable peer geometry, and partial-creation leakage.

Finish the producer-to-publication/removal path, cancellation/replacement, partial failure, repeated entry and
integrated Debug/Release and controller replay verification. Remove the remaining shared-record slot request field,
then verify all #48 acceptance criteria, publish with successful exact-revision CI, and close the issue.

Open limits: compiled SHP/WEP/SEQ rendering/playback layouts are still compatibility adapters. ETM and global material
or shared-bank producers remain their existing integration work (#207 and the EC register), not retired by resource
ownership. File streams remain serialized. Coordinate actor lifecycle with #55 and the handoffs in #217.

## Cautions

- Keep `resources\rood-reverse` read-only.
- Use focused checks; CI runs the tool suites and source-quality gates.
- Preserve unrelated edits and keep disc-derived data, captures, memory dumps and BIOS files out of commits.
