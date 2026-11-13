"""Regressions for native source adaptations, without game assets."""

import csv
import json
import re
import runpy
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import audit_assembly_gaps
import check_event_dispatch
import copy_fixups
import copy_to_native
import native_views


def flat(source: str) -> str:
    """Source text regardless of how the formatter wrapped it."""
    return " ".join(source.split())


class EncounterFixupTests(unittest.TestCase):
    def test_action_sequence_preserves_data_and_binds_five_owned_entries(self):
        names = {
            "F00187": ("WaitForLiveActionEffectReady", 0x80078748),
            "F00188": ("StartLiveReadyActionAnimation", 0x800787F0),
            "F00192": ("WaitForLiveActionResolution", 0x80079050),
            "F00193": ("WaitForLiveTargetRecovery", 0x800790BC),
            "F00195": ("FinishLiveActionSequence", 0x800793C0),
        }
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as table:
            owned = {row["id"]: row for row in csv.DictReader(table)}
        for identifier, (name, address) in names.items():
            binding = f"VS::Battle::{name}"
            with self.subTest(identifier=identifier):
                self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS[identifier], binding)
                self.assertIn(("BATTLE/BATTLE.PRG", address, binding), copy_fixups.MODULE_BINDING_ENTRIES)
                self.assertEqual(owned[identifier]["path"], r"Native\Game\src\Battle\ActionSequence.cpp")
                self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[binding],
                                 r"Native\Game\include\Battle\ActionSequenceLive.hpp")
        generated = (root / r"Native\Game\src\Battle\ActionExecution.cpp").read_text(encoding="utf-8")
        self.assertNotIn("void Battle_", generated)
        self.assertIn("Battle_settledCameraDistance", generated)
        self.assertIn("Battle_cameraEaseMode", generated)
        binding = (root / r"Native\Game\src\Battle\ActionSequenceLive.cpp").read_text(encoding="utf-8")
        self.assertIn("Field_sphericalCamera.values.distance", binding)
        self.assertNotIn("Battle_settledCameraDistance", binding)
        self.assertIn("Render::SetLiveCameraProjectionDistance", binding)
        self.assertNotIn("Core_Gte_SetGeomScreen", binding)
        self.assertIn("Target recovery refused: empty sequence has no last action", binding)

    def test_action_camera_selection_binds_all_eight_owned_entries(self):
        names = ("ActionTargetLimbPosition", "ActionTargetDistance", "ActionCameraMidpoint",
                 "StepCameraTowardActionMidpoint", "UpdateInitialTargetSelection",
                 "UpdateActionAreaSelection", "UpdateFinalTargetSelection", "UpdateActionCameraZoom")
        addresses = (0x80077F14, 0x80077F70, 0x800780A8, 0x8007820C,
                     0x80078248, 0x800782E4, 0x80078364, 0x800784AC)
        with (Path(__file__).parent / "owned.csv").open(encoding="utf-8", newline="") as table:
            owned = {row["id"]: row for row in csv.DictReader(table)}
        for number, (name, address) in enumerate(zip(names, addresses, strict=True), 179):
            identifier, binding = f"F{number:05}", f"VS::Battle::Live{name}"
            with self.subTest(identifier=identifier):
                self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS[identifier], binding)
                self.assertIn(("BATTLE/BATTLE.PRG", address, binding), copy_fixups.MODULE_BINDING_ENTRIES)
                self.assertEqual(owned[identifier]["path"], r"Native\Game\src\Battle\ActionCameraSelection.cpp")
                self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[binding],
                                 r"Native\Game\include\Battle\ActionCameraSelectionLive.hpp")
        root = Path(__file__).resolve().parents[2]
        generated = (root / r"Native\Game\src\Battle\ActionExecution.cpp").read_text(encoding="utf-8")
        for name in names:
            self.assertNotIn(f"Battle_{name}(", generated)
        self.assertIn("Battle_settledCameraDistance", generated)
        self.assertIn("Battle_cameraEaseMode", generated)

    def test_action_outcomes_keep_globals_and_bind_owned_module_entries(self):
        names = {
            "F00048": ("ResolveLiveEnemyDefeat", "BATTLE/BATTLE.PRG", 0x8006C1CC),
            "F00059": ("PlayLiveWeaponHitSounds", "BATTLE/BATTLE.PRG", 0x8006C5AC),
            "F00061": ("ApplyLiveActionSourceEffects", "BATTLE/BATTLE.PRG", 0x8006CA20),
            "F00156": ("BeginLiveActorDefeatAnimation", "BATTLE/BATTLE.PRG", 0x80074A20),
            "F01851": ("ApplyLiveLearnedActionUnlocks", "BATTLE/INITBTL.PRG", 0x800F9818),
        }
        with (Path(__file__).parent / "owned.csv").open(encoding="utf-8", newline="") as table:
            owned = {row["id"]: row for row in csv.DictReader(table)}
        for identifier, (name, module, address) in names.items():
            with self.subTest(identifier=identifier):
                binding = f"VS::Battle::{name}"
                self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS[identifier], binding)
                self.assertIn((module, address, binding), copy_fixups.MODULE_BINDING_ENTRIES)
                self.assertEqual(owned[identifier]["path"], r"Native\Game\src\Battle\ActionOutcomes.cpp")
                self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[binding],
                                 r"Native\Game\include\Battle\ActionOutcomesLive.hpp")
        generated = (Path(__file__).resolve().parents[2] / r"Native\Game\src\Battle\ActionFlow.cpp").read_text(
            encoding="utf-8")
        for symbol in ("Battle_actionState", "Battle_actionFlowMode", "Battle_recoveryRewardMode",
                       "Battle_actionSelection", "Battle_spellCommitFlag"):
            self.assertIn(symbol, generated)
        for name in ("Battle_ResolveEnemyDefeat", "Battle_PlayWeaponHitSounds",
                     "Battle_ApplyActionSourceEffects", "Battle_BeginActorDefeatAnimation",
                     "Battle_ApplyLearnedActionUnlocks"):
            self.assertNotIn(name, generated)

    def marking_fixture(self, root):
        tables = {
            "FUNCTION_CATALOG.csv": "id,copy,kind,notes,copied,compiles,modernized\nF1,yes,C,,x,x,x\n",
            "DATA_LEDGER.csv": "id,copied,compiles,notes\nG1,retired,x,retired-by:fixture\n",
            "TYPE_LEDGER.csv": "id,copied,compiles,notes\nT1,retired,x,retired-by:fixture\n",
        }
        for name, contents in tables.items():
            (root / name).write_text(contents, encoding="utf-8")
        return SimpleNamespace(files={}, owned={"F1"}, cat_by_id={})

    def test_mark_ledgers_rejects_malformed_input_before_any_write(self):
        for name in ("FUNCTION_CATALOG.csv", "DATA_LEDGER.csv", "TYPE_LEDGER.csv"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                copier = self.marking_fixture(root)
                path = root / name
                path.write_text(path.read_text(encoding="utf-8") + "bad,unquoted,extra,fields,here,here,here,here\n", encoding="utf-8")
                saved = {table.name: table.read_bytes() for table in root.iterdir()}
                with (
                    mock.patch.object(copy_to_native, "LEDGERS", root),
                    self.assertRaisesRegex(ValueError, f"Malformed {re.escape(name)} row bad"),
                ):
                    copy_to_native.mark_ledgers(copier)
                self.assertEqual(saved, {table.name: table.read_bytes() for table in root.iterdir()})

    def test_mark_ledgers_preserves_modernized_owned_status(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            copier = self.marking_fixture(root)
            with mock.patch.object(copy_to_native, "LEDGERS", root):
                copy_to_native.mark_ledgers(copier)
            with (root / "FUNCTION_CATALOG.csv").open(encoding="utf-8", newline="") as table:
                row = next(csv.DictReader(table))
            self.assertEqual((row["copied"], row["compiles"], row["modernized"]), ("x", "x", "x"))

    def test_real_copy_ledgers_have_no_extra_or_missing_csv_fields(self):
        for name in ("FUNCTION_CATALOG.csv", "DATA_LEDGER.csv", "TYPE_LEDGER.csv"):
            with (copy_to_native.LEDGERS / name).open(encoding="utf-8", newline="") as table:
                for row in csv.DictReader(table):
                    with self.subTest(name=name, identifier=row.get("id")):
                        self.assertNotIn(None, row)
                        self.assertNotIn(None, row.values())

    def test_sigil_coordinate_accessor_is_retired_without_module_consumers(self):
        with (copy_to_native.LEDGERS / "FUNCTION_CATALOG.csv").open(encoding="utf-8", newline="") as table:
            row = next(row for row in csv.DictReader(table) if row["id"] == "F00632")
        self.assertEqual((row["copy"], row["copied"]), ("no", "retired"))
        self.assertIn("retired-by:VS::Field::SigilDoorRoomPair", row["notes"])
        self.assertFalse(any(identifier == "F00632" for _, _, identifier in copy_fixups.MODULE_FUNCTION_ENTRIES))
        self.assertNotIn("F00632", copy_fixups.OWNED_CALL_BINDINGS)

    def test_room_door_getter_retires_after_typed_unlock_consumer_adoption(self):
        with (copy_to_native.LEDGERS / "FUNCTION_CATALOG.csv").open(encoding="utf-8", newline="") as table:
            row = next(row for row in csv.DictReader(table) if row["id"] == "F00623")
        self.assertEqual((row["copy"], row["copied"]), ("no", "retired"))
        self.assertFalse(any(identifier == "F00623" for _, _, identifier in copy_fixups.MODULE_FUNCTION_ENTRIES))
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F00067"], "Field_ApplyUnlockEffectToRoomObjectEntry")

    def test_active_trigger_getter_retires_after_typed_targeting_adoption(self):
        with (copy_to_native.LEDGERS / "FUNCTION_CATALOG.csv").open(encoding="utf-8", newline="") as table:
            row = next(row for row in csv.DictReader(table) if row["id"] == "F00624")
        self.assertEqual((row["copy"], row["copied"]), ("no", "retired"))
        self.assertFalse(any(identifier == "F00624" for _, _, identifier in copy_fixups.MODULE_FUNCTION_ENTRIES))
        self.assertNotIn("F00624", copy_fixups.OWNED_CALL_BINDINGS)
        source = (copy_to_native.REPO / "Native/Game/src/Battle/TargetingLive.cpp").read_text(encoding="utf-8")
        self.assertIn("LiveActiveRoomTriggerInfo()", source)
        self.assertNotIn("Field_ActiveRoomTriggers(", source)

    def test_unused_platform_clear_alias_retires_without_module_binding(self):
        with (copy_to_native.LEDGERS / "FUNCTION_CATALOG.csv").open(encoding="utf-8", newline="") as table:
            row = next(row for row in csv.DictReader(table) if row["id"] == "F00592")
        self.assertEqual((row["copy"], row["copied"]), ("no", "retired"))
        self.assertFalse(any(identifier == "F00592" for _, _, identifier in copy_fixups.MODULE_FUNCTION_ENTRIES))
        self.assertNotIn("F00592", copy_fixups.OWNED_CALL_BINDINGS)

    def test_empty_calculation_sentinels_retire_with_distinct_typed_guards(self):
        with (copy_to_native.LEDGERS / "FUNCTION_CATALOG.csv").open(encoding="utf-8", newline="") as table:
            rows = {row["id"]: row for row in csv.DictReader(table)}
        with (copy_to_native.REPO / "tools/port/owned.csv").open(encoding="utf-8", newline="") as table:
            owned = {row["id"] for row in csv.DictReader(table)}
        for identifier, address, kind in (("F00341", 0x8007E974, "Hit"),
                                          ("F00407", 0x80081130, "Stat"),
                                          ("F00436", 0x80083990, "Effect")):
            with self.subTest(identifier=identifier):
                row = rows[identifier]
                guard = f"VS::Battle::RejectLive{kind}Selector"
                self.assertEqual((row["copy"], row["copied"], row["cpp"]), ("no", "retired", ""))
                self.assertIn(f"retired-by:{guard}", row["notes"])
                self.assertNotIn(identifier, owned)
                self.assertNotIn(identifier, copy_fixups.OWNED_CALL_BINDINGS)
                self.assertIn(("BATTLE/BATTLE.PRG", address, guard), copy_fixups.MODULE_BINDING_ENTRIES)

    def test_action_routing_keeps_zero_and_direct_prerequisites_out_of_hit_roll(self):
        source = (copy_to_native.REPO / "Native/Game/src/Battle/ActionPreview.cpp").read_text(encoding="utf-8")
        body = source[source.index("bool ResolveHits("):source.index("ActionRequest FreshPreview(")]
        self.assertIn("if (prerequisite == 0) continue;", body)
        direct = body[body.index("if (prerequisite >= kFirstDirectPrerequisite"):body.index("} else if")]
        self.assertIn("world.Effect(", direct)
        self.assertNotIn("world.Roll(", direct)
        self.assertEqual(source.count("world.Roll("), 2)
        live = (copy_to_native.REPO / "Native/Game/src/Battle/ActionPreviewLive.cpp").read_text(encoding="utf-8")
        self.assertIn("LiveRollActionHit(", live)
        self.assertIn("ApplyLiveActionHitEffect(", live)

    def test_texture_animations_use_the_callers_overlay_atlas(self):
        copier = object.__new__(copy_to_native.Copier)
        copier.used_fixups = set()
        atlas = "Menu_Results_disMap"
        for animation in ("TintFade", "FlashFade", "Wipe"):
            with self.subTest(animation=animation):
                name = f"VS::Render::DrawTexture{animation}"
                live = f"VS::Render::DrawLiveTexture{animation}"
                arguments = "x, y, texId, colors"
                if animation == "Wipe":
                    arguments += ", wipe_edge_x"
                row = {"id": "test", "final_name": "Menu::Results::DrawHeading"}
                for whitespace in ("", "\n    "):
                    source = f"{name}({whitespace}{arguments});"
                    self.assertEqual(
                        copier.apply_fixups(row, source),
                        f"{live}({atlas}, {arguments});",
                    )

    def test_promoted_ending_slice_leaves_no_copier_adaptations(self):
        # #276 row 54: the ENDING.PRG results and staff roll are typed C++ (Ending/ClearResultsCore.cpp,
        # Ending/Credits/StaffRollCore.cpp), so no copier text edit may still target them.
        with (Path(__file__).parent / "owned.csv").open(encoding="utf-8", newline="") as table:
            owned = {row["id"]: row["name"] for row in csv.DictReader(table)}
        ending = {fid: name for fid, name in owned.items() if name.startswith("VS::Ending::")}
        self.assertEqual(len(ending), 47)
        for fid, name in ending.items():
            with self.subTest(fid=fid):
                self.assertNotIn(fid, copy_fixups.FIXES)
                final_name = name.removeprefix("VS::")
                self.assertEqual(
                    [row for row, *_ in copy_fixups.REGEX_FIXES if row and re.search(row, final_name)], [])
        adaptations = (Path(__file__).parent / "cpp_adaptations.json").read_text(encoding="utf-8")
        self.assertNotIn("ClearResults.cpp", adaptations)
        self.assertNotIn("StaffRoll.cpp", adaptations)

    def test_promoted_script_actor_opcodes_leave_no_copier_adaptations(self):
        # #276 row 34: the 47 actor opcode handlers and their 4 helpers are typed C++ (Event/ScriptActorCommands.cpp);
        # each opcode table slot holds the C binding at the handler's module-image address.
        with (Path(__file__).parent / "owned.csv").open(encoding="utf-8", newline="") as table:
            owned = {row["id"]: row for row in csv.DictReader(table)
                     if row["path"] == r"Native\Game\src\Event\ScriptActorCommands.cpp"}
        self.assertEqual(len(owned), 51)
        opcodes = {fid: (address, name) for fid, address, name in copy_fixups.EVENT_SCRIPT_ACTOR_OPCODES}
        self.assertEqual(len(opcodes), 47)
        self.assertEqual(set(owned) - set(opcodes), set(copy_fixups.EVENT_SCRIPT_ACTOR_LIVE_BINDINGS))
        for fid, row in owned.items():
            with self.subTest(fid=fid):
                self.assertNotIn(fid, copy_fixups.FIXES)
                final_name = row["name"].removeprefix("VS::")
                self.assertEqual(
                    [r for r, *_ in copy_fixups.REGEX_FIXES if r and re.search(r, final_name)],
                    [r"^Event::(?:Script|Command)"] * 2 if fid in opcodes else [])
                if fid in opcodes:
                    address, name = opcodes[fid]
                    binding = f"Event_{name}Opcode"
                    self.assertEqual(copy_fixups.EVENT_OPCODE_BINDINGS[fid], binding)
                    self.assertIn(("BATTLE/BATTLE.PRG", address, binding), copy_fixups.MODULE_BINDING_ENTRIES)
                    self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[binding],
                                     r"Native\Game\src\Event\ScriptActorOpcodes.h")
                else:
                    live = copy_fixups.OWNED_CALL_BINDINGS[fid]
                    self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[live],
                                     r"Native\Game\include\Event\ScriptActorCommandsLive.hpp")
        adaptations = (Path(__file__).parent / "cpp_adaptations.json").read_text(encoding="utf-8")
        self.assertNotIn("ScriptActors.cpp", adaptations)

    def test_promoted_script_media_opcodes_leave_no_copier_adaptations(self):
        # #276 row 36: the 53 sound, dialog and effect opcode handlers and their 4 helpers are typed C++
        # (Event/Script{Audio,Dialog,Effect}Commands.cpp); each opcode table slot holds the C binding at the
        # handler's module-image address, and the script VM's frame updates call the effect Live helpers.
        cores = {
            r"Native\Game\src\Event\ScriptAudioCommands.cpp": 29,
            r"Native\Game\src\Event\ScriptDialogCommands.cpp": 10,
            r"Native\Game\src\Event\ScriptEffectCommands.cpp": 18,
        }
        with (Path(__file__).parent / "owned.csv").open(encoding="utf-8", newline="") as table:
            owned = {row["id"]: row for row in csv.DictReader(table) if row["path"] in cores}
        self.assertEqual(len(owned), sum(cores.values()))
        opcodes = {fid: (address, name) for fid, address, name in copy_fixups.EVENT_SCRIPT_MEDIA_OPCODES}
        self.assertEqual(len(opcodes), 53)
        headers = {f"Event_{name}Opcode": header for header, rows in copy_fixups.EVENT_SCRIPT_MEDIA_OPCODE_HEADERS
                   for _, _, name in rows}
        self.assertEqual(set(owned) - set(opcodes),
                         set(copy_fixups.EVENT_SCRIPT_EFFECT_LIVE_BINDINGS) | {"F01039"})
        for fid, row in owned.items():
            with self.subTest(fid=fid):
                self.assertNotIn(fid, copy_fixups.FIXES)
                final_name = row["name"].removeprefix("VS::")
                self.assertEqual(
                    [r for r, *_ in copy_fixups.REGEX_FIXES if r and re.search(r, final_name)],
                    [r"^Event::(?:Script|Command)"] * 2 if fid in opcodes else [])
                if fid in opcodes:
                    address, name = opcodes[fid]
                    binding = f"Event_{name}Opcode"
                    self.assertEqual(copy_fixups.EVENT_OPCODE_BINDINGS[fid], binding)
                    self.assertIn(("BATTLE/BATTLE.PRG", address, binding), copy_fixups.MODULE_BINDING_ENTRIES)
                    self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[binding], headers[binding])
                elif fid != "F01039":
                    live = copy_fixups.OWNED_CALL_BINDINGS[fid]
                    self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[live],
                                     r"Native\Game\include\Event\ScriptEffectCommandsLive.hpp")
        # The placement helper's only caller (opcode 0x13) is promoted, so it needs no call binding.
        self.assertNotIn("F01039", copy_fixups.OWNED_CALL_BINDINGS)
        adaptations = (Path(__file__).parent / "cpp_adaptations.json").read_text(encoding="utf-8")
        for generated in ("ScriptAudio.cpp", "ScriptDialog.cpp", "ScriptEffects.cpp"):
            self.assertNotIn(generated, adaptations)

    def test_promoted_battle_targeting_prerequisites_leave_no_copier_adaptations(self):
        # #276 row 31: the 83 targeting, attack prerequisite, battle ability and battle mode functions are typed C++
        # (Battle/{Targeting,AttackPrerequisites,Abilities,BattleMode}.cpp); generated callers reach them through
        # the *Live.hpp bindings and each prerequisite table slot holds a Battle_*Slot C binding.
        cores = {
            r"Native\Game\src\Battle\Targeting.cpp": 26,
            r"Native\Game\src\Battle\AttackPrerequisites.cpp": 43,
            r"Native\Game\src\Battle\Abilities.cpp": 12,
            r"Native\Game\src\Battle\BattleMode.cpp": 2,
        }
        with (Path(__file__).parent / "owned.csv").open(encoding="utf-8", newline="") as table:
            owned = {row["id"]: row for row in csv.DictReader(table) if row["path"] in cores}
        self.assertEqual(len(owned), sum(cores.values()))
        slots = {fid: (address, name) for fid, address, name in copy_fixups.BATTLE_ATTACK_PREREQUISITE_SLOTS}
        self.assertEqual(len(slots), 40)
        live = {fid: (header, name) for header, bindings in copy_fixups.BATTLE_TARGETING_PREREQUISITE_LIVE_HEADERS
                for fid, name in bindings.items()}
        self.assertEqual(len(live), 23)
        names = {row["name"].removeprefix("VS::Battle::") for row in owned.values()}
        for _, pattern, *_ in copy_fixups.REGEX_FIXES:
            for name in names:
                self.assertNotRegex(pattern, rf"\b(?:Battle_|VS::Battle::){name}\b")
        for fid, row in owned.items():
            with self.subTest(fid=fid):
                self.assertNotIn(fid, copy_fixups.FIXES)
                self.assertTrue(row["name"].startswith("VS::Battle::"))
                final_name = row["name"].removeprefix("VS::")
                # Only the family-wide Battle:: rewrites of other promoted helpers apply to these rows.
                self.assertEqual({r for r, *_ in copy_fixups.REGEX_FIXES if r and re.search(r, final_name)},
                                 {r"^Battle::"})
                if fid in slots:
                    address, name = slots[fid]
                    binding = f"Battle_{name}Slot"
                    self.assertIn(("BATTLE/BATTLE.PRG", address, binding), copy_fixups.MODULE_BINDING_ENTRIES)
                    self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[binding],
                                     r"Native\Game\src\Battle\AttackPrerequisites.h")
                    self.assertNotIn(fid, copy_fixups.OWNED_CALL_BINDINGS)
                if fid in live:
                    header, name = live[fid]
                    self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS[fid], f"VS::Battle::{name}")
                    self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[f"VS::Battle::{name}"], header)
        self.assertTrue(set(live) <= set(owned))
        self.assertFalse(set(live) & set(slots))
        adaptations = (Path(__file__).parent / "cpp_adaptations.json").read_text(encoding="utf-8")
        for generated in ("Battle\\\\Targeting.cpp", "Battle\\\\Abilities.cpp", "Battle\\\\AttackPrerequisites.cpp",
                          "Battle\\\\BattleMode.cpp", "Battle_CheckActionHitPrerequisite(",
                          "Battle_CalculateActionRangeSquared("):
            self.assertNotIn(generated, adaptations)

    def test_script_actor_reference_resolver_returns_unhandled_references(self):
        # #303: 573B8.c:156 falls off the end of its switch; the PS1 epilogue returns the reference in v0.
        fixes = copy_fixups.FIXES["F01275"]
        source = ("int Event_ResolveScriptActorReference(u_short arg0)\n{\n    switch (arg0 >> 12) {\n"
                  "    case 2:\n        return arg0;\n    }\n}")
        for old, new, _ in fixes:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertTrue(source.endswith("    }\n    return arg0;\n}"))

    def test_texture_pop_ins_pass_bounded_color_pairs(self):
        copier = object.__new__(copy_to_native.Copier)
        copier.used_fixups = set()
        for namespace, colors in (
            ("Menu::Results::", "Menu_Results_foodChainColors"),
            ("Menu::Results::", "Menu_Results_timeAttackHeadingColors"),
        ):
            with self.subTest(colors=colors):
                row = {"id": "test", "final_name": namespace + "DrawHeading"}
                self.assertEqual(
                    copier.apply_fixups(row, f"Draw(x, &{colors}[1]);"),
                    f"Draw(x, std::span({colors}).subspan(1));",
                )
        row = {"id": "test", "final_name": "Menu::Results::DrawStart"}
        self.assertEqual(
            copier.apply_fixups(row, "Tint(&Menu_Results_timeAttackStartColors[1]);"),
            "Tint(&Menu_Results_timeAttackStartColors[1]);",
        )

    def test_absent_model_effect_targets_empty_slot_without_deferred_proof(self):
        for fixup_id in ("F01433", "F01438"):
            self.assertNotIn(fixup_id, copy_fixups.FIXES)
        # F01425 carries only the #282 refused disc request guard.
        for _, new, why in copy_fixups.FIXES.get("F01425", []):
            self.assertIn("#282", why)
            self.assertNotIn("Deferred", new)
        # #276 row 36: opcode 0xA0 (F01170) is typed C++; a slot reference, or a model no actor has, targets the
        # empty slot and starts, as func_800BFE50 and func_8007BE20 did.
        self.assertNotIn("F01170", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Event\ScriptEffectCommands.cpp").read_text(encoding="utf-8")
        start = core.index("ScriptStep CommandPlayActorEffect(")
        handler = core[start:core.index("\n}\n", start)]
        self.assertIn("in_range && !slot.battle_actor && !slot.occupied &&", handler)
        self.assertIn("(reference_type == kSlotReference || actor == kModelNotFoundSlot);", handler)
        self.assertIn("if (!absent_model && unavailable) {", handler)
        self.assertTrue(handler.rstrip().endswith(
            "host.StartActorEffect(effect, actor);\n  return ScriptStep::kAdvance;"))
        live = (root / r"Native\Game\src\Event\ScriptEffectCommandsLive.cpp").read_text(encoding="utf-8")
        self.assertRegex(live, r"\.occupied = Actors_actorRecords\[actor\] != nullptr \|\|\s+"
                               r"Field_gimmickInstances\[actor\] != nullptr")
        self.assertNotIn("Deferred", handler + live)
        self.assertNotIn("Deferred", "".join(copy_fixups.NATIVE_HELPER_HEADERS))

    def test_promoted_field_camera_slice_leaves_no_copier_adaptations(self):
        # #276 field-camera-background: the camera, room background and weather are typed VS::Field cores
        # (Field/Camera.cpp, Background.cpp, Environment.cpp); the camera core refuses a tile with no authored
        # collision cell itself (CameraHost::CollisionCellFlags), so no copier text edit may still target them.
        with (Path(__file__).parent / "owned.csv").open(encoding="utf-8", newline="") as table:
            owned = {row["id"]: row for row in csv.DictReader(table)}
        paths = {rf"Native\Game\src\Field\{name}.cpp" for name in ("Camera", "Background", "Environment")}
        promoted = {fid: row["name"] for fid, row in owned.items()
                    if row["kind"] == "function" and row["path"] in paths}
        self.assertEqual(len(promoted), 56)
        generic = {row for row, *_ in copy_fixups.REGEX_FIXES if row and re.search(row, "Field::NotPromoted")}
        for fid, name in promoted.items():
            with self.subTest(fid=fid):
                self.assertTrue(name.startswith("VS::Field::"))
                self.assertNotIn(fid, copy_fixups.FIXES)
                self.assertEqual(
                    [row for row, *_ in copy_fixups.REGEX_FIXES
                     if row and row not in generic and re.search(row, name.removeprefix("VS::"))], [])
        adaptations = (Path(__file__).parent / "cpp_adaptations.json").read_text(encoding="utf-8")
        for name in ("Camera", "Background", "Environment"):
            self.assertNotIn(rf"Field\\{name}.cpp", adaptations)
        for fid, name in copy_fixups.FIELD_CAMERA_LIVE_BINDINGS.items():
            self.assertIn(fid, promoted)
            self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[f"VS::Field::{name}"],
                             r"Native\Game\include\Field\CameraLive.hpp")
        self.assertNotIn("F00661", copy_fixups.OWNED_CALL_BINDINGS)
        self.assertNotIn("F00668", copy_fixups.OWNED_CALL_BINDINGS)

    def test_scripted_fall_door_initializes_unassigned_fields(self):
        source = "    Field_MpdDoorRecord destination;\n"
        edits = [edit for edit in copy_fixups.FIXES["F00158"] if "#323" in edit[2]]
        self.assertEqual(len(edits), 1)
        for old, new, _ in edits:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, "    Field_MpdDoorRecord destination{};\n")

    def test_trap_tile_effect_initializes_actor_endpoint_fields(self):
        source = "    Effects_ActionEffectRequest request;\n"
        edits = [edit for edit in copy_fixups.FIXES["F00086"] if "#324" in edit[2]]
        self.assertEqual(len(edits), 1)
        for old, new, _ in edits:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, "    Effects_ActionEffectRequest request{};\n")

    def test_unchecked_cell_readers_read_an_absent_cell(self):
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["VS::Field::LiveCollisionCellOrAbsent"],
            r"Native\Game\include\Field\CollisionMapLive.hpp",
        )
        sources = {
            "F00158": "    Field_MpdTileCell* tile;\n"
                      "        tile = VS::Field::LiveCollisionCellAt(0, 0, floorOverride >> 16);\n"
                      "        tile = VS::Field::LiveCollisionCellAt(position[0],\n",
            "F00159": "    Field_MpdTileCell* temp_s4;\n"
                      "        temp_s4 = VS::Field::LiveCollisionCellAt(0, 0, temp_a2 >> 0x10);\n"
                      "        temp_s4 = VS::Field::LiveCollisionCellAt(arg1->packedTile.target.tileX,\n",
            "F00547": "    if ((VS::Field::LiveCollisionCellAt(var_a0, var_a1, var_a2)->hasRoomTrigger & 1)\n",
            "F00596": "        Field_MpdTileCell* cell;\n"
                      "        cell = VS::Field::LiveCollisionCellAt(object->positionX / (ONE * 128), "
                      "object->positionZ / (ONE * 128), 0);\n",
            "F01308": "            Field_MpdTileCell* room = VS::Field::LiveCollisionCellAt(x, z, 0);\n",
        }
        for function_id, source in sources.items():
            with self.subTest(function_id):
                edits = [edit for edit in copy_fixups.FIXES[function_id] if "#288" in edit[2]]
                self.assertEqual(len(edits), 1 if function_id == "F00547" else 2)
                for old, new, why in edits:
                    self.assertIn(old, source)
                    self.assertIn("#288", why)
                    source = source.replace(old, new)
                self.assertNotIn("LiveCollisionCellAt(", source)
                self.assertIn("VS::Field::LiveCollisionCellOrAbsent(", source)
                self.assertNotRegex(source, r"(?<!const )Field_MpdTileCell\*")

    def test_action_plan_selection_counts_start_at_zero(self):
        source = (copy_to_native.REPO / r"Native\Game\src\Battle\Ai\PlanningWorkspace.cpp").read_text(
            encoding="utf-8")
        self.assertIn("std::array<int, kActionPlanPassCount> selected{};", source)
        self.assertLess(source.index("selected{};"), source.index("if (host.SnapshotStats())"))
        self.assertNotIn("F01716", copy_fixups.FIXES)

    def test_action_plan_enumeration_uses_the_owned_record_binding(self):
        live = (copy_to_native.REPO / r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.cpp").read_text(
            encoding="utf-8")
        workspace = (copy_to_native.REPO / r"Native\Game\src\Battle\Ai\PlanningWorkspaceLive.cpp").read_text(
            encoding="utf-8")
        self.assertIn("EnumerateLiveActionCandidates(&state_, threshold);", live)
        self.assertIn("BuildLiveActorActionPlan(&plan);", workspace)
        self.assertNotIn("Battle_Ai_BuildActorActionPlan(", workspace)
        self.assertIn("ReconcileLiveCompletedActionPlan(request_);", workspace)
        self.assertNotIn("Battle_Ai_ReconcileCompletedActionPlan(", workspace)

    def test_actor_gimmick_read_adapter_preserves_registry_initialization_writes(self):
        source = (
            "Field_gimmickInstances[i] = NULL;\n"
            "value = Field_gimmickInstances[actor_id];\n"
            "if (Field_gimmickInstances[actor_id] == NULL) return -1;"
        )
        matches = [(pattern, replacement) for _, pattern, replacement, _ in copy_fixups.REGEX_FIXES
                   if "Field_FindGimmickForActorSlot" in replacement]
        self.assertEqual(len(matches), 1)
        actual = re.sub(*matches[0], source)
        self.assertIn("Field_gimmickInstances[i] = NULL;", actual)
        self.assertIn("value = Field_FindGimmickForActorSlot(actor_id);", actual)
        self.assertIn("if (Field_FindGimmickForActorSlot(actor_id) == NULL)", actual)

    def test_sfx_reference_carriers_left_the_copier_for_owned_cores(self):
        # #276 audio-control-sfx: the resolver, release, channel-ID and fade carriers are typed in the owned
        # cores (EffectSelector, std::intptr_t), so no generated signature is widened any more.
        owned = self.owned_items()
        for function_id, path in (
            ("F03520", r"Native\Engine\src\Audio\SfxCore.cpp"),
            ("F03203", r"Native\Engine\src\Audio\SoundEffects\SoundEffectProgramsCore.cpp"),
            ("F03150", r"Native\Engine\src\Audio\Control\SoundControlCore.cpp"),
            ("F03108", r"Native\Engine\src\Audio\Control\SoundControlCore.cpp"),
            ("F03112", r"Native\Engine\src\Audio\Control\SoundControlCore.cpp"),
            ("F03116", r"Native\Engine\src\Audio\Control\SoundControlCore.cpp"),
        ):
            with self.subTest(function_id=function_id):
                self.assertNotIn(function_id, copy_fixups.NATIVE_SIGNATURE_REWRITES)
                self.assertNotIn(function_id, copy_fixups.FIXES)
                self.assertEqual(owned[function_id]["path"], path)

    def test_native_pointer_fields_cannot_widen_serialized_records(self):
        copier = object.__new__(copy_to_native.Copier)
        copier.type_by_id = {"T00000": {"role": "OnDisc"}}
        copier.fields = {"T00000.000": {"type": "int"}}
        with (
            mock.patch.object(copy_fixups, "NATIVE_POINTER_FIELDS", {"T00000.000"}),
            self.assertRaisesRegex(ValueError, "requires a runtime field"),
        ):
            copier.load_hazards()

    def test_room_retention_predicate_uses_the_declared_byte_return(self):
        # #276 slice 19b: the predicate is owned; the Live adapter returns 0 or 1 through its declared char, so the
        # native trigger host needs no function-pointer cast fixup.
        self.assertNotIn("F01261", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        self.assertEqual(owned["F00296"]["name"], "VS::Actors::IsActorRetainedAfterDefeat")
        live = (root / r"Native\Game\include\Actors\ActorStateLive.hpp").read_text(encoding="utf-8")
        self.assertIn("char IsLiveActorRetainedAfterDefeat(int actor_id);", live)
        triggers = (root / r"Native\Game\src\Event\TriggersLive.cpp").read_text(encoding="utf-8")
        self.assertIn("Actors::IsLiveActorRetainedAfterDefeat(actor) != 0", triggers)
        self.assertNotIn("int (*)(int)", triggers)

    def test_animation_unwind_uses_the_bound_clip_without_suppressing_failures(self):
        # #276 slice 20c: the unwind is owned; the Live adapter looks the old clip up in the bank the record bound,
        # not the shared slot selected for the next clip, and reports unavailable events with -1.
        self.assertNotIn("F00998", copy_fixups.FIXES)
        self.assertNotIn("Actors_FindBoundAnimationHeader", copy_fixups.NATIVE_HELPER_HEADERS)
        root = Path(__file__).resolve().parents[2]
        self.assertFalse((root / r"Native\Game\src\Actors\AnimationBindings.h").exists())
        live = (root / r"Native\Game\src\Actors\AnimationPoseLive.cpp").read_text(encoding="utf-8")
        start = live.index("int RestartLiveAnimationEvents(Actors_ActorRecord& actor) {")
        restart = live[start:live.index("\n}\n", start)]
        self.assertIn("FindLiveBoundClip(actor)", restart)
        self.assertIn("EventRestart::kUnavailable", restart)
        self.assertIn("return -1;", restart)
        self.assertIn("BankHandle(actor.sharedSequences[slot]) == actor.animationHeader", live)
        # #276 slice 20b: shared-bank retirement is owned; the Live host asks whether the bound clip has events.
        control = (root / r"Native\Game\src\Actors\AnimationControlLive.cpp").read_text(encoding="utf-8")
        self.assertIn("BoundLiveAnimationClip(record)", control)
        self.assertIn("FindClipEvents(*bound).status == ClipEventsStatus::kStarts", control)
        self.assertNotIn("F00279", copy_fixups.FIXES)

    def test_platform_lookup_callers_check_for_a_missing_platform(self):
        root = Path(__file__).resolve().parents[2]
        movement = (root / r"Native\Game\src\Actors\Movement.cpp").read_text(encoding="utf-8")
        collision = (root / r"Native\Game\src\Field\Collision.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Field_FindMovingPlatformById", movement + collision)
        # #276 slice 21 (actors-movement): the tile jump and platform contact are owned; the typed core refuses a
        # missing platform before any actor change (#257).
        for name in ("Actors_BeginActorJumpToTile", "Actors_UpdateActorPlatformContact"):
            with self.subTest(name=name):
                self.assertNotIn(f"\n{name}(", movement)
                self.assertNotIn(f" {name}(", movement)
        core = (root / r"Native\Game\src\Actors\ActorMovement.cpp").read_text(encoding="utf-8")
        for message in ("jump target platform layer {} is not visible", "actor {} rides invisible platform layer {}",
                        "actor {} anchors to invisible platform layer {}"):
            with self.subTest(message=message):
                self.assertIn(message, core)
        # The ledge offset is the typed core (#276): an invisible support platform reports no ledge before the facing
        # changes (#257).
        typed = (root / r"Native\Game\src\Field\ActorCollision.cpp").read_text(encoding="utf-8")
        ledge = typed[typed.index("\nbool CalcActorLedgeOffset("):]
        guard, facing = ledge.index("if (!platform)"), ledge.index("actor.move_facing =")
        self.assertLess(guard, facing)
        self.assertIn('VS_LOG_ERROR("field.platform"', ledge[guard:facing])
        self.assertIn("return false;", ledge[guard:facing])

    def test_platform_placement_uses_an_owned_authored_contact(self):
        # #276 slice 19b: room placement is owned; the typed core asks the host for the platform contact and keeps
        # the floor placement when the platform is missing, instead of the old fixup's abort.
        self.assertNotIn("F00901", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        self.assertEqual(owned["F00901"]["name"], "VS::Actors::PlaceActorInRoom")
        core = (root / r"Native\Game\src\Actors\ActorPlacement.cpp").read_text(encoding="utf-8")
        self.assertIn("host.MovingPlatformContact(layer)", core)
        self.assertIn("is placed on missing moving platform layer", core)
        for legacy in ("abort();", "anchor[", "isHidden =", "State_SetScriptFlag"):
            with self.subTest(legacy=legacy):
                self.assertNotIn(legacy, core)
        live = (root / r"Native\Game\src\Actors\ActorStateLive.cpp").read_text(encoding="utf-8")
        self.assertIn("Field::LiveMovingPlatformPlacement(support_layer)", live)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["VS::Field::LiveMovingPlatformPlacement"],
            r"Native\Game\include\Field\PlatformPlacementLive.hpp",
        )
        scaffold = runpy.run_path(
            str(Path(__file__).resolve().parents[1] / "scaffold" / "gen_solution.py"),
            run_name="scaffold_platform_test",
        )
        self.assertTrue(
            scaffold["needs_c_headers"](
                "VS.Game", r"src\Field\PlatformPlacementLive.cpp"
            )
        )
        self.assertFalse(
            scaffold["needs_c_headers"](
                "VS.Game", r"src\Field\PlatformPlacement.cpp"
            )
        )

    def test_title_stages_clear_game_before_consuming_ending_provenance(self):
        # #276 slice 53: _titleMenu (F03670) is the typed VS::Title::Run, which stages the clear game
        # through its host before clearing the pending flag; TitleMenuLive.cpp binds both.
        self.assertNotIn("F03670", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Title\TitleMenu.cpp").read_text(encoding="utf-8")
        self.assertTrue(core.startswith("// Hand-maintained (tools/port/owned.csv)."))
        self.assertLess(core.index("host.StageClearGame();"), core.index("host.ClearPendingClearSave();"))
        live = (root / r"Native\Game\src\Title\TitleMenuLive.cpp").read_text(encoding="utf-8")
        self.assertIn("void StageClearGame() override { Save_NativeStageClearGame(); }", live)
        self.assertIn("void ClearPendingClearSave() override { Boot_isClearSavePending = 0; }", live)

    def test_owned_native_core_keeps_retained_c_binding_header(self):
        for identifier, c_name, core_name, core_header in (
              ("F00782", "Field_CreateGimmickInstanceEntry", "VS::Field::CreateGimmickInstance",
               r"Native\Game\include\Field\GimmickResources.hpp"),
              ("F00837", "Field_FindGimmickBelowEntry", "VS::Field::FindGimmickBelow",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00838", "Field_GimmickSubtypeEntry", "VS::Field::GimmickSubtype",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00917", "Field_AreGimmicksMovingEntry", "VS::Field::AreGimmicksMoving",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F02468", "Field_MeasureTileStackEntry", "VS::Field::MeasureTileStack",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F02469", "Field_IsGimmickWithinHeightBandEntry", "VS::Field::IsGimmickWithinHeightBand",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00854", "Field_SetGimmickOrientationEntry", "VS::Field::SetGimmickOrientation",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00912", "Field_CanPlaceGimmickAtPointEntry", "VS::Field::CanPlaceGimmickAtPoint",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00836", "Field_DropUnsupportedGimmickStackEntry", "VS::Field::DropUnsupportedGimmickStack",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00908", "Field_FindInteractableGimmickAheadEntry", "VS::Field::FindInteractableGimmickAhead",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00913", "Field_BeginGimmickInteractionEntry", "VS::Field::BeginGimmickInteraction",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00914", "Field_ApplyGimmickInteractionCommandEntry", "VS::Field::ApplyGimmickInteractionCommand",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F01014", "Field_UpdateGimmickReactionAnimationEntry", "VS::Field::UpdateGimmickReactionAnimation",
               r"Native\Game\include\Field\GimmickReactions.hpp"),
              ("F01013", "Field_UpdateGimmickMotionEntry", "VS::Field::UpdateGimmickMotion",
               r"Native\Game\include\Field\GimmickDynamics.hpp"),
              ("F00065", "Field_DisableTrapAtPositionEntry", "VS::Field::DisableTrapAtPosition",
               r"Native\Game\include\Field\InteractionTarget.hpp"),
              ("F00602", "Field_RoomTriggerAtTileEntry", "VS::Field::FindRoomTriggerAtTile",
               r"Native\Game\include\Field\InteractionTarget.hpp"),
              ("F00616", "Field_SetRoomTriggerEnabledEntry", "VS::Field::SetRoomTriggerEnabled",
               r"Native\Game\include\Field\InteractionTarget.hpp"),
              ("F00942", "Field_FindTopGimmickAtTileEntry", "VS::Field::FindTopGimmickAtTile",
               r"Native\Game\include\Field\GimmickPlacement.hpp"),
              ("F00783", "Field_ReleaseGimmickInstanceEntry", "VS::Field::ReleaseGimmickInstance",
               r"Native\Game\include\Field\GimmickResources.hpp"),
              ("F00780", "Field_FindSharedGimmickModelEntry", "VS::Field::FindSharedGimmickModel",
               r"Native\Game\include\Field\GimmickResources.hpp"),
              ("F00781", "Field_SetGimmickPaletteTableEntry", "VS::Field::SetGimmickPaletteTable",
               r"Native\Game\include\Field\GimmickResources.hpp"),
            ("F00620", "Field_MovingPlatformTargetWaypointEntry", "VS::Field::MovingPlatformTargetWaypoint",
             r"Native\Game\include\Field\PlatformPlacement.hpp"),
            ("F00629", "Field_SetPlatformFreezeAppearanceEntry", "VS::Field::SetPlatformFreezeAppearance",
             r"Native\Game\include\Field\PlatformPlacement.hpp"),
            ("F00596", "Field_DrawMovingPlatformShadowsEntry", "VS::Field::DrawMovingPlatformShadows",
             r"Native\Game\include\Field\PlatformPlacement.hpp"),
            ("F00595", "Field_UpdateMovingPlatformsEntry", "VS::Field::UpdateMovingPlatforms",
             r"Native\Game\include\Field\PlatformPlacement.hpp"),
            ("F00597", "Field_CollectMovingPlatformPositionsEntry", "VS::Field::CollectMovingPlatformPositions",
             r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00593", "Field_InitializeMovingPlatformsEntry", "VS::Field::InitializeMovingPlatforms",
               r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00594", "Field_ResetMovingPlatformWaypointEntry", "VS::Field::ResetMovingPlatformWaypoint",
               r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00591", "Field_SetMovingPlatformActorEntry", "VS::Field::SetMovingPlatformActor",
               r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00604", "Field_SetMovingPlatformsPausedEntry", "VS::Field::SetMovingPlatformsPaused",
               r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00605", "Field_FreezeMovingPlatformsEntry", "VS::Field::FreezeMovingPlatforms",
               r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00066", "Field_FreezeRoomGimmicksTemporarilyEntry", "VS::Field::FreezeRoomGimmicksTemporarily",
               r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00547", "Field_CurrentSavePointIdEntry", "VS::Field::CurrentSavePointId",
               r"Native\Game\include\Field\ChestInteractions.hpp"),
              ("F00828", "Field_RefreshAshleyInteractionTileEntry", "VS::Field::RefreshAshleyInteractionTile",
               r"Native\Game\include\Field\ChestInteractions.hpp"),
              ("F01637", "Battle_Ai_RefreshActionCandidateReadinessEntry", "VS::Battle::Ai::RefreshActionCandidateReadiness",
               r"Native\Game\include\Battle\Ai\PlanningPrimitives.hpp"),
              ("F01697", "Battle_Ai_ApplySimulatedActionResultsEntry", "VS::Battle::Ai::ApplySimulatedActionResults",
               r"Native\Game\include\Battle\Ai\PlanningPrimitives.hpp"),
              ("F01698", "Battle_Ai_SimulateActionCandidateEntry", "VS::Battle::Ai::SimulateActionCandidate",
               r"Native\Game\include\Battle\Ai\PlanningPrimitives.hpp"),
              ("F01699", "Battle_Ai_UpdateCandidateHitWeightEntry", "VS::Battle::Ai::UpdateCandidateHitWeight",
               r"Native\Game\include\Battle\Ai\PlanningPrimitives.hpp"),
              ("F01704", "Battle_Ai_EvaluateActionCandidateEntry", "VS::Battle::Ai::EvaluateActionCandidate",
               r"Native\Game\include\Battle\Ai\PlanningPrimitives.hpp"),
              ("F01707", "Battle_Ai_SelectFallbackActionEntry", "VS::Battle::Ai::SelectFallbackAction",
               r"Native\Game\include\Battle\Ai\PlanningPrimitives.hpp"),
              ("F01712", "Battle_Ai_EnumerateActionCandidatesEntry", "VS::Battle::Ai::EnumerateActionCandidates",
               r"Native\Game\include\Battle\Ai\PlanningPrimitives.hpp"),
              ("F01713", "Battle_Ai_AdvancePlanningSimulationEntry", "VS::Battle::Ai::AdvancePlanningSimulation",
               r"Native\Game\include\Battle\Ai\PlanningPrimitives.hpp"),
              ("F01716", "Battle_Ai_BuildActorActionPlanEntry", "VS::Battle::Ai::BuildActorActionPlan",
               r"Native\Game\include\Battle\Ai\PlanningWorkspace.hpp"),
              ("F01717", "Battle_Ai_ReconcileCompletedActionPlanEntry",
               "VS::Battle::Ai::ReconcileCompletedActionPlan",
               r"Native\Game\include\Battle\Ai\PlanningWorkspace.hpp"),
              ("F00619", "Field_SetMovingPlatformVisibilityEntry", "VS::Field::SetMovingPlatformVisibility",
               r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00621", "Field_MovingPlatformDwellTicksEntry", "VS::Field::MovingPlatformDwellTicks",
               r"Native\Game\include\Field\PlatformPlacement.hpp"),
              ("F00067", "Field_ApplyUnlockEffectToRoomObjectEntry", "VS::Field::ApplyUnlockEffectToRoomObject",
               r"Native\Game\include\Field\ChestInteractions.hpp"),
              ("F00581", "Field_ToggleDoorCollisionEntry", "VS::Field::ToggleDoorCollision",
               r"Native\Game\include\Field\RoomSwitches.hpp"),
              ("F00582", "Field_BeginDoorToggleEntry", "VS::Field::BeginDoorToggle",
               r"Native\Game\include\Field\RoomSwitches.hpp"),
              ("F00583", "Field_StartDoorAnimationEntry", "VS::Field::StartDoorAnimation",
               r"Native\Game\include\Field\RoomSwitches.hpp"),
              ("F00586", "Field_FinishDoorAnimationEntry", "VS::Field::FinishDoorAnimation",
               r"Native\Game\include\Field\RoomSwitches.hpp"),
            ("F00580", "Field_InteractiveDoorAtTileEntry", "VS::Field::InteractiveDoorAtTile",
             r"Native\Game\include\Field\RoomSwitches.hpp"),
            ("F00633", "Field_MarkSigilDoorUnlockedEntry", "VS::Field::MarkSigilDoorUnlocked",
             r"Native\Game\include\Field\RoomSwitches.hpp"),
            ("F00634", "Field_IsRoomSigilDoorUnlockedEntry", "VS::Field::RoomSigilDoorUnlockByte",
             r"Native\Game\include\Field\RoomSwitches.hpp"),
            ("F00610", "Field_SceneDoorIdEntry", "VS::Field::SceneDoorId",
             r"Native\Game\include\Field\MinimapScene.hpp"),
            ("F00611", "Field_MarkEntranceDoorOnSceneMapEntry", "VS::Field::MarkEntranceDoorOnSceneMap",
             r"Native\Game\include\Field\MinimapScene.hpp"),
            ("F00552", "Field_StoreEntranceDoorForReloadEntry", "VS::Field::EnteredDoorReloadByte",
             r"Native\Game\include\Field\Transitions.hpp"),
            ("F00577", "Field_SetEntranceDoorRecordEntry", "VS::Field::SetEntranceDoorRecord",
             r"Native\Game\include\Field\Transitions.hpp"),
            ("F00578", "Field_FindEntranceDoorNearTileEntry", "VS::Field::FindEntranceDoorNearTile",
             r"Native\Game\include\Field\Transitions.hpp"),
            ("F00158", "Field_CheckAshleyTileTriggersEntry", "VS::Field::CheckAshleyTileTriggers",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00159", "Field_CheckActorTrapAndFallTriggersEntry", "VS::Field::CheckActorTrapAndFallTriggers",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00153", "Field_TryTriggerFacingTileEventEntry", "VS::Field::TryTriggerFacingTileEvent",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00154", "Field_FindFacingRoomObjectEntry", "VS::Field::FindFacingRoomObject",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00155", "Field_FindFacingUnlootedChestEntry", "VS::Field::FindFacingUnlootedChest",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00152", "Field_TileInFrontOfActorEntry", "VS::Field::TileInFrontOfActor",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00139", "Field_WaitForChestCameraFocusEntry", "VS::Field::WaitForChestCameraFocus",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00140", "Field_StepTrapCameraFocusEntry", "VS::Field::StepTrapCameraFocus",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00142", "Field_IsChestAnimationReadyEntry", "VS::Field::IsChestAnimationReady",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00146", "Field_TrackInteractionCameraToAshleyEntry", "VS::Field::TrackInteractionCameraToAshley",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00135", "Field_TriggerActorTrapEntry", "VS::Field::TriggerActorTrap",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00134", "Field_InteractWithLockedRoomObjectEntry", "VS::Field::InteractWithLockedRoomObject",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00132", "Field_BeginObjectInteractionWaitEntry", "VS::Field::BeginObjectInteractionWait",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00133", "Field_EndObjectInteractionWaitEntry", "VS::Field::EndObjectInteractionWait",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00137", "Field_BeginTrapInteractionEntry", "VS::Field::BeginTrapInteraction",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00141", "Field_DispatchTrapOrChestInteractionEntry", "VS::Field::DispatchTrapOrChestInteraction",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00145", "Field_FinishTrapInteractionEntry", "VS::Field::FinishTrapInteraction",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00138", "Field_BeginChestInteractionEntry", "VS::Field::BeginChestInteraction",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00144", "Field_FinishChestInteractionEntry", "VS::Field::FinishChestInteraction",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00147", "Field_OpenChestContentsEntry", "VS::Field::OpenChestContents",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00148", "Field_UpdateChestContentsSelectionEntry", "VS::Field::UpdateChestContentsSelection",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00136", "Field_TryOpenChestEntry", "VS::Field::TryOpenChest",
             r"Native\Game\include\Field\ChestInteractions.hpp"),
            ("F00606", "Field_BeginSwitchToggleEntry", "VS::Field::BeginSwitchToggle",
             r"Native\Game\include\Field\RoomSwitches.hpp"),
            ("F00570", "Field_ResetGimmickAnimationQueueEntry", "VS::Field::ResetGimmickAnimationQueue",
             r"Native\Game\include\Field\GimmickAnimationQueue.hpp"),
            ("F00585", "Field_UpdateGimmickAnimationQueueEntry", "VS::Field::UpdateGimmickAnimationQueue",
             r"Native\Game\include\Field\GimmickAnimationQueue.hpp"),
            ("F00584", "Field_QueueGimmickAnimationEntry", "VS::Field::QueueGimmickAnimation",
             r"Native\Game\include\Field\GimmickAnimationQueue.hpp"),
            ("F00607", "Field_ToggleRoomSwitchEntry", "VS::Field::ToggleRoomSwitch",
             r"Native\Game\include\Field\RoomSwitches.hpp"),
            ("F00853", "Field_GimmickPlacementEntry", "VS::Field::GimmickPlacement",
             r"Native\Game\include\Field\GimmickPlacement.hpp"),
            ("F01261", "Event_EvaluateRoomEventTriggersEntry", "VS::Event::EvaluateRoomEventTriggers",
             r"Native\Game\include\Event\RoomTriggers.hpp"),
            ("F01276", "Event_EvaluateInterpolationCurveEntry", "VS::Event::EvaluateInterpolationCurve",
             r"Native\Game\include\Event\ScriptVm.hpp"),
        ):
            with self.subTest(identifier=identifier):
                copier = object.__new__(copy_to_native.Copier)
                copier.owned = {identifier: {"kind": "function", "name": core_name,
                                              "header": core_header, "path": "owned"}}
                copier.provider = {}
                copier.public_headers = set()
                copier.item_row = {}
                copier.report = {"owned": []}
                self.assertTrue(copier.take_owned(identifier, c_name))
                self.assertEqual(copier.provider[c_name], copy_fixups.NATIVE_HELPER_HEADERS[c_name])
                self.assertEqual(copier.provider[core_name], core_header)

    def test_event_dispatch_audit_includes_owned_opcode_headers(self):
        manifest = {
            r"Native\Game\src\Event\ScriptVm.h": "generated",
            r"Native\Game\src\Event\ScriptVm.cpp": "not-a-header",
        }
        owned = {
            "F01131": {"header": r"Native\Game\src\Event\WeaponStub.h"},
            "T00001": {"header": r"Native\Game\include\CppOnly.hpp"},
            "F01033": {},
        }
        self.assertEqual(
            check_event_dispatch.event_headers(manifest, owned),
            [
                r"Native\Game\src\Event\ScriptVm.h",
                r"Native\Game\src\Event\WeaponStub.h",
            ],
        )

    def test_event_dispatch_audit_rejects_an_owned_handler_without_a_header(self):
        for row in ({}, {"header": ""}, {"header": " \t "}):
            with (
                self.subTest(row=row),
                self.assertRaisesRegex(ValueError, "F01131.*header"),
            ):
                check_event_dispatch.event_headers({}, {"F01131": row})

    def test_event_dispatch_audit_deduplicates_shared_and_generated_headers(self):
        header = r"Native\Game\src\Event\WeaponStub.h"
        self.assertEqual(
            check_event_dispatch.event_headers(
                {header: "generated"},
                {
                    "F01131": {"header": f" \t{header}\t "},
                    "F01031": {"header": header},
                },
            ),
            [header],
        )
        self.assertEqual(
            check_event_dispatch.event_headers({}, {"F01027": {"header": header}}),
            [copy_fixups.NATIVE_HELPER_HEADERS[copy_fixups.EVENT_OPCODE_BINDINGS["F01027"]]],
        )

    def test_event_dispatch_audit_uses_promoted_opcode_bindings(self):
        binding = copy_fixups.EVENT_OPCODE_BINDINGS["F01130"]
        self.assertEqual(binding, "Event_ScriptSetAshleyBattleModeOpcode")
        self.assertEqual(
            check_event_dispatch.event_headers(
                {}, {"F01130": {"header": r"Native\Game\include\Event\WeaponMode.hpp"}}
            ),
            [r"Native\Game\src\Event\WeaponModeOpcode.h"],
        )
        self.assertEqual(
            check_event_dispatch.opcode_symbol("F01130", "Event::ScriptSetAshleyBattleMode"), binding
        )

    def test_event_dispatch_audit_names_unpromoted_handlers_by_catalog(self):
        self.assertEqual(
            check_event_dispatch.opcode_symbol("F01131", "Event::ScriptWaitForActor"), "Event_ScriptWaitForActor"
        )
        self.assertTrue(set(copy_fixups.EVENT_OPCODE_BINDINGS) <= copy_fixups.EVENT_OPCODE_HANDLERS)
        for binding in copy_fixups.EVENT_OPCODE_BINDINGS.values():
            self.assertIn(binding, copy_fixups.NATIVE_HELPER_HEADERS)

    def test_effect_origin_aliases_the_primary_request_position(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\DATA_LEDGER.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            row = next(item for item in csv.DictReader(stream)
                       if item["id"] == "G00532")
        self.assertIn("view-of:G00529+0xE0", row["notes"])
        self.assertIn("Effects_ActionEffectRequest.position", row["notes"])
        self.assertEqual(row["size"], "8")
        self.assertEqual(row["type"], "SVECTOR[1]")
        header = (root / r"Native\Game\src\Effects\Runtime.h").read_text(
            encoding="utf-8"
        )
        source = (root / r"Native\Game\src\Effects\Runtime.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "#define Effects_effectOrigin ((SVECTOR*)(&Effects_actionRequest.position))",
            header,
        )
        self.assertNotRegex(
            source, r"SVECTOR\s+Effects_effectOrigin\s*\["
        )

    def test_texture_binding_receives_its_payload_extent(self):
        # #276 slice 38: the package walk is owned; its typed binder hands each block its payload extent.
        self.assertNotIn("F01626", copy_fixups.FIXES)
        owned = self.owned_items()
        self.assertEqual(
            owned["F01626"]["path"], r"Native\Game\src\Effects\EffectResources.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        live = (root / r"Native\Game\src\Effects\EffectResourcesLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("BindLiveEffectTextureTables(", live)
        self.assertEqual(
            owned["F01606"]["path"],
            r"Native\Game\src\Effects\TextureBindings.cpp",
        )

    def test_retired_execution_hooks_have_no_production_consumers(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            catalog = {row["id"]: row for row in csv.DictReader(stream)}
        for identity in ("F03575", "F00766"):
            row = catalog[identity]
            self.assertEqual(row["copy"], "no")
            self.assertIn("retired-", row["notes"])
            name = row["final_name"].replace("::", "_")
            for path in (root / "Native").rglob("*"):
                if path.suffix not in (".c", ".cpp", ".h", ".hpp"):
                    continue
                if "Tests" in path.parts:
                    continue
                text = audit_assembly_gaps.mask_literals(
                    path.read_text(encoding="utf-8")
                )
                self.assertNotRegex(text, rf"\b{re.escape(name)}\b", str(path))

    def test_scripted_navigation_uses_full_native_records_and_explicit_aliases(self):
        root = Path(__file__).resolve().parents[2]
        owned = self.owned_items()
        for identity, address, live in (
            ("F01805", 0x800E527C, "SetLiveActorNavigationOverride"),
            ("F01806", 0x800E5308, "UpdateLiveActorNavigationOverrides"),
        ):
            self.assertEqual(
                owned[identity]["path"],
                r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
            )
            self.assertIn(
                ("BATTLE/BATTLE.PRG", address, f"VS::Battle::Ai::{live}"),
                copy_fixups.MODULE_BINDING_ENTRIES,
            )
        self.assertEqual(
            native_views.NATIVE_FIELD_ALIASES["T00532"]["scriptedTargetXZ"], 0x160
        )
        core = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("std::bit_cast<std::int32_t>(squared)", core)
        source = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitivesLive.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("std::construct_at(storage)", source)
        self.assertIn(
            "VS::Core::Memory::AllocateLiveFromHighEnd(sizeof(Battle_Ai_ActorAiState))",
            "".join(source.split()),
        )
        self.assertIn("auto* destination = state;", source)
        self.assertIn("destination->scriptedTargetXZ = target;", source)
        self.assertIn("VS::Battle::Ai::FreeLiveActorNavigationOverride(route_->actorId)", source)
        self.assertIn("SaveLiveMovementRandomState();", source)
        self.assertNotIn("actor[3]", source)

    def test_route_availability_keeps_original_shift_mutation_and_native_bounds(self):
        root = Path(__file__).resolve().parents[2]
        self.assertEqual(
            self.owned_items()["F01694"]["path"],
            r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
        )
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800DC638, "VS::Battle::Ai::UpdateLiveRouteGateAvailability"),
            copy_fixups.MODULE_BINDING_ENTRIES,
        )
        core = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("first_bit = 1u << (first_bit & kVariableShiftMask)", core)
        self.assertIn("second_bit = 1u << (second_bit & kVariableShiftMask)", core)
        self.assertIn("first route word depends on an", core)
        self.assertIn("original stack-register value", core)
        source = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitivesLive.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("kOriginalPlatformCapacity = 13", source)
        self.assertIn("auxiliary == Field_roomData.section6", source)
        self.assertIn("Field_roomData.header.section6Length >= kRouteAuxiliaryBytes", source)
        self.assertIn("std::memcpy(location, &word, sizeof word)", source)

    def test_gimmick_physics_occupancy_calls_cross_the_explicit_native_view_boundary(self):
        old, new, reason = copy_fixups.FIXES["F01013"][0]
        self.assertEqual(old, "VS::Battle::Ai::AddAiTileOccupant(object);")
        self.assertIn("(const Field_GimmickInstance*)object", new)
        self.assertIn("native owning Gimmick", reason)

    def test_occupancy_entries_use_typed_native_grids_and_shared_contact_helper(self):
        root = Path(__file__).resolve().parents[2]
        owned = self.owned_items()
        for identity in ("F01826", "F01827"):
            self.assertEqual(
                owned[identity]["path"],
                r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
            )
        self.assertEqual(
            owned["F01836"]["path"], r"Native\Game\src\Battle\Ai\ActorCleanup.cpp"
        )
        self.assertEqual(
            owned["F01836"]["name"], "VS::Battle::Ai::ReleaseActorAiContactOccupancy"
        )
        # The module pointer table stores 0x800E6EB0, so it resolves to the Live binding.
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800E6EB0, "VS::Battle::Ai::ReleaseLiveActorAiContactOccupancy"),
            copy_fixups.MODULE_BINDING_ENTRIES,
        )
        self.assertNotIn(
            ("BATTLE/BATTLE.PRG", 0x800E6EB0, "F01836"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        for address, live in ((0x800E6898, "AddLiveAiTileOccupant"),
                              (0x800E68A0, "RemoveLiveAiTileOccupant")):
            self.assertIn(
                ("BATTLE/BATTLE.PRG", address, f"VS::Battle::Ai::{live}"),
                copy_fixups.MODULE_BINDING_ENTRIES,
            )
        grid = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("(placement.layer >> 2U)", grid)
        self.assertIn("offset > kOccupancyMapBytes - kOccupancyCellBytes", grid)
        self.assertIn("offset % kOccupancyCellBytes != 0", grid)
        self.assertIn("static_cast<int>(grid.OccupancyHeight(x, z)) + delta", grid)
        self.assertEqual(grid.count("ChangeOccupancy(grid, placement,"), 2)
        live = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitivesLive.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("const Field_GimmickInstance* gimmick", live)
        cleanup = (root / r"Native\Game\src\Battle\Ai\ActorCleanup.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("kContactCommands = 0x4800", cleanup)
        self.assertIn("if ((*support & kSupportLayerBits) != 0) return true;", cleanup)
        self.assertIn("host.ClearContactOccupancy(static_cast<int>(contact))", cleanup)
        live = (root / r"Native\Game\src\Battle\Ai\ActorCleanupLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("ClearLiveAiContactOccupancy(contact);", live)
        self.assertIn("model.supportLayer |", live)

    def test_ashley_alliance_uses_native_callback_storage_without_shifting_views(self):
        self.assertEqual(
            native_views.NATIVE_FIELD_ALIASES["T00373"],
            {"allianceReactionCount": 0x126, "temporaryAshleyAlliance": 0x13},
        )
        self.assertEqual(
            native_views.NATIVE_FIELD_ALIASES["T00392"]["savedDecisionCallback"],
            0x6C,
        )
        self.assertNotIn(0x6C, native_views.AI_POINTERS)
        reaction = native_views.rebase(
            "T00373",
            "    unsigned char usesRoomActionDelay;\n"
            "    char reserved89[164];\n",
            [],
        )
        self.assertIn("u_char temporaryAshleyAlliance;", reaction)
        self.assertIn("char reserved89Prefix[0x9D];", reaction)
        self.assertIn("u_char allianceReactionCount;", reaction)
        settings = native_views.rebase("T00392", "    char reserved6C[40];\n", [])
        self.assertIn("char reserved6C[40];", settings)
        self.assertIn(
            "int (*savedDecisionCallback)(Battle_Ai_ActorAiState*);", settings
        )
        with self.assertRaisesRegex(ValueError, "reaction counter storage"):
            native_views.rebase("T00373", "unsigned char usesRoomActionDelay;\n", [])
        with self.assertRaisesRegex(ValueError, "saved decision callback slot"):
            native_views.rebase("T00392", "char reserved6C[4];\n", [])

    def test_ashley_alliance_ownership_and_source_integer_comparison(self):
        root = Path(__file__).resolve().parents[2]
        owned = self.owned_items()
        for identity in ("F01681", "F01682"):
            self.assertEqual(
                owned[identity]["path"], r"Native\Game\src\Battle\Ai\Relations.cpp"
            )
        source = (root / r"Native\Game\src\Battle\Ai\Relations.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("unsigned{actor.ReactionCount()} + 1", source)
        self.assertIn(
            "actor.SetReactionCount(static_cast<std::uint8_t>(reactions));", source
        )
        self.assertIn("if (reactions < kRequiredReactions) return;", source)
        self.assertNotIn("VS_UNPORTED", source)
        binding = (root / r"Native\Game\src\Battle\Ai\RelationsLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("UpdateLiveEngagementApproachBehavior", binding)
        self.assertIn("settings_.savedDecisionCallback = nullptr;", binding)
        self.assertNotIn("VS_UNPORTED", binding)

    def test_plg074_reuses_identical_code_without_merging_overlay_state(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as handle:
            catalog = {row["id"]: row for row in csv.DictReader(handle)}
        addresses = (
            0x800F98F8, 0x800FA7F0, 0x800FB414, 0x800FBBB0, 0x800FBDC8,
            0x800FC28C, 0x800FC404, 0x800FC590, 0x800FCFEC, 0x800FD534,
        )
        for offset, address in enumerate(addresses):
            identity = f"F{1986 + offset:05}"
            with self.subTest(id=identity):
                row = catalog[identity]
                self.assertEqual(row["copied"], "x")
                if offset in (2, 8):
                    # #46/#271: PLG074's origin and emitter are typed VS::Effects code; the overlay image
                    # stores neither address, so no module entry resolves to them.
                    self.assertNotIn(
                        ("EFFECT/PLG074.BIN", address, identity),
                        copy_fixups.MODULE_FUNCTION_ENTRIES,
                    )
                    self.assertEqual(
                        audit_assembly_gaps.canonical_id(row, catalog), identity
                    )
                    self.assertTrue(self.owned_items()[identity]["name"].startswith("VS::Effects::"))
                    self.assertNotIn("native-template:", row["notes"])
                elif offset in (0, 1, 9):
                    # #276: the tiled and open rings and the rotating surface are typed template instances; the
                    # overlay image's entry addresses resolve to PLG073's live slot bindings, which PLG074 shares
                    # byte for byte.
                    live = {
                        0: "UpdateLivePlg073TiledRing",
                        1: "UpdateLivePlg073Ring",
                        9: "UpdateLivePlg073RotatingSurface",
                    }[offset]
                    self.assertIn(
                        ("EFFECT/PLG074.BIN", address, f"VS::Effects::Templates::{live}"),
                        copy_fixups.MODULE_BINDING_ENTRIES,
                    )
                    self.assertNotIn(
                        ("EFFECT/PLG074.BIN", address, identity),
                        copy_fixups.MODULE_FUNCTION_ENTRIES,
                    )
                    self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS[identity],
                                     f"VS::Effects::Templates::{live}")
                    self.assertEqual(
                        audit_assembly_gaps.canonical_id(row, catalog),
                        f"F{1975 + offset:05}",
                    )
                elif offset in (3, 4, 5, 6, 7):
                    # #276: the trail stages are typed VS::Effects::Plg073 stages; PLG074's emitter calls them
                    # directly with its own cursor and no image stores their addresses.
                    self.assertNotIn(
                        ("EFFECT/PLG074.BIN", address, identity),
                        copy_fixups.MODULE_FUNCTION_ENTRIES,
                    )
                    self.assertTrue(self.owned_items()[identity]["name"].startswith("VS::Effects::Plg073::Trail"))
                    self.assertEqual(
                        audit_assembly_gaps.canonical_id(row, catalog),
                        f"F{1975 + offset:05}",
                    )
        cursor = next(
            blob for blob in copy_fixups.MODULE_DATA_BLOBS
            if blob[4] == "Content_plg074TrailEmissionCounter"
        )
        self.assertEqual(cursor[:4], ("EFFECT/PLG074.BIN", 0x577C, 0x800FEF7C, 4))
        source = (root / r"Native\Game\src\Effects\TrailEmitterLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("Content_plg074TrailEmissionCounter[0]", source)
        self.assertNotIn("Effects_Plg073_trailEmissionCounter", source)

    def room_draw_sources(self):
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Engine\src\Render\RoomDraw.cpp").read_text(
            encoding="utf-8"
        )
        live = (root / r"Native\Engine\src\Render\RoomDrawLive.cpp").read_text(
            encoding="utf-8"
        )
        return core, live

    def assert_room_draw_owned_without_entry(self, identity, name):
        owned = self.owned_items()[identity]
        self.assertEqual(owned["path"], r"Native\Engine\src\Render\RoomDraw.cpp")
        self.assertEqual(owned["name"], f"VS::Render::{name}")
        # #247: the typed room renderer has no guest address; nothing resolves to it.
        self.assertFalse(
            [entry for entry in copy_fixups.MODULE_FUNCTION_ENTRIES
             if entry[2] == identity]
        )

    def test_room_packet_linking_reuses_shared_depth_resolution_and_native_handles(self):
        self.assert_room_draw_owned_without_entry("F00752", "LinkRoomPolygonByDepth")
        core, live = self.room_draw_sources()
        self.assertEqual(core.count("LinkRoomPolygonByDepth("), 2)
        self.assertIn("ResolveRoomPolygonDepth(projected_depth, flags, clip)", core)
        self.assertIn("biased < 0", core)
        self.assertNotIn("Render_frameScratch", core)
        self.assertIn(
            "AddPrim(static_cast<u_long*>(Render_frameScratch.orderingTable) + depth,",
            flat(live),
        )
        relaxed = (
            Path(__file__).resolve().with_name("relaxed_sources.txt")
            .read_text(encoding="utf-8").split()
        )
        self.assertNotIn(r"Native\Engine\src\Render\RoomDraw.cpp", relaxed)

    def test_room_subdivision_entries_share_the_existing_bounded_recursion(self):
        for identity, name in (
            ("F00748", "SubdivideRoomTriangle"),
            ("F00749", "SubdivideRoomQuad"),
        ):
            self.assert_room_draw_owned_without_entry(identity, name)
        core, _ = self.room_draw_sources()
        self.assertEqual(
            flat(core).count(
                "if (levels < 0 || levels > kRoomMaximumSubdivisionLevels) {"
            ),
            2,
        )
        self.assertIn("std::ranges::copy(vertices, captured.begin());", core)
        self.assertIn(".SubdivideTriangle(captured, levels, captured_material);", core)
        self.assertIn(".SubdivideQuad(captured, levels, captured_material);", core)

    def test_room_coordinate_decoder_uses_the_live_owned_polygon_pipeline(self):
        self.assert_room_draw_owned_without_entry("F00746", "DecodeRoomPolygonVertices")
        core, _ = self.room_draw_sources()
        self.assertEqual(core.count("DecodeRoomPolygonVertices("), 2)
        self.assertIn("const std::uint32_t shift = coordinate_shift & kCoordinateShiftMask;", core)
        self.assertIn("ProjectAndDecodeRoomPolygon(", core)
        self.assertIn("vertex.position = QuadCorner(record, triangle.positions[0], shift);", core)

    def test_fog_and_depth_cue_are_owned_by_the_typed_render_cores(self):
        owned = self.owned_items()
        for identity, name, binding in (
            ("F03404", "VS::Render::ApplyDepthCueColor", "VS::Render::ApplyLiveDepthCueColor"),
            ("F03408", "VS::Render::SetFogNearDistance", "VS::Render::SetLiveFogNearDistance"),
        ):
            self.assertEqual(owned[identity]["name"], name)
            self.assertEqual(
                owned[identity]["path"], r"Native\Engine\src\Render\DepthCue.cpp"
            )
            self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS[identity], binding)
            self.assertNotIn(identity, copy_fixups.NATIVE_FUNCTION_REDIRECTS)
            self.assertFalse(
                [entry for entry in copy_fixups.MODULE_FUNCTION_ENTRIES
                 if entry[2] == identity]
            )
        for sdk in ("DpqColor", "SetFogNear"):
            self.assertNotIn(sdk, copy_to_native.GTE_FUNCS)
        self.assertEqual(copy_to_native.SDK_PROMOTED_ROWS["DpqColor"], "F03404")
        self.assertEqual(copy_to_native.SDK_PROMOTED_ROWS["SetFogNear"], "F03408")

    def test_source_root_and_normalization_keep_the_wide_native_api_separate(self):
        owned = self.owned_items()
        for identity in ("F03405", "F03411", "F03412", "F03413", "F03414"):
            self.assertEqual(
                owned[identity]["path"],
                r"Native\Engine\src\Core\Math\ScalarMath.cpp",
            )
            self.assertNotIn(identity, copy_fixups.NATIVE_FUNCTION_REDIRECTS)
        for identity in ("F03411", "F03412", "F03413", "F03414"):
            self.assertEqual(
                owned[identity]["name"], "VS::Core::Math::NormalizeVector"
            )
        self.assertFalse(
            [entry for entry in copy_fixups.MODULE_FUNCTION_ENTRIES
             if entry[2] in {"F03405", "F03411", "F03412", "F03413", "F03414"}]
        )

    def test_resident_rotation_pair_is_owned_by_the_typed_kernel(self):
        owned = self.owned_items()
        for identity in ("F03399", "F01017"):
            self.assertEqual(
                owned[identity]["name"],
                "VS::Core::Math::LookupRotationSineCosine",
            )
            self.assertEqual(
                owned[identity]["path"],
                r"Native\Engine\src\Core\Math\ScalarMath.cpp",
            )
            self.assertNotIn(identity, copy_fixups.NATIVE_FUNCTION_REDIRECTS)
        self.assertNotIn(
            ("SLUS_010.40", 0x80041104, "F03399"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )

    def test_core_math_promotions_bind_every_generated_caller_shape(self):
        owned = self.owned_items()
        for identity in copy_to_native.SDK_PROMOTED_ROWS.values():
            self.assertIn(identity, owned)
            self.assertTrue(
                owned[identity]["name"].startswith(("VS::Core::Math::", "VS::Render::"))
            )
        for identity, binding in copy_fixups.OWNED_CALL_BINDINGS.items():
            with self.subTest(identity=identity):
                self.assertIn(identity, owned)
                self.assertIn(binding, copy_fixups.NATIVE_HELPER_HEADERS)
                header = copy_fixups.NATIVE_HELPER_HEADERS[binding]
                retained_c = {
                    "F00782": ("Field_CreateGimmickInstanceEntry", r"Native\Game\src\Field\GimmickResourcesLive.h"),
                    "F00837": ("Field_FindGimmickBelowEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00838": ("Field_GimmickSubtypeEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00917": ("Field_AreGimmicksMovingEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F02468": ("Field_MeasureTileStackEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F02469": ("Field_IsGimmickWithinHeightBandEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00854": ("Field_SetGimmickOrientationEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00912": ("Field_CanPlaceGimmickAtPointEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00836": ("Field_DropUnsupportedGimmickStackEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00908": ("Field_FindInteractableGimmickAheadEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00913": ("Field_BeginGimmickInteractionEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00914": ("Field_ApplyGimmickInteractionCommandEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F01014": ("Field_UpdateGimmickReactionAnimationEntry", r"Native\Game\src\Field\GimmickReactionsLive.h"),
                    "F01013": ("Field_UpdateGimmickMotionEntry", r"Native\Game\src\Field\GimmickDynamicsLive.h"),
                    "F00065": ("Field_DisableTrapAtPositionEntry", r"Native\Game\src\Field\InteractionTargetLive.h"),
                    "F00602": ("Field_RoomTriggerAtTileEntry", r"Native\Game\src\Field\InteractionTargetLive.h"),
                    "F00616": ("Field_SetRoomTriggerEnabledEntry", r"Native\Game\src\Field\InteractionTargetLive.h"),
                    "F00942": ("Field_FindTopGimmickAtTileEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F00783": ("Field_ReleaseGimmickInstanceEntry", r"Native\Game\src\Field\GimmickResourcesLive.h"),
                    "F00780": ("Field_FindSharedGimmickModelEntry", r"Native\Game\src\Field\GimmickResourcesLive.h"),
                    "F00781": ("Field_SetGimmickPaletteTableEntry", r"Native\Game\src\Field\GimmickResourcesLive.h"),
                    "F00620": ("Field_MovingPlatformTargetWaypointEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00629": ("Field_SetPlatformFreezeAppearanceEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00596": ("Field_DrawMovingPlatformShadowsEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00595": ("Field_UpdateMovingPlatformsEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00597": ("Field_CollectMovingPlatformPositionsEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00593": ("Field_InitializeMovingPlatformsEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00594": ("Field_ResetMovingPlatformWaypointEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00591": ("Field_SetMovingPlatformActorEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00604": ("Field_SetMovingPlatformsPausedEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00605": ("Field_FreezeMovingPlatformsEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00066": ("Field_FreezeRoomGimmicksTemporarilyEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00547": ("Field_CurrentSavePointIdEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00828": ("Field_RefreshAshleyInteractionTileEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F01637": ("Battle_Ai_RefreshActionCandidateReadinessEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01697": ("Battle_Ai_ApplySimulatedActionResultsEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01698": ("Battle_Ai_SimulateActionCandidateEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01699": ("Battle_Ai_UpdateCandidateHitWeightEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01704": ("Battle_Ai_EvaluateActionCandidateEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01707": ("Battle_Ai_SelectFallbackActionEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01712": ("Battle_Ai_EnumerateActionCandidatesEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01713": ("Battle_Ai_AdvancePlanningSimulationEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01716": ("Battle_Ai_BuildActorActionPlanEntry", r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F01717": ("Battle_Ai_ReconcileCompletedActionPlanEntry",
                               r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.h"),
                    "F00619": ("Field_SetMovingPlatformVisibilityEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00621": ("Field_MovingPlatformDwellTicksEntry", r"Native\Game\src\Field\PlatformPlacementLive.h"),
                    "F00067": ("Field_ApplyUnlockEffectToRoomObjectEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00581": ("Field_ToggleDoorCollisionEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00582": ("Field_BeginDoorToggleEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00583": ("Field_StartDoorAnimationEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00586": ("Field_FinishDoorAnimationEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00580": ("Field_InteractiveDoorAtTileEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00633": ("Field_MarkSigilDoorUnlockedEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00634": ("Field_IsRoomSigilDoorUnlockedEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00610": ("Field_SceneDoorIdEntry", r"Native\Game\src\Field\MinimapSceneLive.h"),
                    "F00611": ("Field_MarkEntranceDoorOnSceneMapEntry", r"Native\Game\src\Field\MinimapSceneLive.h"),
                    "F00552": ("Field_StoreEntranceDoorForReloadEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00577": ("Field_SetEntranceDoorRecordEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00578": ("Field_FindEntranceDoorNearTileEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00158": ("Field_CheckAshleyTileTriggersEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00159": ("Field_CheckActorTrapAndFallTriggersEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00153": ("Field_TryTriggerFacingTileEventEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00154": ("Field_FindFacingRoomObjectEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00155": ("Field_FindFacingUnlootedChestEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00152": ("Field_TileInFrontOfActorEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00139": ("Field_WaitForChestCameraFocusEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00140": ("Field_StepTrapCameraFocusEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00142": ("Field_IsChestAnimationReadyEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00146": ("Field_TrackInteractionCameraToAshleyEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00135": ("Field_TriggerActorTrapEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00134": ("Field_InteractWithLockedRoomObjectEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00132": ("Field_BeginObjectInteractionWaitEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00133": ("Field_EndObjectInteractionWaitEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00141": ("Field_DispatchTrapOrChestInteractionEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00137": ("Field_BeginTrapInteractionEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00145": ("Field_FinishTrapInteractionEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00138": ("Field_BeginChestInteractionEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00144": ("Field_FinishChestInteractionEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00147": ("Field_OpenChestContentsEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00148": ("Field_UpdateChestContentsSelectionEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00136": ("Field_TryOpenChestEntry", r"Native\Game\src\Field\ChestInteractionsLive.h"),
                    "F00606": ("Field_BeginSwitchToggleEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00570": ("Field_ResetGimmickAnimationQueueEntry", r"Native\Game\src\Field\GimmickAnimationQueueLive.h"),
                    "F00585": ("Field_UpdateGimmickAnimationQueueEntry", r"Native\Game\src\Field\GimmickAnimationQueueLive.h"),
                    "F00584": ("Field_QueueGimmickAnimationEntry", r"Native\Game\src\Field\GimmickAnimationQueueLive.h"),
                    "F00607": ("Field_ToggleRoomSwitchEntry", r"Native\Game\src\Field\RoomSwitchesLive.h"),
                    "F00853": ("Field_GimmickPlacementEntry", r"Native\Game\src\Field\GimmickPlacementLive.h"),
                    "F01261": ("Event_EvaluateRoomEventTriggersEntry", r"Native\Game\src\Event\RoomTriggerEntryLive.h"),
                    "F01276": ("Event_EvaluateInterpolationCurveEntry", r"Native\Game\src\Event\InterpolationCurveLive.h"),
                }
                if identity in retained_c:
                    self.assertEqual((binding, header), retained_c[identity])
                else:
                    self.assertTrue(binding.startswith("VS::"), binding)
                    self.assertTrue(header.endswith("Live.hpp"), header)
        root = Path(__file__).resolve().parents[2]
        for binding, header in copy_fixups.NATIVE_HELPER_HEADERS.items():
            if not binding.startswith(("VS::Core::Math::", "VS::Render::")):
                continue
            with self.subTest(binding=binding):
                name = binding.rsplit("::", 1)[-1]
                self.assertRegex(
                    (root / header).read_text(encoding="utf-8"),
                    rf"\b{name}\(",
                )
        for stem in ("Vector", "Matrix", "Interpolation", "Trig"):
            self.assertFalse(
                (root / rf"Native\Engine\src\Core\Math\{stem}.h").exists(),
                f"{stem}.h is a retired generated C ABI",
            )

    def test_promoted_distance_slice_has_typed_callers_and_no_address_entries(self):
        owned = self.owned_items()
        for rid, name in {
            "F01771": "SquaredTileDistance",
            "F01772": "SquaredTileDistanceFromComponents",
            "F01773": "SquaredDistanceWithHeightOffsets",
            "F01774": "SquaredVectorLength",
        }.items():
            self.assertEqual(owned[rid]["name"], f"VS::Core::Math::{name}")
            self.assertEqual(owned[rid]["path"], r"Native\Engine\src\Core\Math\Distance.cpp")
            self.assertEqual(
                owned[rid]["header"], r"Native\Engine\include\Core\Math\Distance.hpp"
            )
        self.assertFalse(
            [entry for entry in copy_fixups.MODULE_FUNCTION_ENTRIES
             if entry[2] in {"F01771", "F01772", "F01773", "F01774"}]
        )
        root = Path(__file__).resolve().parents[2]
        table = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        for address in ("0x800E45F4u", "0x800E4604u", "0x800E4624u", "0x800E4660u"):
            self.assertNotIn(address, table)
        self.assertNotIn("Core_Math_Squared", table)
        ai = root / r"Native\Game\src\Battle\Ai"
        caller = (ai / "GroundNavigation.cpp").read_text(encoding="utf-8")
        self.assertIn("UnpackTileCoordinate(navigator.destination)", caller)
        # F01640 reads both positions as typed Position3 values.
        perception = (ai / "ActorPerception.cpp").read_text(encoding="utf-8")
        self.assertIn("Core::Math::SquaredDistanceWithHeightOffsets(\n          observer.position", perception)
        self.assertNotIn("(short*)", perception)
        for path in ai.glob("*.cpp"):
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("Core_Math_SquaredVectorLength", text, path.name)
            self.assertNotIn("Core_Math_SquaredTileDistance", text, path.name)
            self.assertNotIn("Core_Math_SquaredDistanceWithHeightOffsets", text, path.name)
        # F01659 keeps record-derived height separate from battle-head position.
        execution = (ai / "ActionExecution.cpp").read_text(encoding="utf-8")
        binding = (ai / "ActionExecutionLive.cpp").read_text(encoding="utf-8")
        self.assertIn("Core::Math::SquaredDistanceWithHeightOffsets(", execution)
        self.assertIn("state.position, position.value(), state.point_height, height", execution)
        self.assertIn("Core::Math::PositionOf(Battle_actors[0]->model->position)", binding)
        self.assertNotIn("(short*)", execution)
        # Navigation.cpp's distance users (F01710, F01740), Movement.cpp's
        # (F01748, F01749) and OrbitingBoss.cpp's (F01848) moved to typed cores.
        for name in ("Navigation", "Movement", "OrbitingBoss"):
            text = (ai / f"{name}.cpp").read_text(encoding="utf-8")
            self.assertNotIn("VS::Core::Math::Squared", text, name)
        for name in ("TargetApproach", "BossOrbit"):
            text = (ai / f"{name}.cpp").read_text(encoding="utf-8")
            self.assertIn('#include "Core/Math/Distance.hpp"', text, name)

    def test_native_terrain_envelope_calls_the_existing_builder_without_guest_stack(self):
        self.assertEqual(
            self.owned_items()["F01832"]["path"],
            r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
        )
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800E6B4C, "VS::Battle::Ai::BuildLiveAiTerrainHeightsWithWorkspace"),
            copy_fixups.MODULE_BINDING_ENTRIES,
        )
        root = Path(__file__).resolve().parents[2]
        source = (
            root / r"Native\Game\src\Battle\Ai\NavigationPrimitivesLive.cpp"
        ).read_text(encoding="utf-8")
        bodies = audit_assembly_gaps.definitions(
            source, {"BuildLiveAiTerrainHeightsWithWorkspace"}
        )
        self.assertEqual(len(bodies), 1)
        body = bodies[0].body
        self.assertIn("LiveSpecialTerrainBuilder builder;", body)
        self.assertIn("BuildAiTerrainHeightsWithWorkspace(builder)", body)
        self.assertIn("BuildLiveAiSpecialTerrainHeights();", source)
        self.assertNotIn("Battle_Ai_BuildAiSpecialTerrainHeights", source)
        self.assertNotIn("AllocateFromHighEnd", body)
        self.assertNotIn("Core_ScratchBase", body)

    def test_frame_update_rows_bind_module_addresses_to_the_live_frame_update(self):
        owned = self.owned_items()
        for identity in ("F01765", "F01800", "F01819", "F01824", "F01825"):
            self.assertEqual(owned[identity]["path"], r"Native\Game\src\Battle\Ai\FrameUpdate.cpp")
            self.assertEqual(owned[identity]["header"], r"Native\Game\include\Battle\Ai\FrameUpdate.hpp")
        root = Path(__file__).resolve().parents[2]
        pointers = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        for address, binding in (
            (0x800E65DC, "VS::Battle::Ai::CacheLiveActorAiWorkingState"),
            (0x800E6828, "VS::Battle::Ai::InitializeLiveActorLocomotionMode"),
            (0x800E685C, "VS::Battle::Ai::SetLiveActorVerticalMovementSteps"),
        ):
            self.assertIn(("BATTLE/BATTLE.PRG", address, binding), copy_fixups.MODULE_BINDING_ENTRIES)
            self.assertIn(binding, copy_fixups.NATIVE_HELPER_HEADERS)
            self.assertIn(f'0x{address:08X}u, 0u, (void*)&{binding},', pointers)
        loop = (root / r"Native\Game\src\Boot\GameplayLoopLive.cpp").read_text(encoding="utf-8")
        self.assertIn("VS::Battle::Ai::RunLiveActorAiUpdateStages();", loop)
        self.assertNotIn("Battle_Ai_RunActorAiUpdateStages", loop)
        generated = (root / r"Native\Game\src\Boot\GameLoop.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Battle_Ai_RunActorAiUpdateStages", generated)

    def test_segment_trace_owns_typed_geometry_and_the_live_boolean_consumer(self):
        root = Path(__file__).resolve().parents[2]
        scaffold = runpy.run_path(str(root / r"tools\scaffold\gen_solution.py"))
        self.assertTrue(
            scaffold["needs_c_headers"]("VS.Game", r"src\Field\SegmentCollisionLive.cpp")
        )
        self.assertFalse(
            scaffold["needs_c_headers"]("VS.Game", r"src\Field\SegmentCollision.cpp")
        )
        for identity in ("F01833", "F01834"):
            self.assertEqual(
                self.owned_items()[identity]["path"],
                r"Native\Game\src\Field\SegmentCollision.cpp",
            )
        for address, identity, binding in (
            (0x800E6BA0, "F01833", "VS::Field::IsLivePointOutsideSolidVolumes"),
            (0x800E6C34, "F01834", "VS::Field::TraceLiveActorAndTerrainCollision"),
        ):
            self.assertNotIn(
                ("BATTLE/BATTLE.PRG", address, identity),
                copy_fixups.MODULE_FUNCTION_ENTRIES,
            )
            self.assertIn(
                ("BATTLE/BATTLE.PRG", address, binding),
                copy_fixups.MODULE_BINDING_ENTRIES,
            )
            self.assertIn(binding, copy_fixups.NATIVE_HELPER_HEADERS)
        live = (copy_to_native.REPO / "Native/Game/src/Battle/ActionPreviewLive.cpp").read_text(encoding="utf-8")
        core = (copy_to_native.REPO / "Native/Game/src/Battle/ActionPreview.cpp").read_text(encoding="utf-8")
        self.assertIn("return definition.resetsHitResults != 0;", live)
        self.assertIn("Field::TraceLiveActorAndTerrainCollision(", live)
        self.assertIn("? std::optional{std::bit_cast<ActionPoint>(impact)}", live)
        self.assertIn("if (!impact) return false;", core)

    def test_screen_warp_routes_through_the_live_binding(self):
        root = Path(__file__).resolve().parents[2]
        scaffold = runpy.run_path(str(root / r"tools\scaffold\gen_solution.py"))
        self.assertTrue(
            scaffold["needs_c_headers"](
                "VS.Game", r"src\Effects\ScreenDistortion\RadialWarpLive.cpp"
            )
        )
        self.assertEqual(
            self.owned_items()["F02458"]["path"],
            r"Native\Game\src\Effects\ScreenDistortion\RadialWarp.cpp",
        )
        self.assertNotIn(
            ("GIM/SCREFF2.PRG", 0x800F9BD8, "F02458"),
            copy_fixups.MODULE_FUNCTION_ENTRIES,
        )
        self.assertFalse(
            (root / r"Native\Game\src\Effects\ScreenDistortion\RadialWarp.h").exists()
        )
        caller = (root / r"Native\Game\src\Effects\ScreenEffects.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "VS::Effects::ScreenDistortion::WarpLivePolygonVertices();", caller
        )
        self.assertIn(
            '#include "Effects/ScreenDistortion/RadialWarpLive.hpp"', caller
        )
        table = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("0x800F9BD8u", table)
        core = (
            root / r"Native\Game\src\Effects\ScreenDistortion\RadialWarp.cpp"
        ).read_text(encoding="utf-8")
        self.assertNotIn("Legacy/", core)
        self.assertNotIn("u_long", core)
        live = (
            root / r"Native\Game\src\Effects\ScreenDistortion\RadialWarpLive.cpp"
        ).read_text(encoding="utf-8")
        self.assertNotIn("Core_ScratchBase", live)
        self.assertNotIn("0x1F800", live)
        self.assertIn("Core_Math_kArcTangentTable", live)

    def test_screen_effect_defaults_own_native_state_and_use_typed_access(self):
        root = Path(__file__).resolve().parents[2]
        scaffold = runpy.run_path(str(root / r"tools\scaffold\gen_solution.py"))
        self.assertTrue(
            scaffold["needs_c_headers"]("VS.Game", r"src\Effects\ScreenEffectState.cpp")
        )
        owned = self.owned_items()
        self.assertEqual(owned["G00512"]["kind"], "global")
        self.assertEqual(
            owned["G00512"]["path"], r"Native\Game\src\Effects\ScreenEffectState.cpp"
        )
        for identity in ("F01363", "F01389", "F01390"):
            self.assertEqual(len(copy_fixups.FIXES[identity]), 1)
        self.assertIn(
            "Effects_screenEffectDefaults.radialWarpArg0",
            copy_fixups.FIXES["F01363"][0][1],
        )
        self.assertEqual(
            copy_fixups.FIXES["F01390"][0][1], "&Effects_screenEffectDefaults"
        )
        with (root / r"Docs\ledgers\DATA_LEDGER.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            row = next(row for row in csv.DictReader(stream) if row["id"] == "G00512")
        self.assertEqual(
            (row["address"], row["size"], row["type"]),
            ("0x800F4ED8", "16", "short[8]"),
        )
        symbols = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(
            encoding="utf-8"
        )
        for address, size, name in (
            ("800F4ED8", 16, "Effects_screenEffectDefaults"),
            ("800F1D08", 4, "Field_matchingDoorRecords"),
        ):
            self.assertIn(
                f'{{"BATTLE/BATTLE.PRG", 0x{address}u, {size}u, '
                f"(void*)&{name}, sizeof {name}, 0u, 0u}}",
                symbols,
            )

    def test_actor_shadows_import_mesh_data_and_own_the_production_draw_entry(self):
        expected = {
            "Content_battleShadowVertices": (0x81268, 0x800E9A68, 72),
            "Content_battleShadowFaces": (0x812D4, 0x800E9AD4, 96),
        }
        for name, values in expected.items():
            blob = next(item for item in copy_fixups.MODULE_DATA_BLOBS if item[4] == name)
            self.assertEqual(blob[:4], ("BATTLE/BATTLE.PRG", *values))
        owned = self.owned_items()
        self.assertEqual(
            owned["F01268"]["path"], r"Native\Engine\src\Render\ModelGeometry.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Actors\ModelDrawing.cpp").read_text(
            encoding="utf-8"
        )
        live = (root / r"Native\Game\src\Actors\ModelDrawingLive.cpp").read_text(
            encoding="utf-8"
        )
        # The shadow follows a failed projection but only for the actor's own draw.
        self.assertIn(
            "if (role == ModelDrawRole::kSelf && host.CastsShadow()) host.DrawShadow();",
            core,
        )
        self.assertIn("return actor_.instance.soundEnabled != 0;", live)
        self.assertIn("VS::Render::DrawLiveActorGroundShadow(actor_,", live)

    def test_model_geometry_reads_models_only_through_recorded_extents(self):
        owned = self.owned_items()
        for identity in ("F01265", "F01266", "F01267", "F01268", "F01269", "F01270"):
            self.assertEqual(
                owned[identity]["header"],
                r"Native\Engine\include\Render\ModelGeometry.hpp",
            )
        root = Path(__file__).resolve().parents[2]

        def generated(path):
            return (root / path).read_text(encoding="utf-8")

        gimmicks = generated(r"Native\Game\src\Field\Gimmicks.cpp")
        gimmick_resources = generated(r"Native\Game\src\Field\GimmickResourcesLive.cpp")
        rendering = generated(r"Native\Game\src\Actors\ModelRenderingLive.cpp")
        assets = " ".join(generated(r"Native\Content\src\ActorAssets.cpp").split())
        assets_live = " ".join(generated(r"Native\Content\src\ActorAssetsLive.cpp").split())
        # Each loader records the bytes it owns and forgets them before release (#269).
        self.assertEqual(owned["F00782"]["path"], r"Native\Game\src\Field\GimmickResources.cpp")
        self.assertEqual(owned["F00783"]["path"], r"Native\Game\src\Field\GimmickResources.cpp")
        self.assertIn("VS::Render::RecordLiveModelExtent(record.model, model_bytes);", gimmick_resources)
        self.assertIn("VS::Render::ForgetLiveModelExtent(record_->model);", gimmick_resources)
        self.assertLess(gimmick_resources.index("VS::Render::ForgetLiveModelExtent(record_->model);"),
                gimmick_resources.index("VS::Core::Memory::FreeLiveFromLowEnd(record_->model);"))
        self.assertIn("host.RecordModelCopy(model.geometry, original, kWeaponGeometryBytes);", assets)
        self.assertIn("host.ForgetModelGeometry(model.geometry);", assets)
        self.assertIn("Render::RecordLiveModelCopy(Pointer(destination), Pointer(source),", assets_live)
        self.assertIn("Render::ForgetLiveModelExtent(Pointer(model));", assets_live)
        for call in (
            "Render::ProjectLiveGimmickModel(*object)",
            "Render::DrawLiveGimmickGouraudPolygonsWithOverrides(*object);",
            "Render::DrawLiveGimmickGouraudPolygons(*object);",
            "Render::ProjectLiveWeaponModel(weapon)",
            "Render::DrawLiveWeaponFlatPolygonsWithOverrides(weapon, color);",
            "Render::DrawLiveWeaponFlatPolygons(weapon, color);",
        ):
            self.assertIn(call, rendering)
        for source in (gimmicks, gimmick_resources, rendering, assets):
            self.assertNotIn("ProjectModelVertices(", source)
            self.assertNotIn("Render_DrawModel", source)

    def test_room_euler_uses_the_source_staged_kernel_and_shared_octant_pair(self):
        owned = self.owned_items()
        for identity in ("F00761", "F00762", "F01017"):
            self.assertEqual(
                owned[identity]["path"], r"Native\Engine\src\Core\Math\ScalarMath.cpp"
            )
        self.assertNotIn("F01017", copy_fixups.NATIVE_FUNCTION_REDIRECTS)
        core, live = self.room_draw_sources()
        self.assertIn("view, Math::BuildEulerRotationMatrix(rotation));", flat(core))
        for source in (core, live):
            self.assertNotIn("Core_Gte_RotMatrixYXZ(", source)

    def test_core_idle_entry_is_owned_and_matrix_entries_reuse_native_leaves(self):
        self.assertNotIn("F03568", copy_fixups.NATIVE_FUNCTION_REDIRECTS)
        owned = self.owned_items()
        self.assertEqual(owned["F03568"]["path"], r"Native\Engine\src\Core\Runtime.cpp")
        self.assertEqual(owned["F03568"]["name"], "VS::Core::IdleInstruction")
        self.assertEqual(
            owned["F03406"]["path"], r"Native\Engine\src\Core\Math\ScalarMath.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Core\Math\ScalarMath.cpp").read_text(
            encoding="utf-8"
        )
        self.assertEqual(source.count("return MultiplyMatrixIntoRight(left, right);"), 3)

    def test_weapon_override_reads_a_typed_matrix_member_not_native_guest_offset(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01269"]["path"], r"Native\Engine\src\Render\ModelGeometry.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\ModelGeometryLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("weapon.boneWorldMatrices[kWeaponPaletteMatrix]", source)
        self.assertIn("weapon.ownerMotionState};", source)
        core = (root / r"Native\Engine\src\Render\ModelGeometry.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("ModelAppearanceMaterial(overrides.appearance_state)", core)

    def test_gimmick_override_uses_the_existing_model_kernel_and_typed_state(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01270"]["path"], r"Native\Engine\src\Render\ModelGeometry.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\ModelGeometry.cpp").read_text(
            encoding="utf-8"
        )
        live = (root / r"Native\Engine\src\Render\ModelGeometryLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("return {object.palettePulse.kind, object.motionState};", live)
        self.assertIn("override ? ModelAppearanceMaterial(state) : 0", source)

    def test_room_tint_pass_shares_the_explicit_native_color_contract(self):
        for identity, name in (
            ("F00769", "TintRoomGeometryPrimitives"),
            ("F00770", "TintRoomPrimitiveColor"),
        ):
            self.assert_room_draw_owned_without_entry(identity, name)
        core, live = self.room_draw_sources()
        self.assertEqual(core.count("TintRoomPrimitiveColor("), 2)
        self.assertIn("tint.strength, static_cast<std::int16_t>(tint.red * kTintOffsetScale),", flat(core))
        self.assertIn("return RoomTintStatus::kTruncated;", core)
        self.assertNotIn("Field_RoomTint", core)
        self.assertIn("const Field_RoomTint& tint", flat(live))
        root = Path(__file__).resolve().parents[2]
        self.assertFalse((root / r"Native\Engine\src\Render\RoomDraw.h").exists())

    def test_room_light_triangle_shares_the_existing_owned_sampler_kernel(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F00768"]["path"], r"Native\Engine\src\Render\RoomLighting.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Engine\src\Render\RoomLighting.cpp").read_text(
            encoding="utf-8"
        )
        self.assertEqual(owned["F00768"]["name"], "VS::Render::InterpolateRoomLightTriangle")
        self.assertEqual(owned["F00767"]["name"], "VS::Render::SampleRoomLighting")
        self.assertEqual(source.count("InterpolateRoomLightTriangle("), 2)
        self.assertIn("return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(area));", source)
        self.assertNotIn("Legacy/", source)

    def test_script_weapon_mode_owns_its_original_opcode_and_named_wait_state(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01130"]["path"], r"Native\Game\src\Event\WeaponMode.cpp"
        )
        self.assertEqual(owned["F01130"]["name"], "VS::Event::ScriptSetAshleyBattleMode")
        self.assertEqual(owned["F01130"]["header"], r"Native\Game\include\Event\WeaponMode.hpp")
        self.assertIn(
            ("BATTLE/BATTLE.PRG", 0x800B9DB4, "Event_ScriptSetAshleyBattleModeOpcode"),
            copy_fixups.MODULE_BINDING_ENTRIES,
        )
        root = Path(__file__).resolve().parents[2]
        vm = (root / r"Native\Game\src\Event\ScriptVm.cpp").read_text(encoding="utf-8")
        self.assertIn('#include "Event/WeaponModeOpcode.h"', vm)
        self.assertIn("Event_ScriptSetAshleyBattleModeOpcode,", vm)
        self.assertNotIn("VS::Event::ScriptSetAshleyBattleMode", vm)
        pointers = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertIn(
            '{"BATTLE/BATTLE.PRG", 0x800B9DB4u, 0u, (void*)&Event_ScriptSetAshleyBattleModeOpcode, 0u, 0u, 0u}',
            pointers,
        )
        core = (root / r"Native\Game\src\Event\WeaponMode.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Legacy/", core)
        self.assertNotIn("u_char", core)
        with (root / r"Docs\ledgers\DATA_LEDGER.csv").open(
            encoding="utf-8-sig", newline=""
        ) as handle:
            row = next(row for row in csv.DictReader(handle) if row["id"] == "G00473")
        self.assertEqual(row["final_name"], "Event::weaponTransitionPending")
        self.assertEqual(row["confidence"], "high")
        with (root / r"tools\naming\sym_out\Game_Event\globals.csv").open(
            encoding="utf-8-sig", newline=""
        ) as handle:
            proposal = next(
                row for row in csv.DictReader(handle) if row["id"] == "G00473"
            )
        for field in ("final_name", "purpose", "confidence", "notes"):
            self.assertEqual(proposal[field], row[field])
        with (root / r"Docs\ledgers\TRACE_QUEUE.csv").open(
            encoding="utf-8-sig", newline=""
        ) as handle:
            self.assertNotIn("G00473", {item["id"] for item in csv.DictReader(handle)})
        source = (root / r"Native\Game\src\Event\ScriptVmLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "Event_weaponTransitionPending = static_cast<char>(value);", source
        )
        self.assertNotIn("Event_reservedResetByte", source)
        self.assertNotIn("Event_reservedResetByte", vm)
        control = (root / r"Native\Game\src\Event\ScriptControl.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("host.SetWeaponTransitionPending(false);", control)

    def test_owned_function_catalog_matches_the_registered_native_definition(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8-sig", newline=""
        ) as handle:
            rows = list(csv.DictReader(handle))
        owned = self.owned_items()
        rows = [
            row
            for row in rows
            if row["id"] in owned
            and owned[row["id"]]["kind"] == "function"
            and row["copy"] == "yes"
        ]
        self.assertTrue(rows)
        definitions, paths = audit_assembly_gaps.native_definitions(rows, owned)
        for row in rows:
            with self.subTest(id=row["id"]):
                if audit_assembly_gaps.template_row(row, owned):
                    status, _ = audit_assembly_gaps.template_status(row)
                    self.assertIn(
                        status, ("template-instance", "stub", "partial-unported")
                    )
                else:
                    bodies = audit_assembly_gaps.native_bodies(
                        definitions[paths[row["id"]]], row, owned
                    )
                    self.assertEqual(len(bodies), 1)
                    status = audit_assembly_gaps.native_status(bodies[0].body)
                    self.assertIn(status, ("implemented", "stub", "partial-unported"))
                expected = (
                    "x" if status in ("implemented", "template-instance") else "stub"
                )
                self.assertEqual(
                    (row["final_location"], row["copied"]),
                    (owned[row["id"]]["path"], expected),
                )

    def test_orbit_constructor_shares_the_existing_owned_teardown(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01845"]["path"],
            r"Native\Game\src\Battle\Ai\ActorCleanup.cpp",
        )
        self.assertEqual(owned["F01845"]["name"], "VS::Battle::Ai::AllocateOrbitingBossState")

    def test_hostility_transition_is_owned_with_existing_reaction_lifecycle(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01695"]["path"],
            r"Native\Game\src\Battle\Ai\ActorCleanup.cpp",
        )
        self.assertEqual(owned["F01695"]["name"], "VS::Battle::Ai::MakeTargetHostile")

    def test_dynamic_tile_marker_is_owned_with_existing_tile_primitives(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01828"]["path"],
            r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
        )
        self.assertEqual(owned["F01828"]["name"], "VS::Battle::Ai::MarkAiTileDynamicObstacle")

    def test_room_screen_bounds_share_the_original_predicate(self):
        self.assert_room_draw_owned_without_entry("F00756", "SetRoomPolygonScreenBounds")
        core, _ = self.room_draw_sources()
        # Whole records and subdivision leaves share one on-screen predicate.
        self.assertEqual(
            flat(core).count("if (!IsRoomPolygonOnScreen(polygon.bounds, clip_.screen) ||"),
            2,
        )
        self.assertIn(
            "clip.screen = {wrap(bounds.left), wrap(bounds.top), wrap(bounds.right), wrap(bounds.bottom)};",
            flat(core),
        )
        self.assertIn("return Math::WrapSigned16(static_cast<std::uint32_t>(edge));", core)

    def test_room_far_depth_owns_both_packet_emission_paths(self):
        self.assert_room_draw_owned_without_entry("F00754", "SetRoomPolygonFarClip")
        core, _ = self.room_draw_sources()
        self.assertEqual(core.count("biased >= clip.far_depth"), 1)
        # Whole records and subdivision leaves both resolve depth through it.
        self.assertEqual(
            core.count("ResolveRoomPolygonDepth(polygon.depth, flags, clip_)"), 1
        )
        self.assertEqual(core.count("LinkRoomPolygonByDepth("), 2)
        self.assertIn(
            "clip.far_depth = Math::WrapSigned16(static_cast<std::uint32_t>(depth));",
            core,
        )

    def test_native_pointer_sort_is_owned_and_shared_with_ai_ranking(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01829"]["path"], r"Native\Engine\src\Core\PointerSort.cpp"
        )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("Core::SortPointersByDescendingKey(ranked)", source)
        self.assertNotIn("Core_SortPointersByDescendingKey", source)
        self.assertNotIn("SortCandidatePointers(entries.first(", source)

    def test_promoted_pointer_sort_has_no_original_address_entry(self):
        owned = self.owned_items()
        self.assertEqual(owned["F01829"]["name"], "VS::Core::SortPointersByDescendingKey")
        self.assertEqual(
            owned["F01829"]["header"], r"Native\Engine\include\Core\PointerSort.hpp"
        )
        root = Path(__file__).resolve().parents[2]
        table = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertNotIn("SortPointersByDescendingKey", table)
        self.assertNotIn("0x800E6974u", table)

    def test_ai_lifecycle_leaves_share_the_native_persistence_projection(self):
        owned = self.owned_items()
        for identifier, name in (
            ("F01820", "VS::Battle::Ai::PackActorAiPersistenceFlags"),
            ("F01831", "VS::Battle::Ai::ClearActorAiNavigationReset"),
            ("F01835", "VS::Battle::Ai::InitializeActorAiExtensionHook"),
        ):
            self.assertEqual(owned[identifier]["name"], name)
            self.assertEqual(
                owned[identifier]["path"],
                r"Native\Game\src\Battle\Ai\ActorCleanup.cpp",
            )
        root = Path(__file__).resolve().parents[2]
        snapshot = (root / r"Native\Game\src\Save\RuntimeSnapshot.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "found->aiFlags = VS::Battle::Ai::PackLiveActorAiPersistenceFlags(slot);",
            snapshot,
        )
        self.assertNotIn("relation.previousClassification <<", snapshot)

    def test_every_native_redirect_retains_its_module_symbol_binding(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            rows = {row["id"]: row for row in csv.DictReader(stream)}
        source = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(
            encoding="utf-8"
        )
        for identifier, (target, _) in copy_fixups.NATIVE_FUNCTION_REDIRECTS.items():
            if target.startswith("VS::"):
                # Promoted C++ targets are reached only by name; no address resolves.
                self.assertNotIn(target.rsplit("::", 1)[-1] + ",", source)
                continue
            name = rows[identifier]["final_name"].replace("::", "_")
            self.assertTrue(
                f"(void*)&{name}," in source,
                f"Native alias {identifier} must retain its original module binding",
            )

    def test_fixed_trig_entries_are_owned_by_the_typed_scalar_kernel(self):
        owned = self.owned_items()
        for identifier, name in (
            ("F00764", "VS::Core::Math::Cosine"),
            ("F00765", "VS::Core::Math::Sine"),
            ("F02405", "VS::Core::Math::Sine"),
            ("F03400", "VS::Core::Math::Cosine"),
            ("F03401", "VS::Core::Math::Sine"),
        ):
            self.assertEqual(owned[identifier]["name"], name)
            self.assertEqual(
                owned[identifier]["path"],
                r"Native\Engine\src\Core\Math\ScalarMath.cpp",
            )
            self.assertNotIn(identifier, copy_fixups.NATIVE_FUNCTION_REDIRECTS)
        root = Path(__file__).resolve().parents[2]
        self.assertFalse((root / r"Native\Engine\src\Core\Math\Trig.h").exists())

    def test_tile_translation_callers_preserve_the_full_actor_id_abi(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Actors\Movement.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn(
            "Actors_TranslateActorToTileAtRate",
            source,
            "Tile-translation callers must use the int actor-ID Live adapter",
        )
        # #276 slice 21 (actors-movement): the teleport, cancel and travel callers are owned and translate the typed
        # motion record directly, so no actor ID narrows on the way.
        self.assertNotIn("TranslateLiveActorToTileAtRate", source)
        core = (root / r"Native\Game\src\Actors\ActorMovement.cpp").read_text(encoding="utf-8")
        self.assertEqual(
            " ".join(core.split()).count("TranslateActorToTileAtRate(host.Terrain(), "), 3
        )
        header = (root / r"Native\Game\include\Actors\ActorMotionLive.hpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("void TranslateLiveActorToTileAtRate(int actor_id,", header)

    def test_navigation_leaves_own_typed_entries_and_forward_the_actor_id(self):
        owned = self.owned_items()
        for identifier, name in (
            ("F01801", "VS::Battle::Ai::ActorNavigationOverrideStatus"),
            ("F01802", "VS::Battle::Ai::HasActorNavigationState"),
            ("F01803", "VS::Battle::Ai::CopyActorNavigationOverrideState"),
        ):
            self.assertEqual(owned[identifier]["name"], name)
            self.assertEqual(
                owned[identifier]["path"],
                r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
            )
        root = Path(__file__).resolve().parents[2]
        # #276 row 21: the scripted-path host polls the override for its actor.
        caller = (root / r"Native\Game\src\Actors\ActorMotionLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("Battle::Ai::LiveActorNavigationOverrideStatus(id_);", caller)
        self.assertNotIn("LiveActorNavigationOverrideStatus();", caller)

    def test_vignette_producers_publish_one_encoded_ordering_word(self):
        for identifier, head, packet in (
            ("F02461", "arg3", "arg2"),
            ("F02462", "temp_s4", "var_s2"),
        ):
            fixes = copy_fixups.FIXES[identifier]
            old = "\n".join(item[0] for item in fixes)
            new = "\n".join(item[1] for item in fixes)
            self.assertIn(f"void** {head}", old)
            self.assertIn(f"u_long* {head}", new)
            self.assertIn(f"(*{head} & 0xFFFFFF)", new)
            self.assertIn(f"*{head} = LegacyGpu_Handle({packet});", new)
            self.assertNotIn(f"LegacyGpu_Handle(*{head})", new)
            self.assertNotIn("uintptr_t", new)

    @staticmethod
    def owned_items():
        with (
            Path(__file__)
            .resolve()
            .with_name("owned.csv")
            .open(encoding="utf-8", newline="") as stream
        ):
            return {row["id"]: row for row in csv.DictReader(stream)}

    def test_relocation_entries_own_complete_existing_motion_requests(self):
        owned = self.owned_items()
        names = {
            "F04184": ("Actors_BeginTeleport", "BeginTeleport", "BeginLiveTeleport"),
            "F04185": ("Actors_BeginTileTravel", "BeginTileTravel", "BeginLiveTileTravel"),
            "F04186": ("Actors_BeginTimedTravel", "BeginTimedTravel", "BeginLiveTimedTravel"),
        }
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Actors\Movement.cpp").read_text(
            encoding="utf-8"
        )
        # The command queue (row 21's commands phase) owns the callers now.
        commands = (root / r"Native\Game\src\Actors\ActorCommandsLive.cpp").read_text(
            encoding="utf-8"
        )
        for identifier, (c_name, core, live) in names.items():
            self.assertEqual(owned[identifier]["name"], f"VS::Actors::{core}")
            self.assertEqual(
                owned[identifier]["path"], r"Native\Game\src\Actors\MotionRequests.cpp"
            )
            self.assertNotIn(f"void {c_name}(", source)
            self.assertNotIn(c_name, source)
            self.assertNotIn(c_name, commands)
            self.assertIn(f"{live}(actor_id, target, facing);", commands)
            self.assertEqual(
                copy_fixups.NATIVE_HELPER_HEADERS[f"VS::Actors::{live}"],
                r"Native\Game\include\Actors\MotionRequestsLive.hpp",
            )

    def test_actor_sound_transition_owns_both_catalog_occurrences_and_callers(self):
        owned = self.owned_items()
        for identifier in ("F00995", "F01007"):
            self.assertEqual(
                owned[identifier]["name"], "VS::Actors::StepSoundTransition"
            )
            self.assertEqual(
                owned[identifier]["path"], r"Native\Game\src\Actors\SoundOverrides.cpp"
            )
        root = Path(__file__).resolve().parents[2]
        source = (root / r"Native\Game\src\Actors\ModelRenderingLive.cpp").read_text(
            encoding="utf-8"
        )
        # The room pass's host reaches the transition for the actor and for a
        # frozen actor's companion through one override.
        self.assertIn("UpdateActorSoundTransition(*actor);", source)
        self.assertIn('#include "Actors/SoundTransitionBinding.hpp"', source)
        self.assertNotIn("soundEffect.parameter -= step;", source)
        header = (root / r"Native\Engine\src\Audio\ActorSound.h").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("int Audio_UpdateActorSoundTransition()", header)

    def test_movement_commands_write_through_the_actor_table_adapter(self):
        owned = self.owned_items()
        self.assertEqual(owned["F04180"]["name"], "VS::Actors::SetDirectMovement")
        self.assertEqual(
            owned["F01025"]["name"], "VS::Actors::QueueNavigationTileTarget"
        )
        root = Path(__file__).resolve().parents[2]
        adapter = re.compile(r"VS::Actors::SetDirectMovement\((?:VS::Actors::)?ActorRecordMovementRequests\(\), ")
        # #276 slice 21 (actors-movement): the jump redirect's host owns the movement-module caller.
        for path in (
            r"Native\Game\src\Actors\ActorMovementLive.cpp",
            r"Native\Game\src\Boot\GameplayLoopLive.cpp",
        ):
            source = " ".join((root / path).read_text(encoding="utf-8").split())
            self.assertIn('#include "Actors/MovementCommand.hpp"', source)
            self.assertRegex(source, adapter)
            self.assertNotIn("Actors_SetDirectMovement(", source)
            self.assertNotIn("Actors_QueueNavigationTileTarget(", source)
        commands = " ".join(
            (root / r"Native\Game\src\Actors\ActorCommandsLive.cpp")
            .read_text(encoding="utf-8")
            .split()
        )
        self.assertIn('#include "Actors/MovementCommand.hpp"', commands)
        self.assertIn(
            "QueueNavigationTileTarget(ActorRecordMovementRequests(), ", commands
        )
        self.assertNotIn("Actors_QueueNavigationTileTarget(", commands)

    def test_condition_snapshot_binds_an_explicit_planner_row_span(self):
        owned = self.owned_items()
        self.assertEqual(
            owned["F01689"]["name"], "VS::Battle::Ai::CaptureActorConditionTable"
        )
        root = Path(__file__).resolve().parents[2]
        planning = (root / r"Native\Game\src\Battle\Ai\ActionPlanning.cpp").read_text(
            encoding="utf-8"
        )
        live = (root / r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn('#include "Battle/Ai/ConditionSnapshotBinding.hpp"', planning)
        self.assertIn('#include "Battle/Ai/ConditionSnapshotBinding.hpp"', live)
        self.assertIn("CaptureBattleActorConditions(rows_);", live)
        self.assertIn(
            "std::array<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows_{};",
            live,
        )
        self.assertNotIn("Battle_Ai_CaptureActorConditionTable(", planning + live)
        pointers = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("0x800DC424u", pointers)

    def test_fallback_selection_migrates_the_owned_workspace_caller(self):
        root = Path(__file__).resolve().parents[2]
        workspace = (root / r"Native\Game\src\Battle\Ai\PlanningWorkspaceLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('#include "Battle/Ai/PlanningPrimitivesLive.hpp"', workspace)
        self.assertIn("SelectLiveFallbackAction(&plan);", workspace)
        self.assertNotIn("Battle_Ai_SelectFallbackAction(", workspace)

    def test_planning_stats_lifecycle_uses_owned_policies_and_reset_binding(self):
        owned = self.owned_items()
        root = Path(__file__).resolve().parents[2]
        planning = (root / r"Native\Game\src\Battle\Ai\ActionPlanning.cpp").read_text(
            encoding="utf-8"
        )
        header = (root / r"Native\Game\src\Battle\Ai\ActionPlanning.h").read_text(
            encoding="utf-8"
        )
        bindings = (root / r"Native\Game\src\Battle\Ai\PlanningPrimitivesLive.cpp").read_text(
            encoding="utf-8"
        )
        host = bindings.split("class LiveActorActionPlan final", 1)[1].split(
            "class LiveTargeting final", 1)[0]
        for identifier, operation in (("F01714", "Snapshot"), ("F01715", "Restore")):
            core = f"VS::Battle::Ai::{operation}PlanningActorStats"
            live = f"VS::Battle::Ai::{operation}LivePlanningActorStats"
            with self.subTest(identifier=identifier):
                self.assertEqual(owned[identifier]["name"], core)
                self.assertEqual(
                    owned[identifier]["path"],
                    r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp",
                )
                self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS[identifier], live)
                self.assertEqual(
                    copy_fixups.NATIVE_HELPER_HEADERS[live],
                    r"Native\Game\include\Battle\Ai\PlanningPrimitivesLive.hpp",
                )
                self.assertIn(f"{operation}LivePlanningActorStats()", host)
                self.assertNotIn(f"Battle_Ai_{operation}PlanningActorStats", planning + header)
        reset = (root / r"Native\Game\src\Battle\Ai\RoomState.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("VS::Battle::Ai::InvalidateLivePlanningStatsSnapshot();", reset)

    def test_collision_samples_read_the_live_room_map_as_bytes(self):
        owned = self.owned_items()
        self.assertEqual(owned["F00772"]["name"], "VS::Field::SampleCollisionHeightCode")
        root = Path(__file__).resolve().parents[2]
        collision = (root / r"Native\Game\src\Field\Collision.cpp").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("Field_SampleCollisionHeightCode(", collision)
        # #276 row 23: the floor and ceiling readers moved to the typed collision map core, which samples through
        # the owned sampler; its Live binding hands the cores the current room's map as bytes.
        field = root / r"Native\Game\src\Field"
        core = (field / "CollisionMap.cpp").read_text(encoding="utf-8")
        self.assertIn('#include "Field/CollisionSampling.hpp"', core)
        self.assertIn("SampleCollisionHeightCode(map, ", core)
        live = (field / "CollisionMapLive.cpp").read_text(encoding="utf-8")
        for reader in ("InterpolatedFloorHeight", "FloorHeightAt", "CeilingHeightAt", "FloorAndCeilingHeightsAt",
                       "RoomCeilingHeight"):
            self.assertIn(f" {reader}(LiveRoomCollisionMap(), world_x, world_z);", live)

    def test_saved_music_voice_release_uses_the_live_sequencer_binding(self):
        owned = self.owned_items()
        self.assertEqual(owned["F03198"]["name"], "VS::Audio::Seq::ReleaseSavedVoiceConflicts")
        root = Path(__file__).resolve().parents[2]
        seq = root / r"Native\Engine\src\Audio\Seq"
        sequence = (seq / "MusicSequence.cpp").read_text(encoding="utf-8")
        self.assertEqual(sequence.count("host.ReleaseSavedVoiceConflicts(state);"), 2)
        self.assertNotIn("ReleaseSavedVoiceConflicts()", sequence)
        live = (seq / "SeqMusicSequenceLive.cpp").read_text(encoding="utf-8")
        self.assertIn('#include "Audio/MusicVoiceConflicts.hpp"', live)
        self.assertEqual(live.count("ReleaseLiveSavedVoiceConflicts();"), 1)

    def test_ai_target_memory_aliases_preserve_the_original_prefix(self):
        old, new, _ = next(
            fix
            for fix in copy_fixups.TYPE_FIXES["T00532"]
            if fix[0] == "char reserved3C[8];"
        )
        self.assertIn(old, new)
        self.assertIn("u_int rememberedTargetTile;", new)
        self.assertIn("u_short targetMemoryFrames;", new)
        self.assertIn("short savedRelationOptions;", new)
        aliases = native_views.NATIVE_FIELD_ALIASES["T00532"]
        self.assertEqual(aliases["rememberedTargetTile"], 0x3C)
        self.assertEqual(aliases["targetMemoryFrames"], 0x40)
        owned = self.owned_items()
        self.assertEqual(owned["F01792"]["name"], "VS::Battle::Ai::RememberAiActionTarget")
        self.assertEqual(
            owned["F01792"]["path"],
            r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp",
        )

    def test_ai_target_forget_keeps_its_module_address_and_callers(self):
        # #276 row 28: Battle_Ai_ForgetAiActionTarget (F01793) is typed C++ beside the target memory it clears.
        owned = self.owned_items()
        self.assertEqual(owned["F01793"]["name"], "VS::Battle::Ai::ForgetAiActionTarget")
        self.assertEqual(owned["F01793"]["path"], r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp")
        self.assertNotIn("F01793", copy_fixups.FIXES)
        live = "VS::Battle::Ai::ForgetLiveAiActionTarget"
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F01793"], live)
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[live],
                         r"Native\Game\include\Battle\Ai\PlanningPrimitivesLive.hpp")
        self.assertIn(("BATTLE/BATTLE.PRG", 0x800E4CE8, live), copy_fixups.MODULE_BINDING_ENTRIES)
        ai = copy_to_native.REPO / r"Native\Game\src\Battle\Ai"
        calls = {name: (ai / name).read_text(encoding="utf-8").count(f"{live}(")
                 for name in ("Perception.cpp", "Movement.cpp", "PlanningPrimitivesLive.cpp")}
        self.assertEqual(calls, {"Perception.cpp": 0, "Movement.cpp": 0, "PlanningPrimitivesLive.cpp": 0})
        # F01643 moved the perception pass's forget into its live binding.
        perception = (ai / "ActorPerceptionLive.cpp").read_text(encoding="utf-8")
        self.assertIn("ForgetLiveAiActionTarget(State());", perception)
        # F01660 moved the blocked-movement detour's forget into its live binding.
        blocked = (ai / "BlockedMovementLive.cpp").read_text(encoding="utf-8")
        self.assertIn("ForgetLiveAiActionTarget(&state_);", blocked)
        live_source = (ai / "PlanningPrimitivesLive.cpp").read_text(encoding="utf-8")
        self.assertIn("void ForgetTarget() override { ForgetLiveAiActionTarget(&State()); }", live_source)
        self.assertNotIn("Battle_Ai_ForgetAiActionTarget", live_source)
        # F01810 retired ActionSelection.cpp, the generated home of the original forget.
        self.assertFalse((ai / "ActionSelection.cpp").exists())

    def test_ai_action_metadata_keeps_its_module_address_and_caller(self):
        # #276 row 28: Battle_Ai_BuildAiActionMetadata (F01810) is typed C++ beside the slot readers it feeds.
        owned = self.owned_items()
        self.assertEqual(owned["F01810"]["name"], "VS::Battle::Ai::BuildAiActionMetadata")
        self.assertEqual(owned["F01810"]["path"], r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp")
        self.assertNotIn("F01810", copy_fixups.FIXES)
        live = "VS::Battle::Ai::BuildLiveAiActionMetadata"
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F01810"], live)
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[live],
                         r"Native\Game\include\Battle\Ai\PlanningPrimitivesLive.hpp")
        self.assertIn(("BATTLE/BATTLE.PRG", 0x800E5710, live), copy_fixups.MODULE_BINDING_ENTRIES)
        ai = copy_to_native.REPO / r"Native\Game\src\Battle\Ai"
        caller = (ai / "ActorState.cpp").read_text(encoding="utf-8")
        # Its only caller, the settings application (F01813), is owned; the binding rebuilds the metadata last.
        self.assertNotIn(live, caller)
        self.assertNotIn("Battle_Ai_BuildAiActionMetadata", caller)
        settings = (ai / "ActorSettingsLive.cpp").read_text(encoding="utf-8")
        self.assertEqual(settings.count("BuildLiveAiActionMetadata(reinterpret_cast<Battle_Ai_ActorAiState*>(state));"), 1)
        # The function was the only item in ActionSelection.cpp/.h, so the copy no longer writes them.
        for name in ("ActionSelection.cpp", "ActionSelection.h"):
            self.assertFalse((ai / name).exists())
        manifest = json.loads((copy_to_native.REPO / r"tools\port\copy_manifest.json").read_text(encoding="utf-8"))
        self.assertNotIn(r"Native\Game\src\Battle\Ai\ActionSelection.cpp", manifest)

    def test_ai_linked_actors_keep_their_module_address_and_caller(self):
        # #276 row 28: Battle_Ai_ResolveAiLinkedActors (F01811) is typed C++ beside the actor relation policies.
        owned = self.owned_items()
        self.assertEqual(owned["F01811"]["name"], "VS::Battle::Ai::ResolveAiLinkedActors")
        self.assertEqual(owned["F01811"]["path"], r"Native\Game\src\Battle\Ai\Relations.cpp")
        self.assertNotIn("F01811", copy_fixups.FIXES)
        live = "VS::Battle::Ai::ResolveLiveAiLinkedActors"
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F01811"], live)
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[live],
                         r"Native\Game\include\Battle\Ai\RelationsLive.hpp")
        self.assertIn(("BATTLE/BATTLE.PRG", 0x800E5998, live), copy_fixups.MODULE_BINDING_ENTRIES)
        ai = copy_to_native.REPO / r"Native\Game\src\Battle\Ai"
        caller = (ai / "ActorState.cpp").read_text(encoding="utf-8")
        self.assertNotIn(live, caller)
        # The owned settings application (F01813) resolves links right before rebuilding the action metadata.
        settings = (ai / "ActorSettingsLive.cpp").read_text(encoding="utf-8")
        self.assertEqual(settings.count("ResolveLiveAiLinkedActors();"), 1)
        self.assertLess(settings.index("ResolveLiveAiLinkedActors();"), settings.index("BuildLiveAiActionMetadata("))
        for name in ("ActorState.cpp", "ActorState.h"):
            self.assertNotIn("Battle_Ai_ResolveAiLinkedActors", (ai / name).read_text(encoding="utf-8"))
        pointers = (copy_to_native.REPO / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertEqual(pointers.count("0x800E5998u"), 1)
        self.assertIn(f"0x800E5998u, 0u, (void*)&{live},", pointers)

    def test_ai_saved_settings_flags_keep_their_module_address_and_caller(self):
        # #276 row 28: Battle_Ai_ApplySavedAiSettingsFlags (F01812) is typed C++ in Battle/Ai/ActorSettings.cpp.
        owned = self.owned_items()
        self.assertEqual(owned["F01812"]["name"], "VS::Battle::Ai::ApplySavedAiSettingsFlags")
        self.assertEqual(owned["F01812"]["path"], r"Native\Game\src\Battle\Ai\ActorSettings.cpp")
        self.assertNotIn("F01812", copy_fixups.FIXES)
        live = "VS::Battle::Ai::ApplyLiveSavedAiSettingsFlags"
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F01812"], live)
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[live],
                         r"Native\Game\include\Battle\Ai\ActorSettingsLive.hpp")
        self.assertIn(("BATTLE/BATTLE.PRG", 0x800E5A74, live), copy_fixups.MODULE_BINDING_ENTRIES)
        ai = copy_to_native.REPO / r"Native\Game\src\Battle\Ai"
        # The owned creation core (F01818) applies the saved flags only when their enable bit is set,
        # before the host decodes the settings.
        caller = (ai / "ActorCreation.cpp").read_text(encoding="utf-8")
        call = "settings = ApplySavedAiSettingsFlags(settings, actor.saved_settings_flags);"
        self.assertEqual(caller.count(call), 1)
        self.assertLess(caller.index("if ((actor.saved_settings_flags & 1u) != 0) {"), caller.index(call))
        self.assertLess(caller.index(call), caller.index("host.ApplySettings(state, settings);"))
        for name in ("ActorState.cpp", "ActorState.h"):
            self.assertNotIn("Battle_Ai_ApplySavedAiSettingsFlags", (ai / name).read_text(encoding="utf-8"))
        pointers = (copy_to_native.REPO / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertEqual(pointers.count("0x800E5A74u"), 1)
        self.assertIn(f"0x800E5A74u, 0u, (void*)&{live},", pointers)

    def test_ai_settings_application_keeps_its_module_address_and_caller(self):
        # #276 row 28: Battle_Ai_ApplyActorAiSettings (F01813) is typed C++ in Battle/Ai/ActorSettings.cpp.
        owned = self.owned_items()
        self.assertEqual(owned["F01813"]["name"], "VS::Battle::Ai::ApplyActorAiSettings")
        self.assertEqual(owned["F01813"]["path"], r"Native\Game\src\Battle\Ai\ActorSettings.cpp")
        # The #272 decision-callback guard moved into the owned binding with the function.
        self.assertNotIn("F01813", copy_fixups.FIXES)
        live = "VS::Battle::Ai::ApplyLiveActorAiSettings"
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F01813"], live)
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[live],
                         r"Native\Game\include\Battle\Ai\ActorSettingsLive.hpp")
        self.assertIn(("BATTLE/BATTLE.PRG", 0x800E5A9C, live), copy_fixups.MODULE_BINDING_ENTRIES)
        ai = copy_to_native.REPO / r"Native\Game\src\Battle\Ai"
        # The owned creation binding (F01818) decodes the settings once, into the state it stored.
        caller = (ai / "ActorCreationLive.cpp").read_text(encoding="utf-8")
        call = "ApplyLiveActorAiSettings(reinterpret_cast<Battle_Ai_SettingsView*>(&state),"
        self.assertEqual(caller.count(call), 1)
        self.assertLess(caller.index("AiState& state = StoreState(created);"), caller.index(call))
        self.assertIn('#include "Battle/Ai/ActorSettingsLive.hpp"', caller)
        tables = (ai / "ActorState.cpp").read_text(encoding="utf-8")
        for name in ("ActorState.cpp", "ActorState.h"):
            self.assertNotIn("Battle_Ai_ApplyActorAiSettings", (ai / name).read_text(encoding="utf-8"))
        # The decode reads the module tables, so the copy still emits them.
        for table in ("Battle_Ai_kPerceptionRangesSquared", "Battle_Ai_viewAngles", "Battle_Ai_decisionCallbacks",
                      "Battle_Ai_kWanderTimingPresets", "Battle_Ai_kFlags34Bit14Values"):
            self.assertIn(table, tables)
        pointers = (copy_to_native.REPO / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertEqual(pointers.count("0x800E5A9Cu"), 1)
        self.assertIn(f"0x800E5A9Cu, 0u, (void*)&{live},", pointers)

    def test_ai_state_creation_keeps_its_module_address_and_caller(self):
        # #276 row 28: Battle_Ai_CreateActorAiState (F01818) is typed C++ in Battle/Ai/ActorCreation.cpp.
        owned = self.owned_items()
        self.assertEqual(owned["F01818"]["name"], "VS::Battle::Ai::CreateActorAiState")
        self.assertEqual(owned["F01818"]["path"], r"Native\Game\src\Battle\Ai\ActorCreation.cpp")
        # The layout and diagnostic fixups moved into the owned binding with the function.
        self.assertNotIn("F01818", copy_fixups.FIXES)
        live = "VS::Battle::Ai::CreateLiveActorAiState"
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F01818"], live)
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[live],
                         r"Native\Game\include\Battle\Ai\ActorCreationLive.hpp")
        self.assertIn(("BATTLE/BATTLE.PRG", 0x800E6178, live), copy_fixups.MODULE_BINDING_ENTRIES)
        ai = copy_to_native.REPO / r"Native\Game\src\Battle\Ai"
        for name in ("ActorState.cpp", "ActorState.h"):
            self.assertNotIn("Battle_Ai_CreateActorAiState", (ai / name).read_text(encoding="utf-8"))
        # Spawning, its only caller, creates the state through the live binding.
        spawning = (copy_to_native.REPO / r"Native\Game\src\Actors\ActorSpawningLive.cpp").read_text(encoding="utf-8")
        self.assertEqual(spawning.count("return Battle::Ai::CreateLiveActorAiState(actor, flags);"), 1)
        self.assertIn('#include "Battle/Ai/ActorCreationLive.hpp"', spawning)
        self.assertNotIn("Battle_Ai_CreateActorAiState", spawning)
        pointers = (copy_to_native.REPO / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertEqual(pointers.count("0x800E6178u"), 1)
        self.assertIn(f"0x800E6178u, 0u, (void*)&{live},", pointers)

    @staticmethod
    def battle_ability_function(name: str) -> str:
        # #276 row 31: Battle_AdvanceBattleAbilitySequence (F00099) is typed C++ in Battle/Abilities.cpp.
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Battle/Abilities.cpp").read_text(encoding="utf-8")
        start = source.index(f"void {name}(")
        return source[start:source.index("\n}\n", start)]

    def test_lethal_chain_success_keeps_credit_without_continuing(self):
        self.assertNotIn("F00099", copy_fixups.FIXES)
        chain = flat(self.battle_ability_function("ContinueChain"))
        self.assertIn("if (host.ActorHp(action.first_target_actor) != 0 && ValidAbilitySlot(slot)) {", chain)
        self.assertIn("} else {", chain)
        for operation in (
            "host.SetChainStep(host.ChainStep() + 1);",
            "host.SetQueueIndex(host.QueueIndex() + 1);",
            "host.QueueChainCount(static_cast<int>(host.QueueIndex()));",
            "host.FinishBattleAbilitySequence();",
        ):
            self.assertEqual(chain.count(operation), 1)
        # The kill credit follows the branch, so a lethal step still banks it without continuing.
        self.assertGreater(chain.index("CountChainKill(host);"), chain.index("host.FinishBattleAbilitySequence();"))

    def test_both_credit_paths_bank_points_beyond_pending_thresholds(self):
        self.assertNotIn("F00099", copy_fixups.FIXES)
        credit = flat(self.battle_ability_function("CountChainKill"))
        self.assertIn("host.TotalKills() < kMaxKills", credit)
        self.assertNotIn("PointRequirements", credit)
        self.assertIn("CountChainKill(host);", self.battle_ability_function("ContinueChain"))
        self.assertIn("CountChainKill(host);", self.battle_ability_function("ContinueDefense"))
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Battle/Abilities.cpp").read_text(encoding="utf-8")
        self.assertIn("constexpr int kMaxKills = 0xFFFF;", source)

    def test_music_cancellation_clears_the_native_pointer_field(self):
        self.assertNotIn("F03246", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        live = (root / "Native/Engine/src/Audio/Mixer/MixControlLive.cpp").read_text()
        self.assertIn("Audio_Seq_pendingMusicData.param1 = 0;", live)
        self.assertNotIn("*(int*)&Audio_Seq_pendingMusicData", live)

    def test_particle_child_events_project_native_parent_fields(self):
        source = """        ++node->age;
        Effects_EmitParticleEvent(arg0, node->tickEvent - 1, 0, 0, (void*)node);
        Effects_EmitParticleEvent(arg0, node->endEvent - 1, 0, 0, (void*)node);
        Effects_EmitParticleEvent(arg0, node->endEvent - 1, 0, 0, (void*)node);"""
        for old, new, _ in copy_fixups.FIXES["F01535"]:
            source = source.replace(old, new)
        self.assertIn("Effects_PluginEffectState parent = {0};", source)
        for axis in "xyz":
            self.assertIn(
                f"parent.position.v{axis} = node->motion.position.v{axis};",
                source,
            )
        self.assertIn("parent.transparency = node->transparency;", source)
        self.assertIn("parent.variant = node->opaque61Bit0;", source)
        self.assertEqual(source.count("&parent"), 3)
        self.assertNotIn("(void*)node", source)

    def test_original_occlusion_queries_use_the_owned_terrain_decoder(self):
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        for fid, name in (
            ("F01759", "VS::Field::GroundHeightAtWorldPosition"),
            ("F01760", "VS::Field::GroundHeightAtPackedTile"),
            ("F01761", "VS::Field::GroundHeightAtTile"),
            ("F01762", "VS::Field::GroundHeightFlagsAtPackedTile"),
            ("F01763", "VS::Field::GroundHeightFlagsAtWorldPosition"),
            ("F01764", "VS::Field::GroundHeightFlagsAtTile"),
            ("F01767", "VS::Field::IsPositionAboveGround"),
            ("F01768", "VS::Field::IsTerrainSegmentBlocked"),
        ):
            with self.subTest(fid=fid):
                self.assertEqual(owned[fid]["name"], name)
                self.assertEqual(
                    owned[fid]["path"],
                    r"Native\Game\src\Field\TerrainHeight.cpp",
                )
                self.assertEqual(
                    owned[fid]["header"],
                    r"Native\Game\include\Field\TerrainHeight.hpp",
                )

    def test_promoted_terrain_callers_pass_the_grid_and_decoded_values(self):
        fixes = [(row, pat, rep) for row, pat, rep, _ in copy_fixups.REGEX_FIXES
                 if "VS::Field::" in pat and "SampleCollisionHeightCode" not in pat
                 and "TransitionDoorsAtTile" not in pat and "FindMovingPlatformById" not in pat
                 and "DrawRoomTriggerMarkers" not in pat]
        self.assertEqual(len(fixes), 3)
        self.assertTrue(all(re.search(row, "Battle::Ai::Movement") for row, _, _ in fixes))
        text = "\n".join((
            "a = VS::Field::GroundHeightAtPackedTile(state->tile.word);",
            "b = VS::Field::GroundHeightFlagsAtPackedTile(state->tile);",
            "c = VS::Field::GroundHeightAtTile(x, z);",
            "d = VS::Field::GroundHeightAtWorldPosition(x, z);",
            "e = VS::Field::GroundHeightFlagsAtTile(nx, nz);",
            "f = VS::Field::IsTerrainSegmentBlocked(from, to, extent, own);",
        ))
        for _, pat, rep in fixes:
            text = re.sub(pat, rep, text)
        terrain = "VS::Battle::Ai::CurrentTerrain()"
        self.assertEqual(text.splitlines(), [
            f"a = VS::Field::GroundHeightAtPackedTile({terrain}, "
            "VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(state->tile.word)));",
            f"b = VS::Field::GroundHeightFlagsAtPackedTile({terrain}, "
            "VS::Core::Math::UnpackTileCoordinate(static_cast<std::uint32_t>(state->tile)));",
            f"c = VS::Field::GroundHeightAtTile({terrain}, x, z);",
            f"d = VS::Field::GroundHeightAtWorldPosition({terrain}, x, z);",
            f"e = VS::Field::GroundHeightFlagsAtTile({terrain}, nx, nz);",
            f"f = VS::Field::IsTerrainSegmentBlocked({terrain}, VS::Core::Math::PositionOf(*from), "
            "VS::Core::Math::PositionOf(*to), extent, own);",
        ])
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS["VS::Battle::Ai::CurrentTerrain"],
                         r"Native\Game\src\Battle\Ai\CurrentTerrain.hpp")

    def test_promoted_room_ai_lifecycle_callers_pass_the_runtime(self):
        fixes = [(row, pat, rep) for row, pat, rep, _ in copy_fixups.REGEX_FIXES
                 if "InitializeRoomAiMap" in pat]
        self.assertEqual(len(fixes), 1)
        row, pat, rep = fixes[0]
        self.assertTrue(re.search(row, "Battle::Ai::InitializeRoomNavigation"))
        self.assertTrue(re.search(row, "Field::FreeRoom"))
        runtime = "VS::Battle::Ai::BattleRoomAiRuntime()"
        self.assertEqual(re.sub(pat, rep, "VS::Battle::Ai::InitializeRoomAiMap();"),
                         f"VS::Battle::Ai::InitializeRoomAiMap({runtime});")
        self.assertEqual(re.sub(pat, rep, "    VS::Battle::Ai::FreeRoomAiState();"),
                         f"    VS::Battle::Ai::FreeRoomAiState({runtime});")
        owned = {row["id"]: row for row in csv.DictReader(
            Path(__file__).resolve().with_name("owned.csv").open(encoding="utf-8"))}
        self.assertEqual(owned["F01837"]["name"], "VS::Battle::Ai::FreeRoomAiState")
        self.assertEqual(owned["F01838"]["name"], "VS::Battle::Ai::InitializeRoomAiMap")
        self.assertEqual(owned["F01839"]["name"], "VS::Battle::Ai::SampleAiTileGroundHeight")
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS["VS::Battle::Ai::BattleRoomAiRuntime"],
                         owned["F01838"]["header"])

    def test_promoted_flight_terrain_callers_pass_explicit_inputs(self):
        def apply(text, row_name):
            for row, pat, rep, _ in copy_fixups.REGEX_FIXES:
                if re.search(row, row_name):
                    text = re.sub(pat, rep, text)
            return text

        neighborhood = "VS::Battle::Ai::ActorFlightNeighborhood"
        self.assertEqual(
            apply("VS::Battle::Ai::LowestReachableGroundHeight((void*)state, angle)",
                  "Battle::Ai::UpdateFlightMovement"),
            f"VS::Battle::Ai::LowestReachableGroundHeight({neighborhood}("
            "*(Battle_Ai_ActorAiState *)((void*)state)), angle)")
        self.assertEqual(
            apply("VS::Battle::Ai::LowestReachableGroundHeight(ai_state, movement_angle)",
                  "Battle::Ai::UpdateFlightDirectionalMovement"),
            f"VS::Battle::Ai::LowestReachableGroundHeight({neighborhood}(*ai_state), movement_angle)")
        self.assertEqual(
            apply("VS::Battle::Ai::MinimumAiGroundHeightAlongSegment(px, pz, x, z)",
                  "Battle::Ai::UpdateFlightMovement"),
            "VS::Battle::Ai::MinimumAiGroundHeightAlongSegment(VS::Battle::Ai::CurrentTerrain(), px, pz, x, z)")
        self.assertEqual(
            apply("ceiling = VS::Battle::Ai::RoomNavigationCeilingHeight();", "Actors::PlaceActorAtTile"),
            "ceiling = VS::Battle::Ai::RoomNavigationCeilingHeight(VS::Battle::Ai::BattleRoomAiRuntime());")
        owned = {row["id"]: row for row in csv.DictReader(
            Path(__file__).resolve().with_name("owned.csv").open(encoding="utf-8"))}
        for fid, name in (("F01731", "LowestReachableGroundHeight"),
                          ("F01830", "MinimumAiGroundHeightAlongSegment"),
                          ("F01843", "RoomNavigationCeilingHeight")):
            self.assertEqual(owned[fid]["name"], f"VS::Battle::Ai::{name}")
            self.assertEqual(owned[fid]["header"], r"Native\Game\include\Battle\Ai\FlightTerrain.hpp")
        self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS["VS::Battle::Ai::ActorFlightNeighborhood"],
                         r"Native\Game\src\Battle\Ai\CurrentTerrain.hpp")
        self.assertNotIn(r"Native\Game\src\Battle\Ai\FlightTerrain.cpp",
                         Path(__file__).resolve().with_name("relaxed_sources.txt").read_text(
                             encoding="utf-8").split())

    def test_palette_pulse_uses_the_native_gimmick_owner(self):
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        self.assertEqual(
            owned["F00979"]["header"],
            r"Native\Game\include\Actors\AppearanceTransition.hpp",
        )
        # The room pass is owned (#276 row 22); its host pulses the typed
        # actor and gimmick records.
        self.assertNotIn("F01008", copy_fixups.FIXES)
        self.assertEqual(
            copy_fixups.OWNED_CALL_BINDINGS["F01008"],
            "VS::Actors::UpdateAndDrawLiveRoomActors",
        )
        live = self.model_rendering_live_source()
        self.assertIn("UpdateLiveActorPalettePulse(*actor);", live)
        self.assertIn("UpdateLiveGimmickPalettePulse(*object);", live)
        self.assertNotIn("UpdatePalettePulse(NULL", live)

    def test_transition_text_is_owned_by_the_typed_text_box_core(self):
        # The encoded-control decoding (0xE5 non-printing, 0x8F space, unsigned
        # bytes) is now VS::Render::BlendTextBoxIntoFramebuffer, covered by the
        # [render-font] Catch2 cases; no generated copy remains to patch.
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        for fid, name in (
            ("F01328", "VS::Render::FormatEncodedText"),
            ("F01334", "VS::Render::DrawProgressiveTextBoxText"),
            ("F01335", "VS::Render::DrawScaledTextBoxText"),
            ("F01337", "VS::Render::BlendTextBoxIntoFramebuffer"),
        ):
            self.assertEqual(owned[fid]["name"], name)
            self.assertNotIn(fid, copy_fixups.FIXES)
        self.assertEqual(
            copy_fixups.OWNED_CALL_BINDINGS["F01337"],
            "VS::Render::BlendLiveTextBoxIntoFramebuffer",
        )

    def test_attached_draw_requires_a_resident_parent_matrix(self):
        live = self.model_rendering_live_source()
        self.assertIn(
            "if (MATRIX* parent = ResolveLiveAttachmentParentMatrix(actor))", live
        )
        self.assertNotIn("entries[", live)
        self.assertNotIn("boneMatrices[actor->instance.attachBone]", live)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["VS::Actors::ResolveLiveAttachmentParentMatrix"],
            r"Native\Game\include\Actors\ModelDrawingLive.hpp",
        )

    def test_grounded_history_uses_the_actor_native_layout(self):
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        self.assertEqual(
            owned["F00989"]["header"],
            r"Native\Game\include\Actors\MotionRequests.hpp",
        )
        # #276 slice 21 (actors-movement): the landing is owned; its typed core snapshots through the typed history API.
        self.assertNotIn("F00988", copy_fixups.FIXES)
        self.assertEqual(owned["F00988"]["header"], r"Native\Game\include\Actors\ActorMovement.hpp")
        core = (Path(__file__).resolve().parents[2] / owned["F00988"]["path"]).read_text(encoding="utf-8")
        self.assertIn("SaveActorGroundedTransform(", core)
        self.assertNotIn("Field_GimmickInstance", core)
        # #276 slice 20c: the motion update is owned; its Live host saves through the same grounded history API
        # under the original motion-state guard.
        self.assertNotIn("F04130", copy_fixups.FIXES)
        self.assertEqual(owned["F04130"]["header"], r"Native\Game\include\Actors\AnimationPose.hpp")
        pose = (Path(__file__).resolve().parents[2] / owned["F04130"]["path"]).read_text(encoding="utf-8")
        self.assertIn("host.SaveGroundedTransform()", pose)
        live = (Path(__file__).resolve().parents[2] / r"Native\Game\src\Actors\AnimationPoseLive.cpp").read_text(
            encoding="utf-8")
        self.assertIn("SaveLiveActorGroundedTransform(&actor_);", live)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["VS::Actors::SaveLiveActorGroundedTransform"],
            r"Native\Game\include\Actors\MotionRequestsLive.hpp",
        )

    def test_native_file_cards_draw_after_footer_and_before_fade(self):
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Save_NativeDrawFileOverlay"],
            r"Native\Game\include\Save\NativeMenu.h",
        )
        # #276 slices 51 and 53: MENU7's _drawFileMenu and its TITLE.PRG copy (F03624) are the typed
        # Menu::Files core, which asks its host for the cards after the footer and before the fade;
        # FilesLive.cpp's hosts draw them with Save_NativeDrawFileOverlay(0) and (1).
        self.assertNotIn("F02757", copy_fixups.FIXES)
        self.assertNotIn("F03624", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Menu\Files.cpp").read_text(encoding="utf-8")
        self.assertTrue(core.startswith("// Hand-maintained (tools/port/owned.csv)."))
        frame = core.split("host.DrawFileOverlay();", 1)
        self.assertEqual(len(frame), 2)
        self.assertIn("state.screen_fade", frame[1].split("\n", 2)[1])
        live = (root / r"Native\Game\src\Menu\FilesLive.cpp").read_text(encoding="utf-8")
        self.assertIn("void DrawFileOverlay() override { Save_NativeDrawFileOverlay(0); }", live)
        self.assertIn("void DrawFileOverlay() override { Save_NativeDrawFileOverlay(1); }", live)

    def test_file_pickers_promote_to_typed_core_with_live_callers(self):
        owned = self.owned_items()
        root = Path(__file__).resolve().parents[2]
        sources = {
            path: (root / path).read_text(encoding="utf-8")
            for path in (r"Native\Game\src\Menu\FilesLive.cpp", r"Native\Game\src\Title\TitleMenuLive.cpp")
        }
        for identifier, core, live, path in (
            ("F02764", "UpdateDataSaveMenu", "UpdateLiveDataSaveMenu", r"Native\Game\src\Menu\FilesLive.cpp"),
            ("F02767", "UpdateDataLoadMenu", "UpdateLiveDataLoadMenu", r"Native\Game\src\Menu\FilesLive.cpp"),
            ("F03628", "UpdateLoadMenu", "UpdateLiveLoadMenu", r"Native\Game\src\Title\TitleMenuLive.cpp"),
            ("F03634", "UpdateSaveMenu", "UpdateLiveSaveMenu", r"Native\Game\src\Title\TitleMenuLive.cpp"),
        ):
            with self.subTest(identifier=identifier):
                self.assertEqual(owned[identifier]["name"], f"VS::Menu::Files::{core}")
                self.assertEqual(owned[identifier]["path"], r"Native\Game\src\Save\FileMenu.cpp")
                self.assertEqual(owned[identifier]["header"], r"Native\Game\include\Save\FileMenu.hpp")
                self.assertEqual(
                    copy_fixups.NATIVE_HELPER_HEADERS[f"VS::Menu::{live}"],
                    r"Native\Game\include\Save\FileMenuLive.hpp",
                )
                # TitleMenuLive.cpp calls the binding from inside namespace VS.
                self.assertRegex(sources[path], rf"\b(VS::)?Menu::{live}\(")
                self.assertNotIn(f"Menu_Files_{core}", sources[path])
                self.assertNotIn(f"VS::Menu::Files::{core}(", sources[path])
                matches = [(pattern, replacement) for _, pattern, replacement, _ in copy_fixups.REGEX_FIXES
                           if replacement == f"VS::Menu::{live}("]
                self.assertEqual(len(matches), 1)
                self.assertEqual(re.sub(*matches[0], f"x = VS::Menu::Files::{core}(0);"),
                                 f"x = VS::Menu::{live}(0);")

    def test_trigger_marker_import_owns_the_complete_geometry_definition(self):
        blob = next(
            item
            for item in copy_fixups.MODULE_DATA_BLOBS
            if item[4] == "Content_battleTriggerMarkerBytes"
        )
        self.assertEqual(blob[:4], ("BATTLE/BATTLE.PRG", 0x7FD48, 0x800E8548, 116))
        self.assertNotIn(blob[4], copy_fixups.MODULE_DATA_BLOB_TYPES)
        registry = Path(__file__).resolve().with_name("owned.csv")
        with registry.open(encoding="utf-8", newline="") as stream:
            owned = {row["id"]: row for row in csv.DictReader(stream)}
        for fid in ("F00617", "F00618"):
            with self.subTest(fid=fid):
                self.assertEqual(
                    owned[fid]["path"],
                    r"Native\Game\src\Field\RoomTriggerMarkers.cpp",
                )
                self.assertEqual(
                    owned[fid]["header"],
                    r"Native\Game\include\Field\RoomTriggerMarkers.hpp",
                )
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["VS::Field::DrawLiveRoomTriggerMarkers"],
            r"Native\Game\include\Field\RoomTriggerMarkersLive.hpp",
        )

    def test_native_resume_hooks_precede_event_and_reset_mutation(self):
        # #276 slice 25c: the zone and room loads (F00533, F00543) are the typed VS::Field::FinishZoneLoad and
        # FinishRoomLoad; the resume hooks are RoomLoadHost/ZoneLoadHost calls in the core.
        self.assertNotIn("F00533", copy_fixups.FIXES)
        self.assertNotIn("F00543", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        loading = (root / r"Native\Game\src\Field\RoomLoading.cpp").read_text(encoding="utf-8")
        room = loading.split("bool FinishRoomLoad(RoomLoadHost& host, int room_id) {", 1)[1]
        self.assertLess(room.index("host.FinishNativeResume()"), room.index("StartRoomPlay("))
        self.assertLess(room.index("StartRoomCamera("), room.index("host.FinishNativeResume()"))
        self.assertIn("host.JumpToTitle();", room.split("StartRoomPlay(", 1)[0])
        zone = loading.split("void FinishZoneLoad(ZoneLoadHost& host, int zone_id) {", 1)[1]
        zone = zone.split("\n}\n", 1)[0]
        self.assertIn("if (!host.NativeResumePending()) {\n    host.ResetZoneScriptFlags(", zone)
        room_live = (root / r"Native\Game\src\Field\RoomLoadingLive.cpp").read_text(encoding="utf-8")
        self.assertIn("return Save_NativeFinishResume() != 0;", room_live)
        self.assertIn("void JumpToTitle() override { Boot::JumpToLiveTitle(); }", room_live)
        for fid in ("F03423", "F03437"):
            with self.subTest(fid=fid):
                reset = copy_fixups.FIXES[fid][-1]
                self.assertIn("{\n    Save_NativeResetSession();", reset[1])
        # #276 slice 53: TITLE.PRG's _initGameData (F03673) is the typed VS::Title::InitializeNewGameData,
        # which resets the session through its host before any state write.
        self.assertNotIn("F03673", copy_fixups.FIXES)
        core = (root / r"Native\Game\src\Title\TitleMenu.cpp").read_text(encoding="utf-8")
        body = core.split("void InitializeNewGameData(Host& host) {", 1)[1]
        self.assertTrue(body.lstrip().startswith("host.ResetSession();"))
        live = (root / r"Native\Game\src\Title\TitleMenuLive.cpp").read_text(encoding="utf-8")
        self.assertIn("void ResetSession() override { Save_NativeResetSession(); }", live)
        for name in (
            "Save_NativeResumePending",
            "Save_NativeFinishResume",
            "Save_NativeResetSession",
        ):
            self.assertEqual(
                copy_fixups.NATIVE_HELPER_HEADERS[name],
                r"Native\Game\include\Save\NativeResume.h",
            )

    def test_owned_ai_direction_queries_need_no_region_cell_fixes(self):
        # F01728 and F01739 are owned by AreaRoute.cpp and F01742 by Wander.cpp;
        # each reads the region cells natively, so no generated fix remains.
        for fid in ("F01728", "F01739", "F01742"):
            with self.subTest(fid=fid):
                self.assertNotIn(fid, copy_fixups.FIXES)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Battle_Ai_TerrainRegionCell"],
            r"Native\Game\src\Battle\Ai\FrameUpdate.h",
        )

    def test_room_geometry_names_preserve_index_flags_and_bias_roles(self):
        examples = (
            (
                "Field::FindRoomGeometryGroup",
                "void* find(int arg0) { int v1; int* a1; return arg0 < v1 ? a1 + arg0 * 16 : NULL; }",
                "void* find(int group_index) { int group_count; int* groups; return group_index < group_count ? groups + group_index * 16 : NULL; }",
            ),
            (
                "Field::ShowRoomGeometryGroup",
                "int show(int arg0) { int* temp_v0 = find(arg0); *temp_v0 &= ~0x100; }",
                "int show(int group_index) { int* group = find(group_index); *group &= ~0x100; }",
            ),
            (
                "Field::HideRoomGeometryGroup",
                "int hide(int arg0) { int* temp_v0 = find(arg0); *temp_v0 |= 0x100; }",
                "int hide(int group_index) { int* group = find(group_index); *group |= 0x100; }",
            ),
            (
                "Field::SwapRoomGeometryGroupFlags",
                "int swap(int arg0, int arg1) { int* temp_s0 = find(arg0); int* temp_a1 = find(arg1); int temp_a0 = *temp_s0; *temp_s0 = *temp_a1; *temp_a1 = temp_a0; }",
                "int swap(int first_group_index, int second_group_index) { int* first_group = find(first_group_index); int* second_group = find(second_group_index); int saved_flags = *first_group; *first_group = *second_group; *second_group = saved_flags; }",
            ),
            (
                "Field::AddRoomGeometryDepthBias",
                "int bias(int arg0, int arg1) { u_short* temp_v0 = find(arg0); temp_v0[1] = (u_short)(temp_v0[1] + arg1); }",
                "int bias(int group_index, int depth_bias) { u_short* group_halfwords = find(group_index); group_halfwords[1] = (u_short)(group_halfwords[1] + depth_bias); }",
            ),
            (
                "Field::DisableRoomGeometryGroup",
                "int disable(int arg0) { int* temp_v0 = find(arg0); *temp_v0 |= 0x1000; }",
                "int disable(int group_index) { int* group = find(group_index); *group |= 0x1000; }",
            ),
            (
                "Field::UnrelatedGeometryCommand",
                "void untouched(int arg0) { int temp_v0 = arg0; }",
                "void untouched(int arg0) { int temp_v0 = arg0; }",
            ),
        )
        for name, source, expected in examples:
            with self.subTest(name=name):
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_lfo_catalogue_preserves_unsigned_byte_source_data_references(self):
        source = "short* Audio_Seq_kLfoWaveforms[];"
        for old, new, _ in copy_fixups.FIXES["G01840"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, "short* Audio_Seq_kLfoWaveforms[256];")
        self.assertEqual(
            copy_fixups.MODULE_SAMPLE_POINTERS["G01840"],
            "Content_residentLfoSamples",
        )
        blob = next(
            item
            for item in copy_fixups.MODULE_DATA_BLOBS
            if item[4] == "Content_residentLfoSamples"
        )
        self.assertEqual(blob[:4], ("SLUS_010.40", 0x800, 0x80010000, 0x52000))
        self.assertEqual(copy_fixups.MODULE_DATA_BLOB_TYPES[blob[4]], ("short", 2))

    def test_hud_timer_retirement_does_not_add_a_packed_pointer_view(self):
        ledger = (
            Path(__file__).resolve().parents[2] / "Docs" / "ledgers" / "DATA_LEDGER.csv"
        )
        with ledger.open(encoding="utf-8", newline="") as stream:
            row = next(
                item for item in csv.DictReader(stream) if item["id"] == "G01934"
            )
        self.assertEqual(row["disposition"], "drop")
        self.assertNotIn("view-of:", row["notes"])

    def test_puzzle_timer_uses_owned_mutable_text_instead_of_pointer_binding(self):
        source = 'char const Battle_puzzleTimerText[] = "00:00:00";'
        for old, new, _ in copy_fixups.FIXES["G00614"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, 'char Battle_puzzleTimerText[9] = "00:00:00";')
        source = (
            "void Battle_DrawPuzzleTimer(int* value)\n{\n"
            "    char sp10[4];\n"
            "    *(int*)sp10 = *value;\n"
            "Battle_puzzleTimerTextPointer[6 - i * 3] = digit;\n"
            "    VS::Render::DrawLiveCompactText(Battle_puzzleTimerTextPointer, xy, ordering);"
            "\n}"
        )
        for old, new, _ in copy_fixups.FIXES["F01384"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertNotIn("Battle_puzzleTimerTextPointer", source)
        self.assertIn("VS::Render::DrawLiveCompactText(Battle_puzzleTimerText,", source)
        self.assertLess(
            source.index("Core_Debugf"), source.index("DrawLiveCompactText")
        )
        self.assertIn("const State_SceneTimer* timer", source)
        self.assertIn("timer->seconds, timer->minutes, timer->hours", source)
        self.assertNotIn("*(int*)sp10", source)
        source = (
            "        Render_activeTextBoxIndex = id;\n\n"
            "Battle_DrawPuzzleTimer(&Battle_puzzleTimer);"
        )
        for old, new, _ in copy_fixups.FIXES["F01392"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(source, "Battle_DrawPuzzleTimer(&State_sceneTimer);")

    def test_orbit_return_globals_preserve_complete_command_objects(self):
        for gid, name, extent in (
            ("G00130", "Event_kOrbitReturnFirstCommand", 4),
            ("G00131", "Event_kOrbitReturnSecondCommand", 3),
        ):
            source = f"u_char {name};"
            for old, new, _ in copy_fixups.FIXES[gid]:
                self.assertIn(old, source)
                source = source.replace(old, new)
            self.assertEqual(source, f"u_char {name}[{extent}];")
        source = (
            "Event_CommandCameraMotion(&Event_kOrbitReturnFirstCommand, 0);\n"
            "Event_CommandCameraMotion(&Event_kOrbitReturnSecondCommand, 0);"
        )
        for old, new, _ in copy_fixups.FIXES["F01238"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertNotIn("&Event_kOrbitReturn", source)

    def test_camera_track_diagnostics_preserve_duration_and_update_order(self):
        source = "        c->durationFrames = script[2];"
        for old, new, _ in copy_fixups.FIXES["F01221"]:
            if "durationFrames" not in old:
                continue
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertTrue(source.startswith("        c->durationFrames = script[2];"))
        self.assertIn("Core_Debugf", source)
        # #276 slice 19b: case 10 reads the actor's facing from a typed local placement, not camera offset 0x1DA.
        old, new, _ = next(item for item in copy_fixups.FIXES["F01221"] if "actorQueryScratch" in item[0])
        self.assertIn("*(short*)((char*)Event_camera + 0x1DA)", old)
        self.assertIn("Actors_ActorLocation placement{};", new)
        self.assertIn("VS::Actors::LiveActorPlacement(id, &placement);", new)
        self.assertIn("s->actorRotation.vy = placement.position.pad;", new)
        self.assertNotIn("Event_camera", new)

    def test_script_actor_tile_store_reads_a_typed_placement(self):
        # #276 slice 19b, then row 34: the tile bytes come from the typed placement view, not a 12-byte stack buffer.
        self.assertNotIn("F01052", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Event\ScriptActorCommands.cpp").read_text(encoding="utf-8")
        start = core.index("ScriptStep ScriptStoreActorTile(")
        handler = core[start:core.index("\n}\n", start)]
        self.assertIn("const PackedTile tile = PlacementOf(host, actor).tile;", handler)
        for flag, field in (("flag", "x"), ("flag + 1", "layer"), ("flag + 2", "z")):
            with self.subTest(field=field):
                self.assertIn(f"host.SetFlag({flag}, tile.{field});", handler)
        generated = (root / r"Native\Game\src\Event\ScriptActors.cpp").read_text(encoding="utf-8")
        self.assertNotIn("u_char position[12];", generated)
        source = "    if (c->motionMode != 11) {"
        for old, new, _ in copy_fixups.FIXES["F01223"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("Core_Warnf", source)
        self.assertNotIn("return", source)
        self.assertTrue(source.endswith("    if (c->motionMode != 11) {"))

    def test_template_instances_apply_fixups_without_option_overrides(self):
        # The textured PLG265 dispatcher no longer needs a fixup: the template
        # header's own #undef ends the option (#290).
        self.assertNotIn("F02351", copy_fixups.FIXES)
        copier = Path(__file__).with_name("copy_to_native.py").read_text()
        templates = copier.split("    def copy_templates(", 1)[1].split("    # ---", 1)[
            0
        ]
        self.assertIn("block = self.apply_fixups(r, block)", templates)

    def test_template_instances_drop_options_their_header_undefines(self):
        sources = {
            "src/EFFECT/PLG.BIN/0.c": (
                "#define VS_RING_FUNCTION first\n"
                "#define VS_RING_TRANSPOSED\n"
                '#include "src/EFFECT/rings.h"\n'
                "#define VS_PARTICLE_UNTEXTURED\n"
                '#include "src/EFFECT/particleDispatch.h"\n'
                "#define KEPT 5\n"
                "#define VS_RING_FUNCTION second\n"
                '#include "src/EFFECT/rings.h"\n'
                '#include "src/EFFECT/particleDispatch.h"\n'
            ),
            "src/EFFECT/rings.h": (
                "#ifdef VS_RING_TRANSPOSED\n#endif\n"
                "#undef VS_RING_FUNCTION\n#undef VS_RING_TRANSPOSED\n"
            ),
            "src/EFFECT/particleDispatch.h": "  #undef VS_PARTICLE_UNTEXTURED\n",
        }
        copier = object.__new__(copy_to_native.Copier)
        copier.src = mock.Mock()
        copier.src.load.side_effect = lambda rel: sources[rel].encode()
        rel = "src/EFFECT/PLG.BIN/0.c"
        includes = copier.plugin_includes(rel)
        self.assertEqual(
            includes,
            [
                (3, "rings", [f"{rel}:1", f"{rel}:2"]),
                (5, "particleDispatch", [f"{rel}:4"]),
                (8, "rings", [f"{rel}:6", f"{rel}:7"]),
                (9, "particleDispatch", [f"{rel}:6"]),
            ],
        )

    def test_plg128_second_surface_does_not_repeat_its_texture(self):
        surface = (
            Path(__file__).resolve().parents[2]
            / "Native/Game/src/Effects/Templates/Surface.cpp"
        ).read_text(encoding="utf-8")
        blocks = {
            name: surface.split(f"bool Plg128::{name}(", 1)[1].split("\n}\n", 1)[0]
            for name in ("Surface", "Surface2")
        }
        # PLG128 F8.c:44 sets VS_SURFACE_REPEAT; surfaces.h:275 ends it before
        # the second surface at F8.c:61 (#290). The owned instances keep that
        # as the texture span each passes to the typed template.
        self.assertIn("SurfaceTextureSpan::kFourColumns", blocks["Surface"])
        self.assertNotIn("kFourColumns", blocks["Surface2"])
        self.assertIn("SurfaceTextureSpan::kSixteenColumns", blocks["Surface2"])

    def test_texture_reset_uses_the_complete_native_state(self):
        source = "\n".join(f"    arg0[{index}] = 0;" for index in range(8))
        for old, new, _ in copy_fixups.FIXES["F01590"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("*state = (Effects_ParticleTextureAnimation){0};", source)
        self.assertNotIn("arg0[", source)

    def test_unbound_texture_diagnostics_do_not_invent_a_frame(self):
        source = "    arg0->textureIndex = textureIndex;"
        for old, new, _ in copy_fixups.FIXES["F01591"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("Core_Debugf", source)
        self.assertIn("request_effect=%u", source)
        self.assertNotIn("Effects_currentInstance->context.effectId", source)
        self.assertNotIn("return", source)
        self.assertTrue(source.endswith("arg0->textureIndex = textureIndex;"))
        source = "        Effects_InitializeParticleTextureAnimation(&state->texture, effect->ringSegments, effect->ringPositions);"
        for old, new, _ in copy_fixups.FIXES["F01861"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("closed ring definition=%u parameter=0x%X", source)
        self.assertNotIn("return", source)

    def test_texture_provenance_does_not_read_secondary_only_instance_metadata(self):
        for fid in ("F01591", "F01592", "F01861"):
            with self.subTest(fid=fid):
                replacement = "\n".join(new for _, new, _ in copy_fixups.FIXES[fid])
                self.assertNotIn("Effects_currentInstance", replacement)
                self.assertIn("Effects_secondaryRequest.effectId", replacement)
                self.assertIn("Effects_actionRequest.effectId", replacement)

    def test_texture_names_preserve_state_and_stream_roles(self):
        examples = (
            (
                "Effects::ClearParticleTextureState",
                "void clear(int* arg0) { state = arg0; }",
                "void clear(int* storage) { state = storage; }",
            ),
            (
                "Effects::InitializeParticleTextureAnimation",
                "void init(State* arg0, int textureIndex, int arg2) { arg0->program = table[arg2 + 2]; }",
                "void init(State* animation, int textureIndex, int programIndex) { animation->program = table[programIndex + 2]; }",
            ),
            (
                "Effects::AdvanceParticleTextureAnimation",
                "void advance(State* arg0) { int w = read(arg0); switch (w & 0xF0000000) {} }",
                "void advance(State* animation) { int command = read(animation); switch (command & 0xF0000000) {} }",
            ),
        )
        for name, source, expected in examples:
            with self.subTest(name=name):
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_shared_bank_retirement_unwinds_before_detaching_and_freeing(self):
        # #276 slice 20b: UnloadSharedSequence (F00279) is the typed core; generated callers bind the Live adapter.
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Actors\AnimationControl.cpp").read_text(encoding="utf-8")
        start = core.index("int UnloadSharedSequence(AnimationControlHost& host, int bank) {")
        source = core[start:core.index("\n}\n", start)]
        self.assertLess(source.index("host.CanUnwindBoundAnimation(*actor)"),
                        source.index("host.RestartAnimationEvents(*actor) < 0"))
        self.assertLess(source.index("host.RestartAnimationEvents(*actor) < 0"),
                        source.index("actor->animation_header = SequenceHandle::kNone"))
        self.assertLess(source.index("actor->blend_pose = actor->pose"),
                        source.index("host.ReleaseSharedSequence(bank)"))
        self.assertIn("return -1;", source)
        self.assertIn("return result;", source)
        self.assertNotIn("F00279", copy_fixups.FIXES)
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F00279"], "VS::Actors::UnloadLiveSharedSequence")

    def test_animation_event_unwind_has_an_explicit_nullable_contract(self):
        # #276 slice 20c: the unwind and the pose start are owned. The typed restart refuses a moved model whose
        # events are unavailable; the Live pose start returns before resetting a pose it could not unwind.
        self.assertNotIn("F00998", copy_fixups.FIXES)
        self.assertNotIn("F00992", copy_fixups.FIXES)
        # Row 34: opcode 0x3C (F01081) holds the script rather than advancing after a failed shared-bank retirement.
        self.assertNotIn("F01081", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        script = (root / r"Native\Game\src\Event\ScriptActorCommands.cpp").read_text(encoding="utf-8")
        self.assertIn("host.UnloadSharedSequence(static_cast<int>(selector)) < 0) {\n    return ScriptStep::kHold;",
                      script)
        events = (root / r"Native\Game\src\Actors\AnimationEvents.cpp").read_text(encoding="utf-8")
        self.assertIn("EventRestart::kUnavailable", events)
        live = (root / r"Native\Game\src\Actors\AnimationPoseLive.cpp").read_text(encoding="utf-8")
        start = live.index("void InitializeLiveActorAnimationPose(")
        init = live[start:live.index("\n}\n", start)]
        self.assertLess(init.index("RestartLiveAnimationEvents(*actor) < 0"), init.index("actor->poseOverrideActive = 0;"))
        self.assertIn("Core_Errorf", live)
        # #276 slice 20b: PlayActorAnimation (F00856) is owned and keeps the override when the unwind fails.
        core = (root / r"Native\Game\src\Actors\AnimationControl.cpp").read_text(encoding="utf-8")
        self.assertIn(
            "if (actor->pose_override_active && host.RestartAnimationEvents(*actor) < 0) {\n    return -1;\n  }\n"
            "  actor->animation_event_flag = false;\n  actor->pose_override_active = false;",
            core,
        )

    def test_scripted_effect_requires_its_actor_before_starting_playback(self):
        # #276 row 36: the typed handler refuses an unavailable actor through its host (Core_Errorf) and holds the
        # script before any playback starts; a model reference must match the actor's model.
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Event\ScriptEffectCommands.cpp").read_text(encoding="utf-8")
        start = core.index("ScriptStep CommandPlayActorEffect(")
        handler = core[start:core.index("\n}\n", start)]
        self.assertIn("reference_type <= kSlotReference ? host.ResolveActor(selector) : kNoActor", handler)
        self.assertIn("(!slot.battle_actor || slot.model_id != selector)", handler)
        self.assertLess(handler.index("!in_range ||"), handler.index("!slot.occupied;"))
        self.assertLess(handler.index("return ScriptStep::kHold;"), handler.index("host.StartActorEffect"))
        live = (root / r"Native\Game\src\Event\ScriptEffectCommandsLive.cpp").read_text(encoding="utf-8")
        start = live.index("void ReportUnavailableActor(")
        self.assertIn("Core_Errorf", live[start:live.index("\n  }\n", start)])

    def test_sound_effect_commands_bind_their_handler_table_addresses(self):
        # #276 audio-control-sfx: Audio_Mixer_kCommandHandlers (SLUS_010.40 0x800301E0) stores these 13 handler
        # addresses; every other owned control/SFX address left the module symbols.
        handlers = {
            0x80017208: "LivePlayInterfaceProgramPair", 0x80017254: "LivePlayInterfaceProgram",
            0x800172D4: "LivePlaySharedProgram", 0x8001733C: "LivePlayBlobPrograms",
            0x8001746C: "LiveStopPrograms", 0x80017870: "LiveSetProgramVolume",
            0x80017958: "LiveFadeProgramVolume", 0x80017AD8: "LiveSetSceneVolume",
            0x80017B54: "LiveFadeSceneVolume", 0x80017C1C: "LiveSetProgramPan",
            0x80017CFC: "LiveFadeProgramPan", 0x80017E74: "LiveSetScenePan",
            0x80017EEC: "LiveFadeScenePan",
        }
        root = Path(__file__).resolve().parents[2]
        pointers = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        for address, name in handlers.items():
            with self.subTest(name=name):
                binding = f"VS::Audio::SoundEffects::{name}"
                self.assertIn(("SLUS_010.40", address, binding), copy_fixups.MODULE_BINDING_ENTRIES)
                self.assertEqual(copy_fixups.NATIVE_HELPER_HEADERS[binding],
                                 r"Native\Engine\include\Audio\SoundEffectProgramsLive.hpp")
                self.assertIn(f"0x{address:08X}u, 0u, (void*)&{binding},", pointers)
        for retired in ("Audio_PlaySfx,", "Audio_ResolveSfxProgram,", "Audio_Control_SetMusicVolume,",
                        "Audio_SoundEffects_PlayProgramPair,"):
            self.assertNotIn(f"(void*)&{retired}", pointers)

    def test_empty_stack_top_is_checked_after_remaining_member_updates(self):
        source = """    UpdateRemainingMembers();
    i = 0;

    while ((temp_v0_2 = func_800A3C00(Field_gimmickInstances[temp_s4], i))) {}
"""
        for old, new, _ in copy_fixups.FIXES["F00836"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        guard = source.index("if (Field_gimmickInstances[temp_s4] == NULL)")
        self.assertLess(source.index("UpdateRemainingMembers"), guard)
        self.assertLess(guard, source.index("while ("))
        self.assertIn("no eligible stack top", source)

    def test_title_arena_reset_invalidates_native_ai_before_new_allocations(self):
        source = """void Boot_InitializeTitleSession(void)
{
    VS::Core::Memory::InitializeHeap((Core_HeapBlockHeader*)0x8010C000, 0xF2000);
    VS::Content::InitializeLiveDiscQueue();
    VS::Content::ResetDiscDrive();
}
"""
        for old, new, _ in copy_fixups.FIXES["F03437"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertLess(
            source.index("VS::Core::Memory::ResetLiveGameHeap()"),
            source.index("Battle_Ai_InvalidateSessionBindings"),
        )
        self.assertLess(
            source.index("Save_NativeResetSession"),
            source.index("VS::Core::Memory::ResetLiveGameHeap()"),
        )
        self.assertLess(
            source.index("Battle_Ai_InvalidateSessionBindings"),
            source.index("VS::Content::InitializeLiveDiscQueue"),
        )
        self.assertEqual(source.count("Battle_Ai_InvalidateSessionBindings"), 1)
        self.assertEqual(
            copy_fixups.NATIVE_HELPER_HEADERS["Battle_Ai_InvalidateSessionBindings"],
            r"Native\Game\src\Battle\Ai\RoomState.h",
        )

    def test_perception_uses_owned_model_prefixes_for_auxiliary_targets(self):
        # F01640's live binding keeps the old copy fixup: the target and its occluders resolve through their
        # battle actor's model, since valid auxiliary AI targets own model prefixes without an actor-record entry.
        self.assertNotIn("F01640", copy_fixups.FIXES)
        ai = copy_to_native.REPO / r"Native\Game\src\Battle\Ai"
        live = (ai / "ActorPerceptionLive.cpp").read_text(encoding="utf-8")
        world = live[live.index("class LiveTargetPerceptionWorld"):]
        model = world[world.index("std::optional<PerceivedModel> Model("):]
        model = model[:model.index("\n  }\n")]
        self.assertIn("Battle_actors[target]", model)
        self.assertIn("*actor->model", model)
        self.assertIn("Core::Math::PositionOf(model.position)", model)
        listed = world[world.index("std::vector<ListedPerceptionActor> ListedActors()"):]
        listed = listed[:listed.index("\n  }\n")]
        self.assertIn("Core::Math::PositionOf(actor.model->position)", listed)
        self.assertNotIn("Actors_actorRecords", model + listed)
        self.assertNotIn("(short*)", (ai / "ActorPerception.cpp").read_text(encoding="utf-8"))

    def test_optional_actor_wrappers_diagnose_and_return_before_dereferencing(self):
        for key in (
            "F00946",
            "F00947",
            "F00948",
            "F00949",
            "F00950",
            "F00952",
            "F00953",
        ):
            old, new, _ = copy_fixups.FIXES[key][0]
            self.assertIn("Actors_actorRecords[", old)
            self.assertIn("== NULL", new)
            self.assertIn("Core_Debugf", new)
            self.assertIn("return;", new)

    def test_optional_absolute_slide_checks_actor_before_position_access(self):
        source = """    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];
    sp10.vx = arg1->vx - temp_s1->instance.position.vx;
"""
        for old, new, _ in copy_fixups.FIXES["F00951"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertLess(source.index("if (temp_s1 == NULL)"), source.index("sp10.vx"))
        self.assertLess(source.index("return;"), source.index("sp10.vx"))
        self.assertIn("absolute slide skips absent actor=%d", source)

    def test_bone_endpoints_are_owned_by_the_skeleton_binding(self):
        # #276 row 20: the typed skeleton binding resolves the actor or gimmick
        # owner itself, so the owner-ordering and null-transform fixes retire
        # with the generated bodies.
        owned = self.owned_items()
        for identity in ("F00876", "F00882", "F00887"):
            with self.subTest(identity=identity):
                self.assertNotIn(identity, copy_fixups.FIXES)
                self.assertEqual(
                    owned[identity]["path"], r"Native\Game\src\Actors\Skeleton.cpp"
                )
        root = Path(__file__).resolve().parents[2]
        live = (root / r"Native\Game\src\Actors\SkeletonLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("Field_FindGimmickForActorSlot(actor_id)", live)
        self.assertNotIn("(Actors_ActorRecord*)", live)
        self.assertNotIn("Field_gimmickInstances[", live)

    def test_room_lifetimes_revoke_diagnostic_readiness(self):
        # #276 slice 25c: both room lifetimes revoke the inspection first, through their typed hosts.
        root = Path(__file__).resolve().parents[2]
        loading = (root / r"Native\Game\src\Field\RoomLoading.cpp").read_text(encoding="utf-8")
        for header in (
            "bool FinishRoomLoad(RoomLoadHost& host, int room_id) {",
            "void UnloadCurrentRoom(RoomUnloadHost& host) {",
        ):
            with self.subTest(header=header):
                body = loading.split(header, 1)[1]
                self.assertTrue(body.startswith("\n  host.ResetGameplayInspection();\n"))
        room_live = (root / r"Native\Game\src\Field\RoomLoadingLive.cpp").read_text(encoding="utf-8")
        self.assertEqual(
            room_live.count("void ResetGameplayInspection() override { Boot_ResetGameplayInspection(); }"), 2)

    def test_gimmick_palette_matches_ledger_element_width_without_growth(self):
        source = "u_char pulsedPalette[32];"
        for old, new, _ in copy_fixups.TYPE_FIXES["T00255"]:
            source = source.replace(old, new)
        self.assertEqual(source, "u_short pulsedPalette[16];")

    def test_room_rendering_dispatches_typed_appearance_owners(self):
        self.assertNotIn("F01008", copy_fixups.FIXES)
        live = self.model_rendering_live_source()
        self.assertIn("UpdateLiveActorAppearanceTransition(*actor);", live)
        self.assertIn("UpdateLiveGimmickAppearanceTransition(*object);", live)
        self.assertNotIn("(Actors_ActorRecord*)", live)

    def test_effect_register_consumers_preserve_original_operand_order(self):
        for function_id, function_name in (
            ("F01531", "Effects::SetEffectScriptRegister"),
            ("F01532", "Effects::BranchIfEffectRegisterEqual"),
        ):
            old, replacement, _ = copy_fixups.FIXES[function_id][0]
            source = old.replace(old, replacement)
            for row, pattern, value, _ in copy_fixups.REGEX_FIXES:
                if re.search(row, function_name):
                    source = re.sub(pattern, value, source)
            self.assertIn("registerIndex = Effects_ReadScriptByte(instance)", source)
            self.assertIn("value = Effects_ReadScriptHalfword(instance)", source)
            self.assertLess(source.index("registerIndex ="), source.index("value ="))
            self.assertNotIn("arg0", source)
            self.assertEqual(source.count("Effects_ReadScript"), 2)
        self.assertIn("(u_char)instance.registers", source.replace("->", "."))

    def test_score_preview_requests_use_initialized_native_record_storage(self):
        source = """Content_ActorAssetRequest slot;
slot.destination = Menu_Score_zudBuffer + 0x1900;
slot.destination = Menu_Score_zudBuffer + 0x1900;
slot.destination = Menu_Score_zudBuffer + 0x1EE8;
"""
        for old, new, _ in copy_fixups.FIXES["F02840"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
            if re.search(row, "Menu::Score::LoadEnemyModelParts"):
                source = re.sub(pattern, replacement, source)
        self.assertIn("Content_ActorAssetRequest request = {0};", source)
        self.assertEqual(source.count("sizeof(Actors_ActorRecord)"), 3)
        self.assertIn("sizeof(Actors_WeaponModel)", source)
        self.assertNotIn("0x1900", source)
        self.assertNotIn("0x1EE8", source)
        allocation = """VS::Core::Memory::AllocateFromLowEnd(0x3200);
VS::Core::Memory::AllocateFromLowEnd(0x24D0);
"""
        for old, new, why in copy_fixups.FIXES["F02839"]:
            if "#282" in why:
                continue
            self.assertIn(old, allocation)
            allocation = allocation.replace(old, new)
        self.assertIn("2 * sizeof(Actors_ActorRecord)", allocation)
        self.assertIn("2 * sizeof(Actors_WeaponModel)", allocation)

    def test_disc_request_callers_handle_a_full_queue(self):
        # main.c:9411 returns no request when all 32 are busy (#282).
        enqueue = re.compile(r"VS::Content::EnqueueLive(?:Priority)?DiscRequest\((.+?), ")
        guarded = [(fid, old, new) for fid, fixes in copy_fixups.FIXES.items()
                   for old, new, why in fixes if "#282" in why]
        # F03526 and F03547 left the copier for the owned SoundBanksCore.cpp StartLoad (#276 row 13), which
        # reads the bank without the queue when no request is free. F03496 left it for the owned MusicCore.cpp
        # LoadMusicSlot (#276 audio-music-streams), which reads the music the same way. F02442 (two reads) and
        # F02418 left it for the owned Ending cores (#276 row 54), whose EndingLive disc read completes a
        # refused request without the queue when no request is free. F01342 left it for the owned Framework.cpp
        # LoadMenuOverlay (#276 row 45), whose FrameworkLive.cpp BeginOverlayLoad keeps the guard below. F02769
        # left it for the owned Files.cpp LoadGameOverAssetsStep (#276 row 51), whose FilesLive.cpp
        # BeginGameOverLoad keeps the guard below. F01612 left it for the owned EffectResources.cpp
        # PollEffectResourceRead (#276 row 38), which completes a refused read through the host's
        # CompleteRefusedRead (EffectResourcesLive.cpp). F00176 left it for the owned ActorSpawning.cpp
        # SpawnRoomEnemy (#276 row 19c), whose ActorSpawningLive.cpp BeginZudRead keeps the guard below. F02503
        # left it for the owned ItemRows.cpp LoadItemText (#276 row 46), whose ItemRowsLive.cpp StartItemTextLoad
        # keeps the guard below. F00645 (two reads) left it for the owned Background.cpp QueueTiledBackgroundLoad
        # (#276 row 24), which completes a refused read through the host's CompleteRefusedRead (BackgroundLive.cpp).
        # F02899 left it for the owned Combination.cpp LoadCombinationRecipes (#276 row 49), whose SetupLive.cpp
        # StartRecipeLoad keeps the guard below. F00532 and F00542 left it for the owned RoomLoading.cpp
        # QueueZoneLoad and QueueRoomLoad (#276 row 25c), whose RoomLoadingLive.cpp BeginRead keeps the guard
        # below.
        self.assertGreaterEqual(len(guarded), 20)
        owned = self.owned_items()
        self.assertEqual(owned["F03526"]["path"], r"Native\Engine\src\Audio\SoundBanksCore.cpp")
        self.assertEqual(owned["F03547"]["path"], r"Native\Engine\src\Audio\SoundBanksCore.cpp")
        self.assertEqual(owned["F03496"]["path"], r"Native\Engine\src\Audio\MusicCore.cpp")
        self.assertEqual(owned["F02442"]["path"], r"Native\Game\src\Ending\ClearResultsCore.cpp")
        self.assertEqual(owned["F02418"]["path"], r"Native\Game\src\Ending\Credits\StaffRollCore.cpp")
        self.assertEqual(owned["F01342"]["path"], r"Native\Game\src\Menu\Framework\Framework.cpp")
        self.assertEqual(owned["F02769"]["path"], r"Native\Game\src\Menu\Files.cpp")
        self.assertEqual(owned["F01612"]["path"], r"Native\Game\src\Effects\EffectResources.cpp")
        self.assertEqual(owned["F00176"]["path"], r"Native\Game\src\Actors\ActorSpawning.cpp")
        self.assertEqual(owned["F02503"]["path"], r"Native\Game\src\Menu\Framework\ItemRows.cpp")
        self.assertEqual(owned["F00645"]["path"], r"Native\Game\src\Field\Background.cpp")
        self.assertEqual(owned["F02899"]["path"], r"Native\Game\src\Menu\Setup\Combination.cpp")
        self.assertEqual(owned["F00532"]["path"], r"Native\Game\src\Field\RoomLoading.cpp")
        self.assertEqual(owned["F00542"]["path"], r"Native\Game\src\Field\RoomLoading.cpp")
        root = Path(__file__).resolve().parents[2]
        background = "".join((root / r"Native\Game\src\Field\Background.cpp").read_text(encoding="utf-8").split())
        self.assertIn("state.texture_read=host.CompleteRefusedRead(section->texture,state.tim_buffer);", background)
        self.assertIn("state.layout_read=host.CompleteRefusedRead(section->layout,state.layout);", background)
        background_live = (root / r"Native\Game\src\Field\BackgroundLive.cpp").read_text(encoding="utf-8")
        self.assertIn("Content::CompleteLiveRefusedDiscRequest(", background_live)
        room_loading_live = (root / r"Native\Game\src\Field\RoomLoadingLive.cpp").read_text(encoding="utf-8")
        self.assertIn("request = Content::CompleteLiveRefusedDiscRequest(&disc_file, data);", room_loading_live)
        background_tests = (root / r"Native\Tests\src\FieldBackgroundTests.cpp").read_text(encoding="utf-8")
        self.assertIn('"CompleteRefusedRead 100 1", "CompleteRefusedRead 200 2"', background_tests)
        effects = "".join((root / r"Native\Game\src\Effects\EffectResources.cpp").read_text(encoding="utf-8").split())
        self.assertIn("}else{state.read=host.CompleteRefusedRead(read,destination.id);}", effects)
        effects_live = (root / r"Native\Game\src\Effects\EffectResourcesLive.cpp").read_text(encoding="utf-8")
        self.assertIn("Content::CompleteLiveRefusedDiscRequest(", effects_live)
        effects_tests = (root / r"Native\Tests\src\EffectResourcesTransformsTests.cpp").read_text(encoding="utf-8")
        self.assertIn("Effect resource reads complete a read the queue refuses", effects_tests)
        core = (root / r"Native\Engine\src\Audio\SoundBanksCore.cpp").read_text(encoding="utf-8")
        self.assertIn("host.CompleteRefusedRequest(file, slot.data)", core)
        music = (root / r"Native\Engine\src\Audio\MusicCore.cpp").read_text(encoding="utf-8")
        self.assertIn("host.CompleteRefusedRequest(*file, music.data)", music)
        ending = "".join((root / r"Native\Game\src\Ending\EndingLive.cpp").read_text(encoding="utf-8").split())
        self.assertIn("if(slot_==nullptr){slot_=Content::CompleteLiveRefusedDiscRequest(&file_,buffer);}else", ending)
        tests = (root / r"Native\Tests\src\AudioPlaybackBanksTests.cpp").read_text(encoding="utf-8")
        self.assertIn("A full disc queue reads the sound-effect bank without it", tests)
        self.assertIn("A full disc queue reads the wave bank without it", tests)
        music_tests = (root / r"Native\Tests\src\AudioMusicStreamTests.cpp").read_text(encoding="utf-8")
        self.assertIn("A music read the disc queue refuses still completes into the music buffer", music_tests)
        for fid, old, new in guarded:
            with self.subTest(fid=fid):
                old, new = "".join(old.split()), "".join(new.split())
                call = re.search(r"VS::Content::EnqueueLive\w*\((.+?),.*?\);", old)
                self.assertIn(f"if({call.group(1)}!=NULL){{{call.group(0)}}}", new)
                if fid != "F01425":
                    self.assertIn(
                        f"}}else{{{call.group(1)}=VS::Content::CompleteLiveRefusedDiscRequest(", new)
        root = Path(__file__).resolve().parents[2]
        sites = 0
        for path in (root / "Native").rglob("*.cpp"):
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
            if lines and lines[0].startswith("// Hand-maintained"):
                continue
            for index, line in enumerate(lines):
                match = enqueue.search(line)
                if match:
                    sites += 1
                    previous = next(text.strip() for text in reversed(lines[:index]) if text.strip())
                    self.assertEqual(previous, f"if ({match.group(1)} != NULL) {{", f"{path}:{index + 1}")
        # #276 rows 17 and 13 moved six sites into owned code: Lifecycle.cpp's four overlay loads became the one
        # guarded SessionLive.cpp load, and SoundBanks.cpp's two bank loads became SoundBanksCore.cpp StartLoad.
        # The audio-music-streams row moved Music.cpp's music load into MusicCore.cpp LoadMusicSlot. Row 54 moved
        # ClearResults.cpp's two results reads and StaffRoll.cpp's illustration read into EndingLive.cpp LiveDiscRead,
        # and row 45 moved the menu overlay load into FrameworkLive.cpp BeginOverlayLoad. Row 51 moved Files.cpp's
        # game-over artwork load into FilesLive.cpp BeginGameOverLoad. Row 38 moved EffectResources.cpp's resource
        # read into the owned PollEffectResourceRead. Row 19c moved ActorLoading.cpp's ZUD read into
        # ActorSpawningLive.cpp BeginZudRead, and row 46 moved ItemRows.cpp's item text load into
        # ItemRowsLive.cpp StartItemTextLoad. Row 24 moved Background.cpp's tiled TIM and layout reads into the
        # owned QueueTiledBackgroundLoad, whose BackgroundLive.cpp EnqueueRead runs only for an allocated read.
        # Row 49 moved Combination.cpp's recipe read into SetupLive.cpp StartRecipeLoad. Row 25c moved
        # RoomLoading.cpp's ZND and MPD reads into RoomLoadingLive.cpp BeginRead.
        event_source = (root / r"Native\Game\src\Event\ScriptVmLive.cpp").read_text(encoding="utf-8")
        event = "".join(re.sub(r"//[^\n]*", "", event_source).split())
        self.assertIn(
            "if(request_!=nullptr){request_generation_=Content::LiveDiscRequestGeneration(request_);"
            "Content::EnqueueLiveDiscRequest(request_,pending_buffer_->data());",
            event,
        )
        self.assertIn(
            "}else{request_=Content::CompleteLiveRefusedDiscRequest(&pending_file_,pending_buffer_->data());",
            event,
        )
        sites += 1
        self.assertGreaterEqual(sites, 15)
        session = "".join((root / r"Native\Game\src\Boot\SessionLive.cpp").read_text(encoding="utf-8").split())
        self.assertIn("if(slot!=nullptr){VS::Content::EnqueueLivePriorityDiscRequest(", session)
        self.assertIn("}else{slot=VS::Content::CompleteLiveRefusedDiscRequest(", session)
        menu = (root / r"Native\Game\src\Menu\Framework\FrameworkLive.cpp").read_text(encoding="utf-8")
        menu = "".join(menu.split())
        self.assertIn("if(control.cdQueue!=nullptr){VS::Content::EnqueueLiveDiscRequest(control.cdQueue,", menu)
        self.assertIn("}else{control.cdQueue=VS::Content::CompleteLiveRefusedDiscRequest(", menu)
        files = "".join((root / r"Native\Game\src\Menu\FilesLive.cpp").read_text(encoding="utf-8").split())
        self.assertIn("if(g_game_over_request!=nullptr){Content::EnqueueLiveDiscRequest(g_game_over_request,", files)
        self.assertIn("}else{g_game_over_request=Content::CompleteLiveRefusedDiscRequest(&file,", files)
        spawning = "".join((root / r"Native\Game\src\Actors\ActorSpawningLive.cpp").read_text(encoding="utf-8").split())
        self.assertIn("if(request!=nullptr){Content::EnqueueLivePriorityDiscRequest(request,", spawning)
        self.assertIn("}else{request=Content::CompleteLiveRefusedDiscRequest(&disc,Field_zoneContext.zudData);}", spawning)
        items = "".join((root / r"Native\Game\src\Menu\Framework\ItemRowsLive.cpp").read_text(encoding="utf-8").split())
        self.assertIn("if(g_item_text_request!=nullptr){Content::EnqueueLiveDiscRequest(g_item_text_request,", items)
        self.assertIn("}else{g_item_text_request=Content::CompleteLiveRefusedDiscRequest(&file,block);}", items)
        setup = "".join((root / r"Native\Game\src\Menu\Setup\SetupLive.cpp").read_text(encoding="utf-8").split())
        self.assertIn("if(g_recipes.request!=nullptr){Content::EnqueueLiveDiscRequest(g_recipes.request,data);}", setup)
        self.assertIn("}else{g_recipes.request=Content::CompleteLiveRefusedDiscRequest(&request,data);}", setup)

    def test_heap_callers_use_the_live_binding_and_disc_ring_is_bytes(self):
        names = ("AllocateFromLowEnd", "AllocateFromHighEnd", "FreeFromLowEnd", "FreeFromHighEnd")
        for name in names:
            with self.subTest(name=name):
                source = f"block = VS::Core::Memory::{name}(size);"
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, "Content::Unrelated"):
                        source = re.sub(pattern, replacement, source)
                live = name.replace("From", "LiveFrom")
                self.assertEqual(source, f"block = VS::Core::Memory::{live}(size);")
                self.assertEqual(
                    copy_fixups.NATIVE_HELPER_HEADERS[f"VS::Core::Memory::{live}"],
                    r"Native\Engine\include\Core\Memory\HeapLive.hpp",
                )
        source = "extern Core_HeapBlockHeader* Content_legacyDiscReadBuffer;"
        for old, new, _ in copy_fixups.FIXES["G01921"]:
            source = source.replace(old, new)
        self.assertEqual(source, "extern u_char* Content_legacyDiscReadBuffer;")
        self.assertNotIn("F03468", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        drive = " ".join((root / r"Native\Content\src\CdDriveLive.cpp").read_text(encoding="utf-8").split())
        self.assertIn("constexpr std::uint32_t kSectorBytes = 2048;", drive)
        self.assertIn("Content_legacyDiscReadBuffer + static_cast<std::uint32_t>(slot * kSectorBytes)", drive)
        bound = {name for _, _, name in copy_fixups.MODULE_BINDING_ENTRIES}
        for name in ("FreeLiveFromHighEndAt", "FreeLiveFromLowEndAt", "AllocateLiveFromHighEndAt",
                     "AllocateLiveFromLowEndAt", "InitializeLiveHeapAt"):
            self.assertIn(f"VS::Core::Memory::{name}", bound)

    def test_math_live_bindings_preserve_destination_positions_and_order(self):
        # The generated bodies and their rename rules were retired by the
        # typed Core::Math slice; the Live adapters now carry each original
        # argument order, and no rename rule targets the retired names.
        retired = (
            "Core::Math::AddVectors", "Core::Math::AddWideVectors",
            "Core::Math::NormalizeFixedVector",
            "Core::Math::InterpolateTimedScalar",
            "Core::Math::InterpolateShortPair",
            "Core::Math::InterpolateThirteenBitScalar",
        )
        for name in retired:
            with self.subTest(name=name):
                self.assertFalse(
                    [row for row, _, _, _ in copy_fixups.REGEX_FIXES
                     if row and re.search(row, name)]
                )
        root = Path(__file__).resolve().parents[2]
        include = root / r"Native\Engine\include\Core\Math"
        vector = " ".join(
            (include / "VectorLive.hpp").read_text(encoding="utf-8").split()
        )
        for signature in (
            "Destination* AddLiveVectors(Destination* destination, "
            "const Left* left, const Right* right) noexcept",
            "void AddLiveVectorsTo(const Left* left, const Right* right, "
            "Destination* destination) noexcept",
            "void NormalizeLiveFixedVector(const Source* source, "
            "Destination* destination) noexcept",
        ):
            self.assertIn(signature, vector)
        interpolation = " ".join(
            (include / "InterpolationLive.hpp").read_text(encoding="utf-8").split()
        )
        self.assertIn(
            "std::int32_t start, std::int32_t end, std::int32_t total, "
            "std::int32_t current) noexcept { return InterpolateTimedScalar("
            "{start, end}, {current, total});",
            interpolation,
        )
        self.assertIn(
            "InterpolateLiveShortPair( std::span<const std::int16_t, "
            "kScalarRangeHalfwords> endpoints, std::int32_t weight) noexcept",
            interpolation,
        )

    def test_weapon_sequence_copy_names_preserve_the_native_offset_contract(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            row = next(item for item in csv.DictReader(stream)
                       if item["id"] == "F00797")
        self.assertEqual(row["name"], "func_8009C378")
        self.assertEqual(row["final_name"], "Content::CopyWeaponSequence")
        # #276 slice 16: the typed core copies the fixed block without relocating the
        # on-disc section offsets and records the clone's extent (#261).
        self.assertNotIn("F00797", copy_fixups.FIXES)
        source = " ".join((root / row["final_location"]).read_text(encoding="utf-8").split())
        body = source.split("void CopyWeaponSequence(ActorAssetHost& host, AssetAddress destination,", 1)[1]
        body = body.split(" } ", 1)[0]
        self.assertIn("kWeaponSequenceBytes", body)
        self.assertIn("host.RecordSequenceCopy(destination, source, kWeaponSequenceBytes);", body)
        self.assertNotIn("VS_ONDISC", body)
        header = (root / r"Native\Content\include\Content\ActorAssets.hpp").read_text(encoding="utf-8")
        self.assertIn("inline constexpr std::size_t kWeaponSequenceBytes = 0x7800;", header)
        live = " ".join((root / r"Native\Content\src\ActorAssetsLive.cpp").read_text(encoding="utf-8").split())
        self.assertIn("Actors::RegisterSequenceBankCopy(Pointer(destination), Pointer(source),", live)

    def test_audio_music_stream_names_left_the_copier_for_owned_cores(self):
        # #276 audio-music-streams: the typed cores name these parameters, so no generated rename or disc-request
        # fixup remains, and the generated callers reach the live bindings.
        owned = self.owned_items()
        audio = r"Native\Engine\src\Audio"
        rows = {row for row, *_ in copy_fixups.REGEX_FIXES}
        for function_id, name, path in (
            ("F00063", "PlayRandomActorImpactSound", rf"{audio}\ActorSoundCore.cpp"),
            ("F00994", "ActorSoundPanAndVolume", rf"{audio}\ActorSoundCore.cpp"),
            ("F03496", "LoadMusicSlot", rf"{audio}\MusicCore.cpp"),
            ("F03381", "Cutscene::UploadSecondMonoBuffer", rf"{audio}\Cutscene\StreamBuffersCore.cpp"),
            ("F03542", "PlayCutsceneStreamSlot", rf"{audio}\StreamCore.cpp"),
            ("F00626", "SetRoomAmbienceDucked", rf"{audio}\AmbienceCore.cpp"),
        ):
            with self.subTest(function_id=function_id):
                self.assertEqual(owned[function_id]["name"], f"VS::Audio::{name}")
                self.assertEqual(owned[function_id]["path"], path)
                self.assertNotIn(function_id, copy_fixups.FIXES)
                self.assertNotIn(rf"^Audio::{name}$", rows)
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F00994"], "VS::Audio::LiveActorSoundPanAndVolume")
        self.assertEqual(copy_fixups.OWNED_CALL_BINDINGS["F01001"], "VS::Audio::LivePlayActorSoundPair")

    def test_actor_sound_pan_and_volume_caller_drops_the_unread_argument(self):
        # #276 audio-music-streams: 30DB0.c passes func_800AD62C a fourth argument it never reads (40564.c:2789).
        # #276 slice 20b: the caller (F00843) is owned; its Live host passes the actor index and the two outputs.
        root = Path(__file__).resolve().parents[2]
        live = (root / r"Native\Game\src\Actors\AnimationControlLive.cpp").read_text(encoding="utf-8")
        self.assertIn("Audio::LiveActorSoundPanAndVolume(id, &result.pan, &result.volume);", live)
        self.assertNotIn("F00843", copy_fixups.FIXES)
        for function_id in ("F00926", "F00913"):
            with self.subTest(function_id=function_id):
                self.assertFalse([old for old, *_ in copy_fixups.FIXES.get(function_id, [])
                                  if "LivePlayActorSoundPair" in old])

    def test_remaining_event_names_preserve_snapshot_and_retained_bindings(self):
        examples = {
            "Event::ScriptEndCutsceneAndJump": (
                "temp_v1 = state; state = 0; if (temp_v1) Cleanup();",
                "previous_skip_state = state; state = 0; "
                "if (previous_skip_state) Cleanup();",
            ),
            "Event::RestoreScriptTextureRectangle": (
                "temp_s0 = Allocate(); SetRect(&sp10); "
                "new_var = 0x1F800000; new_var = 0x10;",
                "move_primitive = Allocate(); SetRect(&source_rectangle); "
                "new_var = 0x1F800000; new_var = 0x10;",
            ),
            "Event::QueueEventScriptLoad": (
                "int Load(int arg0) { return CurrentEvent(); }",
                "int Load(int arg0) { return CurrentEvent(); }",
            ),
        }
        for name, (original, expected) in examples.items():
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_actor_projection_calls_the_bounded_live_binding_directly(self):
        # The owned typed name removes the function-pointer cast, so the tagged
        # bone wrappers call the binding that applies the #263 bone bounds.
        for row in ("F00876", "F00877"):
            for old, new, _ in copy_fixups.FIXES.get(row, []):
                self.assertNotIn("ProjectActorBoneEndpoint)", old)
                self.assertNotIn("ProjectActorBoneEndpoint)", new)
        source = "v1 = VS::Actors::ProjectActorBoneEndpoint(arg0, var_v0, (int*)arg2, arg3);"
        for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
            if row == "":
                source = re.sub(pattern, replacement, source)
        self.assertIn(
            "VS::Actors::ProjectLiveActorBoneEndpoint(arg0, var_v0, (int*)arg2, arg3)",
            source,
        )
        root = Path(__file__).resolve().parents[2]
        skeleton = (root / r"Native\Game\src\Actors\SkeletonLive.cpp").read_text(
            encoding="utf-8"
        )
        self.assertRegex(
            skeleton,
            r"ProjectLiveActorBoneEndpoint\(\s*actor_id, bone, projected\.data\(\), endpoint\)",
        )
        self.assertRegex(
            skeleton,
            r"ProjectLiveActorBoneEndpoint\(\s*actor_id, bone, screen_out, endpoint\)",
        )
        self.assertNotIn("ProjectActorBoneEndpoint)", skeleton)

    def test_actor_texture_origin_scopes_keep_distinct_local_bindings(self):
        # The setup is owned (#276 row 22): both texture slot loops read the
        # origin table through one bounded halfword reader.
        self.assertNotIn("F01860", copy_fixups.FIXES)
        live = self.model_rendering_live_source()
        self.assertIn(
            "slot, SlotOriginHalfword(slot, 0), SlotOriginHalfword(slot, 1));", live
        )
        self.assertNotIn("new_var", live)
        patterns = [
            pattern for row, pattern, _, _ in copy_fixups.REGEX_FIXES
            if re.search(row, "Actors::InitializeActorModelResources")
        ]
        self.assertNotIn(r"\bnew_var\b", patterns)

    def model_rendering_live_source(self):
        root = Path(__file__).resolve().parents[2]
        return (root / r"Native\Game\src\Actors\ModelRenderingLive.cpp").read_text(
            encoding="utf-8"
        )

    def test_menu_names_preserve_choice_arithmetic_and_retained_speed_member(self):
        examples = {
            "Menu::SelectTextBoxChoice": (
                "int v0 = selectedIndex - 1; selectedIndex = v0 + count;",
                "int previous_choice_index = selectedIndex - 1; "
                "selectedIndex = previous_choice_index + count;",
            ),
            "Menu::SetTextBoxText": (
                "box->speed = 1; if (box->speed != 0) Continue();",
                "box->speed = 1; if (box->speed != 0) Continue();",
            ),
        }
        for name, (original, expected) in examples.items():
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_menu_matrix_aliases_apply_each_binding_rule_once(self):
        for name in ("Menu::Framework::BuildYawMatrix",
                     "Menu::Framework::BuildPitchMatrix"):
            with self.subTest(name=name):
                patterns = [
                    pattern for row, pattern, _, _ in copy_fixups.REGEX_FIXES
                    if re.search(row, name)
                ]
                self.assertTrue(patterns)
                self.assertEqual(len(patterns), len(set(patterns)))

    def test_battle_hud_shadowed_locals_preserve_screen_and_texture_units(self):
        names = ("clipped_icon_left", "overlay_texture_end")
        for name in names:
            with self.subTest(name=name):
                matches = [
                    (old, new) for old, new, _ in copy_fixups.FIXES["F01355"]
                    if name in new
                ]
                self.assertEqual(len(matches), 1)
                old, new = matches[0]
                self.assertEqual(new, re.sub(r"\bv0\b", name, old))
        patterns = [
            pattern for row, pattern, _, _ in copy_fixups.REGEX_FIXES
            if re.search(row, "Battle::DrawSlidingStatusIcon")
        ]
        self.assertNotIn(r"\bv0\b", patterns)

    def test_battle_repeated_cast_prototypes_keep_declared_types_and_roles(self):
        original = "int arg0, int arg1, u_int arg2, int arg3"
        expected = "int actor_slot, int limb_tag, u_int encoded_amount, int label_flags"
        matches = [
            (old, new) for old, new, _ in copy_fixups.FIXES["F00523"]
            if old == original
        ]
        self.assertEqual(matches, [(original, expected)])
        self.assertEqual(
            (original + "; " + original).replace(*matches[0]),
            expected + "; " + expected,
        )

    def test_effect_shared_body_names_supply_instance_prototypes(self):
        source = (
            "int VS_CLOSED_RING_FUNCTION(Effects_RendererSlot* arg0, "
            "u_int arg1, int arg2) { return arg1; }"
        )
        for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
            if re.search(row, "Effects::Templates::ClosedRing"):
                source, _ = copy_to_native.substitute_fixup(source, pattern, replacement)
        head = copy_to_native.Copier.head_of(source)
        self.assertEqual(
            head.replace("VS_CLOSED_RING_FUNCTION", "Effects_Plg047_ClosedRing"),
            "int Effects_Plg047_ClosedRing(Effects_RendererSlot* renderer_slot, "
            "u_int lifecycle_command, int packed_parameters)",
        )

    def test_effect_timeline_shadow_declaration_is_not_the_active_track(self):
        old = "    Effects_TimelineTrack* temp_a1;"
        new = "    Effects_TimelineTrack* unused_track_declaration;"
        self.assertIn(
            (old, new),
            [(before, after) for before, after, _ in copy_fixups.FIXES["F01589"]],
        )
        source = old + "\n{ Effects_TimelineTrack* temp_a1 = tracks; Use(temp_a1); }"
        source = source.replace(old, new, 1)
        for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
            if re.search(row, "Effects::TickEffectTimeline"):
                source, _ = copy_to_native.substitute_fixup(source, pattern, replacement)
        self.assertIn("Effects_TimelineTrack* unused_track_declaration;", source)
        self.assertIn("Effects_TimelineTrack* timeline_track = tracks; Use(timeline_track);", source)

    def test_texture_page_macro_name_preserves_packet_and_handle_contracts(self):
        macro = copy_fixups.MACRO_REPLACEMENTS["_insertTPage"]
        self.assertIn("#define _insertTPage(after, texture_page_mode)", macro)
        self.assertNotRegex(macro, r"\barg1\b")
        self.assertIn("0xE1000200 | ((texture_page_mode) & 0x1FF)", macro)
        self.assertIn("LegacyGpu_Handle(p_)", macro)
        self.assertIn("Render_frameScratch.primitiveCursor = p_ + 3;", macro)

    def test_render_names_preserve_member_names_and_distinct_numeric_lifetimes(self):
        cases = (
            (
                "Render::DrawGimFramebufferFade",
                "int temp_s0 = arg0->brightness; Draw(arg0, arg1, arg2);",
                "int brightness = image->brightness; Draw(image, clut, ordering_table);",
            ),
            (
                "Render::DrawTextureWipe",
                "var_a0 = 12; temp_a1 = x + var_a0; var_a0 = arg4 - x; var_s6 += 12;",
                "strip_width_or_fade_weight = 12; strip_right_x = x + strip_width_or_fade_weight; "
                "strip_width_or_fade_weight = wipe_edge_x - x; texture_u += 12;",
            ),
        )
        for name, source, expected in cases:
            with self.subTest(name=name):
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source, _ = copy_to_native.substitute_fixup(source, pattern, replacement)
                self.assertEqual(source, expected)

    def test_render_palette_block_bindings_preserve_scopes_and_provenance(self):
        block = "{ int v0 = 2; int v1 = 3; int a0 = 4; }"
        expected = (
            "{ int base_luminance_work = 2; int base_green_work = 3; "
            "int base_blue_work = 4; }"
        )
        for name in ("Render::TransformBackgroundGradient", "Render::TransformModelTint"):
            with self.subTest(name=name):
                source = block + "\n" + block + "\n// v0, v1, a0\n"
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source, _ = copy_to_native.substitute_fixup(source, pattern, replacement)
                self.assertEqual(source, expected + "\n" + expected + "\n// v0, v1, a0\n")

    def test_render_unproven_and_unused_arguments_remain_explicit_retentions(self):
        for name, retained in (
            ("Render::InitializeLoadingDisplay", "arg2 arg3 arg4 arg5"),
            ("Render::SetFramebufferEffectParameter", "arg0"),
            ("Render::ActivateGimLayer", "arg1 arg2"),
            ("Render::LoadRoomPaletteTransition", "arg2"),
            ("Render::SetModelTint", "v0"),
        ):
            with self.subTest(name=name):
                source = retained
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source, _ = copy_to_native.substitute_fixup(source, pattern, replacement)
                self.assertEqual(source, retained)

    def test_loaded_save_validation_names_preserve_true_means_apply(self):
        # #276 slice 53: TITLE.PRG's _applyLoadedSaveFile (F03600) is the typed
        # VS::Save::MemoryCard::ValidateAndApplyLoadedSave, whose flag is named for its true meaning.
        owned = self.owned_items()
        self.assertEqual(owned["F03600"]["name"], "VS::Save::MemoryCard::ValidateAndApplyLoadedSave")
        self.assertNotIn("F03600", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        header = (root / r"Native\Engine\include\Save\MemoryCard\GameFiles.hpp").read_text(encoding="utf-8")
        self.assertRegex(header, r"ValidateAndApplyLoadedSave\(\s*SaveStateHost& state, SaveImage incoming, bool apply\)")
        self.assertNotIn("verifyOnly", header)

    def test_lifecycle_scoped_names_preserve_exact_declaration_shapes(self):
        # #276 slice 53 promoted the TITLE.PRG functions these declaration renames applied to
        # (F03602, F03623, F03626 and F03663) to owned typed C++; no generated copy remains to rename.
        owned = self.owned_items()
        for identity in ("F03602", "F03623", "F03626", "F03663"):
            with self.subTest(identity=identity):
                self.assertNotIn(identity, copy_fixups.FIXES)
                self.assertIn(identity, owned)
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Title\TitleMenu.cpp").read_text(encoding="utf-8")
        self.assertIn("const int scanline = line + item.text_position;", core)

    def test_save_format_field_names_keep_original_identity_and_storage(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FIELD_LEDGER.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            fields = {row["id"]: row for row in csv.DictReader(stream)}
        for identity, original, name in (
            ("T00594.002", "unk8", "formatSignature"),
            ("T00596.002", "unk20", "cardTitleTail"),
        ):
            with self.subTest(identity=identity):
                self.assertEqual(fields[identity]["name"], original)
                self.assertEqual(fields[identity]["final_name"], name)
        header = (root / r"Native\Engine\src\Save\MemoryCard\GameFiles.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("    int formatSignature;", header)
        self.assertIn("    u_short cardTitleTail[16];", header)

    def test_identifier_fixups_see_code_between_numeric_separator_literals(self):
        source = "auto n = 1'000; int arg0 = 7; auto m = 2'000; // arg0\n"
        changed, count = copy_to_native.substitute_fixup(source, r"\barg0\b", "value")
        self.assertEqual(changed, source.replace("int arg0", "int value"))
        self.assertEqual(count, 1)

    def test_regex_fixups_compile_each_distinct_selector_once(self):
        copy_to_native.compiled_fixup_regex.cache_clear()
        self.addCleanup(copy_to_native.compiled_fixup_regex.cache_clear)
        copier = object.__new__(copy_to_native.Copier)
        copier.used_fixups = set()
        rules = [
            (rf"^RegexCacheFixture::{index}$", r"\barg0\b", "value", "fixture")
            for index in range(600)
        ]
        row = {"id": "REGEX_CACHE_FIXTURE", "final_name": "Unrelated::NoMatch"}
        re.purge()
        with (
            mock.patch.object(copy_fixups, "REGEX_FIXES", rules),
            mock.patch("re._compiler.compile", wraps=re._compiler.compile) as compile_regex,
        ):
            for _ in range(3):
                self.assertEqual(copier.apply_fixups(row, "arg0"), "arg0")
            self.assertEqual(compile_regex.call_count, 600)
            compile_regex.reset_mock()
            self.assertEqual(copier.apply_fixups(row, "arg0"), "arg0")
            self.assertEqual(compile_regex.call_count, 0)

    def test_regex_fixups_keep_order_and_observe_rule_list_changes(self):
        copy_to_native.compiled_fixup_regex.cache_clear()
        self.addCleanup(copy_to_native.compiled_fixup_regex.cache_clear)
        copier = object.__new__(copy_to_native.Copier)
        copier.used_fixups = set()
        row = {"id": "REGEX_ORDER_FIXTURE", "final_name": "Fixture::Ordered"}
        rules = [
            (r"^Fixture::Ordered$", r"\barg0\b", "middle", "first"),
            (r"^Fixture::Ordered$", r"\bmiddle\b", "result", "second"),
        ]
        with (
            mock.patch.dict(copy_fixups.FIXES, {row["id"]: [("SOURCE", "arg0", "literal")]}),
            mock.patch.object(copy_fixups, "REGEX_FIXES", rules),
        ):
            self.assertEqual(copier.apply_fixups(row, "SOURCE"), "result")
            self.assertEqual(
                copier.used_fixups,
                {(row["id"], "SOURCE"), (rules[0][0], rules[0][1]),
                 (rules[1][0], rules[1][1])},
            )
            rules.append((r"^Fixture::Ordered$", r"\bresult\b", "updated", "third"))
            self.assertEqual(copier.apply_fixups(row, "SOURCE"), "updated")
            rules[0] = (rules[0][0], rules[0][1], "different", "replacement changed")
            self.assertEqual(copier.apply_fixups(row, "SOURCE"), "different")

    def test_regex_fixups_preserve_search_flags_and_backreferences(self):
        copy_to_native.compiled_fixup_regex.cache_clear()
        self.addCleanup(copy_to_native.compiled_fixup_regex.cache_clear)
        copier = object.__new__(copy_to_native.Copier)
        copier.used_fixups = set()
        row = {"id": "REGEX_FLAGS_FIXTURE", "final_name": "Prefix::FIXTURE::Nested"}
        rule = (r"(?i)fixture::", r"(?i)\barg([01])\b", r"parameter_\1", "fixture")
        with mock.patch.object(copy_fixups, "REGEX_FIXES", [rule]):
            self.assertEqual(
                copier.apply_fixups(row, "ARG0 + arg1"),
                "parameter_0 + parameter_1",
            )
            self.assertEqual(copier.used_fixups, {(rule[0], rule[1])})

    def test_identifier_fixups_preserve_comments_and_literal_data(self):
        copier = object.__new__(copy_to_native.Copier)
        copier.used_fixups = set()
        source = r'''int arg0 = 0; // arg0
/* arg0 */ const char* text = "arg0 \"arg0";
const char* raw = R"tag(arg0 // still literal)tag";
int character = 'arg0';
arg0++;
int arg01 = arg0;'''
        expected = source.replace("int arg0 = 0;", "int actor_id = 0;")
        expected = expected.replace("arg0++;", "actor_id++;")
        expected = expected.replace("int arg01 = arg0;", "int arg01 = actor_id;")
        rule = (r"^Fixture$", r"\barg0\b", "actor_id", "identifier only")
        with mock.patch.object(copy_fixups, "REGEX_FIXES", [rule]):
            self.assertEqual(
                copier.apply_fixups({"id": "IDENTIFIER_FIXTURE",
                                     "final_name": "Fixture"}, source),
                expected,
            )
        self.assertEqual(copier.used_fixups, {(rule[0], rule[1])})

    def test_explicit_literal_fixups_keep_their_existing_regex_contract(self):
        copier = object.__new__(copy_to_native.Copier)
        copier.used_fixups = set()
        rule = (r"^Fixture$", r'"legacy-name"', '"native-name"', "explicit literal")
        with mock.patch.object(copy_fixups, "REGEX_FIXES", [rule]):
            self.assertEqual(
                copier.apply_fixups(
                    {"id": "LITERAL_FIXTURE", "final_name": "Fixture"},
                    'const char* name = "legacy-name";',
                ),
                'const char* name = "native-name";',
            )
        self.assertEqual(copier.used_fixups, {(rule[0], rule[1])})

    def test_room_actor_cache_opcode_name_preserves_original_provenance(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            row = next(item for item in csv.DictReader(stream)
                       if item["id"] == "F01099")
        self.assertEqual(row["name"], "func_800B91C4")
        self.assertEqual(row["final_name"], "Event::ScriptClearRoomActorCache")
        source = (root / row["final_location"]).read_text(encoding="utf-8")
        self.assertIn("func_800B91C4", source)
        self.assertIn("Event_ScriptClearRoomActorCache", source)
        self.assertNotIn("Event_ScriptPreloadRoom", source)
        # F00231 duplicates ForgetRoomEnemies, so the copy calls the typed core.
        self.assertIn(
            "VS::Actors::ForgetLiveRoomEnemies(instruction[2], instruction[1]);",
            source,
        )
        self.assertNotIn("Field_ClearSavedRoomEnemies", source)

    def test_event_parameter_scope_matches_registered_opcode_handlers(self):
        root = Path(__file__).resolve().parents[2]
        with (root / r"Docs\ledgers\FUNCTION_CATALOG.csv").open(
            encoding="utf-8", newline=""
        ) as stream:
            scope = {
                row["id"] for row in csv.DictReader(stream)
                if re.match(r"^Event::(?:Script|Command)", row["final_name"])
            }
        self.assertEqual(scope, copy_fixups.EVENT_OPCODE_HANDLERS)

    def test_event_opcode_parameter_names_follow_the_dispatch_contract(self):
        examples = (
            "Event::ScriptBranchOnFlagImmediate",
            "Event::ScriptPlayActorAnimation",
            "Event::CommandStartScriptContext",
            "Event::CommandLoadNextEvent",
        )
        original = (
            "intptr_t Handler(u_char* arg0, short arg1) "
            "{ return arg0[Event_threadWaitFrames[arg1]]; }"
        )
        expected = (
            "intptr_t Handler(u_char* instruction, short thread_index) "
            "{ return instruction[Event_threadWaitFrames[thread_index]]; }"
        )
        for name in examples:
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_event_opcode_naming_does_not_change_non_handler_parameters(self):
        original = "int Utility(int arg0, int arg1) { return arg0 + arg1; }"
        opcode_rules = [
            rule for rule in copy_fixups.REGEX_FIXES
            if rule[0] == r"^Event::(?:Script|Command)"
        ]
        self.assertEqual(
            {pattern for _, pattern, _, _ in opcode_rules},
            {r"\barg0\b", r"\barg1\b"},
        )
        for name in ("Event::EvaluateRoomEventTriggers", "Event::OpcodeSize"):
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in opcode_rules:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, original)

    def test_event_helpers_receive_only_their_reviewed_parameter_names(self):
        examples = {
            "Event::EvaluateRoomEventTriggers": (
                "int Utility(int arg0, int arg1) { return arg0 + arg1; }",
                "int Utility(int trigger_request, int event_or_actor_id) "
                "{ return trigger_request + event_or_actor_id; }",
            ),
            "Event::OpcodeSize": (
                "int Size(u_char* arg0) { return sizes[arg0[0]]; }",
                "int Size(u_char* instruction) "
                "{ return sizes[instruction[0]]; }",
            ),
        }
        for name, (original, expected) in examples.items():
            with self.subTest(name=name):
                source = original
                for row, pattern, replacement, _ in copy_fixups.REGEX_FIXES:
                    if re.search(row, name):
                        source = re.sub(pattern, replacement, source)
                self.assertEqual(source, expected)

    def test_geometry_copy_preserves_offsets_and_verified_parameter_names(self):
        # #276 slice 16: the typed core copies the SHP bytes and leaves the four
        # section members as serialized offsets (30DB0.c:507 relocated them in place).
        self.assertNotIn("F00786", copy_fixups.FIXES)
        # The copier applies no fixups to an owned row; the core is hand-maintained.
        with Path(__file__).resolve().with_name("owned.csv").open(encoding="utf-8") as registry:
            owned = {row["id"]: row for row in csv.DictReader(registry)}
        self.assertEqual(owned["F00786"]["path"], r"Native\Content\src\ActorAssets.cpp")
        self.assertEqual(owned["F00786"]["name"], "VS::Content::CopyModelGeometry")
        root = Path(__file__).resolve().parents[2]
        source = " ".join((root / r"Native\Content\src\ActorAssets.cpp").read_text(encoding="utf-8").split())
        body = source.split("AssetAddress CopyModelGeometry(ActorAssetHost& host, AssetAddress destination,", 1)[1]
        body = body.split(" } // decomp:", 1)[0]
        self.assertIn("AssetAddress source, std::int32_t bytes)", body)
        self.assertIn("CopyExact(host, destination, source, size,", body)
        self.assertIn('host.Refuse("model geometry block", bytes);', body)
        self.assertNotIn("+=", body)
        self.assertNotRegex(body, r"\barg\d+\b")

    def test_gimmick_copy_uses_only_the_native_owned_runtime_prefix(self):
        source = """memset(s1, 0, 0x178);
VS::Core::Memory::CopyAlignedWords(s1, Field_gimmickInstances[arg0->dataAddress], 0x178);
"""
        for old, new, _ in copy_fixups.FIXES["F00782"]:
            if "0x178" not in old:
                continue
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertEqual(
            source.count("offsetof(Field_GimmickInstance, reserved178)"), 2
        )
        self.assertNotIn("sizeof(Field_GimmickInstance)", source)
        self.assertNotIn("0x178", source)

    def test_gimmick_actor_above_alias_callers_use_original_scalar_loads(self):
        cases = {
            "F00836": "Field_FindActorAboveGimmick(Field_gimmickInstances[temp_s4], i)",
            "F00908": "func_800A3C00(object, 1)",
        }
        for identity, source in cases.items():
            with self.subTest(identity=identity):
                matches = [(old, new) for old, new, _ in copy_fixups.FIXES[identity]
                           if old == source]
                self.assertEqual(len(matches), 1)
                old, replacement = matches[0]
                self.assertIn(old, source)
                self.assertIn("VS::Field::LiveFindActorOccupyingTile(", replacement)
                self.assertIn("->positionY", replacement)
                self.assertEqual(replacement.count("(u_char)"), 2)
                self.assertNotIn("Field_FindActorAboveGimmick", replacement)
        root = Path(__file__).resolve().parents[2]
        generated = (root / r"Native\Game\src\Field\Gimmicks.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Field_FindActorAboveGimmick", generated)
        self.assertNotIn("int Field_FindInteractableGimmickAhead(", generated)
        self.assertNotIn("int Field_BeginGimmickInteraction(", generated)
        self.assertNotIn("void Field_ApplyGimmickInteractionCommand(", generated)
        live = (root / r"Native\Game\src\Field\GimmickPlacementLive.cpp").read_text(encoding="utf-8")
        self.assertIn("static_cast<std::uint8_t>(support.tile_word)", live)
        self.assertIn("support.position_y, first_actor", live)
        self.assertIn("support.position_y, 1", " ".join(live.split()))
        self.assertIn("FindLiveInteractableGimmickAhead(slot)", live)

    def test_gimmick_reaction_generated_calls_keep_typed_retained_entry(self):
        root = Path(__file__).resolve().parents[2]
        generated = (root / r"Native\Game\src\Field\Gimmicks.cpp").read_text(encoding="utf-8")
        call = "Field_UpdateGimmickReactionAnimationEntry((Field_GimmickMotionView*)object)"
        self.assertEqual(generated.count(call), 0)
        self.assertNotIn("int Field_UpdateGimmickReactionAnimation(", generated)
        self.assertNotIn("void Field_UpdateGimmickMotion(", generated)
        self.assertNotRegex(generated, r"\(\(int\s*\(\*\).*?\)Field_UpdateGimmickReactionAnimationEntry")
        live = (root / r"Native\Game\src\Field\GimmickDynamicsLive.cpp").read_text(encoding="utf-8")
        self.assertIn("UpdateLiveGimmickReactionAnimation(record_)", live)
        owned = self.owned_items()
        self.assertEqual(owned["F01014"]["path"], r"Native\Game\src\Field\GimmickReactions.cpp")
        self.assertEqual(owned["F01013"]["path"], r"Native\Game\src\Field\GimmickDynamics.cpp")

    def test_score_viewer_banner_uses_native_frame_pointers(self):
        source = """    void** scratch = (void**)Core_ScratchAddr(0);

    line = scratch[0];
AddPrim(*((void**)Core_ScratchAddr(1)) + 0x1C, line++);
    scratch = (void**)Core_ScratchAddr(0);

    AddPrim(scratch[1] + 0x1C, poly++);

    scratch[0] = (void*)poly;
"""
        for old, new, _ in copy_fixups.FIXES["F02810"]:
            self.assertEqual(source.count(old), 1)
            source = source.replace(old, new)
        self.assertEqual(source.count("Render_frameScratch.orderingTable + 0x1C"), 2)
        self.assertIn("Render_frameScratch.primitiveCursor = poly;", source)
        self.assertNotIn("Core_ScratchAddr", source)

    def test_score_scroll_links_keep_the_original_byte_displacement(self):
        source = """before = Render_frameScratch.overlayOrderingTable + 8;
*before = ((u_long)prim << 8) >> 8;
"""
        for old, new, _ in copy_fixups.FIXES["F02799"]:
            self.assertEqual(source.count(old), 1)
            source = source.replace(old, new)
        self.assertIn("*before = LegacyGpu_Handle(prim);", source)
        self.assertIn("Render_frameScratch.overlayOrderingTable + 8;", source)
        self.assertNotIn("((u_long)prim", source)

    def test_score_number_buffers_include_all_digits_and_terminators(self):
        cases = (
            (
                "F02816",
                'char digits[2];\nsprintf(digits, "%02d", titleNo);',
                'char digits[sizeof "00"];',
                '"%02d"',
            ),
            (
                "F02817",
                'char digits[2];\nsprintf(digits, "%03d", enemyNo);',
                'char digits[sizeof "000"];',
                '"%03d"',
            ),
            (
                "F02820",
                'char sp60[12];\nsprintf(sp60, "NO.   %03d/%03d", selected + 1, 78);',
                'char sp60[sizeof "NO.   000/000"];',
                '"NO.   %03d/%03d"',
            ),
        )
        for function, source, declaration, format_string in cases:
            with self.subTest(function=function):
                if function == "F02816":
                    source += """
poly = *(void**)((uintptr_t)Core_ScratchBase() + 0x000);
AddPrim(*(void**)((uintptr_t)Core_ScratchBase() + 0x008), poly++);
*(void**)((uintptr_t)Core_ScratchBase() + 0x000) = poly;
"""
                elif function == "F02817":
                    source += """
poly = *(void**)Core_ScratchAddr(0);
AddPrim(*(void**)Core_ScratchAddr(2), poly++);
*(void**)Core_ScratchAddr(0) = poly;
"""
                for old, new, _ in copy_fixups.FIXES[function]:
                    self.assertEqual(source.count(old), 1)
                    source = source.replace(old, new)
                self.assertIn(declaration, source)
                self.assertIn(format_string, source)
                if function != "F02820":
                    self.assertIn(
                        "AddPrim(Render_frameScratch.overlayOrderingTable, poly++);",
                        source,
                    )
                    self.assertNotIn("Core_Scratch", source)

    def test_npc_area_filter_keeps_geometry_without_creating_player_ui(self):
        # #276 row 31: Battle_FilterAreaActionTargets (F00103) is typed C++ in Battle/Targeting.cpp.
        self.assertNotIn("F00103", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Battle/Targeting.cpp").read_text(encoding="utf-8")
        start = source.index("void FilterAreaActionTargets(ActionTargetingHost& host) {")
        body = flat(source[start:source.index("\n}\n", start)])
        self.assertIn("host.SetVolume(TargetingVolumeSlot::kArea, area); if (host.IsPlayerAction()) { "
                      "host.ShowAreaVolume(); }", body)
        self.assertIn("int kept = 0;", body)
        self.assertIn("host.IsPointInVolume(TargetingVolumeSlot::kArea, source.position)", body)
        self.assertNotIn("InitSphere", body)
        live = (root / "Native/Game/src/Battle/TargetingLive.cpp").read_text(encoding="utf-8")
        start = live.index("void ShowAreaVolume() override {")
        self.assertIn("Battle_SetSecondarySphereVolume(&area);", live[start:live.index("\n  }\n", start)])

    def test_native_target_capacity_is_not_a_ps1_allocation_or_visible_limit(self):
        root = Path(__file__).resolve().parents[2]
        action = (root / "Native/Game/include/Battle/Action.h").read_text()
        self.assertIn("Battle_TargetPreview previews[255];", action)
        self.assertIn("Battle_HitResult targets[255];", action)
        boot = (root / "Native/Game/src/Boot/GameplayOverlayLive.cpp").read_text()
        self.assertIn("AllocateLiveFromHighEnd(sizeof(Battle_ActionState))", boot)
        self.assertNotIn("0x2C10", boot)
        self.assertNotIn("0x2C10", (root / "Native/Game/src/Boot/GameLoop.cpp").read_text())
        # #276 row 31: the targeting is typed C++ (Battle/Targeting.cpp) bound by Battle/TargetingLive.cpp.
        api = (root / "Native/Game/include/Battle/Targeting.hpp").read_text(encoding="utf-8")
        self.assertIn("inline constexpr int kTargetPreviewCapacity = 255;", api)
        self.assertIn("inline constexpr int kSelectionTargetCapacity = 255;", api)
        core = (root / "Native/Game/src/Battle/Targeting.cpp").read_text(encoding="utf-8")
        self.assertIn("if (!PreviewSlotAvailable(count)) {", core)
        live = flat((root / "Native/Game/src/Battle/TargetingLive.cpp").read_text(encoding="utf-8"))
        self.assertIn("static_assert(std::extent_v<decltype(Battle_ActionState::previews)> == "
                      "static_cast<std::size_t>(battle::kTargetPreviewCapacity));", live)
        self.assertIn("state_.selection = {};", live)
        self.assertIn("Battle_MapTargetPageIndex(", live)
        self.assertIn("Battle_HandleTargetPageButtons()", live)
        for targeting in (core, live):
            self.assertNotIn("0x84C", targeting)
            self.assertNotIn("sphere-cursor", targeting)

    def test_ai_planning_owns_the_full_native_action_result(self):
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        self.assertEqual(
            owned["F01718"]["path"],
            r"Native\Game\src\Battle\Ai\PlanningWorkspace.cpp",
        )
        source = (root / "Native/Game/src/Battle/Ai/ActionPlanning.cpp").read_text()
        self.assertNotIn("int Battle_Ai_RebuildActorActionPlans(", source)
        for offset in ("0x1ECC", "0x9CC", "0xD8C", "0x1D8C"):
            self.assertNotIn(offset, source)
        self.assertFalse((root / "Native/Game/src/Actors/ActorLoading.cpp").exists())
        spawning = (root / "Native/Game/src/Actors/ActorSpawningLive.cpp").read_text(encoding="utf-8")
        self.assertNotIn("spawn137", spawning)

    def test_ai_plan_rebuild_reads_the_queued_action_request(self):
        # #255: actorEvaluationContext views func_800DEEA4_t, so no actor pointer slots may shift it.
        self.assertNotIn("T00380", native_views.ACTOR_VIEWS)
        self.assertEqual(native_views.pointer_slots("T00380"), ())
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        self.assertEqual(owned["F01718"]["name"], "VS::Battle::Ai::RebuildActorActionPlans")
        execution = (root / "Native/Game/src/Battle/ActionSequenceLive.cpp").read_text(encoding="utf-8")
        self.assertIn("Ai::RebuildLiveActorActionPlans(&action_)", execution)
        self.assertIn('#include "Battle/Ai/PlanningWorkspaceLive.hpp"', execution)
        for path in ("Actors/ActorStateLive.cpp", "Boot/GameplayLoopLive.cpp", "Event/CutsceneLifecycleLive.cpp"):
            text = (root / "Native/Game/src" / path).read_text(encoding="utf-8")
            self.assertNotIn("Battle_Ai_RebuildActorActionPlans(", text, path)
            # ActorStateLive.cpp sits inside namespace VS, so it names Battle::Ai without the VS:: prefix.
            self.assertIn("Battle::Ai::RebuildLiveActorActionPlans(", text, path)
        # The owned creation binding (F01818) sits inside VS::Battle::Ai.
        creation = (root / "Native/Game/src/Battle/Ai/ActorCreationLive.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Battle_Ai_RebuildActorActionPlans(", creation)
        self.assertIn("RebuildLiveActorActionPlans(nullptr)", creation)
        table = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertNotIn("0x800DEC88u", table)

    def test_actor_commands_route_through_the_live_binding(self):
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        self.assertEqual(owned["F01720"]["name"], "VS::Battle::Ai::IssueActorCommand")
        self.assertFalse((root / "Native/Game/src/Battle/Ai/ActorCommands.h").exists())
        for path in ("ActionExecutionLive.cpp", "Movement.cpp", "Navigation.cpp", "OrbitingBoss.cpp"):
            text = (root / "Native/Game/src/Battle/Ai" / path).read_text(encoding="utf-8")
            self.assertNotIn("Battle_Ai_IssueActorCommand(", text, path)
            self.assertNotIn("Battle/Ai/ActorCommands.h", text, path)
            # Navigation.cpp's command issuer (F01740) moved to AreaRouteLive.cpp,
            # Movement.cpp's (F01748, F01749) to TargetApproachLive.cpp and
            # OrbitingBoss.cpp's (F01849) to BossOrbitLive.cpp.
            if path in ("Navigation.cpp", "Movement.cpp", "OrbitingBoss.cpp"):
                self.assertNotIn("IssueLiveActorCommand(", text, path)
                continue
            self.assertIn("IssueLiveActorCommand(", text, path)
            self.assertIn('#include "Battle/Ai/ActorCommandsLive.hpp"', text, path)
        # The owned creation binding (F01818) sits inside VS::Battle::Ai.
        creation = (root / "Native/Game/src/Battle/Ai/ActorCreationLive.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Battle_Ai_IssueActorCommand(", creation)
        self.assertIn("IssueLiveActorCommand(&state, static_cast<int>(intent));", creation)
        self.assertIn('#include "Battle/Ai/ActorCommandsLive.hpp"', creation)
        for name in ("TargetApproachLive.cpp", "BossOrbitLive.cpp"):
            text = (root / "Native/Game/src/Battle/Ai" / name).read_text(encoding="utf-8")
            self.assertIn("IssueLiveActorCommand(&state_, static_cast<int>(intent));", text, name)
            self.assertIn('#include "Battle/Ai/ActorCommandsLive.hpp"', text, name)
        table = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertNotIn("0x800DEEFCu", table)

    def test_sphere_wireframe_routes_through_the_live_binding(self):
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        self.assertEqual(owned["F01288"]["name"], "VS::Battle::DrawSphereVolumeWireframe")
        self.assertFalse((root / "Native/Game/src/Battle/TargetingDraw.h").exists())
        text = (root / "Native/Game/src/Battle/Sphere.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Battle_DrawSphereVolumeWireframe(", text)
        self.assertIn("VS::Battle::DrawLiveSphereVolumeWireframe(shape, color);", text)
        self.assertIn('#include "Battle/TargetingDrawLive.hpp"', text)
        core = (root / "Native/Game/src/Battle/TargetingDraw.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Legacy/", core)
        self.assertNotIn("Core_Gte", core)
        table = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertNotIn("0x800C0B50u", table)

    def test_action_target_effects_route_through_the_live_binding(self):
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        self.assertEqual(owned["F00062"]["name"], "VS::Battle::ApplyActionTargetEffects")
        self.assertFalse((root / "Native/Game/src/Battle/ActionResults.h").exists())
        text = (root / "Native/Game/src/Effects/Timeline.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Battle_ApplyActionTargetEffects(", text)
        self.assertEqual(text.count("VS::Battle::ApplyLiveActionTargetEffects("), 2)
        self.assertIn('#include "Battle/ActionResultsLive.hpp"', text)
        core = (root / "Native/Game/src/Battle/ActionResults.cpp").read_text(encoding="utf-8")
        self.assertNotIn("Legacy/", core)
        self.assertNotIn("Battle_actionState", core)
        table = (root / r"Native\Game\src\Boot\ModulePointers.cpp").read_text(encoding="utf-8")
        self.assertNotIn("0x8006CB04u", table)

    def test_particle_attraction_uses_native_fields_after_pointer_widening(self):
        source = """damping = ((Effects_ParticleAttractionView*)node)->attraction;
direction.vx = ((Effects_ParticleAttractionView*)node)->targetX - (state->x >> 12);
direction.vy = ((Effects_ParticleAttractionView*)node)->targetY - (state->y >> 12);
direction.vz = ((Effects_ParticleAttractionView*)node)->targetZ - (state->z >> 12);
"""
        for old, new, _ in copy_fixups.FIXES["F01554"]:
            self.assertIn(old, source)
            source = source.replace(old, new)
        self.assertIn("damping = node->attraction;", source)
        for index, axis in enumerate("xyz"):
            self.assertIn(
                f"direction.v{axis} = node->attractionTarget[{index}] - (state->{axis} >> 12);",
                source,
            )
        self.assertNotIn("Effects_ParticleAttractionView", source)

    def test_door_queries_keep_owned_native_pointer_storage(self):
        root = Path(__file__).resolve().parents[2]
        with (root / "tools/port/owned.csv").open(newline="") as file:
            owned = {row["id"]: row for row in csv.DictReader(file)}
        for item in ("T00232", "G00330"):
            with self.subTest(item=item):
                self.assertIn(item, owned)
                self.assertEqual(
                    owned[item]["header"], r"Native\Game\src\Field\DoorMatchList.h"
                )
        self.assertEqual(owned["F00579"]["name"], "VS::Field::TransitionDoorsAtTile")
        self.assertEqual(owned["F00579"]["header"], r"Native\Game\include\Field\DoorMatches.hpp")
        source = (root / "Native/Game/src/Field/Doors.cpp").read_text()
        caller = (root / "Native/Game/src/Field/ChestInteractionsLive.cpp").read_text()
        live_header = (root / "Native/Game/include/Field/DoorMatchesLive.hpp").read_text()
        self.assertIn('"Field/DoorMatchList.h"', live_header)
        self.assertIn('#include "Field/DoorMatchesLive.hpp"', caller)
        self.assertNotIn("Field_TransitionDoorsAtTile(", caller)
        self.assertEqual(caller.count("VS::Field::TransitionLiveDoorsAtTile("), 1)
        self.assertIn("std::span(matches->records, count)", caller)
        self.assertRegex(
            caller,
            r"count\s*>\s*Field_roomData\.header\.doorsLength\s*/\s*sizeof\(Field_MpdDoorRecord\)",
        )
        core = (root / "Native/Game/src/Field/DoorMatches.cpp").read_text()
        self.assertNotIn("Legacy/", core)
        self.assertNotIn("Field_roomData", core)
        self.assertNotIn("Field_DoorMatchList Field_matchingDoorRecords;", source)
        self.assertNotIn("Field_TransitionDoorsAtTile(int", source)
        owned_header = (root / "Native/Game/src/Field/DoorMatchList.h").read_text()
        self.assertIn("Field_MpdDoorRecord** records;", owned_header)
        self.assertNotIn("records[0]", owned_header)

    def test_weapon_naming_draw_links_use_native_handles(self):
        # #276 slice 51: the naming screen is the typed Menu::Rename core; its frame and selection box hand
        # complete packets to the host instead of linking 24-bit truncated pointers.
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Rename.cpp").read_text(encoding="utf-8")
        self.assertTrue(source.startswith("// Hand-maintained (tools/port/owned.csv)."))
        self.assertNotIn("<< 8) >> 8", source)
        self.assertNotIn("Legacy/", source)
        self.assertNotIn("LegacyGpu_Handle", source)
        for function, identifier in (("DrawFrame", "F02781"), ("DrawSelectionBox", "F02782")):
            with self.subTest(function=function):
                self.assertNotIn(identifier, copy_fixups.FIXES)
                self.assertIn(f"void {function}(", source)

    def test_workshop_choices_are_owned_typed_part_lists(self):
        # #276 slice 49: the six MENUC part lists are hand-maintained; they build their rows in a bounded
        # PartListRows instead of pointer arrays carved out of the PS1-sized text allocation.
        owned = self.owned_items()
        root = Path(__file__).resolve().parents[2]
        root = root / "Native" / "Game" / "src" / "Menu" / "Setup"
        selectors = (
            ("F02886", "Assembly.cpp", "SelectAssemblyBlade"),
            ("F02887", "Assembly.cpp", "SelectAssemblyGrip"),
            ("F02888", "Assembly.cpp", "SelectGem"),
            ("F02903", "Combination.cpp", "SelectCombinationBlade"),
            ("F02909", "Combination.cpp", "SelectCombinationShield"),
            ("F02913", "Combination.cpp", "SelectCombinationArmor"),
        )
        for fid, filename, function in selectors:
            with self.subTest(function=function):
                self.assertNotIn(fid, copy_fixups.FIXES)
                self.assertEqual(owned[fid]["name"], f"VS::Menu::Setup::{function}")
                self.assertEqual(owned[fid]["path"], rf"Native\Game\src\Menu\Setup\{filename}")
                source = (root / filename).read_text(encoding="utf-8")
                self.assertIn(f"int {function}(Context& context, int ", source)
                self.assertNotIn("char**", source)
                self.assertNotRegex(source, r"0x(?:6A4|384|1324)\b")
        screen = (root / "SetupScreen.cpp").read_text(encoding="utf-8")
        self.assertIn("void ShowPartList(Context& context, const PartListRows& rows)", screen)

    def test_loot_staging_uses_last_slots_and_preflights_composites(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Loot/LootInventory.cpp").read_text()
        reviewed_arguments = {
            "F02849": "looted_weapon",
            "F02851": "looted_shield",
            "F02852": "looted_item",
        }
        for fid in (
            "F02846",
            "F02847",
            "F02848",
            "F02849",
            "F02850",
            "F02851",
            "F02852",
        ):
            for old, new, _ in copy_fixups.FIXES[fid]:
                if fid in reviewed_arguments:
                    old = re.sub(r"\barg0\b", reviewed_arguments[fid], old)
                    new = re.sub(r"\barg0\b", reviewed_arguments[fid], new)
                self.assertIn(new, source)
                self.assertNotIn(old, source)
        self.assertIn("Items_ItemCategory_Grip, 1", source)
        self.assertIn("Items_ItemCategory_Gem, gems", source)

    def test_loot_overflow_never_appends_a_failed_staging_index(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Loot/LootInventory.cpp").read_text()
        stage = source.split("void Menu_Loot_StageDropRecord(", 1)[1]
        stage = stage.split("// decomp:", 1)[0]
        self.assertNotIn("Menu_Loot_AppendLootRow(", stage)
        self.assertEqual(stage.count("Menu_Loot_AppendStagedDrop("), 8)
        append = source.split("void Menu_Loot_AppendStagedDrop(", 1)[1]
        append = append.split("int Menu_Loot_AdvanceStagedInventory(", 1)[0]
        self.assertLess(
            append.index("if (index == 0)"), append.index("Menu_Loot_AppendLootRow(")
        )
        self.assertIn("Menu_Loot_paging.deferred = 1;", append)

    def test_loot_pages_share_row_capacity_and_reset_before_source_release(self):
        root = Path(__file__).resolve().parents[2]
        screen = (root / "Native/Game/src/Menu/Loot/LootScreen.cpp").read_text()
        self.assertNotIn("160 * sizeof", screen)
        self.assertNotIn("sizeof *Menu_Loot_lootList * 160", screen)
        self.assertEqual(screen.count("kMenuLootRowCapacity"), 6)
        selection = screen.split("int Menu_Loot_UpdateLootSelection(", 1)[1]
        self.assertIn("} else if (Menu_Loot_AdvanceStagedInventory()) {", selection)
        cleanup = (root / "Native/Game/src/Items/Loot.cpp").read_text()
        cleanup = cleanup.split("void ClearRoomLootList(", 1)[1]
        self.assertLess(
            cleanup.index("flow.ResetLootPaging();"),
            cleanup.index("list.Clear();"),
        )

    def test_misc_inventory_write_checks_capacity_before_mutation(self):
        root = Path(__file__).resolve().parents[2]
        source = (
            root / "Native" / "Game" / "src" / "Items" / "Inventory.cpp"
        ).read_text()
        merge = source.split("ItemCopy CopyMisc(", 1)[1]
        probe = merge.index("const auto probe = CopyItem(")
        self.assertLess(merge.index("if (request.write) {"), probe)
        self.assertIn("if (probe.slot == 0) return {0, probe.status};", merge)
        self.assertLess(probe, merge.index("FillStack(*stack, count)"))
        self.assertNotIn("F02511", copy_fixups.FIXES)

    def test_particle_cone_preserves_original_zero_height_quotient(self):
        _, new, _ = copy_fixups.FIXES["F01515"][1]
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Effects/Particles.cpp").read_text()
        self.assertTrue(
            new in source, "cone fix missing from generated particle emitter"
        )
        self.assertIn("scratch->vector.vx = scratch->vector.vz = -1;", new)
        self.assertIn("0x800D34BC/0x800D34F8", new)
        self.assertEqual(new.count("/ scratch->extent.vy;"), 2)
        self.assertNotIn("Core_Random_Rand", new)

    def test_ashley_spawn_initializes_materials_for_empty_equipment_slots(self):
        # 18.c:func_800F9CCC passes both materials even when their model IDs are zero (#276 slice 19c).
        root = Path(__file__).resolve().parents[2]
        self.assertFalse((root / "Native/Game/src/Actors/Ashley.cpp").exists())
        header = (root / "Native/Game/include/Actors/ActorSpawning.hpp").read_text(encoding="utf-8")
        equipment = header.split("struct AshleyEquipment {", 1)[1].split("\n};", 1)[0]
        self.assertIn("std::uint16_t blade_material = 0;", equipment)
        self.assertIn("std::uint16_t shield_material = 0;", equipment)

    def test_misc_shortcut_label_is_encoded_before_reset_capture(self):
        old, new, _ = copy_fixups.FIXES["F02553"][0]
        declaration = "static char Menu_Inventory_miscItemShortcutMisc[] = " + old + ";"
        encoded = declaration.replace(old, new)
        self.assertIn(r'"\x16\x12\x1C\x0C\xE7\x00\x00\xF8"', encoded)
        self.assertNotIn("MISC", encoded)
        self.assertNotIn("|0xF8|", encoded)

    def test_generated_label_and_reload_initializer_both_terminate(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Inventory/Inventory.cpp").read_text()
        label = r'"\x16\x12\x1C\x0C\xE7\x00\x00\xF8"'
        self.assertIn(
            "static char Menu_Inventory_miscItemShortcutMisc[] = " + label, source
        )
        self.assertIn("static const char initial[] = " + label, source)
        self.assertNotIn(r'"MISC\00000|0xF8|"', source)

    def test_title_settings_retains_original_defaults_in_shared_helper(self):
        # #276 slice 53: _initEnvironment (F03669) is the typed VS::Title::InitializeSettings, whose
        # host restores the original defaults (menuFlags 0x2D8, ...) through the shared helper.
        self.assertNotIn("F03669", copy_fixups.FIXES)
        self.assertIn(
            "Boot_InitializePlayerSettings", copy_fixups.NATIVE_HELPER_HEADERS
        )
        root = Path(__file__).resolve().parents[2]
        core = (root / r"Native\Game\src\Title\TitleMenu.cpp").read_text(encoding="utf-8")
        self.assertIn("host.ResetPlayerSettings();", core)
        live = (root / r"Native\Game\src\Title\TitleMenuLive.cpp").read_text(encoding="utf-8")
        self.assertIn("void ResetPlayerSettings() override { Boot_InitializePlayerSettings(); }", live)
        helper = (root / r"Native\Game\src\Boot\PlayerSettings.cpp").read_text(encoding="utf-8")
        self.assertIn("0x2D8", helper)

    def test_misc_shortcut_reserves_all_native_text_pointers(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Inventory/Inventory.cpp").read_text()
        self.assertIn(
            "VS::Core::Memory::AllocateLiveFromHighEnd(0x1900 + 64 * 2 * sizeof(*menuText))",
            source,
        )
        self.assertIn("menuText = (char **)(((u_char*)(buf)) + 0x1900);", source)
        self.assertNotIn("AllocateLiveFromHighEnd(0x1B00)", source)

    def test_plugin_table_never_binds_the_development_dispatcher(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Effects/PurgeTables.cpp").read_text()
        table = source.split(
            "Effects_EffectRenderer Effects_effectEntryPoints[] = {", 1
        )[1]
        table = table.split("};", 1)[0]
        self.assertNotIn("InvokeAllPluginUpdates", table)
        self.assertIn("VS::Effects::Templates::UpdateLivePlg198Ring", table)

    def test_rank_menu_preserves_native_packet_link(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/Magic.cpp").read_text()
        menu = source.split("int Menu_Magic_UpdateWarlockMenu(", 1)[1]
        menu = menu.split("int Menu_Magic_UpdateShamanMenu(", 1)[0]
        self.assertIn("*spell_level_ordering_table = LegacyGpu_Handle(prim);", menu)
        self.assertNotIn("*spell_level_ordering_table = ((u_int)prim << 8) >> 8;", menu)

    def test_break_art_menu_reserves_navigation_and_matching_reset_storage(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Menu/BreakArts.cpp").read_text()
        self.assertIn("static u_short Menu_BreakArts_artListActionIds[5];", source)
        reset = source.split("void Menu_BreakArts_ResetMenu1Statics(", 1)[1]
        self.assertIn("static const u_short initial[5] = {};", reset)
        self.assertNotIn("static const u_short initial[4] = {};", reset)

    def test_encounter_trace_uses_action_actor_not_targeting_mode(self):
        root = Path(__file__).resolve().parents[2]
        source = (root / "Native/Game/src/Boot/Encounter.cpp").read_text()
        inspect = source.split("void InspectTargeting(", 1)[1].split("\n}", 1)[0]
        self.assertIn(
            "state.source_actor = action.selection.primaryTarget.target.targetActor;",
            inspect,
        )
        self.assertNotIn("state.source_actor = action.sourceActorId;", inspect)

    def test_secondary_body_uses_the_live_zone_character_table(self):
        # 146C.c:6676 reads the body model from the zone enemy record; #276 slice 19c bounds it by zndEnemyCount.
        root = Path(__file__).resolve().parents[2]
        core = (root / "Native/Game/src/Actors/ActorSpawning.cpp").read_text(encoding="utf-8")
        self.assertIn("const std::uint8_t body_model = host.ZoneEnemyModel(zud);", core)
        self.assertIn("request.model_id = static_cast<std::int16_t>(body_model + 1);", core)
        live = (root / "Native/Game/src/Actors/ActorSpawningLive.cpp").read_text(encoding="utf-8")
        model = live.split("std::uint8_t ZoneEnemyModel(int zud) const override {", 1)[1].split("\n  }\n", 1)[0]
        self.assertIn("zud >= ZoneEnemyCount()", model)
        self.assertIn("Field_zoneContext.zndEnemies[zud].recordBytes[kZoneEnemyModelByte]", model)
        for text in (core, live):
            self.assertNotIn("Actors_zoneCharacterInitData", text)

    def test_preview_weapon_alias_uses_real_model_slots(self):
        root = Path(__file__).resolve().parents[2]
        # #276 row 46 promoted func_800F9CB0 (F02465) to the owned ActorPreview.cpp ResetActorPreviewPose, which
        # attaches the preview actor's weapon models (slots 2 and 3) through ActorPreviewLive.cpp.
        self.assertEqual(self.owned_items()["F02465"]["path"], r"Native\Game\src\Menu\Framework\ActorPreview.cpp")
        header = (root / "Native/Game/include/Menu/Framework/ActorPreview.hpp").read_text()
        self.assertIn("inline constexpr int kPreviewWeaponModelSlot = 2;", header)
        preview = (root / "Native/Game/src/Menu/Framework/ActorPreview.cpp").read_text()
        reset = preview.split("void ResetActorPreviewPose(ActorPreviewHost& host) {", 1)[1]
        reset = reset.split("\n}\n", 1)[0]
        self.assertIn("const int slot = kPreviewWeaponModelSlot + weapon;", reset)
        self.assertIn("host.SetWeaponAttached(slot);", reset)
        live = (root / "Native/Game/src/Menu/Framework/ActorPreviewLive.cpp").read_text()
        self.assertIn("return Actors_weaponModels[slot];", live)
        self.assertIn("model->reserved08Bit4 = 1;", live)
        self.assertNotIn("Battle_unusedGlobal1", preview + live)
        inventory = (root / "Native/Game/src/Menu/Inventory/Inventory.cpp").read_text()
        self.assertIn("Actors_weaponModels[i + 2]->reserved11 = 0;", inventory)
        self.assertNotIn("Battle_unusedGlobal1", inventory)
        self.assertIn(
            "Actors_weaponModels[3] != NULL && Menu_Inventory_IsTwoHandedWeapon(category)",
            inventory,
        )
        self.assertNotIn("Battle_unusedGlobal2", inventory)

    def test_equipment_slot_storage_is_owned_and_native_sized(self):
        root = Path(__file__).resolve().parents[2]
        allocation = (root / "Native/Game/src/Actors/ActorAllocation.h").read_text()
        slot = (root / "Native/Game/src/Actors/ActorModelSlot.h").read_text()
        self.assertIn('#include "Actors/ActorModelSlot.h"', allocation)
        self.assertNotIn("storage[0x5E8]", allocation)
        self.assertIn("Actors_WeaponModel weapon;", slot)
        registry = (root / "tools/port/owned.csv").read_text()
        self.assertIn("T00633,type,Actors_ActorModelSlot,", registry)

    def test_native_audio_preserves_optional_layer_guard_and_connects_sfx(self):
        self.assertNotIn("F03192", copy_fixups.FIXES)
        root = Path(__file__).resolve().parents[2]
        mixer = (root / "Native/Engine/src/Audio/Mixer/VoiceMixer.cpp").read_text()
        self.assertNotIn("Audio_Driver_WriteDirtyVoiceParameters(", mixer)
        self.assertNotIn("Audio_Driver_WriteVoiceParameters(", mixer)
        live = (root / "Native/Engine/src/Audio/Mixer/VoiceMixerLive.cpp").read_text()
        self.assertEqual(live.count("Audio_Native_ConfigureChannel("), 1)
        self.assertIn("Audio_Native_Envelope(voice)", live)
        self.assertIn("if (secondary == nullptr) return LiveLayers{active, nullptr, nullptr};", live)
        sequencer = (root / "Native/Engine/src/Audio/Seq/SeqSequencerLive.cpp").read_text()
        self.assertEqual(sequencer.count("Audio_Native_Tick();"), 1)


if __name__ == "__main__":
    unittest.main()
