"""Eight separately bounded HUD controls; shared original R3000 execution only."""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, MASK, Machine, s16, s32, store_problems

SPECS = {
    "F00235": (0, 0x8007BD8C, 0x8007BDD8),
    "F01356": (1, 0x800C9E08, 0x800C9E5C),
    "F01357": (2, 0x800C9E5C, 0x800C9EB8),
    "F01359": (3, 0x800C9F88, 0x800C9F98),
    "F01382": (4, 0x800CB550, 0x800CB560),
    "F01383": (5, 0x800CB560, 0x800CB56C),
    "F01385": (6, 0x800CB654, 0x800CB660),
    "F01386": (7, 0x800CB660, 0x800CB66C),
}
ACTOR = 0x80500000
STATUS_OFFSET = 0x948
LIMB = 0x80600000
TABLE = 0x800EBC68
HIDDEN = 0x800EB9AF
MODE = 0x800EB9CC
SHORTCUT = 0x800EBC78
LOWER_SCREEN = 0x800F4E90
MINIMAP = 0x800F1CD6
HIDE_ENTRY = SPECS["F01385"][1]
MINIMAP_ENTRY = 0x8008B4C8
# Eight full words, including slide offset, indicator/carousel, animation and
# lower-screen peers. The status mapping ends before the shortcut request alias.
WINDOWS = (
    (0x800EB9AC, 8),
    (MODE, 8),
    (SHORTCUT, 4),
    (LOWER_SCREEN, 8),
    (0x800F1CD4, 4),
)
TARGETS = {
    0: (HIDDEN, MINIMAP),
    1: (),
    2: (),
    3: (SHORTCUT,),
    4: (LOWER_SCREEN,),
    5: (LOWER_SCREEN,),
    6: (HIDDEN,),
    7: (MODE,),
}
MUTATIONS = {
    "F00235": {
        "F00235-truncated-command": {0x8007BD94: 0x309000FF},
        "F00235-no-hide-call": {0x8007BDA0: 0x00000000},
        "F00235-hide-command-two": {0x8007BDA4: 0x24040002},
        "F00235-overlay-hidden-visible": {0x8007BDAC: 0x24040001},
        "F00235-no-show-call": {0x8007BDB0: 0x00000000},
        "F00235-show-command-one": {0x8007BDB4: 0x24040001},
        "F00235-wrong-final-helper": {0x8007BDBC: 0x0C032D95},
        "F00235-return-normalized": {0x8007BDC4: 0x0010102B},
    },
    "F01356": {
        "F01356-direct-shift-six": {0x800C9E18: 0x00031982},
        "F01356-low-mask-15-bits": {0x800C9E1C: 0x30667FFF},
        "F01356-mapped-shift-17": {0x800C9E20: 0x00031C43},
        "F01356-byte-table-load": {0x800C9E34: 0x90820000},
        "F01356-icon-shift-15": {0x800C9E3C: 0x000213C0},
        "F01356-no-merge": {0x800C9E40: 0x00000000},
        "F01356-only-seven-mappings": {0x800C9E48: 0x28A20007},
        "F01356-extra-mapping-alias": {0x800C9E48: 0x28A20009},
        "F01356-byte-table-stride": {0x800C9E50: 0x24840001},
    },
    "F01357": {
        "F01357-unsigned-hp": {0x800C9E5C: 0x94830000},
        "F01357-unsigned-max-hp": {0x800C9E60: 0x94840002},
        "F01357-critical-bound-one": {0x800C9E64: 0x28620001},
        "F01357-hp-scale-two": {0x800C9E6C: 0x00032840},
        "F01357-critical-result-one": {0x800C9E74: 0x24020001},
        "F01357-unsigned-quarter": {0x800C9E78: 0x00A4102B},
        "F01357-max-scale-two": {0x800C9E80: 0x00041000},
        "F01357-damaged-result-two": {0x800C9E88: 0x24020002},
        "F01357-unsigned-three-quarters": {0x800C9E90: 0x00A2102B},
        "F01357-wounded-result-one": {0x800C9E98: 0x24020001},
        "F01357-good-result-four": {0x800C9EA4: 0x24020004},
        "F01357-excellent-result-three": {0x800C9EAC: 0x24020003},
    },
    "F01359": {
        "F01359-not-requested": {0x800C9F8C: 0x24020000},
        "F01359-wide-request": {0x800C9F94: 0xA462BC78},
        "F01359-request-peer": {0x800C9F94: 0xA062BC79},
    },
    "F01382": {
        "F01382-disabled": {0x800CB554: 0x24020000},
        "F01382-wide-timer": {0x800CB55C: 0xAC624E90},
        "F01382-timer-peer": {0x800CB55C: 0xA0624E91},
    },
    "F01383": {
        "F01383-wide-disable": {0x800CB568: 0xA4404E90},
        "F01383-disable-peer": {0x800CB568: 0xA0404E91},
    },
    "F01385": {
        "F01385-command-zero": {0x800CB65C: 0xA040B9AF},
        "F01385-wide-visibility": {0x800CB65C: 0xAC44B9AC},
        "F01385-status-icon-peer": {0x800CB65C: 0xA044B9AE},
    },
    "F01386": {
        "F01386-command-zero": {0x800CB668: 0xA040B9CC},
        "F01386-wide-mode": {0x800CB668: 0xAC44B9CC},
        "F01386-indicator-peer": {0x800CB668: 0xA044B9CD},
    },
}


def vector_for(identifier: str, rng: random.Random, number: int) -> list[int]:
    operation = SPECS[identifier][0]
    commands = (
        0,
        1,
        -1,
        2,
        3,
        255,
        256,
        -256,
        257,
        0x7FFFFFFF,
        -0x80000000,
        -0x7FFFFF00,
        0x7FFFFF00,
    )
    command = (
        commands[number % len(commands)] if number < 256 else s32(rng.getrandbits(32))
    )
    statuses = (
        1 << number
        if number < 32
        else (
            (0, 0xFFFFFFFF, 0xE000001F, 0x001FFFE0, 0x10200020)[number % 5]
            if number < 64
            else rng.getrandbits(32)
        )
    )
    boundaries = (
        (-32768, -32768),
        (-1, 32767),
        (0, 0),
        (1, 0),
        (1, 400),
        (2, 400),
        (99, 400),
        (100, 400),
        (299, 400),
        (300, 400),
        (399, 400),
        (400, 400),
        (401, 400),
        (2, 0),
        (2, -32768),
        (32767, 32767),
        (2, 3),
        (2, 2),
    )
    if number < len(boundaries):
        hp, max_hp = boundaries[number]
    elif number % 3:
        max_hp = rng.choice((0, 1, 2, 3, 4, 100, 400, 32767, -32768, -1))
        hp = s16(
            rng.choice(
                (
                    0,
                    1,
                    2,
                    max_hp // 4,
                    max_hp // 4 + 1,
                    max_hp * 3 // 4,
                    max_hp * 3 // 4 + 1,
                    max_hp - 1,
                    max_hp,
                    max_hp + 1,
                )
            )
        )
    else:
        hp, max_hp = s16(rng.getrandbits(16)), s16(rng.getrandbits(16))
    peers = [rng.getrandbits(32) for _ in range(8)]
    mapping = [rng.getrandbits(16) for _ in range(8)]
    if number < 64:
        mapping = [0x8000, 1, 0x0100, 0xFFFF, 0x7FFF, 0, 0xAA55, 0x0123]
    return [operation, command, statuses, hp, max_hp, *peers, *mapping]


def run_original(machine: Machine, values: list[int]) -> tuple[dict, list[int]]:
    operation, command, statuses, hp, max_hp = values[:5]
    peers, mapping = values[5:13], values[13:]
    cursor = 0
    for base, size in WINDOWS:
        block = b"".join(
            word.to_bytes(4, "little") for word in peers[cursor : cursor + size // 4]
        )
        machine.store(base, block)
        cursor += size // 4
    machine.put(ACTOR + STATUS_OFFSET, statuses)
    machine.put(LIMB, hp, 2)
    machine.put(LIMB + 2, max_hp, 2)
    for slot, icon in enumerate(mapping):
        machine.put(TABLE + slot * 2, icon, 2)
    machine.follow.update((HIDE_ENTRY, MINIMAP_ENTRY))
    calls = []
    original_call = machine.call

    def observe_call(address: int) -> bool:
        # Observation only: both helpers continue through their actual original
        # instructions. No external status, limb or minimap service is stubbed.
        calls.append(
            [
                {HIDE_ENTRY: 0, MINIMAP_ENTRY: 1}.get(address, address),
                machine.r[4] & MASK,
            ]
        )
        return original_call(address)

    machine.call = observe_call
    entry = next(entry for op, entry, _ in SPECS.values() if op == operation)
    machine.r[4] = (
        ACTOR if operation == 1 else LIMB if operation == 2 else command & MASK
    )
    machine.execute(entry)
    after = [
        machine.load(base + offset)
        for base, size in WINDOWS
        for offset in range(0, size, 4)
    ]
    result = (
        machine.r[2] & MASK if operation < 3 else 0
    )  # The other five signatures are void.
    return {
        "return": result,
        "peers": after,
        "calls": calls,
        "stores": machine.stores,
    }, values


def check_original(
    _image, values: list[int], observation: dict, native: list[int], _reference: dict
) -> list[str]:
    if len(native) < 11 or len(native) != 11 + native[10] * 2:
        return ["invalid native protocol"]
    if native[0]:
        return ["native refused a valid original input"]
    problems = []
    if native[1] != observation["return"]:
        problems.append(
            f"return native {native[1]:#x} original {observation['return']:#x}"
        )
    if native[2:10] != observation["peers"]:
        problems.append(
            f"packed peers native {native[2:10]} original {observation['peers']}"
        )
    calls = [native[11 + i * 2 : 13 + i * 2] for i in range(native[10])]
    if calls != observation["calls"]:
        problems.append(
            f"call order/arguments native {calls} original {observation['calls']}"
        )
    allowed = [(address, address + 1) for address in TARGETS[values[0]]]
    if values[0] == 0:
        allowed.append((DEFAULT_STACK - 0x18, DEFAULT_STACK))
    return problems + store_problems(observation["stores"], allowed)


def build_case(identifier: str) -> dict:
    operation, entry, end = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> list[int]:
        return vector_for(identifier, rng, number)

    def describe(image, results) -> list[str]:
        lines = [
            "Executed the complete named function including terminal return delay slots.",
            "Compared eight complete peer words, all allowed stores and the consumed return.",
            "No calculations were stubbed. F00235 follows the actual HUD and minimap helper instructions "
            "and compares their call order/arguments; the native driver uses Field::SetMinimapVisible.",
        ]
        if operation == 2:
            counts = [
                sum(output[1] == level for _, _, output in results)
                for level in range(5)
            ]
            lines.append(
                f"Limb levels 0..4: {counts}; signed halfwords, no division; zero/negative max HP included."
            )
        if operation == 1:
            lines.append(
                "Statuses bits 5..20 directly mapped; 21..28 use eight unsigned halfwords; "
                "bits 0..4 and 29..31 are ignored, including the shortcut-request alias."
            )
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "DRIVER": "hud_state_driver.cpp",
        "SOURCES": (
            r"Native\Game\src\Battle\HudState.cpp",
            r"Native\Game\src\Field\Map.cpp",
            r"Native\Engine\src\Core\Math\ScalarMath.cpp",
            r"Native\Engine\src\Core\Math\Distance.cpp",
        ),
        "VECTORS": 1024,
        "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 256,
        "MUTANTS": MUTATIONS[identifier],
        "vector": vector,
        "run": run_original,
        "check": check_original,
        "describe": describe,
    }
