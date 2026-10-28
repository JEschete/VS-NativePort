"""Run the standalone player without installing it into the game environment."""

from vs_player.cli import entrypoint

if __name__ == "__main__":
    raise SystemExit(entrypoint())
