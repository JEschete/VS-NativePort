import copy
from contextlib import nullcontext
import hashlib
import io
import json
import sys
import tempfile
import threading
import unittest
import urllib.error
import zipfile
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from vs_player.config import PlayerPaths
from vs_player.abilities import defense_value, defensive_loadout_advice, offensive_value
from vs_player.controller import (
    CombatController, StaleDecision, direction_buttons, input_lease_permitted,
    strategy_planning_ready,
)
from vs_player.choices import available_choices
from vs_player.doors import PhysicalDoor, PhysicalRoute
from vs_player.menus import MenuTransaction
from vs_player.model import Decision, LocalModel, ModelResponseError, NavigationDecision
from vs_player.navigation import SceneTileGoal, Terrain, TerrainNavigator, field_navigation_ready
from vs_player.observations import Frame
from vs_player.reaction import ReactionController
from vs_player.runtime import download, validate_archive
from vs_player.runner import CombatGoal, PlayOptions, native_lab_script, native_outcome, play
from vs_player.store import RUN_MARKER, Retention, RunStore
from vs_player.trace import ObservationFreshness, TraceReader


def sample_record(tick=1, **timing):
    actor = {
        "slot": 0, "model": 0, "hp": 100, "max_hp": 100, "mp": 20,
        "risk": 3, "position": [64, 0, 64], "weapon_drawn": 1,
        "max_mp": 40, "base_attack_type": 0, "shield_pp": 0, "weapon_pp": 0, "actor_flags": 0,
        "run_speed": 18,
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
            "player_schema": 2, "ready": True, "zone": 9, "room": 6,
            "control_mode": 2, "action_phase": 7, "source_actor": 0,
            "game_over": False, "input": {"held": 0, "analog": False}, "actors": [actor, enemy],
            "transition": 0, "field_rendering_enabled": True,
            "reaction_timing": reaction, "target_previews": [],
            "target_page": {"first": 0, "visible": 0, "selected": -1, "selection_step": 0},
            "learned_actions": [22, 23, 24, 40, 41, 42],
            "action_options": [],
            "item_options": [], "items": [],
            "status_masks": [1 << index for index in range(32)],
            "menu": {"available": False, "loading": False, "selected_menu": -1,
                     "submenu_phase": 0, "selected_row": -1, "list_phase": 0,
                     "assignment_phase": 0, "assignment_type": 0, "assignment_selected": -1,
                     "rows": [], "shortcut": 0, "loot_input_ready": False, "loot_count": 0},
            "scene": {"available": True, "camera": {"yaw": 2048},
                      "event": {"phase": 0, "forced_skip": False, "dialogue_waits": [False] * 8}},
        },
    }


class ObservationTests(unittest.TestCase):
    def test_native_script_actor_preserves_unclassified_action_type_without_becoming_hostile(self):
        record = sample_record()
        actor = copy.deepcopy(record["data"]["actors"][1])
        actor.update(slot=6, model=-1, hp=100, base_attack_type=128,
                     hostile_to_player=False, relation_to_player=-1, spawn_index=128)
        record["data"]["actors"].append(actor)
        frame = Frame.decode(record)
        self.assertEqual(frame.actors[-1].base_attack_type, 128)
        self.assertEqual([actor.slot for actor in frame.enemies], [2])
        self.assertEqual(record["data"]["actors"][-1], actor)
        for value in (-1, 256, True):
            with self.subTest(value=value), self.assertRaises(ValueError):
                record["data"]["actors"][-1]["base_attack_type"] = value
                Frame.decode(record)

    def test_unclassified_native_attack_type_is_not_a_physical_attack_forecast(self):
        record = sample_record(active=False)
        record["data"]["actors"][1]["base_attack_type"] = 128
        advice = defensive_loadout_advice(Frame.decode(record))
        self.assertFalse(advice["physical_attack_expected"])
        self.assertFalse(advice["spell_attack_possible"])
        self.assertEqual(advice["unclassified_base_attack_types"], [128])

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


class NavigationTests(unittest.TestCase):
    def test_scene_frontier_exploration_is_opt_in_after_near_goal_visits(self):
        frame = Frame.decode(self.frame())
        navigator = TerrainNavigator()
        self.assertNotIn("frontier_exploration", navigator.request(frame, (160, 96)))
        terrain = Terrain.decode(frame.raw["navigation"])
        navigator.completed_positions = {
            (terrain.point(index)[0], terrain.point(index)[2]): 2
            for index in range(len(terrain.samples)) if terrain.point(index)[0] >= 96
        }
        with self.assertRaisesRegex(ValueError, "no supported outgoing"):
            navigator.request(frame, (320, 96))
        context = navigator.request(frame, (320, 96), explore_frontier=True)
        self.assertTrue(context["frontier_exploration"])
        self.assertTrue(context["candidates"])
        self.assertTrue(all(route["waypoints"][0]["position"][0] == 32
                            for route in context["candidates"].values()))

    def test_scene_target_uses_observed_routes_after_native_field_control(self):
        record = self.frame()
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 0)
        record["data"]["scene"]["event"]["phase"] = 4
        self.assertIsNone(goal.request(Frame.decode(record)))
        record["data"]["scene"]["event"]["phase"] = 0
        context = goal.request(Frame.decode(record))
        self.assertEqual(context["destination_world_xz"], (192, 64))
        self.assertTrue(context["scene_trigger_objective"]["requires_actual_native_activation"])
        self.assertIsNone(goal.activation_tick)
        self.assertEqual(record["data"]["actors"][0]["position"], [32, 0, 96])

    def test_scene_target_allows_a_normal_strategy_change_before_leaving_targeting(self):
        record = self.frame()
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 0)
        record["data"]["control_mode"] = 2
        frame = Frame.decode(record)
        self.assertTrue(goal.pending(frame))
        self.assertFalse(goal.ready(frame))
        self.assertIsNone(goal.request(frame))
        record["data"]["scene"]["event"]["phase"] = 4
        self.assertFalse(goal.pending(Frame.decode(record)))

    def test_scene_target_can_leave_then_reenter_an_already_matching_tile(self):
        record = self.frame()
        record["data"]["actors"][0]["position"] = [160, 0, 96]
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 0)
        context = goal.request(frame)
        self.assertEqual(context["destination_world_xz"], (32, 96))
        self.assertTrue(context["scene_trigger_objective"]["staging_tile_change"])
        record["data"]["actors"][0]["position"] = [32, 0, 96]
        context = goal.request(Frame.decode(record))
        self.assertEqual(context["destination_world_xz"], (192, 64))
        self.assertIsNone(goal.activation_tick)
        self.assertNotIn("passed", goal.result())

    def test_scene_target_needs_the_actual_native_room_source_and_selector(self):
        record = self.frame()
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 0)
        event = record["data"]["scene"]["event"]
        event.update({"phase": 4, "source": 1, "id": 0})
        goal.observe(Frame.decode(record))
        self.assertIsNone(goal.activation_tick)
        event.update({"source": 2, "id": 1})
        goal.observe(Frame.decode(record))
        self.assertIsNone(goal.activation_tick)
        event["id"] = 0
        record["data"]["room"] += 1
        goal.observe(Frame.decode(record))
        self.assertIsNone(goal.activation_tick)
        record["data"]["room"] -= 1
        goal.navigator.path = [{"position": (160, 0, 96), "jump": False}]
        goal.observe(Frame.decode(record))
        self.assertEqual(goal.activation_tick, frame.tick)
        self.assertFalse(goal.navigator.path)
        self.assertIsNone(goal.request(Frame.decode(record)))

    def test_scene_target_never_guesses_an_unobserved_staging_support(self):
        record = self.frame()
        record["data"]["actors"][0]["position"] = [160, 0, 64]
        record["data"]["navigation"].update(
            origin=[160, 64], width=1, depth=1, samples=[[0, -2032, 5]],
        )
        frame = Frame.decode(record)
        with self.assertRaisesRegex(ValueError, "No observed adjacent support"):
            SceneTileGoal((1, 0), frame.zone, frame.room, 0).request(frame)

    def test_scene_target_rejects_a_route_from_before_native_scene_ownership(self):
        record = self.frame()
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 0)
        goal.request(frame)
        record["tick"] += 1
        record["data"]["scene"]["event"]["phase"] = 4
        goal.observe(Frame.decode(record))
        record["tick"] += 1
        record["data"]["scene"]["event"]["phase"] = 0
        with self.assertRaises(StaleDecision):
            goal.accept(NavigationDecision("route1", "cross original tile", 1, frame.tick),
                        Frame.decode(record))
        self.assertFalse(goal.navigator.path)

    def test_scene_target_rejects_non_native_coordinates(self):
        for tile in ((-1, 0), (256, 0), (True, 0), (1,), [1, 0]):
            with self.subTest(tile=tile), self.assertRaises(ValueError):
                SceneTileGoal(tile, 1, 0, 0)

    def test_explicit_door_navigation_can_leave_a_hostile_honestly_alive(self):
        record = self.frame()
        enemy = copy.deepcopy(sample_record()["data"]["actors"][1])
        record["data"]["actors"].append(enemy)
        frame = Frame.decode(record)
        navigator = TerrainNavigator()
        self.assertIsNone(navigator.request(frame, (160, 96)))
        self.assertIsNotNone(navigator.request(frame, (160, 96), avoid_hostiles=True))
        navigator.accept(NavigationDecision("route1", "Continue to the original door", 1, 900), frame)
        self.assertIsNotNone(navigator.step(frame))
        self.assertEqual(frame.enemies[0].hp, 20)
        self.assertEqual(len(frame.enemies), 1)

    def test_ordinary_combat_motion_is_preserved_until_measured_approach_stalls(self):
        navigator = TerrainNavigator()
        record = self.frame()
        enemy = copy.deepcopy(sample_record()["data"]["actors"][1])
        enemy["position"] = [640, -448, 96]
        record["data"]["actors"].append(enemy)
        for index in range(10):
            record["tick"] = index*8 + 1
            record["data"]["actors"][0]["position"][0] += 18
            frame = Frame.decode(record)
            self.assertFalse(navigator.ordinary_approach_stalled(frame, frame.enemies[0], 8))
        stalled = False
        for index in range(12):
            record["tick"] += 8
            frame = Frame.decode(record)
            stalled = navigator.ordinary_approach_stalled(frame, frame.enemies[0], 8)
        self.assertTrue(stalled)
        self.assertEqual(navigator.feedback[-1]["waypoint"]["mode"], "ordinary_approach")

    def test_failed_walking_descent_proposes_a_logged_native_jump_before_rejecting_landing(self):
        record = self.frame()
        record["data"]["actors"][0]["position"] = [32, -640, 96]
        samples = record["data"]["navigation"]["samples"]
        for sample in samples:
            sample[2] = 0
        samples[3] = [32128, -2032, 5]
        samples[4] = [32640, -2032, 1]
        samples[5] = [32640, -2032, 5]
        navigator = TerrainNavigator()
        navigator.request(Frame.decode(record), (160, 96))
        navigator.accept(NavigationDecision("route1", "descend", 1, 900), Frame.decode(record))
        for tick in range(10, 160, 8):
            record["tick"] = tick
            navigator.step(Frame.decode(record))
        self.assertEqual(navigator.feedback[-1]["retry_mode"], "directed_jump")
        context = navigator.request(Frame.decode(record), (160, 96))
        self.assertTrue(context["candidates"]["route1"]["waypoints"][-1]["jump"])

    def test_native_steep_descent_also_waits_for_sheathed_weapon(self):
        navigator = TerrainNavigator()
        record = self.frame()
        record["data"]["actors"][0]["weapon_drawn"] = 1
        navigator.request(Frame.decode(record), (160, 96))
        navigator.accept(NavigationDecision("route1", "advance", 1, 900), Frame.decode(record))
        navigator.path = [{"position": (160, 512, 96), "jump": False,
                           "takeoff": None, "drop": 512}]
        self.assertEqual(navigator.step(Frame.decode(record)).buttons, frozenset({"Cross"}))

    def test_pursuit_can_descend_a_real_ledge_without_inventing_a_jump(self):
        record = self.frame()
        record["data"]["actors"][0]["position"] = [32, -640, 96]
        enemy = copy.deepcopy(sample_record()["data"]["actors"][1])
        enemy["position"] = [160, -128, 96]
        record["data"]["actors"].append(enemy)
        samples = record["data"]["navigation"]["samples"]
        for sample in samples:
            sample[2] = 0
        samples[3] = [32128, -2032, 5]
        samples[4] = [32640, -2032, 1]
        samples[5] = [32640, -2032, 5]
        context = TerrainNavigator().request(Frame.decode(record), (160, 96), target_slot=2)
        route = context["candidates"]["route1"]["waypoints"]
        self.assertEqual(route[-1]["position"], (160, -128, 96))
        self.assertFalse(route[-1]["jump"])

    def test_selected_live_hostile_can_use_terrain_without_ignoring_identity_changes(self):
        record = self.frame()
        enemy = copy.deepcopy(sample_record()["data"]["actors"][1])
        enemy["position"] = [160, 0, 96]
        record["data"]["actors"].append(enemy)
        frame = Frame.decode(record)
        navigator = TerrainNavigator()
        self.assertIsNone(navigator.request(frame, (160, 96)))
        context = navigator.request(frame, (160, 96), target_slot=2)
        self.assertEqual(context["target_identity"], (2, 58, 0))
        decision = NavigationDecision("route1", "Approach the selected live hostile", 1, 900)
        navigator.accept(decision, frame)
        self.assertIsNotNone(navigator.step(frame))
        record["tick"] = 10
        record["data"]["actors"][1]["spawn_index"] = 1
        with self.assertRaises(StaleDecision):
            navigator.accept(decision, Frame.decode(record))
        self.assertIsNone(navigator.step(Frame.decode(record)))
        self.assertEqual(navigator.path, [])

    def test_native_climb_waits_for_normal_sheathe_acknowledgment(self):
        navigator = TerrainNavigator()
        record = self.frame()
        record["data"]["actors"][0]["weapon_drawn"] = 1
        navigator.request(Frame.decode(record), (160, 96))
        navigator.accept(NavigationDecision("route1", "advance", 1, 900), Frame.decode(record))
        navigator.path = [{"position": (160, -128, 96), "jump": True,
                           "takeoff": (32, 0, 96)}]
        self.assertEqual(navigator.step(Frame.decode(record)).buttons, frozenset({"Cross"}))
        record["tick"] = 10
        self.assertIsNone(navigator.step(Frame.decode(record)))
        self.assertFalse(navigator.path[0].get("jump_sent", False))
        record["tick"] = 20
        record["data"]["actors"][0]["weapon_drawn"] = 0
        self.assertIn("Square", navigator.step(Frame.decode(record)).buttons)

    def test_jump_requires_observed_takeoff_alignment_and_native_ground_readiness(self):
        navigator = TerrainNavigator()
        record = self.frame()
        navigator.request(Frame.decode(record), (160, 96))
        navigator.accept(NavigationDecision("route1", "advance", 1, 900), Frame.decode(record))
        navigator.path = [{"position": (160, -128, 96), "jump": True,
                           "takeoff": (96, 0, 96)}]
        self.assertNotIn("Square", navigator.step(Frame.decode(record)).buttons)
        record["tick"] = 10
        record["data"]["actors"][0]["position"] = [96, 0, 96]
        record["data"]["navigation"]["movement_mode"] = 1
        self.assertNotIn("Square", navigator.step(Frame.decode(record)).buttons)
        self.assertFalse(navigator.path[0].get("jump_sent", False))
        record["tick"] = 20
        record["data"]["navigation"]["movement_mode"] = 0
        self.assertIn("Square", navigator.step(Frame.decode(record)).buttons)

    def test_native_jump_proposal_can_cross_a_ledge_but_not_a_wall_or_pit(self):
        record = self.frame()
        record["data"]["actors"][0]["position"] = [32, 0, 96]
        record["data"]["actors"][0]["weapon_drawn"] = 0
        data = record["data"]["navigation"]
        data["samples"][4] = [32640, -2032, 1]
        data["samples"][5] = [32640, -2032, 5]
        for index in (0, 1, 2, 6, 7, 8):
            data["samples"][index][2] = 0
        context = TerrainNavigator().request(Frame.decode(record), (160, 96))
        self.assertTrue(context["candidates"]["route1"]["waypoints"][0]["jump"])
        data["samples"][4][2] = 0
        with self.assertRaisesRegex(ValueError, "outgoing navigation"):
            TerrainNavigator().request(Frame.decode(record), (160, 96))
        data["samples"][4][2] = 3
        with self.assertRaisesRegex(ValueError, "outgoing navigation"):
            TerrainNavigator().request(Frame.decode(record), (160, 96))

    def test_door_approach_continues_without_an_object_interaction_acknowledgment(self):
        record = self.frame()
        record["data"]["actors"][0]["position"] = [32, 0, 96]
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 1, "tile_z": 0, "layer": 0, "required_value": 0,
             "current_flag_value": 0},
        ]
        door = PhysicalDoor("z009-r006-d000")
        frame = Frame.decode(record)
        door.observe(frame)
        self.assertIsNotNone(door.navigation_request(frame))
        door.interaction_seen = True
        self.assertIsNotNone(door.navigation_request(frame))

    def test_corner_jitter_is_not_waypoint_progress(self):
        navigator = TerrainNavigator()
        frame = Frame.decode(self.frame())
        navigator.request(frame, (160, 96))
        navigator.accept(NavigationDecision("route1", "advance", 1, 900), frame)
        navigator.path = [{"position": (96, 0, 96), "jump": False}]
        navigator.progress_position = (84, 0, 160)
        for index, tick in enumerate(range(10, 160, 8)):
            record = self.frame(tick)
            record["data"]["actors"][0]["position"] = [84 if index % 2 else 108, 0, 160]
            navigator.step(Frame.decode(record))
        self.assertEqual(navigator.path, [])
        self.assertEqual(navigator.feedback[-1]["status"], "movement_blocked")

    def test_cheap_sideways_steps_cannot_compete_with_routes_to_the_goal(self):
        record = self.frame()
        record["data"]["actors"][0]["position"] = [351, 0, 537]
        record["data"]["navigation"].update(
            origin=[32, 32], width=7, depth=13,
            samples=[[0, -2032, 5] for _ in range(7*13)],
        )
        context = TerrainNavigator().request(Frame.decode(record), (320, 64))
        cheapest = min(context["candidates"].values(), key=lambda route: route["estimated_cost"])
        self.assertLess(cheapest["remaining_distance"], 200)
        self.assertTrue(all(route["remaining_distance"] < 200
                            for route in context["candidates"].values()))

    def frame(self, tick=1):
        record = sample_record(tick, active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["actors"] = record["data"]["actors"][:1]
        record["data"]["actors"][0]["weapon_drawn"] = 0
        record["data"]["actors"][0]["position"] = [32, 0, 96]
        record["data"]["navigation"] = {
            "available": True, "origin": [32, 32], "width": 3, "depth": 3,
            "spacing": 64, "collision_height": 128,
            "movement_mode": 0, "locomotion_state": 0, "route_phase": 0,
            "jumping": False, "facing_tile": [1, 0, 0],
            "samples": [[0, -2032, 5] for _ in range(9)],
        }
        return record

    def test_routes_go_around_blocked_terrain_instead_of_through_it(self):
        record = self.frame()
        record["data"]["navigation"]["samples"][4][2] = 0
        navigator = TerrainNavigator()
        context = navigator.request(Frame.decode(record), (160, 96))
        self.assertTrue(context["candidates"])
        first = next(iter(context["candidates"]))
        navigator.accept(NavigationDecision(first, "Observed route around wall", 1, 900),
                         Frame.decode(record))
        self.assertNotIn((96, 0, 96), [step["position"] for step in navigator.path])
        self.assertEqual(navigator.path[-1]["position"], (160, 0, 96))
        self.assertIsNotNone(navigator.step(Frame.decode(self.frame(10))))

    def test_invalid_missing_pit_and_low_ceiling_are_not_walkable(self):
        record = self.frame()
        data = record["data"]["navigation"]
        data["samples"][0] = [-32768, -2032, 5]
        data["samples"][1] = [0, -2032, 7]
        data["samples"][2] = [0, 32704, 5]
        terrain = Terrain.decode(data)
        self.assertEqual([terrain.supported(index) for index in range(4)],
                         [False, False, False, True])
        data["samples"].pop()
        with self.assertRaisesRegex(ValueError, "sample count"):
            Terrain.decode(data)

    def test_scene_change_and_new_hostile_invalidate_navigation(self):
        navigator = TerrainNavigator()
        record = self.frame()
        navigator.request(Frame.decode(record), (160, 96))
        decision = NavigationDecision("route1", "advance", 1, 900)
        record["data"]["room"] = 7
        with self.assertRaises(StaleDecision):
            navigator.accept(decision, Frame.decode(record))
        with self.assertRaises(StaleDecision):
            navigator.accept(decision, Frame.decode(sample_record()))

    def test_model_cannot_invent_a_waypoint_or_command(self):
        frame = Frame.decode(self.frame())
        context = TerrainNavigator().request(frame, (160, 96))
        model = LocalModel()
        for value in ({"choice": "route1", "reason": "advance"},
                      {"choice": "teleport", "reason": "shortcut"},
                      {"choice": "route1", "reason": "advance", "set_position": [1, 2, 3]}):
            response = {"done": True, "done_reason": "stop",
                        "message": {"content": json.dumps(value)}}
            with self.subTest(value=value), mock.patch.object(model, "request", return_value=response):
                if value == {"choice": "route1", "reason": "advance"}:
                    self.assertEqual(model.navigate(frame, "door", context).choice, "route1")
                else:
                    with self.assertRaises(ModelResponseError):
                        model.navigate(frame, "door", context)


class ReactionTests(unittest.TestCase):
    def test_defeated_offensive_target_has_no_continuation_window(self):
        reactor = ReactionController()
        record = sample_record(timer=40)
        reactor.observe(Frame.decode(record))
        record["tick"] = 2
        record["data"]["reaction_timing"]["timer"] = 42
        record["data"]["actors"][1]["hp"] = 0
        self.assertIsNone(reactor.observe(Frame.decode(record)))
        record["tick"] = 3
        record["data"]["reaction_timing"]["active"] = False
        reactor.observe(Frame.decode(record))
        reactor.finish()
        self.assertEqual(reactor.results, [])

    def test_target_defeat_cancellation_never_hides_a_native_rejection(self):
        for rejected in (False, True):
            with self.subTest(rejected=rejected):
                reactor = ReactionController()
                record = sample_record(timer=42)
                self.assertIsNotNone(reactor.observe(Frame.decode(record)))
                record["tick"] = 2
                record["data"]["reaction_timing"].update(timer=44, attempted=rejected)
                record["data"]["actors"][1]["hp"] = 0
                reactor.observe(Frame.decode(record))
                reactor.finish()
                self.assertEqual([row["status"] for row in reactor.results],
                                 ["rejected"] if rejected else [])

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
    def test_new_game_cli_wires_an_exclusive_production_source(self):
        from vs_player import cli
        command = ["player", "play", "--game-exe", "VS.Desktop.exe",
                   "--data", "data", "--new-game"]
        with (mock.patch.object(sys, "argv", command),
              mock.patch.object(cli, "play", return_value={"passed": True}) as run,
              mock.patch("sys.stdout", new_callable=io.StringIO)):
            self.assertEqual(cli.main(), 0)
        options = run.call_args.args[1]
        self.assertTrue(options.new_game)
        self.assertIsNone(options.scenario)
        self.assertIsNone(options.save_file)
        for extra in (["--save-file", "earned.vssave"], ["--scenario", "fixture.json"]):
            with (self.subTest(extra=extra), mock.patch.object(sys, "argv", command + extra),
                  mock.patch("sys.stderr", new_callable=io.StringIO),
                  self.assertRaises(SystemExit) as caught):
                cli.main()
            self.assertEqual(caught.exception.code, 2)

    def test_production_new_game_does_not_use_continue_or_scene_skip_inputs(self):
        options = PlayOptions(Path("VS.Desktop.exe"), Path("data"), None, new_game=True)
        self.assertEqual(options.desktop_startup_presses(), "1000:Start,1300:Circle")
        continued = PlayOptions(Path("VS.Desktop.exe"), Path("data"), None,
                                save_file=Path("earned.vssave"))
        self.assertEqual(continued.desktop_startup_presses(),
                         "1000:Start,1300:Circle,1450:Circle,1510:Up,1570:Circle")
        for changes in (
            {"save_file": Path("earned.vssave")},
            {"scenario": Path("fixture.json")},
            {"executable": Path("VS.CombatLab.exe")},
            {"setup_presses": "120:Up"},
        ):
            arguments = {"executable": Path("VS.Desktop.exe"), "data": Path("data"),
                         "scenario": None, "new_game": True}
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                PlayOptions(**(arguments | changes)).desktop_startup_presses()
        with self.assertRaises(ValueError):
            PlayOptions(Path("VS.Desktop.exe"), Path("data"), None).desktop_startup_presses()

    def test_transport_json_is_lossless_and_compact(self):
        client = LocalModel()
        payload = {"model": "test", "messages": [{"role": "user", "content": "Exact facts."}]}
        with mock.patch.object(client.opener, "open", return_value=io.BytesIO(b'{"done":true}')) as opener:
            self.assertEqual(client.request("/api/chat", payload), {"done": True})
        body = opener.call_args.args[0].data.decode()
        self.assertEqual(json.loads(body), payload)
        self.assertEqual(body, json.dumps(payload, separators=(",", ":")))

    def test_navigation_json_is_compact_without_discarding_geometry(self):
        client = LocalModel()
        context = {"candidates": {"observed-route": {"waypoints": [[64, 128], [192, 128]]}}}
        response = {
            "done": True, "done_reason": "stop",
            "message": {"content": '{"choice":"observed-route","reason":"Follow supported tiles."}'},
        }
        with mock.patch.object(client, "request", return_value=response) as request:
            client.navigate(Frame.decode(sample_record()), "reach the door", context)
        content = request.call_args.args[1]["messages"][1]["content"]
        prompt = json.loads(content)
        self.assertEqual(prompt["navigation"], context)
        self.assertEqual(content, json.dumps(prompt, separators=(",", ":")))

    def test_planning_json_is_compact_without_discarding_observations(self):
        client = LocalModel()
        frame = Frame.decode(sample_record())
        response = {
            "done": True, "done_reason": "stop",
            "message": {"content": json.dumps({
                "choice": "attack:2:-1:1", "chain_priority": "123",
                "defense_priority": "321", "reason": "Attack the observed hostile.",
            })},
        }
        with mock.patch.object(client, "request", return_value=response) as request:
            client.decide(frame, "fight", [], {"target_choice_required": True})
        content = request.call_args.args[1]["messages"][1]["content"]
        prompt = json.loads(content)
        self.assertEqual(prompt["observation"], json.loads(json.dumps(frame.planning_data())))
        self.assertEqual(prompt["current_run"], {"target_choice_required": True})
        self.assertEqual(content, json.dumps(prompt, separators=(",", ":")))

    def test_http_failure_preserves_bounded_server_details(self):
        client = LocalModel()
        for detail in (b'{"error":"request exceeds context size"}', b"x" * 10000):
            with self.subTest(length=len(detail)):
                error = urllib.error.HTTPError(
                    client.endpoint + "/api/chat", 400, "Bad Request", {},
                    io.BytesIO(detail),
                )
                with mock.patch.object(client.opener, "open", side_effect=error):
                    with self.assertRaises(RuntimeError) as caught:
                        client.request("/api/chat", {"model": "test"})
                message = str(caught.exception)
                self.assertIn("400", message)
                self.assertIn("/api/chat", message)
                self.assertIn(detail[:100].decode(), message)
                self.assertLess(len(message), 4300)
                self.assertTrue(error.closed)

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
            "item_id": -1,
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
    def test_spatial_retrieval_is_scoped_and_survives_raw_log_cleanup(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            paths = PlayerPaths(Path(directory) / "state")
            store = RunStore(paths)
            run = store.create("spatial observation", {})
            payload = {
                "zone": 9, "room": 17, "door": "z009-r017-d002", "geometry_sha256": "fixture",
                "observation": {"status": "movement_blocked", "tick": 150,
                                "player": [402, 0, 312], "waypoint": [416, 0, 288]},
            }
            store.event(run, "navigation.outcome", payload)
            store.finish(run, "passed", {})
            store.cleanup(Retention(successful_days=1), now=10**12)
            store.close()
            store = RunStore(paths)
            try:
                self.assertEqual(store.navigation_experience(9, 17, "z009-r017-d002", "fixture"), [payload])
                self.assertEqual(store.navigation_experience(9, 17, "z009-r017-d002", "changed"), [])
                self.assertEqual(store.navigation_experience(9, 12, "z009-r017-d002", "fixture"), [])
            finally:
                store.close()
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
    def test_strategic_response_cannot_start_preparation_in_a_native_cinematic(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        observed = Frame.decode(record)
        decision = Decision("wait", -1, -1, (1, 2, 3), (1, 2, 3),
                            "Wait for owned control.", observed.tick, 0)
        controller = CombatController()
        record["tick"] += 50
        record["data"]["control_mode"] = 3
        record["data"]["scene"]["event"]["phase"] = 2
        self.assertFalse(strategy_planning_ready(Frame.decode(record)))
        with self.assertRaises(StaleDecision):
            controller.accept(decision, Frame.decode(record), observed)
        self.assertIsNone(controller.decision)
        self.assertFalse(controller.preparing)

    def test_event_owned_ability_reward_uses_only_the_real_enabled_rows(self):
        record = sample_record(tick=10, active=False)
        record["data"].update(control_mode=3, action_phase=6)
        record["data"]["scene"]["event"]["phase"] = 4
        record["data"]["menu"].update(
            available=True, event_owned=True, list_phase=17, selected_row=0,
            rows=[{"index": 0, "name": "Unavailable", "disabled": True},
                  {"index": 1, "name": "Earned chain", "disabled": False}],
        )
        frame = Frame.decode(record)
        choices = available_choices(frame, {})
        self.assertEqual({row["action_id"] for row in choices.values()}, {1})
        controller = CombatController()
        decision = Decision("reward", -1, -1, (1, 2, 3), (1, 2, 3),
                            "Choose the earned ability.", 10, 0, action_id=1)
        controller.accept(decision, frame, frame)
        self.assertEqual(controller.step(frame).buttons, frozenset({"Down"}))
        for button in ("Circle", "Up", "Down"):
            self.assertTrue(input_lease_permitted(frame, frozenset({button})))
        self.assertFalse(input_lease_permitted(frame, frozenset({"Square"})))
        self.assertFalse(input_lease_permitted(frame, frozenset({"Circle", "Right"})))
        record["tick"] = 20
        record["data"]["menu"]["selected_row"] = 1
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))
        record["data"]["menu"]["event_owned"] = False
        self.assertFalse(input_lease_permitted(Frame.decode(record), frozenset({"Circle"})))
        self.assertTrue(all(choice["intent"] != "reward"
                            for choice in available_choices(Frame.decode(record), {}).values()))

    def test_event_owned_loot_remains_interactive_without_authorizing_cutscene_movement(self):
        record = sample_record(tick=10, active=False)
        record["data"].update(control_mode=3, action_phase=4)
        record["data"]["scene"]["event"]["phase"] = 4
        record["data"]["menu"].update(available=True, event_owned=True, selected_menu=12,
                                       loot_input_ready=True, loot_count=2)
        frame = Frame.decode(record)
        controller = CombatController()
        self.assertEqual(controller.step(frame).buttons, frozenset({"Square"}))
        self.assertTrue(input_lease_permitted(frame, frozenset({"Square"})))
        self.assertFalse(input_lease_permitted(frame, frozenset({"Up"})))
        self.assertFalse(input_lease_permitted(frame, frozenset({"Square", "Up"})))
        record["data"]["menu"]["event_owned"] = False
        self.assertFalse(input_lease_permitted(Frame.decode(record), frozenset({"Square"})))

    def test_native_target_menu_exposes_explicit_limb_choices_to_the_model(self):
        record = sample_record(tick=10, active=False)
        record["data"].update(control_mode=2, action_phase=1)
        record["data"]["target_previews"] = [
            {"actor": 2, "limb": 0, "position": [128, -100, 64]},
            {"actor": 2, "limb": 3, "position": [128, -20, 64]},
        ]
        record["data"]["target_page"].update(first=0, visible=2, selected=0, selection_step=19)
        choices = available_choices(Frame.decode(record), {})
        attacks = [choice for choice in choices.values() if choice["intent"] == "attack"]
        self.assertEqual({choice["target_limb"] for choice in attacks}, {0, 3})

    def test_unspecified_limb_does_not_confirm_the_first_native_menu_row(self):
        record = sample_record(tick=10, active=False)
        record["data"].update(control_mode=2, action_phase=1)
        record["data"]["target_previews"] = [
            {"actor": 2, "limb": 0, "position": [128, -100, 64]},
            {"actor": 2, "limb": 3, "position": [128, -20, 64]},
        ]
        record["data"]["target_page"].update(first=0, visible=2, selected=0, selection_step=19)
        frame = Frame.decode(record)
        controller = CombatController()
        decision = Decision("attack", 2, -1, (1, 2, 3), (1, 2, 3), "open native targets", 10, 0)
        controller.accept(decision, frame, frame)
        self.assertIsNone(controller.step(frame))
        controller.accept(Decision("attack", 2, 3, (1, 2, 3), (1, 2, 3),
                                   "choose observed limb", 10, 0), frame, frame)
        self.assertEqual(controller.step(frame).buttons, frozenset({"Down"}))
        record["tick"] = 20
        record["data"]["target_page"]["selected"] = 1
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))

    def test_native_whole_actor_targets_do_not_require_a_nonexistent_limb(self):
        record = sample_record(tick=10, active=False)
        record["data"].update(control_mode=2, action_phase=1)
        record["data"]["target_previews"] = [
            {"actor": 2, "limb": -1, "position": [128, 0, 64]},
        ]
        record["data"]["target_page"].update(first=0, visible=1, selected=0, selection_step=19)
        frame = Frame.decode(record)
        controller = CombatController()
        controller.accept(Decision("attack", 2, -1, (1, 2, 3), (1, 2, 3),
                                   "whole actor target", 10, 0), frame, frame)
        self.assertFalse(controller.needs_target_choice(frame))
        self.assertEqual(controller.step(frame).buttons, frozenset({"Circle"}))

    def test_native_limb_choice_is_rejected_if_the_menu_changes_during_planning(self):
        record = sample_record(tick=10, active=False)
        record["data"].update(control_mode=2, action_phase=1)
        record["data"]["target_previews"] = [
            {"actor": 2, "limb": 3, "position": [128, 0, 64]},
        ]
        record["data"]["target_page"]["selection_step"] = 19
        observed = Frame.decode(record)
        record["data"]["target_previews"] = []
        controller = CombatController()
        with self.assertRaisesRegex(StaleDecision, "target limb"):
            controller.accept(Decision("attack", 2, 3, (1, 2, 3), (1, 2, 3),
                                       "observed native limb", 10, 0),
                              Frame.decode(record), observed)

    def test_native_loot_claim_requires_the_actual_ready_loot_screen(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=2, action_phase=12)
        record["data"]["menu"].update(available=True, selected_menu=12,
                                      loot_input_ready=True, loot_count=1)
        controller = CombatController()
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Square"}))
        record["tick"] = 10
        record["data"]["menu"]["loot_input_ready"] = False
        self.assertIsNone(controller.step(Frame.decode(record)))

    def item_record(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["actors"][0]["hp"] = 50
        record["data"]["items"] = [{"slot": 0, "id": 323, "count": 2}]
        record["data"]["item_options"] = [{
            "id": 323, "action_id": 224, "count": 2, "name": "Recover",
            "restrictions": 4, "effect_id": 62, "calculator_id": 15, "stat_factor": 10,
        }]
        return record

    def test_owned_healing_item_does_not_require_a_learned_healing_spell(self):
        record = self.item_record()
        frame = Frame.decode(record)
        choices = available_choices(frame, {})
        self.assertEqual({row["intent"] for row in choices.values()}, {"item"})
        self.assertEqual({row["item_id"] for row in choices.values()}, {323})
        record["data"]["item_options"][0]["effect_id"] = 63
        self.assertNotIn("item", {row["intent"] for row in available_choices(Frame.decode(record), {}).values()})

    def test_observed_large_hits_offer_noncritical_recovery_without_hiding_combat(self):
        record = self.item_record()
        record["data"]["actors"][0].update(hp=155, max_hp=250)
        frame = Frame.decode(record)
        self.assertNotIn("item", {row["intent"] for row in available_choices(frame, {}).values()})
        choices = available_choices(frame, {"max_incoming_hp_damage": 106})
        self.assertEqual({row["intent"] for row in choices.values()},
                         {"item", "attack", "approach", "retreat"})

    def test_dullahan_threat_margin_keeps_healing_and_counterattack_selectable(self):
        record = self.item_record()
        record["data"]["actors"][0].update(hp=162, max_hp=250)
        record["data"]["actors"][1].update(hp=117, max_hp=186)
        choices = available_choices(Frame.decode(record), {"max_incoming_hp_damage": 88})
        self.assertIn("item:0:224:1:323", choices)
        self.assertIn("attack:2:-1:1", choices)
        self.assertIn("retreat:2:-1:1", choices)

    def test_one_survivable_observed_hit_retains_urgent_recovery_priority(self):
        record = self.item_record()
        record["data"]["actors"][0].update(hp=155, max_hp=250)
        choices = available_choices(Frame.decode(record), {"max_incoming_hp_damage": 160})
        self.assertEqual({row["intent"] for row in choices.values()}, {"item"})
        record["data"]["actors"][0].update(hp=200, max_hp=250)
        choices = available_choices(Frame.decode(record), {"max_incoming_hp_damage": 300})
        self.assertIn("item:0:224:1:323", choices)
        self.assertIn("retreat:2:-1:1", choices)

    def test_item_preparation_cancels_old_attack_targeting_without_refusing_the_item(self):
        record = self.item_record()
        record["data"].update(control_mode=2, action_phase=1, source_actor=0)
        frame = Frame.decode(record)
        decision = Decision("item", 0, -1, (1, 2, 3), (1, 2, 3),
                            "Recover before another large hit", 1, 900, 224, 1, 323)
        controller = CombatController()
        controller.accept(decision, frame, frame)
        self.assertEqual(controller.step(frame).buttons, frozenset({"Cross"}))
        self.assertFalse(controller.transaction.closing)
        record["tick"] = 10
        record["data"].update(control_mode=1, action_phase=0)
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"L2"}))

    def test_consumable_shortcut_verifies_count_decrement_and_real_hp_recovery(self):
        for consumed in (False, True):
            with self.subTest(consumed=consumed):
                record = self.item_record()
                decision = Decision("item", 0, -1, (1, 2, 3), (1, 2, 3),
                                    "Use owned recovery", 1, 900, 224, 1, 323)
                transaction = MenuTransaction(decision, Frame.decode(record))
                opening = transaction.step(Frame.decode(record))
                self.assertEqual((opening.button, opening.duration), ("L2", 8))
                record["tick"] = 10
                record["data"]["control_mode"] = 6
                self.assertEqual(transaction.step(Frame.decode(record)).button, "L2+Up")
                record["tick"] = 20
                record["data"]["control_mode"] = 5
                record["data"]["menu"].update(
                    available=True, selected_menu=4, shortcut=5, list_phase=17, selected_row=0,
                    rows=[{"index": 0, "text": "Cure Root", "disabled": False,
                           "item_id": 323, "item_count": 2}],
                )
                self.assertEqual(transaction.step(Frame.decode(record)).button, "Circle")
                record["tick"] = 30
                record["data"].update(control_mode=2, action_phase=5, source_actor=0)
                record["data"]["reaction_timing"]["action"] = 224
                record["data"]["actors"][0]["hp"] = 99
                record["data"]["items"][0]["count"] = 1 if consumed else 2
                transaction.step(Frame.decode(record))
                record["tick"] = 40
                record["data"].update(control_mode=1, action_phase=0)
                transaction.step(Frame.decode(record))
                self.assertEqual(transaction.result["status"], "accepted" if consumed else "no_effect")
                self.assertEqual(transaction.result["item_id"], 323)
                self.assertEqual(transaction.result["effect"]["item_count_after"], 1 if consumed else 2)

    def test_door_navigation_is_a_choice_not_enemy_defeat_or_scene_reuse(self):
        frame = self.frame()
        self.assertNotIn("navigate", {row["intent"] for row in available_choices(frame, {}).values()})
        context = {"physical_navigation_objective": {"id": "z009-r006-d000"}}
        self.assertIn("navigate", {row["intent"] for row in available_choices(frame, context).values()})
        decision = Decision("navigate", -1, -1, (1, 2, 3), (1, 2, 3),
                            "Continue to the original door", 1, 900)
        controller = CombatController()
        controller.accept(decision, frame, frame)
        self.assertIsNone(controller.step(frame))
        targeting = self.frame(10, control_mode=2, action_phase=1)
        self.assertEqual(controller.step(targeting).buttons, frozenset({"Cross"}))
        self.assertIsNone(controller.step(self.frame(20, room=7)))
        self.assertIsNone(controller.decision)
        self.assertEqual(controller.feedback[-1]["kind"], "scene_changed")

    def test_active_terrain_path_never_suppresses_native_target_confirmation(self):
        controller = CombatController()
        original = self.frame()
        controller.accept(self.decision(), original, original)
        path = [{"position": (160, 0, 96), "jump": False}]
        frame = self.frame(
            10, control_mode=2, action_phase=1,
            target_page={"first": 0, "visible": 1, "selected": 0, "selection_step": 19},
            target_previews=[{"actor": 2, "limb": 0}],
        )
        self.assertFalse(field_navigation_ready(frame))
        action = controller.step(frame, allow_strategy=not (path and field_navigation_ready(frame)))
        self.assertEqual(action.buttons, frozenset({"Circle"}))

    def test_earned_reward_uses_enabled_native_rows_and_normal_confirmation(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=2, action_phase=14)
        record["data"]["actors"] = record["data"]["actors"][:1]
        record["data"]["menu"].update(
            available=True, list_phase=17, selected_row=0,
            rows=[{"index": 0, "text": "Chain", "disabled": True},
                  {"index": 1, "text": "Defense", "disabled": False}],
        )
        frame = Frame.decode(copy.deepcopy(record))
        choices = available_choices(frame, {})
        self.assertEqual(list(choices.values())[0]["intent"], "reward")
        self.assertEqual({choice["action_id"] for choice in choices.values()}, {1})
        decision = Decision("reward", -1, -1, (1, 2, 3), (1, 2, 3),
                            "Choose an earned defense", 1, 900, 1, 1)
        controller = CombatController()
        controller.accept(decision, frame, frame)
        self.assertEqual(controller.step(frame).buttons, frozenset({"Down"}))
        record["tick"] = 10
        record["data"]["menu"]["selected_row"] = 1
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))
        self.assertIsNone(controller.decision)
        record["data"]["menu"]["rows"][1]["text"] = "Changed menu"
        with self.assertRaises(StaleDecision):
            CombatController().accept(decision, Frame.decode(record), frame)

    def test_native_target_rejection_requires_movement_before_retry(self):
        controller = CombatController()
        original = self.frame()
        controller.accept(self.decision(), original, original)
        rejected = self.frame(10, control_mode=2, action_phase=1,
                              target_page={"selection_step": 16})
        self.assertEqual(controller.step(rejected).buttons, frozenset({"Cross"}))
        retry = self.frame(20)
        controller.accept(self.decision(20), retry, retry)
        movement = controller.step(retry)
        self.assertIsNotNone(movement)
        self.assertNotIn("Circle", movement.buttons)
        record = sample_record(30, active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["actors"][0]["position"][1] = -15
        self.assertNotIn("Circle", controller.step(Frame.decode(record)).buttons)
        record["tick"] = 40
        record["data"]["actors"][0]["position"][1] = -16
        self.assertIn("Circle", controller.step(Frame.decode(record)).buttons)

    def test_learned_chains_can_fill_an_empty_saved_loadout_without_grants(self):
        record = sample_record(active=False, chain_actions=[0, 0, 0])
        record["data"]["action_options"] = [
            {"id": action, "family": "chain", "restrictions": 0} for action in (22, 23, 28)
        ]
        choices = available_choices(Frame.decode(record), {})
        self.assertEqual({(row["action_id"], row["equip_slot"]) for row in choices.values()},
                         {(22, 1)})
        record["data"]["reaction_timing"]["chain_actions"][0] = 22
        choices = available_choices(Frame.decode(record), {})
        self.assertEqual({(row["action_id"], row["equip_slot"]) for row in choices.values()},
                         {(28, 2)})
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

    def test_caster_debuff_choices_keep_legal_magic_ward_visible(self):
        record = sample_record(active=False)
        record["data"]["actors"][0]["mp"] = 50
        record["data"]["actors"][1]["spell_action_ids"] = [130]
        record["data"]["action_options"] = [
            {"id": action, "family": "sorcerer", "cost": cost, "restrictions": 0}
            for action, cost in ((55, 7), (75, 7), (76, 21))
        ]
        choices = available_choices(Frame.decode(record), {})
        self.assertEqual({(row["target_slot"], row["action_id"]) for row in choices.values()},
                         {(2, 55), (2, 75), (0, 76)})

        for condition in ("active", "low_mp", "restricted", "unlearned", "no_caster", "blocked"):
            with self.subTest(condition=condition):
                variant = copy.deepcopy(record)
                context = {}
                if condition == "active":
                    variant["data"]["actors"][0]["statuses"] = variant["data"]["status_masks"][18]
                elif condition == "low_mp":
                    variant["data"]["actors"][0]["mp"] = 25
                elif condition == "restricted":
                    variant["data"]["action_options"][-1]["restrictions"] = 1
                elif condition == "unlearned":
                    variant["data"]["action_options"].pop()
                elif condition == "no_caster":
                    variant["data"]["actors"][1]["spell_action_ids"] = []
                else:
                    context["controller_feedback"] = [
                        {"action_id": 76, "target": 0, "status": "blocked"}
                    ]
                choices = available_choices(Frame.decode(variant), context)
                self.assertNotIn((0, 76), {(row["target_slot"], row["action_id"])
                                          for row in choices.values()})

    def test_caster_buff_visibility_preserves_urgent_healing_priority(self):
        record = sample_record(active=False)
        record["data"]["actors"][0].update(hp=20, mp=50)
        record["data"]["actors"][1]["spell_action_ids"] = [130]
        record["data"]["action_options"] = [
            {"id": action, "family": "sorcerer", "cost": cost, "restrictions": 0}
            for action, cost in ((55, 7), (75, 7), (76, 21), (96, 6))
        ]
        choices = available_choices(Frame.decode(record), {})
        self.assertEqual({(row["target_slot"], row["action_id"]) for row in choices.values()},
                         {(0, 96)})

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
    def test_accelerated_scene_cli_is_explicit_and_preserves_normal_defaults(self):
        from vs_player import cli
        command = ["player", "play", "--game-exe", "VS.CombatLab.exe", "--data", "data",
                   "--scenario", "fixture.json", "--scene-investigation", "--speed", "4"]
        with (mock.patch.object(sys, "argv", command),
              mock.patch.object(cli, "play", return_value={"passed": False}) as run,
              mock.patch("sys.stdout", new_callable=io.StringIO)):
            self.assertEqual(cli.main(), 2)
        options = run.call_args.args[1]
        self.assertEqual(options.simulation_speed, 4)
        self.assertFalse(options.strict_reactions)
        options.validate_simulation_speed()
        defaults = PlayOptions(Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
        self.assertEqual(defaults.simulation_speed, 1)
        self.assertTrue(defaults.strict_reactions)

    def test_accelerated_scene_investigation_cannot_certify_reaction_timing(self):
        basic = {"executable": Path("VS.CombatLab.exe"), "data": Path("data"),
                 "scenario": Path("fixture.json")}
        PlayOptions(**basic).validate_simulation_speed()
        PlayOptions(**basic, simulation_speed=4, strict_reactions=False).validate_simulation_speed()
        for speed in (0, 3, 8, True):
            with self.subTest(speed=speed), self.assertRaises(ValueError):
                PlayOptions(**basic, simulation_speed=speed,
                            strict_reactions=False).validate_simulation_speed()
        for changes in (
            {"strict_reactions": True},
            {"minimum_chains": 1},
            {"minimum_defenses": 1},
            {"door_case": "z009-r017-d002"},
            {"door_route": ("z009-r017-d002",)},
            {"new_game": True},
            {"save_file": Path("earned.vssave")},
            {"executable": Path("VS.Desktop.exe")},
        ):
            arguments = basic | {"simulation_speed": 4, "strict_reactions": False} | changes
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                PlayOptions(**arguments).validate_simulation_speed()

    def test_desktop_new_game_and_continue_finalize_from_host_not_lab_results(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text(
                json.dumps({"sequence": 1, "category": "host.exit",
                            "data": {"ticks": 18000, "game_finished": False}}) + "\n"
                + json.dumps({"sequence": 2, "category": "session.end",
                              "data": {"trace_failed": False}}) + "\n",
                encoding="utf-8",
            )
            reader = TraceReader(path)
            try:
                reader.read()
                for new_game in (False, True):
                    options = PlayOptions(
                        Path("VS.Desktop.exe"), Path("data"), None, new_game=new_game,
                        save_file=None if new_game else Path("earned.vssave"),
                    )
                    complete, result = native_outcome(options, reader, 0)
                    self.assertTrue(complete)
                    assert result is not None
                    self.assertFalse(result["game_finished"])
                    self.assertIn("new_game" if new_game else "continue", result["coverage"])
                    self.assertFalse(native_outcome(options, reader, 1)[0])
                reader.host_exit = None
                self.assertFalse(native_outcome(options, reader, 0)[0])
                options = PlayOptions(Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
                checkpoint = path.parent / "sequence-results.json"
                script = {"status": "completed", "teardown_complete": True, "engine_errors": 0}
                checkpoint.write_text(json.dumps({"scripts": [script]}), encoding="utf-8")
                self.assertTrue(native_outcome(options, reader, 0)[0])
                for change in ({"teardown_complete": False}, {"engine_errors": 1},
                               {"status": "blocked"}):
                    checkpoint.write_text(json.dumps({"scripts": [script | change]}), encoding="utf-8")
                    self.assertFalse(native_outcome(options, reader, 0)[0])
            finally:
                reader.close()

    def test_native_lab_scene_goal_is_explicit_and_preserves_default_script(self):
        basic = (Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
        options = PlayOptions(*basic)
        self.assertEqual(native_lab_script(options, Path("owned.json"), {}),
                         {"name": "external-player", "scenario": "owned.json", "frames": 3600})
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            path = Path(directory) / "scene.json"
            expectation = {"events": [2], "event_source": 2, "return_zone": 28,
                           "return_room": 16, "preconditions": "Original local2 after genuine victory"}
            path.write_text(json.dumps(expectation), encoding="utf-8")
            options = PlayOptions(*basic, scene_expectation=path, setup_presses="120:Up",
                                  scene_target_tile=(1, 17))
            script = native_lab_script(options, Path("owned.json"), {"room_events": "production"})
            self.assertEqual(script["scene"], expectation)
            self.assertEqual(script["press"], "120:Up")
            self.assertNotIn("stop", script)
            path.write_text(json.dumps(expectation | {"event_source": 0}), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "exactly one original room-local event"):
                native_lab_script(options, Path("owned.json"), {"room_events": "production"})
            path.write_text(json.dumps(expectation), encoding="utf-8")
            with self.assertRaises(ValueError):
                native_lab_script(options, Path("owned.json"), {"room_events": "isolated"})
            path.write_text("[]", encoding="utf-8")
            with self.assertRaises(ValueError):
                native_lab_script(options, Path("owned.json"), {"room_events": "production"})
        for change in ({"executable": Path("VS.Desktop.exe")}, {"scenario": None},
                       {"new_game": True}, {"save_file": Path("save.vssave")}):
            arguments = {"executable": basic[0], "data": basic[1], "scenario": basic[2],
                         "scene_expectation": Path("scene.json")} | change
            with self.subTest(change=change), self.assertRaises(ValueError):
                PlayOptions(**arguments).validate_scene_expectation()

    def test_cli_forwards_the_explicit_native_scene_expectation(self):
        from vs_player import cli

        command = ["player.py", "play", "--game-exe", "VS.CombatLab.exe",
                   "--data", "data", "--scenario", "fixture.json",
                   "--scene-expectation", "source-scene.json", "--scene-target-tile", "1", "17"]
        with (mock.patch.object(sys, "argv", command),
              mock.patch("vs_player.cli.play", return_value={"passed": True}) as managed,
              mock.patch("sys.stdout", new_callable=io.StringIO)):
            self.assertEqual(cli.main(), 0)
        options = managed.call_args.args[1]
        self.assertEqual(options.scene_expectation, Path("source-scene.json"))
        self.assertEqual(options.scenario, Path("fixture.json"))
        self.assertEqual(options.scene_target_tile, (1, 17))

    def test_native_scene_finalization_requires_exact_contract_control_and_teardown(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text(json.dumps({"sequence": 1, "category": "session.end",
                                        "data": {"trace_failed": False}}) + "\n", encoding="utf-8")
            reader = TraceReader(path)
            try:
                reader.read()
                basic = (Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
                options = PlayOptions(*basic, scene_expectation=Path("source-scene.json"))
                expected = {"events": [2], "event_source": 2, "return_zone": 28,
                            "return_room": 16, "preconditions": "Original local2 after genuine victory"}
                scene = {"coverage": "scene_playback_control", "status": "passed",
                         "expected_events": [2], "matched_events": 1, "event_source": 2,
                         "return_zone": 28, "return_room": 16,
                         "preconditions": expected["preconditions"],
                         "control_tick": 50, "passed_tick": 170}
                script = {"status": "passed", "teardown_complete": True,
                          "engine_errors": 0, "scene": scene}
                checkpoint = path.parent / "sequence-results.json"
                checkpoint.write_text(json.dumps({"scripts": [script]}), encoding="utf-8")
                self.assertTrue(native_outcome(options, reader, 0, expected)[0])
                self.assertFalse(native_outcome(options, reader, 0)[0])
                self.assertFalse(native_outcome(PlayOptions(*basic), reader, 0)[0])
                self.assertFalse(native_outcome(options, reader, 1, expected)[0])
                for change in ({"status": "blocked"}, {"expected_events": [3]},
                               {"matched_events": 0}, {"event_source": 0}, {"return_room": 17},
                               {"preconditions": "different"}, {"passed_tick": 169},
                               {"control_tick": True}):
                    with self.subTest(change=change):
                        checkpoint.write_text(json.dumps({
                            "scripts": [script | {"scene": scene | change}],
                        }), encoding="utf-8")
                        self.assertFalse(native_outcome(options, reader, 0, expected)[0])
                for change in ({"status": "completed"}, {"status": "cancelled"},
                               {"teardown_complete": False}, {"engine_errors": 1}):
                    checkpoint.write_text(json.dumps({"scripts": [script | change]}), encoding="utf-8")
                    self.assertFalse(native_outcome(options, reader, 0, expected)[0])
            finally:
                reader.close()

    def test_source_setup_releases_before_model_strategy_and_preserves_default(self):
        basic = (Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
        self.assertEqual(PlayOptions(*basic).setup_control_tick(), 0)
        options = PlayOptions(*basic, frames=300, setup_presses="120:Up,122:Up+Right")
        self.assertEqual(options.setup_control_tick(), 126)

    def test_source_setup_rejects_combat_skip_conflicts_and_unbounded_pulses(self):
        for pulses in ("120:Circle", "120:Cross", "120:Square", "120:Start", "120:L2",
                       "bad", "120:", "120:Up+Down", "120:Left+Right", "120:Up+Up",
                       "120:Up,121:Right", "120:Up,100:Right", "299:Up"):
            with self.subTest(pulses=pulses):
                options = PlayOptions(Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"),
                                      frames=300, setup_presses=pulses)
                with self.assertRaises(ValueError):
                    options.setup_control_tick()

    def test_source_setup_cannot_relax_physical_doors_or_replace_desktop_progression(self):
        basic = (Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
        for changes in ({"door_case": "z009-r006-d000"},
                        {"door_route": ("z009-r006-d000",)}, {"clear_enemies_first": True}):
            with self.subTest(changes=changes):
                options = PlayOptions(*basic, setup_presses="120:Up", **changes)
                with self.assertRaisesRegex(ValueError, "physical-door"):
                    options.setup_control_tick()
        options = PlayOptions(Path("VS.Desktop.exe"), Path("data"), None,
                              save_file=Path("earned.vssave"), setup_presses="120:Up")
        with self.assertRaisesRegex(ValueError, "Combat Lab"):
            options.setup_control_tick()

    def test_native_dialogue_circle_lease_survives_cutscene_control_only_while_visible(self):
        record = sample_record(active=False)
        record["data"]["control_mode"] = 3
        record["data"]["scene"]["event"].update(phase=4, dialogue_waits=[True] + [False] * 7)
        frame = Frame.decode(record)
        self.assertTrue(input_lease_permitted(frame, frozenset({"Circle"})))
        self.assertFalse(input_lease_permitted(frame, frozenset({"Up"})))
        self.assertFalse(input_lease_permitted(frame, frozenset({"Circle", "Up"})))
        record["data"]["scene"]["event"]["dialogue_waits"] = [False] * 8
        self.assertFalse(input_lease_permitted(Frame.decode(record), frozenset({"Circle"})))
        record["data"]["ready"] = False
        self.assertFalse(input_lease_permitted(Frame.decode(record), frozenset({"Circle"})))

    def test_post_combat_handoff_requests_the_real_door_without_live_targets(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 2, "tile_z": 0, "layer": 0, "required_value": 0,
             "current_flag_value": 0}
        ]
        goal = CombatGoal("z009-r006-d000", clear_enemies_first=True)
        goal.observe(Frame.decode(record))
        self.assertFalse(goal.exit_ready(Frame.decode(record)))
        record["data"]["actors"][1]["hp"] = 0
        goal.observe(Frame.decode(record))
        record["data"]["scene"]["event"]["phase"] = 4
        self.assertFalse(goal.exit_ready(Frame.decode(record)))
        record["data"]["scene"]["event"]["phase"] = 0
        frame = Frame.decode(record)
        self.assertTrue(goal.exit_ready(frame))
        choices = available_choices(frame, {"physical_navigation_objective": {"id": goal.door.identity}})
        self.assertEqual(list(choices.values())[0]["intent"], "navigate")

    def test_stale_observations_never_authorize_input_but_transient_gaps_recover(self):
        guard = ObservationFreshness(0.1)
        self.assertFalse(guard.observe(0.1425, 10.0, True))
        self.assertFalse(guard.observe(0.11, 10.5, True))
        self.assertTrue(guard.observe(0.005, 10.6, True))
        self.assertIsNone(guard.stale_since)
        self.assertFalse(guard.observe(0.2, 20.0, True))
        with self.assertRaisesRegex(TimeoutError, "recover"):
            guard.check_timeout(22.0)

    def test_explicit_combat_first_needs_observed_death_not_actor_disappearance(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        goal = CombatGoal("z009-r006-d000", clear_enemies_first=True)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 2, "tile_z": 0, "layer": 0, "required_value": 0,
             "current_flag_value": 0}
        ]
        goal.observe(Frame.decode(record))
        self.assertTrue(goal.combat_pending(Frame.decode(record)))
        removed = copy.deepcopy(record)
        removed["data"]["actors"] = removed["data"]["actors"][:1]
        goal.observe(Frame.decode(removed))
        self.assertTrue(goal.combat_pending(Frame.decode(removed)))
        record["data"]["actors"][1]["hp"] = 0
        goal.observe(Frame.decode(record))
        self.assertFalse(goal.combat_pending(Frame.decode(record)))

    def test_cinematic_only_actors_do_not_contaminate_a_real_combat_goal(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        goal = CombatGoal()
        goal.observe(Frame.decode(record))
        record["tick"] = 2
        record["data"]["actors"][1]["hp"] = 0
        record["data"]["control_mode"] = 3
        record["data"]["scene"]["event"]["phase"] = 4
        goal.observe(Frame.decode(record))
        self.assertEqual(len(goal.dead), 1)

        cinematic = copy.deepcopy(record)
        cinematic["tick"] = 3
        cinematic["data"]["room"] = 7
        cinematic["data"]["actors"][1].update(slot=3, model=41, hp=100, max_hp=100,
                                              spawn_index=1)
        cinematic["data"]["reaction_timing"].update(source_actor=3, target_actor=0,
                                                     incoming_hp_damage=999)
        goal.observe(Frame.decode(cinematic))
        self.assertEqual(len(goal.seen), 1)
        self.assertEqual(goal.encounters, {(58, 2)})
        self.assertEqual(goal.max_incoming_hp_damage, 0)

        record["data"]["actors"] = record["data"]["actors"][:1]
        record["data"]["control_mode"] = 1
        record["data"]["scene"]["event"]["phase"] = 0
        for tick in (4, 124):
            record["tick"] = tick
            goal.observe(Frame.decode(record))
        self.assertEqual(goal.accepted_tick, 124)

    def test_cinematic_actor_that_enters_normal_combat_still_needs_a_real_death(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=3, action_phase=0)
        goal = CombatGoal()
        goal.observe(Frame.decode(record))
        self.assertFalse(goal.seen)
        record["tick"] = 2
        record["data"].update(control_mode=1)
        record["data"]["scene"]["event"]["phase"] = 2
        goal.observe(Frame.decode(record))
        self.assertFalse(goal.seen)
        record["tick"] = 3
        record["data"]["scene"]["event"]["phase"] = 0
        record["data"]["reaction_timing"].update(source_actor=2, target_actor=0,
                                                 incoming_hp_damage=88)
        goal.observe(Frame.decode(record))
        self.assertEqual(len(goal.seen), 1)
        self.assertEqual(goal.max_incoming_hp_damage, 88)
        removed = copy.deepcopy(record)
        removed["tick"] = 200
        removed["data"]["actors"] = removed["data"]["actors"][:1]
        goal.observe(Frame.decode(removed))
        self.assertFalse(goal.dead)
        self.assertIsNone(goal.accepted_tick)

    def test_explicit_scene_combat_first_uses_actual_deaths_in_its_declared_room(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        frame = Frame.decode(record)
        scope = (frame.zone, frame.room)
        goal = CombatGoal(clear_enemies_first=True)
        goal.observe(frame)
        self.assertTrue(goal.combat_pending(frame, scope))
        removed = copy.deepcopy(record)
        removed["data"]["actors"] = removed["data"]["actors"][:1]
        goal.observe(Frame.decode(removed))
        self.assertTrue(goal.combat_pending(Frame.decode(removed), scope))
        record["data"]["actors"][1]["hp"] = 0
        goal.observe(Frame.decode(record))
        self.assertFalse(goal.combat_pending(Frame.decode(record), scope))
        self.assertIsNone(goal.door)

    def test_combat_gate_waits_for_real_native_flag_before_navigation(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 2, "tile_z": 0, "layer": 0, "required_value": 2,
             "current_flag_value": 0}
        ]
        gate = PhysicalDoor("z009-r006-d000")
        gate.observe(Frame.decode(record))
        self.assertIsNone(gate.blocked_reason)
        self.assertTrue(gate.waiting_for_gate)
        self.assertIsNone(gate.step(Frame.decode(record)))
        self.assertIsNone(gate.navigation_request(Frame.decode(record)))
        record["tick"] = 20
        record["data"]["doors"][0]["current_flag_value"] = 2
        gate.observe(Frame.decode(record))
        self.assertFalse(gate.waiting_for_gate)
        self.assertEqual(gate.contract["current_flag_value"], 2)
        self.assertIsNone(gate.blocked_reason)
        self.assertIsNotNone(gate.step(Frame.decode(record)))

    def test_enemy_death_alone_does_not_clear_a_still_unsatisfied_gate(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 2, "tile_z": 0, "layer": 0, "required_value": 2,
             "current_flag_value": 0}
        ]
        gate = PhysicalDoor("z009-r006-d000")
        gate.observe(Frame.decode(record))
        record["tick"] = 100
        record["data"]["actors"][1]["hp"] = 0
        gate.observe(Frame.decode(record))
        self.assertIsNone(gate.blocked_reason)
        self.assertTrue(gate.waiting_for_gate)
        record["tick"] = 219
        gate.observe(Frame.decode(record))
        self.assertIsNone(gate.blocked_reason)
        record["tick"] = 220
        gate.observe(Frame.decode(record))
        self.assertIn("story flag", gate.blocked_reason)
        self.assertEqual(record["data"]["doors"][0]["current_flag_value"], 0)

    def test_walking_stall_uses_only_two_native_jumps_before_explicit_block(self):
        door = PhysicalDoor("z009-r006-d000")
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 1, "tile_z": 10, "layer": 0, "required_value": 0,
             "current_flag_value": 0}
        ]
        door.observe(Frame.decode(record))
        jumps = []
        for tick in range(1, 650, 8):
            record["tick"] = tick
            action = door.step(Frame.decode(record))
            if action is not None and "Square" in action.buttons:
                jumps.append(action)
        self.assertEqual(len(jumps), 2)
        self.assertTrue(all(action.buttons - {"Square"} for action in jumps))
        self.assertEqual(len(door.axis_attempts), 2)
        self.assertIn("bounded jump and axis recovery", door.blocked_reason)

    def test_continuous_route_requires_every_live_loading_and_quiet_gate(self):
        route = PhysicalRoute(("z009-r006-d000", "z009-r007-d001"))
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        door = {"index": 0, "destination_zone": 9, "destination_room": 7,
                "tile_x": 1, "tile_z": 0, "layer": 0, "required_value": 0,
                "current_flag_value": 0}
        record["data"]["doors"] = [door]
        route.observe(Frame.decode(record))
        route.step(Frame.decode(record))
        record["tick"] = 2
        record["data"]["ready"] = False
        route.observe(Frame.decode(record))
        record["tick"] = 3
        record["data"].update(ready=True, room=7)
        record["data"]["doors"] = [door | {"index": 1, "destination_room": 8}]
        route.observe(Frame.decode(record))
        record["tick"] = 123
        route.observe(Frame.decode(record))
        self.assertEqual(len(route.checkpoints), 1)
        self.assertEqual(route.checkpoints[0]["acceptance_tick"], 123)
        self.assertIsNone(route.accepted_tick)
        self.assertIsNotNone(route.step(Frame.decode(record)))
        record["tick"] = 124
        record["data"]["ready"] = False
        route.observe(Frame.decode(record))
        record["tick"] = 125
        record["data"].update(ready=True, room=8)
        route.observe(Frame.decode(record))
        record["tick"] = 245
        route.observe(Frame.decode(record))
        record["tick"] = 300
        route.observe(Frame.decode(record))
        self.assertEqual(route.accepted_tick, 245)
        self.assertEqual(route.result()["accepted_legs"], 2)
        self.assertEqual(route.checkpoints[0]["acceptance_tick"], 123)
        self.assertIsNone(route.step(Frame.decode(record)))

    def test_disconnected_route_is_rejected_before_an_interaction(self):
        route = PhysicalRoute(("z009-r006-d000", "z009-r008-d000"))
        record = sample_record(active=False)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 1, "tile_z": 0, "layer": 0, "required_value": 0,
             "current_flag_value": 0}
        ]
        with self.assertRaisesRegex(ValueError, "authored destination"):
            route.observe(Frame.decode(record))
        self.assertEqual(route.legs[0].open_ticks, [])

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
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["actors"] = record["data"]["actors"][:1]
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
                "seen_hostiles": 2, "zero_hp_hostiles": 2,
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
                self.assertEqual(store.experience(58, 2)[0]["coverage"], "combat_defeat_and_control")
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
