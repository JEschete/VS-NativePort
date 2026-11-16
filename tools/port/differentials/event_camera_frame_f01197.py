"""F01197 func_800BB41C (BATTLE/BATTLE.PRG 4A0A8.c:2788), typed Event::CommandWaitCameraTracks."""

from event_camera_frame_common import WAIT, configure

globals().update(
    configure(
        1197,
        WAIT,
        0x800BB41C,
        0x800BB450,
        {
            "and-to-or": {0x800BB43C: 0x00621025},
            "xor-to-addu": {0x800BB440: 0x00621821},
            "operand-byte-0": {0x800BB430: 0x92030000},
        },
    )
)
