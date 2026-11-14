"""F02606 _sorcererMagicMenu reset path: original instructions vs native menu."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from magic_menus_common import configure  # noqa: E402

ID = "F02606"
globals().update(
    configure(2, 0x801037A8, 0x80103AEC, 0x801037DC, 0x24040000)
)
