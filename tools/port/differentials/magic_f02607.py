"""F02607 _enchanterMagicMenu reset path: original instructions vs native menu."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from magic_menus_common import configure  # noqa: E402

ID = "F02607"
globals().update(
    configure(3, 0x80103AEC, 0x80103E30, 0x80103B20, 0x24040000)
)
