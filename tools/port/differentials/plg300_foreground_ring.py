"""F02398, PLG300 func_800FD198, src/EFFECT/PLG300.BIN/110.c:355."""

from repeating_rings_common import configure

configure(
    globals(),
    ring=6,
    ledger_id="F02398",
    module="EFFECT/PLG300.BIN",
    entry=0x800FD198,
    end=0x800FE0CC,
    palette_address=0x800FE370,
)
