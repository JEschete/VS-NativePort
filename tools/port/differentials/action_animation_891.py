"""F00891 turn, actual F00819 and cached-owner completion against R3000."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from action_animations_common import configure  # noqa: E402

globals().update(configure(891))
