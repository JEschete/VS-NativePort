# Current Handoff

## State

- Verified source revision: `098dbb8e4be4f3d5b39abbba89469333c760ec48` (with optional-stream fix
  `b81206114f94a5807e8123ebb82c5caf19c6d197`).
- [#529](https://github.com/JEschete/VS-NativePort/issues/529) invalidates version-9 launcher caches after the
  workshop result schema change. The refreshed version-10 assets were accepted by a one-frame game startup; issue
  evidence records the CI run and test commands.
- [#530](https://github.com/JEschete/VS-NativePort/issues/530) handles an absent optional AKAO stream-authoring
  directory as empty; its regression and integration evidence are recorded on the issue.
- The game-start check covers asset/catalog startup only; it does not claim interactive gameplay or save/load parity.

## Next and open limits

Follow the owner-authorized order in [#4](https://github.com/JEschete/VS-NativePort/issues/4) and integration
sequence in [#217](https://github.com/JEschete/VS-NativePort/issues/217). #59 remains gated on the open #55
actor-lifecycle handoff; complete that handoff before claiming #59 adoption.

Broader string-table and character-map conversion remains open under [#500](https://github.com/JEschete/VS-NativePort/issues/500).
The adopted instant-text slice does not implement progressive dialogue playback, full Unicode shaping/line breaking,
or retire the page/packet rendering backend.

Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits or issue
attachments.
