"""F00069 func_8006D064 (BATTLE.PRG 146C.c:2217), typed Effects::RestoreActionActorTint."""

from action_callbacks_common import RESTORE, configure

globals().update(
    configure(
        69,
        RESTORE,
        0x8006D064,
        0x8006D0A4,
        {
            "restore-mode-9": {0x8006D070: 0x24040009},
            "duration-8": {0x8006D074: 0x24050008},
            "actor-zero": {0x8006D078: 0x00003021},
            "tinted-kept": {0x8006D094: 0x00000000},
        },
    )
)
