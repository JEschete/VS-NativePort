"""F02666 MENU4.PRG _getLimbCount (120.c:1114), typed Menu::Status::LimbCount."""

from status_switching_common import LIMBS, configure

globals().update(
    configure(
        2666,
        LIMBS,
        0x80104514,
        0x8010455C,
        {
            "five-limbs": {0x80104548: 0x28620005},
            "limb-stride": {0x80104550: 0x248400D8},
            "unnamed-counted": {0x8010453C: 0x00000000},
        },
    )
)
