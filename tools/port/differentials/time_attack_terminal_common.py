"""Seven independently bounded MENUF terminal/dispatch originals.

Images and the optional disassembler are accessed only when executing a proof.
The R3000 interpreter, mutation validation and store checks are shared tools.
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
from differentials.results_drawing_common import edit_mutants, verify_edits

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800
DRIVER = "time_attack_terminal_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Results\TimeAttackTerminal.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
SPECS = {
    "F03011": (0, 0x8010310C, 0x80103530),
    "F03012": (1, 0x80103530, 0x80103748),
    "F03016": (2, 0x80103D88, 0x8010412C),
    "F03017": (3, 0x8010412C, 0x8010459C),
    "F03008": (4, 0x80102BB8, 0x80102C54),
    "F03009": (5, 0x80102C54, 0x80102D10),
    "F03014": (6, 0x801037A0, 0x8010384C),
}
STATE = (
    0x800F1CD8, 0x8010989C, 0x801098A0, 0x8010988C, 0x801098A4,
    0x801098A8, 0x80109894, 0x80109898, 0x80109878,
)
UNSIGNED_STATE = {3, 8}
FLAGS = 0x80061598
SCORES = 0x8005FEC8
BUTTONS = 0x8005E1D0
PALETTE_BUFFER = 0x801098AC
OUTER = 0x80250000
ASSET = 0x80200000
REQUEST = 0x80220000
RECTANGLES = 0x80221000
PALETTE = 0x80222000
FILES = 0x80102AE0
SLOTS = ((0x80109854, 0x80109858), (0x8010985C, 0x80109860))
TIM_STRIDE = 0x8220
COLORS = 160
CALL_WORDS = 16
INIT_LEAVES = {
    0x80102D10: 0, 0x80103530: 1, 0x8010310C: 2,
    0x80107A8C: 4, 0x80107D4C: 5, 0x80107D98: 6,
}
UPDATE_LEAVES = {
    0x8010384C: 0, 0x8010412C: 1, 0x80103D88: 2,
    0x80107FC0: 4, 0x801080E4: 5, 0x801083AC: 6,
}

EDITS = {
    "F03011": {
        "reserve-count": (0x80103130, 0x24050001, 0x24050002),
        "reserve-owner": (0x80103138, 0x24060005, 0x24060004),
        "file-size-index": (0x80103144, 0x8E04000C, 0x8E040014),
        "loaded-state": (0x80103194, 0x24020004, 0x24020003),
        "pixel-x": (0x801031BC, 0x24020340, 0x24020341),
        "pixel-height": (0x801031DC, 0x240200FF, 0x240200FE),
        "palette-width": (0x80103220, 0x240200A0, 0x2402009F),
        "palette-word-clear": (0x8010323C, 0xAC400000, 0),
        "palette-copy-size": (0x80103260, 0x24060140, 0x2406013C),
        "queue-music": (0x80103270, 0x0C040DD2, 0),
        "minutes-width": (0x801032B8, 0x90A200C5, 0x80A200C5),
        "zero-time-fallback": (0x801032E0, 0x34423B63, 0x34423B62),
        "record-comparison": (0x80103344, 0x0044102A, 0x0082102A),
        "round-mask": (0x80103480, 0x3042007F, 0x3042003F),
        "difficulty-bit": (0x801034B8, 0x00031FC0, 0x00031F80),
        "rank-count": (0x801034CC, 0x29A20003, 0x29A20002),
        "reset-animation": (0x801034FC, 0xAC40988C, 0),
        "complete-result": (0x80103510, 0x24020001, 0x24020000),
    },
    "F03012": {
        "reserve-count": (0x80103558, 0x24050002, 0x24050001),
        "file-index": (0x80103578, 0x26040010, 0x26040008),
        "loaded-state": (0x801035CC, 0x24020004, 0x24020003),
        "pixel-height": (0x80103624, 0x240200FF, 0x240200FE),
        "first-palette-only": (0x8010363C, 0x16000019, 0),
        "palette-width": (0x80103670, 0x240200A0, 0x2402009F),
        "palette-clear": (0x8010368C, 0xAC400000, 0),
        "tim-count": (0x801036AC, 0x2A020002, 0x2A020001),
        "pixel-column-stride": (0x801036B4, 0x26520040, 0x2652003F),
        "free-call": (0x801036E4, 0x0C010F18, 0),
        "complete-result": (0x801036EC, 0x24020001, 0x24020000),
        "reset-readout": (0x801036F4, 0xAC609894, 0),
        "reset-selection": (0x8010371C, 0xAC609878, 0),
        "reset-caller-phase": (0x80103724, 0xAE201CD8, 0),
    },
    "F03016": {
        "pulse-divisor": (0x80103DC0, 0x00022203, 0x000221C3),
        "pulse-step": (0x80103DD0, 0x24420040, 0x2442003F),
        "pulse-mask": (0x80103DD4, 0x30420FFF, 0x304207FF),
        "count-sound-time": (0x80103DE8, 0x24020030, 0x2402002F),
        "time-delay": (0x80103E48, 0x24C6FFE0, 0x24C6FFE1),
        "rating-delay": (0x80103E5C, 0x24C6FFA0, 0x24C6FFA1),
        "record-delay": (0x80103E80, 0x24C6FF80, 0x24C6FF81),
        "record-sound": (0x80103EA8, 0x24050076, 0x24050075),
        "last-rank": (0x80103F3C, 0x24060002, 0x24060001),
        "timer-step": (0x80103F54, 0x24630001, 0x24630002),
        "confirm-mask": (0x80103F58, 0x30420020, 0x30420010),
        "reveal-end": (0x80103F64, 0x286200C0, 0x286200BF),
        "hold-end": (0x80104024, 0x286201C2, 0x286201C1),
        "fade-timer": (0x80104050, 0x24030007, 0x24030008),
        "closing-zero": (0x801040DC, 0x18400003, 0x04400003),
        "retire-count": (0x80104100, 0x24050001, 0x24050002),
        "complete-result": (0x80104110, 0x24020001, 0x24020000),
    },
    "F03017": {
        "last-intro-sound": (0x8010417C, 0x2882002E, 0x2882002D),
        "course-delay": (0x801041CC, 0x24C6FFF1, 0x24C6FFF2),
        "boss-delay": (0x801041E0, 0x24C6FFE2, 0x24C6FFE3),
        "best-time-delay": (0x801041F4, 0x24C6FFD3, 0x24C6FFD4),
        "confirm-mask": (0x80104208, 0x30420020, 0x30420010),
        "intro-end": (0x80104228, 0x2862005A, 0x28620059),
        "intro-fade": (0x80104240, 0x24020008, 0x24020007),
        "fade-decrement": (0x80104268, 0x24C6FFFF, 0x24C6FFFE),
        "countdown-speed": (0x801042E8, 0x24040004, 0x24040003),
        "countdown-color": (0x8010430C, 0x24849724, 0x24849728),
        "enable-effect": (0x80104334, 0x24040001, 0x24040000),
        "countdown-sound-range": (0x80104390, 0x28620003, 0x28620002),
        "go-scale": (0x8010442C, 0x24849700, 0x248496F0),
        "neutral-time": (0x80104448, 0x24020037, 0x24020036),
        "go-fade-base": (0x80104494, 0x24020068, 0x24020067),
        "countdown-offset-limit": (0x801044E0, 0x28E20008, 0x28E20006),
        "countdown-x": (0x80104504, 0x24E400A0, 0x24E4009F),
        "countdown-end": (0x80104524, 0x2842003E, 0x2842003D),
        "restored-speed": (0x80104554, 0x24040002, 0x24040003),
        "retire-count": (0x80104570, 0x24050002, 0x24050001),
        "complete-result": (0x80104580, 0x24020001, 0x24020000),
    },
    "F03008": {
        "initial-phase": (0x80102C04, 0xA2240000, 0),
        "init-callee": (0x80102C0C, 0x0C040B15, 0x0C040DE8),
        "nonzero-result": (0x80102C14, 0x0002102B, 0x0002102A),
        "complete-result": (0x80102C30, 0x24020001, 0x24020000),
        "clear-phase": (0x80102C38, 0xA2200000, 0),
    },
    "F03009": {
        "dispatch-count": (0x80102C60, 0x2C620007, 0x2C620006),
        "game-clear-callee": (0x80102C8C, 0x0C040B44, 0x0C040D4C),
        "start-callee": (0x80102C9C, 0x0C040D4C, 0x0C040C43),
        "results-callee": (0x80102CAC, 0x0C040C43, 0x0C040B44),
        "heal-call": (0x80102CBC, 0x0C042392, 0),
        "finish-call": (0x80102CC4, 0x0C0423A8, 0),
        "score-only-result": (0x80102CD0, 0x24040001, 0x24040000),
        "cube-start-callee": (0x80102CD4, 0x0C041EA3, 0x0C041F53),
        "cube-results-callee": (0x80102CE4, 0x0C041F53, 0x0C041F66),
        "cube-quit-callee": (0x80102CF4, 0x0C041F66, 0x0C041EA3),
    },
    "F03014": {
        "dispatch-count": (0x801037AC, 0x2C620007, 0x2C620006),
        "game-clear-callee": (0x801037D8, 0x0C040E13, 0x0C04104B),
        "start-callee": (0x801037E8, 0x0C04104B, 0x0C040F62),
        "results-callee": (0x801037F8, 0x0C040F62, 0x0C040E13),
        "score-only-result": (0x8010380C, 0x24040001, 0x24040000),
        "cube-start-callee": (0x80103810, 0x0C041FF0, 0x0C042039),
        "cube-results-callee": (0x80103820, 0x0C042039, 0x0C0420EB),
        "cube-quit-callee": (0x80103830, 0x0C0420EB, 0x0C041FF0),
    },
}


def call(calls: list, tag: int, *values: int) -> None:
    words = [tag, *values]
    if len(words) > CALL_WORDS:
        raise ValueError("terminal endpoint record overflow")
    calls.append(words + [0] * (CALL_WORDS - len(words)))


def observation(machine: Machine, values: list[int], calls: list) -> dict:
    values = [*values, len(calls), *(word for row in calls for word in row)]
    return {"values": values, "names": [f"word{i}" for i in range(len(values))],
            "stores": list(machine.stores)}


def state_vector(rng: random.Random) -> list[int]:
    return [rng.randrange(3), rng.randrange(800), rng.randrange(4),
            rng.getrandbits(32), rng.randrange(4), rng.randrange(0x1000000),
            rng.randrange(1000), rng.randrange(1000), rng.getrandbits(32)]


def load_vector(rng: random.Random, number: int, operation: int) -> dict:
    state = state_vector(rng)
    state[0] = number % 3
    fields = [rng.randrange(8), *[rng.choice((0, 1, 59, 99, 127, 128, 255))
                                  for _ in range(3)],
              rng.randrange(256), rng.randrange(256)]
    if number % 11 == 0:
        fields[1:4] = [0, 0, 0]
    time = (fields[1] << 16 | fields[2] << 8 | fields[3]) or 0x3B3B63
    modes = [
        [time, min(time + 1, 0xFFFFFF), min(time + 2, 0xFFFFFF)],
        [max(0, time - 1), time, min(time + 1, 0xFFFFFF)],
        [0, 1, time],
        [0, 1, max(0, time - 1)],
        sorted(rng.randrange(0x1000000) for _ in range(3)),
    ]
    scores = [rng.getrandbits(32) for _ in range(24)]
    for rank, record in enumerate(modes[(number // 3) % len(modes)]):
        scores[fields[0] * 3 + rank] = record | rng.randrange(256) << 24
    return {
        "operation": operation, "state": state,
        "queue": rng.choice((0, 1, 2, 3, 4, 4, 4, 5, 6, 7, -1)),
        "busy": rng.randrange(3) == 0, "bytes": rng.choice((34816, 67584, 81920)),
        "fields": fields, "scores": scores,
        "present": [rng.randrange(2) for _ in range(4)],
        "rects": [[rng.randrange(40), rng.randrange(30),
                   rng.choice((1, 61, 64, 127, 160, 256)), rng.randrange(1, 257)]
                  for _ in range(4)],
        "palette": [rng.randrange(65536) for _ in range(COLORS)],
        "saved": [rng.randrange(65536) for _ in range(COLORS)],
    }


RESULT_TIMERS = (-1, 0, 1, 7, 8, 47, 48, 49, 95, 96, 97, 127, 128, 129,
                 159, 160, 168, 176, 190, 191, 192, 448, 449, 450,
                 0x7FFFFFFF, -0x80000000)
START_TIMERS = (
    (0, 14, 15, 30, 44, 45, 46, 88, 89, 90, 0x7FFFFFFF, -0x80000000, -15),
    (0, 1, 2, 7, 8, -1, 0x7FFFFFFF, -0x80000000),
    (0, 77, -1),
    (0, 1, 6, 7, 8, 14, 15, 29, 30, 44, 45, 46, 54, 55, 59, 60, 61, 62,
     0x7FFFFFFF, -15),
    (0, 62, 99, -1),
)


def vector(identifier: str, rng: random.Random, number: int) -> dict:
    operation = SPECS[identifier][0]
    if operation < 2:
        return load_vector(rng, number, operation)
    if operation < 4:
        state = state_vector(rng)
        phases = 4 if operation == 2 else 5
        state[2] = number % phases
        timers = RESULT_TIMERS if operation == 2 else START_TIMERS[state[2]]
        state[1] = timers[(number // phases) % len(timers)]
        if state[1] == 128:
            state[4] = 0
        return {"operation": operation, "state": state,
                "buttons": rng.choice((0, 0, 0, 0x20, 0x10, rng.getrandbits(32)))}
    return {
        "operation": operation,
        "outer": (0, 1, 2, 3, 255)[number % 5] if operation == 4 else rng.randrange(256),
        "selector": (number // 5 if operation == 4 else number) % 7,
        "result": rng.choice((0, 0, 1, -1, 0x7FFFFFFF, -0x80000000, s32(rng.getrandbits(32)))),
        "mutation": rng.choice((-1, 0, 1, 2, 255, rng.randrange(256))),
    }


def prepare(machine: Machine, state: list[int] | None = None) -> None:
    machine.map_zero(0x8005E000, 0x4000)
    machine.map_zero(0x800F1C00, 0x100)
    if state is not None:
        for address, word in zip(STATE, state, strict=True):
            machine.put(address, word)


def read_state(machine: Machine) -> list[int]:
    return [machine.load(address, signed=index not in UNSIGNED_STATE)
            for index, address in enumerate(STATE)]


def load_endpoints(machine: Machine, value: dict, calls: list) -> None:
    operation = value["operation"]
    request_slot, buffer_slot = SLOTS[operation]
    file_address = FILES + (operation + 1) * 8
    machine.stub(0x8007DFF0, lambda m: call(calls, 50, *m.r[4:7]))

    def allocate(m):
        call(calls, 51, m.r[4])
        m.r[2] = ASSET

    def request(m):
        call(calls, 52, (m.r[4] - FILES) // 8)
        m.r[2] = REQUEST

    def parse(m):
        offset = m.r[4] - ASSET
        if offset % TIM_STRIDE or not 0 <= offset // TIM_STRIDE < 2:
            raise Fault(f"TIM offset outside fixture: {offset:#x}")
        index = offset // TIM_STRIDE
        call(calls, 54, index)
        fields = [0, RECTANGLES + (2 * index + 1) * 8,
                  PALETTE if value["present"][2 * index + 1] else 0,
                  RECTANGLES + 2 * index * 8,
                  ASSET + index * TIM_STRIDE if value["present"][2 * index] else 0]
        for field, word in enumerate(fields):
            m.store(m.r[5] + field * 4, word.to_bytes(4, "little"))

    def upload(m):
        area = [m.load(m.r[4] + index * 2, 2, signed=True) for index in range(4)]
        palette = m.r[5] == PALETTE
        index = 0 if palette else (m.r[5] - ASSET) // TIM_STRIDE
        call(calls, 56 if palette else 55, index, *area,
             m.load(PALETTE) if palette else 0)

    def copy(m):
        call(calls, 57, int(m.r[4] == PALETTE_BUFFER), int(m.r[5] == PALETTE), m.r[6])
        m.store(m.r[4], m.bytes(m.r[5], m.r[6]))

    def poll(m):
        call(calls, 60)
        m.r[2] = int(value["busy"])

    machine.stub(0x80043E3C, allocate)
    machine.stub(0x80044B10, request)
    machine.stub(0x80044BC4, lambda m: call(
        calls, 53, int(m.r[4] == REQUEST), int(m.r[5] == ASSET),
        int(m.load(request_slot) == REQUEST), int(m.load(buffer_slot) == ASSET)))
    machine.stub(0x8008D820, parse)
    machine.stub(0x800288FC, upload)
    machine.stub(0x800490B0, copy)
    machine.stub(0x80044B80, lambda m: call(calls, 58, int(m.r[4] == REQUEST)))
    machine.stub(0x80103748, lambda _m: call(calls, 59))
    machine.stub(0x80045350, poll)
    machine.stub(0x80045000, lambda m: call(calls, 61, *m.r[4:7]))
    machine.stub(0x80043C60, lambda m: call(calls, 62, int(m.r[4] == ASSET), *read_state(m)))
    machine.put(file_address + 4, value["bytes"])
    machine.put(FILES + (2 - operation) * 8 + 4, value["bytes"] + 2048)


def run_load(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    prepare(machine, value["state"])
    machine.map_zero(REQUEST, 0x3000)
    machine.put(REQUEST, value["queue"], 2)
    for slot, pointer in zip(SLOTS[value["operation"]], (REQUEST, ASSET), strict=True):
        machine.put(slot, pointer)
    for offset, word in zip((0xC4, 0xC5, 0xC6, 0xC7, 0, 1), value["fields"], strict=True):
        machine.put(FLAGS + offset, word, 1)
    for index, score in enumerate(value["scores"]):
        machine.put(SCORES + index * 4, score)
    for index, area in enumerate(value["rects"]):
        machine.store(RECTANGLES + index * 8, struct.pack("<4h", *area))
    for address, key in ((PALETTE, "palette"), (PALETTE_BUFFER, "saved")):
        machine.store(address, struct.pack(f"<{COLORS}H", *value[key]))
    calls = []
    load_endpoints(machine, value, calls)
    machine.execute(entry)
    values = [s32(machine.r[2]), *read_state(machine),
              *(machine.load(SCORES + index * 4) for index in range(24)),
              *(machine.load(RECTANGLES + index * 2, 2, signed=True) for index in range(16)),
              *(machine.load(PALETTE + index * 2, 2) for index in range(COLORS)),
              *(machine.load(PALETTE_BUFFER + index * 2, 2) for index in range(COLORS))]
    inputs = [value["operation"], *value["state"], value["queue"], int(value["busy"]),
              value["bytes"], *value["fields"], *value["scores"], *value["present"],
              *(word for area in value["rects"] for word in area),
              *value["palette"], *value["saved"]]
    return observation(machine, values, calls), inputs


DRAW_LINES = {
    0x8010559C: 0, 0x8010581C: 1, 0x801056E8: 2, 0x801059B8: 3,
    0x80105B30: 4, 0x80105790: 5, 0x8010564C: 6, 0x801059FC: 7,
    0x8010459C: 8, 0x80104650: 9, 0x801046F8: 10, 0x801047D4: 11,
    0x8010489C: 12,
}


def run_screen(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    prepare(machine, value["state"])
    machine.put(BUTTONS, value["buttons"])
    machine.follow.add(0x80040F28)
    calls = []
    machine.stub(0x80104914, lambda m: call(calls, 10, s32(m.r[4])))
    for address, kind in DRAW_LINES.items():
        def draw(m, kind=kind):
            timer, rank = (m.r[7], m.r[6]) if kind == 4 else (m.r[6], 0)
            call(calls, 20, kind, s32(m.r[4]), s32(m.r[5]), s32(timer), s32(rank))
        machine.stub(address, draw)
    for address, tag in ((0x80045988, 40), (0x80045D64, 41)):
        machine.stub(address, lambda m, tag=tag: call(calls, tag, *m.r[4:6]))
    machine.stub(0x8007E0A8, lambda m: call(calls, 63, *m.r[4:7]))
    for address, tag in ((0x8007C36C, 70), (0x8007DDAC, 71), (0x8007DE2C, 75),
                          (0x8007DE44, 76), (0x8007DE5C, 77), (0x8007DD50, 78)):
        machine.stub(address, lambda m, tag=tag: call(calls, tag, s32(m.r[4])))
    for address, tag in ((0x8007DDB8, 72), (0x8007DDF8, 74)):
        machine.stub(address, lambda m, tag=tag: call(
            calls, tag, m.load(m.r[4], signed=True), m.load(m.r[4] + 4, signed=True)))
    machine.stub(0x8007DDD4, lambda m: call(
        calls, 73, *(m.load(m.r[4] + channel, 1) for channel in range(3))))
    machine.execute(entry)
    return observation(machine, [s32(machine.r[2]), *read_state(machine)], calls), [
        value["operation"], *value["state"], value["buttons"]]


def run_dispatch(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    prepare(machine)
    machine.map_zero(OUTER, 4)
    machine.put(OUTER, value["outer"], 1)
    machine.put(OUTER + 1, 0xAA, 1)
    machine.put(FLAGS + 0xB6, value["selector"], 1)
    machine.r[4] = OUTER
    calls = []

    def endpoint(m, tag, selector):
        outer = m.load(OUTER, 1) if value["operation"] == 4 else -1
        call(calls, tag, selector, outer)
        if value["operation"] == 4 and value["mutation"] >= 0:
            m.store(OUTER, bytes([value["mutation"]]))
        m.r[2] = value["result"] & MASK

    for leaves, tag in ((INIT_LEAVES, 90), (UPDATE_LEAVES, 91)):
        for address, selector in leaves.items():
            machine.stub(address, lambda m, tag=tag, selector=selector:
                         endpoint(m, tag, selector))
    machine.stub(0x80108E48, lambda m: endpoint(m, 92, 3))
    machine.stub(0x80108EA0, lambda m: endpoint(m, 93, 3))
    machine.follow |= {0x80102C54, 0x801037A0}
    machine.execute(entry)
    return observation(machine, [s32(machine.r[2]), machine.load(OUTER, 1),
                                  machine.load(OUTER + 1, 1)], calls), [
        value["operation"], value["outer"], value["selector"],
        value["result"], value["mutation"]]


def allowed_stores(operation: int) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    indices = {
        0: (0, 1, 2, 3, 4, 5), 1: (0, 1, 2, 3, 6, 7, 8),
        2: (1, 2, 3), 3: (1, 2),
    }.get(operation, ())
    allowed += [(STATE[index], STATE[index] + 4) for index in indices]
    if operation < 2:
        allowed += [(SLOTS[operation][0], SLOTS[operation][1] + 4),
                    (RECTANGLES, RECTANGLES + 32), (PALETTE, PALETTE + 4)]
    if operation == 0:
        allowed.append((SCORES, SCORES + 96))
    if operation == 4:
        allowed.append((OUTER, OUTER + 1))
    return allowed


def build_case(identifier: str) -> dict:
    operation, entry, end = SPECS[identifier]
    runner = run_load if operation < 2 else run_screen if operation < 4 else run_dispatch

    def describe(image, results):
        verify_edits(EDITS[identifier], image)
        if image.read(end - 8) != 0x03E00008:
            raise ValueError("independently bounded body no longer ends in JR RA")
        lines = [
            f"{identifier}: original {entry:#x}..{end:#x}, return and delay slot included.",
            f"Compared all {len(results)} native vectors with original R3000 instructions.",
            "Observed final state and ordered endpoint arguments are compared; original stores are restricted "
            "to declared field, fixture and stack ranges.",
            "Native-only invalid selectors, owners, table bounds and effect allocation are Catch2 refusal tests.",
            "Metadata import performs no image access and does not import the optional disassembler.",
        ]
        if operation < 2:
            lines += [
                "Doubles: page reservation, heap/request allocation/enqueue/free, TIM parse/image upload, "
                "palette memcpy and music queue/poll/play. Rectangles, all 160 source and copied palette "
                "halfwords, complete 24 score words and controller state at the free callback are compared.",
                "TIM decoding, physical I/O, audio and rasterization are outside this original proof.",
                "Original Loaded alone advances. Error remains pending; #410 is not classified.",
            ]
        elif operation < 4:
            if operation == 2:
                lines.append("Actual resident rsin instructions run without a double.")
            lines += [
                "Doubles: existing heading/time/rating/leaderboard/palette endpoints, audio, tick-speed, "
                "framebuffer-effect setters and page retirement. Setter pairs/tints compare every consumed "
                "field; unused source-vector z/reserved words are not part of the setter contract.",
                "No GPU rasterization, audio playback or framebuffer packet allocation in this proof.",
                "Repeated completion semantics are unchanged; actual Live owner behavior is tested separately.",
            ]
        else:
            lines += [
                "Doubles: six presentation initialize/update leaves plus healing/no-op endpoints.",
                (
                    "F03008 follows original F03009/F03014 dispatch instructions and compares callback-visible "
                    "phase, callback writes, signed full-word child returns and the untouched adjacent byte."
                    if operation == 4 else
                    "Direct dispatch checks branch selection, signed full-word child returns and healing/no-op "
                    "call order; the unrelated outer-byte fixture remains unchanged."
                ),
                "No claim that doubled game-clear/cube-puzzle internals have executed in this comparison.",
            ]
        return lines

    return {
        "ID": identifier, "ENTRY": entry, "END": end,
        "MODULES": (MODULE, "SLUS_010.40"), "DRIVER": DRIVER, "SOURCES": SOURCES,
        "INCLUDES": (r"Native\Tests\include",),
        "CXX_STANDARD": "c++latest", "VECTORS": 1024, "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 256, "MUTANTS": edit_mutants(EDITS[identifier]),
        "vector": lambda rng, number: vector(identifier, rng, number),
        "run": lambda machine, value: runner(machine, value, entry),
        "check": lambda _image, _value, original, native, _reference:
            printed_problems(original, native, allowed_stores(operation)),
        "describe": describe,
    }
