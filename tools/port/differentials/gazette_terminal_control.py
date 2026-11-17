"""F02803/F04156 boundary fixtures; original instructions own all transitions.

Each external service is doubled, never the controller under proof. Observations
include return value, all original local/module state, persistent cursor bytes,
textbox coordinates and guards, MON aliases/records, and ordered endpoint
payloads with the state visible at render and sound calls.
"""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, DEFAULT_STACK_SIZE, Fault, Machine, s32
from gazette_terminal_common import (
    CONTROLLER,
    CURSOR_MEMORY,
    DESCRIPTION_POINTER,
    DESCRIPTIONS,
    ENEMIES,
    MASK,
    MON_DATA,
    SCREEN,
    enemies,
    enemy_observation,
    flatten,
    observe,
    setup_enemies,
    text,
    word,
)

REQUEST, BOX, ACTOR, OUTER_PHASE = 0x80307000, 0x80308000, 0x80309000, 0x8030B000
BOX_BYTES = 40
REQUEST_POINTER, FRAMEWORK_CURSOR = 0x8010A46C, 0x800F4EE8
RANK_TABLE, RANK, FILE_DESCRIPTOR = 0x80108564, 0x8010A464, 0x80102818
ROWS = 0x8010A230
ACTOR_POINTER, ACTOR_HEIGHT, ACTOR_DISTANCE = 0x800F453C, 0x63E, 0x656
PRESSED, REPEATED, HELD, RELEASED, STICKS = (
    0x8005E1D0, 0x8005DFDC, 0x8005E1C0, 0x80055C78, 0x8005DFC0)
HUD_OFFSET, FADE_STEP, CURRENT_MENU, HIDE_MENU = (
    0x800EB9B0, 0x801022D8, 0x800F51C0, 0x801022D6)
SCREEN_FIELDS = (0x8010A214, 0x8010A218, 0x8010A21C)
STATE_FIELDS = (
    (0x8010A43C, 4, True), (0x8010A440, 4, True), (0x8010A220, 4, True),
    (0x8010A430, 2, True), (0x8010A432, 2, True), (0x8010A434, 2, True),
    (0x8010A444, 4, True), (0x8010A44E, 2, True), (0x8010A44C, 2, False),
    (0x8010A450, 2, True), (0x8010A452, 2, True), (0x8010A438, 4, True),
    (0x8010A448, 4, True), (0x8010A45C, 4, True),
    (0x8010A458, 4, False), (0x8010A454, 4, False),
)
PHASES = (*range(16), 19, 20, 20, 20, 21, 25, 26, 27, -1, 16, 24, 28)
PAGE_CREATE = {0x801056B8: 0, 0x80106780: 1, 0x80106F9C: 2, 0x80105D8C: 3}
PAGE_DRAW = {0x8010579C: 0, 0x80106808: 1, 0x80107120: 2, 0x80105F00: 3}


def vector(rng: random.Random, number: int, mode: int) -> dict:
    if mode == SCREEN:
        phase = (3, 4, 4, 4, 5, 6, 6, 7, 8, 8, 0, 255)[number % 12]
        turn = number // 12
        return {
            "mode": mode,
            "state": [phase, (0, 1, 8, -1, -0x80000000)[turn % 5],
                      (0, 1, -1)[turn % 3], (0, 1, 2, -1)[turn % 4]],
            "environment": [word(rng, number), (0, 1, -1)[turn % 3],
                            rng.randrange(256), rng.randrange(256)],
            "replies": [(0, 1, -1)[(turn // 2) % 3],
                        (0, 1, -1, 257, 256, 0x7FFFFFFF)[turn % 6],
                        (0, 1, 2, -1)[(turn // 2) % 4]],
        }
    phase = PHASES[number % len(PHASES)]
    turn = number // len(PHASES) + number % 3
    step = (0, 1, 7, 8, 9, -1, -0x80000000, 0x7FFFFFFF)[turn % 8]
    state = [
        phase, turn % 3, step,
        rng.choice((-32768, -1, 0, 63, 64, 1024, 32767)),
        rng.randrange(-32768, 32768), rng.randrange(-32768, 32768),
        rng.choice((0, 639, 640, 1000, 6144, 6145, -0x80000000, 0x7FFFFFFF)),
        rng.randrange(-32768, 32768), rng.randrange(65536),
        number % 78, rng.choice((-32768, -1, 0, 0, 1, 2, 3, 32767)),
        word(rng, number), (0, 4, 8, 1, 2)[turn % 5], turn % 2,
        rng.getrandbits(32), rng.getrandbits(32),
    ]
    if phase in (25, 26, 27):
        state[12] = (1, 2)[turn % 2]
    cursor = []
    for last in (24, 70, 15):
        cursor += [(0, last, rng.randrange(last + 1))[turn % 3],
                   (0, 7, 1, 2, 5, 6)[(turn // 3) % 6]]
    cursor += [turn % 8, turn % 4]
    pressed = (0, 0x1000, 0x4000, 0x5000, 0x40, 0x10, 0x50, 0x20,
               0x60, 0x30, 0x100, 0x140, 4, 8, 0x48, 0x44)[turn % 16]
    repeated = (0, 0x1000, 0x4000, 0x5000, 4, 8, 12, 0x1004, 0x4008)[turn % 9]
    if phase in (5, 8, 14):
        cursor_offset, last = {5: (0, 24), 8: (4, 15), 14: (2, 70)}[phase]
        navigation = (
            (0, 0, 0x1000, 0x4000), (last, 7, 0x4000, 0x1000),
            (0, 0, 0, 0x1000), (last, 7, 0, 0x4000),
            (0, 1, 0, 0x1000), (1, 1, 0, 0x1000),
            (2, 2, 0, 0x5000), (last, 6, 0, 0x4000),
            (1, 6, 0, 0x4000), (0, 5, 0, 0x4000),
        )[turn % 10]
        cursor[cursor_offset:cursor_offset + 2] = navigation[:2]
        pressed |= navigation[2]
        repeated = navigation[3]
    held = (0, 1, 2, 3, 0x1001, 0x4001, 0x8001, 0x2001, 0x5001, 0xA001,
            0x1002, 0x4002, 0x5002, 4, 8, 12, 10)[turn % 17]
    axes = (0, 63, 64, 65, 127, 128, 191, 192, 193, 255)
    if turn % 3:
        sticks = [128, 128, axes[(turn // 3) % len(axes)], axes[turn % len(axes)]]
    else:
        sticks = [axes[(turn // 3) % len(axes)], axes[turn % len(axes)], 128, 128]
    if phase == 20:
        controls = (
            (2, 0, 0, 128, 128), (3, 255, 255, 128, 128),
            (0x1001, 64, 192, 128, 128), (0x4001, 128, 128, 128, 128),
            (0xA001, 128, 128, 128, 128), (0x1002, 128, 128, 128, 128),
            (0x4002, 128, 128, 128, 128), (2, 128, 128, 128, 0),
            (2, 128, 128, 128, 255), (0x1002, 128, 128, 0, 128),
            (1, 128, 128, 0, 0), (1, 128, 128, 255, 255),
        )[turn % 12]
        held = (held & 12) | controls[0]
        sticks = list(controls[1:])
        if turn % 12 in (2, 3):
            state[3] = 512
        if 5 <= turn % 12 <= 9:
            state[6] = 1024
    rows = enemies(rng, number)
    if turn % 7 == 0:
        rows[state[9]][13:15] = [state[9], state[9]]
    present = phase not in (3, 21) or turn % 2 == 0
    file_pointer = ENEMIES if phase >= 13 else 0
    request_pointer = REQUEST if phase == 13 else 0
    description_pointer = DESCRIPTIONS if phase > 13 else 0
    return {
        "mode": mode, "state": state, "pad": rng.randrange(65536), "cursor": cursor,
        "input": [pressed, repeated, held, (0, 4, 8, 12, MASK)[turn % 5], *sticks],
        "rows": rows, "box": [rng.randrange(256) for _ in range(BOX_BYTES)],
        "delays": [0 if (turn + index) % 3 == 0 else rng.randrange(1, 256)
                   for index in range(8)],
        "host": [(-3, -2, -1, 0, 1, 2, 3)[turn % 7], (turn + 1) % 4,
                 (0, 159, 160, 1536, 1537, 32767, 32768, 65535)[turn % 8],
                 rng.randrange(-32768, 32768), (0, 0, 1, -1)[turn % 4],
                 (0, 1, 2, 3, 4, -1)[turn % 6], rng.randrange(1 << 20),
                 rng.randrange(7000, 16000), int(present), turn % 16,
                 file_pointer, request_pointer, description_pointer],
    }


def state_words(machine: Machine) -> list[int]:
    return [machine.load(address, size, signed) for address, size, signed in STATE_FIELDS]


def cursor_bytes(machine: Machine) -> list[int]:
    return list(machine.bytes(CURSOR_MEMORY, 8))


def setup_controller(machine: Machine, value: dict) -> None:
    setup_enemies(machine, value)
    for (address, size, _), item in zip(STATE_FIELDS, value["state"], strict=True):
        machine.put(address, item, size)
    machine.put(0x8010A436, value["pad"], 2)
    machine.store(CURSOR_MEMORY, bytes(value["cursor"]))
    machine.store(BOX, bytes(value["box"]))
    for index, delay in enumerate(value["delays"]):
        machine.put(ROWS + index * 16 + 2, delay, 1)
    for address, item in zip((PRESSED, REPEATED, HELD, RELEASED), value["input"][:4], strict=True):
        machine.put(address, item)
    for index, item in enumerate(value["input"][4:]):
        machine.put(STICKS + index * 4, item)
    host = value["host"]
    machine.put(FRAMEWORK_CURSOR + 2, host[1], 1)
    machine.put(FRAMEWORK_CURSOR + 3, 0, 1)
    machine.put(ACTOR_POINTER, ACTOR)
    machine.put(ACTOR + ACTOR_DISTANCE, host[2], 2)
    machine.put(ACTOR + ACTOR_HEIGHT, host[3], 2)
    machine.put(REQUEST, host[5], 2)
    machine.put(FILE_DESCRIPTOR, host[6])
    machine.put(FILE_DESCRIPTOR + 4, host[7])
    machine.put(RANK, host[9], 2)
    for index in range(16):
        offset = 32 + index * 4
        machine.put(RANK_TABLE + index * 2, offset, 2)
        machine.store(RANK_TABLE + offset * 2, bytes((0x10 + index, 0x20, 0xE7)))
    machine.put(MON_DATA, host[10])
    machine.put(REQUEST_POINTER, host[11])
    machine.put(DESCRIPTION_POINTER, host[12])


class ControllerRecorder:
    def __init__(self, machine: Machine, value: dict) -> None:
        self.calls: list[list[int]] = []
        self.value = value
        simple = {
            0x80100414: (20, 2), 0x800F9A24: (21, 1), 0x801080C8: (22, 0),
            0x800CD064: (26, 1), 0x800F9E0C: (29, 0), 0x80104AF8: (30, 0),
            0x800FFBC8: (31, 0), 0x800FFBA8: (32, 0), 0x80107090: (35, 0),
            0x80108098: (44, 0),
        }
        for address, (kind, count) in simple.items():
            machine.stub(address, lambda m, k=kind, n=count: self.simple(m, k, n))
        machine.stub(0x800CCF08, self.initialize_box)
        machine.stub(0x800C6BF0, self.rank_text)
        machine.stub(0x800CCDF4, self.box)
        machine.stub(0x80104BD0, self.top)
        machine.stub(0x8010552C, self.overview)
        for address, page in PAGE_CREATE.items():
            machine.stub(address, lambda m, p=page: self.calls.append([33, p]))
        for address, page in PAGE_DRAW.items():
            machine.stub(address, lambda m, p=page: self.draw_page(m, p))
        machine.stub(0x800FFB90, self.deactivate)
        machine.stub(0x80045988, self.sound)
        machine.stub(0x80043E3C, self.allocate)
        machine.stub(0x80044B10, self.allocate_request)
        machine.stub(0x80044BC4, self.enqueue)
        machine.stub(0x80044B80, self.release_request)
        machine.stub(0x80043C60, self.free_file)
        machine.stub(0x80107FBC, self.preview)
        machine.stub(0x80104B40, self.description)
        machine.stub(0x80102A38, self.visibility)
        machine.stub(0x80107A98, self.navigation)
        machine.stub(0x801061F8, self.viewer)
        machine.stub(0x800FFC04, self.information)

    def simple(self, machine: Machine, kind: int, count: int) -> None:
        self.calls.append([kind, *(s32(machine.r[index]) for index in range(4, 4 + count))])

    def initialize_box(self, machine: Machine) -> None:
        self.calls.append([23, *(s32(machine.r[index]) for index in range(4, 8)),
                           *(machine.load(machine.r[29] + offset, signed=True)
                             for offset in range(16, 32, 4))])

    def rank_text(self, machine: Machine) -> None:
        content = text(machine, machine.r[5])
        self.calls.append([24, machine.r[4], len(content), *content])

    def box(self, machine: Machine) -> None:
        self.calls.append([25, machine.r[4]])
        machine.r[2] = BOX if self.value["host"][8] else 0

    def top(self, machine: Machine) -> None:
        self.calls.append([27, s32(machine.r[4])])
        machine.r[2] = self.value["host"][0] & MASK

    def overview(self, machine: Machine) -> None:
        state = state_words(machine)
        self.calls.append([28, s32(machine.r[4]), *state[3:8]])

    def draw_page(self, machine: Machine, page: int) -> None:
        state = state_words(machine)
        self.calls.append([34, page, s32(machine.r[4]), *state[:2], *cursor_bytes(machine)])

    def deactivate(self, machine: Machine) -> None:
        self.calls.append([36, s32(machine.r[4]), state_words(machine)[2]])

    def sound(self, machine: Machine) -> None:
        self.calls.append([37, s32(machine.r[4]), s32(machine.r[5]), *cursor_bytes(machine)])

    def allocate(self, machine: Machine) -> None:
        self.calls.append([38, s32(machine.r[4])])
        machine.r[2] = ENEMIES

    def allocate_request(self, machine: Machine) -> None:
        self.calls.append([39, machine.load(machine.r[4], signed=True),
                           machine.load(machine.r[4] + 4, signed=True)])
        machine.r[2] = REQUEST

    def enqueue(self, machine: Machine) -> None:
        if (machine.r[4], machine.r[5]) != (REQUEST, ENEMIES):
            raise Fault("enqueue endpoint did not receive allocated request and destination")
        self.calls.append([40])

    def release_request(self, machine: Machine) -> None:
        if machine.r[4] != REQUEST:
            raise Fault("release endpoint did not receive the completed request")
        self.calls.append([41])

    def free_file(self, machine: Machine) -> None:
        if machine.r[4] != ENEMIES:
            raise Fault("heap release endpoint did not receive MON ownership")
        self.calls.append([42])

    def preview(self, machine: Machine) -> None:
        self.calls.append([43, s32(machine.r[4])])
        machine.r[2] = self.value["host"][4] & MASK

    def description(self, machine: Machine) -> None:
        self.calls.append([45, state_words(machine)[9]])

    def visibility(self, machine: Machine) -> None:
        self.calls.append([46, int(machine.r[4] != 0)])

    def navigation(self, machine: Machine) -> None:
        state = state_words(machine)
        self.calls.append([47, s32(machine.r[4]), state[11], state[13]])

    def viewer(self, machine: Machine) -> None:
        state = state_words(machine)
        self.calls.append([48, s32(machine.r[4]), s32(machine.r[5]), state[9],
                           *state[3:8], state[13], state[14], state[15]])

    def information(self, machine: Machine) -> None:
        content = text(machine, machine.r[4])
        self.calls.append([49, len(content), *content])


def run_controller(machine: Machine, value: dict, entry: int) -> tuple:
    setup_controller(machine, value)
    recorder = ControllerRecorder(machine, value)
    machine.execute(entry)
    values = [s32(machine.r[2]), *state_words(machine), machine.load(0x8010A436, 2),
              *cursor_bytes(machine), *machine.bytes(BOX, BOX_BYTES),
              machine.load(MON_DATA), machine.load(REQUEST_POINTER),
              machine.load(DESCRIPTION_POINTER), *enemy_observation(machine)]
    inputs = [CONTROLLER, *value["state"], value["pad"], *value["input"],
              *value["cursor"], *flatten(value["rows"]), *value["box"],
              *value["delays"], *value["host"]]
    return observe(machine, values, recorder.calls), inputs


class ScreenRecorder:
    def __init__(self, machine: Machine, value: dict) -> None:
        self.calls: list[list[int]] = []
        self.value = value
        machine.stub(0x800FFB68, lambda m: self.calls.append([60, s32(m.r[4])]))
        machine.stub(0x801031BC, lambda m: self.reply(m, 61, 0))
        machine.stub(0x8008A4DC, lambda m: self.environment(m, 62))
        machine.stub(0x800CB654, lambda m: self.environment(m, 63))
        machine.stub(0x800FA9D0, self.idle)
        machine.stub(0x8010337C, lambda m: self.reply(m, 65, 2))
        machine.stub(0x80100414, lambda m: self.calls.append([20, s32(m.r[4]), s32(m.r[5])]))
        machine.stub(0x800FFA88, lambda m: self.calls.append([66, s32(m.r[4])]))
        machine.stub(0x800FA8E0, lambda m: self.calls.append([67, s32(m.r[4])]))

    def reply(self, machine: Machine, kind: int, index: int) -> None:
        self.calls.append([kind])
        machine.r[2] = self.value["replies"][index] & MASK

    def environment(self, machine: Machine, kind: int) -> None:
        self.calls.append([kind, s32(machine.r[4]), machine.load(HUD_OFFSET, signed=True)])

    def idle(self, machine: Machine) -> None:
        self.calls.append([64, machine.load(OUTER_PHASE, 1), machine.load(HUD_OFFSET, signed=True)])
        machine.r[2] = self.value["replies"][1] & MASK


def run_screen(machine: Machine, value: dict, entry: int) -> tuple:
    machine.put(OUTER_PHASE, value["state"][0], 1)
    for address, item in zip(SCREEN_FIELDS, value["state"][1:], strict=True):
        machine.put(address, item)
    for address, size, item in zip((HUD_OFFSET, FADE_STEP, CURRENT_MENU, HIDE_MENU),
                                  (4, 4, 1, 1), value["environment"], strict=True):
        machine.put(address, item, size)
    recorder = ScreenRecorder(machine, value)
    machine.r[4] = OUTER_PHASE
    machine.execute(entry)
    values = [s32(machine.r[2]), machine.load(OUTER_PHASE, 1)]
    values += [machine.load(address, signed=True) for address in SCREEN_FIELDS]
    values += [machine.load(HUD_OFFSET, signed=True), machine.load(FADE_STEP, signed=True),
               machine.load(CURRENT_MENU, 1), machine.load(HIDE_MENU, 1)]
    return observe(machine, values, recorder.calls), [
        SCREEN, *value["state"], *value["environment"], *value["replies"]]


def run(machine: Machine, value: dict, entry: int) -> tuple:
    return (run_controller if value["mode"] == CONTROLLER else run_screen)(machine, value, entry)


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    stack = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + 4)
    if value["mode"] == SCREEN:
        return [stack, (SCREEN_FIELDS[0], SCREEN_FIELDS[-1] + 4),
                (OUTER_PHASE, OUTER_PHASE + 1), (HUD_OFFSET, HUD_OFFSET + 4),
                (CURRENT_MENU, CURRENT_MENU + 1), (HIDE_MENU, HIDE_MENU + 1)]
    return [stack, (0x8010A220, 0x8010A224), (0x8010A430, 0x8010A460),
            (REQUEST_POINTER, DESCRIPTION_POINTER + 4),
            (CURSOR_MEMORY, CURSOR_MEMORY + 8), (BOX + 16, BOX + 24)]
