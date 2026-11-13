"""F00818 defense policy, original instructions vs typed native code."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from action_animations_common import configure  # noqa: E402

globals().update(configure(818))
