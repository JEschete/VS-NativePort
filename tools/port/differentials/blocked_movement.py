"""F01636, F01641, F01642, F01644, F01660 blocked movement vs BlockedMovement.cpp.

func_800D8400 (the adjacent step destination), the movement history tests func_800D8EF4
inlines at 0x800D927C (stalled) and 0x800D928C (repeating), func_800D9538 (the saved random
state) and func_800DB0BC (the detour) run with the arctangent followed. The edge test, the
destination occupancy, the gap landing, the random draw, the target forgetting and the locomotion
dispatch are stubs: each call is recorded with its arguments and, from the detour, a snapshot of
the fields the detour writes; the primitives return queued answers.

func_800D8400 kept byte 1 of its caller's $s0 in the tiles it passed to the primitives; the
native passes 0 until a gap landing replaces it. Vectors seed that byte as 0 about half the time,
and otherwise it is not compared in those arguments. A probe direction outside 0..7 reads past
the frame's direction codes; the native refuses it, and only the calls before it are compared.
"""

from __future__ import annotations

import random

from differential import (
    Image,
    Machine,
    call_problems,
    field_problems,
    near,
    pack_fields,
    packed,
    position,
    s32,
)

ID = "F01636, F01641, F01642, F01644, F01660"
ENTRY, END = 0x800DB0BC, 0x800DB370
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "blocked_movement_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\BlockedMovement.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1636
MUTANT_VECTORS = 2048

ADJACENT, ADJACENT_END = 0x800D8400, 0x800D85D8
STALLED, REPEATS, HISTORY_END = 0x800D927C, 0x800D928C, 0x800D92B8
SAVE, SAVE_END = 0x800D9538, 0x800D954C
RECOVER = ENTRY
ARCTANGENT = 0x8004137C
EDGE = 0x800D96C8
OCCUPANCY = 0x800D954C
GAP = 0x800DB93C
RANDOM = 0x800E45D4
FORGET = 0x800E4CE8
DISPATCH = 0x800E50A0

DIRECTION_CODES = 0x800F16EC
SAVED_RANDOM = 0x800F58C4
ARENA_ORBIT = 0x800F5920
SCRATCH = 0x1F800000
# Covers a probe direction up to 0xFFFF past the codes.
SCRATCH_SIZE = 0x10400
SCRATCH_DIRECTIONS = 0x1F8003EC
FRAME_RANDOM = 0x1F8003F8
STATE, STATE_SIZE = 0x80500000, 0x200
STACK = (0x80400000 - 0x4000, 0x80400000)
GRID = 32
NO_TARGET = 16
K_ADJACENT, K_STALLED, K_REPEATS, K_SAVE, K_RECOVER = range(5)
KINDS = ("adjacent", "stalled", "repeats", "save", "recover")

# The actor AI state fields any routine reads or writes: name, offset, size, signed.
FIELDS = (
    ("is_stuck", 0x02, 1, False), ("movement_disabled", 0x07, 1, False),
    ("holds_stall_tile", 0x1C, 1, False), ("tile", 0x34, 4, False), ("previous_tile", 0x38, 4, False),
    ("x", 0x4C, 2, True), ("z", 0x50, 2, True), ("x0", 0x70, 4, False), ("x1", 0x74, 4, False),
    ("z0", 0x78, 4, False), ("z1", 0x7C, 4, False), ("mask", 0x81, 1, False),
    ("detour_step", 0xFA, 1, False), ("detour_target", 0xFB, 1, False), ("d0", 0x118, 1, False),
    ("d1", 0x119, 1, False), ("d2", 0x11A, 1, False), ("d3", 0x11B, 1, False),
    ("movement_target", 0x15C, 1, False), ("destination", 0x168, 4, False),
)
# BlockedMover in declaration order, as the driver prints it.
MOVER = ("is_stuck", "holds_stall_tile", "tile", "previous_tile", "x", "z", "detour_step", "detour_target",
         "d0", "d1", "d2", "d3", "movement_target", "destination")
HISTORY = ("x0", "x1", "z0", "z1", "mask")
# The fields a detour call records, as the driver's Note lists them.
SNAPSHOT = (("is_stuck", 0x02, 1, False), ("holds_stall_tile", 0x1C, 1, False), ("detour_step", 0xFA, 1, False),
            ("directions", 0x118, 4, False), ("movement_target", 0x15C, 1, False),
            ("destination", 0x168, 4, False))
SNAPSHOT_NAMES = [name for name, *_ in SNAPSHOT]
RECORD = 4 + len(SNAPSHOT)
C_EDGE, C_OCCUPANCY, C_GAP, C_RANDOM, C_FORGET, C_DISPATCH = range(6)
# Added to a call's code when the original passed something other than the actor.
NOT_THE_ACTOR = 100
# Byte 1 of a tile word; see the module docstring.
TAG = 0xFF00
# The record arguments that carry func_800D8400's destination word.
TAGGED_ARGUMENTS = {C_EDGE: 2, C_OCCUPANCY: 1, C_GAP: 1}

MUTANTS = {
    # func_800D8400
    "disabled-ignored": {0x800D8438: 0x00000000},
    "start-status-2": {0x800D844C: 0x3C020200},
    "equal-probes-twice": {0x800D8464: 0x24140002},
    "adjacent-midpoint-0x100": {0x800D8474: 0x2C420101},
    "next-quadrant-wrap-1": {0x800D8484: 0x30420001},
    "second-always-swapped": {0x800D8490: 0x00000000},
    "dx-from-dz-bits": {0x800D84E8: 0x00031680},
    "tile-z-dropped": {0x800D84AC: 0x00131400},
    "held-tile-free": {0x800D8530: 0x00000000},
    "inner-edge-gap": {0x800D8548: 0x00000000},
    "landing-status-2-found": {0x800D8564: 0x24020002},
    "blocked-status-3": {0x800D8598: 0x3C020300},
    "tag-kept": {0x800D85A4: 0x3442FFFF},
    # the history tests func_800D8EF4 inlines
    "mask-0x7F": {0x800D9280: 0x2402007F},
    "x-words-ignored": {0x800D9298: 0x00000000},
    "z-compared-with-x": {0x800D92A0: 0x8E020070},
    "z-second-word-first": {0x800D92A4: 0x8E030078},
    # func_800D9538
    "random-from-0x3FC": {0x800D953C: 0x8C6303FC},
    "saved-at-0x58C8": {0x800D9548: 0xAC4358C8},
    # func_800DB0BC
    "orbit-ignored": {0x800DB0E0: 0x00000000},
    "moved-z-only": {0x800DB0EC: 0x34840000},
    "stuck-not-marked": {0x800DB114: 0x00000000},
    "centre-0x20": {0x800DB130: 0x2484FFE0},
    "sector-shift-10": {0x800DB148: 0x00021A82},
    "lateral-turn-1": {0x800DB150: 0x24630001},
    "axis-midpoint-0x100": {0x800DB160: 0x2C420101},
    "order-range-4": {0x800DB1A8: 0x24040004},
    "opposite-turn-2": {0x800DB1B0: 0x26020002},
    "directions-not-stored": {0x800DB1F8: 0x00000000},
    "detour-probe-z-from-x": {0x800DB238: 0x92230034},
    "free-on-zero": {0x800DB258: 0x0441FFDA},
    "three-probes": {0x800DB264: 0x2A020003},
    "exhausted-at-6": {0x800DB288: 0x2C420006},
    "forget-skipped": {0x800DB294: 0x00000000},
    "stall-tile-kept": {0x800DB2A0: 0x00000000},
    "step-index-unbiased": {0x800DB2B8: 0x90420118},
    "destination-z-kept": {0x800DB2FC: 0x00000000},
    "target-not-copied": {0x800DB310: 0x00000000},
    "dispatch-on-zero": {0x800DB314: 0x04400006},
    "dispatch-mode-1": {0x800DB31C: 0x24050001},
    "blocked-step-kept": {0x800DB33C: 0x00000000},
    "detour-returns-0": {0x800DB344: 0x24020000},
    "moved-step-kept": {0x800DB348: 0x00000000},
}

_battle: Image | None = None


def battle() -> Image:
    global _battle
    if _battle is None:
        _battle = Image.load("BATTLE/BATTLE.PRG")
    return _battle


def tile_byte(rng: random.Random) -> int:
    if rng.random() < 0.85:
        return rng.randrange(GRID)
    return rng.choice((0, 1, 31, 254, 255, rng.getrandbits(8)))


def direction(rng: random.Random) -> int:
    if rng.random() < 0.75:
        return rng.randrange(8)
    return rng.choice((-1, 8, 9, 0xFFFF, 0x10000, 0x10003, s32(rng.getrandbits(32))))


def angle(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.3:
        return rng.choice((0, 0x1FF, 0x200, 0x201, 0x3FF, 0x400, 0x5FF, 0x600, 0x601, 0xA00, 0xE00, 0xE01))
    if roll < 0.85:
        return rng.randrange(0x1000)
    return rng.getrandbits(32)


def answer(rng: random.Random, common: tuple[int, ...]) -> int:
    return rng.choice(common) if rng.random() < 0.9 else s32(rng.getrandbits(32))


def landing(rng: random.Random) -> int:
    status = rng.choice((0, 0, 1, 1, 2, 0xFF, rng.getrandbits(8)))
    return status << 24 | rng.getrandbits(24)


def history(rng: random.Random) -> dict:
    x0, z0 = rng.getrandbits(32), rng.getrandbits(32)
    return {"x0": x0, "x1": x0 if rng.random() < 0.6 else rng.getrandbits(32),
            "z0": z0, "z1": z0 if rng.random() < 0.6 else x0 if rng.random() < 0.3 else rng.getrandbits(32),
            "mask": rng.choice((0xFF, 0xFF, 0x7F, 0xFE, rng.getrandbits(8)))}


def mover(rng: random.Random) -> dict:
    tile = packed(rng, tile_byte(rng), tile_byte(rng))
    roll = rng.random()
    if roll < 0.75:
        previous = packed(rng, tile & 0xFF, tile >> 16 & 0xFF)
    elif roll < 0.95:
        previous = near(rng, tile, 1)
    else:
        previous = rng.choice((0xFFFFFFFF, rng.getrandbits(32)))
    destination = near(rng, tile, rng.choice((1, 2, 4, 8))) if rng.random() < 0.9 else rng.getrandbits(32)
    step = rng.choice((0, 0, 0, 0, 1, 2, 3, 4, 5, 6, rng.getrandbits(8)))
    return {
        "is_stuck": rng.choice((0, 1, rng.getrandbits(8))),
        "holds_stall_tile": rng.choice((0, 1, rng.getrandbits(8))),
        "tile": tile, "previous_tile": previous,
        "x": position(rng, tile & 0xFF), "z": position(rng, tile >> 16 & 0xFF),
        "detour_step": step,
        "detour_target": rng.choice((NO_TARGET, rng.randrange(NO_TARGET), rng.getrandbits(8))),
        **{f"d{i}": rng.randrange(8) if rng.random() < 0.9 else rng.getrandbits(8) for i in range(4)},
        "movement_target": rng.getrandbits(8),
        "destination": destination,
    }


def vector(rng: random.Random, number: int) -> dict:
    image = battle()
    kind = number % 5 if number < 20 else rng.choices(range(5), (35, 10, 10, 5, 40))[0]
    codes = ([image.read(DIRECTION_CODES + i, 1) for i in range(8)] if rng.random() < 0.8
             else [rng.getrandbits(8) for _ in range(8)])
    fields = {name: 0 for name, *_ in FIELDS}
    raw = rng.randbytes(STATE_SIZE)
    for name, offset, size, signed in FIELDS:
        value = int.from_bytes(raw[offset:offset + size], "little", signed=signed)
        fields[name] = value
    v = {"kind": kind, "codes": codes, "state": raw, "fields": fields}
    if kind == K_ADJACENT:
        first = direction(rng)
        second = first if rng.random() < 0.3 else direction(rng)
        fields["movement_disabled"] = rng.getrandbits(8) if rng.random() < 0.1 else 0
        fields["tile"] = packed(rng, tile_byte(rng), tile_byte(rng))
        v.update(first=first, second=second,
                 direction=(direction(rng) & 0xFFFFFFFF if first == second else angle(rng)),
                 s0=rng.getrandbits(32) & ~TAG if rng.random() < 0.5 else rng.getrandbits(32),
                 edges=[answer(rng, (-1, 0, 0, 1, 1, 5)) for _ in range(2)],
                 occupancy=[answer(rng, (-1, 0, 1, 1, 7)) for _ in range(2)],
                 landings=[landing(rng) for _ in range(2)])
    elif kind in (K_STALLED, K_REPEATS):
        fields.update(history(rng))
    elif kind == K_SAVE:
        v.update(frame=rng.getrandbits(32), saved=rng.getrandbits(32))
    else:
        fields.update(mover(rng))
        v.update(orbit=0 if rng.random() < 0.85 else rng.getrandbits(32) or 1,
                 random=rng.choice((0, 1, 0, 1, s32(rng.getrandbits(32)))),
                 occupancy=[answer(rng, (-1, 0, 0, 1, 2)) for _ in range(6)])
    return v


def run(machine: Machine, v: dict):
    kind = v["kind"]
    machine.images.append(Image(bytes(pack_fields(v["state"], FIELDS, v["fields"])), STATE))
    machine.map_zero(SCRATCH, SCRATCH_SIZE)
    machine.store(SCRATCH_DIRECTIONS, bytes(v["codes"]))
    machine.follow.add(ARCTANGENT)
    records: list[list[int]] = []
    queues = {key: list(v.get(key, ())) for key in ("edges", "occupancy", "landings")}

    def pop(key: str) -> int:
        return (queues[key].pop(0) if queues[key] else 0) & 0xFFFFFFFF

    def snapshot(m: Machine) -> list[int]:
        return m.load_fields(STATE, SNAPSHOT, SNAPSHOT_NAMES) if kind == K_RECOVER else [0] * len(SNAPSHOT)

    def record(m: Machine, code: int, *arguments: int) -> None:
        if m.r[4] != STATE:
            code += NOT_THE_ACTOR
        records.append([code, *arguments, *[0] * (3 - len(arguments)), *snapshot(m)])

    def edge(m: Machine) -> None:
        record(m, C_EDGE, m.r[5], m.r[6], s32(m.r[7]))
        m.r[2] = pop("edges")

    def occupancy(m: Machine) -> None:
        record(m, C_OCCUPANCY, m.r[5])
        m.r[2] = pop("occupancy")

    def gap(m: Machine) -> None:
        record(m, C_GAP, m.r[5], s32(m.r[6]), m.r[7])
        m.r[2] = pop("landings")

    def draw(m: Machine) -> None:
        records.append([C_RANDOM, s32(m.r[4]), 0, 0, *[0] * len(SNAPSHOT)])
        m.r[2] = v["random"] & 0xFFFFFFFF

    machine.stub(EDGE, edge)
    machine.stub(OCCUPANCY, occupancy)
    machine.stub(GAP, gap)
    machine.stub(RANDOM, draw)
    machine.stub(FORGET, lambda m: record(m, C_FORGET, 0))
    machine.stub(DISPATCH, lambda m: record(m, C_DISPATCH, s32(m.r[5]), s32(m.r[6])))
    f = v["fields"]
    if kind == K_ADJACENT:
        machine.r[4:8] = [STATE, v["first"] & 0xFFFFFFFF, v["second"] & 0xFFFFFFFF, v["direction"]]
        machine.r[16] = v["s0"]
        machine.execute(ADJACENT)
        inputs = [kind, *v["codes"], f["movement_disabled"], f["tile"], v["first"], v["second"], v["direction"],
                  *(value for key in ("edges", "occupancy", "landings") for value in (len(v[key]), *v[key]))]
    elif kind in (K_STALLED, K_REPEATS):
        machine.r[16] = STATE
        machine.execute(STALLED if kind == K_STALLED else REPEATS, stop=HISTORY_END)
        inputs = [kind, *(f[name] for name in HISTORY)]
    elif kind == K_SAVE:
        machine.put(FRAME_RANDOM, v["frame"])
        machine.put(SAVED_RANDOM, v["saved"])
        machine.execute(SAVE)
        inputs = [kind, v["frame"], s32(v["saved"])]
    else:
        machine.put(ARENA_ORBIT, v["orbit"])
        machine.r[4] = STATE
        machine.execute(RECOVER)
        inputs = [kind, *v["codes"], int(v["orbit"] != 0), v["random"], *(f[name] for name in MOVER),
                  len(v["occupancy"]), *v["occupancy"]]
    observation = {"result": machine.r[2] if kind != K_SAVE else machine.load(SAVED_RANDOM),
                   "fields": machine.load_fields(STATE, FIELDS, MOVER), "records": records,
                   "stores": machine.stores}
    return observation, inputs


def allowed(kind: int) -> list[tuple[int, int]]:
    if kind == K_SAVE:
        return [(SAVED_RANDOM, SAVED_RANDOM + 4)]
    spans = [STACK]
    if kind == K_RECOVER:
        layout = {name: (offset, size) for name, offset, size, _ in FIELDS}
        spans += [(STATE + layout[name][0], STATE + sum(layout[name])) for name in MOVER]
        # The directions are stored as one word.
        spans.append((STATE + layout["d0"][0], STATE + layout["d3"][0] + 1))
    return spans


def untagged(records: list[list[int]]) -> list[list[int]]:
    """Drop byte 1 of func_800D8400's destination arguments; see the module docstring."""
    out = []
    for record in records:
        index = TAGGED_ARGUMENTS.get(record[0] % NOT_THE_ACTOR)
        out.append([value & ~TAG if i == index else value for i, value in enumerate(record)])
    return out


def recover_refuses(v: dict) -> bool:
    """Whether the native refuses: the detour step names a stored direction outside 0..7."""
    f = v["fields"]
    if v["orbit"] or (f["tile"] ^ f["previous_tile"]) & 0xFF00FF:
        return False
    step = f["detour_step"]
    return 1 <= step <= 4 and f[f"d{step - 1}"] >= 8


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    refused, result, count = native[0], native[1], native[2]
    fields = native[3:3 + count]
    calls = native[3 + count]
    start = 4 + count
    records = [native[start + i * RECORD:start + (i + 1) * RECORD] for i in range(calls)]
    original_records = obs["records"]
    kind = v["kind"]
    problems = []
    for address, size in obs["stores"]:
        if not any(low <= address and address + size <= high for low, high in allowed(kind)):
            problems.append(f"original stored {size} bytes at {address:#x}")
            break
    if kind == K_SAVE and obs["stores"] != [(SAVED_RANDOM, 4)]:
        problems.append(f"original stores {obs['stores']}")
    if kind == K_ADJACENT:
        if v["s0"] & TAG:
            records, original_records = untagged(records), untagged(original_records)
        outside = next((i for i, record in enumerate(original_records)
                        if record[0] % NOT_THE_ACTOR == C_EDGE and record[3] >= 8), None)
        if outside is not None:
            if not refused:
                problems.append("native accepted a direction outside 0..7")
            elif records != original_records[:outside]:
                problems.append(f"calls before the refusal differ: native {records} "
                                f"original {original_records[:outside]}")
            return problems
    if kind == K_RECOVER and recover_refuses(v):
        if not refused or records:
            problems.append(f"native did not refuse cleanly: refused {refused}, calls {records}")
        return problems
    if refused:
        return [*problems, "native refused"]
    if result != obs["result"]:
        problems.append(f"result native {result:#x} original {obs['result']:#x}")
    if kind == K_RECOVER:
        problems += field_problems(MOVER, fields, obs["fields"])
    return problems + call_problems(records, original_records)


def adjacent_outcome(v: dict, obs: dict, native: list[int]) -> str:
    if v["fields"]["movement_disabled"]:
        return "disabled"
    if native[0]:
        return "refused"
    result, probes = obs["result"], sum(record[0] == C_EDGE for record in obs["records"])
    status = result >> 24
    if result == 0:
        return f"none after {probes}"
    if status == 2:
        return f"blocked at {probes}"
    if status == 1:
        return f"found at {probes}"
    return f"found status {status} at {probes}"


def recover_outcome(v: dict, obs: dict, native: list[int]) -> str:
    if native[0]:
        return "refused"
    codes = [record[0] for record in obs["records"]]
    if v["orbit"]:
        return "orbit"
    if (v["fields"]["tile"] ^ v["fields"]["previous_tile"]) & 0xFF00FF:
        return "moved"
    started = "started, " if C_RANDOM in codes else ""
    if C_FORGET in codes:
        return started + "exhausted"
    return started + ("dispatched" if C_DISPATCH in codes else "advanced")


def describe(image: Image, results: list) -> list[str]:
    tallies: dict[str, dict[str, int]] = {name: {} for name in KINDS}
    for v, obs, native in results:
        kind = v["kind"]
        if kind == K_ADJACENT:
            outcome = adjacent_outcome(v, obs, native)
        elif kind == K_RECOVER:
            outcome = recover_outcome(v, obs, native)
        elif kind == K_SAVE:
            outcome = "saved"
        else:
            outcome = "true" if obs["result"] else "false"
        tally = tallies[KINDS[kind]]
        tally[outcome] = tally.get(outcome, 0) + 1
    ranges = (("F01636", ADJACENT, ADJACENT_END), ("F01641/F01642", STALLED, HISTORY_END),
              ("F01644", SAVE, SAVE_END))
    lines = [f"{name} {low:08X}-{high - 4:08X} inclusive: {(high - low) // 4} original instructions; "
             f"sha256={image.sha256(low, high)}" for name, low, high in ranges]
    lines += [f"{name}: " + ", ".join(f"{outcome} {count}" for outcome, count in sorted(tally.items()))
              for name, tally in tallies.items()]
    return lines
