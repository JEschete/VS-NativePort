"""F00834 break art policy including the actual original F00833 body."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from action_animations_common import configure  # noqa: E402

globals().update(configure(834))
