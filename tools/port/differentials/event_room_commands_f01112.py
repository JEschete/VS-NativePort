"""F01112 func_800B9770 (BATTLE.PRG 4A0A8.c:1966), typed Event::ScriptSetGimmickVisibility."""

from event_room_commands_common import GIMMICK, configure

globals().update(
    configure(
        1112,
        GIMMICK,
        0x800B9770,
        0x800B979C,
        {
            "visibility-reads-third": {0x800B9780: 0x90450003},
            "platform-signed": {0x800B977C: 0x80440001},
            "returns-hold": {0x800B9790: 0x24020001},
        },
    )
)
