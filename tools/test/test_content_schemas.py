"""Checks the content JSON Schemas against the fixtures the native writer produces."""

import copy
import json
import re
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[2]
SCHEMAS = ROOT / "Native" / "Content" / "schemas"
FIXTURES = ROOT / "Native" / "Tests" / "data"


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class ActorAnimationSchemaTests(unittest.TestCase):
    """vs.actor-animation (Content/ActorAnimation.hpp)."""

    @classmethod
    def setUpClass(cls) -> None:
        schema = load(SCHEMAS / "actor-animation.schema.json")
        Draft202012Validator.check_schema(schema)
        cls.schema = schema
        cls.validator = Draft202012Validator(schema)
        cls.sample = load(FIXTURES / "animation" / "sample-bank.json")

    def errors(self, document: dict) -> list[str]:
        return [error.message for error in self.validator.iter_errors(document)]

    def test_the_native_writers_fixture_is_valid(self) -> None:
        self.assertEqual(self.errors(self.sample), [])

    def test_the_schema_names_the_native_version(self) -> None:
        header = (
            ROOT / "Native" / "Content" / "include" / "Content" / "ActorAnimation.hpp"
        ).read_text(encoding="utf-8")
        name = re.search(r'kActorAnimationSchema = "([^"]+)"', header)
        version = re.search(r"kActorAnimationSchemaVersion = (\d+);", header)
        self.assertIsNotNone(name)
        self.assertIsNotNone(version)
        self.assertEqual(self.schema["properties"]["schema"]["const"], name[1])
        self.assertEqual(
            self.schema["properties"]["schema_version"]["const"], int(version[1])
        )

    def test_edits_the_native_reader_rejects_are_invalid(self) -> None:
        edits = {
            "a newer version": lambda d: d.update(schema_version=2),
            "too many bones": lambda d: d.update(bones=42),
            "an uppercase hash": lambda d: d.update(
                source_sha256=d["source_sha256"].upper()
            ),
            "clip 255 in the map": lambda d: d["bank"]["map"].__setitem__(1, 255),
            "an unknown clip member": lambda d: d["bank"]["clips"][0].update(extra=1),
            "a reference without its command": lambda d: d["bank"]["clips"][1].update(
                root={"track": 7}
            ),
            "two initial keys": lambda d: d["bank"]["motion"][1].update(
                raw_scale=[1, 2, 3]
            ),
            "a raw scale past a byte": lambda d: d["bank"]["motion"][4][
                "raw_scale"
            ].__setitem__(1, 256),
            "an event without its op": lambda d: d["bank"]["motion"][5][
                "events"
            ].__setitem__(2, [5]),
            "uppercase unread bytes": lambda d: d["bank"]["motion"][8].update(
                unread="ABCD"
            ),
            "half an unread byte": lambda d: d["bank"]["motion"][8].update(
                unread="abc"
            ),
            "a fractional delta": lambda d: d["bank"]["motion"][1][
                "commands"
            ].__setitem__(1, 1.5),
        }
        for name, edit in edits.items():
            with self.subTest(name):
                document = copy.deepcopy(self.sample)
                edit(document)
                self.assertNotEqual(self.errors(document), [])


if __name__ == "__main__":
    unittest.main()
