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
ZERO, V0, V1, A0, A1, A2, A3, T0, T1, T2, SP, RA = 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 29, 31
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

    def test_gte_interpolation_scales_the_ir_registers_by_ir0(self) -> None:
        def cop2(move: int, gpr: int, register: int) -> int:
            return 0x12 << 26 | move << 21 | gpr << 16 | register << 11

        def gpf(shift: int, low_limit: int) -> int:
            return 0x4B9 << 20 | shift << 19 | low_limit << 10 | differential.GTE_GPF

        def program(command: int) -> tuple[int, ...]:
            return (cop2(4, A3, 8), cop2(4, A0, 9), cop2(4, A1, 10), cop2(4, A2, 11), NOP, NOP, command,
                    cop2(0, V0, 25), cop2(0, V1, 26), cop2(0, T0, 27), cop2(0, T1, 9), cop2(0, T2, 11), JR_RA,
                    NOP)

        # The game's func_80041AF4 issues 0x4B90003D (sf 0, lm 0).
        self.assertEqual(gpf(0, 0), 0x4B90003D)
        m = run(*program(gpf(0, 0)), a0=-3, a1=0x12345, a2=0x7FFF, a3=0x10)
        self.assertEqual((m.r[V0], m.r[V1], m.r[T0]), (-0x30 & MASK, 0x23450, 0x7FFF0))
        self.assertEqual((m.r[T1], m.r[T2]), (-0x30 & MASK, 0x7FFF))  # IR saturates; MAC keeps it
        m = run(*program(gpf(1, 1)), a0=-0x2000, a1=0x2000, a2=0x7FFF, a3=0x1000)
        self.assertEqual((m.r[V0], m.r[V1], m.r[T0]), (-0x2000 & MASK, 0x2000, 0x7FFF))
        self.assertEqual(m.r[T1], 0)  # lm clamps IR at zero

    def test_signed_overflow_traps(self) -> None:
        with self.assertRaisesRegex(Fault, "overflow"):
            run(r_type(0x20, V0, A0, A1), JR_RA, NOP, a0=0x7FFFFFFF, a1=1)
        with self.assertRaisesRegex(Fault, "overflow"):
            run(i_type(8, V0, A0, 1), JR_RA, NOP, a0=0x7FFFFFFF)
        with self.assertRaisesRegex(Fault, "overflow"):
            run(r_type(0x22, V0, A0, A1), JR_RA, NOP, a0=-0x80000000, a1=1)
        self.assertEqual(run(r_type(0x21, V0, A0, A1), JR_RA, NOP, a0=0x7FFFFFFF, a1=1).r[V0], 0x80000000)


class NormalClipping(unittest.TestCase):
    @staticmethod
    def cop2(move: int, gpr: int, register: int) -> int:
        return 0x12 << 26 | move << 21 | gpr << 16 | register << 11

    def clip(self, points: tuple[int, int, int], command: int = 0x4B400006) -> Machine:
        cop2 = self.cop2
        return run(
            cop2(4, A0, 12),
            cop2(4, A1, 13),
            cop2(4, A2, 14),
            NOP,
            NOP,
            command,
            cop2(0, V0, 24),
            cop2(2, V1, 31),
            NOP,
            JR_RA,
            NOP,
            a0=points[0],
            a1=points[1],
            a2=points[2],
        )

    def test_nclip_winding_signed_coordinates_and_degenerate_results(self) -> None:
        for points, mac0 in (
            ((0, 0x00000006, 0x00040000), 24),  # Clockwise with screen Y pointing down.
            ((0, 0x00040000, 0x00000006), 0xFFFFFFE8),
            ((0x00010001, 0x00030003, 0x00050005), 0),
            ((0xFFFEFFFF, 0xFFFE0002, 0x0002FFFF), 12),
        ):
            with self.subTest(points=points):
                m = self.clip(points)
                self.assertEqual((m.r[V0], m.r[V1]), (mac0, 0))

    def test_nclip_wraps_mac0_and_tests_only_the_complete_wide_sum(self) -> None:
        # Sony Inline Programming Reference 6-42 checks the entire OPZ expression as <4>.
        for points, mac0, flag in (
            ((0x80008000, 0x80007FFF, 0x7FFF8000), 0xFFFE0001, 0x80010000),
            ((0x80008000, 0x7FFF8000, 0x80007FFF), 0x0001FFFF, 0x80008000),
            ((0x80008000, 0x80017FFF, 0x00010000), 0x7FFFFFFF, 0),
            ((0x80008000, 0x80017FFF, 0x0001FFFF), 0x80000000, 0x80010000),
            ((0x80008000, 0x0001FFFF, 0x80017FFF), 0x80000000, 0),
            ((0x80008000, 0x0001FFFE, 0x80017FFF), 0x7FFFFFFF, 0x80008000),
            # The first three products' sum exceeds signed32; the complete sum cancels.
            ((0x80008000, 0x80008001, 0x80018000), 1, 0),
            ((0x7FFF8000, 0x7FFF8001, 0x7FFE8000), 0xFFFFFFFF, 0),
        ):
            with self.subTest(points=points):
                m = self.clip(points)
                self.assertEqual((m.r[V0], m.r[V1]), (mac0, flag))
                self.assertEqual(m.gte[24], mac0)

    def test_nclip_preserves_other_state_and_ignores_non_nclip_command_bits(
        self,
    ) -> None:
        for command in (0x4B400006, 0x4A000006, 0x4B480406, 0x4A140006):
            with self.subTest(command=hex(command)):
                m = Machine(image(command, JR_RA, NOP))
                m.gte.update(
                    {register: 0x12340000 + register for register in range(32)}
                )
                m.gte[12], m.gte[13], m.gte[14] = 0, 6, 0x00040000
                m.lzcs, m.hi, m.lo = 0xFFFF0000, 0x10203040, 0x50607080
                m.gte_flag = 0xFFFFF000
                before = dict(m.gte)
                m.execute(BASE)
                before[24] = 24
                self.assertEqual(m.gte, before)
                self.assertEqual(m.gte_flag, 0)
                self.assertEqual(
                    (m.lzcs, m.hi, m.lo), (0xFFFF0000, 0x10203040, 0x50607080)
                )
                self.assertEqual((m.stores, m.overlay, m.trace), ([], {}, []))

    def test_nclip_flag_control_moves_reset_and_repeat_entry(self) -> None:
        cop2 = self.cop2
        for value, expected in (
            (MASK, 0xFFFFF000),
            (0x80000FFF, 0),
            (0x00401000, 0x00401000),  # IR3 and IR0 do not contribute to bit 31.
            (0x00080000, 0x00080000),  # Neither does the blue colour limiter.
            (0x00010000, 0x80010000),
            (0x00008000, 0x80008000),
        ):
            with self.subTest(value=hex(value)):
                m = run(
                    cop2(6, A0, 31),
                    cop2(2, T0, 31),
                    NOP,
                    cop2(4, A1, 24),
                    cop2(0, T1, 24),
                    NOP,
                    cop2(2, T2, 31),
                    NOP,
                    0x4B400006,
                    cop2(2, V0, 31),
                    NOP,
                    JR_RA,
                    NOP,
                    a0=value,
                    a1=0x80000000,
                )
                self.assertEqual(
                    (m.r[T0], m.r[T1], m.r[T2], m.r[V0]),
                    (expected, 0x80000000, expected, 0),
                )
        m = Machine(image(0x4B400006, JR_RA, NOP))
        m.gte[12], m.gte[13], m.gte[14] = 0x80008000, 0x80007FFF, 0x7FFF8000
        m.execute(BASE)
        self.assertEqual((m.gte_read(24), m.gte_flag), (0xFFFE0001, 0x80010000))
        m.gte[12], m.gte[13], m.gte[14] = 0, 6, 0x00040000
        m.execute(BASE)
        self.assertEqual((m.gte_read(24), m.gte_flag), (24, 0))

    def test_nclip_status_support_does_not_invent_other_gte_status(self) -> None:
        cop2 = self.cop2
        for command in (0x4A000028, 0x4B90003D):
            with self.subTest(command=hex(command)):
                with self.assertRaisesRegex(Fault, "GTE FLAG.*not modelled"):
                    run(0x4B400006, command, cop2(2, V0, 31), NOP, JR_RA, NOP)
                # A subsequent NCLIP defines FLAG again without needing a new Machine.
                self.assertEqual(
                    run(command, 0x4B400006, cop2(2, V0, 31), NOP, JR_RA, NOP).r[V0], 0
                )
        for command in (cop2(2, V0, 30), cop2(6, A0, 30)):
            with self.subTest(command=hex(command)), self.assertRaisesRegex(
                Fault, "unhandled GTE instruction"
            ):
                run(command, JR_RA, NOP)


class SdkMatrix(unittest.TestCase):
    IDENTITY = (4096, 0, 0, 0, 4096, 0, 0, 0, 4096)

    @staticmethod
    def cop2(move: int, gpr: int, register: int) -> int:
        return 0x12 << 26 | move << 21 | gpr << 16 | register << 11

    @classmethod
    def sdk_words(cls, command: int = 0x4A486012) -> tuple[int, ...]:
        # ApplyMatrixSV, SLUS_010.40 libgte/MTX_06.s, 0x80041724..0x8004177B.
        return (
            *(i_type(0x23, T0 + index, A0, 4 * index) for index in range(5)),
            *(cls.cop2(6, T0 + index, index) for index in range(5)),
            i_type(0x32, 0, A1, 0),
            i_type(0x32, 1, A1, 4),
            NOP,
            command,
            *(cls.cop2(0, T0 + index, 9 + index) for index in range(3)),
            *(i_type(0x29, T0 + index, A2, 2 * index) for index in range(3)),
            JR_RA,
            r_type(0x21, V0, A2, ZERO),
        )

    def apply(self, matrix, vector, command: int = 0x4A486012) -> Machine:
        def setup(m: Machine) -> None:
            for index, value in enumerate(matrix):
                m.put(DATA + 2 * index, value, 2)
            m.put(DATA + 18, 0xA55A, 2)
            for index, value in enumerate(vector):
                m.put(DATA + 0x40 + 2 * index, value, 2)
            m.put(DATA + 0x46, 0xDEAD, 2)
            m.put(DATA + 0x86, 0xBEEF, 2)
            m.gte_flag = 0xFFFFF000

        return run(
            *self.sdk_words(command),
            setup=setup,
            a0=DATA,
            a1=DATA + 0x40,
            a2=DATA + 0x80,
        )

    def assert_result(self, m: Machine, mac, ir, flag: int) -> None:
        self.assertEqual(
            tuple(differential.s32(m.gte_read(register)) for register in (25, 26, 27)),
            mac,
        )
        self.assertEqual(
            tuple(differential.s32(m.gte_read(register)) for register in (9, 10, 11)),
            ir,
        )
        self.assertEqual(
            tuple(m.load(DATA + 0x80 + 2 * axis, 2, signed=True) for axis in range(3)),
            ir,
        )
        self.assertEqual(m.gte_flag, flag)

    def test_sdk_sequence_packs_signed_rotation_and_v0(self) -> None:
        for matrix, vector, expected in (
            (self.IDENTITY, (-32768, 32767, -1), (-32768, 32767, -1)),
            ((4096, 0, 0, 0, 0, -4096, 0, 4096, 0), (7, -11, 13), (7, -13, -11)),
            ((0, 0, 4096, 0, 4096, 0, -4096, 0, 0), (7, -11, 13), (13, -11, -7)),
            ((0, -4096, 0, 4096, 0, 0, 0, 0, 4096), (7, -11, 13), (11, 7, 13)),
            ((4096, 0, 0, 0, 4096, 0, 0, 0, -4096), (7, -11, 13), (7, -11, -13)),
            (
                (4096, 2048, -1024, -8192, 4096, 512, 256, -4096, 8192),
                (8, -16, 32),
                (-8, -28, 80),
            ),
        ):
            with self.subTest(matrix=matrix):
                m = self.apply(matrix, vector)
                self.assert_result(m, expected, expected, 0)
                self.assertEqual(m.r[V0], DATA + 0x80)
                self.assertEqual(m.load(DATA + 0x86, 2), 0xBEEF)
                self.assertEqual(
                    m.stores, [(DATA + 0x80, 2), (DATA + 0x82, 2), (DATA + 0x84, 2)]
                )
                self.assertEqual((m.stubs, m.patches), ({}, {}))

    def test_mvmva_accumulates_wide_before_arithmetic_q12_shift(self) -> None:
        m = self.apply(
            (-32768, -32768, -32768, 32767, 32767, 32767, -32768, 32767, 1),
            (-32768, -32768, -32768),
        )
        self.assert_result(m, (786432, -786408, 0), (32767, -32768, 0), 0x81800000)
        for vector, expected in (
            ((-1, 0, 0), (-1, -1, -2)),
            ((1, 0, 0), (0, 0, 1)),
        ):
            with self.subTest(vector=vector):
                m = self.apply((1, 0, 0, 4095, 0, 0, 4097, 0, 0), vector)
                self.assert_result(m, expected, expected, 0)

    def test_mvmva_ir_limits_and_channel_specific_summary_flags(self) -> None:
        for row, saturation_flag in enumerate((0x81000000, 0x80800000, 0x00400000)):
            matrix = [0] * 9
            matrix[3 * row] = matrix[3 * row + 1] = 4096
            for vector, mac_value, ir_value, clipped in (
                ((32767, 0, 0), 32767, 32767, False),
                ((32767, 1, 0), 32768, 32767, True),
                ((-32768, 0, 0), -32768, -32768, False),
                ((-32768, -1, 0), -32769, -32768, True),
            ):
                with self.subTest(row=row, vector=vector):
                    mac, ir = [0] * 3, [0] * 3
                    mac[row], ir[row] = mac_value, ir_value
                    m = self.apply(matrix, vector)
                    self.assert_result(
                        m, tuple(mac), tuple(ir), saturation_flag if clipped else 0
                    )
        m = self.apply((4096, 4096, 0) * 3, (-32768, -1, 0))
        self.assert_result(
            m, (-32769, -32769, -32769), (-32768, -32768, -32768), 0x81C00000
        )

    def test_rotation_and_v0_moves_are_transfers_not_commands(self) -> None:
        cop2 = self.cop2
        for flag in (None, 0xFFFFF000):
            with self.subTest(flag=flag):
                m = Machine(
                    image(
                        cop2(6, A0, 4),
                        cop2(4, A1, 0),
                        cop2(4, A2, 1),
                        cop2(0, V0, 0),
                        cop2(0, V1, 1),
                        NOP,
                        JR_RA,
                        NOP,
                    )
                )
                m.gte_rotation = [11, 22, 33, 44, 55]
                m.gte_flag = flag
                m.r[A0], m.r[A1], m.r[A2] = 0x12348000, 0x80007FFF, 0xBEEF8001
                before = dict(m.gte)
                m.execute(BASE)
                before.update({0: 0x80007FFF, 1: -32767})
                self.assertEqual(m.gte, before)
                self.assertEqual(m.gte_rotation, [11, 22, 33, 44, -32768])
                self.assertEqual((m.r[V0], m.r[V1]), (0x80007FFF, 0xFFFF8001))
                self.assertEqual(m.gte_flag, flag)
                self.assertEqual((m.stores, m.overlay), ([], {}))

    def test_lwc2_v0_signed_offsets_wrap_alignment_and_atomic_faults(self) -> None:
        cop2 = self.cop2

        def setup(m: Machine) -> None:
            m.put(DATA, 0x80007FFF)
            m.put(DATA + 4, 0xCAFE8001)
            m.gte_flag = 0xFFFFF000

        m = run(
            i_type(0x32, 0, A0, -4),
            i_type(0x32, 1, A0, 0),
            cop2(0, V0, 0),
            cop2(0, V1, 1),
            NOP,
            JR_RA,
            NOP,
            setup=setup,
            a0=DATA + 4,
        )
        self.assertEqual((m.r[V0], m.r[V1]), (0x80007FFF, 0xFFFF8001))
        self.assertEqual((m.load(DATA), m.load(DATA + 4)), (0x80007FFF, 0xCAFE8001))
        self.assertEqual((m.gte_flag, m.stores), (0xFFFFF000, []))
        m = run(
            i_type(0x32, 0, A0, 4),
            JR_RA,
            NOP,
            setup=lambda machine: machine.put(0, 0xFEDCBA98),
            a0=0xFFFFFFFC,
        )
        self.assertEqual(m.gte_read(0), 0xFEDCBA98)
        for register in (0, 1):
            for address, offset, message in (
                (DATA, 1, "unaligned 4-byte load"),
                (DATA, 2, "unaligned 4-byte load"),
                (DATA, 3, "unaligned 4-byte load"),
                (0x80700000, 0, "unmapped read"),
            ):
                m = Machine(image(i_type(0x32, register, A0, offset)))
                m.map_zero(DATA, 0x10)
                m.r[A0] = address
                m.gte[register], m.gte_flag = 0x1234, None
                before = dict(m.gte)
                with (
                    self.subTest(register=register, address=address, offset=offset),
                    self.assertRaisesRegex(Fault, message),
                ):
                    m.step(BASE)
                self.assertEqual(m.gte, before)
                self.assertIsNone(m.gte_flag)
                self.assertEqual((m.stores, m.overlay), ([], {}))

    def test_mvmva_preserves_state_and_does_not_expand_sqr_gpf_status(self) -> None:
        for previous_flag in (None, 0xFFFFF000):
            m = Machine(image(0x4A486012))
            m.gte.update({register: 0x12340000 + register for register in range(32)})
            m.gte[0], m.gte[1] = 0xFFFE0001, 3
            m.gte_rotation = [4096, 0, 4096, 0, 4096]
            m.gte_flag = previous_flag
            m.lzcs, m.hi, m.lo = 0xFFFF0000, 0x10203040, 0x50607080
            m.put(DATA, 0xAABBCCDD)
            before, rotation = dict(m.gte), list(m.gte_rotation)
            registers, overlay = list(m.r), dict(m.overlay)
            m.step(BASE)
            before.update({9: 1, 10: -2, 11: 3, 25: 1, 26: 0xFFFFFFFE, 27: 3})
            self.assertEqual(m.gte, before)
            self.assertEqual(m.gte_rotation, rotation)
            self.assertEqual(m.gte_flag, 0)
            self.assertEqual((m.r, m.overlay), (registers, overlay))
            self.assertEqual((m.lzcs, m.hi, m.lo), (0xFFFF0000, 0x10203040, 0x50607080))
            self.assertEqual((m.stores, m.trace, m.patches), ([], [], {}))
        cop2 = self.cop2
        for command in (0x4A000028, 0x4B90003D):
            with (
                self.subTest(command=hex(command)),
                self.assertRaisesRegex(Fault, "GTE FLAG.*not modelled"),
            ):
                run(
                    0x4A486012,
                    command,
                    cop2(6, A0, 0),
                    cop2(4, A0, 0),
                    i_type(0x32, 1, A1, 0),
                    cop2(2, V0, 31),
                    NOP,
                    JR_RA,
                    NOP,
                    a1=DATA,
                )
            m = run(command, 0x4A486012, cop2(2, V0, 31), NOP, JR_RA, NOP)
            self.assertEqual(m.r[V0], 0)

    def test_mvmva_accepts_only_sdk_configuration_and_fake_command_variants(
        self,
    ) -> None:
        for fake in range(32):
            with self.subTest(fake=fake):
                m = self.apply(self.IDENTITY, (7, -11, 13), 0x4A086012 | fake << 20)
                self.assert_result(m, (7, -11, 13), (7, -11, 13), 0)
        command = 0x4A486012
        unsupported = [
            command & ~(1 << 19),
            command | 1 << 10,
            *(command | value << 17 for value in (1, 2, 3)),
            *(command | value << 15 for value in (1, 2, 3)),
            *((command & ~(3 << 13)) | value << 13 for value in (0, 1, 2)),
            *(command | 1 << bit for bit in (6, 7, 8, 9, 11, 12)),
        ]
        for word in unsupported:
            m = Machine(image(word))
            m.gte_flag = 0xFFFFF000
            before = dict(m.gte)
            with (
                self.subTest(word=hex(word)),
                self.assertRaisesRegex(Fault, "unsupported GTE MVMVA configuration"),
            ):
                m.step(BASE)
            self.assertEqual((m.gte, m.gte_flag), (before, 0xFFFFF000))
        cop2 = self.cop2
        for word in (
            cop2(6, A0, 8),
            cop2(6, A0, 30),
            cop2(2, V0, 0),
            cop2(4, A0, 2),
            cop2(0, V0, 2),
            i_type(0x32, 2, A0, 0),
        ):
            with (
                self.subTest(word=hex(word)),
                self.assertRaisesRegex(Fault, "unhandled"),
            ):
                run(word, JR_RA, NOP)


class Mac0Stores(unittest.TestCase):
    @staticmethod
    def unchanged_state(m: Machine) -> tuple:
        return (
            tuple(m.r),
            tuple(m.gte.items()),
            tuple(m.gte_rotation),
            m.gte_flag,
            m.lzcs,
            m.hi,
            m.lo,
            tuple(m.images),
            tuple(m.zero),
            tuple(m.trace),
            dict(m.stubs),
            set(m.tail_stubs),
            set(m.follow),
            dict(m.patches),
            m.steps,
        )

    @staticmethod
    def machine(word: int) -> Machine:
        m = Machine(image(word))
        m.gte.update({register: 0x12340000 + register for register in range(32)})
        m.gte_rotation = [4096, 0, 4096, 0, 4096]
        m.lzcs, m.hi, m.lo = 0xFFFF0000, 0x10203040, 0x50607080
        m.r[24] = 0x13579BDF
        return m

    def test_original_swc2_mac0_word_preserves_sign_bytes_and_other_state(self) -> None:
        for value, payload in (
            (0, b"\x00\x00\x00\x00"),
            (0x12345678, b"\x78\x56\x34\x12"),
            (0x7FFFFFFF, b"\xff\xff\xff\x7f"),
            (0x80000000, b"\x00\x00\x00\x80"),
            (0xFFFFFFFF, b"\xff\xff\xff\xff"),
        ):
            for flag in (None, 0x80010000):
                with self.subTest(value=hex(value), flag=flag):
                    m = self.machine(0xE9580000)
                    m.store(DATA, b"\xa5" * 16)
                    m.r[T2], m.gte[24], m.gte_flag = DATA + 4, value, flag
                    before = self.unchanged_state(m)
                    m.step(BASE)
                    self.assertEqual(self.unchanged_state(m), before)
                    self.assertEqual(
                        m.bytes(DATA, 16), b"\xa5" * 4 + payload + b"\xa5" * 8
                    )
                    self.assertEqual(m.stores, [(DATA + 4, 4)])

    def test_swc2_signed_offsets_wrap_and_stay_within_one_word(self) -> None:
        for base, offset, destination in (
            (0x80300010, -8, 0x80300008),
            (0x80300000, 8, 0x80300008),
            (0x80308000, -32768, 0x80300000),
            (0x80300000, 32764, 0x80307FFC),
            (0xFFFFFFFC, 4, 0),
            (0, -4, 0xFFFFFFFC),
        ):
            with self.subTest(base=hex(base), offset=offset):
                m = self.machine(i_type(0x3A, 24, A0, offset))
                low, high = (destination - 4) & MASK, (destination + 4) & MASK
                m.put(low, 0x10203040)
                m.put(destination, 0xA5A5A5A5)
                m.put(high, 0x50607080)
                m.r[A0], m.gte[24] = base, 0x89ABCDEF
                before, keys = self.unchanged_state(m), set(m.overlay)
                m.step(BASE)
                self.assertEqual(self.unchanged_state(m), before)
                self.assertEqual(m.bytes(destination, 4), b"\xef\xcd\xab\x89")
                self.assertEqual(m.bytes(low, 4), b"\x40\x30\x20\x10")
                self.assertEqual(m.bytes(high, 4), b"\x80\x70\x60\x50")
                self.assertEqual(set(m.overlay), keys)
                self.assertTrue(all(0 <= address <= MASK for address in m.overlay))
                self.assertEqual(m.stores, [(destination, 4)])

    def test_swc2_misalignment_faults_before_any_write(self) -> None:
        for base, offset in ((DATA, 1), (DATA, 2), (DATA, 3), (0, -1)):
            m = self.machine(i_type(0x3A, 24, A0, offset))
            m.store(DATA, b"\xa5" * 16)
            m.r[A0], m.gte_flag = base, None
            before, memory = self.unchanged_state(m), dict(m.overlay)
            with (
                self.subTest(base=hex(base), offset=offset),
                self.assertRaisesRegex(Fault, "unaligned 4-byte store"),
            ):
                m.step(BASE)
            self.assertEqual(self.unchanged_state(m), before)
            self.assertEqual((m.overlay, m.stores), (memory, []))

    def test_swc2_refuses_every_other_data_register_explicitly(self) -> None:
        for register in range(32):
            if register in (8, 14, 24):
                continue
            m = self.machine(i_type(0x3A, register, A0, 0))
            m.store(DATA, b"\xa5" * 16)
            m.r[A0] = DATA + 1
            before, memory = self.unchanged_state(m), dict(m.overlay)
            with (
                self.subTest(register=register),
                self.assertRaisesRegex(Fault, "unsupported SWC2 data register"),
            ):
                m.step(BASE)
            self.assertEqual(self.unchanged_state(m), before)
            self.assertEqual((m.overlay, m.stores), (memory, []))

    def test_swc2_preserves_nclip_and_unknown_sqr_gpf_status(self) -> None:
        cfc_flag = SdkMatrix.cop2(2, V0, 31)
        m = Machine(image(0x4B400006, 0xE9580000, cfc_flag))
        m.gte[12], m.gte[13], m.gte[14] = 0x80008000, 0x80007FFF, 0x7FFF8000
        m.r[T2] = DATA
        m.step(BASE)
        m.step(BASE + 4)
        m.step(BASE + 8)
        self.assertEqual(m.bytes(DATA, 4), b"\x01\x00\xfe\xff")
        self.assertEqual((m.gte_read(24), m.r[V0]), (0xFFFE0001, 0x80010000))
        for command in (0x4A000028, 0x4B90003D):
            m = Machine(image(command, 0xE9580000, cfc_flag))
            m.r[T2], m.gte[24] = DATA, 0x89ABCDEF
            m.step(BASE)
            before = self.unchanged_state(m)
            m.step(BASE + 4)
            self.assertEqual(self.unchanged_state(m), before)
            self.assertEqual(m.bytes(DATA, 4), b"\xef\xcd\xab\x89")
            with (
                self.subTest(command=hex(command)),
                self.assertRaisesRegex(Fault, "GTE FLAG.*not modelled"),
            ):
                m.step(BASE + 8)


class SdkProjection(unittest.TestCase):
    @staticmethod
    def machine(
        *,
        matrix=SdkMatrix.IDENTITY,
        vector=(12, -7, 1024),
        translation=(0, 0, 0),
        projection=(160 << 16, 120 << 16, 256, 0, 0),
        command=0x4A180001,
    ) -> Machine:
        """Initialize every transform/projection input explicitly, without game defaults."""
        m = Machine(image(command))
        m.gte_rotation = [
            (matrix[index] & 0xFFFF) | (matrix[index + 1] & 0xFFFF) << 16
            for index in range(0, 8, 2)
        ] + [differential.s16(matrix[8])]
        m.gte_translation = list(translation)
        m.gte_projection = dict(zip((24, 25, 26, 27, 28), projection, strict=True))
        m.gte[0] = (vector[0] & 0xFFFF) | (vector[1] & 0xFFFF) << 16
        m.gte[1] = differential.s16(vector[2])
        m.gte_flag = 0xFFFFF000
        return m

    @staticmethod
    def state(m: Machine) -> tuple:
        return (
            Mac0Stores.unchanged_state(m),
            tuple(m.gte_translation),
            tuple(m.gte_projection.items()),
            dict(m.overlay),
            list(m.stores),
        )

    def test_translation_and_projection_control_transfers_preserve_state(self) -> None:
        for register, value, expected in (
            (5, 0x80000000, -2147483648),
            (6, 0xFFFF8001, -32767),
            (7, 0x7FFFFFFF, 2147483647),
            (24, 0xFFFF0000, -65536),
            (25, 0x80000000, -2147483648),
            (26, 0x1234FFFF, 65535),
        ):
            for flag in (None, 0xFFFFF000):
                with self.subTest(register=register, flag=flag):
                    m = self.machine(command=SdkMatrix.cop2(6, A0, register))
                    m.r[A0], m.gte_flag = value, flag
                    before = Mac0Stores.unchanged_state(m)
                    translation, projection = (
                        list(m.gte_translation),
                        dict(m.gte_projection),
                    )
                    if register < 8:
                        translation[register - 5] = expected
                    else:
                        projection[register] = expected
                    m.step(BASE)
                    self.assertEqual(Mac0Stores.unchanged_state(m), before)
                    self.assertEqual(m.gte_translation, translation)
                    self.assertEqual(m.gte_projection, projection)

    def test_rtps_fifo_repeat_entry_and_unmodified_state(self) -> None:
        for previous_flag in (None, 0xFFFFF000):
            with self.subTest(previous_flag=previous_flag):
                m = self.machine()
                m.gte.update(
                    {
                        12: 0x11112222,
                        13: 0x33334444,
                        14: 0x55556666,
                        16: 10,
                        17: 20,
                        18: 30,
                        19: 40,
                        20: 0xAABBCCDD,
                    }
                )
                m.gte_flag = previous_flag
                m.lzcs, m.hi, m.lo = 0xFFFF0000, 0x10203040, 0x50607080
                before, controls = (
                    dict(m.gte),
                    (
                        list(m.gte_rotation),
                        list(m.gte_translation),
                        dict(m.gte_projection),
                    ),
                )
                registers = list(m.r)
                m.step(BASE)
                before.update(
                    {
                        8: 0,
                        9: 12,
                        10: -7,
                        11: 1024,
                        12: 0x33334444,
                        13: 0x55556666,
                        14: 0x007600A3,
                        16: 20,
                        17: 30,
                        18: 40,
                        19: 1024,
                        24: 0,
                        25: 12,
                        26: 0xFFFFFFF9,
                        27: 1024,
                    }
                )
                self.assertEqual(m.gte, before)
                self.assertEqual(m.gte_flag, 0)
                self.assertEqual(
                    (m.gte_rotation, m.gte_translation, m.gte_projection), controls
                )
                self.assertEqual(m.r, registers)
                self.assertEqual(
                    (m.lzcs, m.hi, m.lo), (0xFFFF0000, 0x10203040, 0x50607080)
                )
                self.assertEqual(
                    (m.stores, m.overlay, m.trace, m.stubs, m.patches),
                    ([], {}, [], {}, {}),
                )
                m.gte[0], m.gte[1] = 0x0008FFFC, 512
                m.step(BASE)
                self.assertEqual(
                    tuple(m.gte[index] for index in (12, 13, 14)),
                    (0x55556666, 0x007600A3, 0x007C009E),
                )
                self.assertEqual(
                    tuple(m.gte[index] for index in (16, 17, 18, 19)),
                    (30, 40, 1024, 512),
                )
                self.assertEqual(m.gte_flag, 0)

    def test_rtps_signed44_partial_overflow_wrap_and_arithmetic_shift(self) -> None:
        for axis, translation_value, component, expected_mac, flag in (
            (0, 2147483647, 1, -2147483648, 0xC1000000),
            (1, 2147483647, 1, -2147483648, 0xA0800000),
            (2, 2147483647, 1, -2147483648, 0x90460000),
            (0, -2147483648, -1, 2147483647, 0x89000000),
            (1, -2147483648, -1, 2147483647, 0x84800000),
            (2, -2147483648, -1, 2147483647, 0x82440000),
        ):
            with self.subTest(axis=axis, component=component):
                matrix, translation = [0] * 9, [0, 0, 1]
                matrix[axis * 3], translation[axis] = 4096, translation_value
                m = self.machine(
                    matrix=matrix,
                    translation=translation,
                    vector=(component, 0, 0),
                    projection=(0, 0, 0, 0, 0),
                )
                m.step(BASE)
                self.assertEqual(differential.s32(m.gte_read(25 + axis)), expected_mac)
                self.assertEqual(m.gte[9 + axis], -32768 if component == 1 else 32767)
                self.assertEqual(m.gte_flag, flag)
        for translation, row, vector, mac, ir, flag in (
            (2147483647, (4096, -4096, 0), (1, 1, 0), 2147483647, 32767, 0xC9000000),
            (268435456, (0, 0, 0), (0, 0, 0), 268435456, 32767, 0x81000000),
            (
                0,
                (-32768, -32768, -32768),
                (-32768, -32768, -32768),
                786432,
                32767,
                0x81000000,
            ),
            (0, (1, 0, 0), (-1, 0, 0), -1, -1, 0),
            (0, (4095, 0, 0), (1, 0, 0), 0, 0, 0),
        ):
            with self.subTest(translation=translation, row=row):
                m = self.machine(
                    matrix=(*row, 0, 0, 0, 0, 0, 0),
                    vector=vector,
                    translation=(translation, 0, 1),
                    projection=(0, 0, 0, 0, 0),
                )
                m.step(BASE)
                self.assertEqual(
                    (differential.s32(m.gte_read(25)), m.gte[9], m.gte_flag),
                    (mac, ir, flag),
                )

    def test_rtps_strict_ir_depth_screen_limits_and_summary(self) -> None:
        for translation, ir, depth, flag in (
            ((32767, -32768, 32767), (32767, -32768, 32767), 32767, 0),
            ((32768, -32769, 32768), (32767, -32768, 32767), 32768, 0x81C00000),
            ((0, 0, 32768), (0, 0, 32767), 32768, 0x00400000),
            ((0, 0, 65535), (0, 0, 32767), 65535, 0x00400000),
            ((0, 0, 65536), (0, 0, 32767), 65535, 0x80440000),
            ((0, 0, -1), (0, 0, -1), 0, 0x80060000),
            ((0, 0, 0), (0, 0, 0), 0, 0x80020000),
        ):
            with self.subTest(translation=translation):
                m = self.machine(
                    vector=(0, 0, 0),
                    translation=translation,
                    projection=(0, 0, 0, 0, 0),
                )
                m.step(BASE)
                self.assertEqual(tuple(m.gte[index] for index in (9, 10, 11)), ir)
                self.assertEqual((m.gte[19], m.gte_flag), (depth, flag))
        for vector, sxy, flag in (
            ((1023, -1024, 1), 0xFC0003FF, 0),
            ((1024, -1025, 1), 0xFC0003FF, 0x80006000),
            ((-1024, 1023, 1), 0x03FFFC00, 0),
            ((-1025, 1024, 1), 0x03FFFC00, 0x80006000),
        ):
            with self.subTest(vector=vector):
                m = self.machine(vector=vector, projection=(0, 0, 1, 0, 0))
                m.step(BASE)
                self.assertEqual((m.gte[14], m.gte_flag), (sxy, flag))

    def test_rtps_unr_division_rounding_and_near_plane_edges(self) -> None:
        # PSX-SPX documents the last two clamped-but-not-overflowed UNR examples.
        for h, z, quotient, overflow in (
            (0, 0, 0x1FFFF, True),
            (0, 1, 0, False),
            (1, 1, 0x10000, False),
            (2, 1, 0x1FFFF, True),
            (1, 2, 0x08000, False),
            (3, 2, 0x18000, False),
            (1, 3, 0x05555, False),
            (65535, 32768, 0x1FFFE, False),
            (65535, 65535, 0x0FFFF, False),  # UNR is not exact even for H == SZ3.
            (0xFE3F, 0x7F20, 0x1FFFF, False),
            (0xF015, 0x780B, 0x1FFFF, False),
        ):
            with self.subTest(h=h, z=z):
                self.assertEqual(Machine.gte_divide(h, z), (quotient, overflow))
        self.assertEqual(Machine.gte_divide(0x1234FFFF, 65535), (0x0FFFF, False))
        self.assertEqual((0xFE3F * 0x10000 + 0x7F20 // 2) // 0x7F20, 0x1FFFE)

    def test_rtps_screen_and_depth_cue_use_wide_mac0_before_limiters(self) -> None:
        m = self.machine(vector=(32767, -32768, 1), projection=(65536, -1, 1, 0, 0))
        m.step(BASE)
        self.assertEqual(
            (m.gte[14], m.gte[24], m.gte_flag), (0xFC0003FF, 0, 0x8001E000)
        )
        for dqa, dqb, mac0, ir0, flag in (
            (0, 0, 0, 0, 0),
            (0, -1, 0xFFFFFFFF, 0, 0x00001000),
            (256, 0, 0x01000000, 4096, 0),
            (256, 4096, 0x01001000, 4096, 0x00001000),
            (32767, 65535, 0x7FFFFFFF, 4096, 0x00001000),
            (32767, 65536, 0x80000000, 4096, 0x80011000),
            (-32768, 0, 0x80000000, 0, 0x00001000),
            (-32768, -1, 0x7FFFFFFF, 0, 0x80009000),
            (0x1234FFFF, 0, 0xFFFF0000, 0, 0x00001000),
            (0, 0x80000000, 0x80000000, 0, 0x00001000),
        ):
            with self.subTest(dqa=dqa, dqb=dqb):
                m = self.machine(vector=(0, 0, 1), projection=(0, 0, 1, dqa, dqb))
                m.step(BASE)
                self.assertEqual((m.gte[24], m.gte[8], m.gte_flag), (mac0, ir0, flag))

    def test_projection_result_transfers_and_faults_are_atomic(self) -> None:
        for register, value, expected in (
            (8, -32767, b"\x01\x80\xff\xff"),
            (14, 0x80017FFF, b"\xff\x7f\x01\x80"),
        ):
            for base, offset, destination in (
                (DATA + 8, -4, DATA + 4),
                (0xFFFFFFFC, 4, 0),
            ):
                with self.subTest(register=register, base=base):
                    m = self.machine(command=i_type(0x3A, register, A0, offset))
                    m.gte[register], m.r[A0], m.gte_flag = value, base, None
                    low = (destination - 4) & MASK
                    m.store(low, b"\xa5" * 4)
                    m.store(destination, b"\xa5" * 8)
                    before = self.state(m)[:3]
                    m.step(BASE)
                    self.assertEqual(self.state(m)[:3], before)
                    self.assertEqual(m.bytes(low, 4), b"\xa5" * 4)
                    self.assertEqual(m.bytes(destination, 8), expected + b"\xa5" * 4)
                    self.assertTrue(all(0 <= address <= MASK for address in m.overlay))
                    self.assertEqual(m.stores, [(destination, 4)])
            for offset in (1, 2, 3):
                m = self.machine(command=i_type(0x3A, register, A0, offset))
                m.gte[register], m.r[A0], m.gte_flag = value, DATA, None
                m.store(DATA, b"\xa5" * 16)
                before = self.state(m)
                with (
                    self.subTest(register=register, offset=offset),
                    self.assertRaisesRegex(Fault, "unaligned 4-byte store"),
                ):
                    m.step(BASE)
                self.assertEqual(self.state(m), before)
        m = self.machine(command=SdkMatrix.cop2(0, V0, 19))
        m.gte[19], m.gte_flag = 0x1234FFFF, None
        m.step(BASE)
        self.assertEqual(m.r[V0], 65535)
        self.assertIsNone(m.gte_flag)
        for move, register in (
            (0, 15),
            (4, 15),
            (0, 28),
            (4, 28),
            (0, 29),
            (4, 29),
            (0, 16),
            (0, 17),
            (0, 18),
            (4, 19),
            (2, 26),
            (6, 27),
            (6, 28),
        ):
            m = self.machine(command=SdkMatrix.cop2(move, A0, register))
            before = self.state(m)
            with (
                self.subTest(move=move, register=register),
                self.assertRaisesRegex(Fault, "unhandled GTE instruction"),
            ):
                m.step(BASE)
            self.assertEqual(self.state(m), before)

    def test_rtps_configuration_guards_and_unknown_other_command_flags(self) -> None:
        for fake in range(32):
            m = self.machine(command=0x4A080001 | fake << 20)
            m.step(BASE)
            self.assertEqual(
                (m.gte[9], m.gte[10], m.gte[11], m.gte_flag), (12, -7, 1024, 0)
            )
        for word in (
            0x4A000001,
            *(0x4A080001 | 1 << bit for bit in range(6, 19)),
        ):
            m = self.machine(command=word)
            before = self.state(m)
            with (
                self.subTest(word=hex(word)),
                self.assertRaisesRegex(Fault, "unsupported GTE RTPS configuration"),
            ):
                m.step(BASE)
            self.assertEqual(self.state(m), before)
        for command in (0x4A000028, 0x4B90003D):
            m = self.machine()
            m.images = [
                image(
                    command,
                    SdkMatrix.cop2(6, A0, 5),
                    SdkMatrix.cop2(6, A0, 26),
                    i_type(0x3A, 14, A1, 0),
                    SdkMatrix.cop2(0, V1, 19),
                    SdkMatrix.cop2(2, V0, 31),
                    0x4A180001,
                    SdkMatrix.cop2(2, V0, 31),
                )
            ]
            m.r[A1] = DATA
            for index in range(5):
                m.step(BASE + 4 * index)
                self.assertIsNone(m.gte_flag)
            with (
                self.subTest(command=hex(command)),
                self.assertRaisesRegex(Fault, "GTE FLAG.*not modelled"),
            ):
                m.step(BASE + 20)
            m.step(BASE + 24)
            m.step(BASE + 28)
            self.assertEqual((m.gte_flag, m.r[V0]), (0, 0))


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
            run(0x48821000, JR_RA, NOP)  # mtc2 to unsupported vector 1
        with self.assertRaisesRegex(Fault, "unhandled opcode"):
            run(0xC8020000, JR_RA, NOP)  # lwc2 to unsupported vector 1
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
