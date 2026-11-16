"""F01414 func_800CD158: framebuffer backdrop shading and header timing."""

from text_box_frames_common import BACKDROP, configure

globals().update(configure(1414, BACKDROP, 0x800CD158, 0x800CD3A0, {
    "signed-line-count": {0x800CD1E0: 0x82C30021},
    "signed-open-duration": {0x800CD344: 0x82C20022},
    "wrong-header-offset": {0x800CD354: 0x2644FFFA},
}))
