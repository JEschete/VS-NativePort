"""Shared original-instruction and actual native renderer comparison for six leaves."""

from __future__ import annotations

import random
import struct

from differential import DEFAULT_STACK, Fault, Image, Machine, s32

MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
SOURCES = (
    r"Native\Game\src\Battle\FloatingLabelRendering.cpp",
    r"Native\Engine\src\Render\UiTexturedQuads.cpp",
    r"Native\Engine\src\Render\GameFont.cpp",
    r"Native\Engine\src\Render\UiPackets.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
LABEL = 0x80200000
SCRATCH = 0x1F800000
PRIMITIVES = 0x80180000
PREFIX = 0x80170000
ORDERING = 0x801F8000
CUE = 0x800F2280
END_LINK = 0xFFFFFF
MASK = 0xFFFFFFFF
PRIMITIVE_BYTES = 4096
ADD_PRIMITIVE = 0x8002B4D4
COMPACT_TEXT = 0x800C6540
SPRINTF = 0x80026EF4
MEMMOVE = 0x80027794
PREFIX_WORDS = (0x03FFFFFF, 0x60ABCDEF, 0x000B000A, 0x00020001)
SEED_STRIDE = 0x9E3779B9
EDGE_X = (
    -2147483648,
    -65536,
    -60,
    -49,
    -48,
    -33,
    -32,
    -17,
    -16,
    -1,
    0,
    15,
    16,
    319,
    320,
    334,
    335,
    336,
    65535,
    2147483647,
)


def original_vector(rng: random.Random, number: int, kind: int) -> dict:
    """Bias every animation boundary and retain full-width unused payload fields."""
    state = bytearray(rng.randbytes(24))
    x = EDGE_X[number % len(EDGE_X)] if number % 3 else s32(rng.getrandbits(32))
    y = rng.choice((-32769, -32768, -1, 0, 40, 100, 32767, 32768, 2147483647))
    frame = rng.randrange(256)
    value = rng.getrandbits(16)
    mode = rng.getrandbits(8)
    if kind != 0 and number < 256:
        mode = number
    length = rng.getrandbits(8)
    if kind == 0:
        mode = 3 + number % 6
        length = (0, 1, 2, 3, 4, 127, 128, 255)[number % 8]
        value = (value & 0x7FFF) | ((number % 2) << 15)
    elif kind == 1:
        frame = number % 31
    elif kind == 3:
        frame = number % 46
        value = number % 5
        if value == 2:
            length = (number // 5) % 8
    elif kind == 4:
        frame = number % 91
        value = (0, 1, 2, 9, 10, 99, 999, 1000, 32768, 65535)[number % 10]
        if number < 8:
            frame = number % 4
            value = 1 if number < 4 else 2
    state[11] = frame
    struct.pack_into("<H", state, 12, value)
    state[14] = mode
    state[15] = length
    steps = (-2147483648, -1, 0, 1, 4, 5, 15, 16, 255, 2147483647)
    step = steps[number % len(steps)] if number % 2 else s32(rng.getrandbits(32))
    return {
        "kind": kind,
        "x": x,
        "y": y,
        "step": step,
        "state": bytes(state),
        "seed": rng.getrandbits(32),
    }


def traversal(machine: Machine, bucket: int) -> list[int]:
    """Walk actual OT links, retaining every command word and tag length."""
    result = []
    link = machine.load(bucket) & END_LINK
    visited = set()
    while link != END_LINK:
        if link in visited:
            raise Fault(f"primitive cycle at {link:#x}")
        visited.add(link)
        address = link | 0x80000000
        if address != PREFIX and not (
            PRIMITIVES <= address < PRIMITIVES + PRIMITIVE_BYTES
        ):
            raise Fault(f"primitive left its actual arena at {address:#x}")
        tag = machine.load(address)
        length = tag >> 24
        if length:
            result.append(length << 24)
            result.extend(
                machine.load(address + index * 4) for index in range(1, length + 1)
            )
        link = tag & END_LINK
    return result


def execute_original(
    machine: Machine, vector: dict, entry: int
) -> tuple[dict, list[int]]:
    """Execute every leaf, AddPrim, compact-font and sprintf instruction, without stubs."""
    machine.map_zero(SCRATCH, 0x100)
    machine.map_zero(ORDERING - 0x20, 0x40)
    machine.map_zero(CUE, 8)
    machine.map_zero(PRIMITIVES, PRIMITIVE_BYTES)
    for index in range(PRIMITIVE_BYTES // 4):
        machine.put(
            PRIMITIVES + index * 4,
            vector["seed"] ^ ((index * SEED_STRIDE) & MASK),
        )
    machine.store(PREFIX, struct.pack("<4I", *PREFIX_WORDS))
    machine.store(LABEL, vector["state"])
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ORDERING)
    machine.put(ORDERING - 20, PREFIX & END_LINK)
    machine.put(ORDERING - 16, PREFIX & END_LINK)
    machine.put(CUE, vector["x"], 2)
    machine.put(CUE + 2, vector["y"], 2)
    machine.put(CUE + 4, 0x81FF7FFE)
    before_cue = machine.bytes(CUE, 8)
    machine.follow.update((ADD_PRIMITIVE, COMPACT_TEXT, SPRINTF, MEMMOVE))
    machine.r[4] = vector["x"] & MASK
    machine.r[5] = vector["y"] & MASK
    machine.r[6] = vector["step"] & MASK if vector["kind"] == 2 else LABEL
    machine.execute(entry)
    observation = {
        "return": s32(machine.r[2]) if vector["kind"] == 0 else 0,
        "state": list(struct.unpack("<6I", machine.bytes(LABEL, 24))),
        "used_words": (machine.load(SCRATCH) - PRIMITIVES) // 4,
        "quad_draw": traversal(machine, ORDERING - 20),
        "text_draw": traversal(machine, ORDERING - 16),
        "cue_unchanged": before_cue == machine.bytes(CUE, 8),
        "stores": machine.stores,
        "steps": machine.steps,
    }
    inputs = [
        vector["kind"],
        vector["x"],
        vector["y"],
        vector["step"],
        *struct.unpack("<6I", vector["state"]),
        vector["seed"],
    ]
    return observation, inputs


def check_original(
    _image: Image,
    vector: dict,
    observation: dict,
    native: list[int],
    _reference: dict,
) -> list[str]:
    """Compare exact traversed packets, untouched payload and bounded owner mutations."""
    if len(native) < 11 or native[0] != 1:
        return ["native refused or returned an incomplete observation"]
    problems = []
    if native[1] != observation["return"]:
        problems.append(f"return {native[1]} != {observation['return']}")
    # Native PacketQueue owns complete packets, so it needs no legacy terminal
    # zero-length link word. No command, glyph or draw-order normalization occurs.
    sentinel_words = int(vector["kind"] in (3, 4))
    if native[2] + sentinel_words != observation["used_words"]:
        problems.append(
            f"allocation native {native[2]} + terminal {sentinel_words} "
            f"!= original {observation['used_words']}"
        )
    if native[3:9] != observation["state"]:
        problems.append("24-byte payload changed")
    quad_count = native[9]
    split = 10 + quad_count
    if split >= len(native):
        return [*problems, "truncated native quad traversal"]
    if native[10:split] != observation["quad_draw"]:
        problems.append(
            f"quad traversal {native[10:split]} != {observation['quad_draw']}"
        )
    text_count = native[split]
    if text_count != len(native) - split - 1:
        problems.append("truncated native text traversal")
    if native[split + 1 :] != observation["text_draw"]:
        problems.append("text traversal command words differ")
    if not observation["cue_unchanged"]:
        problems.append("original unexpectedly changed cue BSS")
    allowed = (
        (DEFAULT_STACK - 0x4000, DEFAULT_STACK + 16),
        (PRIMITIVES, PRIMITIVES + PRIMITIVE_BYTES),
        (ORDERING - 20, ORDERING - 12),
        (SCRATCH, SCRATCH + 4),
    )
    for address, size in observation["stores"]:
        if not any(low <= address and address + size <= high for low, high in allowed):
            problems.append(f"store outside required owners: {address:#x}+{size}")
            break
    return problems


def describe_original(_image: Image, results: list) -> list[str]:
    """Expose the actual instruction, animation and traversal coverage achieved."""
    vectors = [vector for vector, _, _ in results]
    observations = [observation for _, observation, _ in results]
    return [
        f"frames={len({vector['state'][11] for vector in vectors})}; "
        f"raw_modes={len({vector['state'][14] for vector in vectors})}; "
        f"max_original_steps={max(item['steps'] for item in observations)}",
        "Exact OT draw traversal compares every packet word, RGB/pad preservation, "
        "screen halfwords, UV, page, palette, 5-word compact glyphs and draw mode.",
        "Both OT buckets include a pre-existing four-word tile; the untouched bucket "
        "and the active bucket's existing tail must survive.",
        "Followed actual AddPrim, compact-font helper and sprintf instructions; "
        "no external callbacks, math stubs or text-lookup stubs.",
        "Only handles are normalized and the compact text's obsolete zero-length "
        "terminal link is accounted separately; no framebuffer/gameplay parity claim.",
    ]


def specification(
    identifier: str,
    entry: int,
    end: int,
    kind: int,
    mutants: dict[str, dict[int, int]],
) -> dict[str, object]:
    """Bind one original ID to the shared, non-interpreter case implementation."""

    def vector(rng: random.Random, number: int) -> dict:
        """Create this leaf's bounded original-input vector."""
        return original_vector(rng, number, kind)

    def run(machine: Machine, value: dict) -> tuple[dict, list[int]]:
        """Run this leaf's complete original range and real SDK helpers."""
        return execute_original(machine, value, entry)

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": MODULES,
        "DRIVER": "floating_label_rendering_driver.cpp",
        "SOURCES": SOURCES,
        "VECTORS": 2048,
        "MUTANT_VECTORS": 192,
        "SEED": int(identifier[1:]) + 20261113,
        "MUTANTS": mutants,
        "vector": vector,
        "run": run,
        "check": check_original,
        "describe": describe_original,
    }
