import json
import shutil
import sqlite3
import time
import uuid
from dataclasses import dataclass
from pathlib import Path

from .config import PlayerPaths

RUN_MARKER = "run.json"


@dataclass(frozen=True)
class Retention:
    successful_runs: int = 10
    successful_days: int = 7
    run_bytes: int = 512 * 1024 * 1024
    total_log_bytes: int = 10 * 1024 * 1024 * 1024
    interval_seconds: int = 900

    def __post_init__(self) -> None:
        if min(self.successful_runs, self.successful_days, self.run_bytes,
               self.total_log_bytes, self.interval_seconds) <= 0:
            raise ValueError("Retention limits must be positive")


def owned_tree_size(path: Path) -> int:
    if path.is_symlink() or path.is_junction():
        raise ValueError(f"Refusing a linked run directory: {path}")
    total = 0
    for child in path.rglob("*"):
        if child.is_symlink() or child.is_junction():
            raise ValueError(f"Run directory contains a link: {child}")
        if child.is_file():
            total += child.stat().st_size
    return total


class RunStore:
    def __init__(self, paths: PlayerPaths) -> None:
        paths.initialize()
        self.paths = paths
        if paths.runs.resolve().parent != paths.root.resolve():
            raise ValueError("Run storage must remain inside the owned state directory")
        self.database = sqlite3.connect(paths.database)
        self.database.execute("PRAGMA journal_mode=WAL")
        self.database.executescript("""
            CREATE TABLE IF NOT EXISTS runs (
                id TEXT PRIMARY KEY, created REAL NOT NULL, ended REAL,
                status TEXT NOT NULL, pinned INTEGER NOT NULL, goal TEXT NOT NULL,
                summary TEXT
            );
            CREATE TABLE IF NOT EXISTS events (
                run_id TEXT NOT NULL, sequence INTEGER NOT NULL, time REAL NOT NULL,
                kind TEXT NOT NULL, payload TEXT NOT NULL,
                PRIMARY KEY (run_id, sequence)
            );
            CREATE TABLE IF NOT EXISTS outcomes (
                run_id TEXT NOT NULL, model INTEGER NOT NULL, weapon INTEGER NOT NULL,
                summary TEXT NOT NULL
            );
            CREATE TABLE IF NOT EXISTS navigation_evidence (
                run_id TEXT NOT NULL, sequence INTEGER NOT NULL,
                zone INTEGER NOT NULL, room INTEGER NOT NULL, door TEXT NOT NULL,
                geometry_sha256 TEXT NOT NULL, payload TEXT NOT NULL,
                PRIMARY KEY (run_id, sequence)
            );
        """)
        self.database.commit()

    def directory(self, run_id: str) -> Path:
        if len(run_id) != 32 or any(char not in "0123456789abcdef" for char in run_id):
            raise ValueError("Invalid owned run identity")
        path = self.paths.runs / run_id
        if path.is_symlink() or path.is_junction() or path.resolve().parent != self.paths.runs.resolve():
            raise ValueError("Run path is not a direct owned directory")
        return path

    def create(self, goal: str, metadata: dict) -> str:
        run_id = uuid.uuid4().hex
        directory = self.directory(run_id)
        directory.mkdir()
        created = time.time()
        manifest = {
            "owner": "VS.Player", "schema": 1, "id": run_id,
            "created": created, "status": "starting", "pinned": True,
            "goal": goal, "metadata": metadata,
        }
        (directory / RUN_MARKER).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        self.database.execute(
            "INSERT INTO runs(id,created,status,pinned,goal) VALUES (?,?,?,?,?)",
            (run_id, created, "starting", 1, goal),
        )
        self.database.commit()
        return run_id

    def event(self, run_id: str, kind: str, payload: dict) -> None:
        if self.database.execute("SELECT id FROM runs WHERE id=?", (run_id,)).fetchone() is None:
            raise ValueError("Cannot write an event for an unknown run")
        sequence = self.database.execute(
            "SELECT COALESCE(MAX(sequence),0)+1 FROM events WHERE run_id=?", (run_id,)
        ).fetchone()[0]
        self.database.execute(
            "INSERT INTO events VALUES (?,?,?,?,?)",
            (run_id, sequence, time.time(), kind, json.dumps(payload, separators=(",", ":"))),
        )
        if kind == "navigation.outcome":
            observation = payload["observation"]
            if observation["status"] not in ("movement_blocked", "route_reached"):
                self.database.rollback()
                raise ValueError("Only measured navigation outcomes become spatial evidence")
            self.database.execute(
                "INSERT INTO navigation_evidence VALUES (?,?,?,?,?,?,?)",
                (run_id, sequence, payload["zone"], payload["room"], payload["door"],
                 payload["geometry_sha256"], json.dumps(payload)),
            )
        self.database.commit()

    def finish(self, run_id: str, status: str, summary: dict) -> None:
        if status not in ("passed", "failed", "blocked", "cancelled"):
            raise ValueError("Run needs an explicit terminal outcome")
        path = self.directory(run_id) / RUN_MARKER
        manifest = json.loads(path.read_text(encoding="utf-8"))
        if manifest.get("owner") != "VS.Player" or manifest["id"] != run_id:
            raise ValueError("Run ownership marker does not match")
        pinned = status != "passed"
        ended = time.time()
        manifest.update(status=status, pinned=pinned, ended=ended, summary=summary)
        temporary = path.with_suffix(".pending")
        temporary.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        temporary.replace(path)
        self.database.execute(
            "UPDATE runs SET status=?,pinned=?,ended=?,summary=? WHERE id=?",
            (status, int(pinned), ended, json.dumps(summary), run_id),
        )
        self.database.commit()
        self.event(run_id, "run.finished", summary | {"status": status})

    def experience(self, model: int, weapon: int, limit: int = 4) -> list[dict]:
        return [json.loads(row[0]) for row in self.database.execute(
            "SELECT summary FROM outcomes WHERE model=? AND weapon=? ORDER BY rowid DESC LIMIT ?",
            (model, weapon, limit),
        )]

    def navigation_experience(self, zone: int, room: int, door: str,
                              geometry_sha256: str, limit: int = 4) -> list[dict]:
        return [json.loads(row[0]) for row in self.database.execute(
            "SELECT payload FROM navigation_evidence WHERE zone=? AND room=? "
            "AND door=? AND geometry_sha256=? ORDER BY rowid DESC LIMIT ?",
            (zone, room, door, geometry_sha256, limit),
        )]

    def pin(self, run_id: str) -> None:
        path = self.directory(run_id) / RUN_MARKER
        manifest = json.loads(path.read_text(encoding="utf-8"))
        row = self.database.execute("SELECT status FROM runs WHERE id=?", (run_id,)).fetchone()
        if row is None or manifest.get("owner") != "VS.Player" or manifest.get("id") != run_id:
            raise ValueError("Cannot pin an unknown or unowned evidence directory")
        if row[0] not in ("passed", "failed", "blocked", "cancelled"):
            raise ValueError("Pinning requires a completed run")
        manifest["pinned"] = True
        temporary = path.with_suffix(".pending")
        temporary.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        temporary.replace(path)
        self.database.execute("UPDATE runs SET pinned=1 WHERE id=?", (run_id,))
        self.database.commit()

    def remember(self, run_id: str, model: int, weapon: int, summary: dict) -> None:
        row = self.database.execute("SELECT status FROM runs WHERE id=?", (run_id,)).fetchone()
        if row is None or row[0] != "passed":
            raise ValueError("Only verified passed outcomes become reusable experience")
        self.database.execute("INSERT INTO outcomes VALUES (?,?,?,?)",
                              (run_id, model, weapon, json.dumps(summary)))
        self.database.commit()

    def cleanup(self, retention: Retention, now: float | None = None) -> dict:
        now = time.time() if now is None else now
        rows = self.database.execute(
            "SELECT id,created,status,pinned FROM runs ORDER BY created DESC"
        ).fetchall()
        sizes = {}
        for run_id, _, _, _ in rows:
            directory = self.directory(run_id)
            if directory.exists():
                sizes[run_id] = owned_tree_size(directory)
        total = sum(sizes.values())
        kept_successes = 0
        removable = []
        for run_id, created, status, pinned in rows:
            if status != "passed" or pinned or run_id not in sizes:
                continue
            kept_successes += 1
            expired = now - created > retention.successful_days * 86400
            if expired or kept_successes > retention.successful_runs:
                removable.append(run_id)
        if total > retention.total_log_bytes:
            for run_id, _, status, pinned in reversed(rows):
                if status == "passed" and not pinned and run_id in sizes and run_id not in removable:
                    removable.append(run_id)
                if total - sum(sizes[item] for item in removable) <= retention.total_log_bytes:
                    break
        deleted = []
        for run_id in removable:
            directory = self.directory(run_id)
            manifest = json.loads((directory / RUN_MARKER).read_text(encoding="utf-8"))
            if (manifest.get("owner") != "VS.Player" or manifest.get("id") != run_id
                    or manifest.get("pinned") is not False or manifest.get("status") != "passed"):
                raise ValueError(f"Cleanup refused an unowned, active or pinned run: {run_id}")
            owned_tree_size(directory)
            shutil.rmtree(directory)
            self.database.execute("DELETE FROM events WHERE run_id=?", (run_id,))
            self.database.commit()
            deleted.append(run_id)
            total -= sizes[run_id]
        return {
            "deleted": deleted, "remaining_bytes": total,
            "over_budget_pinned_evidence": total > retention.total_log_bytes,
        }

    def close(self) -> None:
        self.database.close()
