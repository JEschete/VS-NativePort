"""F00239 func_8007BEC4 (BATTLE.PRG 146C.c:8611), typed Effects::ReleaseScriptedEffectTrigger."""

from scripted_effect_triggers_common import RELEASE, configure

globals().update(
    configure(
        239,
        RELEASE,
        0x8007BEC4,
        0x8007BEE4,
        {
            "track-incremented": {0x8007BED0: 0x24840001},
            "callee-is-phase": {0x8007BECC: 0x0C033D23},
            "call-skipped": {0x8007BECC: 0x00000000},
        },
    )
)
