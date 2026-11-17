"""F02606 _sorcererMagicMenu: original full state machine and row contract."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from magic_menus_common import configure  # noqa: E402

globals().update(configure(2))
