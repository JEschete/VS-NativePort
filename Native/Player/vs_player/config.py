import json
import os
from dataclasses import dataclass
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
REPOSITORY = PROJECT.parents[1]
OWNER_MARKER = ".vs-player-owned.json"


@dataclass(frozen=True)
class PlayerPaths:
    root: Path

    @classmethod
    def default(cls) -> "PlayerPaths":
        local = os.environ.get("LOCALAPPDATA")
        if not local:
            raise RuntimeError("LOCALAPPDATA is unavailable; pass --state-dir explicitly")
        return cls(Path(local) / "VS-NativePort" / "Player")

    @property
    def runtime(self) -> Path:
        return self.root / "runtime"

    @property
    def models(self) -> Path:
        return self.root / "models"

    @property
    def runs(self) -> Path:
        return self.root / "runs"

    @property
    def database(self) -> Path:
        return self.root / "player.sqlite3"

    def initialize(self) -> None:
        if self.root.is_symlink() or self.root.is_junction():
            raise ValueError("Player state cannot be a linked directory")
        root = self.root.resolve()
        if root == Path(root.anchor) or root == Path.home().resolve():
            raise ValueError("Player state needs its own dedicated directory")
        marker = root / OWNER_MARKER
        if marker.is_symlink() or self.database.is_symlink():
            raise ValueError("Player ownership and history cannot be linked files")
        if root.exists():
            if not marker.is_file():
                raise ValueError(f"Refusing an existing unowned state directory: {root}")
            if json.loads(marker.read_text(encoding="utf-8")) != {
                "owner": "VS.Player", "schema": 1,
            }:
                raise ValueError(f"Invalid state-directory ownership marker: {marker}")
        else:
            root.mkdir(parents=True)
            marker.write_text(
                json.dumps({"owner": "VS.Player", "schema": 1}) + "\n",
                encoding="utf-8",
            )
        for path in (self.runtime, self.models, self.runs):
            if path.is_symlink() or path.is_junction() or path.resolve().parent != root:
                raise ValueError(f"Player storage cannot escape its owned directory: {path}")
            path.mkdir(exist_ok=True)


def runtime_manifest() -> dict:
    return json.loads((PROJECT / "runtime.json").read_text(encoding="utf-8"))
