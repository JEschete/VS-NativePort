"""F01920, PLG052 INCLUDE_ASM: verified independently, never replaced by PLG077."""

from repeating_rings_common import configure

configure(
    globals(),
    ring=0,
    ledger_id="F01920",
    module="EFFECT/PLG052.BIN",
    entry=0x800F9800,
    end=0x800FA770,
    palette_address=0x800FA770,
)
