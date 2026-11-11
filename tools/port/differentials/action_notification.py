"""F01719 (74088.c:1596) action notification vs Relations.cpp.

func_800DEEA4 lets the acting actor of a completed action engage again at once, clearing its AI
state's reengagement grace unless the acting entry is skipped or the actor has no state, and then
has every AI actor react through func_800DC810, stubbed here to record whether it received the
request. The request and the AI states hold random bytes around the fields; vectors keep the
acting actor within the AI slots, where the native refuses what the original would read outside
the state table, and always pass a request, which the live binding checks before the call.
"""

from __future__ import annotations

import random

from differential import Image, Machine, field_writer, output_problems

ID = "F01719"
ENTRY, END = 0x800DEEA4, 0x800DEEFC
DRIVER = "actor_reaction_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\Ai\Relations.cpp",)
VECTORS, SEED = 1024, 1719

APPLY = 0x800DC810
STATE_TABLE = 0x800F5878
K_NOTIFY, C_APPLY = 2, 5

DATA = 0x80500000
REQUEST = DATA
STATES, STATE_SIZE = DATA + 0x1000, 0x200
DATA_SIZE = 0x1000 + 16 * STATE_SIZE
ACTORS = 16
ACTING_ACTOR, ACTING_SKIP, GRACE = 0x4, 0x44, 0x9A

MUTANTS = {
    "skipped-entry-clears": {0x800DEEB8: 0x00000000},
    "skipped-entry-not-applied": {0x800DEEB8: 0x1440000C},
    "actor-from-byte-5": {0x800DEEC0: 0x90820005},
    "missing-state-written": {0x800DEED8: 0x00000000},
    "grace-not-cleared": {0x800DEEE0: 0x00000000},
    "cooldown-cleared": {0x800DEEE0: 0xA4400098},
    "grace-byte-cleared": {0x800DEEE0: 0xA040009A},
    "changes-not-applied": {0x800DEEE4: 0x00000000},
}


def state_address(actor: int) -> int:
    return STATES + actor * STATE_SIZE


def vector(rng: random.Random, number: int) -> dict:
    return {"actor": rng.randrange(ACTORS), "skip": 0 if rng.random() < 0.7 else rng.getrandbits(32) | 1,
            "present": [rng.random() < 0.75 for _ in range(ACTORS)],
            "grace": [rng.choice((0, 1, 0x5A, rng.getrandbits(16))) for _ in range(ACTORS)],
            "noise": rng.randbytes(DATA_SIZE)}


def data_block(v: dict) -> bytes:
    block = bytearray(v["noise"])
    put = field_writer(block, DATA)
    put(REQUEST + ACTING_ACTOR, v["actor"], 1)
    put(REQUEST + ACTING_SKIP, v["skip"], 4)
    for index, grace in enumerate(v["grace"]):
        put(state_address(index) + GRACE, grace, 2)
    return bytes(block)


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    for index, present in enumerate(v["present"]):
        machine.put(STATE_TABLE + 4 * index, state_address(index) if present else 0)
    calls: list[list[int]] = []
    machine.stub(APPLY, lambda m: calls.append([C_APPLY, int(m.r[4] == REQUEST), 0]))
    machine.r[4] = REQUEST
    machine.execute(ENTRY)
    observation = {"values": [machine.load(state_address(index) + GRACE, 2) for index in range(ACTORS)],
                   "calls": calls, "stores": machine.stores}
    inputs = [K_NOTIFY, v["actor"], int(v["skip"] != 0)]
    for present, grace in zip(v["present"], v["grace"], strict=True):
        inputs += [int(present), grace]
    return observation, inputs


ALLOWED = [*((state_address(index) + GRACE, state_address(index) + GRACE + 2) for index in range(ACTORS)),
           (0x80400000 - 0x4000, 0x80400000)]
NAMES = [f"grace[{index}]" for index in range(ACTORS)]


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    return output_problems(NAMES, native, obs, ALLOWED)


def describe(image: Image, results: list) -> list[str]:
    paths: dict[str, int] = {}
    for v, obs, _native in results:
        signature = ("skipped" if v["skip"] else "cleared" if v["present"][v["actor"]] else "no state")
        signature += "+applied" if obs["calls"] else ""
        paths[signature] = paths.get(signature, 0) + 1
    ranked = sorted(paths.items(), key=lambda item: -item[1])
    return ["F01719 paths: " + ", ".join(f"{path} {count}" for path, count in ranked)]
