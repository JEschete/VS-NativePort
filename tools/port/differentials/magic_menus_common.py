"""Shared original-instruction and native-driver support for MENU0 spell menus."""

from __future__ import annotations

import random

import differential
from differential import Image, Machine
from native_compile_support import COMPILER, COMPILER_OPTIONS, CXX_STANDARD

MODULE = "MENU/MENU0.PRG"
ENTRY_BASE = 0x80102800
FLY_OUT = 0x800FA92C
MODULES = (MODULE,)
DRIVER = "magic_menus_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\MagicMenus.cpp",
    r"Native\Game\src\Menu\MagicSpellMenu.cpp",
)
INCLUDES = (
    r"Native\Engine",
    r"Native\Content",
    r"Native\Engine\include",
    r"Native\Engine\src",
    r"Native\Content\include",
    r"Native\Content\src",
    r"Native\Game\include",
    r"Native\Game\src",
)
VECTORS = 2
SEED = 2604

differential.MODULE_BASES[MODULE] = ENTRY_BASE


def configure(function_index: int, entry: int, end: int, mutant_address: int,
              mutant_word: int) -> dict:
    def vector(rng: random.Random, number: int) -> list[int]:
        del rng
        return [function_index, 1 + (number & 1)]

    def run(machine: Machine, vector: list[int]) -> tuple[dict, list[int]]:
        _, init = vector
        calls: list[list[int]] = []

        def fly_out(m: Machine) -> None:
            calls.append([m.r[4], m.r[5]])

        machine.stub(FLY_OUT, fly_out)
        machine.r[4] = init
        machine.execute(entry)
        return {"result": machine.r[2], "calls": calls}, vector

    def check(_image: Image, _vector: list[int], observation: dict,
              native: list[int], _reference: tuple) -> list[str]:
        if native[0] != observation["result"]:
            return [f"return native {native[0]} original {observation['result']}"]
        native_calls = [[native[2], native[3]]] if native[1] else []
        if native[1] != len(observation["calls"]) or native_calls != observation["calls"]:
            return [f"flyout native {native_calls} original {observation['calls']}"]
        return []

    return {
        "MODULES": (MODULE,),
        "SOURCES": SOURCES,
        "DRIVER": DRIVER,
        "COMPILER": COMPILER,
        "CXX_STANDARD": CXX_STANDARD,
        "COMPILER_OPTIONS": COMPILER_OPTIONS,
        "INCLUDES": INCLUDES,
        "ENTRY": entry,
        "END": end,
        "VECTORS": VECTORS,
        "SEED": SEED + function_index,
        "vector": vector,
        "run": run,
        "check": check,
        "MUTANTS": {"wrong-flyout-menu": {mutant_address: mutant_word}},
    }
