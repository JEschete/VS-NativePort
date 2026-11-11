"""F01705, F01706 (74088.c:716, 782) actor reactions vs Relations.cpp.

func_800DD000 has an AI actor react to an applied action's hits on it and func_800DD344 propagates
its relationships over the battle actor list. Stubs stand in for the callees and record their
arguments: the help and harm tests (func_800DC48C, func_800DC484) answer from the vector,
TryBeginAshleyAlliance (func_800DC210) sets and EndAshleyAlliance (func_800DC19C) clears the
alliance byte, and MakeTargetHostile (func_800DC784) marks the relation hostile as the original
does, so later reads see each change. The native driver's host models the same effects over the
same sixteen AI states.

The AI states, the request and the battle actor nodes hold random bytes around the fields.
Vectors keep the acting actor and the listed IDs within the AI slots, give the original a state
wherever it reads one and keep the battle actor list free of repeats, where the native refuses
what the original would read outside the state table or through null.
"""

from __future__ import annotations

import random

from differential import Image, Machine, field_writer, output_problems, s16, s32

ID = "F01705, F01706"
ENTRY, END = 0x800DD000, 0x800DD604
DRIVER = "actor_reaction_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\Ai\Relations.cpp",)
VECTORS, SEED = 4096, 1705
MUTANT_VECTORS = 1024

REACT, PROPAGATE = 0x800DD000, 0x800DD344
K_REACT, K_PROPAGATE = range(2)
BENEFICIAL, HARMFUL = 0x800DC48C, 0x800DC484
BEGIN_ALLIANCE, END_ALLIANCE, MAKE_HOSTILE = 0x800DC210, 0x800DC19C, 0x800DC784
STATE_TABLE, ORBIT_POINTER, BATTLE_LIST_POINTER = 0x800F5878, 0x800F5920, 0x800F1928
DEFINITIONS, DEFINITION_SIZE, PREREQUISITE_WORDS = 0x8004B9DC, 52, (0x14, 0x18)

DATA = 0x80500000
REQUEST = DATA
NODES, NODE_SIZE = DATA + 0x400, 0x10
ORBIT = DATA + 0x600
STATES, STATE_SIZE = DATA + 0x1000, 0x200
DATA_SIZE = 0x1000 + 16 * STATE_SIZE
ACTORS = 16

# The request: action ID, the acting actor's entry, the hit count and the hits.
ACTION, ACTING, COUNT, HITS, HIT_SIZE = 0x0, 0x4, 0x4A, 0x4C, 0x44
HIT_ACTOR, HIT_HP, HIT_EFFECT, HIT_SKIP = 0x0, 0x4, 0x1C, 0x40
ORBITING = 0xA
# AI state settings bytes, in the driver's ReactionSettings order.
SETTINGS = (0x13, 0x30, 0x20, 0x27, 0x21, 0x2C)
ALLIED = SETTINGS[0]
ID_FIELD, BEHAVIOR, RELATIONS = 0x88, 0x12D, 0x1A4
C_BENEFICIAL, C_HARMFUL, C_BEGIN, C_END, C_HOSTILE = range(5)
CALL_NAMES = ("helps", "harms", "begin", "end", "hostile")
PER_ACTOR = ("behavior", "allied", *(f"relation{target}" for target in range(ACTORS)))

MUTANTS = {
    # func_800DD000
    "skipped-acting-reacts": {0x800DD040: 0x00000000},
    "ashley-reacts": {0x800DD050: 0x00000000},
    "first-prerequisite-ignored": {0x800DD090: 0x00000000},
    "second-prerequisite-inverted": {0x800DD0A8: 0x10450002},
    "prerequisite-4": {0x800DD084: 0x24050004},
    "hit-skip-ignored": {0x800DD108: 0x00000000},
    "other-target-reacts": {0x800DD118: 0x00000000},
    "self-reacts": {0x800DD120: 0x00000000},
    "holding-not-released": {0x800DD138: 0x00000000},
    "released-mode-3": {0x800DD134: 0x24020003},
    "help-ignored": {0x800DD14C: 0x10000019},
    "help-from-anyone-begins": {0x800DD154: 0x00000000},
    "allied-begins-again": {0x800DD164: 0x00000000},
    "begins-without-bit27": {0x800DD174: 0x00000000},
    "begin-forced": {0x800DD180: 0x24050001},
    "hostile-helper-kept": {0x800DD1A8: 0x00000000},
    "helper-class-2-ends": {0x800DD198: 0x24030002},
    "timer-26": {0x800DD1C0: 0x3C02000D},
    "timer-not-cleared": {0x800DD1BC: 0x00000000},
    "harmless-reacts": {0x800DD1DC: 0x00000000},
    "orbit-stopped-for-anyone": {0x800DD1F4: 0x00000000},
    "orbit-stopped-on-restore": {0x800DD208: 0x00000000},
    "orbit-stopped-without-damage": {0x800DD218: 0x00000000},
    "orbit-not-stopped": {0x800DD220: 0x00000000},
    "harm-ends-alliance-for-anyone": {0x800DD224: 0x00000000},
    "harm-ends-without-bit27": {0x800DD234: 0x00000000},
    "bit25-ignored": {0x800DD24C: 0x10000007},
    "bit25-ignores-bit26": {0x800DD25C: 0x00000000},
    "hostile-relation-reacts": {0x800DD27C: 0x00000000},
    "grievance-never-settled": {0x800DD294: 0x1000000A},
    "settle-keeps-counter": {0x800DD2A0: 0x00601021},
    "settle-class-cleared": {0x800DD2B4: 0x00000000},
    "settle-not-stored": {0x800DD2BC: 0x00000000},
    "grievance-counter-0x3F": {0x800DD2C8: 0x3C0303F0},
    "previous-not-recorded": {0x800DD2E0: 0x00000000},
    "grievance-not-stored": {0x800DD2E4: 0x00000000},
    "grievance-hostile-without-bit26": {0x800DD2F0: 0x00000000},
    "hostile-to-self": {0x800DD2FC: 0x02E02821},
    "hit-stride-0x48": {0x800DD310: 0x26B50048},
    # func_800DD344
    "alliance-ignored": {0x800DD370: 0x10000036},
    "self-not-ally": {0x800DD3B0: 0x00000000},
    "ashley-not-ally": {0x800DD3B8: 0x00000000},
    "noncombatant-not-ally": {0x800DD3C8: 0x10000003},
    "ally-class-1": {0x800DD3D4: 0x24040001},
    "allied-peer-judged": {0x800DD3E0: 0x00000000},
    "hostile-peer-neutral": {0x800DD3F4: 0x10000008},
    "hostile-class-2": {0x800DD404: 0x24040002},
    "hostile-keeps-counter": {0x800DD410: 0x00000000},
    "class-shift-5": {0x800DD424: 0x00042140},
    "class-not-stored": {0x800DD430: 0x00000000},
    "alliance-walk-stops": {0x800DD444: 0x00000000},
    "sharing-ignored": {0x800DD454: 0x10000062},
    "non-ally-mutual": {0x800DD494: 0x00000000},
    "one-sided-mutual": {0x800DD4B8: 0x00000000},
    "non-neutral-candidate": {0x800DD4F0: 0x00000000},
    "inactive-candidate": {0x800DD4F8: 0x00000000},
    "candidate-mutual-not-hostile": {0x800DD50C: 0x00000000},
    "non-ally-defends": {0x800DD544: 0x00000000},
    "one-sided-defender": {0x800DD568: 0x00000000},
    "defender-ally-ignored": {0x800DD580: 0x00000000},
    "former-ally-without-grievance": {0x800DD58C: 0x00000000},
    "former-ally-ignored": {0x800DD598: 0x00000000},
    "defended-still-hostile": {0x800DD5B0: 0x00000000},
    "never-shares": {0x800DD5B8: 0x00000000},
    "candidate-walk-stops": {0x800DD5C8: 0x00000000},
    "mutual-walk-stops": {0x800DD5D8: 0x00000000},
}


def state_address(actor: int) -> int:
    return STATES + actor * STATE_SIZE


def actor_of(pointer: int) -> int:
    offset = pointer - STATES
    if 0 <= offset < ACTORS * STATE_SIZE and offset % STATE_SIZE == 0:
        return offset // STATE_SIZE
    return s32(pointer)


def setting(rng: random.Random) -> int:
    """A settings byte the original tests whole."""
    return 0 if rng.random() < 0.5 else rng.choice((1, rng.randrange(1, 0x100)))


def relation(rng: random.Random) -> int:
    word = rng.getrandbits(32) & ~0xC0 | rng.choice((0, 1, 1, 2, 2, 2, 3, 3, 3)) << 6
    if rng.random() < 0.5:
        word &= ~(0x7F << 20)
    if rng.random() < 0.4:
        word = word & ~(3 << 28) | 2 << 28
    return word | 1 if rng.random() < 0.7 else word & ~1


def factions(rng: random.Random, actors: list, own: int) -> None:
    """Gives relations a faction structure, so allies share hostility and defend candidates."""
    faction = [rng.randrange(3) for _ in range(ACTORS)]
    for index, actor in enumerate(actors):
        for target in range(ACTORS):
            if rng.random() < 0.15:
                continue
            if faction[index] == faction[target]:
                classification = 2
            elif index == own:
                classification = rng.choice((1, 1, 1, 3))
            else:
                classification = rng.choice((3, 3, 1))
            word = actor["relations"][target] & ~0xC0 | classification << 6
            if index == own and rng.random() < 0.85:
                word |= 1
            actor["relations"][target] = word


def hit(rng: random.Random, actor: int) -> dict:
    return {"actor": actor, "hp": rng.choice((0, 1, -1, 40, -40, rng.getrandbits(16))) & 0xFFFF,
            "effect": rng.getrandbits(32) & ~3 | rng.choice((0, 1, 2, 2, 3)),
            "skip": 0 if rng.random() < 0.85 else rng.getrandbits(32) | 1}


def vector(rng: random.Random, number: int) -> dict:
    kind = number % 2
    own = 0 if rng.random() < 0.06 else rng.randrange(1, ACTORS)
    listed = rng.sample(range(ACTORS), rng.choice((0, 1, 2, 3, 4, 4, 5, 6, 6, 8, 10, 16)))
    if 0 in listed and rng.random() < 0.7:
        listed.remove(0)
        listed.insert(0, 0)
    if own not in listed and rng.random() < 0.8:
        listed.insert(rng.randrange(len(listed) + 1), own)
    # React reads the acting actor's state, propagation those of the listed actors.
    present = set(range(ACTORS)) if kind == K_REACT else {*listed, own} | {
        actor for actor in range(ACTORS) if rng.random() < 0.6}
    actors = [{"settings": [setting(rng) for _ in SETTINGS[:-1]]
               + [rng.choice((0, 1, 2, 3, 3, rng.getrandbits(8)))],
               "behavior": rng.choice((1, 1, 0, 2, rng.getrandbits(8))),
               "relations": [relation(rng) for _ in range(ACTORS)]} for _ in range(ACTORS)]
    if kind == K_PROPAGATE and rng.random() < 0.5:
        factions(rng, actors, own)
    # Ashley acting reaches the orbit and alliance paths.
    source = rng.choice((0, 0, 0, 0, 0, own, *range(1, ACTORS)))
    hits = [hit(rng, own if rng.random() < 0.65 else rng.choice((rng.randrange(ACTORS), rng.getrandbits(8))))
            for _ in range(rng.choice((0, 1, 1, 2, 2, 3, 5, 8)))]
    prerequisites = [rng.choice((3, 0, 1, 2, 4, 5, rng.getrandbits(6))) for _ in PREREQUISITE_WORDS]
    acting = hit(rng, source)
    acting["skip"] = 0 if rng.random() < 0.92 else acting["skip"] | 1
    return {"kind": kind, "own": own, "listed": listed, "present": present, "actors": actors,
            "action": rng.randrange(8) if rng.random() < 0.8 else rng.randrange(0x200),
            "beneficial": rng.random() < 0.5, "harmful": rng.random() < 0.6,
            "definition": [rng.getrandbits(32) & ~(0x3F << 7) | value << 7 for value in prerequisites],
            "acting": acting, "hits": hits, "orbit": rng.random() < 0.7,
            "orbiting": rng.choice((1, rng.getrandbits(8))), "noise": rng.randbytes(DATA_SIZE)}


def data_block(v: dict) -> bytes:
    block = bytearray(v["noise"])
    put = field_writer(block, DATA)

    def put_hit(address: int, entry: dict) -> None:
        put(address + HIT_ACTOR, entry["actor"], 1)
        put(address + HIT_HP, entry["hp"], 2)
        put(address + HIT_EFFECT, entry["effect"], 4)
        put(address + HIT_SKIP, entry["skip"], 4)

    put(REQUEST + ACTION, v["action"], 2)
    put_hit(REQUEST + ACTING, v["acting"])
    put(REQUEST + COUNT, len(v["hits"]), 2)
    for index, entry in enumerate(v["hits"]):
        put_hit(REQUEST + HITS + index * HIT_SIZE, entry)
    for index, actor in enumerate(v["listed"]):
        node = NODES + index * NODE_SIZE
        put(node, node + NODE_SIZE if index + 1 < len(v["listed"]) else 0, 4)
        put(node + 4, actor, 4)
    put(ORBIT + ORBITING, v["orbiting"], 1)
    for index, actor in enumerate(v["actors"]):
        state = state_address(index)
        for offset, value in zip(SETTINGS, actor["settings"], strict=True):
            put(state + offset, value, 1)
        put(state + ID_FIELD, index, 1)
        put(state + BEHAVIOR, actor["behavior"], 1)
        for target, word in enumerate(actor["relations"]):
            put(state + RELATIONS + 4 * target, word, 4)
    return bytes(block)


def observe(machine: Machine) -> list[int]:
    values = []
    for index in range(ACTORS):
        state = state_address(index)
        values += [machine.load(state + BEHAVIOR, 1), int(machine.load(state + ALLIED, 1) != 0)]
        values += [machine.load(state + RELATIONS + 4 * target) for target in range(ACTORS)]
    return [*values, machine.load(ORBIT + ORBITING, 1)]


def stub_callees(machine: Machine, v: dict, calls: list[list[int]]) -> None:
    def test(code: int, answer: bool):
        def handler(m: Machine) -> None:
            calls.append([code, s32(m.r[4]), s32(m.r[5])])
            m.r[2] = int(answer)
        return handler

    def alliance(code: int, allied: int):
        def handler(m: Machine) -> None:
            calls.append([code, actor_of(m.r[4]), s32(m.r[5]) if code == C_BEGIN else 0])
            m.put(m.r[4] + ALLIED, allied, 1)
        return handler

    def hostile(m: Machine) -> None:
        calls.append([C_HOSTILE, actor_of(m.r[4]), s32(m.r[5])])
        word = m.r[4] + RELATIONS + 4 * m.r[5]
        m.put(word, m.load(word) & 0xFFFF803F | 0xC0)

    machine.stub(BENEFICIAL, test(C_BENEFICIAL, v["beneficial"]))
    machine.stub(HARMFUL, test(C_HARMFUL, v["harmful"]))
    machine.stub(BEGIN_ALLIANCE, alliance(C_BEGIN, 1))
    machine.stub(END_ALLIANCE, alliance(C_END, 0))
    machine.stub(MAKE_HOSTILE, hostile)


def hit_inputs(entry: dict) -> list[int]:
    return [entry["actor"], s16(entry["hp"]), entry["effect"] & 3, int(entry["skip"] != 0)]


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    for index in range(ACTORS):
        machine.put(STATE_TABLE + 4 * index, state_address(index) if index in v["present"] else 0)
    machine.put(ORBIT_POINTER, ORBIT if v["orbit"] else 0)
    machine.put(BATTLE_LIST_POINTER, NODES if v["listed"] else 0)
    for offset, word in zip(PREREQUISITE_WORDS, v["definition"], strict=True):
        machine.put(DEFINITIONS + v["action"] * DEFINITION_SIZE + offset, word)
    calls: list[list[int]] = []
    stub_callees(machine, v, calls)
    machine.r[4] = state_address(v["own"])
    machine.r[5] = REQUEST
    machine.execute(REACT if v["kind"] == K_REACT else PROPAGATE)
    observation = {"values": observe(machine), "calls": calls, "stores": machine.stores}
    inputs = [v["kind"], v["own"], v["action"], int(v["beneficial"]), int(v["harmful"]),
              *((word >> 7) & 0x3F for word in v["definition"]), int(v["orbit"]), v["orbiting"],
              *hit_inputs(v["acting"]), len(v["hits"])]
    for entry in v["hits"]:
        inputs += hit_inputs(entry)
    inputs += [len(v["listed"]), *v["listed"]]
    for index, actor in enumerate(v["actors"]):
        inputs += [int(index in v["present"]), actor["behavior"], *actor["settings"], *actor["relations"]]
    return observation, inputs


def allowed_stores() -> list[tuple[int, int]]:
    spans = [(ORBIT + ORBITING, ORBIT + ORBITING + 1)]
    for index in range(ACTORS):
        state = state_address(index)
        spans += [(state + BEHAVIOR, state + BEHAVIOR + 1), (state + RELATIONS, state + RELATIONS + 4 * ACTORS)]
    return [*spans, (0x80400000 - 0x4000, 0x80400000)]


ALLOWED = allowed_stores()
NAMES = [f"{name}[{index}]" for index in range(ACTORS) for name in PER_ACTOR] + ["orbiting"]


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    return output_problems(NAMES, native, obs, ALLOWED)


def describe(image: Image, results: list) -> list[str]:
    paths: list[dict[str, int]] = [{}, {}]
    for v, obs, _native in results:
        own = v["own"]
        after = obs["values"]
        names = sorted({CALL_NAMES[call[0]] for call in obs["calls"] if call[0] >= C_BEGIN})
        if v["kind"] == K_REACT:
            source = v["acting"]["actor"]
            reacting = sum(1 for entry in v["hits"] if not entry["skip"] and entry["actor"] == own
                           and source != own)
            if v["acting"]["skip"]:
                signature = "skipped"
            elif own == 0:
                signature = "ashley"
            elif any((word >> 7) & 0x3F == 3 for word in v["definition"]):
                signature = "unreactive"
            else:
                held = after[source * len(PER_ACTOR) + 2 + own]
                settled = held != v["actors"][source]["relations"][own]
                signature = f"{min(reacting, 2)} hits" + "".join(f"+{name}" for name in names)
                signature += "+settled" if settled else ""
        else:
            settings = v["actors"][own]["settings"]
            mode = "alliance" if settings[0] else "sharing" if settings[4] else "idle"
            changed = sum(1 for target in range(ACTORS)
                          if after[own * len(PER_ACTOR) + 2 + target] != v["actors"][own]["relations"][target])
            hostile = sum(1 for call in obs["calls"] if call[0] == C_HOSTILE)
            signature = mode if mode == "idle" else (
                f"{mode} {min(changed, 3)} changed" if mode == "alliance" else f"{mode} {min(hostile, 2)} hostile")
        paths[v["kind"]][signature] = paths[v["kind"]].get(signature, 0) + 1
    lines = []
    for kind, name in ((K_REACT, "F01705 reaction"), (K_PROPAGATE, "F01706 propagation")):
        ranked = sorted(paths[kind].items(), key=lambda item: -item[1])
        lines.append(f"{name} paths: " + ", ".join(f"{path} {count}" for path, count in ranked))
    return lines
