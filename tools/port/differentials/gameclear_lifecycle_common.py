"""Six MENUF game-clear originals; endpoint doubles are listed in describe().

Metadata imports neither images nor Capstone. Only the shared differential
Machine executes originals; the case supplies data and external endpoints.
"""

from __future__ import annotations

import random
import struct

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MASK,
    MODULE_BASES,
    Fault,
    Machine,
    printed_problems,
    s32,
)
from differentials.results_drawing_common import (
    ADD_PRIMITIVE,
    ATLAS_ENTRIES,
    PRIMITIVES,
    QUAD_ENTRY,
    SCRATCH,
    c_string,
    coordinate,
    edit_mutants,
    place_atlas,
    verify_edits,
)

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800
DRIVER = "gameclear_lifecycle_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Results\GameClearLifecycle.cpp",
    r"Native\Game\src\Menu\Score\ScoreRank.cpp",
    r"Native\Engine\src\Render\UiTexturedQuads.cpp",
)
SPECS = {
    "F03010": (0, 0x80102D10, 0x8010310C),
    "F03013": (1, 0x80103748, 0x801037A0),
    "F03015": (2, 0x8010384C, 0x80103D88),
    "F03043": (3, 0x801060A8, 0x801064D4),
    "F03045": (4, 0x80107698, 0x80107A8C),
    "F03064": (5, 0x80108E48, 0x80108EA0),
}
STATE = (
    ("init_phase", 0x800F1CD8), ("timer", 0x8010989C),
    ("phase", 0x801098A0), ("state_timer", 0x80109864),
    ("selection", 0x80109878), ("speed", 0x8010988C),
    ("reel", 0x80109890), ("score", 0x80109884),
    ("map", 0x80109888), ("shown_score", 0x80109898),
    ("counting_score", 0x801099EC), ("shown_map", 0x80109894),
    ("counting_map", 0x801099F0), ("clear_count", 0x80109880),
    ("rank", 0x8010987C),
)
UNSIGNED_STATE = {4, 5, 7, 8}
SCORE = 0x8005FEA0
MAP_FLAGS = 0x8005FFD8
MASKS = 0x800E8508
FLAGS = 0x80061598
BUTTONS = 0x8005E1D0
CURRENT_MUSIC = 0x8005E040
REELS = 0x80109610
COLORS = 0x80109784
REQUEST_SLOT = 0x8010984C
BUFFER_SLOT = 0x80109850
ASSET = 0x80200000
REQUEST = 0x80220000
RECTANGLES = 0x80221000
PALETTE = 0x80222000
DIS_FILE = 0x80102AE0
FRAME = 0x8005E210
DRAW_ENVS = 0x8005E0D0
TIM_STRIDE = 0x8220
CAPACITY = 512
CALL_WORDS = 12

# Address, original word, single changed instruction. Every original word is
# checked against the canonical image when executing, not while listing.
EDITS = {
    "F03010": {
        "reserve-first-page": (0x80102D34, 0x2404001D, 0x2404001C),
        "reserve-page-count": (0x80102D38, 0x24050003, 0x24050002),
        "reserve-owner": (0x80102D40, 0x24060005, 0x24060006),
        "loaded-state": (0x80102DA8, 0x24020004, 0x24020003),
        "image-height": (0x80102E00, 0x240200FF, 0x240200FE),
        "palette-width": (0x80102E4C, 0x24020080, 0x2402007F),
        "palette-first-word": (0x80102E68, 0xAC400000, 0),
        "tim-count": (0x80102E88, 0x2A020003, 0x2A020002),
        "image-x-stride": (0x80102E90, 0x26520040, 0x2652003F),
        "reel-random-mask": (0x80102F24, 0x304200F0, 0x304200E0),
        "reel-row": (0x80102F28, 0x34420008, 0x34420004),
        "healing-call": (0x80102F94, 0x0C042392, 0),
        "room-alias": (0x80102FC4, 0x3C020040, 0x3C020020),
        "room-bits": (0x80103010, 0x28820020, 0x2882001F),
        "room-words": (0x80103024, 0x2A020010, 0x2A02000F),
        "chest-count": (0x8010305C, 0x2A020040, 0x2A02003F),
        "map-percent": (0x801030B8, 0x00021080, 0x00021040),
        "completion-result": (0x801030E8, 0x24020001, 0x24020000),
    },
    "F03013": {
        "first-interval": (0x80103760, 0x2482FFE1, 0x2482FFE2),
        "first-upper-bound": (0x80103764, 0x2C420009, 0x2C420008),
        "late-upper-bound": (0x80103770, 0x2C420005, 0x2C420004),
        "track-offset": (0x80103780, 0x24840064, 0x24840063),
        "default-track": (0x80103784, 0x24040080, 0x24040081),
        "music-slot": (0x8010378C, 0x24050002, 0x24050001),
    },
    "F03015": {
        "reveal-sound-time": (0x80103874, 0x240200D0, 0x240200CF),
        "score-delay": (0x801038BC, 0x24C6FFE0, 0x24C6FFE1),
        "map-delay": (0x801038D4, 0x24C6FF90, 0x24C6FF91),
        "heading-delay": (0x801038E8, 0x24C6FF40, 0x24C6FF3F),
        "speed-divisor": (0x80103940, 0x000518C2, 0x00051882),
        "selection-width": (0x8010394C, 0x304200FF, 0x3042007F),
        "top-speed": (0x80103984, 0x2CC20030, 0x2CC2002F),
        "prompt-threshold": (0x80103998, 0x28E20121, 0x28E20120),
        "confirm-mask": (0x801039C8, 0x30420020, 0x30420010),
        "auto-stop": (0x801039DC, 0x28420819, 0x2842081A),
        "fade-ticks": (0x80103A04, 0x24070030, 0x2407002F),
        "skip-clock": (0x80103A9C, 0x24020110, 0x2402010F),
        "stop-speed": (0x80103B28, 0x2CA20009, 0x2CA20008),
        "stop-row": (0x80103B3C, 0x24020008, 0x24020007),
        "hold-threshold": (0x80103BC4, 0x284201C3, 0x284201C2),
        "closing-frames": (0x80103BF0, 0x24110008, 0x24110009),
        "unsigned-selection": (0x80103C8C, 0x90439878, 0x80439878),
        "award-stat-range": (0x80103CC0, 0x2CA20005, 0x2CA20004),
        "retire-owner": (0x80103D3C, 0x24060005, 0x24060004),
        "finish-result": (0x80103D4C, 0x24020001, 0x24020000),
        "state-timer-step": (0x80103D64, 0x24630001, 0x24630002),
        "screen-timer-step": (0x80103D68, 0x24840001, 0x24840002),
    },
    "F03043": {
        "fade-clamp": (0x801060F8, 0x241E0040, 0x241E003F),
        "bonus-label": (0x80106104, 0x2406000B, 0x2406000C),
        "color-step": (0x80106124, 0xA23E0003, 0),
        "centering-padding": (0x8010613C, 0x2442000E, 0x24420010),
        "colon": (0x80106154, 0x2406001A, 0x24060019),
        "clip-width": (0x8010621C, 0x24030140, 0x2403013F),
        "window-height": (0x80106230, 0x24420010, 0x2442000F),
        "row-origin": (0x80106268, 0x26E30008, 0x26E30007),
        "spinning-plus": (0x801062E0, 0x24060018, 0x24060019),
        "digit-advance": (0x80106354, 0x26B5000C, 0x26B5000B),
        "row-count": (0x8010635C, 0x2A220003, 0x2A220002),
        "row-height": (0x80106364, 0x26F7FFF0, 0x26F7FFF1),
        "stopped-palette": (0x801063B0, 0x24147FF2, 0x24147FF3),
    },
    "F03045": {
        "packet-length": (0x801076FC, 0x24070009, 0x24070008),
        "pixel-height": (0x80107720, 0x26740001, 0x26740002),
        "top-window-edge": (0x8010778C, 0x2A6200BE, 0x2A6200BF),
        "top-fade-origin": (0x80107794, 0x240200BD, 0x240200BC),
        "top-fade-scale": (0x8010779C, 0x00021100, 0x000210C0),
        "top-fade-clut": (0x801077B8, 0x24077FF3, 0x24077FF4),
        "top-additive-page": (0x801077DC, 0x34420020, 0x34420040),
        "top-subtractive-page": (0x801078AC, 0x34420040, 0x34420020),
        "bottom-window-edge": (0x801078B0, 0x2A6200CE, 0x2A6200CF),
        "bottom-fade-scale": (0x801078C0, 0x00021100, 0x000210C0),
        "bottom-fade-clut": (0x801078DC, 0x24077FF3, 0x24077FF4),
        "bottom-additive-page": (0x80107900, 0x34420020, 0x34420040),
        "bottom-subtractive-page": (0x801079D0, 0x34420040, 0x34420020),
        "full-window-clut": (0x801079DC, 0x24027FF2, 0x24027FF1),
        "screen-row-step": (0x80107A18, 0x26730001, 0x26730002),
        "texture-row-step": (0x80107A50, 0x26F70001, 0x26F70002),
    },
    "F03064": {
        "bonus": (0x80108E4C, 0x34A586A0, 0x34A586A1),
        "cap": (0x80108E54, 0x34E7C9FF, 0x34E7C9FE),
        "heal-count-width": (0x80108E64, 0x94C40112, 0x84C40112),
        "penalty-scale": (0x80108E7C, 0x00021080, 0x00021040),
        "clear-heal-count": (0x80108E9C, 0xA4C00112, 0),
    },
}


def call(calls: list, *words: int) -> None:
    if len(words) > CALL_WORDS:
        raise ValueError("call record overflow")
    calls.append([*words, *([0] * (CALL_WORDS - len(words)))])


def observation(machine: Machine, values: list[int], calls: list | None = None) -> dict:
    if calls is not None:
        values = [*values, len(calls), *(word for row in calls for word in row)]
    return {"values": values, "names": [f"word{i}" for i in range(len(values))],
            "stores": list(machine.stores)}


def state_vector(rng: random.Random, number: int) -> list[int]:
    return [number % 3, rng.choice((0, 208, 272, 273, 288, 289, 2072, 2073)),
            number % 4, rng.choice((0, 1, 7, 8, 47, 48, 450, 451)),
            rng.getrandbits(32), rng.choice((0, 8, 9, 47, 48, 49)),
            number % 3, rng.randrange(1_000_000_000), rng.randrange(101),
            rng.randrange(999), rng.choice((0, 1, -1)), rng.randrange(100),
            rng.choice((0, 1, -1)), rng.randrange(256), rng.randrange(16)]


def put_state(machine: Machine, state: list[int]) -> None:
    machine.map_zero(0x800F1C00, 0x100)
    for (_, address), value in zip(STATE, state, strict=True):
        machine.put(address, value)


def read_state(machine: Machine) -> list[int]:
    return [machine.load(address, signed=index not in UNSIGNED_STATE)
            for index, (_, address) in enumerate(STATE)]


def reels_vector(rng: random.Random) -> list[list[int]]:
    return [[rng.randrange(13, 18), rng.choice((0, 1, 2, 9, 10, 15, 99, 123, 32767))]
            for _ in range(48)]


def place_reels(machine: Machine, reels: list[list[int]]) -> None:
    for index, (stat, amount) in enumerate(reels):
        machine.store(REELS + index * 4, struct.pack("<hh", stat, amount))


def vector(identifier: str, rng: random.Random, number: int) -> dict:
    operation = SPECS[identifier][0]
    value = {"operation": operation}
    if operation == 5:
        value["misc"] = rng.choice((0, 123, 99_999, 999_899_999, 999_999_999, MASK,
                                   rng.getrandbits(32)))
        value["heals"] = rng.choice((0, 1, 999, 1000, 1001, 32767, 32768, 65535))
    elif operation == 1:
        value["music"] = number % 64 if number % 3 else rng.getrandbits(32)
    elif operation in (0, 2):
        value["state"] = state_vector(rng, number)
        if operation == 2:
            value["buttons"] = 0x20 if number % 3 == 0 else (0x10 if number % 3 == 1 else 0)
            value["reels"] = reels_vector(rng)
            value["delta"] = rng.randrange(1, 100)
            # Bias exactly the deceleration alignment, stop/hold/closing
            # boundaries and signed halfword reward, not only random states.
            if number % 16 == 1:
                value["state"][2:7] = [1, 47, 7, 8, number % 3]
            if number % 16 == 2:
                value["state"][1:6] = [2073, 0, 77, 248, 48]
                value["buttons"] = 0
            if number % 16 == 3:
                value["state"][2:7] = [3, 8, 248, 8, 0]
                value["reels"][15] = [17, -2]
            if number % 32 == 7:
                value["state"][1] = 0x7FFFFFFF
                value["state"][3] = 0x7FFFFFFF
            if number % 16 == 4:
                value["state"][2:4] = [2, 450]
                value["buttons"] = 0
        else:
            value.update(init_vector(rng, number))
    else:
        value.update(draw_vector(rng, number, operation))
    return value


def init_vector(rng: random.Random, number: int) -> dict:
    # ScoreRecord: titles, 6 kills, 10 weapons, 8 best trial words, max chain,
    # completion minutes, best room/chest count, streak, misc.
    record = [rng.getrandbits(16), *(rng.randrange(300) for _ in range(6)),
              *(rng.randrange(300) for _ in range(10)),
              *(rng.choice((0x800000, rng.getrandbits(32))) for _ in range(8)),
              rng.randrange(65), rng.choice((0, 299, 300, 539, 540, 599, 600)),
              rng.choice((0, 512, rng.randrange(513))),
              rng.choice((0, 64, rng.randrange(65))),
              rng.randrange(1_000_000), rng.getrandbits(32)]
    rooms = [rng.getrandbits(32) for _ in range(16)]
    masks = [rng.getrandbits(32) for _ in range(16)]
    rooms[1] |= 0x800000
    masks[1] |= 0xC00000
    masks[15] = MASK
    rooms[15] |= 0x80000000
    return {
        "record": record, "heals": rng.randrange(65536),
        "rooms": rooms, "masks": masks,
        "chests": [rng.choice((0, 1, 255)) for _ in range(63)] + [255],
        "clears": rng.randrange(256),
        "totals": [rng.choice((0, 1, 100, -1, 65535)) for _ in range(3)],
        "random": rng.getrandbits(32), "music_busy": number % 7 == 0,
        "queue_state": (number // 3) % 7 - 1 if number % 4 else 4,
        "file_bytes": 100352,
        "present": [rng.randrange(2) for _ in range(6)],
        "rects": [[rng.randrange(40), rng.randrange(40), rng.randrange(1, 65), 256]
                  for _ in range(6)],
        "palette_word": rng.getrandbits(32),
    }


def draw_vector(rng: random.Random, number: int, operation: int) -> dict:
    atlas = [[rng.randrange(256), rng.randrange(256), rng.randrange(256),
              rng.randrange(1, 25), rng.randrange(65536), rng.randrange(65536)]
             for _ in range(ATLAS_ENTRIES)]
    index = number % ATLAS_ENTRIES
    if number % 32 == 0:
        atlas[index][3] = 255
    elif number % 17 == 0:
        atlas[index][3] = 0
    result = {
        "atlas": atlas, "x": coordinate(rng),
        "y": rng.choice((180, 188, 189, 190, 195, 205, 206, 207, 220,
                         0x7FFFFFFF, -0x80000000)) if number % 4 else coordinate(rng),
        "index": index, "seed": rng.getrandbits(32), "link": rng.getrandbits(32),
    }
    if operation == 3:
        result.update(
            reels=reels_vector(rng), timer=rng.choice((-1, 0, 1, 7, 8, 63, 64, 65)),
            phase=number % 4, selection=rng.getrandbits(32),
            speed=rng.choice((0, 8, 48, MASK, rng.getrandbits(32))),
            reel=number % 3, frame=number % 2,
            clip=[rng.choice((-1, 0, 319, 320, 321)), rng.randrange(240), 320, 240])
    return result


def prepare(machine: Machine) -> None:
    machine.map_zero(0x8005E000, 0x4000)
    machine.map_zero(MASKS, 64)


def put_record(machine: Machine, record: list[int]) -> None:
    offsets = [(0, 4), *((4 + 2 * i, 2) for i in range(6)),
               *((0x14 + 2 * i, 2) for i in range(10)),
               *((0x28 + 12 * i, 4) for i in range(8)),
               (0x88, 2), (0x90, 4), (0x94, 4), (0x98, 4), (0x104, 4), (0x108, 4)]
    for (offset, size), item in zip(offsets, record, strict=True):
        machine.put(SCORE + offset, item, size)


def run_leaf(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    prepare(machine)
    if value["operation"] == 5:
        machine.put(SCORE + 0x108, value["misc"])
        machine.put(SCORE + 0x112, value["heals"], 2)
        machine.execute(entry)
        values = [machine.load(SCORE + 0x108), machine.load(SCORE + 0x112, 2)]
        return observation(machine, values), [5, value["misc"], value["heals"]]
    machine.put(CURRENT_MUSIC, value["music"])
    calls = []
    machine.stub(0x80045110, lambda m: calls.extend((m.r[4], m.r[5])))
    machine.execute(entry)
    return observation(machine, calls), [1, value["music"]]


def run_screen(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    prepare(machine)
    put_state(machine, value["state"])
    place_reels(machine, value["reels"])
    machine.put(BUTTONS, value["buttons"])
    calls = []
    draws = {
        0x80104B8C: (0, 3), 0x80104DBC: (1, 4), 0x8010516C: (2, 4),
        0x801053B0: (3, 3), 0x8010540C: (4, 3), 0x80104C40: (5, 4),
        0x80105020: (6, 4),
    }

    def draw(kind, arity):
        def endpoint(m):
            call(calls, 10 + kind, *(s32(m.r[4 + i]) for i in range(arity)))
            if kind in (1, 2):
                shown, counting = ((0x80109898, 0x801099EC) if kind == 1 else
                                   (0x80109894, 0x801099F0))
                m.store(shown, ((m.load(shown) + value["delta"]) & MASK).to_bytes(4, "little"))
                m.store(counting, (m.load(counting) ^ 1).to_bytes(4, "little"))
        return endpoint

    for address, (kind, arity) in draws.items():
        machine.stub(address, draw(kind, arity))
    machine.stub(0x801060A8, lambda m: call(
        calls, 17, *(s32(m.r[4 + i]) for i in range(4)),
        m.load(0x80109878), m.load(0x8010988C), m.load(0x80109890, signed=True)))
    machine.stub(0x801064D4, lambda m: call(calls, 18, *(s32(m.r[4 + i]) for i in range(4))))
    for address, kind, arity in ((0x80045988, 30, 2), (0x80045D64, 31, 2),
                                 (0x80045BFC, 32, 4), (0x80045C74, 33, 4)):
        machine.stub(address, lambda m, k=kind, n=arity:
                     call(calls, k, *(s32(m.r[4 + i]) for i in range(n))))
    for address, stat in ((0x80108F08, 13), (0x80109028, 14), (0x80108F98, 15),
                          (0x801090B8, 16), (0x80109148, 17)):
        machine.stub(address, lambda m, stat=stat: call(calls, 40, stat, s32(m.r[4])))
    machine.stub(0x8007E0A8, lambda m: call(calls, 41, *m.r[4:7]))
    machine.stub(0x80108EA0, lambda _m: call(calls, 42))
    machine.execute(entry)
    inputs = [2, *value["state"], value["buttons"], value["delta"],
              *(item for pair in value["reels"] for item in pair)]
    return observation(machine, [s32(machine.r[2]), *read_state(machine)], calls), inputs


def run_init(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    prepare(machine)
    put_state(machine, value["state"])
    put_record(machine, value["record"])
    machine.put(SCORE + 0x112, value["heals"], 2)
    for i, (rooms, mask) in enumerate(zip(value["rooms"], value["masks"], strict=True)):
        machine.put(MAP_FLAGS + 4 * i, rooms)
        machine.put(MASKS + 4 * i, mask)
    machine.store(FLAGS + 0x3C0, bytes(value["chests"]))
    machine.put(FLAGS, value["clears"], 1)
    machine.put(DIS_FILE + 4, value["file_bytes"])
    machine.map_zero(REQUEST, 0x3000)
    machine.map_zero(PALETTE, 0x100)
    machine.put(REQUEST, value["queue_state"], 2)
    machine.put(REQUEST_SLOT, REQUEST)
    machine.put(BUFFER_SLOT, ASSET)
    machine.put(PALETTE, value["palette_word"])
    for i, rect in enumerate(value["rects"]):
        machine.store(RECTANGLES + i * 8, struct.pack("<4h", *rect))
    machine.follow |= {0x80108E48, 0x80108AB0, 0x80108A0C}
    calls = []
    machine.stub(0x8007DFF0, lambda m: call(calls, 50, *m.r[4:7]))

    def allocate(m):
        call(calls, 51, m.r[4])
        m.r[2] = ASSET

    def slot(m):
        call(calls, 52, int(m.r[4] == DIS_FILE))
        m.r[2] = REQUEST

    def parse(m):
        offset = m.r[4] - ASSET
        if offset % TIM_STRIDE or not 0 <= offset // TIM_STRIDE < 3:
            raise Fault(f"TIM endpoint index {offset:#x}")
        index = offset // TIM_STRIDE
        call(calls, 54, index)
        fields = [0, RECTANGLES + (2 * index + 1) * 8,
                  PALETTE if value["present"][2 * index + 1] else 0,
                  RECTANGLES + 2 * index * 8,
                  ASSET + index * TIM_STRIDE if value["present"][2 * index] else 0]
        for i, field in enumerate(fields):
            m.store(m.r[5] + i * 4, field.to_bytes(4, "little"))

    def upload(m):
        rect = [m.load(m.r[4] + i * 2, 2, signed=True) for i in range(4)]
        palette = m.r[5] == PALETTE
        index = 0 if palette else (m.r[5] - ASSET) // TIM_STRIDE
        call(calls, 56 if palette else 55, index, *rect,
             m.load(PALETTE) if palette else 0)

    machine.stub(0x80043E3C, allocate)
    machine.stub(0x80044B10, slot)
    machine.stub(0x80044BC4, lambda m: call(
        calls, 53, int(m.r[4] == REQUEST), int(m.r[5] == ASSET),
        int(m.load(REQUEST_SLOT) == REQUEST), int(m.load(BUFFER_SLOT) == ASSET)))
    machine.stub(0x8008D820, parse)
    machine.stub(0x800288FC, upload)
    machine.stub(0x80044B80, lambda m: call(calls, 57, int(m.r[4] == REQUEST)))
    machine.stub(0x80103748, lambda _m: call(calls, 58))

    def poll(m):
        call(calls, 59)
        m.r[2] = int(value["music_busy"])

    def random_value(m):
        call(calls, 62)
        m.r[2] = value["random"]

    machine.stub(0x80045350, poll)
    machine.stub(0x80045000, lambda m: call(calls, 60, *m.r[4:7]))
    machine.stub(0x80043C60, lambda m: call(calls, 61, int(m.r[4] == ASSET)))
    machine.stub(0x80026EC4, random_value)
    for i, address in enumerate((0x80108EA8, 0x80108EC8, 0x80108EE8)):
        def total(m, i=i):
            call(calls, 63 + i)
            m.r[2] = value["totals"][i] & MASK
        machine.stub(address, total)
    machine.execute(entry)
    updated = [machine.load(SCORE + 0x108), machine.load(SCORE + 0x112, 2),
               machine.load(SCORE + 0x94), machine.load(SCORE + 0x98),
               *(machine.load(MAP_FLAGS + i * 4) for i in range(16)),
               *(machine.load(RECTANGLES + i * 2, 2, signed=True) for i in range(24)),
               machine.load(PALETTE)]
    inputs = [0, *value["state"], *value["record"], value["heals"], *value["rooms"],
              *value["masks"], *value["chests"], value["clears"], *value["totals"],
              value["random"], int(value["music_busy"]), value["queue_state"],
              value["file_bytes"], *value["present"],
              *(field for rect in value["rects"] for field in rect), value["palette_word"]]
    return observation(machine, [s32(machine.r[2]), *read_state(machine), *updated], calls), inputs


def run_reel(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    prepare(machine)
    place_atlas(machine, value["atlas"])
    place_reels(machine, value["reels"])
    for address, field in ((0x80109878, "selection"), (0x8010988C, "speed"),
                           (0x80109890, "reel")):
        machine.put(address, value[field])
    machine.put(FRAME, value["frame"])
    machine.store(DRAW_ENVS + ((value["frame"] + 1) & 1) * 92,
                  struct.pack("<4h", *value["clip"]))
    machine.map_zero(SCRATCH, 0x400)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, QUAD_ENTRY + 0x1C)
    calls = []

    def tint(m):
        call(calls, 0, *(s32(m.r[4 + i]) for i in range(3)), *m.bytes(m.r[7], 8))

    def area(m):
        call(calls, 3, *(m.load(m.r[5] + i * 2, 2, signed=True) for i in range(4)))

    def add_prim(m):
        call(calls, 4, s32(m.r[4] - QUAD_ENTRY), s32(m.r[5] - PRIMITIVES))

    def sprintf(m):
        if c_string(m, m.r[5]) != b"%d":
            raise Fault("unexpected reel format")
        text = str(s32(m.r[6])).encode()
        m.store(m.r[4], text + b"\0")
        m.r[2] = len(text)

    machine.stub(0x8010664C, tint)
    machine.stub(0x80107698, lambda m: call(calls, 1, *(s32(m.r[4 + i]) for i in range(3))))
    machine.stub(0x80105DD8, lambda m: call(
        calls, 2, *(s32(m.r[4 + i]) for i in range(4)), m.load(m.r[29] + 0x10)))
    machine.stub(0x800293DC, area)
    machine.stub(ADD_PRIMITIVE, add_prim)
    machine.stub(0x80026EF4, sprintf)
    machine.stub(0x80026E94, lambda m: m.r.__setitem__(2, len(c_string(m, m.r[4]))))
    machine.r[4:8] = [value[key] & MASK for key in ("x", "y", "timer", "phase")]
    machine.execute(entry)
    values = [machine.load(SCRATCH) - PRIMITIVES, *machine.bytes(COLORS, 8)]
    inputs = [3, *(value[key] for key in ("x", "y", "timer", "phase", "selection", "speed", "reel")),
              *value["clip"], *(field for row in value["reels"] for field in row),
              *(field for row in value["atlas"] for field in row)]
    return observation(machine, values, calls), inputs


def stale_word(seed: int, index: int) -> int:
    return (seed + index * 0x01020304) & MASK


def run_rows(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    place_atlas(machine, value["atlas"])
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(QUAD_ENTRY, 0x100)
    machine.map_zero(PRIMITIVES, CAPACITY * 40)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, QUAD_ENTRY + 0x1C)
    machine.put(QUAD_ENTRY, value["link"])
    for index in range(CAPACITY * 10):
        machine.put(PRIMITIVES + index * 4, stale_word(value["seed"], index))
    machine.follow.add(ADD_PRIMITIVE)
    machine.r[4:7] = [value[key] & MASK for key in ("x", "y", "index")]
    machine.execute(entry)
    count = (machine.load(SCRATCH) - PRIMITIVES) // 4
    values = [count, machine.load(QUAD_ENTRY),
              *(machine.load(PRIMITIVES + index * 4) for index in range(count))]
    inputs = [4, value["x"], value["y"], value["index"], value["seed"], value["link"],
              *(field for row in value["atlas"] for field in row)]
    return observation(machine, values), inputs


def allowed_stores(operation: int) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + 0x20)]
    if operation in (0, 2):
        allowed += [(address, address + 4) for _, address in STATE]
    if operation in (0, 5):
        allowed += [(SCORE + 0x108, SCORE + 0x10C), (SCORE + 0x112, SCORE + 0x114)]
    if operation == 0:
        allowed += [(SCORE + 0x94, SCORE + 0x9C), (MAP_FLAGS + 4, MAP_FLAGS + 8),
                    (REQUEST_SLOT, BUFFER_SLOT + 4), (RECTANGLES, RECTANGLES + 48),
                    (PALETTE, PALETTE + 4)]
    if operation == 3:
        allowed += [(COLORS, COLORS + 8), (SCRATCH, SCRATCH + 4)]
    if operation == 4:
        allowed += [(SCRATCH, SCRATCH + 4), (QUAD_ENTRY, QUAD_ENTRY + 4),
                    (PRIMITIVES, PRIMITIVES + CAPACITY * 40)]
    return allowed


def build_case(identifier: str) -> dict:
    operation, entry, end = SPECS[identifier]
    runner = {0: run_init, 1: run_leaf, 2: run_screen,
              3: run_reel, 4: run_rows, 5: run_leaf}[operation]

    def describe(image, results):
        verify_edits(EDITS[identifier], image)
        lines = [
            f"{identifier}: independently bounded {entry:#x}..{end:#x}, return delay slot included.",
            f"All {len(results)} vectors execute the original; invalid native-only contexts are Catch2 tests.",
            "All output fields, ordered endpoint arguments and original stores are checked.",
            "No filesystem/image access or optional disassembler imports occur during metadata discovery.",
        ]
        if operation == 0:
            lines += [
                "Original heal bonus, score calculation and rank instructions followed without doubles.",
                "Doubles: page reservation, heap/request allocation/enqueue/release, TIM parser/LoadImage, "
                "music queue/poll/play, rand and three actor-total getters. TIM rectangles/palette clear "
                "and handles passed to those endpoints are compared; CD I/O, TIM decoding and GPU uploads excluded.",
                f"Initializer phase counts: {[sum(v['state'][0] == phase for v, _, _ in results) for phase in range(3)]}.",
                "Queue Error/nonloaded remains pending; no retry/failure policy or #410 classification added.",
            ]
        elif operation == 1:
            lines += ["Only loadMusicSlot is doubled; its exact 32-bit track and slot arguments are compared."]
        elif operation == 2:
            lines += [
                "Doubles: all presentation/sound/reward/retirement/no-op endpoints. Counting draw doubles mutate "
                "the two readouts, proving callback writeback; GPU/audio/stat-application internals excluded.",
                f"Screen phase counts: {[sum(v['state'][2] == phase for v, _, _ in results) for phase in range(4)]}.",
                "Original completion is not latched. Production caller reentry remains a separate #410 observation.",
            ]
        elif operation == 3:
            lines += [
                "Doubles: tint/animated/fade glyph calls, SetDrawArea, AddPrim, sprintf(%d), strlen. "
                "Complete glyph/color/rectangle arguments, packet cursor and insertion addresses compared.",
                "SDK draw-area command encoding, nested glyph packets and GPU presentation excluded here.",
            ]
        elif operation == 4:
            lines += [
                "Actual resident SDK AddPrim instructions followed. No original endpoints doubled.",
                f"Compared {sum(o['values'][0] for _, o, _ in results)} complete packet words, including "
                "stale unwritten UV halves, packet cursor and ordering head. GPU rasterization excluded.",
            ]
        else:
            lines += ["No original endpoints doubled; misc-score word and unsigned heal-count halfword compared."]
        return lines

    return {
        "ID": identifier, "ENTRY": entry, "END": end,
        "MODULES": (MODULE, "SLUS_010.40"), "DRIVER": DRIVER, "SOURCES": SOURCES,
        "CXX_STANDARD": "c++latest", "VECTORS": 1024, "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 256, "MUTANTS": edit_mutants(EDITS[identifier]),
        "vector": lambda rng, number: vector(identifier, rng, number),
        "run": lambda machine, value: runner(machine, value, entry),
        "check": lambda _image, _value, original, native, _reference:
            printed_problems(original, native, allowed_stores(operation)),
        "describe": describe,
    }
