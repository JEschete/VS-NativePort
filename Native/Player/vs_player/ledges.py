"""Running jumps onto ledges that ordinary walking and climbing cannot reach.

Rules mirror the original movement code (Actors/Movement.c) so plans stay within native behavior:
- Walking climbs at most 256 units; a sheathed, idle Ashley starts a jump with Square.
- A running jump from height h peaks roughly 135-160 units above h. Contacting a wall while her
  feet are within 256 units of its top produces the hang/climb result, so a ledge up to about
  384 units above the takeoff tile is reachable when she takes off close to the platform edge.
- Taking off too early meets the wall while falling (below the grab window); too late walks her
  off the platform. Native takeoff happens at the position observed when Square is pressed, and a
  straight run advances exactly RUN_STRIDE units per game frame from a standing start, so the
  reachable takeoff positions are fixed by the standing position modulo RUN_STRIDE. Execution
  shifts that phase with single diagonal steps (DIAGONAL_STEP on each axis) before running.
"""

import math
from dataclasses import dataclass

from .controller import InputAction, direction_buttons
from .crates import DIRECTIONS, TILE, CratePlanner, observed_crates
from .navigation import TerrainNavigator, field_navigation_ready
from .observations import Frame

CLIMB_UP = 256
MAX_RISE = 384
MAX_ATTEMPTS = 8
# Takeoff distances before the platform edge that grab the ledge (MAP020: 1012 of edge 1024 grabbed,
# 1007 and 1000 met the wall 2 units below the hang height).
PRESS_WINDOW = (2, 14)
# Observed straight run step per game frame and diagonal step per axis.
RUN_STRIDE = 18
DIAGONAL_STEP = 12
MAX_PHASE_STEPS = 6


@dataclass(frozen=True)
class LedgeJump:
    runup: tuple[int, int]
    takeoff: tuple[int, int]
    landing: tuple[int, int]
    direction: int
    takeoff_height: int
    landing_height: int

    def as_dict(self) -> dict:
        return {"runup": self.runup, "takeoff": self.takeoff, "landing": self.landing,
                "direction": self.direction, "takeoff_height": self.takeoff_height,
                "landing_height": self.landing_height}


def plan_ledge_jump(planner: CratePlanner, crates: tuple, start: tuple[int, int],
                    goal: tuple[int, int], failed: frozenset = frozenset()) -> LedgeJump | None:
    """Return the shortest-run-up running jump that makes the goal walkable, or None."""
    region = planner.region(start, crates)
    if goal in region:
        return None
    candidates = []
    for takeoff in region:
        height = planner.standable(*takeoff, crates)
        if height is None:
            continue
        for direction, (dx, dz) in DIRECTIONS.items():
            runup = (takeoff[0] - dx, takeoff[1] - dz)
            if runup not in region or planner.standable(*runup, crates) != height:
                continue
            for distance in (1, 2, 3):
                landing = (takeoff[0] + dx * distance, takeoff[1] + dz * distance)
                other = planner.standable(*landing, crates)
                if other is None:
                    continue
                rise = height - other
                # Intermediate tiles must not block the arc by rising above the takeoff platform.
                between = [planner.support(takeoff[0] + dx * step, takeoff[1] + dz * step, crates)
                           for step in range(1, distance)]
                if any(value is not None and value < height for value in between):
                    break
                if not CLIMB_UP < rise <= MAX_RISE:
                    continue
                if landing in region or (runup, direction) in failed:
                    break
                if goal not in planner.region(landing, crates):
                    break
                route = planner.path(start, runup, crates)
                candidates.append((len(route) if route else math.inf, distance, LedgeJump(
                    runup, takeoff, landing, direction, height, other)))
                break
    if not candidates:
        return None
    return min(candidates, key=lambda item: (item[0], item[1], item[2].runup, item[2].direction))[2]


class LedgeJumpAssist:
    """Execute a planned running jump with ordinary held direction and a timed Square press."""

    def __init__(self, goal: tuple[int, int], crates) -> None:
        self.goal = goal
        # Shares the observed room grid and tile walking with the crate assist.
        self.crates = crates
        self.navigator = TerrainNavigator()
        self.navigator.require_sheathed = True
        self.navigator.avoid_hostiles = True
        self.jump: LedgeJump | None = None
        self.stage = "idle"
        self.stage_tick = 0
        self.sheath_tick = -100
        self.press_tick: int | None = None
        self.attempts = 0
        self.total_attempts = 0
        self.phase_steps = 0
        self.phase_tick = -100
        self.grounded_along: float | None = None
        self.takeoff_along: float | None = None
        self.peak_height: int | None = None
        self.failed: set[tuple[tuple[int, int], int]] = set()
        self.memo: tuple | None = None
        self.feedback: list[dict] = []

    def active(self) -> bool:
        return self.stage != "idle"

    def _state(self, frame: Frame):
        planner = self.crates._planner(frame)
        if planner is None:
            return None, ()
        movable, _, _ = observed_crates(frame)
        return planner, tuple(sorted(movable, key=lambda item: item.slot))

    def needed(self, frame: Frame) -> bool:
        if self.active():
            return True
        if (frame.player is None or not field_navigation_ready(frame) or self.total_attempts >= 24
                or not frame.raw.get("navigation", {}).get("available")):
            return False
        planner, crates = self._state(frame)
        if planner is None:
            return False
        tile = self.crates.player_tile(frame)
        region = planner.region(tile, crates)
        key = (min(region) if region else tile, crates, frozenset(self.failed), self.crates.grid)
        if self.memo is not None and self.memo[0] == key:
            jump = self.memo[1]
        else:
            jump = plan_ledge_jump(planner, crates, tile, self.goal, key[2])
            self.memo = (key, jump)
            if jump is not None:
                self.feedback.append({"status": "ledge_plan", "tick": frame.tick, **jump.as_dict(),
                                      "player_tile": tile, "goal": self.goal})
        if jump is None:
            return False
        self.jump, self.attempts, self.phase_steps = jump, 0, 0
        self._begin(frame, "walk_runup")
        return True

    def _begin(self, frame: Frame, stage: str) -> None:
        self.stage, self.stage_tick = stage, frame.tick
        self.navigator.path.clear()

    def _abandon(self, frame: Frame, reason: str) -> None:
        if self.jump is not None:
            self.failed.add((self.jump.runup, self.jump.direction))
        self.feedback.append({"status": "ledge_jump_abandoned", "tick": frame.tick, "reason": reason,
                              "jump": self.jump.as_dict() if self.jump else None})
        self.jump, self.stage, self.memo = None, "idle", None
        self.navigator.path.clear()

    def _along(self, frame: Frame) -> float:
        dx, dz = DIRECTIONS[self.jump.direction]
        x, _, z = frame.player.position
        return x * dx + z * dz

    def _edge(self) -> float:
        dx, dz = DIRECTIONS[self.jump.direction]
        tx, tz = self.jump.takeoff
        # Far edge of the takeoff tile measured along the jump direction.
        return (tx * dx + tz * dz) * TILE + (TILE if dx + dz > 0 else 0)

    def _toward_landing(self, frame: Frame) -> frozenset[str]:
        # A pure axis direction keeps the run straight; any lateral component would turn it diagonal.
        dx, dz = DIRECTIONS[self.jump.direction]
        return direction_buttons(dx * TILE, dz * TILE, frame.camera_yaw)

    def _offsets(self, frame: Frame) -> tuple[float, float]:
        """Return (along, lateral) offsets of Ashley from the run-up tile centre."""
        dx, dz = DIRECTIONS[self.jump.direction]
        x, _, z = frame.player.position
        cx, cz = x - (self.jump.runup[0] * TILE + 64), z - (self.jump.runup[1] * TILE + 64)
        return cx * dx + cz * dz, cx * -dz + cz * dx

    def phase_ok(self, along: float) -> bool:
        """True when some whole number of run strides from along lands inside the press window."""
        low, high = self._edge() - PRESS_WINDOW[1], self._edge() - PRESS_WINDOW[0]
        return (high - along) % RUN_STRIDE <= high - low

    def in_press_window(self, along: float) -> bool:
        return self._edge() - PRESS_WINDOW[1] <= along <= self._edge() - PRESS_WINDOW[0]

    def step(self, frame: Frame) -> InputAction | None:
        self.crates.observe_grid(frame)
        if not self.active() or self.jump is None or frame.player is None or frame.camera_yaw is None:
            return None
        if frame.tick - self.stage_tick > 1800:
            self._abandon(frame, f"ledge stage {self.stage} exceeded 1800 ticks")
            return None
        jump = self.jump
        if self.stage == "walk_runup":
            if frame.control != 1 or frame.event_phase != 0:
                return None
            if self.crates.player_tile(frame) == jump.runup and field_navigation_ready(frame):
                self._begin(frame, "align")
                return None
            return self._walk(frame, jump.runup)
        if self.stage == "align":
            if not field_navigation_ready(frame):
                return None
            if frame.player.weapon_drawn:
                if frame.tick - self.sheath_tick <= 30:
                    return None
                self.sheath_tick = frame.tick
                return InputAction(frozenset({"Cross"}), 4, "sheathe normally before a running jump")
            along_offset, lateral = self._offsets(frame)
            if abs(lateral) > 20 or not -48 <= along_offset <= 48:
                if frame.tick - self.stage_tick > 240:
                    self._abandon(frame, "could not centre on the run-up tile")
                    return None
                x, _, z = frame.player.position
                cx, cz = jump.runup[0] * TILE + 64 - x, jump.runup[1] * TILE + 64 - z
                return InputAction(direction_buttons(cx, cz, frame.camera_yaw), 1,
                                   "centre on the planned run-up tile")
            along = self._along(frame)
            if not self.phase_ok(along) and self.phase_steps < MAX_PHASE_STEPS:
                # Let the previous step land before measuring the phase again.
                if frame.tick - self.phase_tick < 8:
                    return None
                self.phase_steps += 1
                self.phase_tick = frame.tick
                dx, dz = DIRECTIONS[jump.direction]
                side = -1 if lateral > 0 else 1
                buttons = direction_buttons((dx - dz * side) * TILE, (dz + dx * side) * TILE,
                                            frame.camera_yaw)
                self.feedback.append({"status": "ledge_phase_step", "tick": frame.tick,
                                      "along": along, "edge": self._edge(), "step": self.phase_steps})
                return InputAction(buttons, 1, "diagonal step to shift the running-jump stride phase")
            self.grounded_along = self.takeoff_along = None
            self.peak_height, self.press_tick = None, None
            self.feedback.append({"status": "ledge_run_start", "tick": frame.tick, "along": along,
                                  "edge": self._edge(), "phase_ok": self.phase_ok(along)})
            self._begin(frame, "run")
            return None
        if self.stage == "run":
            along = self._along(frame)
            y = frame.player.position[1]
            if self.press_tick is not None:
                self._begin(frame, "airborne")
                self.stage_tick = self.press_tick
                return self._airborne(frame)
            if (y > jump.takeoff_height + 32 or frame.tick - self.stage_tick > 180
                    or along > self._edge() - PRESS_WINDOW[0]):
                return self._attempt_failed(frame, "passed the takeoff window before the press")
            buttons = self._toward_landing(frame)
            if self.in_press_window(along) and self.crates.player_tile(frame) == jump.takeoff:
                self.press_tick = frame.tick
                self.feedback.append({"status": "ledge_jump_press", "tick": frame.tick,
                                      "along": along, "edge": self._edge(), "attempt": self.attempts + 1})
                return InputAction(buttons | {"Square"}, 2, "running jump toward the planned ledge")
            return InputAction(buttons, 1, "run straight toward the planned ledge takeoff edge")
        if self.stage == "airborne":
            return self._airborne(frame)
        return None

    def _airborne(self, frame: Frame) -> InputAction | None:
        jump = self.jump
        along = self._along(frame)
        y = frame.player.position[1]
        if self.takeoff_along is None:
            if y < jump.takeoff_height - 8:
                self.takeoff_along = self.grounded_along if self.grounded_along is not None else along
            else:
                self.grounded_along = along
        self.peak_height = y if self.peak_height is None else min(self.peak_height, y)
        settled = field_navigation_ready(frame) and frame.tick - self.stage_tick >= 40
        if settled and abs(y - jump.landing_height) <= 16:
            planner, crates = self._state(frame)
            tile = self.crates.player_tile(frame)
            if planner is not None and self.goal in planner.region(tile, crates):
                self.feedback.append({"status": "ledge_jump_done", "tick": frame.tick,
                                      "takeoff_along": self.takeoff_along, "edge": self._edge(),
                                      "attempt": self.attempts + 1, "player_tile": tile})
                self.total_attempts += 1
                self.jump, self.stage, self.memo = None, "idle", None
                self.navigator.path.clear()
                return None
        if settled and (y >= jump.takeoff_height - 8 or frame.tick - self.stage_tick > 150):
            return self._attempt_failed(frame, "landed below the planned ledge")
        if frame.tick - self.stage_tick > 240:
            return self._attempt_failed(frame, "jump did not settle")
        # Holding the direction through the hang lets the native climb finish onto the ledge.
        return InputAction(self._toward_landing(frame), 1, "hold toward the ledge through the jump and climb")

    def _attempt_failed(self, frame: Frame, reason: str) -> None:
        self.attempts += 1
        self.total_attempts += 1
        edge = self._edge()
        record = {"status": "ledge_jump_failed", "tick": frame.tick, "reason": reason,
                  "attempt": self.attempts, "takeoff_along": self.takeoff_along, "edge": edge,
                  "peak_height": self.peak_height, "player": frame.player.position}
        self.feedback.append(record)
        if self.attempts >= MAX_ATTEMPTS:
            self._abandon(frame, "running jump did not reach the ledge within the attempt limit")
            return None
        self.phase_steps = 0
        self._begin(frame, "walk_runup")
        return None

    def _walk(self, frame: Frame, target: tuple[int, int]) -> InputAction | None:
        if (self.crates.player_tile(frame) != target and not self.navigator.path
                and self.crates._route(frame, target, False) is None):
            self._abandon(frame, "observed terrain left no tile path to the run-up tile")
            return None
        return self.crates.walk_with(self.navigator, frame, target, carrying=False)

    def result(self) -> dict:
        return {"goal": self.goal, "attempts": self.total_attempts,
                "failed": sorted([list(runup), direction] for runup, direction in self.failed),
                "feedback": self.feedback[-12:]}
