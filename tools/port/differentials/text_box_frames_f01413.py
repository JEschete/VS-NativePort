"""F01413 func_800CD0FC: information header artwork selection."""

from text_box_frames_common import HEADER, configure

globals().update(configure(1413, HEADER, 0x800CD0FC, 0x800CD158, {
    "omit-third-artwork": {0x800CD100: 0x2CA20002},
    "shift-header-x": {0x800CD12C: 0x34A50009},
}))
