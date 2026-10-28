import heapq
import hashlib
import json
import math
from dataclasses import dataclass

from .controller import InputAction, StaleDecision, direction_buttons
from .model import NavigationDecision
from .observations import Frame, boolean, integer


def geometry_digest(value: dict) -> str:
    return hashlib.sha256(json.dumps(
        {key: value[key] for key in
         ("origin", "width", "depth", "spacing", "collision_radius", "collision_height", "samples")
         if key in value},
        sort_keys=True, separators=(",", ":"),
    ).encode()).hexdigest()


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
        self.feedback: list[dict] = []
        self.last_tick = -100
        self.completed_positions: dict[tuple[int, int], int] = {}
        self.best_waypoint_distance = math.inf
        self.geometry_sha256 = ""
        self.feedback_offset = 0
        self.sheath_sent_tick: int | None = None

    def request(self, frame: Frame, destination: tuple[int, int]) -> dict | None:
        if self.path or frame.player is None or frame.enemies or frame.control != 1:
            return None
        if frame.event_phase != 0 or not frame.ready:
            return None
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
                if not -256 <= height_delta <= 384:
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
                jump = height_delta < -64 or distance > 1
                next_cost = cost + terrain.spacing * distance + (
                    192 + terrain.spacing * (distance-1) if jump else 0
                )
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
        for index in ranked:
            remaining = math.hypot(terrain.point(index)[0]-destination[0],
                                   terrain.point(index)[2]-destination[1])
            if remaining > closest + terrain.spacing:
                continue
            route = []
            cursor = index
            while cursor != start:
                parent, jump = previous[cursor]
                route.append({"position": terrain.point(cursor), "jump": jump,
                              "takeoff": terrain.point(parent) if jump else None})
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
        if not self.offered:
            raise ValueError("Runtime geometry has no supported outgoing navigation edge")
        return {
            "destination_world_xz": destination,
            "player_world": position, "candidates": self.offered,
            "weapon_drawn": frame.player.weapon_drawn,
            "native_climbing_rule": "Actors/Movement.c rejects terrain climbs of two or more height bands "
                                    "while the weapon is drawn. The controller must sheathe normally first.",
            "recent_feedback": self.feedback[-6:],
            "limits": "Candidates lead toward the same closest reachable goal region. "
                      "Estimated cost includes the full route and remaining goal distance, "
                      "not only the truncated execution prefix. Prefer objective progress. "
                      "Routes are proposals, not native reachability guarantees.",
        }

    def accept(self, decision: NavigationDecision, frame: Frame) -> None:
        if ((frame.episode, frame.zone, frame.room) != self.scene or frame.enemies
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
        self.feedback.append({"status": "model_plan", "tick": frame.tick,
                              "choice": decision.choice, "reason": decision.reason,
                              "latency_ms": decision.latency_ms})

    def observations(self) -> list[dict]:
        rows = [row for row in self.feedback[self.feedback_offset:]
                if row["status"] in ("movement_blocked", "route_reached")]
        self.feedback_offset = len(self.feedback)
        return rows

    def step(self, frame: Frame) -> InputAction | None:
        if frame.enemies:
            self.path.clear()
            return None
        if not self.path or frame.player is None or frame.camera_yaw is None:
            return None
        if frame.control != 1 or frame.event_phase != 0 or frame.tick-self.last_tick < 6:
            return None
        position = frame.player.position
        needs_climb = any(step["jump"] and step.get("takeoff")
                          and step["position"][1] < step["takeoff"][1] - 64
                          for step in self.path)
        if needs_climb and frame.player.weapon_drawn:
            if self.sheath_sent_tick is not None:
                if frame.tick-self.sheath_sent_tick > 120:
                    raise RuntimeError("Normal sheathe input was not acknowledged before a native-gated climb")
                return None
            self.sheath_sent_tick = self.last_tick = frame.tick
            self.progress_tick = frame.tick
            return InputAction(frozenset({"Cross"}), 2, "sheathe normally before a native-gated terrain climb")
        target = self.path[0]
        x, y, z = target["position"]
        native = frame.raw["navigation"]
        airborne = native["jumping"] or native["movement_mode"] != 0
        arrival_radius = min(40, max(24, frame.player.run_speed * 2))
        approaching_takeoff = False
        if target["jump"] and not target.get("jump_sent") and target.get("takeoff"):
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
            self.failed_points.add((x, z))
            self.feedback.append({"status": "movement_blocked", "tick": frame.tick,
                                  "player": position, "waypoint": target,
                                  "weapon_drawn": frame.player.weapon_drawn})
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
