"""F02605 _shamanMagicMenu reset path: original instructions vs native menu."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from magic_menus_common import configure  # noqa: E402

ID = "F02605"
globals().update(
    configure(1, 0x80103464, 0x801037A8, 0x80103498, 0x24040000)
)
