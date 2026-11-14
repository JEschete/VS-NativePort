"""Shared R3000 fixture and native input layout for MENU4 equipment stats."""

from __future__ import annotations

import random

import differential
from differential import Image, Machine, R
from native_compile_support import (
    COMPILER as COMPILER,
)
from native_compile_support import (
    COMPILER_OPTIONS as COMPILER_OPTIONS,
)
from native_compile_support import (
    CXX_STANDARD as CXX_STANDARD,
)

MODULE = "MENU/MENU4.PRG"
MODULE_BASE = 0x80102800
STATS = 0x801024C0
PRIMARY = 0x801024A8
SUBTYPE = 0x80102545
RANGE_RISK = 0x80430000
DP_PP = 0x80430100
ENABLED_PAGES = 0x80430200
ITEM = 0x80410000
ACTOR = 0x80440000

RECORD_SIZES = (0x198, 0x164, 0xBC, 0x60, 0x198, 0x30, 0x30)
ID_OFFSETS = (0x18, 0x18, 0, 0, 0x18, 0, 0)
INCLUDES = (
    "Native\\Game\\src",
    "Native\\Engine",
    "Native\\Content",
    "Native\\Engine\\src",
    "Native\\Content\\src",
)
OBSERVATION_FIELDS = (
    *(f"stats[{index}]" for index in range(64)),
    *(f"primary[{index}]" for index in range(8)),
    *(f"range_risk[{index}]" for index in range(4)),
    "current_dp",
    "max_dp",
    "current_pp",
    "max_pp",
    "subtype",
    "enabled_pages",
)

differential.MODULE_BASES[MODULE] = MODULE_BASE


def _halfwords(values: list[int]) -> bytes:
    return b"".join((value & 0xFFFF).to_bytes(2, "little") for value in values)


def _put_halfwords(machine: Machine, address: int, values: list[int]) -> None:
    machine.store(address, _halfwords(values))


def vector(rng: random.Random, number: int, kind: int) -> dict:
    record = bytearray(rng.getrandbits(8) for _ in range(RECORD_SIZES[kind]))
    occupied = number % 3 != 0
    identifier = 1 if occupied else 0
    offset = ID_OFFSETS[kind]
    record[offset:offset + 2] = identifier.to_bytes(2, "little")
    equipped = kind in (0, 1, 2) and bool(number & 1)

    return {
        "kind": kind,
        "record": bytes(record),
        "equipped": equipped,
        "stats": [rng.randrange(-30000, 30001) for _ in range(64)],
        "primary": [rng.randrange(-30000, 30001) for _ in range(8)],
        "range_risk": [rng.randrange(256) for _ in range(4)],
        "dp_pp": [rng.randrange(65536) for _ in range(4)],
    }


def run(machine: Machine, case_vector: dict, entry: int) -> tuple[list[int], list[int]]:
    kind = case_vector["kind"]
    record = case_vector["record"]
    machine.store(ITEM, record)
    _put_halfwords(machine, STATS, case_vector["stats"])
    _put_halfwords(machine, PRIMARY, case_vector["primary"])
    machine.store(RANGE_RISK, bytes(case_vector["range_risk"]))
    _put_halfwords(machine, DP_PP, case_vector["dp_pp"])
    machine.put(SUBTYPE, 0x55, 1)
    machine.put(ENABLED_PAGES, 0x55, 1)

    machine.put(0x800F19FC, ACTOR)
    occupied_actor_offsets = {
        0: (0x3C,),
        1: (0x1D4,),
        2: (0x3B8, 0x494, 0x570, 0x64C, 0x728),
    }
    pointed_offsets = occupied_actor_offsets.get(kind, ())
    if pointed_offsets:
        offset = pointed_offsets[0]
        stats_base = ITEM - offset
        if not case_vector["equipped"]:
            stats_base += 0x1000
    else:
        stats_base = 0x80441000
    machine.put(ACTOR + 0x3C, stats_base)

    def reset_stats(active: Machine) -> None:
        active.store(STATS, bytes(0x80))
        active.store(RANGE_RISK, bytes(4))
        _put_halfwords(active, DP_PP, [0, 0, 0, 0])
        _put_halfwords(active, PRIMARY, [0, 0, 0, 1, 0, 0, 0, 1])

    def copy_bytes(active: Machine) -> None:
        destination = active.r[R["a0"]]
        source = active.r[R["a1"]]
        size = active.r[R["a2"]]
        active.store(destination, active.bytes(source, size))

    def set_range_risk(active: Machine) -> None:
        values = [active.r[R[name]] & 0xFF for name in ("a0", "a1", "a2", "a3")]
        active.store(RANGE_RISK, bytes(values))

    def set_dp_pp(active: Machine) -> None:
        arguments = [active.r[R[name]] for name in ("a0", "a1", "a2", "a3")]
        rounded = [(arguments[index] + 99) // 100 for index in (0, 1)]
        active_values = rounded + [arguments[2] & 0xFFFF, arguments[3] & 0xFFFF]
        _put_halfwords(active, DP_PP, active_values)

    def set_primary(active: Machine) -> None:
        values = [active.r[R[name]] for name in ("a0", "a1", "a2", "a3")]
        _put_halfwords(active, PRIMARY, values + values)
        active.store(RANGE_RISK, bytes(4))

    def set_enabled_pages(active: Machine) -> None:
        active.put(ENABLED_PAGES, active.r[R["a0"]], 1)

    for address, handler in (
        (0x800FD220, reset_stats),
        (0x800C01E0, copy_bytes),
        (0x800FBD0C, set_range_risk),
        (0x800FC208, set_dp_pp),
        (0x800FBD28, set_primary),
        (0x800FBB8C, set_enabled_pages),
        (0x800FC268, None),
    ):
        machine.stub(address, handler)

    machine.r[R["a0"]] = ITEM
    machine.execute(entry)

    observation = [
        *(machine.load(STATS + index * 2, 2, signed=True) for index in range(64)),
        *(machine.load(PRIMARY + index * 2, 2, signed=True) for index in range(8)),
        *(machine.load(RANGE_RISK + index, 1) for index in range(4)),
        *(machine.load(DP_PP + index * 2, 2) for index in range(4)),
        machine.load(SUBTYPE, 1),
        machine.load(ENABLED_PAGES, 1),
    ]
    inputs = [
        kind,
        int(case_vector["equipped"]),
        *case_vector["stats"],
        *case_vector["primary"],
        *case_vector["range_risk"],
        *case_vector["dp_pp"],
        *record,
    ]
    return observation, inputs


def check(
    image: Image,
    case_vector: dict,
    observation: list[int],
    native: list[int],
    reference: list[int],
) -> list[str]:
    del image, case_vector, reference
    differences = [
        f"{name}: native {actual} original {expected}"
        for name, actual, expected in zip(
            OBSERVATION_FIELDS, native, observation, strict=False
        )
        if actual != expected
    ]
    return differences[:8]
