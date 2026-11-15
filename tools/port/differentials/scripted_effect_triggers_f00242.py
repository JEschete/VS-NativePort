"""F00242 func_8007BF0C (BATTLE.PRG 146C.c:8617), typed Effects::IsScriptedEffectTriggerReady."""

from scripted_effect_triggers_common import READY, configure

globals().update(
    configure(
        242,
        READY,
        0x8007BF0C,
        0x8007BF2C,
        {
            "raw-result": {0x8007BF20: 0x00000000},
            "inverted": {0x8007BF20: 0x2C420001},
            "callee-is-phase": {0x8007BF14: 0x0C033D23},
        },
    )
)
