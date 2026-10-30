"""Tile-level crate (gimmick) puzzle planning and physical execution.

Rules mirror the original interaction code so plans stay within native behavior:
- Field_FindInteractableGimmickAhead (BATTLE.PRG func_800A39F8, Gimmicks.c): Ashley idle and
  sheathed, the top crate on her front tile has interaction != 1 and its base lies within
  [feet - 128, feet + 64]. Types 0-3 need a far tile whose support is not above the crate base;
  types 4-6 need 384 units of ceiling clearance.
- Field_BeginGimmickInteraction (func_800A3E6C): types 0-3 slide one tile away; types 4-6 are
  lifted and later placed on the front tile when its support is at most 128 above her feet.
- Actors/Movement.c rejects climb/hang transitions (result >= 2) while actionPoseActive, so a
  carried crate limits walking to small steps.
"""

import math
import time
from collections import deque
from dataclasses import dataclass

from .controller import InputAction, direction_buttons
from .navigation import TerrainNavigator, field_navigation_ready, signed_height
from .observations import Frame

TILE = 128
CLIMB_UP = 256
MAX_DROP = 512
CARRY_STEP = 64
LIFT_CLEARANCE = 384
# Field_TileInFrontOfActor quadrant -> (dx, dz); matches the gimmick direction decode.
DIRECTIONS = {0: (0, -1), 1: (-1, 0), 2: (0, 1), 3: (1, 0)}
MAX_PLAN_ACTIONS = 4


@dataclass(frozen=True)
class Crate:
    slot: int
    kind: int
    x: int
    z: int
    y: int

    @property
    def liftable(self) -> bool:
        return self.kind >= 4


@dataclass(frozen=True)
class RoomGrid:
    width: int
    depth: int
    floor: tuple[int | None, ...]
    ceiling: tuple[int | None, ...]
    passable: tuple[bool, ...]

    @classmethod
    def decode(cls, value: dict) -> "RoomGrid":
        width, depth = int(value["width"]), int(value["depth"])
        count = width * depth
        if not (0 < width <= 256 and 0 < depth <= 256) or any(
            len(value[key]) != count for key in ("floor", "ceiling", "flags")
        ):
            raise ValueError("Room tile grid does not match its dimensions")
        return cls(
            width, depth,
            tuple(signed_height(packed) if packed >= 0 else None for packed in value["floor"]),
            tuple(signed_height(packed) if packed >= 0 else None for packed in value["ceiling"]),
            tuple(flags & 3 == 1 for flags in value["flags"]),
        )

    def inside(self, x: int, z: int) -> bool:
        return 0 <= x < self.width and 0 <= z < self.depth


def observed_crates(frame: Frame) -> tuple[list[Crate], list[Crate], int | None]:
    """Return (movable top-level crates, static support gimmicks, carried slot)."""
    movable, static = [], []
    carried = None
    for gimmick in frame.raw.get("gimmicks", []):
        if gimmick["carried"]:
            carried = gimmick["slot"]
            continue
        # NavigationSupportHeight / Field_GimmickSupportHeight support predicate.
        if gimmick["skipped"] or gimmick["motion_state"] or gimmick["mode"] == 0xFE:
            continue
        crate = Crate(gimmick["slot"], gimmick["object_id"], gimmick["tile_x"], gimmick["tile_z"],
                      gimmick["position"][1])
        if (gimmick["mode"] == 0 and gimmick["interaction"] != 1 and gimmick["visible"]
                and crate.kind <= 6):
            movable.append(crate)
        else:
            static.append(crate)
    return movable, static, carried


class CratePlanner:
    def __init__(self, grid: RoomGrid, static: list[Crate], body_height: int) -> None:
        self.grid = grid
        self.static = static
        self.body_height = body_height
        self._tops: dict[tuple[Crate, ...], dict[tuple[int, int], int]] = {}
        self._regions: dict[tuple, dict] = {}
        self._targets: dict[tuple[Crate, ...], list[tuple[int, int]]] = {}

    def tops(self, crates: tuple[Crate, ...]) -> dict[tuple[int, int], int]:
        cached = self._tops.get(crates)
        if cached is None:
            if len(self._tops) > 4096:
                self._tops.clear()
            cached = {}
            for crate in (*crates, *self.static):
                tile = (crate.x, crate.z)
                cached[tile] = min(cached.get(tile, crate.y - TILE), crate.y - TILE)
            self._tops[crates] = cached
        return cached

    def support(self, x: int, z: int, crates: tuple[Crate, ...]) -> int | None:
        if not self.grid.inside(x, z):
            return None
        index = z * self.grid.width + x
        floor = self.grid.floor[index]
        if floor is None or not self.grid.passable[index]:
            return None
        top = self.tops(crates).get((x, z))
        return floor if top is None else min(floor, top)

    def standable(self, x: int, z: int, crates: tuple[Crate, ...]) -> int | None:
        height = self.support(x, z, crates)
        if height is None:
            return None
        ceiling = self.grid.ceiling[z * self.grid.width + x]
        if ceiling is not None and height - ceiling < self.body_height:
            return None
        return height

    def clear_above(self, x: int, z: int, feet: int) -> bool:
        ceiling = self.grid.ceiling[z * self.grid.width + x]
        return ceiling is None or feet - LIFT_CLEARANCE >= ceiling

    def region(self, start: tuple[int, int], crates: tuple[Crate, ...], carrying: bool = False
               ) -> dict[tuple[int, int], tuple[int, int] | None]:
        key = (start, crates, carrying)
        cached = self._regions.get(key)
        if cached is not None:
            return cached
        if len(self._regions) > 2048:
            self._regions.clear()
        previous: dict[tuple[int, int], tuple[int, int] | None] = {}
        if self.standable(*start, crates) is not None:
            previous[start] = None
            queue = deque([start])
            while queue:
                tile = queue.popleft()
                height = self.standable(*tile, crates)
                for dx, dz in DIRECTIONS.values():
                    neighbor = (tile[0] + dx, tile[1] + dz)
                    if neighbor in previous:
                        continue
                    other = self.standable(*neighbor, crates)
                    if other is None or height is None:
                        continue
                    up = height - other
                    if carrying and abs(up) > CARRY_STEP:
                        continue
                    if up > CLIMB_UP or -up > MAX_DROP:
                        continue
                    previous[neighbor] = tile
                    queue.append(neighbor)
        self._regions[key] = previous
        return previous

    def top(self, tile: tuple[int, int], crates: tuple[Crate, ...]) -> Crate | None:
        stacked = [crate for crate in crates if (crate.x, crate.z) == tile]
        return min(stacked, key=lambda crate: crate.y) if stacked else None

    def _interactable(self, player: tuple[int, int], direction: int, crates: tuple[Crate, ...]
                      ) -> tuple[Crate, tuple[int, int]] | None:
        feet = self.standable(*player, crates)
        dx, dz = DIRECTIONS[direction]
        front = (player[0] + dx, player[1] + dz)
        crate = self.top(front, crates)
        if feet is None or crate is None or not (feet - TILE <= crate.y <= feet + 64):
            return None
        if any(static.x == front[0] and static.z == front[1] and static.y < crate.y
               for static in self.static):
            return None
        return crate, front

    def push_result(self, player: tuple[int, int], direction: int, crates: tuple[Crate, ...]
                    ) -> tuple[Crate, ...] | None:
        found = self._interactable(player, direction, crates)
        if found is None or found[0].liftable:
            return None
        crate, front = found
        dx, dz = DIRECTIONS[direction]
        far = (front[0] + dx, front[1] + dz)
        rest = tuple(other for other in crates if other != crate)
        landing = self.support(*far, rest)
        if landing is None or landing < crate.y:
            return None
        return tuple(sorted((*rest, Crate(crate.slot, crate.kind, *far, landing)),
                            key=lambda item: item.slot))

    def lift_result(self, player: tuple[int, int], direction: int, crates: tuple[Crate, ...]
                    ) -> tuple[Crate, tuple[Crate, ...]] | None:
        found = self._interactable(player, direction, crates)
        if found is None or not found[0].liftable:
            return None
        crate, front = found
        feet = self.standable(*player, crates)
        if feet is None or not self.clear_above(*player, feet) or not self.clear_above(*front, feet):
            return None
        return crate, tuple(other for other in crates if other != crate)

    def place_result(self, player: tuple[int, int], direction: int, carried: Crate,
                     crates: tuple[Crate, ...]) -> tuple[Crate, ...] | None:
        feet = self.standable(*player, crates)
        dx, dz = DIRECTIONS[direction]
        front = (player[0] + dx, player[1] + dz)
        landing = self.support(*front, crates)
        if (feet is None or landing is None or landing < feet - TILE
                or not self.clear_above(*player, feet) or not self.clear_above(*front, feet)):
            return None
        return tuple(sorted((*crates, Crate(carried.slot, carried.kind, *front, landing)),
                            key=lambda item: item.slot))

    def placement_targets(self, crates: tuple[Crate, ...]) -> list[tuple[int, int]]:
        """Tiles where a placed crate can change reachability: below a ledge too high to climb,
        or onto another crate to build a taller step."""
        cached = self._targets.get(crates)
        if cached is not None:
            return cached
        targets = set(self.tops(crates))
        for z in range(self.grid.depth):
            for x in range(self.grid.width):
                height = self.support(x, z, crates)
                if height is None:
                    continue
                for dx, dz in DIRECTIONS.values():
                    other = self.standable(x + dx, z + dz, crates)
                    if other is not None and height - other > CLIMB_UP:
                        targets.add((x, z))
                        break
        if len(self._targets) > 1024:
            self._targets.clear()
        self._targets[crates] = sorted(targets)
        return self._targets[crates]

    def plan(self, player: tuple[int, int], crates: tuple[Crate, ...], goal: tuple[int, int],
             failed: frozenset = frozenset(), budget: float = 2.0) -> list[dict] | None:
        """Breadth-first search over crate actions until the goal tile is walkable.

        Returns [] when the goal is already reachable, or None when no plan exists within the
        action and time limits. Each action names the stand tile, facing quadrant and crate slot.
        """
        deadline = time.monotonic() + budget
        crates = tuple(sorted(crates, key=lambda item: item.slot))
        start_region = self.region(player, crates)
        if goal in start_region:
            return []
        seen = {(min(start_region) if start_region else player, crates)}
        queue = deque([(player, crates, [])])
        while queue:
            position, state, actions = queue.popleft()
            if len(actions) >= MAX_PLAN_ACTIONS:
                continue
            region = self.region(position, state)
            tops = {(crate.x, crate.z): self.top((crate.x, crate.z), state) for crate in state}
            successors = []
            for (tx, tz), crate in sorted(tops.items()):
                for direction, (dx, dz) in DIRECTIONS.items():
                    if time.monotonic() > deadline:
                        return None
                    stand = (tx - dx, tz - dz)
                    if stand not in region or (stand, direction) in failed:
                        continue
                    pushed = self.push_result(stand, direction, state)
                    if pushed is not None:
                        successors.append((stand, pushed, {"action": "push", "stand": stand,
                                                            "direction": direction,
                                                            "slot": crate.slot}))
                    lifted = self.lift_result(stand, direction, state)
                    if lifted is None:
                        continue
                    carried, remaining = lifted
                    carry_region = self.region(stand, remaining, carrying=True)
                    for target in self.placement_targets(remaining):
                        for place_direction, (px, pz) in DIRECTIONS.items():
                            drop = (target[0] - px, target[1] - pz)
                            if drop not in carry_region or (drop, place_direction) in failed:
                                continue
                            placed = self.place_result(drop, place_direction, carried, remaining)
                            if placed is None or placed == state:
                                continue
                            successors.append((drop, placed, {
                                "action": "lift", "stand": stand, "direction": direction,
                                "slot": carried.slot, "place_stand": drop,
                                "place_direction": place_direction,
                            }))
            for next_position, next_state, action in successors:
                next_region = self.region(next_position, next_state)
                plan = [*actions, action]
                if goal in next_region:
                    return plan
                key = (min(next_region) if next_region else next_position, next_state)
                if key in seen:
                    continue
                seen.add(key)
                queue.append((next_position, next_state, plan))
        return None

    def path(self, start: tuple[int, int], goal: tuple[int, int], crates: tuple[Crate, ...],
             carrying: bool = False) -> list[tuple[int, int]] | None:
        previous = self.region(start, crates, carrying)
        if goal not in previous:
            return None
        route = [goal]
        while previous[route[-1]] is not None:
            route.append(previous[route[-1]])
        route.reverse()
        return route


class CrateAssist:
    """Execute a crate plan with ordinary movement, facing taps and Cross presses."""

    def __init__(self, goal: tuple[int, int]) -> None:
        self.goal = goal
        self.grid: RoomGrid | None = None
        self.grid_scope: tuple[int, int] | None = None
        self.navigator = TerrainNavigator()
        self.navigator.require_sheathed = True
        self.navigator.avoid_hostiles = True
        self.plan: list[dict] | None = None
        self.stage = "idle"
        self.stage_tick = 0
        self.press_tick = -100
        self.last_input_tick = -100
        self.turns = 0
        self.failed: set[tuple[tuple[int, int], int]] = set()
        self.actions_done = 0
        self.feedback: list[dict] = []
        self.memo: tuple | None = None

    def observe_grid(self, frame: Frame) -> None:
        scope = (frame.zone, frame.room)
        if scope != self.grid_scope:
            self.grid, self.grid_scope = None, scope
            self.plan, self.stage = None, "idle"
        raw = frame.raw.get("room_tiles")
        if raw is not None:
            self.grid = RoomGrid.decode(raw)

    def _planner(self, frame: Frame) -> CratePlanner | None:
        if self.grid is None or "gimmicks" not in frame.raw:
            return None
        _, static, _ = observed_crates(frame)
        return CratePlanner(self.grid, static, frame.raw["navigation"]["collision_height"])

    @staticmethod
    def player_tile(frame: Frame) -> tuple[int, int]:
        x, _, z = frame.player.position
        return x // TILE, z // TILE

    def active(self) -> bool:
        return self.stage != "idle"

    def needed(self, frame: Frame) -> bool:
        """Plan when the goal tile is not walkable; True while a crate action is pending."""
        self.observe_grid(frame)
        if self.active():
            return True
        if (frame.player is None or not field_navigation_ready(frame)
                or not frame.raw.get("navigation", {}).get("available")):
            return False
        planner = self._planner(frame)
        if planner is None or self.actions_done >= 12:
            return False
        movable, _, carried = observed_crates(frame)
        if carried is not None:
            return False
        crates = tuple(sorted(movable, key=lambda item: item.slot))
        tile = self.player_tile(frame)
        region = planner.region(tile, crates)
        # Moving within one reachable region cannot change the answer, so search once per
        # region/crate configuration instead of stalling the control loop every frame.
        key = (min(region) if region else tile, crates, frozenset(self.failed), self.grid)
        if self.memo is not None and self.memo[0] == key:
            plan = self.memo[1]
        else:
            plan = planner.plan(tile, crates, self.goal, key[2], budget=1.0)
            self.memo = (key, plan)
            if plan is None:
                self.feedback.append({"status": "crate_plan_unavailable", "tick": frame.tick,
                                      "player_tile": tile, "goal": self.goal,
                                      "movable_crates": len(crates)})
        if not plan:
            return False
        self.plan = plan
        self.feedback.append({"status": "crate_plan", "tick": frame.tick, "plan": plan,
                              "player_tile": self.player_tile(frame), "goal": self.goal})
        self._begin(frame, "walk_stand")
        return True

    def _begin(self, frame: Frame, stage: str) -> None:
        self.stage, self.stage_tick, self.turns = stage, frame.tick, 0
        self.navigator.path.clear()

    def _abandon(self, frame: Frame, reason: str) -> None:
        action = self.plan[0] if self.plan else {}
        placing = self.stage in ("walk_place", "place", "settle_place")
        stand = action.get("place_stand" if placing else "stand")
        direction = action.get("place_direction" if placing else "direction")
        if stand is not None and direction is not None:
            self.failed.add((tuple(stand), direction))
        self.feedback.append({"status": "crate_action_failed", "tick": frame.tick, "stage": self.stage,
                              "reason": reason, "action": action})
        self.plan, self.stage = None, "idle"
        self.memo = None
        self.navigator.path.clear()

    def _walk(self, frame: Frame, target: tuple[int, int], carrying: bool) -> InputAction | None:
        if self.player_tile(frame) == target:
            self.navigator.path.clear()
            return None
        if not self.navigator.path:
            planner = self._planner(frame)
            movable, _, _ = observed_crates(frame)
            route = planner.path(self.player_tile(frame), target, tuple(movable), carrying) if planner else None
            if route is None:
                self._abandon(frame, "observed crates left no tile path to the stand tile")
                return None
            heights = [planner.standable(*tile, tuple(movable)) for tile in route]
            waypoints = []
            for previous, tile, height, before in zip(route, route[1:], heights[1:], heights):
                up = before - height
                waypoints.append({
                    "position": (tile[0] * TILE + 64, height, tile[1] * TILE + 64),
                    "jump": up > 64,
                    "takeoff": (previous[0] * TILE + 64, before, previous[1] * TILE + 64) if up > 64 else None,
                    "drop": max(0, height - before),
                })
            self.navigator.path = waypoints
            self.navigator.scene = (frame.episode, frame.zone, frame.room)
            self.navigator.destination = (target[0] * TILE + 64, target[1] * TILE + 64)
            self.navigator.target_identity = None
            self.navigator.progress_tick = frame.tick
            self.navigator.best_waypoint_distance = math.inf
            self.navigator.sheath_sent_tick = None
            self.navigator.sheath_attempts = 0
        return self.navigator.step(frame)

    def _face_and_press(self, frame: Frame, stand: tuple[int, int], direction: int,
                        reason: str) -> InputAction | None:
        dx, dz = DIRECTIONS[direction]
        front = [stand[0] + dx, stand[1] + dz]
        native = frame.raw["navigation"]
        if frame.tick - self.last_input_tick < 6:
            return None
        if native.get("facing_tile", [])[:2] != front:
            self.turns += 1
            if self.turns > 24:
                self._abandon(frame, "could not face the crate from its stand tile")
                return None
            self.last_input_tick = frame.tick
            x, _, z = frame.player.position
            return InputAction(direction_buttons(front[0] * TILE + 64 - x, front[1] * TILE + 64 - z,
                                                 frame.camera_yaw), 2,
                               "turn toward the observed crate from its planned stand tile")
        # Field_FindInteractableGimmickAhead requires an idle, unrouted actor.
        if native["movement_mode"] or native["locomotion_state"] or native["route_phase"]:
            return None
        if frame.player.weapon_drawn:
            self.last_input_tick = frame.tick
            return InputAction(frozenset({"Cross"}), 4, "sheathe normally before a crate interaction")
        self.last_input_tick = self.press_tick = frame.tick
        return InputAction(frozenset({"Cross"}), 2, reason)

    def step(self, frame: Frame) -> InputAction | None:
        self.observe_grid(frame)
        if not self.active() or frame.player is None or frame.camera_yaw is None:
            return None
        if frame.control != 1 or frame.event_phase != 0:
            return None
        if frame.tick - self.stage_tick > 1800:
            self._abandon(frame, f"crate stage {self.stage} exceeded 1800 ticks")
            return None
        action = self.plan[0]
        gimmicks = {gimmick["slot"]: gimmick for gimmick in frame.raw.get("gimmicks", [])}
        crate = gimmicks.get(action["slot"])
        if crate is None:
            self._abandon(frame, "planned crate slot disappeared")
            return None
        stand = tuple(action["stand"])
        if self.stage == "walk_stand":
            if self.player_tile(frame) == stand and field_navigation_ready(frame):
                self._begin(frame, "interact")
                return None
            return self._walk(frame, stand, carrying=False)
        if self.stage == "interact":
            if self.press_tick >= self.stage_tick and (crate["mode"] != 0 or crate["carried"]
                                                       or (crate["tile_x"], crate["tile_z"]) != (
                                                           stand[0] + DIRECTIONS[action["direction"]][0],
                                                           stand[1] + DIRECTIONS[action["direction"]][1])):
                self.feedback.append({"status": "crate_interaction_started", "tick": frame.tick,
                                      "action": action})
                self._begin(frame, "settle")
                return None
            if self.press_tick >= self.stage_tick and frame.tick - self.press_tick < 60:
                return None
            if self.press_tick >= self.stage_tick and frame.tick - self.stage_tick > 240:
                self._abandon(frame, "native crate interaction did not start after Cross")
                return None
            return self._face_and_press(frame, stand, action["direction"],
                                        "physically interact with the planned crate")
        if self.stage == "settle":
            if crate["mode"] != 0 or crate["motion_state"] or not field_navigation_ready(frame):
                return None
            if frame.tick - self.stage_tick < 20:
                return None
            if action["action"] == "lift":
                if not crate["carried"]:
                    self._abandon(frame, "lift did not leave the crate carried")
                    return None
                self._begin(frame, "walk_place")
                return None
            self._complete(frame, crate)
            return None
        place_stand = tuple(action.get("place_stand", stand))
        if self.stage == "walk_place":
            if self.player_tile(frame) == place_stand and field_navigation_ready(frame):
                self._begin(frame, "place")
                return None
            return self._walk(frame, place_stand, carrying=True)
        if self.stage == "place":
            if not crate["carried"]:
                self._begin(frame, "settle_place")
                return None
            if self.press_tick >= self.stage_tick and frame.tick - self.press_tick < 60:
                return None
            if self.press_tick >= self.stage_tick and frame.tick - self.stage_tick > 240:
                self._abandon(frame, "native placement did not start after Cross")
                return None
            return self._face_and_press(frame, place_stand, action["place_direction"],
                                        "physically place the carried crate on the planned tile")
        if self.stage == "settle_place":
            if crate["mode"] != 0 or crate["motion_state"] or not field_navigation_ready(frame):
                return None
            if frame.tick - self.stage_tick < 20:
                return None
            self._complete(frame, crate)
        return None

    def _complete(self, frame: Frame, crate: dict) -> None:
        self.actions_done += 1
        self.feedback.append({"status": "crate_action_done", "tick": frame.tick, "action": self.plan[0],
                              "crate_tile": (crate["tile_x"], crate["tile_z"]),
                              "crate_y": crate["position"][1]})
        # Replan from the observed result rather than trusting the remaining predicted actions.
        self.plan, self.stage = None, "idle"
        self.navigator.path.clear()

    def result(self) -> dict:
        return {"goal": self.goal, "actions_done": self.actions_done,
                "failed": sorted([list(stand), direction] for stand, direction in self.failed),
                "feedback": self.feedback[-12:]}
