"""F02665 original effect navigation, byte cursor state and wrap-safe rotation."""

from status_limbs_common import configure

globals().update(
    configure(
        2665,
        {
            "empty-result-zero": {0x80104170: 0x24020000},
            "skip-last-flag": {0x801041C0: 0x2A02001F},
            "lose-target-store": {0x80104188: 0x00000000},
            "wrong-active-id": {0x801041B8: 0xA0400000},
            "top-element-ten": {0x80104204: 0x2402000A},
            "entry-selected-six": {0x8010423C: 0x24020006},
            "triangle-mask-circle": {0x80104254: 0x30620020},
            "cross-result-two": {0x80104270: 0x24020002},
            "rotate-circle": {0x80104280: 0x30420020},
            "digital-down-63": {0x801042D8: 0x2410003F},
            "yaw-subtract": {0x80104304: 0x00711823},
            "pitch-add": {0x80104308: 0x00902021},
            "pitch-cap-895": {0x80104330: 0x2402037F},
            "up-row-seven": {0x8010435C: 0x2610FFF9},
            "no-change-sound": {0x80104464: 0x00000000},
            "right-exit-element-five": {0x80104430: 0x24020005},
            "description-index-17": {0x80104490: 0x24420011},
            "cursor-x-three": {0x801044C8: 0x24420003},
            "cursor-color-two": {0x801044EC: 0x24020002},
            "wide-cursor-store": {0x801044E8: 0xA60280C4},
        },
    )
)
