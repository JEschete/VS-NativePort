"""Eight stat-factor entries, including three byte-identical original aliases."""

import random
import zlib
from dataclasses import dataclass

from differential import DEFAULT_STACK, MASK, Image, Machine

ID = "F00412-F00417 (182 instructions); F00419/F00426 disjoint spans below"
ENTRY, END = 0x80082144, 0x8008241C
ENTRIES = (ENTRY, 0x800821B0, 0x80082234, 0x800822A0, 0x8008231C, 0x80082360,
           0x800824D8, 0x80083524)
ENDS = (0x800821B0, 0x80082234, 0x800822A0, 0x8008231C, 0x80082360, END,
        0x8008255C, 0x80083590)
FUNCTIONS = ("F00412", "F00413", "F00414", "F00415", "F00416", "F00417", "F00419", "F00426")
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "stat_factors_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\StatFactors.cpp",)
VECTORS, SEED, MUTANT_VECTORS = 16384, 412413414415416417419426, 4096
GUARD = 16
GLOBAL, TABLE, DEFINITIONS = 0x800F19CC, 0x800F1928, 0x8004B9DC
STATE, ACTION, SOURCE, TARGET = 0x80500000, 0x80501000, 0x80502000, 0x80502100
ACTORS, STATS = 0x80600000, 0x80700000
MUTANTS = {}
for index in (0, 2, 7):
    entry, name = ENTRIES[index], FUNCTIONS[index]
    for label, offset, word in (
        ("unsigned-hp", 0x1C, 0x94C30058), ("unsigned-mp", 0x20, 0x94C5005A),
        ("subtract-mp", 0x28, 0x00651823), ("factor-mask", 0x24, 0x3042000F),
        ("wrong-hit", 0x10, 0x8C820018), ("divide-five", 0x48, 0x00051043),
        ("wrong-queue", 0x50, 0x94C30004), ("no-minus-one", 0x54, 0x34040000),
        ("unsigned-return", 0x68, 0x00021402),
    ):
        MUTANTS[f"{name}-{label}"] = {entry + offset: word}
for index in (1, 6):
    entry, name = ENTRIES[index], FUNCTIONS[index]
    current = 0x18 if index == 1 else 0x1C
    for label, offset, word in (
        ("target-owner", 4, 0x90C20000),
        ("unsigned-maximum", 0x24, 0x94450000 | (current + 2)),
        ("unsigned-current", 0x28, 0x94430000 | current),
        ("sum-not-missing", 0x30, 0x00A32821),
        ("factor-mask", 0x38, 0x3042000F), ("divide-five", 0x58, 0x00031043),
        ("wrong-queue", 0x68, 0x94630004), ("no-minus-one", 0x50, 0x34040000),
        ("unsigned-return", 0x80, 0x00021402),
    ):
        MUTANTS[f"{name}-{label}"] = {entry + offset: word}
MUTANTS.update({
    "F00415-source-owner": {0x800822AC: 0x90A30000},
    "F00415-unsigned-hp": {0x800822CC: 0x94640018},
    "F00415-factor-mask": {0x800822D0: 0x3042000F},
    "F00415-divide-fifty": {0x800822F0: 0x00031103},
    "F00415-wrong-queue": {0x80082300: 0x94630004},
    "F00415-no-minus-one": {0x80082304: 0x34040000},
    "F00415-unsigned-return": {0x80082318: 0x00021402},
    "F00416-target-owner": {0x80082328: 0x90C30000},
    "F00416-wrong-pp": {0x80082348: 0x9442014E},
    "F00416-byte-pp": {0x80082348: 0x9042014C},
    "F00416-no-minus-one": {0x8008232C: 0x34050000},
    "F00416-wrong-queue": {0x80082344: 0x94630004},
    "F00416-unsigned-return": {0x8008235C: 0x00021402},
    "F00417-wrong-selected-id": {0x80082370: 0x94A3000A},
    "F00417-wrong-type": {0x800823A0: 0x24020002},
    "F00417-type-mask": {0x8008239C: 0x30440001},
    "F00417-skip-type-gate": {0x800823A4: 0},
    "F00417-effect-wide-mask": {0x800823B4: 0x30430007},
    "F00417-effect-two": {0x800823C0: 0x24020002},
    "F00417-skip-effect-gate": {0x800823C4: 0},
    "F00417-unsigned-hp": {0x800823D4: 0x94A30010},
    "F00417-unsigned-mp": {0x800823D8: 0x94A40012},
    "F00417-subtract-mp": {0x800823E0: 0x00641823},
    "F00417-factor-mask": {0x800823E8: 0x3042000F},
    "F00417-divide-five": {0x80082408: 0x00051043},
    "F00417-unsigned-return": {0x80082418: 0x00021402},
})


@dataclass(frozen=True)
class Vector:
    function: int
    arguments: tuple[int, ...]
    blocks: tuple[tuple[int, bytes], ...]


def put(block: bytearray, offset: int, value: int, size: int = 4) -> None:
    block[GUARD + offset:GUARD + offset + size] = (
        value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 8, number // 8
    selected = (0, 255)[lane % 2]
    locations = ((TABLE, 42 * 4), (STATE, 0x60), (ACTION, 0x1C),
                 (SOURCE, 8), (TARGET, 8), (ACTORS, 0x40), (ACTORS + 0x100, 0x40),
                 (STATS, 0x150), (STATS + 0x1000, 0x150),
                 (DEFINITIONS + 52 * selected, 4))
    blocks = [bytearray(rng.randbytes(size + 2 * GUARD)) for _, size in locations]
    # The original action-state pointer follows actor slots 0..40 in this region.
    put(blocks[0], GLOBAL - TABLE, STATE)
    slots = (0, 39) if lane % 2 else (1, 40)
    for owner, slot in enumerate(slots):
        put(blocks[0], slot * 4, ACTORS + owner * 0x100)
        put(blocks[3 + owner], 0, slot, 1)
        put(blocks[5 + owner], 0x3C, STATS + owner * 0x1000)
        for offset in (0x18, 0x1A, 0x1C, 0x1E, 0x14C):
            value = rng.choice((0, 1, -1, -32768, 32767, 65535, rng.randrange(65536)))
            put(blocks[7 + owner], offset, value, 2)
    queue = (0, 1, 32768, 65535)[lane % 4] | (rng.randrange(1, 65536) << 16)
    put(blocks[1], 0, queue)
    put(blocks[1], 8, selected, 2)
    put(blocks[1], 0x52, (0, 1, 255)[lane % 3], 2)
    for offset in (0x10, 0x12, 0x58, 0x5A):
        value = rng.choice((0, 1, -1, -32768, 32767, rng.randrange(-32768, 32768)))
        put(blocks[1], offset, value, 2)
    effect, action_type = (lane // 8) % 4, lane % 8
    put(blocks[1], 0x28, (rng.getrandbits(32) & ~3) | effect)
    put(blocks[9], 0, (rng.getrandbits(32) & ~0xE0000) | (action_type << 17))
    for hit in range(2):
        factor = (0, 31, 1, 3, rng.randrange(32))[(lane + hit) % 5]
        put(blocks[2], 0x14 + 4 * hit,
            (rng.getrandbits(32) & ~(31 << 22)) | (factor << 22))
    action, hit = ACTION, (lane // 2) % 2
    source, target = SOURCE, TARGET
    if function in (0, 2, 5, 7):
        source = target = 0
    elif function in (1, 4, 6):
        # Keep a valid different unused target in half the vectors for owner mutants.
        target = TARGET if lane % 2 else 0
    elif function == 3:
        source = SOURCE if lane % 2 else 0
    if function == 4 or (function == 5 and (action_type != 1 or effect not in (1, 3))):
        action, hit = 0, MASK
    arguments = (action, source, target, hit, rng.getrandbits(32))
    return Vector(function, arguments, tuple(
        (address - GUARD, bytes(block)) for (address, _), block in zip(locations, blocks, strict=True)))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    inputs = [vector.function, *vector.arguments, len(vector.blocks)]
    for address, block in vector.blocks:
        machine.store(address, block)
        inputs.extend((address, len(block) // 4))
        inputs.extend(int.from_bytes(block[i:i + 4], "little") for i in range(0, len(block), 4))
    machine.r[4:8] = vector.arguments[:4]
    machine.write(DEFAULT_STACK + 16, vector.arguments[4], 4)
    machine.stores.clear()
    machine.execute(ENTRIES[vector.function], limit=100)
    memory = b"".join(machine.bytes(address, len(block)) for address, block in vector.blocks)
    return {"result": machine.r[2], "crc": zlib.crc32(memory),
            "unchanged": all(machine.bytes(address, len(block)) == block
                             for address, block in vector.blocks),
            "stores": machine.stores, "calls": machine.trace}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int],
          reference: dict) -> list[str]:
    del image, vector, reference
    expected = [1, observation["result"], observation["crc"], 1]
    problems = [] if native == expected else [f"native {native} != {expected}"]
    if observation["stores"] or observation["calls"] or not observation["unchanged"]:
        problems.append("unexpected write, callback or guarded memory change")
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = []
    for index, (name, entry, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        count = sum(vector.function == index for vector, _, _ in results)
        mismatches = sum(bool(check(image, vector, observation, native, observation))
                         for vector, observation, native in results if vector.function == index)
        lines.append(f"{name} [{entry:08X},{end:08X}) {(end-entry)//4} instructions "
                     f"sha256={image.sha256(entry, end)}; {count} vectors; mismatches={mismatches}")
    return lines
