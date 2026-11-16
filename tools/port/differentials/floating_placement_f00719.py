"""F00719 updater (func_800941FC) against its complete original instructions, with F00720 and F00721 followed."""

from differentials.floating_placement_common import UPDATE, configure

globals().update(
    configure(
        719,
        UPDATE,
        0x800941FC,
        0x80094844,
        {
            "stack-lift-eleven": {0x8009431C: 0x2442000B},
            "step-limit-twenty-one": {0x80094278: 0x28420016},
            "anchored-left-thirty-three": {0x80094424: 0x24020021},
            "shown-value-drop-nine": {0x80094500: 0x24420009},
            "waiting-value-drop-nine": {0x8009459C: 0x24420009},
            "settled-frame-eleven": {0x80094568: 0x2C63000B},
            "timing-end-forty-seven": {0x8009474C: 0x2C63002F},
            "equipment-origin-low": {0x8009479C: 0x240500AD},
            "ashley-suppression-ignored": {0x80094600: 0x00000000},
            "paused-ignored": {0x800947B4: 0x00000000},
            "no-compaction": {0x8009481C: 0x00000000},
        },
    )
)
