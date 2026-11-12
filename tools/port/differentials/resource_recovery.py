"""F00500-F00502 with the real original/native stat dispatcher and guarded hits."""

from dataclasses import replace

from differential import DEFAULT_STACK, store_problems
from differentials import damage_recovery as shared

ID = "F00500-F00502"
ENTRY, END = 0x80084EEC, 0x80084FF0
ENTRIES = (ENTRY, 0x80084F40, 0x80084F94)
MODULES, SOURCES = shared.MODULES, shared.SOURCES
DRIVER = "resource_recovery_driver.cpp"
VECTORS, SEED, MUTANT_VECTORS = 3072, 500502, 768
MUTANTS = {
    "F00500-no-calculate": {0x80084F00: 0},
    "F00500-wrong-execute": {0x80084F04: 0xAFA00010},
    "F00500-wrong-mode": {0x80084F14: 0x3C050010},
    "F00500-neighbor-mask": {0x80084F08: 0x3C03FF0F},
    "F00500-no-flags": {0x80084F24: 0},
    "F00500-subtract": {0x80084F28: 0x00621823},
    "F00500-no-PP": {0x80084F2C: 0},
    "F00501-no-calculate": {0x80084F54: 0},
    "F00501-wrong-execute": {0x80084F58: 0xAFA00010},
    "F00501-wrong-mode": {0x80084F68: 0x3C050004},
    "F00501-neighbor-mask": {0x80084F5C: 0x3C03FFCF},
    "F00501-no-flags": {0x80084F78: 0},
    "F00501-wrong-field": {0x80084F70: 0x9603003A},
    "F00501-subtract": {0x80084F7C: 0x00621823},
    "F00501-no-DP": {0x80084F80: 0},
    "F00502-no-calculate": {0x80084FA8: 0},
    "F00502-wrong-execute": {0x80084FAC: 0xAFA00010},
    "F00502-no-zero-gate": {0x80084FB8: 0},
    "F00502-invert-zero-gate": {0x80084FB8: 0x14400009},
    "F00502-wrong-mode": {0x80084FD0: 0x34630010},
    "F00502-neighbor-mask": {0x80084FC4: 0x2402FF3F},
    "F00502-no-flags": {0x80084FD4: 0},
    "F00502-subtract": {0x80084FD8: 0x00441023},
    "F00502-no-risk": {0x80084FDC: 0},
}


def vector(rng, number):
    return replace(shared.vector(rng, number * 2), function=number % 3)


def run(machine, value):
    return shared.run(machine, value, ENTRIES)


def check(image, value, observation, native, reference):
    del image, value, reference
    expected = [*observation["memory"], len(observation["events"]), *observation["events"]]
    allowed = [(DEFAULT_STACK - 0x100, DEFAULT_STACK), (shared.LAST, shared.LAST + 4),
               (shared.TARGET + 8, shared.TARGET + 10),
               (shared.TARGET + 0x1C, shared.TARGET + 0x20),
               (shared.TARGET + 0x38, shared.TARGET + 0x3C)]
    return ([] if native == expected else ["resource, neighbor or stat callback differs"]) + \
        store_problems(observation["stores"], allowed)


def describe(image, results):
    return [f"F{500+i:05}: {sum(v.function == i for v, _, _ in results)} vectors; "
            f"[{start:08X},{end:08X}) {(end-start)//4} instructions; "
            f"sha256={image.sha256(start, end)}"
            for i, (start, end) in enumerate(zip(ENTRIES, (*ENTRIES[1:], END), strict=True))]
