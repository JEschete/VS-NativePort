"""F02619 supplementary differential through the actual checked Live row binding."""

from __future__ import annotations

import menu2_ability_rows as rows

globals().update({name: getattr(rows, name) for name in (
    "ID", "ENTRY", "END", "SEED", "VECTORS", "MUTANT_VECTORS", "MUTANTS",
    "MODULES", "COMPILER", "CXX_STANDARD", "vector", "run", "check", "describe")})
DRIVER = "menu2_ability_rows_live_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\BattleAbilitiesUnlock.cpp",
    r"Native\Game\src\Menu\BattleAbilitiesUnlockLive.cpp",
)
NATIVE_REPLACE_SOURCE = SOURCES[1]
NATIVE_REPLACE = (
    "static_cast<std::uint8_t>(\n"
    "                Menu_Framework_menuMemory.cursorMemories[index + 1])",
    "static_cast<signed char>(\n"
    "                Menu_Framework_menuMemory.cursorMemories[index + 1])",
)
INCLUDES = (
    r"Native\Content\include", r"Native\Game\src", r"Native\Engine\src",
    r"Native\Engine", r"Native\Content",
)
