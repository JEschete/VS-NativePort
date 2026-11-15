"""F00119 func_800733F8 (BATTLE.PRG 146C.c:4613), typed Effects::BeginAshleyStandaloneEffect."""

from action_callbacks_common import BEGIN, configure

globals().update(
    configure(
        119,
        BEGIN,
        0x800733F8,
        0x80073484,
        {
            "control-mode-5": {0x8007340C: 0x24100005},
            "count-2": {0x80073414: 0x24020002},
            "variant-1": {0x80073418: 0x24050001},
            "others-unwritten": {0x80073428: 0x00000000},
            "source-slot-unwritten": {0x80073434: 0x00000000},
            "position-z-unwritten": {0x8007344C: 0x00000000},
            "hud-shown": {0x8007345C: 0x00002021},
            "minimap-shown": {0x80073470: 0x24040001},
        },
    )
)
