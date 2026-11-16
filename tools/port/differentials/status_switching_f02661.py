"""F02661 MENU4.PRG _renderBasicStats (120.c:741), typed Menu::Status::DrawVitals."""

from status_switching_common import VITALS, configure

globals().update(
    configure(
        2661,
        VITALS,
        0x80103AC8,
        0x80103E58,
        {
            "hud-shown-skipped": {0x80103B74: 0x00000000},
            "hud-offset-any": {0x80103C1C: 0x00000000},
            "slide-limit-three": {0x80103BF0: 0x2C620003},
            "previous-not-drawn": {0x80103BBC: 0x00000000},
            "analyzed-bit-16": {0x80103C6C: 0x3C030001},
            "label-slide-shift": {0x80103D98: 0x00052C80},
            "risk-max-99": {0x80103E20: 0x24060063},
            "current-hp-from-mp": {0x80103CB4: 0x8666001C},
            "unknown-risk-with-max": {0x80103D54: 0x24050001},
        },
    )
)
