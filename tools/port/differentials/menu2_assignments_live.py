"""Supplemental F02621: real Live entry, foundation Risk calls and lifecycle checks."""

import menu2_assignments as original

globals().update({key: value for key, value in vars(original).items()
                  if not key.startswith("__")})
DRIVER = "menu2_ability_terminal_live_driver.cpp"
SOURCES = (
    *original.SOURCES,
    r"Native\Game\src\Menu\BattleAbilitiesTerminalLive.cpp",
    r"Native\Game\src\Menu\BattleAbilitiesUnlock.cpp",
    r"Native\Game\src\Menu\BattleAbilitiesUnlockLive.cpp",
)
INCLUDES = (
    *original.INCLUDES,
    r"Native\Game\src", r"Native\Engine\src", r"Native\Content\include",
    r"Native\Engine", r"Native\Content",
)
NATIVE_SOURCE = r"Native\Game\src\Menu\BattleAbilitiesTerminalLive.cpp"
NATIVE_REPLACE = (
    "constexpr std::uint32_t kCircle = 0x20;",
    "constexpr std::uint32_t kCircle = 0x10;",
)


def vector(rng, number):
    value = original.vector(rng, number)
    value[8] = 0
    return value


def describe(image, results):
    return [
        *original.describe(image, results),
        "real LiveUpdateAssignments plus foundation LiveSetRiskCost executed",
        "driver controls: category help/flags and cursor restoration, nested row creation, "
        "repeat-entry owner reuse, copied help surviving cancel, no double free, "
        "module/cold reload of image-backed cursor, list, learned-action and animation state",
    ]
