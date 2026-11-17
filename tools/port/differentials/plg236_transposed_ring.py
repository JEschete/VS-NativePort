"""F02289, PLG236 func_800FB62C, src/EFFECT/PLG236.BIN/0.c:41."""

from repeating_rings_common import configure

configure(
    globals(),
    ring=3,
    ledger_id="F02289",
    module="EFFECT/PLG236.BIN",
    entry=0x800FB62C,
    end=0x800FC56C,
    palette_address=0x800FD1B0,
)
