import json
import time
from pathlib import Path

from .observations import Frame


class ObservationFreshness:
    def __init__(self, maximum_age: float) -> None:
        self.maximum_age = maximum_age
        self.stale_since: float | None = None

    def check_timeout(self, now: float) -> None:
        if self.stale_since is not None and now - self.stale_since >= 2.0:
            raise TimeoutError("Native observations did not recover within2 seconds")

    def observe(self, age: float, now: float, ready: bool) -> bool:
        if not ready or age <= self.maximum_age:
            self.stale_since = None
            return True
        if self.stale_since is None:
            self.stale_since = now
        self.check_timeout(now)
        return False


class TraceReader:
    def __init__(self, path: Path, maximum_record_bytes: int = 1024 * 1024) -> None:
        self.path = path
        self.stream = path.open("rb")
        self.pending = b""
        self.maximum_record_bytes = maximum_record_bytes
        self.last_sequence = 0
        self.last_tick = -1
        self.origin: float | None = None
        self.last_frame_received = time.monotonic()
        self.engine_errors: list[dict] = []
        self.ended = False
        self.at_eof = False
        self.host_exit: dict | None = None

    def read(self) -> list[Frame]:
        chunk = self.stream.read(1024 * 1024)
        self.at_eof = not chunk
        if not chunk:
            return []
        self.pending += chunk
        lines = self.pending.split(b"\n")
        self.pending = lines.pop()
        if len(self.pending) > self.maximum_record_bytes:
            raise ValueError("Native trace record exceeded its size bound")
        frames = []
        for line in lines:
            if not line or len(line) > self.maximum_record_bytes:
                raise ValueError("Native trace has an empty or oversized record")
            record = json.loads(line)
            sequence = record["sequence"]
            if type(sequence) is not int or sequence <= self.last_sequence:
                raise ValueError("Native trace sequence is not strictly increasing")
            self.last_sequence = sequence
            category, data = record["category"], record["data"]
            if category == "engine":
                # Core::LogLevel is serialized as an integer, not a string.
                level = data["level"]
                if type(level) is not int:
                    raise ValueError("Native engine record has an invalid severity")
                if level >= 4:
                    self.engine_errors.append(record)
            elif category == "session.end":
                if data["trace_failed"]:
                    raise RuntimeError("The native trace reports incomplete diagnostics")
                self.ended = "script" not in record
            elif category == "host.exit":
                self.host_exit = data
            elif category == "frame":
                frame = Frame.decode(record)
                if frame.tick <= self.last_tick:
                    raise ValueError("Native frame ticks are not strictly increasing")
                self.last_tick = frame.tick
                now = time.monotonic()
                estimate = now - frame.elapsed_us / 1_000_000
                self.origin = estimate if self.origin is None else min(self.origin, estimate)
                self.last_frame_received = now
                frames.append(frame)
        return frames

    def age(self, frame: Frame) -> float:
        if self.origin is None:
            raise RuntimeError("Trace clock has not been initialized")
        return max(0.0, time.monotonic() - self.origin - frame.elapsed_us / 1_000_000)

    def close(self) -> None:
        self.stream.close()


def find_trace(game_directory: Path) -> Path | None:
    candidates = []
    for relative in ("game-logs", str(Path("combat-lab") / "runs")):
        parent = game_directory / relative
        if parent.exists():
            candidates.extend(parent.glob("*/events.jsonl"))
    if not candidates:
        return None
    if len(candidates) != 1:
        raise ValueError("Managed game directory contains more than one trace session")
    return candidates[0]
