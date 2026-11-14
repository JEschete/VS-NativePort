"""Compare a function's original MIPS instructions with its owned native code (#340).

  python tools\\port\\differential.py <case> [--vectors N] [--no-mutants] [--no-build] [--log PATH]
  python tools\\port\\differential.py <case> --replace SOURCE OLD NEW [--vectors N]
  python tools\\port\\differential.py --list

The original side runs the function's own R3000 instructions from the extracted module
(Native\\data\\disc, never committed) in the interpreter below. The native side is a small C++
driver compiled against the owned sources: it reads one vector per line on stdin and prints one
line of integers. A case, tools\\port\\differentials\\<case>.py, supplies only what differs per
function:

  ID, ENTRY, END        catalog ID and the original instruction range [ENTRY, END)
  MODULES               module paths under Native\\data\\disc (default BATTLE/BATTLE.PRG); the
                        first is the function's own module and is passed to the driver
  DRIVER, SOURCES       the driver next to the case and the Native sources it compiles with
  COMPILER, CXX_STANDARD, COMPILER_OPTIONS
                        optional compiler, standard and additional flags (defaults: cl, c++20)
  VECTORS, SEED         default vector count and random seed
  MUTANTS               {name: {address: replacement word}}; every mutant must be detected
  MUTANT_VECTORS        optional: mutants rerun only the first N vectors (default: all)
  vector(rng, number)   -> one input
  run(machine, vector)  -> (observation, driver inputs); stubs calls, then executes ENTRY
                        (Machine.execute's `stop` ends a run of code inlined in a caller)
  check(image, vector, observation, native, reference) -> problems
                        native is the driver's integers; reference is the unpatched observation
                        the driver inputs were taken from (equal to observation outside mutants)
  describe(image, results)  optional coverage lines over (vector, observation, native)

A mutant patches original instruction words; it is detected when check() reports a problem for
at least one vector, or the patched instructions fault. The report states the instruction count
and SHA-256 of the original range, the vector and mismatch counts and each mutant's detection
count, and is written to build\\differential\\<case>.log.

The interpreter executes branch delay slots but not load delay slots (compiled PS1 code never
reads a loaded register in the next instruction). Calls must be stubbed or explicitly followed,
so an unexpected callee stops the run instead of executing silently. A stub registered with
tail=True also takes a jump or branch to its address (a tail call) and continues at $ra. The GTE model covers moves
to and from V0, IR0-IR3, MAC0-MAC3, SXY0-SXY2 and the leading-zero counter
(LZCS/LZCR), plus MFC2 SZ3, LWC2 V0 and SWC2 MAC0-MAC3/IR0/SXY2. Rotation,
translation and OFX/OFY/H control writes are modelled; other control moves
(except FLAG), SXYP and colour aliases still raise.
RTPS supports only sf=1/lm=0, including signed44 accumulation, saturation, integer UNR
division, screen/depth FIFOs and DQA/DQB depth cueing. Controls start deterministically
at zero, not physical game state; fixtures must initialize their transform/projection
state. The SQR, GPF and NCLIP commands are also modelled. MVMVA supports only sf=1,
rotation matrix, V0, TR or no translation and lm=0 (SDK RotTrans/ApplyMatrixSV);
other configurations raise. Only its fake command-number bits are ignored.
RotTrans's SWC2 MAC1-MAC3 result stores preserve the full post-shift MAC words,
not the saturated IR values. Other data-register store aliases still raise.
FLAG command results are modelled for RTPS, NCLIP and this MVMVA subset; reading FLAG
after SQR/GPF raises rather than reporting unmodelled status. Other COP instructions,
syscalls, unaligned accesses and overflow traps raise.

A native broken variant (--replace SOURCE OLD NEW) builds a separate driver from a copy of one of
the case's SOURCES with one exact text replacement, leaving the tree untouched, and runs the
vectors without mutants; the variant is detected when at least one vector mismatches. Its report
is build\\differential\\<case>-variant.log.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import os
import random
import subprocess
import sys
from collections.abc import Callable, Iterable
from dataclasses import dataclass, field
from pathlib import Path
from types import ModuleType

ROOT = Path(__file__).resolve().parents[2]
DISC = ROOT / "Native" / "data" / "disc"
CASES = Path(__file__).resolve().parent / "differentials"
OUTPUT = ROOT / "build" / "differential"
MODULE_BASES = {
    "BATTLE/BATTLE.PRG": 0x80068800,
    "MENU/MENUB.PRG": 0x80102800,
}
STOP = 0xFFFFFFF0
DEFAULT_STACK = 0x80400000
DEFAULT_STACK_SIZE = 0x4000
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat")
VSWHERE = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
REGISTERS = ("zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
             "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra")
R = {name: index for index, name in enumerate(REGISTERS)}
MASK = 0xFFFFFFFF


def s16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def s32(value: int) -> int:
    value &= MASK
    return value - 0x100000000 if value & 0x80000000 else value


def pack_fields(raw: bytes, fields, values: dict) -> bytearray:
    """`raw` with each (name, offset, size, ...) field set from `values`, truncated to its size."""
    block = bytearray(raw)
    for name, offset, size, *_ in fields:
        block[offset:offset + size] = (values[name] & ((1 << size * 8) - 1)).to_bytes(size, "little")
    return block


def placed_state(block: bytearray, offset: int, state: bytearray, model: int) -> bytes:
    """`block` with an AI state (`state`, already packed) at `offset`, its model pointer (0x58) set to `model`."""
    state[0x58:0x5C] = model.to_bytes(4, "little")
    block[offset:offset + len(state)] = state
    return bytes(block)


def field_writer(block: bytearray, base: int) -> Callable[[int, int, int], None]:
    """put(address, value, size) storing `value`'s low `size` bytes into `block`, loaded at `base`."""
    def put(address: int, value: int, size: int) -> None:
        at = address - base
        if not 0 <= at <= len(block) - size:
            raise ValueError(f"{address:#x}+{size} is outside the block at {base:#x}")
        block[at:at + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")
    return put


# Comparison helpers shared by the cases' check() functions.
def field_problems(names, native: list[int], original: list[int], hexadecimal: bool = True) -> list[str]:
    """One "fields: ..." problem naming each of `names` the two runs left different, or none."""
    shown = "{:#x}" if hexadecimal else "{}"
    differing = [f"{name} native {shown.format(mine)} original {shown.format(theirs)}"
                 for name, mine, theirs in zip(names, native, original, strict=False) if mine != theirs]
    return ["fields: " + ", ".join(differing)] if differing else []


def call_problems(native: list[list[int]], original: list[list[int]]) -> list[str]:
    """One problem quoting the first two calls from where the recorded call lists diverge, or none."""
    if native == original:
        return []
    first = next((i for i, (a, b) in enumerate(zip(native, original, strict=False)) if a != b),
                 min(len(native), len(original)))
    return [f"calls differ from {first}: native {native[first:first + 2]} original {original[first:first + 2]}"]


def store_problems(stores, allowed: list[tuple[int, int]]) -> list[str]:
    """The first store the original made outside the `allowed` [low, high) spans, or none."""
    for address, size in stores:
        if not any(low <= address and address + size <= high for low, high in allowed):
            return [f"original stored {size} bytes at {address:#x}"]
    return []


def output_problems(names, native: list[int], observation: dict, allowed: list[tuple[int, int]]) -> list[str]:
    """Compares a driver's [refused, value per name..., call count, three words per call...] output with the
    original's observation {"values", "calls", "stores"}."""
    if native[0]:
        return ["native refused"]
    values = len(names)
    problems = [f"{name} native {theirs & MASK:#x} original {ours & MASK:#x}"
                for name, theirs, ours in zip(names, native[1:1 + values], observation["values"], strict=True)
                if theirs != ours]
    first = 2 + values
    calls = [native[first + i * 3:first + (i + 1) * 3] for i in range(native[1 + values])]
    problems += call_problems(calls, observation["calls"])
    return problems + store_problems(observation["stores"], allowed)


# Vector helpers for the battle-AI cases. A tile word holds x in byte 0 and z in byte 2; bytes 1
# and 3 carry per-actor metadata, drawn at random here.
def packed(rng: random.Random, x: int, z: int) -> int:
    return rng.getrandbits(8) << 24 | (z & 0xFF) << 16 | rng.getrandbits(8) << 8 | (x & 0xFF)


def near(rng: random.Random, tile: int, spread: int) -> int:
    """A tile word within `spread` tiles of `tile` on each axis, wrapped to a byte."""
    x, z = tile & 0xFF, tile >> 16 & 0xFF
    return packed(rng, x + rng.randrange(-spread, spread + 1), z + rng.randrange(-spread, spread + 1))


def position(rng: random.Random, tile_coordinate: int) -> int:
    """A fine coordinate (128 per tile) on or beside the tile, sometimes anywhere."""
    if rng.random() < 0.9:
        return s16(tile_coordinate * 128 + rng.randrange(-64, 192))
    return s16(rng.getrandbits(16))


# The GTE's leading-zero counter, data registers 30 and 31: the square root and normalisation
# helpers use it as a plain integer instruction.
LZCS, LZCR = 30, 31
VXY0, VZ0 = 0, 1
# GTE data registers IR1-IR3 (signed halfwords) and MAC0-MAC3 (words).
IR = (9, 10, 11)
MAC = (25, 26, 27)
MAC0 = 24
GTE_RTPS = 0x01
GTE_NCLIP = 0x06
GTE_MVMVA = 0x12
GTE_SQR = 0x28
GTE_GPF = 0x3D
# IR0 (a signed halfword) and the screen XY FIFO entries SXY0-SXY2 (words), which some routines
# use as scratch registers across calls.
IR0 = 8
SXY = (12, 13, 14)
SZ = (16, 17, 18, 19)
OFX, OFY, H, DQA, DQB = 24, 25, 26, 27, 28
SCRATCH_GTE = (IR0, *SXY)


def leading_sign_bits(value: int) -> int:
    """LZCR: how many leading bits of LZCS equal its sign bit (32 for 0 and -1)."""
    value &= MASK
    if value & 0x80000000:
        value = ~value & MASK
    return 32 - value.bit_length()


class Fault(RuntimeError):
    """The original instructions did something the interpreter treats as a hardware fault."""


@dataclass(frozen=True)
class Image:
    """A module's bytes at their PS1 load address."""

    data: bytes
    base: int
    path: Path | None = None

    @classmethod
    def load(cls, module: str) -> Image:
        path = DISC / Path(module)
        data = path.read_bytes()
        if data[:8] == b"PS-X EXE":
            return cls(data[0x800:], int.from_bytes(data[0x18:0x1C], "little"), path)
        if module not in MODULE_BASES:
            raise SystemExit(f"{module}: no load address is known; add it to MODULE_BASES")
        return cls(data, MODULE_BASES[module], path)

    def contains(self, address: int, size: int = 1) -> bool:
        return self.base <= address and address + size <= self.base + len(self.data)

    def read(self, address: int, size: int = 4, signed: bool = False) -> int:
        if not self.contains(address, size):
            raise Fault(f"read {address:#x} outside the module")
        offset = address - self.base
        return int.from_bytes(self.data[offset:offset + size], "little", signed=signed)

    def sha256(self, start: int, end: int) -> str:
        return hashlib.sha256(self.data[start - self.base:end - self.base]).hexdigest()


class Machine:
    """An R3000 integer core over module images, a sparse overlay and zero-filled regions."""

    def __init__(self, images: Image | Iterable[Image], patches: dict[int, int] | None = None,
                 stack: int = DEFAULT_STACK, stack_size: int = DEFAULT_STACK_SIZE) -> None:
        self.images = [images] if isinstance(images, Image) else list(images)
        self.patches = dict(patches or {})
        self.overlay: dict[int, int] = {}
        self.zero: list[tuple[int, int]] = [(stack - stack_size, stack)]
        self.r = [0] * 32
        self.hi = self.lo = 0
        self.lzcs = 0
        self.gte = dict.fromkeys((VXY0, VZ0) + IR + MAC + (MAC0,) + SCRATCH_GTE + SZ, 0)
        self.gte_rotation = [0] * 5
        self.gte_translation = [0] * 3
        self.gte_projection = dict.fromkeys((OFX, OFY, H, DQA, DQB), 0)
        self.gte_flag: int | None = 0
        self.r[R["sp"]] = stack
        self.stubs: dict[int, Callable[[Machine], None]] = {}
        self.tail_stubs: set[int] = set()
        self.follow: set[int] = set()
        self.trace: list[tuple] = []
        self.stores: list[tuple[int, int]] = []
        self.steps = 0

    # ------------------------------------------------------------------ setup helpers
    def map_zero(self, address: int, size: int) -> None:
        self.zero.append((address, address + size))

    def store(self, address: int, data: bytes) -> None:
        for offset, value in enumerate(data):
            self.overlay[address + offset] = value

    def put(self, address: int, value: int, size: int = 4) -> None:
        self.store(address, (value & ((1 << size * 8) - 1)).to_bytes(size, "little"))

    def stub(self, address: int, handler: Callable[[Machine], None] | None = None, tail: bool = False) -> None:
        """Replace a callee; without a handler the call is a no-op returning $v0 unchanged. With
        `tail`, a jump or branch to the address is a tail call: the stub runs and execution
        continues at $ra."""
        self.stubs[address] = handler or (lambda machine: None)
        if tail:
            self.tail_stubs.add(address)

    # ------------------------------------------------------------------ memory
    def byte(self, address: int) -> int:
        value = self.overlay.get(address)
        if value is not None:
            return value
        for start, end in self.zero:
            if start <= address < end:
                return 0
        for image in self.images:
            if image.contains(address):
                return image.data[address - image.base]
        raise Fault(f"unmapped read {address:#x}")

    def bytes(self, address: int, size: int) -> bytes:
        return bytes(self.byte(address + offset) for offset in range(size))

    def load(self, address: int, size: int = 4, signed: bool = False) -> int:
        return int.from_bytes(self.bytes(address, size), "little", signed=signed)

    def load_fields(self, base: int, fields, names) -> list[int]:
        """The (name, offset, size, signed) `fields` at `base` named in `names`, in that order."""
        layout = {name: (offset, size, signed) for name, offset, size, signed, *_ in fields}
        return [self.load(base + layout[name][0], layout[name][1], layout[name][2]) for name in names]

    def write(self, address: int, value: int, size: int) -> None:
        if address % size:
            raise Fault(f"unaligned {size}-byte store {address:#x}")
        self.stores.append((address, size))
        self.put(address, value, size)

    def fetch(self, pc: int) -> int:
        word = self.patches.get(pc)
        if word is not None:
            return word
        for image in self.images:
            if image.contains(pc, 4):
                return image.read(pc)
        raise Fault(f"fetch outside the modules {pc:#x}")

    def aligned_load(self, address: int, size: int, signed: bool) -> int:
        if address % size:
            raise Fault(f"unaligned {size}-byte load {address:#x}")
        return self.load(address, size, signed) & MASK

    # ------------------------------------------------------------------ execution
    def execute(self, entry: int, limit: int = 1_000_000, stop: int = STOP) -> None:
        """Run from entry until it returns to the caller, or until pc reaches `stop` (for code
        inlined in a caller, where nothing returns)."""
        self.r[R["ra"]] = STOP
        pc = entry
        while pc not in (STOP, stop):
            self.steps += 1
            if self.steps > limit:
                raise Fault("runaway original")
            flow = self.step(pc)
            if flow is None:
                pc += 4
                continue
            if self.step(pc + 4) is not None:
                raise Fault(f"control transfer in the delay slot at {pc + 4:#x}")
            kind, target = flow
            if kind == "call":
                pc = pc + 8 if self.call(target) else target
            elif target in self.tail_stubs:
                self.stubs[target](self)
                pc = self.r[R["ra"]]
            else:
                pc = target

    def call(self, address: int) -> bool:
        """Run a stub and return True, or return False to execute the real callee."""
        handler = self.stubs.get(address)
        if handler is not None:
            handler(self)
            return True
        if address in self.follow:
            return False
        raise Fault(f"call {address:#x} is neither stubbed nor followed")

    def step(self, pc: int) -> tuple[str, int] | None:
        word = self.fetch(pc)
        op, rs, rt = word >> 26, (word >> 21) & 31, (word >> 16) & 31
        rd, shift, funct = (word >> 11) & 31, (word >> 6) & 31, word & 63
        imm, r = s16(word), self.r
        flow = None
        if op == 0:
            flow = self.special(pc, funct, rs, rt, rd, shift)
        elif op == 1:
            value = s32(r[rs])
            if rt in (0x10, 0x11):
                r[31] = pc + 8
            if rt not in (0, 1, 0x10, 0x11):
                raise Fault(f"unhandled REGIMM {rt:#x} at {pc:#x}")
            taken = value < 0 if rt in (0, 0x10) else value >= 0
            flow = ("branch", pc + 4 + (imm << 2) if taken else pc + 8)
        elif op in (2, 3):
            if op == 3:
                r[31] = pc + 8
            target = ((pc + 4) & 0xF0000000) | ((word & 0x3FFFFFF) << 2)
            flow = ("call" if op == 3 else "jump", target)
        elif 4 <= op <= 7:
            a, b = s32(r[rs]), s32(r[rt])
            taken = (a == b, a != b, a <= 0, a > 0)[op - 4]
            flow = ("branch", pc + 4 + (imm << 2) if taken else pc + 8)
        elif op == 8:
            r[rt] = self.checked(s32(r[rs]) + imm, pc)
        elif op == 9:
            r[rt] = (r[rs] + imm) & MASK
        elif op == 0xA:
            r[rt] = int(s32(r[rs]) < imm)
        elif op == 0xB:
            r[rt] = int(r[rs] < (imm & MASK))
        elif op == 0xC:
            r[rt] = r[rs] & (word & 0xFFFF)
        elif op == 0xD:
            r[rt] = r[rs] | (word & 0xFFFF)
        elif op == 0xE:
            r[rt] = r[rs] ^ (word & 0xFFFF)
        elif op == 0xF:
            r[rt] = (word & 0xFFFF) << 16
        elif op in (0x20, 0x21, 0x23, 0x24, 0x25):
            size, signed = {0x20: (1, True), 0x21: (2, True), 0x23: (4, False),
                            0x24: (1, False), 0x25: (2, False)}[op]
            r[rt] = self.aligned_load((r[rs] + imm) & MASK, size, signed)
        elif op in (0x22, 0x26):
            r[rt] = self.load_partial(op, (r[rs] + imm) & MASK, r[rt])
        elif op in (0x28, 0x29, 0x2B):
            size = {0x28: 1, 0x29: 2, 0x2B: 4}[op]
            self.write((r[rs] + imm) & MASK, r[rt], size)
        elif op in (0x2A, 0x2E):
            self.store_partial(op, (r[rs] + imm) & MASK, r[rt])
        elif op == 0x32 and rt in (VXY0, VZ0):
            self.gte_write(rt, self.aligned_load((r[rs] + imm) & MASK, 4, False))
        elif op == 0x3A:
            if rt not in (MAC0, *MAC, IR0, 14):
                raise Fault(
                    f"unsupported SWC2 data register {rt} at {pc:#x} "
                    "(only MAC0-MAC3, IR0 and SXY2 are modelled)"
                )
            self.write((r[rs] + imm) & MASK, self.gte_read(rt), 4)
        elif op == 0x12 and rs in (0, 4) and rd in (VXY0, VZ0, LZCS, LZCR, MAC0, *IR, *MAC, *SCRATCH_GTE):
            if rs == 4:
                self.gte_write(rd, r[rt])
            else:
                r[rt] = self.gte_read(rd)
        elif op == 0x12 and rs == 0 and rd == 19:
            r[rt] = self.gte_read(rd)
        elif op == 0x12 and rs in (2, 6) and rd == 31:
            if rs == 6:
                self.gte_write_flag(r[rt])
            elif self.gte_flag is None:
                raise Fault(f"GTE FLAG after SQR/GPF is not modelled at {pc:#x}")
            else:
                r[rt] = self.gte_flag
        elif op == 0x12 and rs == 6 and rd < 5:
            self.gte_rotation[rd] = s16(r[rt]) if rd == 4 else r[rt] & MASK
        elif op == 0x12 and rs == 6 and 5 <= rd <= 7:
            self.gte_translation[rd - 5] = s32(r[rt])
        elif op == 0x12 and rs == 6 and rd in (OFX, OFY, H):
            self.gte_projection[rd] = r[rt] & 0xFFFF if rd == H else s32(r[rt])
        elif op == 0x12 and rs & 0x10 and funct == GTE_RTPS:
            if word & ~0x01F00000 != 0x4A080001:
                raise Fault(
                    f"unsupported GTE RTPS configuration {word:#010x} at {pc:#x} "
                    "(only sf=1, lm=0 is modelled)"
                )
            self.gte_rtps()
        elif op == 0x12 and rs & 0x10 and funct == GTE_NCLIP:
            self.gte_nclip()
        elif op == 0x12 and rs & 0x10 and funct == GTE_MVMVA:
            configuration = word & ~0x01F00000
            if configuration not in (0x4A086012, 0x4A080012):
                raise Fault(f"unsupported GTE MVMVA configuration {word:#010x} at {pc:#x} "
                            "(only sf=1, rotation, V0, TR/no translation, lm=0 is modelled)")
            if configuration == 0x4A080012:
                self.gte_write_flag(self.gte_transform_v0(self.gte_translation))
            else:
                self.gte_rotate_v0()
        elif op == 0x12 and rs & 0x10 and funct == GTE_SQR:
            self.gte_square(shift=word >> 19 & 1, low_limit=word >> 10 & 1)
        elif op == 0x12 and rs & 0x10 and funct == GTE_GPF:
            self.gte_interpolate(shift=word >> 19 & 1, low_limit=word >> 10 & 1)
        elif op == 0x12:
            raise Fault(
                f"unhandled GTE instruction {word:#010x} at {pc:#x} "
                "(only V0, IR0-IR3, MAC0-MAC3, SXY0-SXY2, SZ3 reads, LZCS/LZCR "
                "and FLAG moves, rotation/translation/OFX/OFY/H writes, SQR, GPF, "
                "NCLIP, SDK RTPS and MVMVA are modelled)"
            )
        else:
            raise Fault(f"unhandled opcode {op:#x} at {pc:#x} (COP instructions need a stub)")
        r[0] = 0
        return flow

    def special(self, pc: int, funct: int, rs: int, rt: int, rd: int,
                shift: int) -> tuple[str, int] | None:
        r = self.r
        if funct == 0:
            r[rd] = (r[rt] << shift) & MASK
        elif funct == 2:
            r[rd] = r[rt] >> shift
        elif funct == 3:
            r[rd] = (s32(r[rt]) >> shift) & MASK
        elif funct == 4:
            r[rd] = (r[rt] << (r[rs] & 31)) & MASK
        elif funct == 6:
            r[rd] = r[rt] >> (r[rs] & 31)
        elif funct == 7:
            r[rd] = (s32(r[rt]) >> (r[rs] & 31)) & MASK
        elif funct == 8:
            return ("jump", r[rs])
        elif funct == 9:
            target = r[rs]
            r[rd] = pc + 8
            return ("call", target)
        elif funct in (0xC, 0xD):
            raise Fault(f"{'syscall' if funct == 0xC else 'break'} at {pc:#x}")
        elif funct == 0x10:
            r[rd] = self.hi
        elif funct == 0x11:
            self.hi = r[rs]
        elif funct == 0x12:
            r[rd] = self.lo
        elif funct == 0x13:
            self.lo = r[rs]
        elif funct in (0x18, 0x19):
            a, b = (s32(r[rs]), s32(r[rt])) if funct == 0x18 else (r[rs], r[rt])
            product = (a * b) & 0xFFFFFFFFFFFFFFFF
            self.lo, self.hi = product & MASK, product >> 32
        elif funct == 0x1A:
            self.divide_signed(s32(r[rs]), s32(r[rt]))
        elif funct == 0x1B:
            a, b = r[rs], r[rt]
            self.lo, self.hi = (MASK, a) if b == 0 else (a // b, a % b)
        elif funct == 0x20:
            r[rd] = self.checked(s32(r[rs]) + s32(r[rt]), pc)
        elif funct == 0x21:
            r[rd] = (r[rs] + r[rt]) & MASK
        elif funct == 0x22:
            r[rd] = self.checked(s32(r[rs]) - s32(r[rt]), pc)
        elif funct == 0x23:
            r[rd] = (r[rs] - r[rt]) & MASK
        elif funct == 0x24:
            r[rd] = r[rs] & r[rt]
        elif funct == 0x25:
            r[rd] = r[rs] | r[rt]
        elif funct == 0x26:
            r[rd] = r[rs] ^ r[rt]
        elif funct == 0x27:
            r[rd] = ~(r[rs] | r[rt]) & MASK
        elif funct == 0x2A:
            r[rd] = int(s32(r[rs]) < s32(r[rt]))
        elif funct == 0x2B:
            r[rd] = int(r[rs] < r[rt])
        else:
            raise Fault(f"unhandled funct {funct:#x} at {pc:#x}")
        return None

    def gte_write(self, register: int, value: int) -> None:
        """mtc2/lwc2: VZ0 and IR keep a signed halfword; LZCR is read-only."""
        if register == LZCS:
            self.lzcs = value
        elif register in (VZ0, IR0, *IR):
            self.gte[register] = s16(value)
        elif register in (VXY0, MAC0, *MAC, *SXY):
            self.gte[register] = value & MASK

    def gte_read(self, register: int) -> int:
        if register == LZCS:
            return self.lzcs
        if register == LZCR:
            return leading_sign_bits(self.lzcs)
        if register in SZ:
            return self.gte[register] & 0xFFFF
        return self.gte[register] & MASK

    def gte_write_flag(self, value: int) -> None:
        """ctc2 FLAG: bits 30-12 are writable; bit 31 summarizes bits 30-23 and 18-13."""
        value &= 0x7FFFF000
        self.gte_flag = value | (0x80000000 if value & 0x7F87E000 else 0)

    def gte_nclip(self) -> None:
        """NCLIP: signed screen-coordinate winding in MAC0, with final signed32 overflow."""
        # Sony Inline Programming Reference pp. 6-3, 6-42: <4> checks the wide OPZ sum,
        # before its conversion to MAC0. https://psx-spx.consoledev.net/geometrytransformationenginegte/
        sx0, sy0 = s16(self.gte[12]), s16(self.gte[12] >> 16)
        sx1, sy1 = s16(self.gte[13]), s16(self.gte[13] >> 16)
        sx2, sy2 = s16(self.gte[14]), s16(self.gte[14] >> 16)
        value = sx0 * sy1 + sx1 * sy2 + sx2 * sy0 - sx0 * sy2 - sx1 * sy0 - sx2 * sy1
        self.gte[MAC0] = value & MASK
        flag = 1 << 16 if value > 0x7FFFFFFF else 1 << 15 if value < -0x80000000 else 0
        self.gte_write_flag(flag)

    def gte_rotate_v0(self) -> None:
        """SDK ApplyMatrixSV MVMVA: sf=1, rotation matrix, V0, no translation, lm=0."""
        self.gte_write_flag(self.gte_transform_v0((0, 0, 0)))

    def gte_transform_v0(self, translation: Iterable[int]) -> int:
        """Shared sf=1/lm=0 rotation/V0 transform; return its MAC/IR status bits."""
        # Sony Inline Programming Reference 5-24..27, 6-3..7, 6-26.
        # SDK callers: libgte/MTX_06.s, SMP_02.s and SMP_04.s (RotTrans).
        vector = (
            s16(self.gte[VXY0]),
            s16(self.gte[VXY0] >> 16),
            s16(self.gte[VZ0]),
        )
        flag = 0
        for row, (ir, mac, offset) in enumerate(zip(IR, MAC, translation, strict=True)):
            value = s32(offset) << 12
            for column, component in enumerate(vector):
                index = row * 3 + column
                coefficient = s16(self.gte_rotation[index // 2] >> (16 * (index % 2)))
                value += coefficient * component
                if value >= 1 << 43:
                    flag |= 1 << (30 - row)
                elif value < -(1 << 43):
                    flag |= 1 << (27 - row)
                value = ((value + (1 << 43)) & ((1 << 44) - 1)) - (1 << 43)
            value >>= 12
            self.gte[mac] = value & MASK
            self.gte[ir] = max(-0x8000, min(0x7FFF, value))
            if not -0x8000 <= value <= 0x7FFF:
                flag |= 1 << (24 - row)
        return flag

    @staticmethod
    def gte_divide(numerator: int, depth: int) -> tuple[int, bool]:
        """RTPS unsigned16 H/SZ3, using the documented integer UNR reciprocal."""
        numerator &= 0xFFFF
        if numerator >= depth * 2:
            return 0x1FFFF, True
        shift = 16 - depth.bit_length()
        denominator = depth << shift
        index = (denominator - 0x7FC0) >> 7
        estimate = max(0, (0x40000 // (index + 0x100) + 1) // 2 - 0x101) + 0x101
        correction = (0x2000080 - denominator * estimate) >> 8
        reciprocal = (0x80 + correction * estimate) >> 8
        return min(0x1FFFF, ((numerator << shift) * reciprocal + 0x8000) >> 16), False

    def gte_rtps(self) -> None:
        """RTPS sf=1/lm=0: signed44 transform, depth/XY FIFOs and depth cue."""
        # Sony Inline Programming Reference 5-24..27, 6-3..7; PSX-SPX documents UNR.
        # SDK caller: src/SLUS_010.40/libgte/SMP_02.s, RotTransPers.
        flag = self.gte_transform_v0(self.gte_translation)

        def limit(value: int, low: int, high: int, bit: int) -> int:
            nonlocal flag
            if not low <= value <= high:
                flag |= 1 << bit
            return max(low, min(high, value))

        def check_mac0(value: int) -> None:
            nonlocal flag
            if value > 0x7FFFFFFF:
                flag |= 1 << 16
            elif value < -0x80000000:
                flag |= 1 << 15

        depth = limit(s32(self.gte[MAC[2]]), 0, 0xFFFF, 18)
        self.gte[16], self.gte[17], self.gte[18], self.gte[19] = (
            self.gte[17],
            self.gte[18],
            self.gte[19],
            depth,
        )
        quotient, overflow = self.gte_divide(self.gte_projection[H], depth)
        if overflow:
            flag |= 1 << 17
        screen = []
        for ir, offset, bit in ((9, OFX, 14), (10, OFY, 13)):
            value = quotient * self.gte[ir] + s32(self.gte_projection[offset])
            check_mac0(value)
            screen.append(limit(value >> 16, -0x400, 0x3FF, bit))
        self.gte[12], self.gte[13], self.gte[14] = (
            self.gte[13],
            self.gte[14],
            (screen[0] & 0xFFFF) | (screen[1] & 0xFFFF) << 16,
        )
        value = quotient * s16(self.gte_projection[DQA]) + s32(self.gte_projection[DQB])
        check_mac0(value)
        self.gte[MAC0] = value & MASK
        self.gte[IR0] = limit(value >> 12, 0, 0x1000, 12)
        self.gte_write_flag(flag)

    def gte_square(self, shift: int, low_limit: int) -> None:
        """SQR: MACn = IRn squared, shifted right 12 with sf; IRn saturates to MACn."""
        self.gte_flag = None
        for ir, mac in zip(IR, MAC, strict=True):
            value = s32(self.gte[ir] * self.gte[ir] >> (12 * shift))
            self.gte[mac] = value & MASK
            self.gte[ir] = max(0 if low_limit else -0x8000, min(0x7FFF, value))

    def gte_interpolate(self, shift: int, low_limit: int) -> None:
        """GPF: MACn = IR0 times IRn, shifted right 12 with sf; IRn saturates to MACn. The colour
        FIFO it also pushes is not modelled."""
        self.gte_flag = None
        for ir, mac in zip(IR, MAC, strict=True):
            value = s32(self.gte[IR0] * self.gte[ir] >> (12 * shift))
            self.gte[mac] = value & MASK
            self.gte[ir] = max(0 if low_limit else -0x8000, min(0x7FFF, value))

    def divide_signed(self, a: int, b: int) -> None:
        if b == 0:  # The R3000 does not trap: quotient -1 or 1, remainder the dividend.
            self.lo, self.hi = (MASK if a >= 0 else 1), a & MASK
        elif a == -0x80000000 and b == -1:
            self.lo, self.hi = 0x80000000, 0
        else:
            quotient = abs(a) // abs(b)
            quotient = -quotient if (a < 0) != (b < 0) else quotient
            self.lo, self.hi = quotient & MASK, (a - quotient * b) & MASK

    @staticmethod
    def checked(value: int, pc: int) -> int:
        if not -0x80000000 <= value <= 0x7FFFFFFF:
            raise Fault(f"overflow trap at {pc:#x}")
        return value & MASK

    def load_partial(self, op: int, address: int, old: int) -> int:
        word = self.load(address & ~3)
        shift = address & 3
        if op == 0x22:  # lwl
            keep = 0x00FFFFFF >> (shift * 8)
            return (old & keep) | ((word << (24 - shift * 8)) & MASK)
        keep = (0xFFFFFF00 << (24 - shift * 8)) & MASK if shift else 0  # lwr
        return (old & keep) | (word >> (shift * 8))

    def store_partial(self, op: int, address: int, value: int) -> None:
        aligned, shift = address & ~3, address & 3
        word = self.load(aligned)
        if op == 0x2A:  # swl
            keep = (0xFFFFFF00 << (shift * 8)) & MASK
            word = (word & keep) | (value >> (24 - shift * 8))
        else:  # swr
            keep = 0x00FFFFFF >> (24 - shift * 8) if shift else 0
            word = (word & keep) | ((value << (shift * 8)) & MASK)
        self.stores.append((aligned, 4))
        self.put(aligned, word)


# ---------------------------------------------------------------------- harness
@dataclass
class Report:
    lines: list[str] = field(default_factory=list)
    mismatches: int = 0
    undetected: list[str] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return not self.mismatches and not self.undetected


Native = Callable[[list[str]], list[list[int]]]


def load_case(name: str) -> ModuleType:
    path = Path(name) if name.endswith(".py") else CASES / f"{name}.py"
    if not path.is_file():
        raise SystemExit(f"no differential case {path}")
    # Cases import this module by name; run as a script it must not load a second copy whose
    # Fault the harness would not catch.
    sys.modules.setdefault("differential", sys.modules[__name__])
    spec = importlib.util.spec_from_file_location(f"differential_{path.stem}", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.PATH = path
    return module


def case_images(case: ModuleType) -> list[Image]:
    return [Image.load(module) for module in getattr(case, "MODULES", ("BATTLE/BATTLE.PRG",))]


def run_case(case: ModuleType, images: list[Image], native: Native, vectors: int | None = None,
             mutants: bool = True) -> Report:
    report = Report()
    image = images[0]
    count = vectors or case.VECTORS
    rng = random.Random(case.SEED)
    cases, lines = [], []
    for number in range(count):
        vector = case.vector(rng, number)
        observation, inputs = case.run(Machine(images), vector)
        cases.append((vector, observation))
        lines.append(" ".join(str(value) for value in inputs))
    outputs = native(lines)
    if len(outputs) != len(cases):
        raise SystemExit(f"the native driver printed {len(outputs)} lines for {len(cases)} vectors")
    results = []
    for number, ((vector, observation), output) in enumerate(zip(cases, outputs, strict=True)):
        problems = case.check(image, vector, observation, output, observation)
        if problems:
            report.mismatches += 1
            if report.mismatches <= 5:
                report.lines.append(f"MISMATCH vector {number}: {'; '.join(problems)}")
        results.append((vector, observation, output))
    code = image.data[case.ENTRY - image.base:case.END - image.base]
    report.lines.append(f"{case.ID} {case.ENTRY:08X}-{case.END - 4:08X} inclusive: {len(code) // 4} "
                        f"original instructions; sha256={image.sha256(case.ENTRY, case.END)}")
    describe = getattr(case, "describe", None)
    report.lines.extend(describe(image, results) if describe else [])
    report.lines.append(f"{count} vectors; mismatches={report.mismatches}")
    if mutants:
        if not getattr(case, "MUTANTS", None):
            report.undetected.append("(the case defines no mutants)")
        sample = results[:getattr(case, "MUTANT_VECTORS", None) or len(results)]
        report.lines.append(f"Mutants (original words patched, {len(sample)} vectors each): " + ", ".join(
            f"{name} {hits} differing" for name, hits in mutant_hits(case, images, sample, report)))
    return report


def mutant_hits(case: ModuleType, images: list[Image], results: list, report: Report):
    for name, patches in getattr(case, "MUTANTS", {}).items():
        hits = 0
        for vector, reference, output in results:
            machine = Machine(images, patches)
            try:
                observation, _ = case.run(machine, vector)
            except Fault:
                hits += 1
                continue
            hits += bool(case.check(images[0], vector, observation, output, reference))
        if not hits:
            report.undetected.append(name)
        yield name, hits


def vcvars() -> Path:
    if VSWHERE.is_file():
        found = subprocess.run([str(VSWHERE), "-latest", "-property", "installationPath"],
                               capture_output=True, text=True, check=False).stdout.strip()
        candidate = Path(found) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
        if found and candidate.is_file():
            return candidate
    return VCVARS


def variant_sources(case: ModuleType, output: Path,
                    replacements: list[tuple[str, str, str]]) -> tuple[list[Path], list[Path]]:
    """Copy each replaced source into output with its one exact replacement; return the sources and
    the original directories of the copies (for their quoted includes)."""
    sources = [ROOT / path for path in case.SOURCES]
    directories: list[Path] = []
    for target, old, new in replacements:
        matches = [index for index, path in enumerate(sources)
                   if path.name.lower() == Path(target).name.lower()
                   or path.resolve() == (ROOT / target).resolve()]
        if len(matches) != 1:
            raise SystemExit(f"--replace {target}: matches {len(matches)} of the case's SOURCES")
        original = ROOT / case.SOURCES[matches[0]]
        text = sources[matches[0]].read_text(encoding="utf-8")
        if text.count(old) != 1:
            raise SystemExit(f"--replace {target}: the old text occurs {text.count(old)} times, not once")
        copy = output / original.name
        copy.write_text(text.replace(old, new), encoding="utf-8")
        sources[matches[0]] = copy
        directories.append(original.parent)
    return sources, directories


def build_driver(case: ModuleType, replacements: list[tuple[str, str, str]] | None = None) -> Path:
    output = OUTPUT / (case.PATH.stem + ("-variant" if replacements else ""))
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "driver.exe"
    sources, directories = variant_sources(case, output, replacements or [])
    includes = [ROOT / "Native" / "Game" / "include", ROOT / "Native" / "Engine" / "include",
                *(ROOT / path for path in getattr(case, "INCLUDES", ())), *directories]
    sources = [case.PATH.with_name(case.DRIVER), *sources]
    compiler = getattr(case, "COMPILER", "cl")
    standard = getattr(case, "CXX_STANDARD", "c++20")
    options = (f"/std:{standard}", "/EHsc", "/O2", "/DVS_DIFFERENTIAL",
               *getattr(case, "COMPILER_OPTIONS", ()))
    command = (f'call "{vcvars()}" >nul && {compiler} /nologo ' + " ".join(options) + " "
               + " ".join(f'/I "{path}"' for path in includes) + " "
               + " ".join(f'"{path}"' for path in sources)
               + f' /Fe:"{executable}" /Fo:"{output}\\\\"')
    # A string, not a list: cmd.exe does not understand the \" escapes a list would add.
    completed = subprocess.run(f'"{os.environ.get("COMSPEC", "cmd.exe")}" /d /s /c "{command}"',
                               capture_output=True, text=True, check=False)
    if completed.returncode:
        raise SystemExit(f"driver build failed:\n{completed.stdout}{completed.stderr}")
    return executable


def driver_runner(executable: Path, module: Image) -> Native:
    def run(lines: list[str]) -> list[list[int]]:
        completed = subprocess.run([str(executable), str(module.path)], input="\n".join(lines) + "\n",
                                   capture_output=True, text=True, check=False)
        if completed.returncode:
            raise SystemExit(f"{executable.name} exited {completed.returncode}: {completed.stderr[-2000:]}")
        return [[int(value) for value in line.split()] for line in completed.stdout.splitlines()]

    return run


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("case", nargs="?", help="case name under tools\\port\\differentials or a path")
    parser.add_argument("--list", action="store_true", help="list the available cases")
    parser.add_argument("--vectors", type=int, help="override the case's vector count")
    parser.add_argument("--no-mutants", action="store_true", help="skip the mutant runs")
    parser.add_argument("--no-build", action="store_true", help="reuse the previously built driver")
    parser.add_argument("--log", type=Path, help="report path (default build\\differential\\<case>.log)")
    parser.add_argument("--replace", nargs=3, action="append", metavar=("SOURCE", "OLD", "NEW"),
                        help="native broken variant: build from a copy of SOURCE with OLD replaced "
                             "by NEW (once each; repeatable) and expect mismatches")
    args = parser.parse_args(argv)
    if args.list or not args.case:
        for path in sorted(CASES.glob("*.py")):
            print(path.stem, "-", (load_case(str(path)).__doc__ or "").strip().splitlines()[0])
        return 0
    case = load_case(args.case)
    images = case_images(case)
    if args.replace:
        executable = build_driver(case, [tuple(item) for item in args.replace])
        report = run_case(case, images, driver_runner(executable, images[0]), args.vectors, False)
        detected = report.mismatches > 0
        report.lines.append("VARIANT DETECTED" if detected else "VARIANT UNDETECTED")
        text = "\n".join(report.lines) + "\n"
        log = args.log or OUTPUT / f"{case.PATH.stem}-variant.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(text, encoding="utf-8")
        sys.stdout.write(text)
        print(f"report: {log}")
        return 0 if detected else 1
    executable = OUTPUT / case.PATH.stem / "driver.exe"
    if not args.no_build or not executable.is_file():
        executable = build_driver(case)
    report = run_case(case, images, driver_runner(executable, images[0]), args.vectors,
                      not args.no_mutants)
    if report.undetected:
        report.lines.append("UNDETECTED mutants: " + ", ".join(report.undetected))
    report.lines.append("PASS" if report.passed else "FAIL")
    text = "\n".join(report.lines) + "\n"
    log = args.log or OUTPUT / f"{case.PATH.stem}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(text, encoding="utf-8")
    sys.stdout.write(text)
    print(f"report: {log}")
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
