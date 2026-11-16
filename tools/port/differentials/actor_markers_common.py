"""Original actor-marker instructions over stubbed projection, gimmick and draw services.

The projection (func_800A1280, func_800A190C), GTE (SetRotMatrix, SetTransMatrix,
RotTransPers), gimmick (func_8009F858), sprite (vs_battle_setSprite) and AddPrim callees
are stubbed at the call boundary with the same contracts the native driver's world
implements; the packed-coordinate helper func_80096254 and the arrow are followed.
"""

from __future__ import annotations

from differential import DEFAULT_STACK, MASK, s16, store_problems

SPECS = {
    "F00731": (0, 0x80095C18, 0x80096254),
    "F00733": (1, 0x8009627C, 0x800962E4),
    "F00737": (2, 0x8009651C, 0x80096768),
    "F00740": (3, 0x80096FF0, 0x800970BC),
}
FOLLOW = {
    "F00731": (0x80096254, 0x8009651C),
    "F00733": (0x80095C18, 0x80096254, 0x8009651C),
    "F00737": (),
    "F00740": (),
}
MUTATIONS = {
    "F00731": {
        "talk-bit": {0x80095C2C: 0x32220004},
        "pinned-mask": {0x80095C54: 0x32310003},
        "pinned-limit": {0x80095C5C: 0x2C420003},
        "depth-bias": {0x80095CB0: 0x2445FFF1},
        "near-side-margin": {0x80095CB4: 0x24C20021},
        "far-side-margin": {0x80095CDC: 0x24C2FFE1},
        "nearest-slot": {0x80095D10: 0x24050041},
        "farthest-slot": {0x80095D1C: 0x240507FE},
        "dot-stride": {0x80095D8C: 0x00021080},
        "dot-texture": {0x80095D80: 0x3652125B},
        "left-edge": {0x80095DDC: 0x28420011},
        "right-edge": {0x80095DFC: 0x28420130},
        "top-reach": {0x80095E40: 0x28420009},
        "bottom-edge": {0x80095E5C: 0x284200D0},
        "alert-side": {0x80095E9C: 0x25080003},
        "question-offset": {0x80095ED0: 0x35080003},
        "small-frame-side": {0x80095F20: 0x24420005},
        "exclamation-mode": {0x80095F50: 0x34630016},
        "frame-color": {0x80095F74: 0x34A58081},
        "frame-page": {0x80095FF0: 0x3C030016},
        "talk-near-distance": {0x800960D4: 0x28A20161},
        "talk-far-distance": {0x800960FC: 0x28A201A0},
        "talk-lift": {0x80096164: 0x2442FFC1},
        "talk-center": {0x80096180: 0x2442FFF9},
        "talk-frame-mask": {0x800961C8: 0x30420003},
        "talk-texture-base": {0x80096210: 0x24630061},
        "missing-speaker-keeps-marker": {0x8009622C: 0},
    },
    "F00733": {
        "hidden-test": {0x80096290: 0x1040000F},
        "byte-hidden": {0x80096280: 0x80422270},
        "first-slot": {0x80096298: 0x24100002},
        "slot-limit": {0x800962C4: 0x2A02000F},
        "kind-argument": {0x800962B4: 0x02002821},
    },
    "F00737": {
        "direction-4-index": {0x800965D4: 0x24060001},
        "direction-10": {0x800965F0: 0x2402000B},
        "turn-step": {0x8009664C: 0x24C60003},
        "turn-wrap": {0x80096658: 0x30C6000F},
        "packet-length": {0x80096640: 0x24020008},
        "shade-command": {0x80096660: 0x2402002C},
        "first-u": {0x80096704: 0x24020031},
        "far-coordinate": {0x800966E4: 0x2403003E},
        "texture-page": {0x80096718: 0x2402000D},
        "clut": {0x80096720: 0x240237FC},
        "ordering-offset": {0x8009674C: 0x2484FFF4},
        "packet-stride": {0x80096744: 0x26100024},
    },
    "F00740": {
        "display-bit": {0x8009700C: 0x30420010},
        "anchor-tag": {0x80097024: 0x240500FA},
        "unsigned-cue-y": {0x8009703C: 0x96040002},
        "cue-top": {0x8009704C: 0x24030029},
        "cue-right-limit": {0x80097064: 0x28420121},
        "cue-lift-direction": {0x80097070: 0x00641021},
        "frames-subtract": {0x8009709C: 0x00521023},
    },
}

SCRATCH, SETTINGS, POSITION = 0x1F800000, 0x80060020, 0x80500000
MARKERS, HIDDEN, FRAMES, SIDES, CUE = 0x800F2260, 0x800F2270, 0x800F2272, 0x800F2274, 0x800F2280
PACKETS, ORDERING = 0x80600000, 0x80700000
VIEW = SCRATCH + 0x14
ACTORS, ACTOR_WORDS, FIRST_ACTOR = 16, 10, 31
INPUT_WORDS = FIRST_ACTOR + ACTORS * ACTOR_WORDS
STORAGE_WORDS = 512
EVENT_WORDS = 5
STORAGE_STEP, ORDERING_STEP = 0x9E3779B9, 0x01000193
FAILED = MASK


def storage_word(seed: int, index: int) -> int:
    return (seed ^ (index * STORAGE_STEP)) & MASK


def ordering_word(seed: int, slot: int) -> int:
    return (seed + slot * ORDERING_STEP) & MASK


def actor_word(vector, actor: int, field: int) -> int:
    return vector[FIRST_ACTOR + actor * ACTOR_WORDS + field]


def candidate_slots(vector) -> list[int]:
    slots = [-4, 0]
    for actor in range(ACTORS):
        depth = s16(actor_word(vector, actor, 3)) - 16
        slots.append(min(max(depth, 64), 2047))
    return slots


def pick(rng, number, edges, low, high):
    return edges[number % len(edges)] if rng.random() < 0.5 else rng.randint(low, high)


def screen_actor(rng, number, player_x):
    near = player_x + rng.choice((-33, -32, -31, 0, 31, 32, 33))
    x = rng.choice((near, pick(rng, number, (15, 16, 17, 303, 304, 305, -32768, 32767), -60, 380)))
    y = pick(rng, number // 3, (7, 8, 9, 39, 40, 41, 207, 208, 209, -32768, 32767), -60, 260)
    depth = pick(rng, number // 5, (79, 80, 81, 2062, 2063, 2064, -32768, 32767, 0), -100, 2200)
    return x, y, depth


def bone_actor(rng, player):
    total = rng.choice((351, 352, 353, 415, 416, 417, rng.randint(0, 900)))
    dx = rng.randint(0, total)
    dz = total - dx
    if rng.random() < 0.5:
        dx = -dx
    if rng.random() < 0.5:
        dz = -dz
    if rng.random() < 0.1:
        dx += rng.choice((65536, -65536, 40000))
    return player[0] - dx, rng.randint(-0x8000, 0x7FFF), player[2] - dz


def actors(rng, number):
    words = []
    player_x = rng.randint(-40, 360)
    bone_player = (rng.randint(-0x8000, 0x7FFF), 0, rng.randint(-0x8000, 0x7FFF))
    for actor in range(ACTORS):
        if actor == 0:
            screen = (player_x, rng.randint(-60, 260), rng.randint(-100, 2200))
            bone = (bone_player[0], rng.randint(-0x8000, 0x7FFF), bone_player[2])
        else:
            screen = screen_actor(rng, number + actor, player_x)
            bone = bone_actor(rng, bone_player)
        gimmick = rng.choice((-1, -2, 0, 0, 0, 1, 2, 7, 15, 255, 0x0FFFFFFF, rng.getrandbits(32)))
        sxy = rng.choice((rng.getrandbits(32), 0x00080008, 0x80008000, 0x00070007, 0xFFFFFFFF))
        words.extend((
            int(rng.random() < 0.88), *screen,
            int(rng.random() < 0.88), *bone,
            gimmick, sxy,
        ))
    return words


KINDS = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x0F, 0x10, 0x18, 0x40, 0x80, 0x81, 0x82, 0x83, 0x84,
         0x85, 0x86, 0x87, 0x88, 0x8F, 0x90, 0xFF, 0x100, -1, -8, -0x7F, -0x80)


def vector_for(identifier, rng, number):
    operation = SPECS[identifier][0]
    markers = [rng.choice((0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0x0F, 0x0F, 0x80, 0x84, 0x88, 0x8F, 0xFF,
                           rng.getrandbits(8))) for _ in range(ACTORS)]
    if operation == 0:
        arg0, arg1 = number % ACTORS, KINDS[number % len(KINDS)]
        if number % 7 == 0:
            arg1 = rng.getrandbits(32)
    elif operation == 1:
        arg0, arg1 = 0, 0
    elif operation == 2:
        arg0, arg1 = (number % 16 if number % 5 else rng.choice((-1, 0x14, rng.getrandbits(32)))), 0
    else:
        arg0, arg1 = rng.choice((0, 1, -1, 30, 0x7FFF, -0x8000, rng.getrandbits(32))), 0
    cue = (
        pick(rng, number, (247, 248, 249, 287, 288, 289, -32768, 32767), -100, 400),
        pick(rng, number // 2, (-32768, -1, 0, 39, 40, 41, 32767), -100, 300),
        rng.getrandbits(16),
        rng.getrandbits(16),
    )
    scalar = [
        operation, arg0, arg1,
        rng.choice((0, 0, 0, 0, 1, -1, 0x100)),
        rng.getrandbits(32),
        rng.getrandbits(16),
        rng.getrandbits(32) | (0x20 if rng.random() < 0.75 else 0),
        PACKETS + 4 * rng.randrange(64),
        ORDERING + 0x100 + 4 * rng.randrange(16),
        rng.getrandbits(32),
        rng.getrandbits(32),
        *cue,
        *markers,
    ]
    result = [value & MASK for value in (*scalar, *actors(rng, number))]
    focus = FOCUS.get(operation)
    if focus is not None and number % 2 == 0:
        focus(rng, result)
    assert len(result) == INPUT_WORDS, len(result)
    return result


def set_actor(words, actor, field, value):
    words[FIRST_ACTOR + actor * ACTOR_WORDS + field] = value & MASK


def focus_marker(rng, words):
    """Half of the F00731 vectors: kind 4 at the clamp edges or a drawable talk marker."""
    actor = words[1]
    if rng.random() < 0.5:
        words[2] = rng.choice((4, 0x84))
        x = rng.choice((15, 16, 17, 303, 304, 305, rng.randint(17, 303)))
        y = rng.choice((7, 8, 9, 39, 40, 41, 207, 208, 209, rng.randint(9, 207)))
        set_actor(words, actor, 0, 1)
        set_actor(words, 0, 0, 1)
        set_actor(words, actor, 1, x)
        set_actor(words, actor, 2, y)
        return
    if rng.random() < 0.4:
        words[2] = rng.choice((0, 1, 5, 6, 0x10))
        words[4] ^= rng.getrandbits(1) << actor
        player_x = s16(words[FIRST_ACTOR + 1])
        set_actor(words, actor, 0, 1)
        set_actor(words, 0, 0, 1)
        set_actor(words, actor, 1, player_x + rng.choice((-33, -32, 32, 33)))
        return
    words[2] = rng.choice((8, 0x88, 0x0F, 0x18))
    words[15 + actor] = rng.choice((0, 3, 7, 11, 0x0F, 0x87, 0xFB, 0xFF))
    words[4] &= ~(1 << actor) & MASK
    set_actor(words, actor, 4, 1)
    set_actor(words, 0, 4, 1)
    set_actor(words, actor, 8, rng.choice((0, 0, 0, 1, 3, -1)))


def focus_cue(rng, words):
    """Half of the F00740 vectors: the player's anchor at the cue's top and right limits."""
    y = rng.choice((39, 40, 41, rng.randint(-60, 39)))
    lift = max(40 - y, 0)
    x = rng.choice((287, 288, 289)) - lift
    words[6] |= 0x20
    set_actor(words, 0, 0, 1)
    set_actor(words, 0, 1, x)
    set_actor(words, 0, 2, y)


FOCUS = {0: focus_marker, 3: focus_cue}


def run_for(identifier, machine, vector):
    operation, entry, _ = SPECS[identifier]
    v = vector
    cursor, base = v[7], v[8]
    for address, size in ((0x800F0000, 0x10000), (0x80060000, 0x100), (SCRATCH, 0x100),
                          (POSITION, 0x10), (PACKETS, 0x1000), (ORDERING, 0x2400)):
        machine.map_zero(address, size)
    for index in range(STORAGE_WORDS):
        machine.put(cursor + index * 4, storage_word(v[10], index))
    for slot in candidate_slots(v):
        machine.put(base + slot * 4, ordering_word(v[9], slot))
    machine.put(SCRATCH, cursor)
    machine.put(SCRATCH + 4, base)
    machine.put(SETTINGS, v[6])
    machine.put(HIDDEN, v[3], 2)
    machine.put(FRAMES, v[5], 2)
    machine.put(SIDES, v[4])
    for index in range(4):
        machine.put(CUE + index * 2, v[11 + index], 2)
        machine.put(POSITION + index * 2, v[11 + index], 2)
    for index in range(ACTORS):
        machine.put(MARKERS + index, v[15 + index], 1)
    events = []
    views = [0]

    def emit(*values):
        events.append([value & MASK for value in (*values, *([0] * (EVENT_WORDS - len(values))))])

    def anchor(m):
        actor, tag, out, endpoint = m.r[4:8]
        emit(1, actor, tag, endpoint)
        if actor < ACTORS and actor_word(v, actor, 0):
            m.put(out, actor_word(v, actor, 1), 2)
            m.put(out + 4, actor_word(v, actor, 3), 2)
            m.put(out + 2, actor_word(v, actor, 2), 2)
            m.r[2] = 0
        else:
            m.r[2] = FAILED

    def position(m):
        actor, tag, out, endpoint = m.r[4:8]
        emit(2, actor, tag, endpoint)
        if actor < ACTORS and actor_word(v, actor, 4):
            for index in range(3):
                m.put(out + index * 2, actor_word(v, actor, 5 + index), 2)
            m.r[2] = 0
        else:
            m.r[2] = FAILED

    def rotate_translate(m):
        point = [m.load(m.r[4] + index * 2, 2, signed=True) for index in range(3)]
        emit(5, *point)
        m.put(m.r[5], actor_word(v, views[0] % ACTORS, 9))
        m.put(m.r[6], 0x5A5A5A5A)
        m.put(m.r[7], 0xA5A5A5A5)
        views[0] += 1
        m.r[2] = 0x12345678

    def gimmick(m):
        actor = m.r[4]
        emit(6, actor)
        m.r[2] = actor_word(v, actor, 8) if actor < ACTORS else FAILED

    def sprite(m):
        brightness, xy, wh, chain = m.r[4:8]
        emit(7, brightness, xy, wh, chain)
        pointer = m.load(SCRATCH)
        color = ((0x64 | (2 if brightness & 0x100 else 0)) << 24) | ((brightness & 255) * 0x10101)
        words = ((m.load(chain) & 0xFFFFFF) | 0x04000000, 0, color, xy, 0, wh)
        for index, word in enumerate(words):
            m.put(pointer + index * 4, word)
        m.put(chain, pointer & 0xFFFFFF)
        m.put(SCRATCH, pointer + 24)
        m.r[2] = pointer

    def add_primitive(m):
        chain, primitive = m.r[4:6]
        emit(8, chain, primitive)
        m.put(primitive, (m.load(primitive) & 0xFF000000) | (m.load(chain) & 0xFFFFFF))
        m.put(chain, primitive & 0xFFFFFF)

    machine.stub(0x800A1280, anchor)
    machine.stub(0x800A190C, position)
    machine.stub(0x80041598, lambda m: emit(3, m.r[4]))
    machine.stub(0x80041578, lambda m: emit(4, m.r[4]))
    machine.stub(0x80041814, rotate_translate)
    machine.stub(0x8009F858, gimmick)
    machine.stub(0x800C0230, sprite)
    machine.stub(0x8002B4D4, add_primitive)
    machine.follow.update(FOLLOW[identifier])
    if operation == 0:
        machine.r[4], machine.r[5] = v[1], v[2]
    elif operation == 2:
        machine.r[4], machine.r[5] = POSITION, v[1]
    elif operation == 3:
        machine.r[4] = v[1]
    machine.stores.clear()
    machine.execute(entry)
    final = machine.load(SCRATCH)
    output = [machine.load(MARKERS + index, 1) for index in range(ACTORS)]
    output.append(machine.load(SIDES))
    output.append(machine.load(FRAMES, 2))
    output.extend(machine.load(CUE + index * 2, 2) for index in range(4))
    output.append(final)
    output.extend(machine.load(base + slot * 4) for slot in candidate_slots(v))
    output.append(len(events))
    for event in events:
        output.extend(event)
    words = min(max(final - cursor, 0) // 4, STORAGE_WORDS)
    output.append(words)
    output.extend(machine.load(cursor + index * 4) for index in range(words))
    return {"output": output, "stores": machine.stores, "cursor": cursor, "base": base}, vector


def output_problems(native, expected):
    if native == expected:
        return []
    shared = min(len(native), len(expected))
    first = shared
    for index in range(shared):
        if native[index] != expected[index]:
            first = index
            break
    window = slice(first, first + 8)
    return [f"word {first} of {len(native)}/{len(expected)}: "
            f"native {native[window]} original {expected[window]}"]


def check_for(_identifier, _image, _vector, observation, native, _reference):
    problems = output_problems(native, observation["output"])
    cursor, base = observation["cursor"], observation["base"]
    allowed = [
        (DEFAULT_STACK - 0x100, DEFAULT_STACK),
        (MARKERS, MARKERS + ACTORS),
        (FRAMES, FRAMES + 2),
        (SIDES, SIDES + 4),
        (CUE, CUE + 8),
        (SCRATCH, SCRATCH + 4),
        (cursor, cursor + STORAGE_WORDS * 4),
        (base - 16, base + 2048 * 4),
    ]
    return problems + store_problems(observation["stores"], allowed)
