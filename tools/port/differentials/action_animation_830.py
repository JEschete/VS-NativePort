"""F00830 target-hit reaction parity and source-impact native correction."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from action_animations_common import configure  # noqa: E402

globals().update(configure(830))
