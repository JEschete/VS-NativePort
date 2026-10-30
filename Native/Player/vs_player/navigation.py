import heapq
import hashlib
import json
import math
from dataclasses import dataclass

from .controller import InputAction, StaleDecision, direction_buttons
from .model import NavigationDecision
from .observations import Actor, Frame, boolean, integer


def geometry_digest(value: dict) -> str:
    return hashlib.sha256(json.dumps(
        {key: value[key] for key in
         ("origin", "width", "depth", "spacing", "collision_radius", "collision_height", "samples")
         if key in value},
        sort_keys=True, separators=(",", ":"),
    ).encode()).hexdigest()


def field_navigation_ready(frame: Frame) -> bool:
    return (frame.ready and frame.player is not None and not frame.game_over
            and frame.control == 1 and frame.event_phase == 0 and not frame.reaction.active)


def signed_height(packed: int) -> int:
    value = packed & 0x7FFF
    return value - 0x8000 if value & 0x4000 else value


@dataclass(frozen=True)
class Terrain:
    origin: tuple[int, int]
    width: int
    depth: int
    spacing: int
    body_height: int
    samples: tuple[tuple[int, int, int], ...]

    @classmethod
    def decode(cls, value: dict) -> "Terrain":
        if value.get("available") is not True:
            raise ValueError("Runtime terrain observations are unavailable")
        origin = value["origin"]
        if not isinstance(origin, list) or len(origin) != 2:
            raise ValueError("Terrain origin requires two world coordinates")
        width = integer(value["width"], "terrain.width", 1, 13)
        depth = integer(value["depth"], "terrain.depth", 1, 13)
        spacing = integer(value["spacing"], "terrain.spacing", 64, 64)
        integer(value["movement_mode"], "terrain.movement_mode", 0, 255)
        integer(value["locomotion_state"], "terrain.locomotion_state", 0, 255)
        integer(value["route_phase"], "terrain.route_phase", 0, 255)
        boolean(value["jumping"], "terrain.jumping")
        raw = value["samples"]
        if not isinstance(raw, list) or len(raw) != width * depth:
            raise ValueError("Terrain sample count does not match the observed patch")
        samples = []
        for sample in raw:
            if not isinstance(sample, list) or len(sample) != 3:
                raise ValueError("Terrain sample needs packed floor, ceiling and flags")
            samples.append((
                integer(sample[0], "terrain.floor", -32768, 32767),
                integer(sample[1], "terrain.ceiling", -32768, 32767),
                integer(sample[2], "terrain.flags", 0, 7),
            ))
        return cls(
            tuple(integer(axis, "terrain.origin", 0, 4096) for axis in origin),
            width, depth, spacing,
            integer(value["collision_height"], "terrain.body_height", 0, 4096),
            tuple(samples),
        )

    def point(self, index: int) -> tuple[int, int, int]:
        return (self.origin[0] + index % self.width * self.spacing,
                signed_height(self.samples[index][0]),
                self.origin[1] + index // self.width * self.spacing)

    def supported(self, index: int) -> bool:
        floor, ceiling, flags = self.samples[index]
        return (
            floor >= 0 and flags == 5
            and (ceiling < 0 or signed_height(floor) - signed_height(ceiling) >= self.body_height)
        )


class TerrainNavigator:
    def __init__(self) -> None:
        self.offered: dict[str, dict] = {}
        self.scene: tuple[int, int, int] | None = None
        self.request_tick = 0
        self.path: list[dict] = []
        self.destination: tuple[int, int] | None = None
        self.progress_position: tuple[int, int, int] | None = None
        self.progress_tick = 0
        self.failed_points: set[tuple[int, int]] = set()
        self.failed_walk_descents: set[tuple[int, int]] = set()
        self.feedback: list[dict] = []
        self.last_tick = -100
        self.completed_positions: dict[tuple[int, int], int] = {}
        self.best_waypoint_distance = math.inf
        self.geometry_sha256 = ""
        self.feedback_offset = 0
        self.sheath_sent_tick: int | None = None
        self.sheath_attempts = 0
        # Scene goals cross original triggers whose condition 9 rejects the battle stance (Triggers.c case 9).
        self.require_sheathed = False
        self.target_identity: tuple[int, int, int] | None = None
        self.direct_goal: tuple[int, int, int] | None = None
        self.direct_best_distance = math.inf
        self.direct_attempt_ticks = 0
        self.avoid_hostiles = False

    def ordinary_approach_stalled(self, frame: Frame, target: Actor, duration: int) -> bool:
        if frame.player is None:
            raise ValueError("Ordinary approach lacks the observed player")
        if self.direct_goal is None or sum(
            (target.position[axis]-self.direct_goal[axis])**2 for axis in (0, 1, 2)
        ) > 128**2:
            self.direct_goal = target.position
            self.direct_best_distance = math.dist(frame.player.position, self.direct_goal)
        distance = math.dist(frame.player.position, self.direct_goal)
        if distance < self.direct_best_distance - 16:
            self.direct_best_distance, self.direct_attempt_ticks = distance, 0
        else:
            self.direct_attempt_ticks += duration
        if self.direct_attempt_ticks >= 90:
            if self.direct_attempt_ticks - duration < 90:
                self.feedback.append({
                    "status": "ordinary_approach_blocked", "tick": frame.tick,
                    "player": frame.player.position,
                    "waypoint": {"position": target.position, "mode": "ordinary_approach"},
                    "weapon_drawn": frame.player.weapon_drawn,
                })
            return True
        return False

    def target(self, frame: Frame) -> Actor | None:
        return next((actor for actor in frame.enemies
                     if (actor.slot, actor.model, actor.spawn_index) == self.target_identity), None)

    def request(self, frame: Frame, destination: tuple[int, int],
                target_slot: int | None = None, avoid_hostiles: bool = False,
                explore_frontier: bool = False) -> dict | None:
        if self.path or not field_navigation_ready(frame):
            return None
        if frame.enemies and target_slot is None and not avoid_hostiles:
            return None
        if frame.player is None:
            raise ValueError("Field navigation lost the observed player")
        target = next((actor for actor in frame.enemies if actor.slot == target_slot), None)
        if target_slot is not None and target is None:
            raise StaleDecision("Navigation target is no longer an observed living hostile")
        self.target_identity = (target.slot, target.model, target.spawn_index) if target else None
        self.avoid_hostiles = avoid_hostiles
        self.scene = (frame.episode, frame.zone, frame.room)
        self.destination = destination
        self.request_tick = frame.tick
        terrain = Terrain.decode(frame.raw["navigation"])
        self.geometry_sha256 = geometry_digest(frame.raw["navigation"])
        valid = [index for index in range(len(terrain.samples)) if terrain.supported(index)
                 and (terrain.point(index)[0], terrain.point(index)[2]) not in self.failed_points]
        if not valid:
            raise ValueError("No supported runtime terrain samples remain near the player")
        position = frame.player.position
        start = min(valid, key=lambda index: sum(
            (terrain.point(index)[axis] - position[axis])**2 for axis in (0, 1, 2)
        ))
        costs, previous = {start: 0.0}, {}
        frontier = [(0.0, start)]
        valid_set = set(valid)
        while frontier:
            cost, index = heapq.heappop(frontier)
            if cost != costs[index]:
                continue
            for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1),
                           (2, 0), (-2, 0), (0, 2), (0, -2),
                           (3, 0), (-3, 0), (0, 3), (0, -3)):
                x, z = index % terrain.width + dx, index // terrain.width + dz
                neighbor = z * terrain.width + x
                if not (0 <= x < terrain.width and 0 <= z < terrain.depth and neighbor in valid_set):
                    continue
                height_delta = terrain.point(neighbor)[1] - terrain.point(index)[1]
                if height_delta < -256:
                    continue
                distance = abs(dx) + abs(dz)
                if distance > 1:
                    midpoints = [
                        (index // terrain.width + dz // distance * step) * terrain.width
                        + index % terrain.width + dx // distance * step
                        for step in range(1, distance)
                    ]
                    if any(terrain.samples[mid][0] < 0 or terrain.samples[mid][2] & 3 != 1
                           for mid in midpoints):
                        continue
                    if all(terrain.supported(mid) for mid in midpoints) and abs(height_delta) <= 64:
                        continue
                point = terrain.point(neighbor)
                jump = (height_delta < -64 or (distance > 1 and height_delta <= 64)
                        or (height_delta > 64 and (point[0], point[2]) in self.failed_walk_descents))
                next_cost = cost + terrain.spacing * distance + (
                    192 + terrain.spacing * (distance-1) if jump else 0
                ) + max(0, height_delta-64)
                if next_cost < costs.get(neighbor, math.inf):
                    costs[neighbor] = next_cost
                    previous[neighbor] = (index, jump)
                    heapq.heappush(frontier, (next_cost, neighbor))
        ranked = sorted((index for index in costs if index != start), key=lambda index: (
            math.hypot(terrain.point(index)[0] - destination[0],
                       terrain.point(index)[2] - destination[1]) * 2 + costs[index]
        ))
        self.offered = {}
        closest = min((math.hypot(terrain.point(index)[0]-destination[0],
                                   terrain.point(index)[2]-destination[1])
                       for index in ranked), default=math.inf)
        first_steps: set[tuple[int, int]] = set()
        for frontier_pass in ((False, True) if explore_frontier else (False,)):
            for index in ranked:
                remaining = math.hypot(terrain.point(index)[0]-destination[0],
                                       terrain.point(index)[2]-destination[1])
                if frontier_pass:
                    if (index % terrain.width not in (0, terrain.width - 1)
                            and index // terrain.width not in (0, terrain.depth - 1)):
                        continue
                elif remaining > closest + terrain.spacing:
                    continue
                route = []
                cursor = index
                while cursor != start:
                    parent, jump = previous[cursor]
                    route.append({"position": terrain.point(cursor), "jump": jump,
                                  "takeoff": terrain.point(parent) if jump else None,
                                  "drop": max(0, terrain.point(cursor)[1]-terrain.point(parent)[1])})
                    cursor = parent
                route.reverse()
                first = (route[0]["position"][0], route[0]["position"][2])
                if first in first_steps:
                    continue
                route = route[:6]
                last = route[-1]["position"]
                if self.completed_positions.get((last[0], last[2]), 0) >= 2:
                    continue
                first_steps.add(first)
                key = f"route{len(self.offered) + 1}"
                self.offered[key] = {
                    "waypoints": route, "estimated_cost": round(costs[index] + remaining),
                    "remaining_distance": round(math.hypot(last[0]-destination[0], last[2]-destination[1])),
                }
                if len(self.offered) == 3:
                    break
            if self.offered:
                break
        if not self.offered:
            raise ValueError("Runtime geometry has no supported outgoing navigation edge")
        context = {
            "destination_world_xz": destination,
            "player_world": position, "candidates": self.offered,
            "weapon_drawn": frame.player.weapon_drawn,
            "player_hp": frame.player.hp,
            "player_max_hp": frame.player.max_hp,
            "falling_risk": "Descending edges use ordinary movement and native fall consequences. "
                            "Prefer smaller drops, especially at low HP; never assume immunity.",
            "target_identity": self.target_identity,
            "target_world": target.position if target else None,
            "living_hostiles": [{"slot": actor.slot, "position": actor.position, "hp": actor.hp}
                                for actor in frame.enemies],
            "native_climbing_rule": "Actors/Movement.c rejects steep terrain transitions while the weapon "
                                    "is drawn, including large descents. Sheathe normally first.",
            "recent_feedback": self.feedback[-6:],
            "limits": "Candidates lead toward the same closest reachable goal region. "
                      "Estimated cost includes the full route and remaining goal distance, "
                      "not only the truncated execution prefix. Prefer objective progress. "
                      "Routes are proposals, not native reachability guarantees.",
        }
        if frontier_pass:
            context["frontier_exploration"] = True
            context["limits"] = (
                "Nearest-progress endpoints were already visited; these supported patch-boundary "
                "routes explore around the obstacle and may initially move away from the goal. "
                "Estimated cost includes the full route and remaining goal distance, not just its "
                "execution prefix. Routes are proposals, not native reachability guarantees."
            )
        return context

    def accept(self, decision: NavigationDecision, frame: Frame) -> None:
        if ((frame.episode, frame.zone, frame.room) != self.scene
                or (frame.enemies and self.target_identity is None and not self.avoid_hostiles)
                or (self.target_identity is not None and self.target(frame) is None)
                or decision.observation_tick != self.request_tick
                or frame.tick-decision.observation_tick > 600
                or geometry_digest(frame.raw.get("navigation", {})) != self.geometry_sha256):
            raise StaleDecision("Navigation observation changed during inference")
        if decision.choice not in self.offered:
            raise StaleDecision("Navigation choice was not in the observed candidate set")
        self.path = [dict(step) for step in self.offered[decision.choice]["waypoints"]]
        self.progress_position = frame.player.position if frame.player else None
        self.progress_tick = frame.tick
        self.best_waypoint_distance = math.inf
        self.sheath_sent_tick = None
        self.sheath_attempts = 0
        self.feedback.append({"status": "model_plan", "tick": frame.tick,
                              "choice": decision.choice, "reason": decision.reason,
                              "latency_ms": decision.latency_ms})

    def observations(self) -> list[dict]:
        rows = [row for row in self.feedback[self.feedback_offset:]
                if row["status"] in ("movement_blocked", "route_reached")]
        self.feedback_offset = len(self.feedback)
        return rows

    def step(self, frame: Frame) -> InputAction | None:
        if ((frame.enemies and self.target_identity is None and not self.avoid_hostiles)
                or (self.target_identity is not None and self.target(frame) is None)
                or (frame.episode, frame.zone, frame.room) != self.scene):
            self.path.clear()
            return None
        target_actor = self.target(frame)
        if target_actor is not None and self.destination is not None and sum(
            (target_actor.position[axis]-self.destination[index])**2
            for index, axis in enumerate((0, 2))
        ) > 128**2:
            self.path.clear()
            self.feedback.append({"status": "target_moved", "tick": frame.tick,
                                  "target": target_actor.slot, "position": target_actor.position})
            return None
        if not self.path or frame.player is None or frame.camera_yaw is None:
            return None
        if not field_navigation_ready(frame) or frame.tick-self.last_tick < 6:
            return None
        position = frame.player.position
        needs_sheathe = self.require_sheathed or any(
            (step["jump"] and step.get("takeoff")
             and step["position"][1] < step["takeoff"][1] - 64)
            or step.get("drop", 0) >= 128 for step in self.path
        )
        if needs_sheathe and frame.player.weapon_drawn:
            if self.sheath_sent_tick is not None and frame.tick-self.sheath_sent_tick <= 30:
                return None
            # Accelerated play can drop a short press before native polling, so resend a bounded number of times.
            self.sheath_attempts += 1
            if self.sheath_attempts > 4:
                raise RuntimeError("Normal sheathe input was not acknowledged before a native-gated terrain transition")
            self.sheath_sent_tick = self.last_tick = frame.tick
            self.progress_tick = frame.tick
            return InputAction(frozenset({"Cross"}), 4, "sheathe normally before a native-gated terrain transition")
        target = self.path[0]
        x, y, z = target["position"]
        native = frame.raw["navigation"]
        airborne = native["jumping"] or native["movement_mode"] != 0
        arrival_radius = min(40, max(24, frame.player.run_speed * 2))
        approaching_takeoff = False
        # A native climb or fall already under way must not be steered back to the takeoff point;
        # reversing the direction mid-climb cancels it and repeats the same climb indefinitely.
        if (target["jump"] and not target.get("jump_sent") and target.get("takeoff")
                and not airborne and not native["locomotion_state"]):
            takeoff = target["takeoff"]
            if sum((takeoff[i]-position[i])**2 for i in (0, 2)) > arrival_radius**2:
                x, y, z = takeoff
                approaching_takeoff = True
        dx, dz = x-position[0], z-position[2]
        if (not approaching_takeoff and not airborne
                and dx*dx+dz*dz <= arrival_radius**2 and abs(y-position[1]) <= 64):
            self.path.pop(0)
            self.progress_position, self.progress_tick = position, frame.tick
            self.best_waypoint_distance = math.inf
            if not self.path:
                self.direct_attempt_ticks = 0
                self.direct_goal = None
                self.direct_best_distance = math.inf
                endpoint = (x, z)
                self.completed_positions[endpoint] = self.completed_positions.get(endpoint, 0) + 1
                self.feedback.append({
                    "status": "route_reached", "tick": frame.tick, "player": position,
                    "endpoint": endpoint, "visits": self.completed_positions[endpoint],
                    "remaining_distance": round(math.hypot(
                        position[0]-self.destination[0], position[2]-self.destination[1],
                    )) if self.destination is not None else None,
                })
            return None
        distance = math.hypot(dx, dz) + abs(y-position[1])
        if distance <= self.best_waypoint_distance - 8:
            self.best_waypoint_distance = distance
            self.progress_position, self.progress_tick = position, frame.tick
        if frame.tick-self.progress_tick > 120:
            retry_jump = target.get("drop", 0) > 64 and not target["jump"]
            if retry_jump:
                self.failed_walk_descents.add((x, z))
            else:
                self.failed_points.add((x, z))
            self.feedback.append({"status": "movement_blocked", "tick": frame.tick,
                                  "player": position, "waypoint": target,
                                  "weapon_drawn": frame.player.weapon_drawn,
                                  "retry_mode": "directed_jump" if retry_jump else None})
            self.path.clear()
            if len(self.failed_points) >= 8:
                raise RuntimeError("Eight observed terrain waypoints failed; new geometry evidence is required")
            return None
        buttons = direction_buttons(dx, dz, frame.camera_yaw)
        if target["jump"] and not approaching_takeoff and not target.get("jump_sent"):
            if native["route_phase"]:
                return None
            if airborne or native["locomotion_state"]:
                self.last_tick = frame.tick
                return InputAction(buttons, 8, "maintain normal direction through native climb movement")
            if frame.player.actor_flags & 0x80000:
                raise ValueError("Native flags suppress the planned field jump")
            buttons |= {"Square"}
            target["jump_sent"] = True
            self.best_waypoint_distance = math.inf
            self.progress_tick = frame.tick
        self.last_tick = frame.tick
        # Shorten the final lease so a fast run step cannot overshoot the waypoint.
        duration = min(4, max(1, int(math.hypot(dx, dz) / max(1, frame.player.run_speed))))
        if airborne:
            duration = 8
        return InputAction(buttons, duration, "execute model-selected observed-terrain waypoint")


# Field_TileInFrontOfActor (Interactions.c:466): quadrant 0 is -Z, 1 is -X, 2 is +Z, 3 is +X.
FACING_QUADRANT_DELTA = ((0, -1), (-1, 0), (0, 1), (1, 0))


class SceneTileGoal:
    def __init__(self, tile: tuple[int, int], zone: int, room: int, selector: int,
                 examine_quadrant: int | None = None, source: int = 2) -> None:
        if (not isinstance(tile, tuple) or len(tile) != 2
                or any(type(value) is not int or not 0 <= value <= 255 for value in tile)
                or any(type(value) is not int or value < 0 for value in (zone, room, selector))
                or source not in (0, 2)
                or (examine_quadrant is not None
                    and (type(examine_quadrant) is not int or not 0 <= examine_quadrant <= 3))):
            raise ValueError("A local-scene target requires native room/selector IDs and a byte-sized tile")
        self.tile = tile
        # Native event source of the triggered script: 2 room-local, 0 external EVENT/*.EVT.
        self.source = source
        # A type-4 trigger is examined with ordinary Cross from this stand tile while facing the quadrant;
        # room-entry scripts can relocate Ashley after the fixture's setup pulses, so she walks back.
        self.examine_quadrant = examine_quadrant
        self.examine_ticks: list[int] = []
        self.examine_turn_tick = -100
        self.zone, self.room, self.selector = zone, room, selector
        self.navigator = TerrainNavigator()
        self.navigator.require_sheathed = True
        self.activation_tick: int | None = None
        self.last_owned_tick = -1
        # Trigger thresholds can fail the conservative footprint test yet admit native movement.
        self.push_until: int | None = None
        self.push_attempts = 0
        self.push_sheath_tick = -100
        # A trigger tile that is also an authored door opens with ordinary Cross (GameLoop.c door path).
        self.door_open_ticks: list[int] = []
        # Imported here: crates builds on TerrainNavigator from this module.
        from .crates import CrateAssist
        from .ledges import LedgeJumpAssist
        self.crates = CrateAssist(tile)
        self.ledges = LedgeJumpAssist(tile, self.crates)
        self.identity = f"scene:{zone}:{room}:local:{selector}:tile:{tile[0]}:{tile[1]}" + (
            f":examine:{examine_quadrant}" if examine_quadrant is not None else "")

    def observe(self, frame: Frame) -> None:
        # The host exports room_tiles only periodically; keep it from any frame, not just the latest.
        self.crates.observe_grid(frame)
        scene = frame.raw["scene"]
        event = scene.get("event", {}) if scene["available"] else {}
        if not field_navigation_ready(frame) or frame.dialogue_waiting:
            self.navigator.path.clear()
        # Only scene ownership (room, script or dialogue) invalidates a pending route; transient
        # combat control changes such as hit reactions leave the observed terrain plan valid.
        if ((frame.zone, frame.room) != (self.zone, self.room)
                or frame.event_phase != 0 or frame.dialogue_waiting):
            self.last_owned_tick = frame.tick
        if (self.activation_tick is None and (frame.zone, frame.room) == (self.zone, self.room)
                and event.get("phase") == 4 and event.get("source") == self.source
                and event.get("id") == self.selector):
            self.activation_tick = frame.tick

    def ready(self, frame: Frame) -> bool:
        return (self.activation_tick is None and (frame.zone, frame.room) == (self.zone, self.room)
                and frame.raw["scene"]["available"] and field_navigation_ready(frame)
                and frame.raw.get("navigation", {}).get("available", False)
                and not frame.dialogue_waiting)

    def pending(self, frame: Frame) -> bool:
        return (self.activation_tick is None and (frame.zone, frame.room) == (self.zone, self.room)
                and frame.ready and frame.raw["scene"]["available"]
                and frame.event_phase == 0 and frame.control in (1, 2))

    def request(self, frame: Frame, avoid_hostiles: bool = False) -> dict | None:
        self.observe(frame)
        if self.push_until is not None and frame.tick < self.push_until:
            return None
        if (not self.ready(frame) or self.navigator.path
                or not frame.raw.get("navigation", {}).get("available")):
            return None
        if frame.player is None:
            raise ValueError("Scene navigation lost the observed player")
        # Crates own movement while the tile grid shows the trigger or examine stand needs them moved.
        if self.crates.needed(frame):
            return None
        # Ledges above walking climb height are reached with an original running jump.
        if self.ledges.needed(frame):
            return None
        x, _, z = frame.player.position
        destination = (self.tile[0] * 128 + 64, self.tile[1] * 128 + 64)
        if self.examine_quadrant is not None and self.on_tile(frame):
            return None
        staging = self.examine_quadrant is None and (x // 128, z // 128) == self.tile
        last = self.navigator.feedback[-1] if self.navigator.feedback else {}
        door = not staging and self.door_tile(frame)
        # A closed door cell never yields route_reached, so its push starts from any adjacent stand.
        if (not staging and (last.get("status") == "route_reached" or door) and self.push_attempts < 3
                and math.hypot(destination[0] - x, destination[1] - z) <= 224):
            self.push_attempts += 1
            self.push_until = frame.tick + (240 if door else 90)
            self.navigator.feedback.append({"status": "direct_tile_push", "tick": frame.tick,
                                            "player": frame.player.position, "tile": self.tile,
                                            "attempt": self.push_attempts})
            return None
        if staging:
            terrain = Terrain.decode(frame.raw["navigation"])
            candidates = []
            for index in range(len(terrain.samples)):
                if not terrain.supported(index):
                    continue
                px, _, pz = terrain.point(index)
                if abs(px // 128 - self.tile[0]) + abs(pz // 128 - self.tile[1]) != 1:
                    continue
                clearance = max(self.tile[0] * 128 - px, px - (self.tile[0] + 1) * 128,
                                self.tile[1] * 128 - pz, pz - (self.tile[1] + 1) * 128)
                if clearance >= 64:
                    candidates.append((px, pz))
            if not candidates:
                raise ValueError("No observed adjacent support can earn the scene's required tile change")
            destination = min(candidates, key=lambda point: math.hypot(point[0] - x, point[1] - z))
        elif door and self.crates.grid is not None:
            grid = self.crates.grid
            stands = [(self.tile[0] + dx, self.tile[1] + dz) for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1))]
            stands = [t for t in stands if grid.inside(*t) and grid.passable[t[1] * grid.width + t[0]]]
            if stands:
                stand = min(stands, key=lambda t: math.hypot(t[0] * 128 + 64 - x, t[1] * 128 + 64 - z))
                destination = (stand[0] * 128 + 64, stand[1] * 128 + 64)
        context = self.navigator.request(frame, destination, avoid_hostiles=avoid_hostiles,
                                         explore_frontier=True)
        if context is not None:
            context["scene_trigger_objective"] = {
                **self.result(), "staging_tile_change": staging,
                "requires_actual_native_activation": True,
            }
        return context

    def accept(self, decision: NavigationDecision, frame: Frame) -> None:
        self.observe(frame)
        if (self.activation_tick is not None or (frame.zone, frame.room) != (self.zone, self.room)
                or self.last_owned_tick >= self.navigator.request_tick):
            raise StaleDecision("Native scene/control ownership changed during trigger navigation")
        self.navigator.accept(decision, frame)

    def pushing(self, frame: Frame) -> bool:
        return ((self.push_until is not None and frame.tick < self.push_until) or self.crates.active()
                or self.ledges.active() or self.examining(frame))

    def on_tile(self, frame: Frame) -> bool:
        if frame.player is None:
            return False
        x, _, z = frame.player.position
        return (x // 128, z // 128) == self.tile

    def examining(self, frame: Frame) -> bool:
        return (self.examine_quadrant is not None and self.activation_tick is None
                and len(self.examine_ticks) < 6 and self.on_tile(frame))

    def examine_step(self, frame: Frame) -> InputAction | None:
        if frame.camera_yaw is None or frame.player is None:
            return None
        if frame.player.weapon_drawn:
            if frame.tick - self.push_sheath_tick <= 30:
                return None
            self.push_sheath_tick = frame.tick
            return InputAction(frozenset({"Cross"}), 4, "sheathe normally before examining the original trigger")
        dx, dz = FACING_QUADRANT_DELTA[self.examine_quadrant]
        face = (self.tile[0] + dx, self.tile[1] + dz)
        x, _, z = frame.player.position
        if frame.raw["navigation"].get("facing_tile", [])[:2] != list(face):
            if frame.tick - self.examine_turn_tick < 8:
                return None
            self.examine_turn_tick = frame.tick
            cx, cz = self.tile[0] * 128 + 64 - x, self.tile[1] * 128 + 64 - z
            # Posted key messages shorter than one host frame are lost at speed 4, so turning taps
            # last 4 ticks; centring first keeps that short step inside the stand tile.
            if abs(cx) > 24 or abs(cz) > 24:
                return InputAction(direction_buttons(cx, cz, frame.camera_yaw), 4,
                                   "centre on the original examine stand tile")
            return InputAction(direction_buttons(face[0] * 128 + 64 - x, face[1] * 128 + 64 - z,
                                                 frame.camera_yaw), 4,
                               "turn toward the original examine quadrant")
        if self.examine_ticks and frame.tick - self.examine_ticks[-1] < 60:
            return None
        self.examine_ticks.append(frame.tick)
        self.navigator.feedback.append({"status": "examine_press", "tick": frame.tick,
                                        "tile": self.tile, "facing": face})
        return InputAction(frozenset({"Cross"}), 4, "examine the original type-4 trigger with ordinary Cross")

    def door_tile(self, frame: Frame) -> bool:
        if any((door["tile_x"], door["tile_z"]) == self.tile for door in frame.raw.get("doors", [])):
            return True
        # Scripted exits sit on MPD lock-trigger cells whose passability 5 blocks collision
        # until Cross opens them (Field_InteractWithLockedRoomObject, Interactions.c:125).
        grid = self.crates.grid
        if grid is None or not grid.inside(*self.tile):
            return False
        index = self.tile[1] * grid.width + self.tile[0]
        return not grid.passable[index] and grid.floor[index] is not None

    def step(self, frame: Frame) -> InputAction | None:
        self.observe(frame)
        if self.crates.active():
            return self.crates.step(frame) if self.activation_tick is None else None
        if self.ledges.active():
            return self.ledges.step(frame) if self.activation_tick is None else None
        if not self.ready(frame):
            return None
        if self.examining(frame):
            self.push_until = None
            return self.examine_step(frame)
        if self.pushing(frame) and frame.player is not None:
            if frame.camera_yaw is None:
                return None
            if frame.player.weapon_drawn:
                if frame.tick - self.push_sheath_tick <= 30:
                    return None
                self.push_sheath_tick = frame.tick
                return InputAction(frozenset({"Cross"}), 4, "sheathe normally before the original trigger tile")
            x, _, z = frame.player.position
            dx = self.tile[0] * 128 + 64 - x
            dz = self.tile[1] * 128 + 64 - z
            facing = frame.raw["navigation"].get("facing_tile", [])[:2] == list(self.tile)
            if (facing and self.door_tile(frame) and len(self.door_open_ticks) < 3
                    and (not self.door_open_ticks or frame.tick - self.door_open_ticks[-1] >= 90)):
                self.door_open_ticks.append(frame.tick)
                self.navigator.feedback.append({"status": "door_open_press", "tick": frame.tick,
                                                "tile": self.tile})
                return InputAction(frozenset({"Cross"}), 2, "physically open the authored door on the trigger tile")
            return InputAction(direction_buttons(dx, dz, frame.camera_yaw), 4,
                               "walk directly into the observed original trigger tile")
        return self.navigator.step(frame)

    def result(self) -> dict:
        return {"zone": self.zone, "room": self.room, "source": 2,
                "selector": self.selector, "tile": self.tile,
                "activation_tick": self.activation_tick,
                "examine_quadrant": self.examine_quadrant,
                "examine_presses": list(self.examine_ticks),
                "crates": self.crates.result(), "ledges": self.ledges.result()}
