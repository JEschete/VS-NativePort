"""F02209, PLG162 func_800F9800, src/EFFECT/PLG162.BIN/0.c:32."""

from repeating_rings_common import configure

configure(
    globals(),
    ring=2,
    ledger_id="F02209",
    module="EFFECT/PLG162.BIN",
    entry=0x800F9800,
    end=0x800FA740,
    palette_address=0x800FD18C,
)
