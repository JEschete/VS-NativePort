"""Compare a function's original MIPS instructions with its owned native code (#340).

  python tools\\port\\differential.py <case> [--vectors N] [--no-mutants] [--no-build] [--log PATH]
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
  VECTORS, SEED         default vector count and random seed
  MUTANTS               {name: {address: replacement word}}; every mutant must be detected
  vector(rng, number)   -> one input
  run(machine, vector)  -> (observation, driver inputs); stubs calls, then executes ENTRY
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
so an unexpected callee stops the run instead of executing silently. COP instructions other than
the GTE's leading-zero counter (mtc2 LZCS, mfc2 LZCS/LZCR), syscalls, unaligned accesses and
overflow traps raise.
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
MODULE_BASES = {"BATTLE/BATTLE.PRG": 0x80068800}
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


# The GTE's leading-zero counter, data registers 30 and 31: the square root and normalisation
# helpers use it as a plain integer instruction.
LZCS, LZCR = 30, 31


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
        self.r[R["sp"]] = stack
        self.stubs: dict[int, Callable[[Machine], None]] = {}
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

    def stub(self, address: int, handler: Callable[[Machine], None] | None = None) -> None:
        """Replace a callee; without a handler the call is a no-op returning $v0 unchanged."""
        self.stubs[address] = handler or (lambda machine: None)

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
    def execute(self, entry: int, limit: int = 1_000_000) -> None:
        """Run from entry until it returns to the caller."""
        self.r[R["ra"]] = STOP
        pc = entry
        while pc != STOP:
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
            pc = (pc + 8 if self.call(target) else target) if kind == "call" else target

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
        elif op == 0x12 and rs in (0, 4) and rd in (LZCS, LZCR):
            if rs == 4:  # mtc2; LZCR is read-only
                self.lzcs = r[rt] if rd == LZCS else self.lzcs
            else:
                r[rt] = self.lzcs if rd == LZCS else leading_sign_bits(self.lzcs)
        elif op == 0x12:
            raise Fault(f"unhandled GTE instruction {word:#010x} at {pc:#x} (only LZCS/LZCR are modelled)")
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
        report.lines.append("Mutants (original words patched): " + ", ".join(
            f"{name} {hits} differing" for name, hits in mutant_hits(case, images, results, report)))
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


def build_driver(case: ModuleType) -> Path:
    output = OUTPUT / case.PATH.stem
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "driver.exe"
    includes = [ROOT / "Native" / "Game" / "include", ROOT / "Native" / "Engine" / "include",
                *(ROOT / path for path in getattr(case, "INCLUDES", ()))]
    sources = [case.PATH.with_name(case.DRIVER), *(ROOT / path for path in case.SOURCES)]
    command = (f'call "{vcvars()}" >nul && cl /nologo /std:c++20 /EHsc /O2 /DVS_DIFFERENTIAL '
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
    args = parser.parse_args(argv)
    if args.list or not args.case:
        for path in sorted(CASES.glob("*.py")):
            print(path.stem, "-", (load_case(str(path)).__doc__ or "").strip().splitlines()[0])
        return 0
    case = load_case(args.case)
    images = case_images(case)
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
