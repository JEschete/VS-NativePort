"""Game_Menu_06 globals: see menu_globals.py for the rules and menu_globals_manual.py for hand-read rows."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from menu_globals import MenuGlobals  # noqa: E402
from menu_globals_manual import MANUAL  # noqa: E402

if __name__ == "__main__":
    MenuGlobals("Game_Menu_06", MANUAL).run().write(partial=False)
