# Current Handoff

## State

- Movie implementation revision on `main`: `f4b88a9ca0352cde24a9b4ef272c8d0f05c823f6`.
- [#53](https://github.com/JEschete/VS-NativePort/issues/53) is complete. Acceptance evidence is recorded
  [on the issue](https://github.com/JEschete/VS-NativePort/issues/53#issuecomment-7227035440), and the exact-revision
  [CI run](https://github.com/JEschete/VS-NativePort/actions/runs/46060548984) passed.
- No movie implementation work is in progress.

## Next and open limits

Follow the owner-authorized order in [#4](https://github.com/JEschete/VS-NativePort/issues/4) and the integration
handoffs in [#217](https://github.com/JEschete/VS-NativePort/issues/217); #217 recommends #60 next.

The adopted title movie path uses owned streaming playback, a bounded audio-sample clock and GPU frame presentation.
Pause and seek are unsupported; PSNR/perceptual parity and direct speaker audibility were not measured. Broader
EC-08 compatibility retirement remains separate. C++ promotion remains the previously completed #276 status.

Keep `resources\rood-reverse` read-only and disc-derived captures, data, dumps and BIOS files out of commits or issue
attachments.
