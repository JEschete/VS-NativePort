import copy
from contextlib import nullcontext
import hashlib
import io
import json
import sys
import tempfile
import threading
import unittest
import zipfile
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from vs_player.config import PlayerPaths
from vs_player.abilities import defense_value, offensive_value
from vs_player.controller import CombatController, StaleDecision, direction_buttons
from vs_player.choices import available_choices
from vs_player.doors import PhysicalDoor
from vs_player.menus import MenuTransaction
from vs_player.model import Decision, LocalModel, ModelResponseError
from vs_player.observations import Frame
from vs_player.reaction import ReactionController
from vs_player.runtime import download, validate_archive
from vs_player.runner import CombatGoal, PlayOptions, play
from vs_player.store import RUN_MARKER, Retention, RunStore
from vs_player.trace import TraceReader


def sample_record(tick=1, **timing):
    actor = {
        "slot": 0, "model": 0, "hp": 100, "max_hp": 100, "mp": 20,
        "risk": 3, "position": [64, 0, 64], "weapon_drawn": 1,
        "max_mp": 40, "base_attack_type": 0, "shield_pp": 0, "weapon_pp": 0,
        "weapon_category": 2, "spawn_index": 128, "hostile_to_player": False,
        "relation_to_player": -1,
        "statuses": 0, "status_immunities": 0, "spell_action_ids": [],
        "weapon_range": {
            "components_world": [64, 128, 128], "shape": 1,
            "angle_steps": 0, "native_extent_world": 128,
        },
    }
    enemy = copy.deepcopy(actor)
    enemy.update(slot=2, model=58, hp=20, max_hp=20, position=[128, 0, 64],
                 hostile_to_player=True, relation_to_player=3, spawn_index=0)
    reaction = {
        "active": True, "eligibility_status": "evaluated",
        "source_actor": 0, "target_actor": 2, "action": 0,
        "queue_index": 0, "chain_step": 0, "timer": 40, "window": 40,
        "tick_speed": 2, "story_timing": True, "previous_slot": 0,
        "previous_buttons": 0, "attempted": False, "succeeded": 0,
        "chain_actions": [22, 23, 24], "defense_actions": [40, 41, 42],
        "eligible": [True, True, True],
        "incoming_type": 1, "incoming_affinity": 0, "incoming_cost": 25,
        "incoming_hp_damage": 110, "incoming_mp_damage": 0, "incoming_statuses": 0,
    }
    reaction.update(timing)
    return {
        "sequence": tick, "tick": tick, "elapsed_us": tick * 16667,
        "category": "frame",
        "data": {
            "player_schema": 1, "ready": True, "zone": 9, "room": 6,
            "control_mode": 2, "action_phase": 7, "source_actor": 0,
            "game_over": False, "input": {"held": 0, "analog": False}, "actors": [actor, enemy],
            "transition": 0, "field_rendering_enabled": True,
            "reaction_timing": reaction, "target_previews": [],
            "target_page": {"first": 0, "visible": 0, "selected": -1, "selection_step": 0},
            "learned_actions": [22, 23, 24, 40, 41, 42],
            "action_options": [],
            "status_masks": [1 << index for index in range(32)],
            "menu": {"available": False, "loading": False, "selected_menu": -1,
                     "submenu_phase": 0, "selected_row": -1, "list_phase": 0,
                     "assignment_phase": 0, "assignment_type": 0, "assignment_selected": -1,
                     "rows": []},
            "scene": {"available": True, "camera": {"yaw": 2048},
                      "event": {"phase": 0, "forced_skip": False, "dialogue_waits": [False] * 8}},
        },
    }


class ObservationTests(unittest.TestCase):
    def test_native_enemy_identity_and_weapon_geometry_are_not_guessed(self):
        frame = Frame.decode(sample_record())
        self.assertEqual(frame.player.position, (64, 0, 64))
        self.assertEqual([actor.slot for actor in frame.enemies], [2])
        self.assertEqual(frame.enemies[0].range_components, (64, 128, 128))
        self.assertEqual(frame.enemies[0].native_extent, 128)

    def test_old_schema_and_duplicate_slots_are_rejected(self):
        record = sample_record()
        del record["data"]["player_schema"]
        with self.assertRaisesRegex(ValueError, "authoritative"):
            Frame.decode(record)
        record = sample_record()
        record["data"]["actors"][1]["slot"] = 0
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            Frame.decode(record)

    def test_boolean_is_not_a_valid_actor_number(self):
        record = sample_record()
        record["data"]["actors"][0]["slot"] = False
        with self.assertRaisesRegex(ValueError, "actor.slot"):
            Frame.decode(record)

    def test_unknown_nonplayer_is_not_automatically_an_enemy(self):
        record = sample_record()
        record["data"]["actors"][1].update(hostile_to_player=False, relation_to_player=-1)
        self.assertEqual(Frame.decode(record).enemies, ())
        record["data"]["reaction_timing"].update(source_actor=2, target_actor=0)
        self.assertEqual([actor.slot for actor in Frame.decode(record).enemies], [2])


class ReactionTests(unittest.TestCase):
    def test_base_hit_never_uses_ward_siphon_or_magic_reflection(self):
        frame = Frame.decode(sample_record(timer=42, source_actor=2, target_actor=0,
                                          incoming_type=0))
        self.assertEqual([defense_value(frame, action) for action in (40, 41, 42, 44)], [0]*4)
        reactor = ReactionController()
        self.assertIsNone(reactor.observe(frame))
        self.assertEqual(reactor.coverage_gaps[0]["status"], "no_effective_equipped_defense")
        record = sample_record(timer=42, source_actor=2, target_actor=0, incoming_type=0,
                               defense_actions=[40, 43, 45])
        selected = ReactionController().observe(Frame.decode(record))
        self.assertEqual(selected.action_id, 43)

    def test_defense_matches_effect_type_affinity_and_missing_resources(self):
        frame = Frame.decode(sample_record(source_actor=2, target_actor=0))
        self.assertGreater(defense_value(frame, 42), 0)
        self.assertEqual(defense_value(frame, 43), 0)
        self.assertGreater(defense_value(frame, 46), 0)
        self.assertEqual(defense_value(frame, 47), 0)
        self.assertEqual(defense_value(frame, 53), 0)
        record = sample_record(source_actor=2, target_actor=0, incoming_hp_damage=0)
        self.assertEqual(defense_value(Frame.decode(record), 42), 0)
        record["data"]["actors"][0]["mp"] = 40
        self.assertEqual(defense_value(Frame.decode(record), 41), 0)

    def test_status_abilities_use_live_parameter_masks_not_stale_enum_bits(self):
        record = sample_record()
        record["data"]["actors"][0]["statuses"] = 1 << 13
        record["data"]["status_masks"][13] = 1 << 13
        self.assertGreater(defense_value(Frame.decode(record), 40), 0)
        record["data"]["actors"][1]["status_immunities"] = record["data"]["status_masks"][12]
        self.assertEqual(offensive_value(Frame.decode(record), 35), 0)

    def test_inactive_frame_does_not_hide_an_eligible_missed_window(self):
        reactor = ReactionController()
        reactor.observe(Frame.decode(sample_record(timer=38)))
        reactor.observe(Frame.decode(sample_record(2, active=False)))
        reactor.observe(Frame.decode(sample_record(3, active=False)))
        self.assertEqual([item["status"] for item in reactor.results], ["missed"])

    def test_exact_native_boundaries_include_only_the_success_interval(self):
        for timer, expected in ((31, False), (32, False), (39, False), (40, False),
                                (41, True), (48, True), (49, False)):
            with self.subTest(timer=timer):
                timing = Frame.decode(sample_record(timer=timer)).reaction
                self.assertEqual(timing.successful_now, expected)

    def test_spell_margin_and_long_chain_minimum_follow_native_rules(self):
        self.assertEqual(Frame.decode(sample_record(source_actor=2, action=54)).reaction.margin, 12)
        self.assertEqual(Frame.decode(sample_record(story_timing=False)).reaction.margin, 10)
        self.assertEqual(Frame.decode(sample_record(queue_index=30)).reaction.margin, 2)
        self.assertFalse(Frame.decode(sample_record(queue_index=30, timer=43)).reaction.successful_now)

    def test_equipped_eligible_new_button_is_selected_once(self):
        reactor = ReactionController()
        record = sample_record(timer=42, previous_slot=1, previous_buttons=32)
        record["data"]["input"]["held"] = 16
        press = reactor.observe(Frame.decode(record))
        self.assertEqual((press.slot, press.button, press.kind), (3, "Square", "chain"))
        self.assertIsNone(reactor.observe(Frame.decode(sample_record(2, timer=44))))
        reactor.observe(Frame.decode(sample_record(
            3, active=False, previous_slot=3, attempted=True, succeeded=1,
        )))
        self.assertEqual(reactor.results[0]["status"], "accepted")
        self.assertIsNone(reactor.pending)

    def test_defense_uses_its_equipped_slots_and_native_acknowledgment(self):
        reactor = ReactionController()
        reactor.priorities((1, 2, 3), (3, 2, 1))
        press = reactor.observe(Frame.decode(sample_record(
            timer=42, source_actor=2, target_actor=0, action=54,
            defense_actions=[40, 41, 0],
        )))
        self.assertEqual((press.slot, press.kind), (2, "defense"))
        reactor.observe(Frame.decode(sample_record(
            2, active=False, previous_slot=2, attempted=True, succeeded=2,
        )))
        self.assertEqual(reactor.results[0]["kind"], "defense")
        self.assertEqual(reactor.results[0]["status"], "accepted")

    def test_unmapped_unavailable_and_zero_window_do_not_send_buttons(self):
        for overrides in (
            {"chain_actions": [0, 0, 0]},
            {"eligible": [False, False, False]},
            {"window": 0},
            {"eligibility_status": "actor_state_unavailable"},
        ):
            with self.subTest(overrides=overrides):
                self.assertIsNone(ReactionController().observe(
                    Frame.decode(sample_record(timer=42, **overrides))
                ))

    def test_missed_rejected_and_unconfirmed_are_not_successes(self):
        missed = ReactionController()
        missed.observe(Frame.decode(sample_record(timer=40)))
        missed.observe(Frame.decode(sample_record(2, timer=50)))
        self.assertEqual(missed.results[0]["status"], "missed")
        rejected = ReactionController()
        rejected.observe(Frame.decode(sample_record(timer=42)))
        rejected.observe(Frame.decode(sample_record(2, timer=44, attempted=True)))
        self.assertEqual(rejected.results[0]["status"], "rejected")
        unconfirmed = ReactionController()
        unconfirmed.observe(Frame.decode(sample_record(timer=42)))
        unconfirmed.finish()
        self.assertEqual(unconfirmed.results[0]["status"], "unconfirmed")

    def test_effect_needs_acknowledgment_matching_queue_and_new_damage(self):
        reactor = ReactionController()
        record = sample_record(timer=42, source_actor=2, target_actor=0,
                               incoming_type=0, defense_actions=[43, 0, 0])
        reactor.observe(Frame.decode(record))
        record["tick"] = 2
        record["data"]["reaction_timing"].update(active=False, previous_slot=1,
                                                  succeeded=2, attempted=True)
        reactor.observe(Frame.decode(record))
        self.assertEqual(len(reactor.results), 1)
        self.assertEqual(reactor.effect_results, [])
        record["tick"] = 3
        record["data"]["reaction_timing"].update(action=43, queue_index=4)
        record["data"]["actors"][1]["hp"] = 14
        reactor.observe(Frame.decode(record))
        record["tick"] = 4
        record["data"]["reaction_timing"]["queue_index"] = 1
        reactor.observe(Frame.decode(record))
        self.assertEqual(reactor.effect_results, [])
        record["tick"] = 5
        record["data"]["actors"][1]["hp"] = 8
        reactor.observe(Frame.decode(record))
        self.assertEqual(reactor.effect_results[0]["observed_amount"], 6)
        self.assertEqual(reactor.effect_results[0]["status"], "effect_verified")

    def test_reused_target_cannot_supply_a_reaction_effect(self):
        reactor = ReactionController()
        record = sample_record(timer=42)
        reactor.observe(Frame.decode(record))
        record["tick"] = 2
        record["data"]["reaction_timing"].update(active=False, previous_slot=1, succeeded=1)
        reactor.observe(Frame.decode(record))
        record["tick"] = 3
        record["data"]["reaction_timing"].update(action=22, queue_index=1)
        record["data"]["actors"][1].update(hp=1, spawn_index=1)
        reactor.observe(Frame.decode(record))
        self.assertEqual(reactor.effect_results[0]["status"], "effect_target_changed")

    def test_later_action_cannot_supply_an_earlier_effect(self):
        reactor = ReactionController()
        record = sample_record(timer=42)
        reactor.observe(Frame.decode(record))
        record["tick"] = 2
        record["data"]["reaction_timing"].update(active=False, previous_slot=1, succeeded=1)
        reactor.observe(Frame.decode(record))
        record["tick"] = 3
        record["data"]["reaction_timing"].update(action=22, queue_index=1)
        reactor.observe(Frame.decode(record))
        record["tick"] = 4
        record["data"]["reaction_timing"].update(action=23, queue_index=2)
        reactor.observe(Frame.decode(record))
        record["tick"] = 5
        record["data"]["reaction_timing"].update(action=22, queue_index=1)
        record["data"]["actors"][1]["hp"] = 0
        reactor.observe(Frame.decode(record))
        self.assertEqual([row["status"] for row in reactor.effect_results], ["no_observed_effect"])

    def test_phantom_pain_requires_observed_weapon_pp(self):
        record = sample_record()
        self.assertEqual(offensive_value(Frame.decode(record), 31), 0)
        record["data"]["actors"][0]["weapon_pp"] = 30
        self.assertGreater(offensive_value(Frame.decode(record), 31), 0)


class ModelTests(unittest.TestCase):
    def test_truncated_and_invalid_model_outputs_keep_their_evidence(self):
        client = LocalModel()
        for response in (
            {"done": True, "done_reason": "length", "message": {"content": '{"reason":"cut'}},
            {"done": True, "done_reason": "stop", "message": {"content": "{}"}},
        ):
            with self.subTest(response=response), mock.patch.object(client, "request", return_value=response):
                with self.assertRaises(ModelResponseError) as caught:
                    client.decide(Frame.decode(sample_record()), "fight", [])
                self.assertEqual(caught.exception.response, response)

    def test_decision_is_bounded_and_cannot_request_state_writes(self):
        decision = {
            "intent": "attack", "target_slot": 2, "target_limb": -1,
            "chain_priority": "123", "defense_priority": "321",
            "reason": "Observed hostile target is in the native range.",
            "action_id": -1, "equip_slot": 0,
        }
        self.assertEqual(Decision.decode(decision, 3, 2.0).target_slot, 2)
        with self.assertRaises(ValueError):
            Decision.decode(decision | {"set_hp": 999}, 3, 2.0)
        with self.assertRaises(ValueError):
            Decision.decode(decision | {"target_slot": True}, 3, 2.0)
        with self.assertRaises(ValueError):
            Decision.decode(decision | {"chain_priority": [1, 1, 2]}, 3, 2.0)

    def test_remote_and_credentialed_endpoints_are_rejected_before_use(self):
        for endpoint in ("https://example.com", "http://192.0.2.1:11551",
                         "http://localhost.example.com", "http://user:pass@127.0.0.1:11551"):
            with self.subTest(endpoint=endpoint), self.assertRaises(ValueError):
                LocalModel(endpoint)
        self.assertEqual(LocalModel().endpoint, "http://127.0.0.1:11551")


class TraceTests(unittest.TestCase):
    def test_nonframe_chunk_is_not_confused_with_end_of_file(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text(json.dumps({
                "sequence": 1, "category": "engine", "data": {"level": 1},
            }) + "\n", encoding="utf-8")
            reader = TraceReader(path)
            try:
                self.assertEqual(reader.read(), [])
                self.assertFalse(reader.at_eof)
                self.assertEqual(reader.read(), [])
                self.assertTrue(reader.at_eof)
                with path.open("a") as output:
                    output.write(json.dumps(sample_record(2)) + "\n")
                self.assertEqual(reader.read()[0].tick, 2)
                self.assertFalse(reader.at_eof)
            finally:
                reader.close()

    def test_partial_lines_and_script_boundaries_preserve_the_master_session(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            path = Path(directory) / "events.jsonl"
            encoded = json.dumps(sample_record()).encode() + b"\n"
            path.write_bytes(encoded[:35])
            reader = TraceReader(path)
            try:
                self.assertEqual(reader.read(), [])
                with path.open("ab") as output:
                    output.write(encoded[35:])
                self.assertEqual(reader.read()[0].tick, 1)
                end = {"sequence": 2, "category": "session.end", "script": 1,
                       "data": {"trace_failed": False}}
                with path.open("ab") as output:
                    output.write(json.dumps(end).encode() + b"\n")
                reader.read()
                self.assertFalse(reader.ended)
                end.pop("script")
                end["sequence"] = 3
                with path.open("ab") as output:
                    output.write(json.dumps(end).encode() + b"\n")
                reader.read()
                self.assertTrue(reader.ended)
            finally:
                reader.close()

    def test_integer_engine_error_levels_are_counted(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text(json.dumps({
                "sequence": 1, "category": "engine",
                "data": {"level": 4, "message": "fixture"},
            }) + "\n", encoding="utf-8")
            reader = TraceReader(path)
            try:
                reader.read()
                self.assertEqual(len(reader.engine_errors), 1)
            finally:
                reader.close()


class StorageTests(unittest.TestCase):
    def test_pinned_success_survives_retention(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            store = RunStore(PlayerPaths(Path(directory) / "state"))
            try:
                run = store.create("acceptance", {})
                store.finish(run, "passed", {})
                store.pin(run)
                store.cleanup(Retention(successful_days=1), now=10**12)
                self.assertTrue(store.directory(run).exists())
                manifest = json.loads((store.directory(run) / RUN_MARKER).read_text())
                self.assertTrue(manifest["pinned"])
            finally:
                store.close()
    def test_only_verified_outcomes_survive_raw_log_cleanup_as_experience(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            store = RunStore(PlayerPaths(Path(directory) / "state"))
            try:
                run = store.create("fixture", {})
                with self.assertRaisesRegex(ValueError, "Only verified"):
                    store.remember(run, 58, 2, {"success": True})
                store.finish(run, "passed", {"verified": True})
                store.remember(run, 58, 2, {"run_id": run, "chains": 3})
                store.cleanup(Retention(successful_days=1), now=10**12)
                self.assertEqual(store.experience(58, 2), [{"run_id": run, "chains": 3}])
                self.assertFalse(store.directory(run).exists())
            finally:
                store.close()

    def test_cleanup_keeps_active_and_failed_evidence_and_unrelated_files(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            paths = PlayerPaths(Path(directory) / "state")
            store = RunStore(paths)
            try:
                with mock.patch("vs_player.store.time.time", return_value=100.0):
                    passed = store.create("passed", {})
                    store.finish(passed, "passed", {"verified": True})
                    failed = store.create("failed", {})
                    store.finish(failed, "failed", {"missed": 1})
                    active = store.create("active", {})
                unrelated = paths.root / "keep.txt"
                unrelated.write_text("owner content")
                report = store.cleanup(Retention(successful_days=1), now=100.0 + 86401)
                self.assertEqual(report["deleted"], [passed])
                self.assertFalse(store.directory(passed).exists())
                self.assertTrue(store.directory(failed).exists())
                self.assertTrue(store.directory(active).exists())
                self.assertEqual(unrelated.read_text(), "owner content")
            finally:
                store.close()

    def test_cleanup_rejects_an_ownership_mismatch(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            store = RunStore(PlayerPaths(Path(directory) / "state"))
            try:
                run = store.create("fixture", {})
                store.finish(run, "passed", {})
                path = store.directory(run) / RUN_MARKER
                value = json.loads(path.read_text())
                value["owner"] = "another tool"
                path.write_text(json.dumps(value))
                with self.assertRaisesRegex(ValueError, "Cleanup refused"):
                    store.cleanup(Retention(successful_days=1), now=10**12)
                self.assertTrue(path.exists())
            finally:
                store.close()

    def test_existing_unowned_state_is_never_claimed(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            path = Path(directory) / "state"
            path.mkdir()
            with self.assertRaisesRegex(ValueError, "unowned"):
                PlayerPaths(path).initialize()


class ControllerTests(unittest.TestCase):
    def test_offensive_mapping_adapts_to_casters_and_empty_mp(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": action, "family": "chain", "restrictions": 0}
            for action in (22, 23, 24, 28, 35)
        ]
        record["data"]["actors"][1]["spell_action_ids"] = [54]
        choices = available_choices(Frame.decode(record), {})
        self.assertEqual({row["action_id"] for row in choices.values()}, {35})
        record["data"]["actors"][1].update(spell_action_ids=[], mp=0)
        choices = available_choices(Frame.decode(record), {})
        self.assertEqual({(row["action_id"], row["equip_slot"]) for row in choices.values()},
                         {(28, 3)})
    def test_base_attack_preparation_choices_cannot_select_a_useless_defense(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": action, "name": str(action), "family": "defense", "cost": 2,
             "restrictions": 0, "menu_order": action-40}
            for action in (40, 41, 42, 43, 45, 46)
        ]
        choices = available_choices(Frame.decode(record), {})
        self.assertTrue(choices)
        self.assertEqual({choice["intent"] for choice in choices.values()}, {"equip_defense"})
        self.assertEqual({choice["action_id"] for choice in choices.values()}, {43, 45})

    def test_native_spell_cooldown_during_inference_waits_without_state_edits(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["action_options"] = [
            {"id": 96, "family": "shaman", "restrictions": 1, "menu_order": 0}
        ]
        frame = Frame.decode(record)
        controller = CombatController()
        decision = Decision("cast", 0, -1, (1, 2, 3), (1, 2, 3), "heal", 1, 1, 96, 1)
        controller.accept(decision, frame, frame)
        self.assertIsNone(controller.step(frame))
        record["tick"] = 10
        record["data"]["action_options"][0]["restrictions"] = 0
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Triangle"}))

    def test_assignment_closes_with_cancel_not_a_different_mapping_button(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": 43, "family": "defense", "restrictions": 0, "menu_order": 3}
        ]
        frame = Frame.decode(record)
        decision = Decision("equip_defense", 0, -1, (1,2,3), (1,2,3), "reflect", 1, 1, 43, 1)
        transaction = MenuTransaction(decision, frame)
        record["data"]["control_mode"] = 5
        record["data"]["reaction_timing"]["defense_actions"][0] = 43
        record["data"]["menu"].update(available=True, selected_menu=3,
                                      submenu_phase=7, assignment_phase=17)
        self.assertEqual(transaction.step(Frame.decode(record)).button, "Cross")

    def frame(self, tick=1, **fields):
        record = sample_record(tick, active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"].update(fields)
        return Frame.decode(record)

    def decision(self, tick=1, intent="attack"):
        return Decision(intent, 2, -1, (1, 2, 3), (3, 2, 1), "fixture", tick, 1)

    def test_targeting_waits_for_native_readiness_and_confirms_observed_target(self):
        controller = CombatController()
        frame = self.frame()
        controller.accept(self.decision(), frame, frame)
        self.assertEqual(controller.step(frame).buttons, frozenset({"Circle"}))
        page = {"first": 0, "visible": 1, "selected": 0, "selection_step": 18}
        targeting = {"control_mode": 2, "action_phase": 1, "target_page": page,
                     "target_previews": [{"actor": 2, "limb": 0}]}
        self.assertIsNone(controller.step(self.frame(10, **targeting)))
        page["selection_step"] = 19
        self.assertEqual(controller.step(self.frame(11, **targeting)).buttons,
                         frozenset({"Circle"}))

    def test_reused_actor_slot_and_scene_change_invalidate_plans(self):
        controller = CombatController()
        original = self.frame()
        changed = sample_record(10, active=False)
        changed["data"]["actors"][1]["spawn_index"] += 1
        with self.assertRaises(StaleDecision):
            controller.accept(self.decision(), Frame.decode(changed), original)
        with self.assertRaises(StaleDecision):
            controller.accept(self.decision(), self.frame(10, room=7), original)

    def test_movement_uses_camera_and_held_input_prevents_strategy_overwrite(self):
        self.assertEqual(direction_buttons(0, -128, 2048), frozenset({"Down"}))
        self.assertEqual(direction_buttons(-128, 0, 0), frozenset({"Right"}))
        controller = CombatController()
        frame = self.frame()
        controller.accept(self.decision(), frame, frame)
        self.assertIsNone(controller.step(frame, allow_strategy=False))

    def test_active_reaction_preempts_slow_strategy(self):
        controller = CombatController()
        result = controller.step(Frame.decode(sample_record(timer=42)), allow_strategy=False)
        self.assertTrue(result.reaction)
        self.assertEqual(result.buttons, frozenset({"Circle"}))


class RunnerTests(unittest.TestCase):
    def test_physical_door_needs_a_live_loading_cycle_and_unbroken_quiet_gate(self):
        route = PhysicalDoor("z009-r006-d000")
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 2, "tile_z": 0, "layer": 0, "required_value": 0,
             "current_flag_value": 0}
        ]
        route.observe(Frame.decode(record))
        self.assertIsNone(route.accepted_tick)
        route.open_ticks.append(2)
        record["tick"] = 3
        record["data"].update(ready=False)
        route.observe(Frame.decode(record))
        record["tick"] = 4
        record["data"].update(ready=True, room=7)
        route.observe(Frame.decode(record))
        record["tick"] = 123
        route.observe(Frame.decode(record))
        self.assertIsNone(route.accepted_tick)
        record["tick"] = 124
        route.observe(Frame.decode(record))
        self.assertEqual(route.accepted_tick, 124)
        record["tick"] = 200
        route.observe(Frame.decode(record))
        self.assertEqual(route.accepted_tick, 124)

    def test_physical_gate_never_manufactures_story_or_selects_a_shadowed_record(self):
        record = sample_record(active=False)
        door = {"index": 1, "destination_zone": 9, "destination_room": 7,
                "tile_x": 2, "tile_z": 0, "layer": 0, "required_value": 2,
                "current_flag_value": 0}
        record["data"]["doors"] = [door]
        route = PhysicalDoor("z009-r006-d001")
        route.observe(Frame.decode(record))
        self.assertIn("story flag", route.blocked_reason)
        self.assertIsNone(route.step(Frame.decode(record)))
        self.assertEqual(door["current_flag_value"], 0)

    def test_goal_requires_observed_death_and_full_quiet_interval(self):
        goal = CombatGoal()
        alive = sample_record(active=False)
        alive["data"]["control_mode"] = 1
        goal.observe(Frame.decode(alive))
        removed = copy.deepcopy(alive)
        removed["tick"] = 150
        removed["data"]["actors"] = removed["data"]["actors"][:1]
        goal.observe(Frame.decode(removed))
        self.assertIsNone(goal.accepted_tick)
        dead = copy.deepcopy(alive)
        dead["tick"] = 160
        dead["data"]["actors"][1]["hp"] = 0
        goal.observe(Frame.decode(dead))
        removed["tick"] = 279
        goal.observe(Frame.decode(removed))
        self.assertIsNone(goal.accepted_tick)
        removed["tick"] = 280
        goal.observe(Frame.decode(removed))
        self.assertEqual(goal.accepted_tick, 280)
        self.assertEqual(goal.encounters, {(58, 2)})

    def test_failure_and_interrupt_pin_evidence_and_finish_worker_threads(self):
        for error, status in ((ValueError("fixture failure"), "failed"),
                              (KeyboardInterrupt(), "cancelled")):
            with self.subTest(status=status), tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
                paths = PlayerPaths(Path(directory) / "state")
                options = PlayOptions(Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
                with (
                    mock.patch("vs_player.runner.verify_installed_model"),
                    mock.patch("vs_player.runner.model_server", return_value=nullcontext(mock.Mock())),
                    mock.patch("vs_player.runner.warm_model", return_value={}),
                    mock.patch("vs_player.runner._play_game", side_effect=error),
                    self.assertRaises(type(error)),
                ):
                    play(paths, options)
                store = RunStore(paths)
                try:
                    row = store.database.execute("SELECT status,pinned,summary FROM runs").fetchone()
                    self.assertEqual(row[:2], (status, 1))
                    self.assertEqual(json.loads(row[2])["type"], type(error).__name__)
                finally:
                    store.close()
                self.assertFalse(any(thread.name in ("player-audit", "player-retention")
                                     for thread in threading.enumerate()))

    def test_successful_runner_wires_persistent_experience(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            paths = PlayerPaths(Path(directory) / "state")
            options = PlayOptions(Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
            summary = {
                "passed": True, "experience_contexts": [{"model": 58, "weapon": 2}],
                "scenario_sha256": "fixture", "ashley_profile": "all-abilities",
                "accepted_reactions": {"chain": 2, "defense": 1}, "inputs": 10, "goal_tick": 900,
            }
            with (
                mock.patch("vs_player.runner.verify_installed_model"),
                mock.patch("vs_player.runner.model_server", return_value=nullcontext(mock.Mock())),
                mock.patch("vs_player.runner.warm_model", return_value={}),
                mock.patch("vs_player.runner._play_game", return_value=summary),
            ):
                result = play(paths, options)
            store = RunStore(paths)
            try:
                self.assertEqual(store.experience(58, 2)[0]["run_id"], result["run_id"])
                self.assertEqual(store.experience(58, 2)[0]["accepted_reactions"], {"chain": 2, "defense": 1})
            finally:
                store.close()


class RuntimeTests(unittest.TestCase):
    def test_downloads_require_the_pinned_digest(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            path = Path(directory) / "runtime.zip"
            payload = b"fixture-runtime"
            with mock.patch("urllib.request.urlopen", return_value=io.BytesIO(payload)):
                download("https://example.invalid/runtime", path, hashlib.sha256(payload).hexdigest())
            self.assertEqual(path.read_bytes(), payload)
            with self.assertRaisesRegex(ValueError, "unexpected digest"):
                download("https://example.invalid/runtime", path, "0" * 64)

    def test_archive_cannot_escape_its_owned_installation(self):
        for name in ("../escape", "/absolute", "C:/absolute", r"..\escape"):
            with self.subTest(name=name):
                buffer = io.BytesIO()
                with zipfile.ZipFile(buffer, "w") as archive:
                    archive.writestr(name, "fixture")
                buffer.seek(0)
                with zipfile.ZipFile(buffer) as archive, self.assertRaises(ValueError):
                    validate_archive(archive)


if __name__ == "__main__":
    unittest.main()
