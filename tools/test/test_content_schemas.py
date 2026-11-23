"""Checks the content JSON Schemas against the fixtures the native writer produces."""

import copy
import json
import re
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator
from referencing import Registry, Resource

ROOT = Path(__file__).resolve().parents[2]
SCHEMAS = ROOT / "Native" / "Content" / "schemas"
FIXTURES = ROOT / "Native" / "Tests" / "data"


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class SchemaChecks:
    """Checks one schema against its native header and writer's fixture."""

    schema_file: str
    header: str
    constant: str
    fixture: str
    referenced: tuple[str, ...] = ()

    @classmethod
    def setUpClass(cls) -> None:
        schema = load(SCHEMAS / cls.schema_file)
        Draft202012Validator.check_schema(schema)
        registry = Registry()
        for name in cls.referenced:
            other = load(SCHEMAS / name)
            registry = registry.with_resource(
                other["$id"], Resource.from_contents(other)
            )
        cls.schema = schema
        cls.validator = Draft202012Validator(schema, registry=registry)
        cls.sample = load(FIXTURES / "animation" / cls.fixture)

    def edits(self) -> dict:
        raise NotImplementedError

    def errors(self, document: dict) -> list[str]:
        return [error.message for error in self.validator.iter_errors(document)]

    def test_the_native_writers_fixture_is_valid(self) -> None:
        self.assertEqual(self.errors(self.sample), [])

    def test_the_schema_names_the_native_version(self) -> None:
        header = (
            ROOT / "Native" / "Content" / "include" / "Content" / self.header
        ).read_text(encoding="utf-8")
        name = re.search(self.constant + r' = "([^"]+)"', header)
        version = re.search(self.constant + r"Version = (\d+);", header)
        self.assertIsNotNone(name)
        self.assertIsNotNone(version)
        self.assertEqual(self.schema["properties"]["schema"]["const"], name[1])
        self.assertEqual(
            self.schema["properties"]["schema_version"]["const"], int(version[1])
        )

    def test_edits_the_native_reader_rejects_are_invalid(self) -> None:
        for name, edit in self.edits().items():
            with self.subTest(name):
                document = copy.deepcopy(self.sample)
                edit(document)
                self.assertNotEqual(self.errors(document), [])


class ActorAnimationSchemaTests(SchemaChecks, unittest.TestCase):
    """vs.actor-animation (Content/ActorAnimation.hpp)."""

    schema_file = "actor-animation.schema.json"
    header = "ActorAnimation.hpp"
    constant = "kActorAnimationSchema"
    fixture = "sample-bank.json"

    def edits(self) -> dict:
        return {
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


class AnimationSetSchemaTests(SchemaChecks, unittest.TestCase):
    """vs.animation-set (Content/AnimationSet.hpp)."""

    schema_file = "animation-set.schema.json"
    header = "AnimationSet.hpp"
    constant = "kAnimationSetSchema"
    fixture = "sample-set.json"
    referenced = ("actor-animation.schema.json",)

    def edits(self) -> dict:
        return {
            "a newer version": lambda d: d.update(schema_version=2),
            "an uppercase hash": lambda d: d.update(
                source_sha256=d["source_sha256"].upper()
            ),
            "selector 65535": lambda d: d["entries"][0].update(selector=65535),
            "texture row 255": lambda d: d["entries"][1].update(texture_row=255),
            "too many bones": lambda d: d["entries"][0].update(bones=42),
            "an unknown entry member": lambda d: d["entries"][0].update(extra=1),
            "a missing texture row": lambda d: d["entries"][0].pop("texture_row"),
            "sixteen entries": lambda d: d.update(entries=d["entries"] * 8),
            "clip 255 in a bank's map": lambda d: d["entries"][1]["bank"][
                "map"
            ].__setitem__(1, 255),
        }


class ActorArchetypeSchemaTests(SchemaChecks, unittest.TestCase):
    """vs.actor-archetype (Content/ActorArchetype.hpp)."""

    schema_file = "actor-archetype.schema.json"
    header = "ActorArchetype.hpp"
    constant = "kActorArchetypeSchema"
    fixture = "sample-archetype.json"
    referenced = ("actor-animation.schema.json",)

    def edits(self) -> dict:
        return {
            "a newer version": lambda d: d.update(schema_version=2),
            "an uppercase hash": lambda d: d.update(
                source_sha256=d["source_sha256"].upper()
            ),
            "header byte 256": lambda d: d["header"].update(shield=256),
            "an unknown header member": lambda d: d["header"].update(extra=1),
            "a missing header member": lambda d: d["header"].pop("weapon"),
            "an uppercase id": lambda d: d.update(body={"model": "Model.OBJ.4F"}),
            "a body file in another directory": lambda d: d.update(
                body={"file": "../4F.gltf"}
            ),
            "a body both named and own": lambda d: d.update(
                body={"file": "Z.gltf", "model": "model.obj.4f"}
            ),
            "a weapon bank": lambda d: d.update(weapon={"animation": "m"}),
            "a shield number": lambda d: d.update(shield=1),
            "too many bones": lambda d: d["battle_animation"].update(bones=42),
            "clip 255 in a bank's map": lambda d: d["battle_animation"]["bank"][
                "map"
            ].__setitem__(1, 255),
            "an unknown member": lambda d: d.update(extra=1),
        }


if __name__ == "__main__":
    unittest.main()
