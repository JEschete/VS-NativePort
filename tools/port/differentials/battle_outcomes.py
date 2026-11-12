"""Five action outcome policies; original BATTLE and INITBTL instruction images."""

import os
import random
from collections import Counter
from dataclasses import dataclass
from functools import lru_cache

from differential import (
    DEFAULT_STACK,
    DISC,
    MASK,
    Image,
    Machine,
    field_problems,
    s32,
    store_problems,
)

FUNCTIONS = ("F00048", "F00059", "F00061", "F00156", "F01851")
SELECTED = os.environ.get("VS_OUTCOME_FUNCTION")
if SELECTED is not None and SELECTED not in FUNCTIONS:
    raise ValueError(f"unknown VS_OUTCOME_FUNCTION: {SELECTED}")
ID = "/".join(FUNCTIONS) + " (bundle header: F00048)"
ENTRIES = (0x8006C1CC, 0x8006C5AC, 0x8006CA20, 0x80074A20, 0x800F9818)
ENDS = (0x8006C250, 0x8006C84C, 0x8006CB04, 0x80074B14, 0x800F989C)
ENTRY, END = ENTRIES[0], ENDS[0]
DRIVER = "battle_outcomes_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\ActionOutcomes.cpp",)
VECTORS, SEED = (1024 if SELECTED else 5 * 1024), 4859611561851
MUTANT_VECTORS = 128 if SELECTED else 5 * 128

ACTORS, GLOBAL = 0x800F1928, 0x800F19CC
RECORDS, STATS = (0x80600000, 0x80600100), (0x80601000, 0x80601100)
STATES = (0x80500000, 0x80504000)
DEFINITIONS, FLAGS = 0x8004B9DC, 0x8005FFB8
ACTION_SIZE, HITS = 0x84C, 30
SCORE, STREAK, LOOT = 0x8006C004, 0x8006C164, 0x8006BE64
PLACEMENT, SOUND = 0x800A1108, 0x80045AA8
REACTION, POSE, RESULT = 0x8009EA14, 0x8009E070, 0x800875C8
REFRESH, MARKER, DEATH, LOCK = 0x800E6700, 0x80095B7C, 0x8009F314, 0x800A09D8
EVENT_WIDTH = 15
COUNTS = {SCORE: 1, STREAK: 1, LOOT: 1, PLACEMENT: 2, SOUND: 3,
          REACTION: 2, POSE: 3, RESULT: 1, REFRESH: 1, MARKER: 2, DEATH: 3, LOCK: 2}

MUTANTS = {
    "F00048-spawn-127": {0x8006C1F4: 0x2402007F},
    "F00048-no-spawn-guard": {0x8006C1F8: 0},
    "F00048-no-defeated-guard": {0x8006C208: 0},
    "F00048-ignore-count": {0x8006C210: 0},
    "F00048-ignore-source": {0x8006C218: 0},
    "F00048-no-score": {0x8006C220: 0},
    "F00048-no-streak": {0x8006C228: 0},
    "F00048-no-loot": {0x8006C230: 0},
    "F00048-wrong-flag": {0x8006C23C: 0xA2020029},
    "F00048-wide-flag": {0x8006C23C: 0xAE020028},
    "F00059-queue-mask-1": {0x8006C5E0: 0x30630001},
    "F00059-world-position": {0x8006C630: 0},
    "F00059-no-placement": {0x8006C63C: 0},
    "F00059-wrong-y": {0x8006C648: 0x97A30016},
    "F00059-impact-neighbor": {0x8006C65C: 0x9223004E},
    "F00059-impact-cap-3": {0x8006C664: 0x2C620003},
    "F00059-category-neighbor": {0x8006C69C: 0x90420024},
    "F00059-nine-categories": {0x8006C6A8: 0x2C620009},
    "F00059-category1-first": {0x8006C6D4: 0x24050046},
    "F00059-category1-second": {0x8006C6E4: 0x24050049},
    "F00059-category2-first": {0x8006C6F4: 0x24050048},
    "F00059-category3-second": {0x8006C724: 0x2405004D},
    "F00059-category4-first": {0x8006C734: 0x24050051},
    "F00059-category5-second": {0x8006C764: 0x24050054},
    "F00059-category6-single": {0x8006C778: 0x2405004E},
    "F00059-category7-first": {0x8006C780: 0x2405004F},
    "F00059-category8-second": {0x8006C7B0: 0x24050056},
    "F00059-category9-first": {0x8006C7E0: 0x24050068},
    "F00059-category10-second": {0x8006C7D0: 0x2405005D},
    "F00059-block-sound": {0x8006C800: 0x24050034},
    "F00059-group": {0x8006C6D0: 0x2404007F},
    "F00059-no-final-sound": {0x8006C808: 0},
    "F00059-count-neighbor": {0x8006C810: 0x96620048},
    "F00059-wrong-stride": {0x8006C820: 0x26310040},
    "F00059-return-zero": {0x8006C840: 0x24020000},
    "F00061-gate-fifteen": {0x8006CA28: 0x28A2000F},
    "F00061-gate-seventeen": {0x8006CA28: 0x28A20011},
    "F00061-mask-one": {0x8006CA4C: 0x30630001},
    "F00061-no-primary-match": {0x8006CA7C: 0},
    "F00061-no-world-guard": {0x8006CA8C: 0},
    "F00061-no-reaction": {0x8006CAC8: 0},
    "F00061-focus-neighbor": {0x8006CACC: 0x26050840},
    "F00061-no-source-zero-gate": {0x8006CAD8: 0},
    "F00061-recovery-pose-seven": {0x8006CAE8: 0x24060007},
    "F00061-no-result": {0x8006CAEC: 0},
    "F00061-result-neighbor": {0x8006CAF0: 0x26040008},
    "F00156-no-refresh": {0x80074A34: 0},
    "F00156-null-guard": {0x80074A54: 0},
    "F00156-clear-opcode": {0x80074A5C: 0xA0400009},
    "F00156-no-hp-gate": {0x80074A78: 0},
    "F00156-no-marker": {0x80074A84: 0},
    "F00156-no-post-marker-reload": {0x80074A8C: 0},
    "F00156-command-33": {0x80074A90: 0x24020021},
    "F00156-retain-neighbor": {0x80074AA0: 0x90420025},
    "F00156-no-retain-gate": {0x80074AA8: 0},
    "F00156-no-death": {0x80074AC0: 0},
    "F00156-origin-neighbor": {0x80074AC4: 0x24A529A0},
    "F00156-no-lock": {0x80074ACC: 0},
    "F00156-lock-zero": {0x80074AD0: 0x24050000},
    "F00156-no-final-actor-reload": {0x80074AD4: 0},
    "F00156-spawn-127": {0x80074AE0: 0x2402007F},
    "F00156-no-slot-zero-exemption": {0x80074AEC: 0x10000003},
    "F00156-return-two": {0x80074AF4: 0x24120002},
    "F01851-first-bit-64": {0x800F9824: 0x24070040},
    "F01851-clear-neighbor": {0x800F982C: 0x34C6BFFF},
    "F01851-flags-neighbor": {0x800F984C: 0x90630001},
    "F01851-reversed-gate": {0x800F9860: 0x14600004},
    "F01851-set-neighbor": {0x800F9870: 0x34424000},
    "F01851-no-clear": {0x800F987C: 0},
    "F01851-wrong-word": {0x800F9880: 0xAC820010},
    "F01851-last-index": {0x800F9888: 0x28A200FF},
    "F01851-wrong-stride": {0x800F9890: 0x24840030},
}
if SELECTED:
    MUTANTS = {name: patches for name, patches in MUTANTS.items() if name.startswith(SELECTED + "-")}


@lru_cache(maxsize=1)
def init_image() -> Image:
    path = DISC / "BATTLE" / "INITBTL.PRG"
    return Image(path.read_bytes(), 0x800F9800, path)


def block(seed: int, size: int) -> bytes:
    return b"".join(((seed ^ (index * 0x9E3779B9)) & MASK).to_bytes(4, "little")
                    for index in range(size // 4))


@dataclass(frozen=True)
class Vector:
    function: int
    lane: int
    mutation: int
    seed: int
    source: int
    target: int
    kill: int
    present: int
    index: int
    count: int
    actors: tuple[tuple[int, ...], ...]
    primary: tuple[int, int]
    hits: tuple[tuple[int, int], ...]
    flags: tuple[int, ...]

    def inputs(self) -> list[int]:
        return [self.function, self.lane, self.mutation, self.seed, self.source, self.target,
                self.kill, self.present, self.index, self.count,
                *(x for actor in self.actors for x in actor), *self.primary,
                *(x for hit in self.hits for x in hit), *self.flags]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = (FUNCTIONS.index(SELECTED), number) if SELECTED else (number % 5, number // 5)
    mutation = lane % (6, 9, 5, 10, 3)[function]
    seed = rng.getrandbits(32)
    target = (0, 1, 14, 15)[lane % 4]
    source = (0, 0, 1, 256, MASK, 0xFFFFFF00)[lane % 6]
    if function == 1:
        source = (0, 1, 14, 15)[lane % 4]
    if function == 2:
        mutation = (lane * 7 + lane // 10) % 5
        source = (0, 1, 15, 16, 255, 256, MASK, 0xFFFFFF00,
                  0x80000000, 0x7FFFFFFF)[lane % 10]
    if function == 3:
        source = target
    index = (lane % 4) | ((rng.getrandbits(30) << 2) if lane % 2 else 0)
    count = (0, 1, 2, 3, 5, 30)[(lane // 9) % 6]
    actors = []
    for owner in range(2):
        spawn = (1, 0x80, 0x7F, 0x81)[(lane // 6 + owner) % 4]
        defeated = (0, 0, 0, 1, 128, 255)[(lane // 3 + owner) % 6]
        category = (lane // 9 + owner * 5) % 13
        retained = (0, 0, 1, 128)[(lane // 5 + owner) % 4]
        hp = (0, 0, 0, 1, 0xFFFF, 0x8000, 0x7FFF)[(lane // 10 + owner) % 7]
        actors.append((spawn, defeated, category, retained, hp, seed >> (owner * 8) & 255))
    if function == 0 and lane % 3 != 2:
        actors[0] = (1, 0, *actors[0][2:])
    if function == 3 and lane % 3:
        actors[0] = (*actors[0][:3], 0, 0, actors[0][5])
        actors[1] = (*actors[1][:3], int(mutation == 4), 0, actors[1][5])
    primary_actor = source if 0 <= s32(source) < 16 and lane % 4 != 3 else (lane * 7) & 255
    if function == 2 and source == 16:
        primary_actor = 16
    primary = (primary_actor | ((seed & 0xFFFF) << 8) | ((lane // 10) % 8) << 24,
               0 if lane % 7 else 0x80000000)
    hits = []
    for hit in range(HITS):
        impact = (0, 1, 2, 3, 4, 5, 6, 255)[(lane // 3 + hit) % 8]
        actor = (hit * 7 + lane) & 255
        position = 0 if (lane + hit) % 5 else (1, 0x80000000, MASK)[hit % 3]
        hits.append((actor | (seed & 0xFFFF) << 8 | impact << 24, position))
    if function == 1 and mutation in (2, 3, 4, 5, 7, 8):
        count = max(count, 1 if mutation == 4 else 3)
        hits[0] = (hits[0][0] & 0x00FFFFFF, 0)
    if function == 1 and lane % 17 == 0:
        mutation, source = 0, (MASK, 256)[lane % 2]
        hits = [(word & 0x00FFFFFF | (3 if hit % 2 else 5) << 24, position)
                for hit, (word, position) in enumerate(hits)]
    flags = tuple(0 if lane % 8 == 0 else MASK if lane % 8 == 1 else rng.getrandbits(32)
                  for _ in range(8))
    return Vector(function, lane, mutation, seed, source, target,
                  (0, 1, MASK, 256, 0x80000000)[lane % 5], int(lane % 7 != 0),
                  index, count, tuple(actors), primary, tuple(hits), flags)


def queue_address(v: Vector, owner: int) -> int:
    return STATES[owner] + 0x854 + ((v.index + owner) & 3) * ACTION_SIZE


def run(m: Machine, v: Vector) -> tuple[dict, list[int]]:
    if v.function == 4:
        # INITBTL overlaps BATTLE's address range: place its image first without
        # replacing the shared Machine or its instruction-patch behavior.
        m.images.insert(0, init_image())
    actor_pointer = RECORDS[0] if v.present or v.function != 3 else 0
    global_pointer = STATES[0]
    for slot in range(16):
        m.put(ACTORS + slot * 4, actor_pointer)
    m.put(GLOBAL, global_pointer)
    queues = tuple(queue_address(v, owner) for owner in range(2))
    for owner in range(2):
        m.store(RECORDS[owner], block(v.seed ^ (owner * 0xA1110000), 0x50))
        spawn, defeated, category, retained, hp, command = v.actors[owner]
        for offset, value in ((8, command), (0x25, category), (0x26, retained),
                              (0x27, spawn), (0x28, defeated)):
            m.put(RECORDS[owner] + offset, value, 1)
        m.put(RECORDS[owner] + 0x3C, STATS[owner])
        m.store(STATS[owner], block(v.seed ^ (0xB0000000 + owner), 8))
        m.put(STATS[owner] + 0x18, hp, 2)
        m.put(STATS[owner] + 0x1A, v.seed >> 16, 2)
        m.map_zero(STATES[owner], 0x3000)
        m.put(STATES[owner], (v.index + owner) & MASK)
        m.store(queues[owner], block(v.seed ^ (0xC0000000 + owner), ACTION_SIZE))
        m.put(queues[owner] + 0x4A, v.count if owner == 0 else 0, 2)
        m.put(queues[owner] + 4, v.primary[0] ^ (owner * 0x01000001))
        m.put(queues[owner] + 0x44, v.primary[1])
        for hit, (word, position) in enumerate(v.hits):
            m.put(queues[owner] + 0x4C + hit * 0x44, word ^ (owner * 0x01000001))
            m.put(queues[owner] + 0x8C + hit * 0x44, position)
        m.store(STATES[owner] + 0x29A8, block(v.seed ^ (0xD0000000 + owner), 8))
    m.store(DEFINITIONS, block(v.seed ^ 0xE0000000, 256 * 0x34))
    for index, flags in enumerate(v.flags):
        m.put(FLAGS + index * 4, flags)
    calls = []
    placements = sounds = 0

    def set_actor(owner: int) -> None:
        for slot in range(16):
            m.put(ACTORS + slot * 4, 0 if owner < 0 else RECORDS[owner])

    def change_primary() -> None:
        m.put(GLOBAL, STATES[1])
        m.put(queues[0] + 4, 0xFF010009)
        m.put(queues[0] + 8, 0x80007FFF)
        m.put(queues[0] + 0x44, 1)

    def mutate(address: int) -> None:
        mode = v.mutation
        if v.function == 0:
            if ((mode in (1, 4) and address == SCORE) or
                (mode == 2 and address == STREAK) or
                (mode in (3, 5) and address == LOOT)):
                set_actor(1)
                m.put(RECORDS[0] + 0x27, 0x80, 1)
                m.put(RECORDS[0] + 0x28, 255 if address != LOOT else 0, 1)
        elif v.function == 1:
            if address == PLACEMENT and placements == 1:
                if mode == 1:
                    m.put(queues[0] + 0x4F, 3, 1)
                if mode in (2, 6):
                    m.put(GLOBAL, STATES[1])
                    m.put(STATES[0], v.index ^ 3)
                    m.put(queues[0] + 0x4F, 0 if mode == 2 else 3, 1)
                    m.put(queues[0] + 0x8C, 1)
                    set_actor(1)
            if address == SOUND and sounds == 1:
                if mode in (3, 4, 5, 7, 8):
                    set_actor(1)
                    m.put(RECORDS[1] + 0x25, 6, 1)
                    m.put(queues[0] + 0x4C, 255, 1)
                    m.put(GLOBAL, STATES[1])
                if mode in (4, 7, 8):
                    m.put(queues[0] + 0x90, 0x040000FF)
                    m.put(queues[0] + 0xD0, 0)
                    m.put(queues[0] + 0x4A, 3, 2)
                if mode == 5:
                    m.put(queues[0] + 0x4A, 1, 2)
        elif v.function == 2:
            if (address == REACTION and mode in (1, 3)) or (address == POSE and mode == 2):
                change_primary()
            if address == RESULT and mode == 4:
                change_primary()
        elif v.function == 3:
            if address == REFRESH:
                if mode == 1:
                    set_actor(0)
                elif mode == 2:
                    set_actor(-1)
                elif mode == 3:
                    set_actor(1)
            if address == MARKER and mode in (4, 5):
                set_actor(1)
                m.put(RECORDS[1] + 0x26, int(mode == 4), 1)
                m.put(GLOBAL, STATES[1])
            if address == DEATH and mode == 6:
                set_actor(1)
                m.put(RECORDS[1] + 0x27, 0x80, 1)
            if address == LOCK and mode in (7, 8, 9):
                set_actor(1)
                m.put(RECORDS[1] + 0x27, 0x80 if mode == 7 else 1, 1)

    def stub(address: int):
        def invoke(machine: Machine) -> None:
            nonlocal placements, sounds
            args = [*machine.r[4:4 + COUNTS[address]], *([0] * (3 - COUNTS[address]))]
            point = [0, 0, 0]
            if address == PLACEMENT:
                actor, output = args[:2]
                placements += 1
                point = placement_point(v.seed, actor)
                machine.put(output, v.seed)
                for axis, value in enumerate(point):
                    machine.put(output + 4 + axis * 2, value, 2)
                machine.put(output + 10, 0x7340, 2)
                args[1] = 0
            elif address == SOUND:
                sounds += 1
                point = [machine.load(args[2] + axis * 2, 2) for axis in range(3)]
                args[2] = 0
            elif address in (REACTION, DEATH):
                point = [machine.load(args[1] + axis * 2, 2) for axis in range(3)]
            calls.extend([address, *args, *point, machine.load(GLOBAL), machine.load(ACTORS),
                          machine.load(RECORDS[0] + 8, 1), machine.load(RECORDS[1] + 8, 1),
                          machine.load(queues[0] + 4), machine.load(queues[0] + 8),
                          machine.load(queues[0] + 0x44), machine.load(queues[0] + 0x4A, 2)])
            mutate(address)
            for register in (*range(2, 16), 24, 25):
                machine.r[register] = 0xC1000000 | (len(calls) // EVENT_WIDTH) << 8 | register
            if address == PLACEMENT:
                machine.r[2] = 0
        return invoke

    for address in COUNTS:
        m.stub(address, stub(address))
    m.r[4:7] = [v.source, v.target, v.kill]
    m.execute(ENTRIES[v.function], limit=12000)
    if v.function == 4 and v.mutation:
        for index in range(8):
            m.put(FLAGS + index * 4, 0 if v.mutation == 1 else v.flags[index] ^ MASK)
        m.execute(ENTRIES[v.function], limit=12000)
    result = m.r[2] if v.function in (1, 3) else 0
    spans = [(base, 0x50) for base in RECORDS]
    spans += [(base, 8) for base in STATS]
    spans += [(base + 0x18, 4) for base in STATS]
    if v.function in (1, 2):
        spans += [(base, ACTION_SIZE) for base in queues]
    if v.function == 4:
        spans = [(DEFINITIONS, 256 * 0x34), (FLAGS, 32)]
    values = [result, m.load(GLOBAL), m.load(ACTORS), *(m.load(base) for base in STATES)]
    values += [m.load(base + 0x29A8 + word * 4) for base in STATES for word in range(2)]
    values += [m.load(base + offset) for base, size in spans for offset in range(0, size, 4)]
    writes = [(address, size) for address, size in m.stores
              if not DEFAULT_STACK - 0x48 <= address < DEFAULT_STACK]
    observation = {"values": values, "calls": calls, "writes": writes, "stores": list(m.stores)}
    return observation, v.inputs()


def placement_point(seed: int, actor: int) -> list[int]:
    return [(seed + actor * 193) & 0xFFFF, ((seed >> 9) - actor * 71) & 0xFFFF,
            ((seed >> 17) + actor * 257) & 0xFFFF]


def check(image: Image, v: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    count = len(observation["values"])
    if len(native) < count + 3:
        return [f"short native output: {len(native)}"]
    problems = field_problems(("native-diagnostics", *(f"value-{i}" for i in range(count))),
                              native[:count + 1], [0, *observation["values"]])
    end = count + 2 + native[count + 1]
    if end >= len(native) or end + 1 + native[end] != len(native):
        return [*problems, "malformed write/call output"]
    writes = [x for store in observation["writes"] for x in store]
    if native[count + 2:end] != writes:
        problems.append(f"writes native {native[count + 2:end][:20]} original {writes[:20]}")
    if native[end + 1:] != observation["calls"]:
        first = next((i for i, (a, b) in enumerate(zip(native[end + 1:], observation["calls"], strict=False))
                      if a != b), min(len(native) - end - 1, len(observation["calls"])))
        first = first // EVENT_WIDTH * EVENT_WIDTH
        problems.append(f"calls at {first // EVENT_WIDTH}: native "
                        f"{native[end + 1 + first:end + 1 + first + EVENT_WIDTH]} original "
                        f"{observation['calls'][first:first + EVENT_WIDTH]}")
    allowed = [(DEFAULT_STACK - 0x48, DEFAULT_STACK)]
    if v.function == 0:
        allowed += [(base + 0x28, base + 0x29) for base in RECORDS]
    elif v.function == 3:
        allowed += [(base + 8, base + 9) for base in RECORDS]
    elif v.function == 4:
        allowed += [(DEFINITIONS + index * 0x34 + 12, DEFINITIONS + index * 0x34 + 16)
                    for index in range(256)]
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    lines = ["VOID results are not compared; XYZ-only sound consumer, no unwritten PS1 pad parity claim."]
    for function, (name, entry, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v.function == function]
        original = init_image() if function == 4 else image
        module = original.path.name if original.path is not None else "unknown"
        counts = Counter(address for _, obs, _ in selected for address in obs["calls"][::EVENT_WIDTH])
        lines.append(f"{name} module={module} [{entry:08X},{end:08X}) "
                     f"{(end - entry) // 4} instructions sha256={original.sha256(entry, end)} "
                     f"vectors={len(selected)} mismatches="
                     f"{sum(bool(check(image, v, o, n, o)) for v, o, n in selected)} "
                     f"mutation_modes={sorted({v.mutation for v, _, _ in selected})} "
                     f"calls={dict(sorted(counts.items()))}")
    return lines
