"""F02397, PLG300 func_800FC228, src/EFFECT/PLG300.BIN/110.c:48."""

from repeating_rings_common import configure

configure(
    globals(),
    ring=5,
    ledger_id="F02397",
    module="EFFECT/PLG300.BIN",
    entry=0x800FC228,
    end=0x800FD198,
    palette_address=0x800FE350,
)
