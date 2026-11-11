"""Interpreter and harness checks for tools\\port\\differential.py (#340); no disc data needed."""

import random
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

import differential
from differential import MASK, Fault, Image, Machine

BASE = 0x80010000
DATA = 0x80300000
ZERO, V0, V1, A0, A1, A2, A3, T0, T1, SP, RA = 0, 2, 3, 4, 5, 6, 7, 8, 9, 29, 31
NOP = 0


def r_type(funct: int, rd: int = 0, rs: int = 0, rt: int = 0, shift: int = 0) -> int:
    return rs << 21 | rt << 16 | rd << 11 | shift << 6 | funct


def i_type(op: int, rt: int, rs: int, imm: int) -> int:
    return op << 26 | rs << 21 | rt << 16 | (imm & 0xFFFF)


def jal(target: int) -> int:
    return 3 << 26 | (target >> 2 & 0x3FFFFFF)


def addiu(rt: int, rs: int, imm: int) -> int:
    return i_type(9, rt, rs, imm)


JR_RA = r_type(8, rs=RA)


def image(*words: int, base: int = BASE) -> Image:
    return Image(b"".join(word.to_bytes(4, "little") for word in words), base)


def run(*words: int, setup=None, **registers: int) -> Machine:
    machine = Machine(image(*words))
    machine.map_zero(DATA, 0x100)
    for name, value in registers.items():
        machine.r[differential.R[name]] = value & MASK
    if setup:
        setup(machine)
    machine.execute(BASE)
    return machine


class Arithmetic(unittest.TestCase):
    def test_immediates_compares_shifts_and_logic(self) -> None:
        m = run(i_type(0xF, T0, 0, 0x1234), i_type(0xD, T0, T0, 0x5678), addiu(T1, ZERO, -1),
                r_type(0x2B, V0, T0, T1), r_type(0x2A, V1, T0, T1), r_type(3, A2, rt=T1, shift=4),
                r_type(0x27, A3, T0, ZERO), r_type(0x26, A0, T0, T1), r_type(2, A1, rt=T1, shift=28),
                JR_RA, NOP)
        self.assertEqual(m.r[T0], 0x12345678)
        self.assertEqual(m.r[T1], MASK)
        self.assertEqual(m.r[V0], 1)  # sltu: 0x12345678 < 0xFFFFFFFF
        self.assertEqual(m.r[V1], 0)  # slt: 0x12345678 < -1 is false
        self.assertEqual(m.r[A2], MASK)  # sra keeps the sign
        self.assertEqual(m.r[A3], ~0x12345678 & MASK)
        self.assertEqual(m.r[A0], ~0x12345678 & MASK)
        self.assertEqual(m.r[A1], 0xF)  # srl does not

    def test_zero_register_stays_zero(self) -> None:
        self.assertEqual(run(addiu(ZERO, ZERO, 5), JR_RA, NOP).r[ZERO], 0)

    def test_pack_fields_truncates_each_field_to_its_size(self) -> None:
        fields = (("byte", 0, 1, False), ("half", 2, 2, True), ("word", 4, 4))
        packed = differential.pack_fields(bytes(range(10)), fields,
                                          {"byte": 0x1FF, "half": -2, "word": 0x12345678})
        self.assertEqual(packed, bytearray([0xFF, 1, 0xFE, 0xFF, 0x78, 0x56, 0x34, 0x12, 8, 9]))

    def test_load_fields_reads_named_fields_with_their_sign(self) -> None:
        machine = Machine(image(NOP))
        machine.store(DATA, bytes([0xFF, 1, 0xFE, 0xFF, 0x78, 0x56, 0x34, 0x12]))
        fields = (("byte", 0, 1, False), ("half", 2, 2, True), ("word", 4, 4, False))
        self.assertEqual(machine.load_fields(DATA, fields, ("word", "half", "byte")), [0x12345678, -2, 0xFF])

    def test_multiply_and_divide(self) -> None:
        m = run(r_type(0x18, rs=A0, rt=A1), r_type(0x12, V0), r_type(0x10, V1), r_type(0x19, rs=A2, rt=A3),
                r_type(0x12, T0), r_type(0x10, T1), JR_RA, NOP, a0=-2, a1=3, a2=MASK, a3=2)
        self.assertEqual((m.r[V0], m.r[V1]), (-6 & MASK, MASK))
        self.assertEqual((m.r[T0], m.r[T1]), (0xFFFFFFFE, 1))
        m = run(r_type(0x1A, rs=A0, rt=A1), r_type(0x12, V0), r_type(0x10, V1), JR_RA, NOP, a0=-7, a1=2)
        self.assertEqual((m.r[V0], m.r[V1]), (-3 & MASK, -1 & MASK))  # truncates toward zero

    def test_division_edge_cases_do_not_trap(self) -> None:
        machine = Machine(image(NOP))
        machine.divide_signed(5, 0)
        self.assertEqual((machine.lo, machine.hi), (MASK, 5))
        machine.divide_signed(-5, 0)
        self.assertEqual((machine.lo, machine.hi), (1, -5 & MASK))
        machine.divide_signed(-0x80000000, -1)
        self.assertEqual((machine.lo, machine.hi), (0x80000000, 0))
        m = run(r_type(0x1B, rs=A0, rt=ZERO), r_type(0x12, V0), r_type(0x10, V1), JR_RA, NOP, a0=9)
        self.assertEqual((m.r[V0], m.r[V1]), (MASK, 9))

    def test_gte_leading_zero_counter(self) -> None:
        def cop2(move: int, gpr: int, register: int) -> int:
            return 0x12 << 26 | move << 21 | gpr << 16 | register << 11

        program = (cop2(4, A0, 30), NOP, NOP, cop2(0, V0, 31), cop2(0, V1, 30), cop2(4, A1, 31), JR_RA,
                   NOP)
        for value, count in ((0, 32), (1, 31), (0x00010000, 15), (0x7FFFFFFF, 1), (MASK, 32),
                             (0x80000000, 1), (0xFFFF0000, 16)):
            m = run(*program, a0=value, a1=0x1234)
            self.assertEqual((m.r[V0], m.r[V1]), (count, value), hex(value))  # LZCR stays read-only
        self.assertEqual(differential.leading_sign_bits(0xFFFFFFFE), 31)

    def test_gte_square_of_the_ir_registers(self) -> None:
        def cop2(move: int, gpr: int, register: int) -> int:
            return 0x12 << 26 | move << 21 | gpr << 16 | register << 11

        def sqr(shift: int, low_limit: int) -> int:
            return 0x4A << 24 | shift << 19 | low_limit << 10 | differential.GTE_SQR

        def program(command: int) -> tuple[int, ...]:
            return (cop2(4, A0, 9), cop2(4, A1, 10), cop2(4, A2, 11), NOP, NOP, command, cop2(0, V0, 25),
                    cop2(0, V1, 26), cop2(0, T0, 27), cop2(0, T1, 9), JR_RA, NOP)

        # IR1-IR3 keep the low halfword, signed: 0x12345 enters as 0x2345 and 0xFFFF as -1.
        m = run(*program(sqr(0, 1)), a0=-3, a1=0x12345, a2=0x8000)
        self.assertEqual((m.r[V0], m.r[V1], m.r[T0]), (9, 0x2345 * 0x2345, 0x40000000))
        self.assertEqual(m.r[T1], 9)
        m = run(*program(sqr(1, 0)), a0=0x2000, a1=0xFFFF, a2=0x7FFF)
        self.assertEqual((m.r[V0], m.r[V1], m.r[T0]), (0x4000, 0, 0x7FFF * 0x7FFF >> 12))
        self.assertEqual(m.r[T1], 0x4000)
        m = run(*program(sqr(0, 0)), a0=0x200, a1=0, a2=0)
        self.assertEqual((m.r[V0], m.r[T1]), (0x40000, 0x7FFF))  # IR saturates; MAC keeps the square
        with self.assertRaisesRegex(Fault, "GTE"):
            run(0x4A000012, JR_RA, NOP)  # MVMVA is not modelled

    def test_signed_overflow_traps(self) -> None:
        with self.assertRaisesRegex(Fault, "overflow"):
            run(r_type(0x20, V0, A0, A1), JR_RA, NOP, a0=0x7FFFFFFF, a1=1)
        with self.assertRaisesRegex(Fault, "overflow"):
            run(i_type(8, V0, A0, 1), JR_RA, NOP, a0=0x7FFFFFFF)
        with self.assertRaisesRegex(Fault, "overflow"):
            run(r_type(0x22, V0, A0, A1), JR_RA, NOP, a0=-0x80000000, a1=1)
        self.assertEqual(run(r_type(0x21, V0, A0, A1), JR_RA, NOP, a0=0x7FFFFFFF, a1=1).r[V0], 0x80000000)


class Memory(unittest.TestCase):
    def test_signed_and_unsigned_loads_and_stores(self) -> None:
        def setup(m: Machine) -> None:
            m.store(DATA, bytes([0x80, 0xFF, 0x34, 0x12]))

        m = run(i_type(0x20, V0, A0, 0), i_type(0x24, V1, A0, 0), i_type(0x21, T0, A0, 0),
                i_type(0x25, T1, A0, 0), i_type(0x23, A2, A0, 0), i_type(0x28, A2, A0, 8),
                i_type(0x29, A2, A0, 12), i_type(0x2B, A2, A0, 16), JR_RA, NOP, setup=setup, a0=DATA)
        self.assertEqual(m.r[V0], 0xFFFFFF80)
        self.assertEqual(m.r[V1], 0x80)
        self.assertEqual(m.r[T0], 0xFFFFFF80)
        self.assertEqual(m.r[T1], 0xFF80)
        self.assertEqual(m.r[A2], 0x1234FF80)
        self.assertEqual(m.bytes(DATA + 8, 12), bytes([0x80, 0, 0, 0, 0x80, 0xFF, 0, 0, 0x80, 0xFF, 0x34, 0x12]))
        self.assertEqual(m.stores, [(DATA + 8, 1), (DATA + 12, 2), (DATA + 16, 4)])

    def test_unaligned_word_through_lwl_lwr_and_swl_swr(self) -> None:
        source = bytes(range(0x10, 0x20))
        for offset in range(4):
            with self.subTest(offset=offset):
                def setup(m: Machine) -> None:
                    m.store(DATA, source)

                m = run(i_type(0x22, V0, A0, 3), i_type(0x26, V0, A0, 0),
                        i_type(0x2A, V0, A1, 3), i_type(0x2E, V0, A1, 0), JR_RA, NOP,
                        setup=setup, a0=DATA + offset, a1=DATA + 0x40 + offset)
                self.assertEqual(m.r[V0], int.from_bytes(source[offset:offset + 4], "little"))
                self.assertEqual(m.bytes(DATA + 0x40 + offset, 4), source[offset:offset + 4])
                self.assertEqual(m.bytes(DATA + 0x40, offset), bytes(offset))  # neighbours untouched

    def test_faults_on_unaligned_unmapped_and_runaway(self) -> None:
        with self.assertRaisesRegex(Fault, "unaligned 4-byte load"):
            run(i_type(0x23, V0, A0, 2), JR_RA, NOP, a0=DATA)
        with self.assertRaisesRegex(Fault, "unaligned 2-byte store"):
            run(i_type(0x29, V0, A0, 1), JR_RA, NOP, a0=DATA)
        with self.assertRaisesRegex(Fault, "unmapped read"):
            run(i_type(0x23, V0, A0, 0), JR_RA, NOP, a0=0x80700000)
        with self.assertRaisesRegex(Fault, "unhandled GTE instruction"):
            run(0x4A000000, JR_RA, NOP)  # a GTE command
        with self.assertRaisesRegex(Fault, "unhandled GTE instruction"):
            run(0x48820000, JR_RA, NOP)  # mtc2 to data register 0
        with self.assertRaisesRegex(Fault, "unhandled opcode"):
            run(0xC8000000, JR_RA, NOP)  # lwc2
        machine = Machine(image(i_type(4, ZERO, ZERO, -1), NOP))  # beq zero, zero, self
        with self.assertRaisesRegex(Fault, "runaway"):
            machine.execute(BASE, limit=100)


class Control(unittest.TestCase):
    def test_branch_delay_slots_execute_on_both_paths(self) -> None:
        program = (i_type(1, 0, A0, 3), addiu(V0, ZERO, 1), addiu(V0, V0, 10), NOP, JR_RA, NOP)  # bltz
        self.assertEqual(run(*program, a0=-1).r[V0], 1)
        self.assertEqual(run(*program, a0=0).r[V0], 11)
        program = (i_type(1, 1, A0, 3), addiu(V0, ZERO, 1), addiu(V0, V0, 10), NOP, JR_RA, NOP)  # bgez
        self.assertEqual(run(*program, a0=0).r[V0], 1)
        self.assertEqual(run(*program, a0=-1).r[V0], 11)
        program = (i_type(5, A1, A0, 3), addiu(V0, ZERO, 1), addiu(V0, V0, 10), NOP, JR_RA, NOP)  # bne
        self.assertEqual(run(*program, a0=1, a1=2).r[V0], 1)
        self.assertEqual(run(*program, a0=2, a1=2).r[V0], 11)

    def test_stubbed_call_sees_the_delay_slot_and_returns(self) -> None:
        seen = []

        def setup(m: Machine) -> None:
            m.stub(0x80020000, lambda machine: (seen.append(machine.r[A0]), machine.r.__setitem__(V0, 42)))

        m = run(addiu(SP, SP, -8), i_type(0x2B, RA, SP, 0), jal(0x80020000), addiu(A0, ZERO, 7),
                addiu(V0, V0, 1), i_type(0x23, RA, SP, 0), addiu(SP, SP, 8), JR_RA, NOP, setup=setup)
        self.assertEqual(seen, [7])
        self.assertEqual(m.r[V0], 43)
        self.assertEqual(m.r[SP], differential.DEFAULT_STACK)

    def test_unstubbed_call_raises_and_followed_call_runs(self) -> None:
        callee = BASE + 0x40
        words = [addiu(SP, SP, -8), i_type(0x2B, RA, SP, 0), jal(callee), NOP, i_type(0x23, RA, SP, 0),
                 addiu(SP, SP, 8), JR_RA, NOP]
        words += [NOP] * (0x40 // 4 - len(words)) + [addiu(V0, A0, 1), JR_RA, NOP]
        with self.assertRaisesRegex(Fault, "neither stubbed nor followed"):
            run(*words, a0=4)
        m = run(*words, setup=lambda machine: machine.follow.add(callee), a0=4)
        self.assertEqual(m.r[V0], 5)
        self.assertEqual(m.stores, [(differential.DEFAULT_STACK - 8, 4)])

    def test_tail_stub_takes_jumps_and_branches_and_returns_to_ra(self) -> None:
        callee = 0x80020000
        seen = []

        def setup(m: Machine, tail: bool) -> None:
            m.stub(callee, lambda machine: seen.append(machine.r[A0]), tail=tail)

        jump = 2 << 26 | (callee >> 2 & 0x3FFFFFF)
        m = run(jump, addiu(A0, ZERO, 3), setup=lambda machine: setup(machine, True))
        self.assertEqual(seen, [3])
        self.assertEqual(m.r[RA], differential.STOP)
        branch = i_type(1, 1, ZERO, (callee - BASE - 4) >> 2)  # bgez $zero
        run(branch, addiu(A0, ZERO, 4), setup=lambda machine: setup(machine, True))
        self.assertEqual(seen, [3, 4])
        with self.assertRaises(Fault):
            run(jump, NOP, setup=lambda machine: setup(machine, False))
        self.assertEqual(seen, [3, 4])

    def test_jump_in_a_delay_slot_is_rejected(self) -> None:
        with self.assertRaisesRegex(Fault, "delay slot"):
            run(JR_RA, JR_RA)

    def test_patches_replace_instruction_words(self) -> None:
        words = (r_type(0x21, V0, A0, A1), JR_RA, NOP)
        machine = Machine(image(*words), {BASE: r_type(0x23, V0, A0, A1)})
        machine.r[A0], machine.r[A1] = 5, 3
        machine.execute(BASE)
        self.assertEqual(machine.r[V0], 2)


class Images(unittest.TestCase):
    def test_psx_executable_header_and_known_module_bases(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            header = bytearray(0x800)
            header[:8] = b"PS-X EXE"
            header[0x18:0x1C] = (0x80010000).to_bytes(4, "little")
            (root / "SLUS_010.40").write_bytes(bytes(header) + b"\x01\x02\x03\x04")
            (root / "BATTLE").mkdir()
            (root / "BATTLE" / "BATTLE.PRG").write_bytes(b"\xAA\xBB\xCC\xDD")
            (root / "OTHER.PRG").write_bytes(b"\x00")
            with mock.patch.object(differential, "DISC", root):
                executable = Image.load("SLUS_010.40")
                self.assertEqual((executable.base, executable.read(0x80010000)), (0x80010000, 0x04030201))
                module = Image.load("BATTLE/BATTLE.PRG")
                self.assertEqual((module.base, module.read(0x80068800, 2, signed=True)), (0x80068800, -0x4456))
                with self.assertRaisesRegex(SystemExit, "no load address"):
                    Image.load("OTHER.PRG")
        with self.assertRaisesRegex(Fault, "outside the module"):
            image(NOP).read(BASE + 2)


class CaseHelpers(unittest.TestCase):
    def test_field_writer_truncates_and_checks_bounds(self) -> None:
        block = bytearray(8)
        put = differential.field_writer(block, BASE)
        put(BASE + 1, -2, 2)
        put(BASE + 4, 0x1_2345_6789, 4)
        self.assertEqual(bytes(block), bytes([0, 0xFE, 0xFF, 0, 0x89, 0x67, 0x45, 0x23]))
        with self.assertRaisesRegex(ValueError, "outside the block"):
            put(BASE + 6, 0, 4)
        with self.assertRaisesRegex(ValueError, "outside the block"):
            put(BASE - 1, 0, 1)

    def test_output_problems_compare_values_calls_and_stores(self) -> None:
        names = ["a", "b"]
        observation = {"values": [1, 0xFFFFFFFF], "calls": [[4, 1, 2]], "stores": [(BASE, 4)]}
        allowed = [(BASE, BASE + 4)]
        self.assertEqual(differential.output_problems(names, [0, 1, 0xFFFFFFFF, 1, 4, 1, 2], observation, allowed), [])
        self.assertEqual(differential.output_problems(names, [1], observation, allowed), ["native refused"])
        problems = differential.output_problems(names, [0, 2, 0xFFFFFFFF, 0], observation, [])
        self.assertEqual(problems, ["a native 0x2 original 0x1", "calls differ from 0: native [] original [[4, 1, 2]]",
                                    f"original stored 4 bytes at {BASE:#x}"])


def fake_case(**overrides) -> types.ModuleType:
    """Sum two registers with addu; the native side is a Python function."""
    case = types.ModuleType("fake_case")
    case.ID, case.ENTRY, case.END, case.VECTORS, case.SEED = "F00000", BASE, BASE + 12, 64, 7
    case.MUTANTS = {"subtract": {BASE: r_type(0x23, V0, A0, A1)}, "gte": {BASE: 0x4A000000}}

    def vector(rng: random.Random, number: int) -> tuple[int, int]:
        return (number, 0) if number < 2 else (rng.getrandbits(32), rng.getrandbits(32))

    def run_vector(machine: Machine, vector: tuple[int, int]) -> tuple[int, list[int]]:
        machine.r[A0], machine.r[A1] = vector
        machine.execute(BASE)
        return machine.r[V0], list(vector)

    def check(image_, vector, observation, native, reference) -> list[str]:
        return [] if observation == native[0] else [f"{observation} != {native[0]}"]

    case.vector, case.run, case.check = vector, run_vector, check
    case.describe = lambda image_, results: [f"{len(results)} results"]
    for name, value in overrides.items():
        setattr(case, name, value)
    return case


def native_sum(lines: list[str]) -> list[list[int]]:
    return [[sum(int(value) for value in line.split()) & MASK] for line in lines]


class Harness(unittest.TestCase):
    images = [image(r_type(0x21, V0, A0, A1), JR_RA, NOP)]

    def test_matching_native_side_passes_and_detects_mutants(self) -> None:
        report = differential.run_case(fake_case(), self.images, native_sum)
        self.assertTrue(report.passed, report.lines)
        text = "\n".join(report.lines)
        self.assertIn("3 original instructions; sha256=", text)
        self.assertIn("64 results", text)
        self.assertIn("64 vectors; mismatches=0", text)
        self.assertIn("gte 64 differing", text)  # a faulting mutant counts as detected
        self.assertIn("Mutants (original words patched, 64 vectors each)", text)

    def test_mutant_vectors_limits_the_mutant_reruns(self) -> None:
        report = differential.run_case(fake_case(MUTANT_VECTORS=5), self.images, native_sum, vectors=20)
        self.assertTrue(report.passed, report.lines)
        text = "\n".join(report.lines)
        self.assertIn("20 vectors; mismatches=0", text)
        self.assertIn("Mutants (original words patched, 5 vectors each)", text)
        self.assertIn("gte 5 differing", text)

    def test_native_mismatches_are_counted(self) -> None:
        def wrong_for_odd(lines: list[str]) -> list[list[int]]:
            return [[value[0] + (number % 2)] for number, value in enumerate(native_sum(lines))]

        report = differential.run_case(fake_case(), self.images, wrong_for_odd, vectors=10, mutants=False)
        self.assertEqual(report.mismatches, 5)
        self.assertFalse(report.passed)
        self.assertTrue(report.lines[0].startswith("MISMATCH vector 1:"))
        self.assertFalse(any(line.startswith("Mutants") for line in report.lines))

    def test_undetected_mutant_fails_the_report(self) -> None:
        case = fake_case(MUTANTS={"same-nop": {BASE + 8: r_type(0, shift=1)}})  # sll zero, zero, 1
        report = differential.run_case(case, self.images, native_sum, vectors=8)
        self.assertEqual(report.undetected, ["same-nop"])
        self.assertFalse(report.passed)

    def test_a_case_without_mutants_fails(self) -> None:
        report = differential.run_case(fake_case(MUTANTS={}), self.images, native_sum, vectors=8)
        self.assertEqual(report.undetected, ["(the case defines no mutants)"])
        self.assertFalse(report.passed)
        self.assertTrue(differential.run_case(fake_case(MUTANTS={}), self.images, native_sum, vectors=8,
                                              mutants=False).passed)

    def test_line_count_mismatch_stops(self) -> None:
        with self.assertRaisesRegex(SystemExit, "printed 0 lines for 4 vectors"):
            differential.run_case(fake_case(), self.images, lambda lines: [], vectors=4)

    def test_driver_runner_parses_integer_lines(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            script = Path(folder) / "driver.py"
            script.write_text("import sys\nfor line in sys.stdin:\n    print(sum(map(int, line.split())), -1)\n")
            runner = differential.driver_runner(Path(sys.executable), Image(b"", 0, script))
            self.assertEqual(runner(["1 2", "-5 3"]), [[3, -1], [-2, -1]])
            script.write_text("import sys\nsys.exit(3)\n")
            with self.assertRaisesRegex(SystemExit, "exited 3"):
                runner(["1"])

    def test_cases_load_and_list(self) -> None:
        case = differential.load_case("actor_settings")
        self.assertEqual((case.ID, case.ENTRY), ("F01813", 0x800E5A9C))
        self.assertTrue(case.PATH.with_name(case.DRIVER).is_file())
        root = differential.ROOT
        self.assertTrue(all((root / source).is_file() for source in case.SOURCES))
        with mock.patch("sys.stdout") as output:
            self.assertEqual(differential.main(["--list"]), 0)
        self.assertIn("actor_settings", "".join(call.args[0] for call in output.write.call_args_list))


if __name__ == "__main__":
    unittest.main()
