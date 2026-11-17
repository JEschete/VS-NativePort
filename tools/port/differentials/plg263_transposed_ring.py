"""F02323, PLG263 func_800FB0CC, src/EFFECT/PLG263.BIN/110.c:40."""

from repeating_rings_common import configure

configure(
    globals(),
    ring=4,
    ledger_id="F02323",
    module="EFFECT/PLG263.BIN",
    entry=0x800FB0CC,
    end=0x800FC00C,
    palette_address=0x800FDF50,
)
