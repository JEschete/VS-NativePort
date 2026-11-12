"""F01656-F01659: escort, action execution and bounded frame decision dispatch.

The original 377 instructions run in differential.py's R3000 interpreter.
F01656 reuses the candidate-movement fixtures and follows the original approach
prefix. F01657/58 are followed inside F01659; external gates, engagement, command,
clock, RNG and movement dispatch use queued answers and writes. Snapshots at every
call compare all actor fields, candidate words, current-candidate identities,
global engagement, cached ticks, all direction words and linked successors.
Player record height/flags are deliberately distinct from the battle-head model.
Missing/invalid original tables and cyclic lists are native refusal-test domains.
"""
from __future__ import annotations

import random

from differential import (
    MASK,
    Image,
    Machine,
    call_problems,
    field_problems,
    field_writer,
    s32,
    store_problems,
)
from differentials import candidate_movement as movement

ID = "F01656, F01657, F01658, F01659"
ENTRY, END = 0x800DAAD8, 0x800DB0BC
MODULES = movement.MODULES
DRIVER = "action_execution_driver.cpp"
SOURCES = (*movement.SOURCES, r"Native\Game\src\Battle\Ai\ActionExecution.cpp")
VECTORS, SEED, MUTANT_VECTORS = 4096, 1656, 768
ENTRIES = (ENTRY, 0x800DAC80, 0x800DAD9C, 0x800DAED0)
CALLEES = (0x800DBD80, 0x800D836C, 0x800D826C, 0x800E45D4, 0x800DEEFC,
           0x800DBC60, 0x800E45B4, 0x800DB820, 0x800DB7B4)
CAN, ENGAGE, READY, RANDOM, COMMAND, COOLDOWN, ELAPSED, DISPATCH, LOCOMOTION = range(9)
CALL_NAMES = ("can", "engage", "ready", "random", "command", "cooldown", "elapsed", "dispatch", "locomotion")
DATA, STRIDE = 0x80600000, 0x600
STATES, CANDIDATES = DATA, DATA + 0x2000
ROOM, NODES, MODELS, GIMMICKS, TILES, BATTLE_MODEL = (
    DATA + 0x2200, DATA + 0x2300, DATA + 0x2500, DATA + 0x2D00, DATA + 0x3500, DATA + 0x3D00)
DATA_SIZE = 0x3E00
ENGAGED, BUDGET, CACHED, DIRECTIONS = 0x800F58E0, 0x800F16F4, 0x1F8003FC, 0x1F8003D0
FIELDS = (
    ("header0", 0, 4, False), ("header4", 4, 4, False), ("header8", 8, 4, False),
    ("headerC", 12, 2, False), ("headerE", 14, 2, False), ("delay_kind", 0x13, 1, False),
    ("restricted", 0x14, 1, False), ("partial", 0x1D, 1, False),
    ("tile_x", 0x34, 1, False), ("tile_z", 0x36, 1, False),
    ("destination", 0x48, 4, False), ("x", 0x4C, 2, True), ("y", 0x4E, 2, True),
    ("z", 0x50, 2, True), ("height", 0x82, 2, True), ("depth", 0xB8, 2, True),
    ("delay", 0x13F, 1, False), ("area132", 0x132, 1, False), ("area154", 0x154, 1, False),
    ("target", 0x15C, 1, False), ("distance", 0x1E4, 4, False), ("count", 0x470, 4, True),
    ("selected", 0x22C, 4, False),
)
STATE_FIELDS = len(FIELDS) + 25
GLOBAL = 4 * STATE_FIELDS
NEXT = GLOBAL + 7
NAMES = [*(f"actor[{actor}].{name}" for actor in range(4)
           for name in [*(name for name, *_ in FIELDS), *(f"candidate[{c}].word[{w}]" for c in range(5) for w in range(5))]),
         "engaged", "cached", "direction0", "direction1", "direction2", "budget", "room_delay",
         *(f"next[{i}]" for i in range(4))]
MUTANTS = {
    "F01656-follow-383": {0x800DAB28: 0x2406017F},
    "F01656-option-one": {0x800DAB30: 0x24070001},
    "F01656-enemy-class-two": {0x800DABF4: 0x24030002},
    "F01656-category-mask-63": {0x800DAC08: 0x3042003F},
    "F01656-flee-kind-one": {0x800DAC20: 0x24050001},
    "F01656-no-temporary-class": {0x800DAC14: 0},
    "F01656-failure-restore-omitted": {0x800DAC54: 0},
    "F01656-success-restore-omitted": {0x800DABC8: 0},
    "F01657-can-gate-ignored": {0x800DAC9C: 0},
    "F01657-engage-omitted": {0x800DACA4: 0},
    "F01657-ready-gate-ignored": {0x800DACB4: 0},
    "F01657-pointer-not-selected": {0x800DACC0: 0},
    "F01657-engaged-not-set": {0x800DACE4: 0},
    "F01657-moving-mask-bit19": {0x800DACEC: 0x3C030008},
    "F01657-random-mask-one": {0x800DAD00: 0x24040001},
    "F01657-limb-mask-three": {0x800DAD10: 0x30420003},
    "F01657-height-216": {0x800DAD28: 0x34636C00},
    "F01657-command-14": {0x800DAD38: 0x2405000E},
    "F01657-cooldown-gate-ignored": {0x800DAD48: 0},
    "F01657-fixed-delay-49": {0x800DAD5C: 0x24020031},
    "F01657-room-delay-neighbor": {0x800DAD6C: 0x90420009},
    "F01658-count-not-reread": {0x800DADE0: 0},
    "F01658-fallback-mask-127": {0x800DAE04: 0x3042007F},
    "F01658-target-not-stored": {0x800DAE4C: 0},
    "F01658-parent-gate-ignored": {0x800DAE70: 0},
    "F01658-mode-gate-ignored": {0x800DAE80: 0},
    "F01658-only-three-specials": {0x800DAEA4: 0x2A220003},
    "F01659-head-included": {0x800DAEEC: 0x00408821},
    "F01659-restricted-gate-ignored": {0x800DAF30: 0},
    "F01659-budget-unsigned": {0x800DAF4C: 0x0062182B},
    "F01659-budget-reversed": {0x800DAF4C: 0x0043182A},
    "F01659-budget-command-16": {0x800DAF5C: 0x24050010},
    "F01659-cache-not-stored": {0x800DAF6C: 0},
    "F01659-no-partial-reread": {0x800DAF70: 0},
    "F01659-default-height-63": {0x800DAFF4: 0x2407FFC1},
    "F01659-height-not-negated": {0x800DAFDC: 0x00403821},
    "F01659-height-equality-included": {0x800DAFCC: 0x14400004},
    "F01659-distance-not-stored": {0x800DB01C: 0},
    "F01659-target-15": {0x800DB02C: 0x2402000F},
    "F01659-destination-zero": {0x800DB034: 0x24020000},
    "F01659-header-clears-16": {0x800DB040: 0xAE00000C},
    "F01659-header8-not-cleared": {0x800DB054: 0},
    "F01659-area-mask-seven": {0x800DB070: 0x30420007},
    "F01659-area132-not-stored": {0x800DB078: 0},
    "F01659-cache-next-word": {0x800DB080: 0xAC2003D4},
    "F01659-dispatch-omitted": {0x800DB084: 0},
    "F01659-locomotion-omitted": {0x800DB08C: 0},
}


def candidate_address(actor: int, handle: int) -> int:
    return STATES + actor * STRIDE + 0x140 if handle == 4 else CANDIDATES + actor * 80 + handle * 20


def location(index: int) -> tuple[int, int, bool]:
    if index < GLOBAL:
        actor, field = divmod(index, STATE_FIELDS)
        if field < len(FIELDS):
            _, offset, size, signed = FIELDS[field]
            return STATES + actor * STRIDE + offset, size, signed
        candidate, word = divmod(field - len(FIELDS), 5)
        return candidate_address(actor, candidate) + word * 4, 4, False
    if index < NEXT:
        return (ENGAGED, CACHED, DIRECTIONS, DIRECTIONS + 4, DIRECTIONS + 8, BUDGET, ROOM + 8)[index - GLOBAL], 1 if index == GLOBAL + 6 else 4, index in (GLOBAL, GLOBAL + 1, GLOBAL + 5)
    return NODES + (index - NEXT) * 0x50, 4, False


def encode(index: int, value: int) -> int:
    if index < GLOBAL and index % STATE_FIELDS == 22:
        return 0 if value < 0 else candidate_address(index // STATE_FIELDS, value)
    if index >= NEXT:
        return 0 if value == 0 else NODES + (value - 1) * 0x50
    return value


def vector(rng: random.Random, number: int) -> dict:
    kind, lane = number % 4, number // 4 % 32
    if kind == 0:
        v = movement.vector(rng, (number // 4) * 6 + 3)
        v["kind"] = 6
        v["fields"]["linked_actor"] = 16 if lane >= 12 else lane % 17
        if lane >= 12:
            v["fields"]["action_status"] = 1
            v["roster"] = [0, 2, 4]
            v["relations"][0] = 0x001000C0
            v["relations"][2] = 0x20000080
            v["relations"][4] = ((lane % 4) << 28) | 0x04000040
            v["answers"][movement.C_FLEE] = [lane % 2]
            if lane % 3 == 0:
                v["effects"].append((movement.C_FLEE, 0, len(movement.FIELDS) + 4,
                                     0x2FE123FF, -1, 0, 0))
        return {"kind": 0, "movement": v}
    fields = []
    for _actor in range(4):
        state = [rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32),
                 rng.getrandbits(16), rng.getrandbits(16), rng.choice((0, 1, 255)),
                 lane % 2, lane // 2 % 2, rng.randrange(32), rng.randrange(32),
                 rng.getrandbits(32), rng.randrange(-1000, 1001), rng.randrange(-200, 201),
                 rng.randrange(-1000, 1001), rng.randrange(-128, 129), rng.randrange(-128, 129),
                 rng.randrange(256), rng.randrange(256), rng.randrange(256), rng.randrange(256),
                 rng.getrandbits(32), rng.randrange(5), -1]
        for _ in range(5):
            state.extend(rng.getrandbits(32) for _ in range(5))
        if lane % 5 == 0:
            state[len(FIELDS) + 21] &= ~(255 << 5)
        elif lane % 5 == 1:
            state[len(FIELDS) + 21] = (state[len(FIELDS) + 21] & ~(255 << 5)) | (128 << 5)
        if kind == 2 and lane >= 8:
            state[21] = 0
        fields.extend(state)
    fields.extend([rng.choice((0, 1, -1)), -987, rng.getrandbits(32), rng.getrandbits(32),
                   rng.getrandbits(32), rng.choice((-10, -1, 0, 1, 10, 0x7FFFFFFF)), rng.randrange(256), 2, 3, 4, 0])
    moving = [rng.choice((0, 0, 0x80000, 0x100000, 0x180000)) for _ in range(16)]
    specials = [rng.choice((-1, 0, 1, 3, 15)) for _ in range(4)]
    gimmicks = [(rng.choice((0, 1, 255)), rng.choice((0, 0, -1, 1))) for _ in range(16)]
    answers = [[rng.choice((0, 1, -1, 2)) for _ in range(20)] for _ in CALLEES]
    answers[RANDOM] = [rng.choice((-1, 0, 1, 2, 4, 7, 10)) for _ in range(20)]
    answers[ELAPSED] = [rng.choice((-10, -1, 0, 1, 10, 0x7FFFFFFF)) for _ in range(20)]
    player = [int(lane % 5 != 0), int(lane % 3 != 0), rng.randrange(-200, 201), rng.randrange(-128, 129), rng.randrange(-128, 129)]
    writes = []
    if lane < 8:
        answers[CAN] = [int(lane != 0)] * 20
        answers[READY] = [int(lane != 1)] * 20
        answers[COOLDOWN] = [int(lane != 2)] * 20
        fields[STATE_FIELDS + 5] = lane % 2
        target = fields[STATE_FIELDS + len(FIELDS) + 3] & 15
        moving[target] = 0x100000 if lane % 2 else 0
    if kind == 2 and lane == 9:
        specials = [-1, -1, -1, 3]
        gimmicks[3] = (1, 0)
        answers[CAN] = answers[READY] = [1] * 20
        fields[STATE_FIELDS + len(FIELDS) + 21] |= 1 << 5
    if lane % 4 == 2:
        writes += [(CAN, 0, STATE_FIELDS + 21, rng.randrange(5)),
                   (RANDOM, 0, STATE_FIELDS + len(FIELDS) + 3, rng.getrandbits(32)),
                   (COOLDOWN, 0, STATE_FIELDS + 5, lane % 3),
                   (COOLDOWN, 0, GLOBAL + 6, rng.randrange(256))]
    if kind == 3:
        fields[STATE_FIELDS + 6:STATE_FIELDS + 8] = [lane % 2, lane // 2 % 2]
        if lane % 4 == 0:
            writes += [(ELAPSED, 0, STATE_FIELDS + 6, 1), (ELAPSED, 0, STATE_FIELDS + 7, 1)]
        if lane % 6 == 0:
            writes += [(ELAPSED, 0, NEXT + 1, 0)]
        if lane % 6 == 1:
            writes += [(DISPATCH, 0, NEXT + 1, 0)]
        if lane % 5 == 0:
            fields[GLOBAL + 5] = answers[ELAPSED][0]
        if lane % 7 == 0:
            fields[STATE_FIELDS + 12] = player[2]
    return {"kind": kind, "fields": fields, "moving": moving, "specials": specials,
            "gimmicks": gimmicks, "player": player, "position": [rng.randrange(-1000, 1001) for _ in range(3)],
            "terrain": [rng.getrandbits(16) for _ in range(1024)], "answers": answers,
            "writes": writes, "noise": rng.randbytes(256)}


def snapshot(m: Machine) -> list[int]:
    values = []
    for index in range(len(NAMES)):
        address, size, signed = location(index)
        value = m.load(address, size, signed)
        if index < GLOBAL and index % STATE_FIELDS == 22:
            actor = index // STATE_FIELDS
            value = next((h for h in range(5) if value == candidate_address(actor, h)), -1 if value == 0 else -2)
        elif index >= NEXT:
            value = 0 if value == 0 else (value - NODES) // 0x50 + 1
        values.append(value)
    return values


def run(machine: Machine, v: dict):
    if v["kind"] == 0:
        obs, inputs = movement.run_at(machine, v["movement"], ENTRY)
        return obs, [0, *inputs]
    block = bytearray(v["noise"] * (DATA_SIZE // 256))
    put = field_writer(block, DATA)
    for i, value in enumerate(v["fields"]):
        address, size, _ = location(i)
        if DATA <= address < DATA + DATA_SIZE:
            put(address, encode(i, value), size)
        else:
            machine.put(address, encode(i, value), size)
    for actor in range(4):
        base = STATES + actor * STRIDE
        for candidate in range(4):
            put(base + 0x430 + candidate * 4, candidate_address(actor, candidate), 4)
        put(NODES + actor * 0x50 + 4, actor, 4)
        put(NODES + actor * 0x50 + 0x44, BATTLE_MODEL, 4)
        machine.put(movement.FRAME_ACTORS + actor * 4, base)
    for target, flags in enumerate(v["moving"]):
        put(MODELS + target * 0x80 + 8, flags, 4)
        machine.put(movement.ACTOR_RECORDS + target * 4, MODELS + target * 0x80 if flags else 0)
    has_ai, moving, y, height, depth = v["player"]
    # F01657's target zero shares this model; preserve its moving flags while
    # the partial-decision fixture independently supplies the player record.
    if v["kind"] == 3:
        put(MODELS + 8, 0x180000 if moving else 0, 4)
        machine.put(movement.ACTOR_RECORDS, MODELS)
        put(MODELS + 0x1E, y, 2)
    put(STATES + 0x82, height, 2)
    put(STATES + 0xB8, depth, 2)
    v["fields"][14], v["fields"][15] = height, depth
    machine.put(movement.AI_STATES, STATES if has_ai else 0)
    for coordinate, value in enumerate(v["position"]):
        put(BATTLE_MODEL + 0x1C + coordinate * 2, value, 2)
    for slot, actor in enumerate(v["specials"]):
        # Dedicated nodes keep special IDs separate from the decision list.
        node = GIMMICKS + 0x600 + slot * 8
        put(node + 4, actor, 4)
        put(ROOM + 0x28 + slot * 4, 0 if actor < 0 else node, 4)
    for actor, (parent, mode) in enumerate(v["gimmicks"]):
        put(GIMMICKS + actor * 0x40 + 0x12, parent, 1)
        put(GIMMICKS + actor * 0x40 + 0x1A, mode, 2)
        machine.put(0x800F45E0 + actor * 4, GIMMICKS + actor * 0x40)
    for index, flags in enumerate(v["terrain"]):
        put(TILES + index * 2, flags, 2)
    initial = bytes(block)
    machine.images.append(Image(initial, DATA))
    machine.put(movement.ROOM_POINTER, ROOM)
    machine.put(movement.BATTLE_ACTORS, NODES)
    machine.put(0x1F8003C0, TILES)
    machine.follow.update((*ENTRIES, 0x800E4624))
    calls, occurrences = [], [0] * len(CALLEES)
    current = 1

    def stub(m: Machine, code: int):
        nonlocal current
        if code == ELAPSED:
            current = (m.r[16] - STATES) // STRIDE
        elif code != RANDOM:
            current = (m.r[4] - STATES) // STRIDE
        argument = 0
        if code in (CAN, COOLDOWN):
            argument = next((h for h in range(5) if m.r[5] == candidate_address(current, h)), -99)
        elif code == RANDOM:
            argument = s32(m.r[4])
        elif code == COMMAND:
            argument = s32(m.r[5])
        calls.append([code, current, argument, 0, *snapshot(m)])
        at = occurrences[code]
        for call, occurrence, index, value in v["writes"]:
            if call == code and occurrence == at:
                address, size, _ = location(index)
                m.write(address, encode(index, value), size)
        m.r[2] = v["answers"][code][at] & MASK if at < len(v["answers"][code]) else 0
        occurrences[code] += 1

    for code, address in enumerate(CALLEES):
        machine.stub(address, lambda m, code=code: stub(m, code))
    machine.r[4], machine.r[5] = STATES + STRIDE, candidate_address(1, 0)
    machine.execute(ENTRIES[v["kind"]])
    changed = [(address, initial[address - DATA], after) for address, after in machine.overlay.items()
               if DATA <= address < DATA + DATA_SIZE and initial[address - DATA] != after]
    obs = {"result": s32(machine.r[2]) if v["kind"] != 3 else 0, "fields": snapshot(machine),
           "calls": calls, "changed": changed, "stores": machine.stores}
    inputs = [v["kind"], *v["fields"], *(int(bool(flags)) for flags in v["moving"]), *v["specials"],
              *(int(parent != 0 and mode == 0) for parent, mode in v["gimmicks"]), *v["player"],
              *v["position"], *v["terrain"], *(item for queue in v["answers"] for item in (len(queue), *queue)),
              len(v["writes"]), *(item for write in v["writes"] for item in write)]
    if v["kind"] == 3:
        inputs[1 + len(NAMES)] = int(v["player"][1])
    return obs, inputs


def allowed_stores(v: dict) -> list[tuple[int, int]]:
    spans = [movement.STACK_SPAN, (ENGAGED, ENGAGED + 4), (CACHED, CACHED + 4), (DIRECTIONS, DIRECTIONS + 4)]
    for actor in range(4):
        for field in (0, 1, 2, 3, 10, 16, 17, 18, 19, 20, 22):
            address, size, _ = location(actor * STATE_FIELDS + field)
            spans.append((address, address + size))
        # Include the halfword/word header span without admitting bytes E/F.
        spans.append((STATES + actor * STRIDE, STATES + actor * STRIDE + 14))
        for handle in range(5):
            address = candidate_address(actor, handle) + 12
            spans.append((address, address + 4))
    for _call, _occurrence, index, _value in v["writes"]:
        address, size, _ = location(index)
        spans.append((address, address + size))
    return spans


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    if v["kind"] == 0:
        names = movement.NAMES
        spans = movement.allowed_stores(v["movement"]) + [
            (movement.STATE + 0x1A4, movement.STATE + 0x1E4)]
    else:
        names, spans = NAMES, allowed_stores(v)
    end = 2 + len(names)
    problems = field_problems(["result", *names], native[1:end], [obs["result"], *obs["fields"]])
    record = 4 + len(names)
    calls = [native[end + 1 + i * record:end + 1 + (i + 1) * record] for i in range(native[end])]
    problems += call_problems(calls, obs["calls"])
    for address, before, after in obs["changed"]:
        if not any(low <= address < high for low, high in spans):
            problems.append(f"unrelated byte {address:#x}: {before:#x}->{after:#x}")
            break
    return problems + store_problems(obs["stores"], spans)


def describe(image: Image, results: list) -> list[str]:
    lines = []
    for kind in range(4):
        subset = [(v, obs) for v, obs, _ in results if v["kind"] == kind]
        patterns = {tuple(tuple(call[:4]) for call in obs["calls"]) for _, obs in subset}
        writes = sum(bool(v["movement"]["effects"] if kind == 0 else v["writes"]) for v, _ in subset)
        lines.append(f"F{1656 + kind:05}: {len(subset)} vectors, {len(patterns)} call-order paths, {writes} callback-mutation vectors")
    return lines
