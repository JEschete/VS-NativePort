"""F04148 _loadMapBackground (MENU5.PRG 4D8.c:206), typed Menu::map::LoadBackground."""

from map_loading_common import BACKGROUND, configure

globals().update(
    configure(
        4148,
        BACKGROUND,
        0x80102FCC,
        0x801030F4,
        {
            "file-sector": {0x80102FE4: 0x34634C61},
            "image-size": {0x80103004: 0x34843001},
            "read-not-stored": {0x80103010: 0x00000000},
            "step-not-reset": {0x80103034: 0x00000000},
            "loaded-state-three": {0x8010306C: 0x24020003},
            "clut-row-two": {0x80103080: 0x24050002},
            "image-offset": {0x801030AC: 0x24A50100},
            "brightness": {0x801030B8: 0x24050040},
            "step-not-advanced": {0x801030C4: 0x00000000},
            "image-not-freed": {0x801030CC: 0x00000000},
            "freed-result-zero": {0x801030D8: 0x24020000},
        },
    )
)
