"""F00518/F00519/F00521/F00527/F00528 elemental enchantments, stat numbers and targeting risk."""

import random
from dataclasses import dataclass

from differential import DEFAULT_STACK, MASK, Image, Machine, field_problems, s32

ID = "F00518/F00519 (first span; F00521/F00527/F00528 spans below)"
ENTRY, END = 0x80086EA4, 0x80086FA8
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "status_helpers_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\StatusHelpers.cpp",)
VECTORS, SEED = 10240, 518519521527528
MUTANT_VECTORS = 1280
FUNCTIONS = ("F00518", "F00519", "F00521", "F00527", "F00528")
ENTRIES = (ENTRY, 0x80086EF4, 0x80087578, 0x80088B8C, 0x80088CA0)
ENDS = (ENTRIES[1], END, 0x800875C8, ENTRIES[4], 0x80088CAC)
GUARD, STATS_SIZE, ACTOR_SIZE, SELECTION_SIZE = 16, 0x970, 0x50, 0x60
GLOBALS, GLOBALS_SIZE = 0x800F18E0, 0x140
ACTORS, CONTROL, FLOW = 0x800F1928, 0x800F196C, 0x800F18F0
TIMER, TICKS, SELECTION_POINTER, PLAYER_POINTER = 0x800E8498, 0x8005E24C, 0x800F19CC, 0x800F19FC
ACTOR, PLAYER, STATS, PLAYER_STATS, SELECTION = (
    0x80500000, 0x80501000, 0x80502000, 0x80504000, 0x80506000)
QUEUE_NUMBER = 0x80093B68
EQUIPMENT = ((0x54, 0x184, 0x1A4), (0x1EC, 0x2E8, 0x308),
             *((0x3B8 + limb * 0xDC, 0x420 + limb * 0xDC, 0x440 + limb * 0xDC)
               for limb in range(6)))
BOUNDARIES = (0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, MASK)
NAMES = ("refused", "diagnostics", "native-canaries", "native-inputs")
MUTANTS = {
    "F00518-absent-written": {0x80086EB0: 0},
    "F00518-increase-subtracted": {0x80086EC8: 0x00471023},
    "F00518-negative-modifier-lost": {0x80086EE0: 0x00E01021},
    "F00519-absent-shield-written": {0x80086F00: 0},
    "F00519-only-five-limbs": {0x80086F94: 0x29220005},
    "F00519-wrong-decrease-stride": {0x80086F88: 0x24C600D8},
    "F00519-negative-armor-modifier-lost": {0x80086F40: 0x00E05821},
    "F00521-suppress-127": {0x8008759C: 0x2402007F},
    "F00521-player-exception-lost": {0x800875A8: 0x10000003},
    "F00521-amount-bits-lost": {0x800875B4: 0x00003021},
    "F00527-control-three": {0x80088B94: 0x24020003},
    "F00527-signed-flow": {0x80088BA8: 0x28620004},
    "F00527-zero-flow-accepted": {0x80088BB4: 0},
    "F00527-nonzero-timer-reads-target": {0x80088BC4: 0},
    "F00527-wrong-status-bit": {0x80088C00: 0x30420004},
    "F00527-position-check-lost": {0x80088C14: 0},
    "F00527-target-identity-check-lost": {0x80088C1C: 0},
    "F00527-cap-99": {0x80088C40: 0x2C620063},
    "F00527-ticks-lost": {0x80088C68: 0x00802021},
    "F00527-negative-remainder-correction-lost": {0x80088C80: 0},
    "F00527-byte-risk-store": {0x80088C4C: 0xA0820020},
    "F00528-no-reset": {0x80088CA8: 0},
    "F00528-reset-to-address": {0x80088CA8: 0xAC428498},
}


@dataclass(frozen=True)
class Vector:
    function: int
    arguments: tuple[int, ...]
    guards: tuple[int, ...]
    blocks: tuple[tuple[int, bytes], ...]


def put(block: bytearray, offset: int, value: int, size: int = 4) -> None:
    block[offset:offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")


def words(block: bytes) -> list[int]:
    return [int.from_bytes(block[offset:offset + 4], "little") for offset in range(0, len(block), 4)]


def guarded(rng: random.Random, size: int) -> bytearray:
    return bytearray(rng.randbytes(size + 2 * GUARD))


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % len(FUNCTIONS), number // len(FUNCTIONS)
    guards = tuple(rng.getrandbits(32) for _ in range(8))
    if function < 2:
        stats = guarded(rng, STATS_SIZE)
        increased, decreased = rng.randrange(8), rng.randrange(8)
        amount = rng.choice(BOUNDARIES) if lane < 256 or rng.random() < 0.6 else rng.getrandbits(32)
        mask = lane % 256 if lane < 256 else rng.randrange(256)
        if lane < 256:
            increased, decreased = lane % 8, (lane // 8) % 8
        elif rng.random() < 0.3:
            decreased = increased
        for index, (identity, current, _) in enumerate(EQUIPMENT):
            put(stats, GUARD + identity, rng.randint(1, 65535) if mask & (1 << index) else 0, 2)
            for slot in range(8):
                if lane < 256 or rng.random() < 0.5:
                    put(stats, GUARD + current + slot * 2, rng.choice(BOUNDARIES), 2)
        return Vector(function, (increased, decreased, amount), guards, ((STATS - GUARD, bytes(stats)),))

    globals_ = guarded(rng, GLOBALS_SIZE)
    actor = guarded(rng, ACTOR_SIZE)
    if function == 2:
        actor_slot = lane % 42 if lane < 256 else rng.randrange(42)
        spawn = (0, 127, 128, 129, 255)[(lane // 42) % 5] if lane < 256 else rng.choice(
            (128, 128, rng.randrange(256)))
        # Explicit player exception, with the spawn read preceding the exception.
        if lane % 16 == 0:
            actor_slot, spawn = 0, 128
        arguments = (actor_slot, rng.getrandbits(32),
                     BOUNDARIES[lane % len(BOUNDARIES)] if lane < 256 else rng.getrandbits(32),
                     rng.getrandbits(32), spawn, rng.getrandbits(32))
        put(globals_, GUARD + ACTORS - GLOBALS + actor_slot * 4, ACTOR)
        put(actor, GUARD + 0x27, spawn, 1)
        return Vector(function, arguments, guards,
                      ((GLOBALS - GUARD, bytes(globals_)), (ACTOR - GUARD, bytes(actor))))

    control = rng.choice((2, 2, 2, 0, 1, 3, 0x80000000, MASK))
    flow = rng.choice((0, 1, 2, 3, 4, 0x7FFFFFFF, 0x80000000, MASK))
    timer = rng.choice((0, 0, 0, 1, 29, 30, MASK, 0x80000000, 0x7FFFFFFF, rng.getrandbits(32)))
    ticks = rng.choice((0, 1, 29, 30, 31, MASK, 0x80000000, 0x7FFFFFFF, rng.getrandbits(32)))
    target_slot = rng.choice((0, 0, 1, 7, 15))
    statuses, position = rng.getrandbits(32), rng.choice((0, 0, 1, MASK, 0x80000000))
    risk = rng.choice((0, 1, 98, 99, 100, 101, 65535, rng.getrandbits(16)))
    if lane < 256:
        scenario = lane % 32
        control, flow, timer, target_slot, statuses, position = 2, 1, 0, 0, 8, 0
        risk = (0, 99, 100, 65535)[(lane // 32) % 4]
        ticks = (0, 1, 29, 30, MASK, 0x80000000, 0x7FFFFFFF)[(lane // 4) % 7]
        if scenario < 8:
            flow = (0, 1, 2, 3, 4, 0x7FFFFFFF, 0x80000000, MASK)[scenario]
        elif scenario < 12:
            control = (0, 1, 3, MASK)[scenario - 8]
        elif scenario < 18:
            timer = (1, 29, 30, MASK, 0x80000000, 0x7FFFFFFF)[scenario - 12]
        elif scenario < 21:
            statuses = (0, 7, 0xFFFFFFF7)[scenario - 18]
        elif scenario < 24:
            position = (1, MASK, 0x80000000)[scenario - 21]
        elif scenario < 27:
            target_slot = (1, 7, 15)[scenario - 24]
        elif scenario == 27:
            timer, ticks = 0x7FFFFFFF, 1
        elif scenario == 28:
            timer, ticks = 0x80000000, MASK
    active = function == 3 and control == 2 and 1 <= flow <= 3
    needs_target = active and timer == 0
    needs_player = needs_target and statuses & 8 != 0 and position == 0 and target_slot == 0
    target_present = int(needs_target or lane % 2 == 0)
    player_present = int(needs_player or lane % 2 == 0)
    arguments = (control, flow, timer, ticks, target_slot, statuses, position, risk,
                 target_present, player_present)
    selected_stats, player_stats = guarded(rng, STATS_SIZE), guarded(rng, STATS_SIZE)
    selection, player = guarded(rng, SELECTION_SIZE), guarded(rng, ACTOR_SIZE)
    clock, speed = guarded(rng, 4), guarded(rng, 4)
    for address, value in ((CONTROL, control), (FLOW, flow),
                           (ACTORS + target_slot * 4, ACTOR),
                           (SELECTION_POINTER, SELECTION if target_present else 0x80F00000),
                           (PLAYER_POINTER, PLAYER if player_present else 0x80F10000)):
        put(globals_, GUARD + address - GLOBALS, value)
    put(actor, GUARD + 0x3C, STATS)
    put(player, GUARD + 0x3C, PLAYER_STATS)
    put(selected_stats, GUARD + 0x948, statuses)
    # Distinct owners, including opposite sides of the cap; aliasing would change the wrong bytes.
    put(selected_stats, GUARD + 0x20, 100 if risk < 100 else 99, 2)
    put(player_stats, GUARD + 0x20, risk, 2)
    put(selection, GUARD + 0xC, target_slot, 1)
    put(selection, GUARD + 0x4C, position)
    put(clock, GUARD, timer)
    put(speed, GUARD, ticks)
    blocks = ((GLOBALS - GUARD, globals_), (ACTOR - GUARD, actor), (PLAYER - GUARD, player),
              (STATS - GUARD, selected_stats), (PLAYER_STATS - GUARD, player_stats),
              (SELECTION - GUARD, selection), (TIMER - GUARD, clock), (TICKS - GUARD, speed))
    return Vector(function, arguments, guards, tuple((address, bytes(block)) for address, block in blocks))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    for address, block in vector.blocks:
        machine.store(address, block)
    args = vector.arguments
    inputs = [vector.function, *vector.guards, *args]
    load, write = machine.load, machine.write

    def observe_load(address: int, size: int = 4, signed: bool = False) -> int:
        value = load(address, size, signed)
        if vector.function == 2 and address == ACTOR + 0x27:
            machine.trace.append((0, args[0], value, 0, 0))
        elif vector.function == 3:
            if address in (CONTROL, FLOW, TICKS):
                machine.trace.append(((CONTROL, FLOW, TICKS).index(address), value & MASK, 0, 0, 0))
            elif address == STATS + 0x948:
                machine.trace.append((3, args[4], value & MASK, int(args[6] != 0), 0))
            elif address == PLAYER_STATS + 0x20:
                machine.trace.append((4, value, 0, 0, 0))
        return value

    def observe_write(address: int, value: int, size: int) -> None:
        if vector.function == 3 and address == PLAYER_STATS + 0x20:
            machine.trace.append((5, value & 0xFFFF, 0, 0, 0))
        write(address, value, size)

    def queue_number(m: Machine) -> None:
        m.trace.append((1, *m.r[4:8]))
        m.r[2] = args[5]  # Arbitrary callee result: the wrapper is void.

    machine.load, machine.write = observe_load, observe_write
    if vector.function < 2:
        machine.r[4:8] = [STATS, *args]
        inputs.extend(words(vector.blocks[0][1]))
    elif vector.function == 2:
        machine.r[4:8] = args[:4]
        machine.stub(QUEUE_NUMBER, queue_number)
    machine.execute(ENTRIES[vector.function], limit=256)
    machine.load, machine.write = load, write
    return {
        "blocks": tuple((address, machine.bytes(address, len(block))) for address, block in vector.blocks),
        "stores": list(machine.stores),
        "trace": list(machine.trace),
    }, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    if len(native) < len(NAMES):
        return ["truncated native output"]
    problems = field_problems(NAMES, native[:4], [0, 0, 1, 1])
    expected_blocks = dict(vector.blocks)
    stores, calls = [], []
    args = vector.arguments
    if vector.function < 2:
        if len(native) != 4 + (STATS_SIZE + 2 * GUARD) // 4:
            return problems + ["wrong native equipment image length"]
        expected_blocks[STATS - GUARD] = b"".join(word.to_bytes(4, "little") for word in native[4:])
        original = dict(vector.blocks)[STATS - GUARD]
        for identity, current, modifier in EQUIPMENT[:1] if vector.function == 0 else EQUIPMENT[1:]:
            if int.from_bytes(original[GUARD + identity:GUARD + identity + 2], "little"):
                stores.extend((STATS + offset + index * 2, 2)
                              for offset, index in ((current, args[0]), (current, args[1]),
                                                    (modifier, args[0]), (modifier, args[1])))
        changed = {address - (STATS - GUARD) + byte for address, size in stores for byte in range(size)}
        actual = expected_blocks[STATS - GUARD]
        if any(actual[index] != original[index] for index in range(len(original)) if index not in changed):
            problems.append("native changed unequipped equipment, row 1, other fields or image guards")
    elif vector.function == 2:
        stores = [(DEFAULT_STACK - 8, 4)]
        if len(native) < 5 or len(native) != 5 + native[4] * 5:
            return problems + ["wrong native number trace length"]
        calls = [tuple(native[offset:offset + 5]) for offset in range(5, len(native), 5)]
    else:
        if len(native) < 7 or len(native) != 7 + native[6] * 5:
            return problems + ["wrong native risk trace length"]
        clock = bytearray(expected_blocks[TIMER - GUARD])
        put(clock, GUARD, native[4])
        expected_blocks[TIMER - GUARD] = bytes(clock)
        player = bytearray(expected_blocks[PLAYER_STATS - GUARD])
        put(player, GUARD + 0x20, native[5], 2)
        expected_blocks[PLAYER_STATS - GUARD] = bytes(player)
        calls = [tuple(native[offset:offset + 5]) for offset in range(7, len(native), 5)]
        control, flow, timer, _, slot, statuses, position, risk, *_ = args
        if vector.function == 4:
            stores = [(TIMER, 4)]
            if native[5] != risk:
                problems.append("reset changed player risk")
        elif control == 2 and 1 <= flow <= 3:
            if timer == 0 and statuses & 8 and position == 0 and slot == 0 and risk < 100:
                stores.append((PLAYER_STATS + 0x20, 2))
            stores.append((TIMER, 4))
    for address, actual in observation["blocks"]:
        expected = expected_blocks[address]
        if actual != expected:
            offsets = [hex(index) for index in range(len(actual)) if actual[index] != expected[index]]
            problems.append(f"image {address:#x} differs at {','.join(offsets[:12])}")
    if observation["stores"] != stores:
        problems.append(f"store sequence {observation['stores']} != {stores}")
    if observation["trace"] != calls:
        problems.append(f"calls/reads {observation['trace']} != {calls}")
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = []
    for name, start, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True):
        lines.append(f"{name} {start:08X}-{end - 4:08X} inclusive: {(end - start) // 4} "
                     f"original instructions; sha256={image.sha256(start, end)}")
    lines.extend((
        f"F00527/F00528 80088B8C-80088CA8 inclusive: 72 original instructions; "
        f"sha256={image.sha256(ENTRIES[3], ENDS[4])}",
        "Three disjoint spans: 65 + 20 + 72 = 157 original instructions. Intervening code is excluded; "
        "F00521 ends at 800875C8 exclusive, not the next cataloged symbol.",
        "Compared complete guarded equipment images, exact original store sequences, number read/call "
        "order and all four arguments, lazy risk-world access order, separate target/player stats, "
        "timer/risk values and every other mapped byte. Native input snapshots and canaries are checked. "
        "Number callee return values are deliberately ignored (void wrapper).",
    ))
    for function, name in enumerate(FUNCTIONS):
        selected = [(v, obs) for v, obs, _ in results if v.function == function]
        count = len(selected)
        if function < 2:
            equal = sum(v.arguments[0] == v.arguments[1] for v, _ in selected)
            negative = sum(s32(v.arguments[2]) < 0 for v, _ in selected)
            absent = sum(not obs["stores"] for _, obs in selected)
            lines.append(f"{name}: {count} vectors, {equal} equal-index, {negative} negative/full-width "
                         f"modifiers, {absent} entirely absent loadouts; sparse equipment and wrap boundaries.")
        elif function == 2:
            queued = sum(any(event[0] == 1 for event in obs["trace"]) for _, obs in selected)
            player_exception = sum(v.arguments[0] == 0 and v.arguments[4] == 128 for v, _ in selected)
            lines.append(f"{name}: {count} vectors, {queued} queued, {count - queued} suppressed, "
                         f"{player_exception} actor-zero exceptions; all 42 valid actor slots.")
        else:
            increments = sum(any(event[0] == 5 for event in obs["trace"]) for _, obs in selected)
            target_reads = sum(any(event[0] == 3 for event in obs["trace"]) for _, obs in selected)
            negative_flow = sum(s32(v.arguments[1]) < 0 for v, _ in selected)
            negative_time = sum(s32(v.arguments[2]) < 0 or s32(v.arguments[3]) < 0 for v, _ in selected)
            lines.append(f"{name}: {count} vectors, {increments} risk increments, {target_reads} target reads, "
                         f"{negative_flow} negative/high flow, {negative_time} signed timer/tick edges.")
    return lines
