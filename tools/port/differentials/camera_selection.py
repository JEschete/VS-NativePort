"""Eight camera/selection policies, real nested root and explicit service callbacks.

The original negative-root table-read domain is deliberately excluded from numeric
parity (and refused by the native policies). Live guards and Catch2 cover those
refusals separately. Bone/camera/menu/presentation services are controlled here;
F00179, F00181 and F00182 calls execute their actual instructions.
"""

import random
from dataclasses import dataclass

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems

FUNCTIONS = tuple(f"F{i:05}" for i in range(179, 187))
ENTRIES = (0x80077F14, 0x80077F70, 0x800780A8, 0x8007820C,
           0x80078248, 0x800782E4, 0x80078364, 0x800784AC)
ENDS = (*ENTRIES[1:], 0x80078748)
ID, ENTRY, END = "/".join(FUNCTIONS), ENTRIES[0], ENDS[-1]
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "camera_selection_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\ActionCameraSelection.cpp",
           r"Native\Game\src\Battle\ActionPreview.cpp",
           r"Native\Engine\src\Core\Math\ScalarMath.cpp",
           r"Native\Engine\src\Core\Math\Distance.cpp")
VECTORS, MUTANT_VECTORS, SEED = 8192, 1024, 179186
STATES, GLOBAL, CAMERA, PROJECTION = (0x80500000, 0x80504000), 0x800F19CC, 0x800F19D0, 0x8005E248
OUTPUT, DEFINITIONS, FLAGS = 0x80700000, 0x8004B9DC, 0x800E8184
TAG, BONE, STEP, TRACK = 0x800A152C, 0x800A1AF8, 0x8006DEFC, 0x8006DB98
MENU, AREA, COMMIT, FILTER = 0x800C58F8, 0x800C4794, 0x80070F28, 0x8007053C
INITIALIZE, CLEAR, PRIMARY, SECONDARY, FREE, RETURN = (
    0x80072BA8, 0x800C16DC, 0x800C06E0, 0x800C0738, 0x800C05B4, 0x800704D8)
APPLY, PRESENT, ROOT = 0x80041534, 0x8007138C, 0x80041B94

# Constants, signed widths, individual stores, gates and each external call.
MUTANTS = {
    "F00179-byte-sentinel": {0x80077F28: 0x240200FF},
    "F00179-default-bone-one": {0x80077F38: 0x24020001},
    "F00179-tag-kind-one": {0x80077F44: 0x24060001},
    "F00179-truncated-bone": {0x80077F4C: 0x304500FF},
    "F00179-endpoint-one": {0x80077F58: 0x24070001},
    "F00179-no-position": {0x80077F54: 0},
    "F00180-next-queue": {0x80077FAC: 0x244210A0},
    "F00180-source-actor-neighbor": {0x80077FB4: 0x92040005},
    "F00180-count-byte": {0x80077FC0: 0x9202004A},
    "F00180-position-byte": {0x80077FD0: 0x9202008C},
    "F00180-unsigned-limb": {0x80077FE4: 0x9205004D},
    "F00180-no-second-query": {0x80078028: 0},
    "F00180-no-horizontal-root": {0x80078060: 0},
    "F00180-omit-z-square": {0x80078064: 0x00000000},
    "F00180-no-vertical-root": {0x80078090: 0},
    "F00181-next-queue": {0x800780EC: 0x244210A0},
    "F00181-count-byte": {0x80078100: 0x9202004A},
    "F00181-position-byte": {0x80078110: 0x9202008C},
    "F00181-unsigned-limb": {0x80078124: 0x9205004D},
    "F00181-wrong-focus": {0x80078164: 0x9A080840},
    "F00181-unsigned-source-x": {0x80078184: 0x97A30010},
    "F00181-negative-x-rounding": {0x80078194: 0},
    "F00181-negative-z-rounding": {0x800781BC: 0},
    "F00181-negative-y-rounding": {0x800781E4: 0},
    "F00181-no-x-store": {0x800781A4: 0},
    "F00181-no-z-store": {0x800781CC: 0},
    "F00181-no-y-store": {0x800781F4: 0},
    "F00182-no-midpoint": {0x8007821C: 0},
    "F00182-always-step": {0x80078224: 0},
    "F00182-clamp-zero": {0x8007822C: 0x24050000},
    "F00182-speed-39": {0x80078234: 0x24060027},
    "F00182-no-step": {0x80078230: 0},
    "F00183-selecting-byte": {0x8007825C: 0x90822984},
    "F00183-source-neighbor": {0x8007826C: 0x8082000D},
    "F00183-delay-30": {0x80078290: 0x2C42001E},
    "F00183-no-wrap": {0x8007828C: 0},
    "F00183-no-frame-store": {0x80078288: 0},
    "F00183-no-menu": {0x800782A4: 0},
    "F00183-no-current-reload": {0x800782B4: 0},
    "F00183-no-selected-store": {0x800782C4: 0},
    "F00183-commit-two": {0x800782C8: 0x24040002},
    "F00184-selecting-byte": {0x800782F4: 0x90822984},
    "F00184-delay-30": {0x80078328: 0x2C42001E},
    "F00184-no-wrap": {0x80078324: 0},
    "F00184-no-area-menu": {0x8007833C: 0},
    "F00184-area-pointer": {0x80078340: 0x24842990},
    "F00184-no-filter": {0x8007834C: 0},
    "F00185-selecting-byte": {0x80078378: 0x90822984},
    "F00185-delay-30": {0x800783AC: 0x2C42001E},
    "F00185-no-wrap": {0x800783A8: 0},
    "F00185-no-enemy-clear": {0x800783C0: 0},
    "F00185-no-player-copy": {0x800783E4: 0},
    "F00185-cancel-254": {0x800783F4: 0x240200FE},
    "F00185-category-mask-seven": {0x80078434: 0x30420007},
    "F00185-no-targeting": {0x8007844C: 0},
    "F00185-no-clear": {0x8007845C: 0},
    "F00185-no-primary": {0x80078464: 0},
    "F00185-no-secondary": {0x8007846C: 0},
    "F00185-no-free": {0x80078474: 0},
    "F00185-no-return": {0x8007847C: 0},
    "F00185-no-auto-clear": {0x8007848C: 0},
    "F00185-commit-one": {0x80078490: 0x24040001},
    "F00186-mode-two": {0x800784D0: 0x24020002},
    "F00186-zoom-unsigned": {0x800784E8: 0x94432C0C},
    "F00186-no-step": {0x800784F8: 0},
    "F00186-no-second-midpoint": {0x80078500: 0},
    "F00186-no-tracking": {0x8007851C: 0},
    "F00186-ignore-z-displacement": {0x80078534: 0},
    "F00186-ignore-yaw": {0x80078548: 0},
    "F00186-projection-step-63": {0x80078584: 0x2442003F},
    "F00186-projection-limit-767": {0x80078594: 0x240202FF},
    "F00186-no-render-setter": {0x800785A0: 0},
    "F00186-distance-minus-191": {0x800785BC: 0x2462FF41},
    "F00186-distance-min-1535": {0x800785CC: 0x240205FF},
    "F00186-no-current-projection": {0x800785D8: 0},
    "F00186-distance-plus-191": {0x80078668: 0x246200BF},
    "F00186-distance-max-2303": {0x8007867C: 0x240208FF},
    "F00186-readiness-mode-two": {0x800786F8: 0x24020002},
    "F00186-ignore-other-yaw": {0x80078714: 0},
    "F00186-no-presentation": {0x8007872C: 0},
}


@dataclass(frozen=True)
class Vector:
    header: tuple[int, ...]
    source: tuple[int, ...]
    target: tuple[int, ...]
    displacement: tuple[int, ...]
    alternate: tuple[int, ...]
    states: tuple[tuple[int, ...], ...]

    def inputs(self) -> list[int]:
        return [*self.header, *self.source, *self.target, *self.displacement,
                *self.alternate, *(x for state in self.states for x in state)]


def vector(rng: random.Random, number: int) -> Vector:
    f, lane = number % 8, number // 8
    boundary = (0, 1, 255, 256, MASK, 0x80000000, 0x7FFFFFFF, 0xFFFF8000)
    actor = boundary[lane % 8] if lane < 128 else rng.getrandbits(32)
    limb = (MASK, 255, 128, 0, 1, 0x10001, 0x80000000, 0xFFFFFF80)[lane % 8]
    execute = (0, 1, MASK, 0x80000000)[lane % 4]
    bone = boundary[(lane // 3) % 8] if lane < 128 else rng.getrandbits(32)
    menu_return = (0, 1, MASK)[lane % 3]
    tracking_return = (0, 1, MASK)[lane % 3]
    mode = (1, 3, 1, 3, 0, 2, 4, MASK)[lane % 8]
    projection = (0, 512, 703, 704, 705, 767, 768, 769,
                  0x7FFFFFFF, 0x80000000, MASK)[(lane // 8) % 11]
    distance = (0, 1344, 1535, 1536, 1537, 1727, 1728,
                2111, 2112, 2303, 2304, 2305, 0x7FFFFFFF, 0x80000000)[(lane // 4) % 14]
    yaw = (0, 0, 1, MASK)[(lane // 16) % 4]
    mutation = (lane // 3) % (12 if f == 7 else 8)
    source = tuple(rng.randrange(-8192, 8192) & 65535 for _ in range(4))
    target = tuple(rng.randrange(-8192, 8192) & 65535 for _ in range(4))
    if lane < 128 and f != 1:
        source = tuple(((-32768, 32767, -1, 0, 1, -5, 9)[(lane + i) % 7]) & 65535 for i in range(4))
        target = tuple(((-32768, 32767, -1, 0, 1, -2, 2)[(lane // 2 + i) % 7]) & 65535 for i in range(4))
    if f == 1 and lane % 16 == 0:
        source, target = (40536, 0, 40536, 71), (25000, 0, 25000, 99)
    displacement = ((0, 0, 0), (0, 0, 1), (0, MASK, 0), (1, 0, 0))[(lane // 3) % 4]
    alternate = ((0, 1, 2, 3, 4, MASK)[(lane // 7) % 6],
                 (767, 768, 704, 0x7FFFFFFF)[(lane // 5) % 4],
                 (1537, 1728, 2304, 2305)[(lane // 3) % 4],
                 (0, 1, MASK)[(lane // 11) % 3],
                 (0, 1, 128, 254, 255)[(lane // 3) % 5], lane % 4)
    states = []
    for owner in range(2):
        selecting = (0, 1, 0x10000, 0x80000000)[(lane // 3 + owner) % 4]
        action = (lane * 17 + owner * 31) & 255
        primary = rng.getrandbits(24) << 8 | (0, 1, 128, 255)[(lane // 12 + owner) % 4]
        preview = (0, 1, 254, 255)[(lane // 5 + owner) % 4]
        selected = (17, 255, 128, 0)[(lane // 7 + owner) % 4]
        frames = (0, 29, 30, 31, 32, 127, 254, 255)[(lane // 4 + owner) % 8]
        zoom = (0, 1, 2, 0, 1, 2, -1, 3)[(lane // 8 + owner) % 8]
        word = selected | preview << 8 | frames << 16 | rng.getrandbits(8) << 24
        qprimary = rng.getrandbits(24) << 8 | (lane * 71 + owner) & 255
        count = (0, 1, 2, 255, 256, 65535)[(lane // 2 + owner) % 6]
        kind = (0, 1, 256, MASK, 0x80000000)[(lane // 3 + owner) % 5]
        union = target
        if kind == 0:
            union = ((lane * 37 + owner) & 255 | ((0, 1, 127, 128, 255)[lane % 5] << 8),
                     target[1], target[2], target[3])
        states.append(((lane + owner) % 4, selecting, rng.getrandbits(16) << 16 | action,
                       primary, word, (zoom & 65535) << 16 | rng.getrandbits(16),
                       *target, qprimary, count << 16 | rng.getrandbits(16), kind, *union, *target))
    if f == 7 and lane < 12:
        zoom, mode, projection, distance, yaw = (
            (0, 1, 512, 1536, 0), (0, 3, 512, 1536, 0),
            (1, 1, 704, 1600, 0), (1, 3, 704, 1600, 0),
            (2, 1, 704, 2200, 0), (2, 3, 704, 2200, 0),
            (2, 3, 704, 2304, 0), (1, 1, 0x7FFFFFFF, 2304, 0),
            (1, 3, 705, 1537, 0), (2, 3, 768, 2305, 0),
            (3, 1, 768, 2304, 0), (-1, 3, 768, 2304, 1),
        )[lane]
        mutation, displacement = 0, (0, 0, 0)
        first = list(states[0])
        first[5] = (zoom & 65535) << 16 | first[5] & 65535
        states[0] = tuple(first)
    current = MASK if f == 7 and mode not in (1, 3) else 0
    return Vector((f, lane, mutation, actor, limb, execute, bone, menu_return,
                   tracking_return, current, mode, projection, distance, yaw),
                  source, target, displacement, alternate, tuple(states))


def queue(base: int, index: int) -> int:
    return base + 0x854 + index * 0x84C


def addresses(base: int) -> list[tuple[int, int]]:
    result = [(base + offset, 4) for offset in (0, 0x2984, 8, 12, 0x298C, 0x2C0C)]
    result += [(base + 0x2998 + i * 2, 2) for i in range(4)]
    for index in range(4):
        q = queue(base, index)
        result += [(q + offset, 4) for offset in (4, 0x48, 0x8C)]
        result += [(q + offset + i * 2, 2) for offset in (0x4C, 0x844) for i in range(4)]
    return result


def run(m: Machine, v: Vector) -> tuple[dict, list[int]]:
    (f, lane, mutation, actor, limb, execute, bone, menu_return,
     tracking_return, current, mode, projection, distance, yaw) = v.header
    alt_mode, alt_projection, alt_distance, alt_yaw, menu_preview, flag_mode = v.alternate
    calls = []
    position_count = 0
    for owner, values in enumerate(v.states):
        base = STATES[owner]
        m.map_zero(base, 0x3000)
        for offset, value in zip((0, 0x2984, 8, 12, 0x298C, 0x2C0C), values[:6], strict=True):
            m.put(base + offset, value)
        for i, value in enumerate(values[6:10]):
            m.put(base + 0x2998 + i * 2, value, 2)
        for index in range(4):
            q = queue(base, index)
            for offset, value in zip((4, 0x48, 0x8C), values[10:13], strict=True):
                m.put(q + offset, value)
            for offset, point in ((0x4C, values[13:17]), (0x844, values[17:21])):
                for i, value in enumerate(point):
                    m.put(q + offset + i * 2, value, 2)
    m.map_zero(CAMERA, 44)
    m.put(CAMERA + 8, distance)
    m.put(CAMERA + 16, yaw)
    m.put(CAMERA + 28, mode)
    m.put(PROJECTION, projection)
    m.put(GLOBAL, STATES[current] if current != MASK else 0)
    for action in range(256):
        m.put(DEFINITIONS + action * 52, ((action + lane) % 16) << 20 | action)
    for category in range(16):
        flag = (0, 1, 128, 255)[(category * 5 + category // 4 + flag_mode) % 4]
        m.put(FLAGS + category, flag, 1)
    m.map_zero(OUTPUT, 8)
    for i, value in enumerate(v.target):
        m.put(OUTPUT + i * 2, value, 2)
    out = OUTPUT
    if f in (0, 2) and lane % 3 == 0:
        out = queue(STATES[0], v.states[0][0]) + 0x844
    elif f in (0, 2) and lane % 3 == 1:
        out = STATES[0] + 0x2998

    def event(address, *args):
        calls.extend([address, *(a & MASK for a in args), *([0] * (7 - len(args)))])

    def owner():
        value = m.load(GLOBAL)
        return STATES.index(value) if value in STATES else MASK

    def low_byte(address, value):
        m.put(address, value & 255, 1)

    def point_at(address, point):
        for i, value in enumerate(point):
            m.put(address + i * 2, value & 65535, 2)

    def mutate(address):
        if address == TAG and f == 0 and mutation == 6:
            m.put(GLOBAL, STATES[1])
        if address == BONE and position_count == 1 and f in (1, 2, 3, 7):
            q = queue(STATES[0], v.states[0][0])
            if mutation == 1:
                m.put(GLOBAL, STATES[1])
                m.put(STATES[0], (v.states[0][0] + 1) & 3)
                low_byte(q + 4, 255)
            elif mutation == 2:
                m.put(q + 0x4A, 0, 2)
                low_byte(q + 4, 255)
                point_at(q + 0x844, v.target)
            elif mutation in (3, 4, 5):
                m.put(q + 0x4A, 65535 if mutation == 3 else 1, 2)
                m.put(q + 0x8C, MASK if mutation == 3 else 0)
                if mutation == 3:
                    point_at(q + 0x4C, v.target)
                else:
                    low_byte(q + 0x4C, 255 if mutation == 4 else 128)
                    low_byte(q + 0x4D, 255 if mutation == 4 else 128)
        if f == 7:
            if address == STEP and mutation in (6, 7, 8, 11):
                if mutation == 6:
                    m.put(GLOBAL, STATES[1])
                    m.put(STATES[1] + 0x2C0E, 3, 2)
                if mutation in (6, 7):
                    m.put(PROJECTION, alt_projection)
                    m.put(CAMERA + 8, alt_distance)
                if mutation in (6, 8, 11):
                    m.put(CAMERA + 28, alt_mode)
                    m.put(CAMERA + 16, alt_yaw)
            if address == APPLY and mutation == 9:
                m.put(PROJECTION, alt_projection)
                m.put(CAMERA + 8, alt_distance)
                m.put(CAMERA + 28, alt_mode)
            if address == TRACK and mutation == 10:
                m.put(CAMERA + 28, alt_mode)
                m.put(CAMERA + 16, alt_yaw)
                m.put(GLOBAL, STATES[1])
        if address in (MENU, AREA):
            if mutation in (1, 2, 5, 6):
                m.put(GLOBAL, 0 if mutation == 6 and menu_return == 0 else STATES[1])
                if mutation == 1:
                    low_byte(STATES[1] + 0x298D, menu_preview ^ 127)
                elif mutation == 2:
                    low_byte(STATES[1] + 0x298D, 255)
                    m.put(STATES[1] + 8, lane * 17 & 255, 2)
                elif mutation == 5:
                    low_byte(STATES[1] + 0x298D, 0)
                    m.put(STATES[1] + 0x2984, 0)
            if mutation == 3:
                low_byte(STATES[0] + 0x298C, 199)
            if mutation == 4:
                low_byte(STATES[0] + 0x298D, 255)
                m.put(STATES[0] + 8, lane * 7 & 255, 2)
        if mutation == 7 and address in (COMMIT, FILTER, INITIALIZE, CLEAR, PRIMARY, SECONDARY, FREE, RETURN):
            m.put(GLOBAL, STATES[1])

    def tag(machine):
        event(TAG, *machine.r[4:7], bone)
        mutate(TAG)
        machine.r[2] = bone

    def position(machine):
        nonlocal position_count
        point = v.source if position_count % 2 == 0 else v.target
        position_count += 1
        event(BONE, machine.r[4], machine.r[5], machine.r[7], *point[:3], 0)
        point_at(machine.r[6], (*point[:3], m.load(machine.r[6] + 6, 2)))
        mutate(BONE)
        machine.r[2] = 0

    def camera_service(address):
        def service(machine):
            point = [machine.load(machine.r[4] + i * 2, 2) for i in range(3)]
            clamp, speed = (machine.r[5:7] if address == STEP else
                            (machine.r[7], machine.load(machine.r[29] + 16)))
            event(address, *point, clamp, speed, tracking_return)
            if address == TRACK:
                for i, value in enumerate(v.displacement):
                    machine.put(machine.r[6] + i * 4, value)
                    machine.put(machine.r[5] + i * 4, value ^ 0x1234)
            mutate(address)
            machine.r[2] = tracking_return
        return service

    def menu(machine):
        initial = next(i for i, base in enumerate(STATES) if machine.r[4] == base + 0x298D)
        event(MENU, initial, m.load(machine.r[4], 1))
        low_byte(machine.r[4], menu_preview)
        mutate(MENU)
        machine.r[2] = menu_return

    def area(machine):
        initial = next((i for i, base in enumerate(STATES) if machine.r[4] == base + 0x2998), MASK)
        event(AREA, initial, *(m.load(machine.r[4] + i * 2, 2) for i in range(4)))
        point_at(machine.r[4], (*v.target[:3], 1))
        mutate(AREA)
        machine.r[2] = menu_return

    def commit(machine):
        current = owner()
        selected = m.load(STATES[current] + 0x298C, 1) if current != MASK else MASK
        action = m.load(STATES[current] + 8, 2) if current != MASK else MASK
        event(COMMIT, machine.r[4], current, selected, action)
        mutate(COMMIT)

    def apply(machine):
        event(APPLY, machine.r[4], m.load(CAMERA + 28), m.load(CAMERA + 8))
        mutate(APPLY)

    m.stubs[TAG], m.stubs[BONE] = tag, position
    m.stubs[STEP], m.stubs[TRACK] = camera_service(STEP), camera_service(TRACK)
    m.stubs[MENU], m.stubs[AREA], m.stubs[COMMIT], m.stubs[APPLY] = menu, area, commit, apply
    for address in (FILTER, INITIALIZE, CLEAR, PRIMARY, SECONDARY, FREE, RETURN, PRESENT):
        def service(machine, address=address):
            event(address, *([machine.r[4]] if address == INITIALIZE else []))
            mutate(address)
        m.stubs[address] = service
    m.follow.update((*ENTRIES, ROOT))
    m.stores.clear()
    m.r[4:7] = [actor, limb, out] if f == 0 else [out if f == 2 else execute, 0, 0]
    m.execute(ENTRIES[f], limit=20000)
    values = [m.r[2] if f == 1 else 0, *(m.load(out + i * 2, 2) for i in range(4)),
              owner(), m.load(CAMERA + 28), m.load(PROJECTION), m.load(CAMERA + 8), m.load(CAMERA + 16)]
    for base in STATES:
        values.extend(m.load(address, size) for address, size in addresses(base))
    values += [len(calls), *calls]
    stores = [(a, n) for a, n in m.stores if not DEFAULT_STACK - 0x2000 <= a < DEFAULT_STACK + 32]
    return {"values": [0, *values], "stores": stores, "queries": position_count}, v.inputs()


def check(image: Image, v: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, v, reference
    expected = observation["values"]
    differences = [(i, a, b) for i, (a, b) in enumerate(zip(native, expected, strict=False)) if a != b]
    problems = [] if native == expected else [f"state/output/events {differences[:6]}; lengths {len(native)}/{len(expected)}"]
    allowed = [(GLOBAL, GLOBAL + 4), (CAMERA, CAMERA + 44),
               (PROJECTION, PROJECTION + 4), (OUTPUT, OUTPUT + 8)]
    for base in STATES:
        allowed.extend((address, address + size) for address, size in addresses(base))
        # Combined union/word stores may cover adjacent compared fields.
        allowed.extend((queue(base, i) + off, queue(base, i) + off + size)
                       for i in range(4) for off, size in ((0x4C, 8), (0x844, 8)))
        allowed.append((base + 0x2998, base + 0x29A0))
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    lines = []
    for f, (name, start, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v.header[0] == f]
        lines.append(f"{name}: {len(selected)} own vectors, "
                     f"{sum(bool(check(image, v, o, n, {})) for v, o, n in selected)} mismatches; "
                     f"[{start:08X},{end:08X}) {(end-start)//4} instructions SHA256={image.sha256(start, end)}")
    return lines + ["Numeric parity excludes original negative-root table reads; no clamp equivalence claim. "
                    "Services controlled; actual Skeleton/Field/Render signatures and Live refusals tested separately."]
