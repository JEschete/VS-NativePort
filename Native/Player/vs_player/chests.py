from .controller import InputAction, direction_buttons
from .navigation import TerrainNavigator
from .observations import Frame

# Field_TileInFrontOfActor (BATTLE.PRG func_80074798): quadrant -> front-tile offset.
FRONT_OFFSET = {0: (0, -1), 1: (-1, 0), 2: (0, 1), 3: (1, 0)}


class RoomChestClaim:
    """Open an unlocked original room chest by walking to it and pressing Cross.

    Field_FindFacingUnlootedChest (BATTLE.PRG func_80074950) accepts Cross only when the chest
    occupies Ashley's front tile and its authored facing equals her facing quadrant, so the
    approach tile is the chest tile minus that quadrant's front offset.
    """

    def __init__(self, zone: int, room: int) -> None:
        self.zone, self.room = zone, room
        self.chest: dict | None = None
        self.skipped: str | None = None
        self.blocked_reason: str | None = None
        self.opened_tick: int | None = None
        self.interaction_seen = False
        self.cross_ticks: list[int] = []
        self.turn_ticks: list[int] = []
        self.last_input_tick = -100
        self.navigator = TerrainNavigator()
        self.identity = f"chest:{zone}:{room}"

    @property
    def pending(self) -> bool:
        return self.opened_tick is None and self.skipped is None and self.blocked_reason is None

    def approach_tile(self) -> tuple[int, int]:
        if self.chest is None:
            raise ValueError("No observed chest to approach")
        dx, dz = FRONT_OFFSET[self.chest["facing"] & 3]
        return self.chest["tile_x"] - dx, self.chest["tile_z"] - dz

    def observe(self, frame: Frame) -> None:
        if not frame.ready or (frame.zone, frame.room) != (self.zone, self.room):
            return
        chests = frame.raw.get("chests")
        if chests is None:
            if self.chest is None and self.skipped is None:
                self.skipped = "Native observation does not expose room chests"
            return
        if self.chest is None and self.skipped is None:
            eligible = [chest for chest in chests
                        if not chest["opened"] and chest["lock_level"] == 0 and chest["misc"]]
            if not eligible:
                self.skipped = "No unopened unlocked original chest with items in the source room"
                return
            self.chest = dict(eligible[0])
            self.identity = f"chest:{self.zone}:{self.room}:{self.chest['index']}"
        if self.chest is None:
            return
        self.interaction_seen |= frame.control == 9
        live = next((chest for chest in chests if chest["index"] == self.chest["index"]), None)
        if live is not None and live["opened"] and self.opened_tick is None:
            self.opened_tick = frame.tick
            self.navigator.path.clear()

    def navigation_request(self, frame: Frame, avoid_hostiles: bool = False) -> dict | None:
        if (not self.pending or self.chest is None or frame.player is None
                or not frame.raw.get("navigation", {}).get("available")
                or (frame.zone, frame.room) != (self.zone, self.room)
                or self._on_approach_tile(frame)):
            return None
        x, z = self.approach_tile()
        return self.navigator.request(frame, (x * 128 + 64, z * 128 + 64), avoid_hostiles=avoid_hostiles)

    def _on_approach_tile(self, frame: Frame) -> bool:
        if frame.player is None:
            return False
        position = frame.player.position
        return (position[0] // 128, position[2] // 128) == self.approach_tile()

    def step(self, frame: Frame) -> InputAction | None:
        if (not self.pending or self.chest is None or not frame.ready or frame.control != 1
                or frame.event_phase != 0 or frame.player is None or frame.camera_yaw is None
                or frame.tick - self.last_input_tick < 6
                or (frame.zone, frame.room) != (self.zone, self.room)):
            return None
        chest_tile = [self.chest["tile_x"], self.chest["tile_z"], self.chest["layer"]]
        position = frame.player.position
        if self._on_approach_tile(frame):
            self.navigator.path.clear()
            if frame.raw.get("navigation", {}).get("facing_tile") == chest_tile:
                if self.cross_ticks and frame.tick - self.cross_ticks[-1] < 90:
                    return None
                if len(self.cross_ticks) >= 3:
                    self.blocked_reason = "Original chest did not open after three facing Cross presses"
                    return None
                self.cross_ticks.append(frame.tick)
                self.last_input_tick = frame.tick
                return InputAction(frozenset({"Cross"}), 2, "physically open the facing original chest")
            if len(self.turn_ticks) >= 24:
                self.blocked_reason = "Could not face the original chest from its approach tile"
                return None
            self.turn_ticks.append(frame.tick)
            self.last_input_tick = frame.tick
            buttons = direction_buttons(chest_tile[0] * 128 + 64 - position[0],
                                        chest_tile[1] * 128 + 64 - position[2], frame.camera_yaw)
            return InputAction(buttons, 2, "turn toward the original chest from its approach tile")
        if frame.raw.get("navigation", {}).get("available"):
            return self.navigator.step(frame)
        x, z = self.approach_tile()
        buttons = direction_buttons(x * 128 + 64 - position[0], z * 128 + 64 - position[2],
                                    frame.camera_yaw)
        if not buttons:
            return None
        self.last_input_tick = frame.tick
        return InputAction(buttons, 4, "walk toward the original chest approach tile")

    def result(self) -> dict:
        return {
            "coverage": "physical_controller_chest", "zone": self.zone, "room": self.room,
            "chest": self.chest, "approach_tile": self.approach_tile() if self.chest else None,
            "opened_tick": self.opened_tick, "interaction_observed": self.interaction_seen,
            "cross_ticks": self.cross_ticks, "turn_inputs": len(self.turn_ticks),
            "skipped": self.skipped, "blocked_reason": self.blocked_reason,
            "navigation_feedback": self.navigator.feedback,
        }
