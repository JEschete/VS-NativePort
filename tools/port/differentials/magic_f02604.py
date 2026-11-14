"""F02604 _warlockMagicMenu reset path: original instructions vs native menu."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from magic_menus_common import configure  # noqa: E402

ID = "F02604"
globals().update(
    configure(0, 0x80102B08, 0x80103464, 0x80102B3C, 0x24040001)
)
