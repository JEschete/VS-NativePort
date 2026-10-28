import re

from .controller import InputAction, direction_buttons
from .observations import Frame


class PhysicalDoor:
    def __init__(self, identity: str) -> None:
        match = re.fullmatch(r"z(\d{3})-r(\d{3})-d(\d{3})", identity)
        if match is None:
            raise ValueError("Physical door needs its original zNNN-rNNN-dNNN identity")
        self.identity = identity
        self.source_zone, self.source_room, self.index = map(int, match.groups())
        self.contract: dict | None = None
        self.loading_seen = False
        self.open_ticks: list[int] = []
        self.arrival_tick: int | None = None
        self.quiet_tick: int | None = None
        self.accepted_tick: int | None = None
        self.blocked_reason: str | None = None
        self.last_input_tick = -100
        self.interaction_seen = False
        self.progress_position: tuple[int, int, int] | None = None
        self.progress_tick = 0
        self.jump_attempts: list[dict] = []
        self.axis_attempts: list[dict] = []
        self.recovery_axis: int | None = None
        self.axis_until = 0

    def observe(self, frame: Frame) -> None:
        if self.contract is None:
            if not frame.ready:
                return
            if (frame.zone, frame.room) != (self.source_zone, self.source_room):
                raise ValueError("Physical-door scenario does not start in its original source room")
            matches = [door for door in frame.raw["doors"] if door["index"] == self.index]
            if len(matches) != 1:
                raise ValueError("Requested original door record is absent from the live room")
            self.contract = dict(matches[0])
            if self.contract["current_flag_value"] < self.contract["required_value"]:
                self.blocked_reason = "Original story flag does not satisfy this physical door"
            for other in frame.raw["doors"]:
                if (other["index"] < self.index and
                    (other["tile_x"], other["tile_z"], other["layer"]) ==
                    (self.contract["tile_x"], self.contract["tile_z"], self.contract["layer"])
                    and other["current_flag_value"] >= other["required_value"]):
                    self.blocked_reason = "Earlier eligible co-located original record takes precedence"
                    break
        if self.open_ticks:
            self.loading_seen |= not frame.ready
            self.interaction_seen |= frame.ready and frame.control == 11
        destination = (self.contract["destination_zone"], self.contract["destination_room"])
        if frame.ready and (frame.zone, frame.room) == destination and self.loading_seen:
            if self.arrival_tick is None:
                self.arrival_tick = frame.tick
            settled = (
                frame.player is not None and frame.player.hp > 0 and not frame.game_over
                and frame.control == 1 and frame.raw["transition"] == 0
                and frame.event_phase == 0 and frame.raw["scene"]["available"]
                and frame.raw["field_rendering_enabled"]
            )
            if settled:
                if self.quiet_tick is None:
                    self.quiet_tick = frame.tick
                if frame.tick - self.quiet_tick >= 120 and self.accepted_tick is None:
                    self.accepted_tick = frame.tick
            else:
                self.quiet_tick = None
        elif self.accepted_tick is None:
            self.quiet_tick = None

    def step(self, frame: Frame) -> InputAction | None:
        if (self.contract is None or self.blocked_reason or self.arrival_tick is not None
                or not frame.ready or frame.control != 1 or frame.event_phase != 0
                or frame.player is None or frame.camera_yaw is None
                or frame.tick-self.last_input_tick < 6
                or (frame.zone, frame.room) != (self.source_zone, self.source_room)):
            return None
        x = self.contract["tile_x"] * 128 + 64
        z = self.contract["tile_z"] * 128 + 64
        dx, dz = x-frame.player.position[0], z-frame.player.position[2]
        position = frame.player.position
        if (self.progress_position is None or frame.tick-self.last_input_tick > 32
                or sum((position[i]-self.progress_position[i])**2 for i in (0, 2)) >= 16**2):
            self.progress_position, self.progress_tick = position, frame.tick
        self.last_input_tick = frame.tick
        if dx*dx+dz*dz <= 192*192 and (
            not self.interaction_seen and
            (not self.open_ticks or frame.tick-self.open_ticks[-1] >= 90)
        ):
            self.open_ticks.append(frame.tick)
            return InputAction(frozenset({"Cross"}), 2, "physically open the authored door")
        buttons = direction_buttons(dx, dz, frame.camera_yaw)
        if not buttons:
            return None
        if self.recovery_axis is not None:
            component = dx if self.recovery_axis == 0 else dz
            if abs(component) <= 16 or frame.tick >= self.axis_until:
                self.recovery_axis = None
            else:
                buttons = direction_buttons(
                    dx if self.recovery_axis == 0 else 0,
                    dz if self.recovery_axis == 2 else 0, frame.camera_yaw,
                )
                return InputAction(buttons, 8, "bounded world-axis correction at a walking obstruction")
        if frame.tick-self.progress_tick >= 90 and dx*dx+dz*dz > 192*192:
            if frame.player.actor_flags & 0x80000:
                self.blocked_reason = "Native movement flags currently suppress field jump input"
                return None
            nearby = [attempt for attempt in self.jump_attempts
                      if sum((position[i]-attempt["position_before"][i])**2 for i in (0, 2)) < 128**2]
            if len(nearby) >= 2:
                attempted = {attempt["axis"] for attempt in self.axis_attempts
                             if sum((position[i]-attempt["position_before"][i])**2 for i in (0, 2)) < 128**2}
                axes = [axis for axis, delta in ((0, dx), (2, dz))
                        if abs(delta) > 16 and axis not in attempted]
                if not axes:
                    self.blocked_reason = "No walking progress after bounded jump and axis recovery"
                    return None
                self.recovery_axis = axes[0]
                self.axis_until = frame.tick + 64
                self.axis_attempts.append({
                    "tick": frame.tick, "position_before": position, "axis": self.recovery_axis,
                })
                buttons = direction_buttons(
                    dx if self.recovery_axis == 0 else 0,
                    dz if self.recovery_axis == 2 else 0, frame.camera_yaw,
                )
                return InputAction(buttons, 8, "bounded world-axis correction at a walking obstruction")
            self.jump_attempts.append({"tick": frame.tick, "position_before": position})
            self.progress_tick = frame.tick
            # Actors_UpdateAshleyMovementInput, original BATTLE/146C.c:2837.
            return InputAction(buttons | {"Square"}, 2, "normal directed jump at an observed walking stall")
        return InputAction(buttons, 8, "walk toward the observed original door trigger")

    def result(self) -> dict:
        return {
            "id": self.identity, "coverage": "physical_controller_entry",
            "contract": self.contract, "open_ticks": self.open_ticks,
            "loading_observed": self.loading_seen, "arrival_tick": self.arrival_tick,
            "quiet_tick": self.quiet_tick, "acceptance_tick": self.accepted_tick,
            "blocked_reason": self.blocked_reason,
            "interaction_observed": self.interaction_seen,
            "jump_attempts": self.jump_attempts,
            "axis_attempts": self.axis_attempts,
        }


class PhysicalRoute:
    def __init__(self, identities: tuple[str, ...]) -> None:
        if not identities:
            raise ValueError("A physical route needs at least one original door record")
        self.legs = tuple(PhysicalDoor(identity) for identity in identities)
        self.index = 0
        self.accepted_tick: int | None = None
        self.checkpoints: list[dict] = []

    @property
    def blocked_reason(self) -> str | None:
        return self.legs[self.index].blocked_reason

    def observe(self, frame: Frame) -> None:
        if self.accepted_tick is not None:
            return
        leg = self.legs[self.index]
        leg.observe(frame)
        if leg.contract is not None and self.index + 1 < len(self.legs):
            following = self.legs[self.index + 1]
            destination = (leg.contract["destination_zone"], leg.contract["destination_room"])
            if destination != (following.source_zone, following.source_room):
                raise ValueError("Next route leg does not start at the observed authored destination")
        if leg.accepted_tick is None:
            return
        self.checkpoints.append(leg.result())
        if self.index + 1 == len(self.legs):
            self.accepted_tick = leg.accepted_tick
            return
        self.index += 1
        self.legs[self.index].observe(frame)

    def step(self, frame: Frame) -> InputAction | None:
        if self.accepted_tick is not None:
            return None
        return self.legs[self.index].step(frame)

    def result(self) -> dict:
        return {
            "coverage": "physical_controller_route",
            "acceptance_tick": self.accepted_tick,
            "accepted_legs": len(self.checkpoints),
            "active_index": self.index,
            "cases": [leg.result() for leg in self.legs],
        }
