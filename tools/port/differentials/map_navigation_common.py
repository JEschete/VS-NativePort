"""Shared harness for the map room navigation differentials (MENU5.PRG 4D8.c:1048-1055, 1511-1763 and
1834-1878, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::map core
(MapNavigation.cpp) in map_navigation_driver.cpp. Every vector draws a scene (a room count word from -1
to 6 and that many 12-byte records whose vertex pointers lead to separate blocks of -1 to 8 vertices),
_centerPoint, _currentRoomIndex, _isCurrentScene, the room-name links, the minimap scene's first room
and the call's argument (a room, a scale factor or a search direction). Vertex halves are biased to the
extremes and the step boundaries (7, 8, 128, 129) so rounding, the bounding-box sentinels and 16-bit
wrapping are exercised; zones and maps come from a small domain so matches and duplicates occur. The
original runs on absolute vertex pointers; the driver gives the core one span per room. Both print the
result, the center, the two room globals, every room's visited word, every vertex halfword (the pad
included, which neither writes) and the sound calls.

For _updateRoomIndex the vector marks a room visited when the drawn links would never reach one, since
the original then never returns (the typed core refuses after one pass; MapNavigationTests.cpp covers
that and links that leave the rooms).
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MODULE_BASES,
    Machine,
    mode_case,
    recorded_observation,
    s32,
)

MODULE_BASES["MENU/MENU5.PRG"] = 0x80102800

MODULES = ("MENU/MENU5.PRG",)
DRIVER = "map_navigation_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\MapNavigation.cpp",)

SCENE = 0x80500000
RECORD_BYTES = 12
VERTEX_BLOCKS = 0x80510000
VERTEX_BLOCK_STRIDE = 0x400
VERTEX_BYTES = 8
NAMES = 0x80520000
NAME_BYTES = 36
NAME_PREVIOUS = 0x20
NAME_NEXT = 0x22
SECTION_F = 0x80530000
SECTION_F_BYTES = 0x20

SCENE_BUFFER_POINTER = 0x800F1BC0  # vs_battle_sceneBuffer
SECTION_F_POINTER = 0x800F1C94  # vs_battle_roomData.sectionF
CENTER = 0x80108DB4  # _centerPoint
CURRENT_ROOM = 0x80108D64  # _currentRoomIndex
IS_CURRENT_SCENE = 0x80108D78  # _isCurrentScene
NAMES_POINTER = 0x80108D98  # _roomNamesTable
PLAY_SFX = 0x80045988  # vs_main_playSfxDefault

MARK, CENTER_ON, SET_TARGET, ANIMATE, SCALE, INITIAL, ADJACENT = range(7)

MAX_ROOMS = 6
MAX_VERTICES = 8
EDGE_HALVES = (0, 1, -1, 7, -7, 8, -8, 128, -128, 129, -129, 0x7FFF, -0x8000, 0x4000, -0x4001)
FACTORS = (4, 0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x10001, -0x10001, 0x7FFFFFFF, -0x80000000)


def half(rng: random.Random) -> int:
    return rng.choice((*EDGE_HALVES, rng.randrange(-0x8000, 0x8000), rng.randrange(-300, 300)))


def word(rng: random.Random) -> int:
    return rng.choice((0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, s32(rng.randrange(1 << 32))))


def rooms_of(count: int) -> int:
    return max(count, 0)


def adjacent_reaches_visited(value: dict) -> int | None:
    """The first room the original's link walk reaches, or None when it would never stop."""
    index = value["current_room"]
    seen = set()
    links = value["links"]
    while True:
        index = links[index][1 if value["argument"] > 0 else 0]
        if value["rooms"][index]["visited"] != 0:
            return index
        if index in seen:
            return None
        seen.add(index)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    lowest = 1 if mode in (CENTER_ON, SET_TARGET, ADJACENT) else -1
    count = rng.choice((*range(lowest, MAX_ROOMS + 1), lowest, MAX_ROOMS))
    zones = rng.choice(((0,), (0, 1), (0, 1, 2)))
    rooms = []
    for _ in range(rooms_of(count)):
        vertex_count = rng.choice((*range(-1, MAX_VERTICES + 1), MAX_VERTICES))
        rooms.append({
            "visited": rng.choice((0, 0, 1, 1, -1, word(rng))),
            "zone": rng.choice(zones),
            "map": rng.choice(zones),
            "vertex_count": vertex_count,
            "vertices": [[half(rng) for _ in range(4)] for _ in range(max(vertex_count, 0))],
        })
    links = [[rng.randrange(max(count, 1)), rng.randrange(max(count, 1))] for _ in range(rooms_of(count))]
    center = [half(rng) for _ in range(3)]
    if mode == ANIMATE:
        pattern = number % 8
        if pattern == 0:
            center = [0, 0, 0]
        elif pattern == 1:
            center = [0, 0, half(rng) or 5]
        elif pattern == 2:
            center = [0, half(rng) or -9, 0]
    argument = 0
    if mode in (CENTER_ON, SET_TARGET):
        argument = rng.randrange(count)
    elif mode == SCALE:
        argument = rng.choice((*FACTORS, word(rng), rng.randrange(-16, 17)))
    elif mode == ADJACENT:
        argument = rng.choice((1, 0, -1, 2, 0x7FFFFFFF, -0x80000000, word(rng)))
    value = {
        "mode": mode,
        "count": count,
        "rooms": rooms,
        "links": links,
        "center": center,
        "argument": argument,
        "current_room": rng.randrange(count) if count > 0 else 0,
        "is_current_scene": rng.choice((0, -1, 3, word(rng))),
        "section_f": [rng.randrange(256) for _ in range(SECTION_F_BYTES)],
    }
    if mode == INITIAL and rooms and rng.random() < 0.5:
        target = rng.choice(rooms)
        value["section_f"][0xC:0x10] = [target["zone"] & 0xFF, (target["zone"] >> 8) & 0xFF,
                                        target["map"] & 0xFF, (target["map"] >> 8) & 0xFF]
    if mode == ADJACENT and adjacent_reaches_visited(value) is None:
        first = links[value["current_room"]][1 if argument > 0 else 0]
        rooms[first]["visited"] = 1
    return value


def section_f_address(value: dict) -> tuple[int, int]:
    data = value["section_f"]
    zone = int.from_bytes(bytes(data[0xC:0xE]), "little", signed=True)
    map_id = int.from_bytes(bytes(data[0xE:0x10]), "little", signed=True)
    return zone, map_id


def block_of(room: int) -> int:
    return VERTEX_BLOCKS + VERTEX_BLOCK_STRIDE * room


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    rooms = value["rooms"]
    machine.store(SCENE, bytes(4 + RECORD_BYTES * MAX_ROOMS))
    machine.put(SCENE, value["count"])
    for index, room in enumerate(rooms):
        record = SCENE + 4 + RECORD_BYTES * index
        machine.put(record, room["visited"])
        machine.put(record + 4, block_of(index))
        machine.put(record + 8, room["zone"], 2)
        machine.put(record + 10, room["map"], 2)
        machine.put(block_of(index), room["vertex_count"])
        for number, vertex in enumerate(room["vertices"]):
            for field, item in enumerate(vertex):
                machine.put(block_of(index) + 4 + VERTEX_BYTES * number + 2 * field, item, 2)
    for index, (previous, following) in enumerate(value["links"]):
        machine.store(NAMES + NAME_BYTES * index, bytes(NAME_BYTES))
        machine.put(NAMES + NAME_BYTES * index + NAME_PREVIOUS, previous, 2)
        machine.put(NAMES + NAME_BYTES * index + NAME_NEXT, following, 2)
    machine.store(SECTION_F, bytes(value["section_f"]))
    machine.put(SCENE_BUFFER_POINTER, SCENE)
    machine.put(SECTION_F_POINTER, SECTION_F)
    machine.put(NAMES_POINTER, NAMES)
    for field, item in enumerate(value["center"]):
        machine.put(CENTER + 2 * field, item, 2)
    machine.put(CURRENT_ROOM, value["current_room"])
    machine.put(IS_CURRENT_SCENE, value["is_current_scene"])
    calls: list[list[int]] = []

    def play(machine: Machine) -> None:
        calls.append([s32(machine.r[4]), s32(machine.r[5])])

    machine.stub(PLAY_SFX, play)
    machine.r[4] = SCENE if mode != ADJACENT else value["argument"] & 0xFFFFFFFF
    machine.r[5] = value["argument"] & 0xFFFFFFFF
    machine.r[2] = 0
    machine.execute(entry)

    values = [
        s32(machine.r[2]) if mode == INITIAL else 0,
        *(machine.load(CENTER + 2 * field, 2, signed=True) for field in range(3)),
        machine.load(IS_CURRENT_SCENE, 4, signed=True),
        machine.load(CURRENT_ROOM, 4, signed=True),
    ]
    names = ["result", "center x", "center y", "center z", "is current scene", "current room"]
    for index, room in enumerate(rooms):
        values.append(machine.load(SCENE + 4 + RECORD_BYTES * index, 4, signed=True))
        names.append(f"room {index} visited")
        for number in range(len(room["vertices"])):
            for field in range(4):
                address = block_of(index) + 4 + VERTEX_BYTES * number + 2 * field
                values.append(machine.load(address, 2, signed=True))
                names.append(f"room {index} vertex {number} half {field}")
    inputs = [
        value["count"],
        value["argument"],
        *value["center"],
        value["current_room"],
        value["is_current_scene"],
        *section_f_address(value),
    ]
    for room in rooms:
        inputs += [room["visited"], room["zone"], room["map"], room["vertex_count"]]
        inputs += [item for vertex in room["vertices"] for item in vertex]
    for previous, following in value["links"]:
        inputs += [previous, following]
    return recorded_observation(machine, values, names, calls), [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    mode = value["mode"]
    rooms = value["rooms"]
    if mode == MARK:
        allowed += [(SCENE + 4 + RECORD_BYTES * index, SCENE + 8 + RECORD_BYTES * index)
                    for index in range(len(rooms))]
    if mode in (CENTER_ON, SET_TARGET, ANIMATE):
        allowed.append((CENTER, CENTER + 6))
    if mode in (CENTER_ON, ANIMATE, SCALE):
        for index, room in enumerate(rooms):
            for number in range(len(room["vertices"])):
                vertex = block_of(index) + 4 + VERTEX_BYTES * number
                allowed.append((vertex, vertex + 6))
    if mode == INITIAL:
        allowed.append((IS_CURRENT_SCENE, IS_CURRENT_SCENE + 4))
    if mode == ADJACENT:
        allowed.append((CURRENT_ROOM, CURRENT_ROOM + 4))
    return allowed


def describe(image, results) -> list[str]:
    called = sum(1 for _, observation, _ in results if observation["calls"])
    empty = sum(1 for value, _, _ in results if any(room["vertex_count"] <= 0 for room in value["rooms"]))
    roomless = sum(1 for value, _, _ in results if value["count"] <= 0)
    return [f"{called} of {len(results)} vectors played the sound",
            f"{empty} vectors had a room without vertices, {roomless} no rooms"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
