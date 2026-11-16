"""F01099/F01104/F01105/F01120 event scene handoff opcodes against original BATTLE.PRG.

The room enemy cache clear (opcode 0x53), the HUD visibility (0x58), the menu and reward transition (0x59) and the
scripted room change (0x68) run as original code with every service they call stubbed and recorded in order: the
room enemy memory, the HUD, the script flags, the reward flow, the menu framework, the zone music switch and the
two scripted transitions, whose door record is compared word for word. The native driver runs the Live opcode
wrappers over the same globals with the same services recorded.
"""

import random
from dataclasses import dataclass

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Image,
    Machine,
    printed_problems,
    recorded_observation,
)

ID = "F01099/F01104/F01105/F01120"
FUNCTIONS = ("F01099", "F01104", "F01105", "F01120")
ENTRIES = (0x800B91C4, 0x800B92D0, 0x800B92F4, 0x800B98C0)
# Verified against the disassembly and the symbol order: func_800B91F0 follows F01099, F01105 (0x800B92F4)
# follows F01104, func_800B94D8 follows F01105's epilogue and func_800B9A40 follows F01120.
ENDS = (0x800B91F0, 0x800B92F4, 0x800B94D8, 0x800B9A40)
ENTRY, END = ENTRIES[0], ENDS[-1]
MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "event_scene_transition_driver.cpp"
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
INCLUDES = (
    r"Native\Game\src",
    r"Native\Engine",
    r"Native\Engine\src",
    r"Native\Content",
)
SOURCES = (
    r"Native\Game\src\Event\EventSceneTransition.cpp",
    r"Native\Game\src\Event\EventSceneTransitionLive.cpp",
    r"Native\Game\src\Field\DoorRecordLive.cpp",
    r"Native\Engine\src\Core\Log.cpp",
)
VECTORS, MUTANT_VECTORS, SEED = 4 * 1024, 4 * 256, 121099

MASK = 0xFFFFFFFF
INSTRUCTION, INSTRUCTION_BYTES = 0x80600000, 10
MENU = 0x80610000
# Event_isMenuTransitionActive (byte), Event_musicSlot (byte) and Event_roomChangeState (halfword).
ACTIVE, MUSIC_SLOT, ROOM_STATE = 0x800F4BF8, 0x800F4C69, 0x800F4BE2
# D_800E9BF0: the cube puzzle screens of modes 10-12, which the native keeps as a constant.
CUBE_SCREENS, CUBE_SCREEN_VALUES = 0x800E9BF0, bytes((6, 4, 5))
OPCODES = (0x53, 0x58, 0x59, 0x68)

FORGET, HUD = 0x8007BBB8, 0x8007BD8C
GET_FLAG, SET_FLAG = 0x800BEBF4, 0x800BEC14
EVENT_REWARDS, POST_ACTION_REWARDS, UPDATE_REWARDS = 0x8007B9A0, 0x8007B9CC, 0x8007B9FC
OPEN_MENU, UPDATE_OVERLAY, SELECTION, RELEASE_ROWS = 0x800CACD0, 0x800C97BC, 0x800CB66C, 0x800CB114
MUSIC_SWITCH, ROOM_TRANSITION, ZONE_TRANSITION = 0x8007BE10, 0x8007C580, 0x8007C654
# Call kinds shared with the driver; a transition is followed by its door's three words.
CALL_FORGET, CALL_HUD, CALL_GET_FLAG, CALL_SET_FLAG = 10, 20, 30, 31
CALL_EVENT_REWARDS, CALL_POST_ACTION_REWARDS, CALL_UPDATE_REWARDS = 40, 41, 42
CALL_OPEN_MENU, CALL_UPDATE_OVERLAY, CALL_SELECTION, CALL_RELEASE_ROWS = 50, 51, 52, 53
CALL_MUSIC_SWITCH, CALL_ROOM, CALL_ZONE = 60, 70, 80
DOOR_FIRST, DOOR_LAST = 1, 2

JR_RA, NOP = 0x03E00008, 0x00000000
MUTANTS = {
    "F01099-early-return": {ENTRIES[0]: JR_RA},
    "F01099-swapped-room-and-zone": {0x800B91D0: 0x90440001, 0x800B91D4: 0x90450002},
    "F01099-room-from-byte-3": {0x800B91D0: 0x90440003},
    "F01099-returns-1": {0x800B91E4: 0x24020001},
    "F01104-not-flipped": {0x800B92E0: NOP},
    "F01104-flips-bit-1": {0x800B92E0: 0x38840002},
    "F01104-skips-hud": {0x800B92DC: NOP},
    "F01104-returns-1": {0x800B92E8: 0x24020001},
    # Reads the next cube puzzle screen: mode 10 shows the start screen.
    "F01105-cube-screen-shifted": {0x800B935C: 0x9065FFF7},
    "F01105-game-clear-unmasked-flag": {0x800B939C: NOP},
    "F01105-game-clear-music-slot-3": {0x800B93C4: 0x24030003},
    "F01105-finish-keeps-screen": {0x800B93E8: NOP},
    "F01105-reward-finish-keeps-active": {0x800B93F8: NOP},
    "F01105-reward-hold-returns-0": {0x800B93E0: 0x24020000},
    "F01105-post-action-runs-event-rewards": {0x800B943C: 0x0C01EE68},
    "F01105-menu-mode-1": {0x800B9480: 0x24050001},
    "F01105-menu-closed-at-3": {0x800B94A8: 0x24020003},
    "F01105-menu-keeps-rows": {0x800B94B4: NOP},
    "F01120-tile-x-mask-15": {0x800B9920: 0x3042000F},
    "F01120-music-switch-bit-2": {0x800B99D8: 0x00042082},
    "F01120-reserved-bit-from-bit-2": {0x800B9998: 0x00021082},
    "F01120-room-is-zone": {0x800B99A8: 0x92020002},
    "F01120-speed-from-byte-8": {0x800B99BC: 0x92060008},
    "F01120-keeps-event-on-bit-3": {0x800B99FC: 0x30420008},
    "F01120-arrival-keeps-state": {0x800B9A28: NOP},
}


@dataclass(frozen=True)
class Vector:
    function: int
    instruction: tuple[int, ...]
    active: int
    music_slot: int
    room_state: int
    flag: int
    rewards_running: int
    menu_status: int
    menu_high: int

    def inputs(self) -> list[int]:
        return [self.function, *self.instruction, self.active, self.music_slot, self.room_state, self.flag,
                self.rewards_running, self.menu_status]


def _instruction(rng: random.Random, function: int) -> tuple[int, ...]:
    raw = [OPCODES[function], *(rng.getrandbits(8) for _ in range(INSTRUCTION_BYTES - 1))]
    if function == 2:
        raw[1] = rng.choice((10, 11, 12, 13, 14, 15, 13, 14, 9, 16, 0, rng.getrandbits(8)))
    elif function == 3:
        raw[1] = rng.choice((0, 0, 1, rng.getrandbits(8)))
        raw[7] = rng.choice((rng.getrandbits(8), rng.getrandbits(4), 0, 0xFF))
    elif function == 0:
        raw[2] = rng.choice((raw[2], raw[1]))
    return tuple(raw)


def vector(rng: random.Random, number: int) -> Vector:
    function = number % len(FUNCTIONS)
    return Vector(
        function=function,
        instruction=_instruction(rng, function),
        active=rng.choice((0, 0, 1, rng.getrandbits(8))),
        music_slot=rng.getrandbits(8),
        room_state=rng.choice((0, 0, 1, 2, 3, rng.getrandbits(16))),
        flag=(rng.getrandbits(24) << 8 | rng.choice((1, 1, 1, 0, 2, rng.getrandbits(8)))) & MASK,
        rewards_running=rng.choice((0, 0, 1, rng.getrandbits(32))),
        menu_status=rng.choice((2, 2, 0, 1, 3, 0x0102, 0xFFFE, rng.getrandbits(16))),
        menu_high=rng.getrandbits(16),
    )


def _door_calls(m: Machine, kind: int) -> list[tuple[int, int, int]]:
    door = m.r[4]
    return [(kind, m.r[5], m.r[6]), (kind + DOOR_FIRST, m.load(door), m.load(door + 4)),
            (kind + DOOR_LAST, m.load(door + 8), 0)]


def _install_stubs(machine: Machine, value: Vector, calls: list[tuple[int, int, int]]) -> None:
    def recorder(kind: int, arguments: int = 0, result=None):
        def handler(m: Machine) -> None:
            calls.append((kind, m.r[4] if arguments else 0, m.r[5] if arguments > 1 else 0))
            if result is not None:
                m.r[2] = result(m) & MASK
        return handler

    def selection(m: Machine) -> int:
        m.put(MENU, value.menu_status, 2)
        m.put(MENU + 2, value.menu_high, 2)
        return MENU

    def transition(kind: int):
        def handler(m: Machine) -> None:
            calls.extend(_door_calls(m, kind))
        return handler

    machine.stub(FORGET, recorder(CALL_FORGET, 2))
    machine.stub(HUD, recorder(CALL_HUD, 1))
    machine.stub(GET_FLAG, recorder(CALL_GET_FLAG, 1, lambda m: value.flag))
    machine.stub(SET_FLAG, recorder(CALL_SET_FLAG, 2))
    machine.stub(EVENT_REWARDS, recorder(CALL_EVENT_REWARDS))
    machine.stub(POST_ACTION_REWARDS, recorder(CALL_POST_ACTION_REWARDS))
    machine.stub(UPDATE_REWARDS, recorder(CALL_UPDATE_REWARDS, 0, lambda m: value.rewards_running))
    machine.stub(OPEN_MENU, recorder(CALL_OPEN_MENU, 2))
    machine.stub(UPDATE_OVERLAY, recorder(CALL_UPDATE_OVERLAY))
    machine.stub(SELECTION, recorder(CALL_SELECTION, 0, selection))
    machine.stub(RELEASE_ROWS, recorder(CALL_RELEASE_ROWS))
    machine.stub(MUSIC_SWITCH, recorder(CALL_MUSIC_SWITCH, 1))
    machine.stub(ROOM_TRANSITION, transition(CALL_ROOM))
    machine.stub(ZONE_TRANSITION, transition(CALL_ZONE))
    machine.follow.update(ENTRIES)


def run(machine: Machine, value: Vector) -> tuple[dict, list[int]]:
    calls: list[tuple[int, int, int]] = []
    machine.map_zero(INSTRUCTION, 16)
    machine.store(INSTRUCTION, bytes(value.instruction))
    machine.map_zero(MENU, 4)
    machine.put(ACTIVE, value.active, 1)
    machine.put(MUSIC_SLOT, value.music_slot, 1)
    machine.put(ROOM_STATE, value.room_state, 2)
    _install_stubs(machine, value, calls)
    machine.stores.clear()
    machine.r[4:6] = [INSTRUCTION, 0]
    machine.execute(ENTRIES[value.function], limit=20000)
    observed = [machine.r[2], machine.load(ACTIVE, 1), machine.load(MUSIC_SLOT, 1), machine.load(ROOM_STATE, 2)]
    names = ["return", "menu-transition-active", "music-slot", "room-change-state"]
    return recorded_observation(machine, observed, names, calls), value.inputs()


def check(image: Image, value: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del value, reference
    table = bytes(image.read(CUBE_SCREENS + index, 1) for index in range(len(CUBE_SCREEN_VALUES)))
    if table != CUBE_SCREEN_VALUES:
        return [f"D_800E9BF0 is {table.hex(' ')}, the native keeps {CUBE_SCREEN_VALUES.hex(' ')}"]
    allowed = [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (ACTIVE, ACTIVE + 1),
        (MUSIC_SLOT, MUSIC_SLOT + 1),
        (ROOM_STATE, ROOM_STATE + 2),
    ]
    return printed_problems(observation, native, allowed)


def _menu_path(value: Vector) -> str:
    mode = value.instruction[1]
    kind = ("cube" if 10 <= mode <= 12 else {13: "time-attack", 14: "game-clear", 15: "post-action"}.get(mode, "menu"))
    return f"{kind}-{'finish' if value.active else 'start'}"


def describe(image: Image, results: list) -> list[str]:
    by_function = [0] * len(FUNCTIONS)
    menu_paths: dict[str, int] = {}
    menu_finished = room_starts = zone_starts = arrivals = 0
    for value, observation, _ in results:
        by_function[value.function] += 1
        returned = observation["values"][0]
        if value.function == 2:
            path = _menu_path(value)
            menu_paths[path] = menu_paths.get(path, 0) + 1
            menu_finished += returned == 4
        elif value.function == 3:
            kinds = [call[0] for call in observation["calls"]]
            room_starts += CALL_ROOM in kinds
            zone_starts += CALL_ZONE in kinds
            arrivals += returned == 0
    ranges = ", ".join(
        f"{name} {entry:08X}-{end - 4:08X} ({(end - entry) // 4} words, sha256={image.sha256(entry, end)[:16]})"
        for name, entry, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True)
    )
    table = bytes(image.read(CUBE_SCREENS + index, 1) for index in range(len(CUBE_SCREEN_VALUES)))
    return [
        "Functions: " + ranges,
        "Vectors per function: "
        + ", ".join(f"{name}={count}" for name, count in zip(FUNCTIONS, by_function, strict=True)),
        f"D_800E9BF0 (cube puzzle screens of modes 10-12): {table.hex(' ')}",
        "F01105 paths: " + ", ".join(f"{path}={count}" for path, count in sorted(menu_paths.items()))
        + f"; finished (returned 4): {menu_finished}",
        f"F01120 room transitions: {room_starts}, zone transitions: {zone_starts}, arrivals (returned 0): {arrivals}",
        "Compared the return, the active byte, the music slot, the room change state, every service call in order "
        "with its arguments (each transition's 12-byte door record word for word; the original's unset stack "
        "fields read 0 here) and the original's stores.",
    ]
