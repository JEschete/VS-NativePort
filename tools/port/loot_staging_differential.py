from __future__ import annotations

import random

from differential import Machine

COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
COMPILER_OPTIONS = (
    "/J", "/clang:-Wno-error=int-conversion", "/clang:-Wno-error=incompatible-pointer-types",
    "/clang:-Wno-error=incompatible-function-pointer-types", "/clang:-Wno-error=implicit-function-declaration",
    "/clang:-Wno-error=implicit-int", "/clang:-Wno-error=return-type", "/clang:-Wno-error=int-to-pointer-cast",
    "/clang:-Wno-c2x-extensions", "/clang:-Wno-unused", "/clang:-Wno-parentheses",
    "/clang:-Wno-missing-braces", "/clang:-Wno-sign-compare", "/clang:-Wno-pointer-sign",
    "/clang:-Wno-switch", "/clang:-Wno-unknown-pragmas", "/clang:-Wno-microsoft-anon-tag",
    "/clang:-fms-extensions", "/clang:-fno-finite-loops", "/clang:-fno-strict-return",
    "/clang:-fno-delete-null-pointer-checks", "/clang:-fwrapv", "/clang:-Wno-register",
    "/clang:-Wno-writable-strings",
)
INCLUDES = (
    r"Native\Engine", r"Native\Content", r"Native\Engine\include", r"Native\Engine\src",
    r"Native\Content\include", r"Native\Content\src", r"Native\Game\include", r"Native\Game\src",
)
INVENTORY = 0x80500000
SOURCE = 0x80510000
INVENTORY_POINTER = 0x8010A6A8
INVENTORY_BYTES = 0xF00
MASK = 0xFFFFFFFF

WEAPON, BLADE, GRIP, SHIELD, ARMOR, GEM, MISC = range(7)
CAPACITIES = (8, 16, 16, 8, 16, 48, 64)
LAYOUTS = (
    (0x000, 0x20, 1, 1),
    (0x280, 0x2C, 0, 1),
    (0x540, 0x10, 0, 2),
    (0x100, 0x30, 4, 1),
    (0x640, 0x28, 0, 1),
    (0x8C0, 0x1C, 0, 2),
    (0xE00, 0x04, 0, 2),
)


def pattern(size: int, salt: int, row: int = 0) -> bytes:
    seed = (salt + row * 17) & 0xFF
    return bytes((seed + byte * 7) & 0xFF for byte in range(size))


def set_integer(data: bytearray, offset: int, value: int, size: int) -> None:
    data[offset:offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")


def fill_category(data: bytearray, layout: tuple[int, int, int, int],
                  count: int, salt: int, weapon: bool = False) -> None:
    offset, stride, id_offset, id_size = layout
    for row in range(count):
        record = pattern(stride, salt, row)
        start = offset + row * stride
        data[start:start + stride] = record
        set_integer(data, start + id_offset, 1 if weapon else row + 1, id_size)


def source_bytes(kind: int, salt: int, gem_mask: int) -> bytes:
    size = (0xAC, 0x2C, 0x10, 0x80, 0x28, 0x1C, 0x04)[kind]
    source = bytearray(pattern(size, salt))
    if kind == BLADE:
        set_integer(source, 0, salt, 1)
    elif kind == ARMOR:
        set_integer(source, 0, salt, 1)
        set_integer(source, 0x27, 0xFE, 1)
    elif kind == GRIP:
        set_integer(source, 0, 0x100 + salt, 2)
    elif kind == GEM:
        set_integer(source, 0, 0x200 + salt, 2)
    elif kind == WEAPON:
        set_integer(source, 4, salt, 1)
        set_integer(source, 0x30, 0x100 + salt, 2)
        for gem in range(3):
            set_integer(source, 0x40 + gem * 0x1C,
                        0x200 + salt + gem if gem_mask & (1 << gem) else 0, 2)
    elif kind == SHIELD:
        set_integer(source, 4, salt, 1)
        set_integer(source, 0x2A, 0xE2, 1)
        for gem in range(3):
            set_integer(source, 0x2C + gem * 0x1C,
                        0x200 + salt + gem if gem_mask & (1 << gem) else 0, 2)
    else:
        set_integer(source, 0, 0x100 + salt, 2)
        source[2] = 0xA5
        source[3] = salt % 100 + 1
    return bytes(source)


def vector(rng: random.Random, number: int, kind: int) -> dict:
    salt = rng.randrange(1, 256)
    owner = rng.randrange(1, 0x80)
    gem_mask = rng.randrange(8)
    counts = [0] * len(CAPACITIES)

    if kind == BLADE:
        counts[BLADE] = rng.randrange(CAPACITIES[BLADE] - 1)
    elif kind == GRIP:
        counts[GRIP] = rng.randrange(CAPACITIES[GRIP] - 1)
    elif kind == GEM:
        counts[GEM] = rng.randrange(CAPACITIES[GEM] - 1)
    elif kind == WEAPON:
        counts[WEAPON] = rng.randrange(CAPACITIES[WEAPON] - 1)
        counts[BLADE] = rng.randrange(CAPACITIES[BLADE] - 1)
        counts[GRIP] = rng.randrange(CAPACITIES[GRIP] - 1)
        gem_count = gem_mask.bit_count()
        counts[GEM] = rng.randrange(CAPACITIES[GEM] - gem_count - 1)
    elif kind == ARMOR:
        counts[ARMOR] = rng.randrange(CAPACITIES[ARMOR] - 1)
    elif kind == SHIELD:
        counts[SHIELD] = rng.randrange(CAPACITIES[SHIELD] - 1)
        gem_count = gem_mask.bit_count()
        counts[GEM] = rng.randrange(CAPACITIES[GEM] - gem_count - 1)
    else:
        counts[MISC] = rng.randrange(CAPACITIES[MISC] - 1)

    return {
        "kind": kind,
        "salt": salt,
        "owner": owner,
        "gem_mask": gem_mask,
        "counts": counts,
        "number": number,
    }


def run(machine: Machine, vector_value: dict, entry: int) -> tuple:
    kind = vector_value["kind"]
    salt = vector_value["salt"]
    gem_mask = vector_value["gem_mask"]
    target = bytearray(INVENTORY_BYTES)
    for category, layout in enumerate(LAYOUTS):
        fill_category(target, layout, vector_value["counts"][category], salt,
                      weapon=category == WEAPON)
    source = source_bytes(kind, salt, gem_mask)
    machine.put(INVENTORY_POINTER, INVENTORY)
    machine.store(INVENTORY, target)
    machine.store(SOURCE, source)

    def copy_bytes(core: Machine) -> None:
        core.store(core.r[4], core.bytes(core.r[5], core.r[6] & MASK))

    def zero_bytes(core: Machine) -> None:
        core.store(core.r[4], bytes(core.r[5] & MASK))

    machine.stub(0x800C01C0, copy_bytes)
    machine.stub(0x800CCC1C, zero_bytes)
    machine.stub(0x800CCC34, copy_bytes)
    if kind == WEAPON:
        machine.follow.update((0x80107C54, 0x80107CE8, 0x80107D7C))
    elif kind == SHIELD:
        machine.follow.add(0x80107D7C)

    machine.r[4] = SOURCE
    if kind in (BLADE, GRIP, GEM):
        machine.r[5] = vector_value["owner"]
    machine.execute(entry)
    observation = (
        machine.r[2] & MASK,
        machine.bytes(INVENTORY, INVENTORY_BYTES),
    )
    inputs = [
        kind,
        salt,
        vector_value["owner"],
        gem_mask,
        *vector_value["counts"],
    ]
    return observation, inputs


def check(image, vector_value: dict, observation: tuple,
          native: list[int], reference: tuple) -> list[str]:
    result, contents = observation
    if len(native) != INVENTORY_BYTES + 1:
        return [f"native printed {len(native)} values, expected {INVENTORY_BYTES + 1}"]
    problems = []
    if native[0] != result:
        problems.append(f"return native {native[0]:#x} original {result:#x}")
    native_bytes = bytes(value & 0xFF for value in native[1:])
    if native_bytes != contents:
        first = next(i for i, (left, right) in enumerate(zip(native_bytes, contents, strict=True))
                     if left != right)
        problems.append(
            f"inventory byte {first:#x} native {native_bytes[first]:#x} original {contents[first]:#x}")
    return problems
