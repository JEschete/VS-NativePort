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
        def event(index: int, **changes):
            def edit(d: dict) -> None:
                d["bank"]["motion"][5]["events"][index].update(changes)

            return edit

        def without_sound(d: dict) -> None:
            d["bank"]["motion"][5]["events"][0].pop("sound")

        return {
            "a newer version": lambda d: d.update(schema_version=3),
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
            "an event as an array": lambda d: d["bank"]["motion"][5][
                "events"
            ].__setitem__(2, [5, 0]),
            "an unknown op name": event(0, op="play-sound"),
            "a named op as its number": event(0, op=4),
            "a sound past a byte": event(0, sound=256),
            "a field the op does not take": event(0, volume=1),
            "bytes beside a named field": event(0, bytes=[9]),
            "a named op without its field": without_sound,
            "a vertex delta past its range": event(
                2, op="move-vertex-y", vertex=1, delta=64
            ),
            "an argument byte past 255": event(1, bytes=[256]),
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

    def test_every_event_form_the_native_writer_uses_is_valid(self) -> None:
        events = [
            {"frame": 1, "op": "move-vertex", "vertex": 261, "x": -64, "y": -1, "z": -128},
            {"frame": 1, "op": "move-vertex-y", "vertex": 300, "delta": -3},
            {"frame": 9, "op": "shadow-radius", "expand_rate": 4660, "shrink_rate": 43981, "frames": 7},
            {"frame": 2, "op": "play-sound-pair", "bytes": []},
            {"frame": 2, "op": "initialize-pose", "bytes": [7]},
            {"frame": 4, "op": 3},
            {"frame": 4, "op": 200, "bytes": [6]},
            {"frame": 5, "op": "end"},
        ]
        document = copy.deepcopy(self.sample)
        document["bank"]["motion"][5]["events"] = events
        self.assertEqual(self.errors(document), [])

    def test_the_event_ops_match_the_native_table(self) -> None:
        """Each op's name and named fields are those of Content/SeqEventForms.cpp."""
        source = (ROOT / "Native" / "Content" / "src" / "SeqEventForms.cpp").read_text(
            encoding="utf-8"
        )
        table = source[source.index("kForms{") : source.index("kNoForm =")]
        kinds = {"Byte": "byte", "SignedByte": "int8", "Word": "halfword"}
        native = {}
        for _kind, op, name, arguments in re.findall(
            r'(Form|Skipped)\((\d+),\s*"([^"]+)"((?:,\s*\w+\("[^"]+"\))*)\)', table
        ):
            fields = []
            for field_kind, field in re.findall(r'(\w+)\("([^"]+)"\)', arguments):
                if field_kind == "VertexMove":
                    fields += [("vertex", "vertex"), (field, "vertexDelta")]
                else:
                    fields.append((field, kinds[field_kind]))
            native[name] = (int(op), fields)
        defs = self.schema["$defs"]
        names, numbers = defs["eventOp"]["oneOf"]
        self.assertEqual(names["enum"], list(native))
        self.assertEqual(numbers["not"]["enum"], [op for op, _ in native.values()])
        schema = {name: [] for name in names["enum"]}
        for rule in defs["event"]["allOf"]:
            fields = [
                (field, value["$ref"].rsplit("/", 1)[1])
                for field, value in rule["then"]["properties"].items()
            ]
            self.assertEqual(rule["then"]["else"]["required"], [f for f, _ in fields])
            for name in rule["if"]["properties"]["op"]["enum"]:
                schema[name] = fields
        self.assertEqual(schema, {name: fields for name, (_, fields) in native.items()})


class AnimationSetSchemaTests(SchemaChecks, unittest.TestCase):
    """vs.animation-set (Content/AnimationSet.hpp)."""

    schema_file = "animation-set.schema.json"
    header = "AnimationSet.hpp"
    constant = "kAnimationSetSchema"
    fixture = "sample-set.json"
    referenced = ("actor-animation.schema.json",)

    def edits(self) -> dict:
        return {
            "a newer version": lambda d: d.update(schema_version=3),
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
            "a newer version": lambda d: d.update(schema_version=3),
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
