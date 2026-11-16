"""F01415 func_800CD3A0: defined packed coordinate reflection."""

from text_box_frames_common import REFLECT, configure

globals().update(configure(1415, REFLECT, 0x800CD3A0, 0x800CD3E4, {
    "no-vertical-flip": {0x800CD3A0: 0x30A20000},
    "wrong-horizontal-axis": {0x800CD3C4: 0x30A20004},
}))
