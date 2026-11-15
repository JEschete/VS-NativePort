"""F00240 func_8007BEE4 (BATTLE.PRG 146C.c:8613), typed Effects::ScriptedEffectPlaybackState."""

from scripted_effect_triggers_common import PHASE, configure

globals().update(
    configure(
        240,
        PHASE,
        0x8007BEE4,
        0x8007BF04,
        {
            "result-incremented": {0x8007BEF8: 0x24420001},
            "callee-is-ready": {0x8007BEEC: 0x0C033BAF},
            "result-cleared": {0x8007BEF8: 0x00001021},
        },
    )
)
