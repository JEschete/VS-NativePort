"""F02838 func_801080C8 (MENU9.PRG 57BC.c:49), typed Menu::Score::InitializeAshleyPreviewPose."""

from encyclopedia_preview_common import POSE, configure

globals().update(
    configure(
        2838,
        POSE,
        0x801080C8,
        0x8010812C,
        {
            "animation-34": {0x801080DC: 0x24030022},
            "weapon-drawn-kept": {0x801080CC: 0x3C06FFFF},
            "pose-offset-0x700": {0x801080E8: 0x24850700},
            "display-flags-one": {0x80108104: 0x24050001},
        },
    )
)
