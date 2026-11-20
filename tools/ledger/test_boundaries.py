"""Synthetic controls for the dependency boundary ledger; no repository files are read."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import boundaries  # noqa: E402
from gates import read_csv  # noqa: E402

PLAN = f"# Plan\n\n{boundaries.SUMMARY_BEGIN}\n\n{boundaries.SUMMARY_END}\n"
PROJECT = """<Project>
  <ItemGroup>
{items}
  </ItemGroup>
</Project>
"""
GAME_DIRECTORIES = """    <ClCompile Include="{path}">
      <AdditionalIncludeDirectories>$(VSCopiedCIncludes);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>"""
LEGACY_DIRECTORIES = """    <ClCompile Include="{path}">
      <AdditionalIncludeDirectories>$(VSLegacyIncludes);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>"""


class Fixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.write("Docs/NATIVE_SOURCE_PORT_PLAN.md", PLAN)
        (root / "Docs" / "ledgers").mkdir()
        self.write("Native/Game/include/Battle/Targeting.hpp", "#pragma once\n")
        self.write("Native/Engine/include/Render/Font.hpp", "#pragma once\n")
        self.write("Native/Engine/include/Audio/Voice.hpp", "#pragma once\n")
        self.write("Native/Engine/include/Platform/Fiber.hpp", "#pragma once\n")
        self.write("Native/Engine/include/Core/Log.hpp", "#pragma once\n")
        self.write("Native/Engine/Legacy/Psx.hpp", '#pragma once\n#include "Core/Log.hpp"\n')
        # Render may include another mechanism, the SDK aggregate and Content; only its Game include crosses.
        self.write("Native/Engine/src/Render/Text.cpp", '#include "Render/Font.hpp"\n#include "Audio/Voice.hpp"\n'
                   '#include "Legacy/Psx.hpp"\n#include "Battle/Targeting.hpp"\n#include <vector>\n')
        # Content may include Core but not a mechanism or the SDK aggregate.
        self.write("Native/Content/src/Assets.cpp", '#include "Core/Log.hpp"\n#include "Render/Font.hpp"\n'
                   '#include "Legacy/Psx.hpp"\n')
        # A Legacy\<System> file belongs to its system: Core may not include Platform.
        self.write("Native/Engine/Legacy/Core/Etc.cpp", '#include "Platform/Fiber.hpp"\n')
        # Game and the tests may include anything.
        self.write("Native/Game/src/Battle/Targeting.cpp", '#include "Render/Font.hpp"\n#include "Legacy/Psx.hpp"\n')
        self.write("Native/Tests/src/TextTests.cpp", '#include "Battle/Targeting.hpp"\n')
        self.project(game=["src\\Render\\Text.cpp"], legacy=["Legacy\\Core\\Etc.cpp"])

    def write(self, path: str, text: str) -> None:
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")

    def project(self, game: list[str], legacy: list[str]) -> None:
        items = [GAME_DIRECTORIES.format(path=path) for path in game]
        items += [LEGACY_DIRECTORIES.format(path=path) for path in legacy]
        self.write("Native/Engine/VS.Engine.vcxproj", PROJECT.format(items="\n".join(items)))

    def rows(self) -> list[dict[str, str]]:
        return read_csv(boundaries.ledger_path(self.root))

    def rules(self) -> list[str]:
        return sorted(issue.rule for issue in boundaries.check(self.root))


class BoundaryTests(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.fixture = Fixture(Path(directory.name))

    def test_first_sync_records_only_the_includes_that_cross_the_matrix(self) -> None:
        boundaries.sync(self.fixture.root)
        rows = [(row["file"], row["from_layer"], row["to_layer"], row["status"]) for row in self.fixture.rows()]
        self.assertEqual(rows, [
            ("Native/Content/src/Assets.cpp", "Content", "Engine.Legacy", "remaining"),
            ("Native/Content/src/Assets.cpp", "Content", "Engine.Render", "remaining"),
            ("Native/Engine/Legacy/Core/Etc.cpp", "Engine.Core", "Engine.Platform", "remaining"),
            ("Native/Engine/src/Render/Text.cpp", "Engine.Render", "Game", "remaining"),
        ])
        self.assertEqual(self.fixture.rules(), [])
        plan = (self.fixture.root / "Docs/NATIVE_SOURCE_PORT_PLAN.md").read_text(encoding="utf-8")
        self.assertIn("**0 of 4 crossing includes resolved**, 4 remaining in 3 files.", plan)

    def test_a_new_crossing_fails_and_sync_does_not_adopt_it(self) -> None:
        boundaries.sync(self.fixture.root)
        self.fixture.write("Native/Engine/src/Audio/Voice.cpp", '#include "Battle/Targeting.hpp"\n')
        boundaries.sync(self.fixture.root)
        self.assertEqual(len(self.fixture.rows()), 4)
        self.assertIn("crossing-new", self.fixture.rules())

    def test_a_removed_crossing_is_stale_until_synced_and_may_not_return(self) -> None:
        boundaries.sync(self.fixture.root)
        self.fixture.write("Native/Content/src/Assets.cpp", '#include "Core/Log.hpp"\n#include "Legacy/Psx.hpp"\n')
        self.assertEqual(self.fixture.rules(), ["ledger-stale"])
        boundaries.sync(self.fixture.root)
        self.assertEqual(self.fixture.rules(), [])
        self.assertEqual([row["status"] for row in self.fixture.rows()],
                         ["remaining", "resolved", "remaining", "remaining"])
        self.fixture.write("Native/Content/src/Assets.cpp", '#include "Render/Font.hpp"\n#include "Legacy/Psx.hpp"\n')
        boundaries.sync(self.fixture.root)
        self.assertEqual(self.fixture.rules(), ["crossing-returned"])

    def test_only_sources_still_crossing_into_game_may_see_game_directories(self) -> None:
        boundaries.sync(self.fixture.root)
        self.fixture.project(game=["src\\Render\\Text.cpp", "Legacy\\Core\\Etc.cpp"], legacy=[])
        self.assertEqual(self.fixture.rules(), ["project-stale"])
        self.fixture.write("Native/Engine/src/Render/Text.cpp", '#include "Render/Font.hpp"\n')
        boundaries.sync(self.fixture.root)
        self.fixture.project(game=[], legacy=["src\\Render\\Text.cpp", "Legacy\\Core\\Etc.cpp"])
        self.assertEqual(self.fixture.rules(), [])

    def test_a_folder_outside_the_matrix_must_be_classified(self) -> None:
        boundaries.sync(self.fixture.root)
        self.fixture.write("Native/Engine/src/Network/Socket.cpp", '#include "Core/Log.hpp"\n')
        self.assertEqual(self.fixture.rules(), ["layer-unknown"])

    def test_an_unknown_status_fails(self) -> None:
        boundaries.sync(self.fixture.root)
        ledger = boundaries.ledger_path(self.fixture.root)
        ledger.write_text(ledger.read_text(encoding="utf-8").replace("remaining", "waived", 1), encoding="utf-8")
        self.assertIn("status-unknown", self.fixture.rules())


if __name__ == "__main__":
    unittest.main()
