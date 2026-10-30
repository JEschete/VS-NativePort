import copy
from contextlib import nullcontext
import hashlib
import io
import json
import sys
import tempfile
import threading
import types
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
from vs_player.choices import available_choices, loot_menu_ready, urgent_recovery_needed
from vs_player.doors import PhysicalDoor, PhysicalRoute
from vs_player.menus import MenuTransaction
from vs_player.model import Decision, LocalModel, ModelResponseError, NavigationDecision
from vs_player.navigation import SceneTileGoal, Terrain, TerrainNavigator, field_navigation_ready
from vs_player.observations import Frame
from vs_player.reaction import ReactionController
from vs_player.processes import model_budget
from vs_player.runtime import download, server_environment, validate_archive
from vs_player.runner import (CombatGoal, PlayOptions, SceneReturnHold, native_lab_script, native_outcome,
                              play)
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

    def test_scene_target_matches_external_event_started_by_room_trigger(self):
        record = self.frame()
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 67, source=0)
        event = record["data"]["scene"]["event"]
        event.update({"phase": 4, "source": 2, "id": 67})
        goal.observe(Frame.decode(record))
        self.assertIsNone(goal.activation_tick)
        event["source"] = 0
        goal.observe(Frame.decode(record))
        self.assertEqual(goal.activation_tick, frame.tick)
        with self.assertRaises(ValueError):
            SceneTileGoal((1, 0), frame.zone, frame.room, 67, source=1)

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

    def test_scene_target_walks_directly_into_a_threshold_after_a_near_route(self):
        record = self.frame()
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 0)
        goal.navigator.feedback.append({"status": "route_reached", "tick": frame.tick,
                                        "remaining_distance": 128})
        self.assertIsNone(goal.request(frame))
        self.assertTrue(goal.pushing(frame))
        action = goal.step(frame)
        self.assertIn("trigger tile", action.reason)
        self.assertTrue(action.buttons)
        record["tick"] += 91
        self.assertFalse(goal.pushing(Frame.decode(record)))
        self.assertEqual(goal.push_attempts, 1)

    def test_scene_target_opens_an_authored_door_on_the_trigger_tile(self):
        record = self.frame()
        record["data"]["doors"] = [{"tile_x": 1, "tile_z": 0, "index": 0}]
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 0)
        goal.navigator.feedback.append({"status": "route_reached", "tick": frame.tick})
        self.assertIsNone(goal.request(frame))
        self.assertEqual(goal.push_until, frame.tick + 240)
        self.assertEqual(goal.step(frame).buttons, frozenset({"Cross"}))
        record["tick"] += 10
        self.assertNotEqual(goal.step(Frame.decode(record)).buttons, frozenset({"Cross"}))
        record["tick"] += 90
        self.assertEqual(goal.step(Frame.decode(record)).buttons, frozenset({"Cross"}))
        self.assertEqual(len(goal.door_open_ticks), 2)

    def test_scene_target_keeps_a_route_across_transient_combat_control(self):
        record = self.frame()
        frame = Frame.decode(record)
        goal = SceneTileGoal((1, 0), frame.zone, frame.room, 0)
        context = goal.request(frame)
        record["tick"] += 1
        record["data"]["control_mode"] = 2
        goal.observe(Frame.decode(record))
        record["tick"] += 1
        record["data"]["control_mode"] = 1
        goal.accept(NavigationDecision(next(iter(context["candidates"])), "cross original tile", 1, frame.tick),
                    Frame.decode(record))
        self.assertTrue(goal.navigator.path)

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

    def test_scene_navigation_sheathes_with_bounded_resends(self):
        navigator = SceneTileGoal((1, 0), 9, 6, 0).navigator
        record = self.frame()
        record["data"]["actors"][0]["weapon_drawn"] = 1
        navigator.request(Frame.decode(record), (160, 96))
        navigator.accept(NavigationDecision("route1", "advance", 1, 900), Frame.decode(record))
        navigator.path = [{"position": (160, 0, 96), "jump": False}]
        sent = []
        for tick in range(0, 200, 10):
            record["tick"] = tick
            action = navigator.step(Frame.decode(record)) if tick < 150 else None
            if action is not None:
                self.assertEqual(action.buttons, frozenset({"Cross"}))
                sent.append(tick)
        self.assertEqual(sent, [0, 40, 80, 120])
        record["tick"] = 160
        with self.assertRaises(RuntimeError):
            navigator.step(Frame.decode(record))
        ordinary = TerrainNavigator()
        record = self.frame()
        record["data"]["actors"][0]["weapon_drawn"] = 1
        ordinary.request(Frame.decode(record), (160, 96))
        ordinary.accept(NavigationDecision("route1", "advance", 1, 900), Frame.decode(record))
        ordinary.path = [{"position": (160, 0, 96), "jump": False}]
        self.assertNotEqual(ordinary.step(Frame.decode(record)).buttons, frozenset({"Cross"}))

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

    def test_parallel_players_do_not_share_one_cleanup_pass(self):
        with tempfile.TemporaryDirectory(prefix="vs-player-test-") as directory:
            paths = PlayerPaths(Path(directory) / "state")
            first, second = RunStore(paths), RunStore(paths)
            try:
                with mock.patch("vs_player.store.time.time", return_value=100.0):
                    run = first.create("passed", {})
                    first.finish(run, "passed", {})
                observed = {}
                original = RunStore._cleanup

                def nested(store, retention, now=None):
                    observed["concurrent"] = second.cleanup(retention, now)
                    return original(store, retention, now)

                with mock.patch.object(RunStore, "_cleanup", nested):
                    report = first.cleanup(Retention(successful_days=1), now=100.0 + 86401)
                self.assertEqual(report["deleted"], [run])
                self.assertEqual(observed["concurrent"]["deleted"], [])
                self.assertTrue(observed["concurrent"]["concurrent_cleanup"])
                self.assertEqual(second.cleanup(Retention(successful_days=1), now=10**12)["deleted"], [])
            finally:
                first.close()
                second.close()

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
    def test_clear_results_screen_is_left_with_circle_after_unskipped_credits(self):
        def ending_frame(tick, **ending):
            record = sample_record(tick, active=False)
            state = {"credits_finished": True, "credits_skip_requested": False,
                     "results_frame": 0x200, "results_leaving": False} | ending
            record["data"].update(ready=False, control_mode=0,
                                  scene={"available": False, "in_game": False, "ending": state})
            return Frame.decode(record)

        controller = CombatController()
        self.assertIsNone(controller.step(ending_frame(60, results_frame=0x40)))
        self.assertIsNone(controller.step(ending_frame(60, credits_skip_requested=True)))
        self.assertIsNone(controller.step(ending_frame(60, credits_finished=False)))
        action = controller.step(ending_frame(60))
        self.assertEqual(action.buttons, frozenset({"Circle"}))
        self.assertTrue(input_lease_permitted(ending_frame(61), frozenset({"Circle"})))
        self.assertFalse(input_lease_permitted(ending_frame(61), frozenset({"Start"})))
        self.assertFalse(input_lease_permitted(ending_frame(61, results_leaving=True),
                                               frozenset({"Circle"})))

    def test_accelerated_inputs_last_one_host_frame_with_a_released_frame_between(self):
        controller = CombatController()
        frame = Frame.decode(sample_record(100, active=False))
        self.assertEqual(controller._action(frame, ("Down",), 2, "menu").duration, 2)
        self.assertEqual(controller.input_gap, 6)
        controller.input_ticks = 4
        self.assertEqual(controller._action(frame, ("Down",), 2, "menu").duration, 4)
        self.assertEqual(controller.input_gap, 12)

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

    def test_area_spell_confirms_native_area_then_selects_observed_target(self):
        record = sample_record(tick=1, active=False)
        record["data"].update(control_mode=1, action_phase=0, action=0)
        record["data"]["action_options"] = [
            {"id": 105, "name": "Banish", "family": "warlock", "cost": 25,
             "restrictions": 0, "menu_order": 0}
        ]
        frame = Frame.decode(record)
        decision = Decision("cast", 2, -1, (1, 2, 3), (1, 2, 3),
                            "Use Banish on the observed hostile.", 1, 0, 105)
        controller = CombatController()
        controller.accept(decision, frame, frame)
        controller.transaction.selected = True

        record["tick"] = 10
        record["data"].update(control_mode=2, action_phase=2, action=105,
                              area_center=[256, 0, 64],
                              target_previews=[
                                  {"actor": 2, "limb": -1, "position": [640, 0, 64]},
                              ])
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))

        record["tick"] = 20
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))

        record["tick"] = 30
        record["data"].update(action_phase=3)
        record["data"]["target_page"].update(
            first=0, visible=1, selected=0, selection_step=19,
        )
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))

    def _positioning_area_controller(self):
        record = sample_record(tick=1, active=False)
        record["data"].update(control_mode=1, action_phase=0, action=0)
        record["data"]["action_options"] = [
            {"id": 130, "name": "Meteor", "family": "warlock", "cost": 40,
             "restrictions": 0, "menu_order": 0}
        ]
        frame = Frame.decode(record)
        decision = Decision("cast", 2, -1, (1, 2, 3), (1, 2, 3),
                            "Use Meteor on the observed hostile.", 1, 0, 130)
        controller = CombatController()
        controller.accept(decision, frame, frame)
        controller.transaction.selected = True
        record["data"].update(control_mode=2, action_phase=2, action=130,
                              area_center=[0, 0, 0], area_positioning=True,
                              target_previews=[
                                  {"actor": 0, "limb": 0, "position": [0, 0, 0]},
                                  {"actor": 2, "limb": -1, "position": [640, 0, 0]},
                              ])
        return controller, record

    def test_area_positioning_steers_sphere_over_target_then_confirms(self):
        controller, record = self._positioning_area_controller()
        record["tick"] = 10
        first = controller.step(Frame.decode(record))
        self.assertTrue(first.buttons)
        self.assertTrue(first.buttons <= {"Up", "Down", "Left", "Right"})

        # The native sphere moved toward +x; the controller learns the sphere rotation from it.
        record["tick"] = 20
        record["data"]["area_center"] = [128, 0, 0]
        second = controller.step(Frame.decode(record))
        self.assertEqual(second.buttons, first.buttons)

        # Within half a learned step of the target the next press could only overshoot.
        record["tick"] = 30
        record["data"]["area_center"] = [600, 0, 0]
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))

    def test_area_phase3_without_target_returns_to_positioning_then_gives_up(self):
        controller, record = self._positioning_area_controller()
        record["tick"] = 10
        record["data"]["area_center"] = [630, 0, 0]
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))
        for retry in range(controller.max_area_retries + 1):
            record["tick"] += 10
            record["data"].update(action_phase=3, area_positioning=True,
                                  target_previews=[{"actor": 0, "limb": 0, "position": [0, 0, 0]}])
            record["data"]["target_page"].update(first=0, visible=1, selected=0, selection_step=19)
            action = controller.step(Frame.decode(record))
            self.assertEqual(action.buttons, frozenset({"Cross"}))
            if retry == controller.max_area_retries:
                break
            self.assertFalse(controller.transaction.closing)
            # Back in positioning, the retry takes one forced step past the target, then confirms.
            record["tick"] += 10
            record["data"].update(action_phase=2, target_previews=[
                {"actor": 0, "limb": 0, "position": [0, 0, 0]},
                {"actor": 2, "limb": -1, "position": [640, 0, 0]},
            ])
            step = controller.step(Frame.decode(record))
            self.assertTrue(step.buttons <= {"Up", "Down", "Left", "Right"})
            record["tick"] += 10
            self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))
        self.assertTrue(controller.transaction.closing)
        self.assertEqual(controller.feedback[-1]["status"], "area_out_of_range")

    def test_area_positioning_learns_sphere_rotation_from_observed_movement(self):
        controller, record = self._positioning_area_controller()
        record["tick"] = 10
        first = controller.step(Frame.decode(record))
        # Native moved the sphere away from the target: the next press must differ.
        record["tick"] = 20
        record["data"]["area_center"] = [-128, 0, 0]
        second = controller.step(Frame.decode(record))
        self.assertNotEqual(second.buttons, first.buttons)

    def test_area_positioning_cancels_when_sphere_cannot_reach_target(self):
        controller, record = self._positioning_area_controller()
        action = None
        for attempt in range(10):
            record["tick"] = 10 + attempt * 10
            action = controller.step(Frame.decode(record))
            if action.buttons == frozenset({"Cross"}):
                break
        self.assertEqual(action.buttons, frozenset({"Cross"}))
        self.assertTrue(controller.transaction.closing)
        self.assertEqual(controller.feedback[-1]["status"], "area_out_of_range")

    def test_area_positioning_gives_up_when_range_clamp_slides_sphere_without_progress(self):
        controller, record = self._positioning_area_controller()
        record["data"]["target_previews"][1]["position"] = [0, 0, 2000]
        # The native range volume clamps the sphere near z=480 and slides it sideways.
        centers = [[0, 0, 0], [0, 0, 240], [0, 0, 470]] + [[(-1) ** n * 40, 0, 475] for n in range(8)]
        action = None
        for attempt, center in enumerate(centers):
            record["tick"] = 10 + attempt * 10
            record["data"]["area_center"] = center
            action = controller.step(Frame.decode(record))
            if action.buttons == frozenset({"Cross"}):
                break
        self.assertEqual(action.buttons, frozenset({"Cross"}))
        self.assertLess(attempt, len(centers) - 1)

    def test_area_positioning_confirms_once_native_volume_covers_target(self):
        controller, record = self._positioning_area_controller()
        record["tick"] = 10
        record["data"]["area_center"] = [500, 0, 0]
        record["data"]["area_volumes"] = [
            {"kind": 129, "half_x": 480, "half_z": 480, "angle": 0, "position": [0, 0, 0]},
            {"kind": 1, "half_x": 192, "half_z": 192, "angle": 0, "position": [500, 0, 0]},
        ]
        self.assertEqual(controller.step(Frame.decode(record)).buttons, frozenset({"Circle"}))

    def test_area_positioning_ignores_range_volume_and_needs_area_coverage(self):
        controller, record = self._positioning_area_controller()
        # Only the range volume sits on the untouched cursor; the area volume is not yet placed.
        record["tick"] = 10
        record["data"]["area_center"] = [600, 0, 0]
        record["data"]["area_volumes"] = [
            {"kind": 129, "half_x": 480, "half_z": 480, "angle": 0, "position": [600, 0, 0]},
            {"kind": 1, "half_x": 16, "half_z": 16, "angle": 0, "position": [600, 0, 0]},
        ]
        action = controller.step(Frame.decode(record))
        self.assertTrue(action.buttons <= {"Up", "Down", "Left", "Right"})

    def test_simultaneous_area_gives_up_when_required_slots_exceed_native_volume(self):
        controller, record = self._positioning_area_controller()
        controller.simultaneous_defeat = (2, 3)
        second = copy.deepcopy(record["data"]["actors"][1])
        second.update(slot=3, position=[1240, 0, 0])
        record["data"]["actors"].append(second)
        record["data"]["target_previews"].append({"actor": 3, "limb": -1, "position": [1240, 0, 0]})
        record["data"]["area_volumes"] = [
            {"kind": 129, "half_x": 480, "half_z": 480, "angle": 0, "position": [0, 0, 0]},
            {"kind": 1, "half_x": 64, "half_z": 64, "angle": 0, "position": [0, 0, 0]},
        ]
        record["tick"] = 10
        action = controller.step(Frame.decode(record))
        self.assertEqual(action.buttons, frozenset({"Cross"}))
        self.assertEqual(controller.feedback[-1]["status"], "area_spread")
        self.assertEqual(controller.feedback[-1]["area_radius"], 64.0)
        self.assertEqual(controller.area_radii, {130: 64.0})

    def test_area_positioning_leaves_when_required_non_hostile_slot_is_not_previewed(self):
        controller, record = self._positioning_area_controller()
        controller.simultaneous_defeat = (2, 4)
        dormant = copy.deepcopy(record["data"]["actors"][1])
        dormant.update(slot=4, hostile_to_player=False, relation_to_player=0)
        record["data"]["actors"].append(dormant)
        record["tick"] = 10
        action = controller.step(Frame.decode(record))
        self.assertEqual(action.buttons, frozenset({"Cross"}))
        self.assertEqual(controller.feedback[-1]["kind"], "area_target_unreachable")

    def test_area_spread_feedback_waits_until_required_hostiles_cluster(self):
        record = sample_record(active=False)
        second = copy.deepcopy(record["data"]["actors"][1])
        second.update(slot=3, position=[1240, 0, 0])
        record["data"]["actors"].append(second)
        record["data"]["action_options"] = [
            {"id": 130, "name": "Meteor", "family": "warlock", "cost": 10,
             "restrictions": 0, "menu_order": 0}
        ]
        context = {"simultaneous_defeat_objective": {"slots": [2, 3], "claim": "",
                                                     "area_radii": {"130": 64.0}},
                   "controller_feedback": []}
        self.assertNotIn("cast:2:130:1", available_choices(Frame.decode(record), context))
        record["data"]["actors"][-1]["position"] = list(record["data"]["actors"][1]["position"])
        record["data"]["actors"][-1]["position"][0] += 100
        self.assertIn("cast:2:130:1", available_choices(Frame.decode(record), context))
        # An unmeasured area spell may still be tried while the hostiles are apart.
        context["simultaneous_defeat_objective"]["area_radii"] = {}
        record["data"]["actors"][-1]["position"][0] += 1000
        self.assertIn("cast:2:130:1", available_choices(Frame.decode(record), context))

    def test_combined_area_objective_offers_highest_covering_rank_only(self):
        record = sample_record(active=False)
        second = copy.deepcopy(record["data"]["actors"][1])
        second.update(slot=3)
        second["position"] = list(record["data"]["actors"][1]["position"])
        second["position"][0] += 300
        record["data"]["actors"].append(second)
        record["data"]["action_options"] = [
            {"id": 106 + rank, "name": "Explosion", "family": "warlock", "cost": 10,
             "restrictions": 0, "menu_order": 0, "rank": rank} for rank in range(4)
        ]
        context = {"simultaneous_defeat_objective": {
            "slots": [2, 3], "claim": "", "area_radii": {"109": 128.0}},
            "controller_feedback": []}
        choices = available_choices(Frame.decode(record), context)
        self.assertIn("cast:2:108:1", choices)
        self.assertFalse({"cast:2:106:1", "cast:2:107:1", "cast:2:109:1"} & set(choices))
        # Ordinary single-target choices keep the base rank.
        ordinary = available_choices(Frame.decode(record), {"controller_feedback": []})
        self.assertFalse(any(row["action_id"] in (107, 108, 109) for row in ordinary.values()))

    def test_combined_objective_protects_required_slot_that_is_not_yet_hostile(self):
        record = sample_record(active=False)
        dormant = copy.deepcopy(record["data"]["actors"][1])
        dormant.update(slot=4, hostile_to_player=False, relation_to_player=0)
        record["data"]["actors"].append(dormant)
        record["data"]["action_options"] = [
            {"id": 118, "name": "GaeaStrike", "family": "warlock", "cost": 10,
             "restrictions": 0, "menu_order": 0, "rank": 0}
        ]
        context = {"simultaneous_defeat_objective": {"slots": [2, 4], "claim": "", "area_radii": {}},
                   "controller_feedback": []}
        choices = available_choices(Frame.decode(record), context)
        self.assertFalse(any(row["target_slot"] == 2 and row["intent"] in ("attack", "cast")
                             for row in choices.values()))
        self.assertIn("wait:-1:-1:1", choices)

    def test_combined_objective_targets_other_hostile_past_protected_caster(self):
        record = sample_record(active=False)
        record["data"]["actors"][1]["spell_action_ids"] = [111]
        other = copy.deepcopy(record["data"]["actors"][1])
        other.update(slot=3, spell_action_ids=[])
        other["position"] = [value + 600 for value in other["position"]]
        dormant = copy.deepcopy(record["data"]["actors"][1])
        dormant.update(slot=4, hostile_to_player=False, relation_to_player=0)
        record["data"]["actors"] += [other, dormant]
        context = {"simultaneous_defeat_objective": {"slots": [2, 4], "claim": "", "area_radii": {}},
                   "controller_feedback": []}
        choices = available_choices(Frame.decode(record), context)
        self.assertIn("attack:3:-1:1", choices)
        self.assertFalse(any(row["target_slot"] == 2 and row["intent"] in ("attack", "cast")
                             for row in choices.values()))

    def test_combined_objective_never_fires_through_a_protected_hostile(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": 102, "name": "Wind", "family": "sorcerer", "cost": 5,
             "restrictions": 0, "menu_order": 0}
        ]
        player, target = record["data"]["actors"][0], record["data"]["actors"][1]
        target["position"] = [player["position"][0] + 600, player["position"][1], player["position"][2]]
        blocker = copy.deepcopy(target)
        blocker.update(slot=3, spell_action_ids=[])
        blocker["position"] = [player["position"][0] + 300, player["position"][1], player["position"][2]]
        dormant = copy.deepcopy(target)
        dormant.update(slot=4, hostile_to_player=False, relation_to_player=0)
        record["data"]["actors"] += [blocker, dormant]
        context = {"simultaneous_defeat_objective": {"slots": [3, 4], "claim": "", "area_radii": {}},
                   "controller_feedback": []}
        choices = available_choices(Frame.decode(record), context)
        self.assertFalse(any(row["intent"] == "cast" and row["target_slot"] in (2, 3)
                             for row in choices.values()))
        self.assertIn("approach:2:-1:1", choices)
        blocker["position"][2] += 400
        choices = available_choices(Frame.decode(record), context)
        self.assertIn("cast:2:102:1", choices)

    def test_area_out_of_range_feedback_suppresses_cast_only_near_failed_position(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": 130, "name": "Meteor", "family": "warlock", "cost": 10,
             "restrictions": 0, "menu_order": 0}
        ]
        frame = Frame.decode(record)
        feedback = [{"status": "area_out_of_range", "action_id": 130, "target": 2,
                     "player_position": frame.player.position, "tick": 1}]
        choices = available_choices(frame, {"controller_feedback": feedback})
        self.assertNotIn("cast:2:130:1", choices)
        far = tuple(value + 512 for value in frame.player.position)
        feedback[0]["player_position"] = far
        self.assertIn("cast:2:130:1", available_choices(frame, {"controller_feedback": feedback}))

    def test_unlisted_spell_target_offers_approach_instead_of_other_spells_here(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": action, "name": name, "family": "sorcerer", "cost": 5,
             "restrictions": 0, "menu_order": index}
            for index, (action, name) in enumerate(((100, "Fire"), (102, "Wind")))
        ]
        frame = Frame.decode(record)
        feedback = [{"kind": "target_not_listed", "intent": "cast", "target": 2,
                     "player_position": frame.player.position, "tick": 1}]
        choices = available_choices(frame, {"controller_feedback": feedback})
        self.assertFalse(any(row["intent"] == "cast" and row["target_slot"] == 2
                             for row in choices.values()))
        self.assertIn("approach:2:-1:1", choices)
        feedback[0]["intent"] = "attack"
        self.assertIn("cast:2:102:1", available_choices(frame, {"controller_feedback": feedback}))
        feedback[0].update(intent="cast", player_position=tuple(value + 512 for value in frame.player.position))
        self.assertIn("cast:2:102:1", available_choices(frame, {"controller_feedback": feedback}))

    def test_combined_objective_approaches_after_area_preview_was_out_of_range_here(self):
        record = sample_record(active=False)
        second = copy.deepcopy(record["data"]["actors"][1])
        second.update(slot=3)
        record["data"]["actors"].append(second)
        record["data"]["action_options"] = [
            {"id": 130, "name": "Meteor", "family": "warlock", "cost": 10,
             "restrictions": 0, "menu_order": 0}
        ]
        frame = Frame.decode(record)
        feedback = [{"status": "area_out_of_range", "action_id": 130, "target": slot,
                     "player_position": frame.player.position, "tick": 1} for slot in (2, 3)]
        context = {"simultaneous_defeat_objective": {"slots": [2, 3], "claim": "", "area_radii": {}},
                   "controller_feedback": feedback}
        choices = available_choices(frame, context)
        self.assertFalse(any(row["intent"] == "cast" for row in choices.values()))
        self.assertIn("approach:2:-1:1", choices)
        for row in feedback:
            row["player_position"] = tuple(value + 512 for value in frame.player.position)
        self.assertIn("cast:2:130:1", available_choices(frame, context))

    def test_combined_objective_out_of_range_from_here_suppresses_every_area_spell(self):
        record = sample_record(active=False)
        second = copy.deepcopy(record["data"]["actors"][1])
        second.update(slot=3)
        record["data"]["actors"].append(second)
        record["data"]["action_options"] = [
            {"id": action, "name": name, "family": "warlock", "cost": 10,
             "restrictions": 0, "menu_order": index}
            for index, (action, name) in enumerate(((113, "Thunderburst"), (130, "Meteor")))
        ]
        frame = Frame.decode(record)
        feedback = [{"status": "area_out_of_range", "action_id": 130, "target": 3,
                     "player_position": frame.player.position, "tick": 1}]
        context = {"simultaneous_defeat_objective": {"slots": [2, 3], "claim": "", "area_radii": {}},
                   "controller_feedback": feedback}
        choices = available_choices(frame, context)
        self.assertFalse(any(row["intent"] == "cast" for row in choices.values()))
        self.assertIn("approach:2:-1:1", choices)
        self.assertIn("approach:3:-1:1", choices)

    def test_simultaneous_defeat_cancels_an_area_that_cannot_defeat_every_required_slot(self):
        record = sample_record(tick=1, active=False)
        second = copy.deepcopy(record["data"]["actors"][1])
        second.update(slot=3, hp=30, max_hp=30, position=[192, 0, 64])
        record["data"]["actors"].append(second)
        record["data"].update(control_mode=1, action_phase=0, action=0)
        record["data"]["action_options"] = [
            {"id": 106, "name": "Explosion", "family": "warlock", "cost": 30,
             "restrictions": 0, "menu_order": 0}
        ]
        frame = Frame.decode(record)
        decision = Decision("cast", 2, -1, (1, 2, 3), (1, 2, 3),
                            "Defeat both hostiles at once.", 1, 0, 106)
        for forecasts, previews, expected in (
            ([{"actor": 2, "limb": -1, "effect_id": 58, "amount": 50},
              {"actor": 3, "limb": -1, "effect_id": 58, "amount": 50}], (2, 3), None),
            ([{"actor": 2, "limb": -1, "effect_id": 58, "amount": 50}], (2,), [3]),
            ([{"actor": 2, "limb": -1, "effect_id": 58, "amount": 50},
              {"actor": 3, "limb": -1, "effect_id": 58, "amount": 10}], (2, 3), [3]),
        ):
            controller = CombatController()
            controller.simultaneous_defeat = (2, 3)
            controller.accept(decision, frame, frame)
            controller.transaction.selected = True
            record["tick"] = 30
            record["data"].update(control_mode=2, action_phase=3, action=106,
                                  target_forecasts=forecasts,
                                  target_previews=[{"actor": slot, "limb": -1,
                                                    "position": [128, 0, 64]}
                                                   for slot in previews])
            record["data"]["target_page"].update(first=0, visible=len(previews), selected=0,
                                                 selection_step=19)
            action = controller.step(Frame.decode(record))
            if expected is None:
                self.assertEqual(action.buttons, frozenset({"Circle"}))
                self.assertFalse(controller.transaction.closing)
            else:
                self.assertEqual(action.buttons, frozenset({"Cross"}))
                self.assertEqual(controller.feedback[-1]["missing"], expected)
                record["tick"] = 40
                self.assertEqual(controller.step(Frame.decode(record)).buttons,
                                 frozenset({"Cross"}))

    def test_simultaneous_defeat_objective_only_offers_area_spells(self):
        record = sample_record(tick=500, active=False)
        record["data"]["actors"][0]["mp"] = 40
        second = copy.deepcopy(record["data"]["actors"][1])
        second.update(slot=3, position=[192, 0, 64])
        record["data"]["actors"].append(second)
        record["data"]["action_options"] = [
            {"id": 106, "name": "Explosion", "family": "warlock", "cost": 30,
             "restrictions": 0, "menu_order": 0},
            {"id": 97, "name": "Solid Shock", "family": "warlock", "cost": 5,
             "restrictions": 0, "menu_order": 1},
        ]
        context = {"simultaneous_defeat_objective": {"slots": [2, 3]}}
        third = copy.deepcopy(second)
        third.update(slot=4, position=[256, 0, 64])
        record["data"]["actors"].append(third)
        rows = available_choices(Frame.decode(record), context).values()
        self.assertTrue(rows)
        self.assertTrue(all(row["target_slot"] in (-1, 4) and row["action_id"] != 106 for row in rows))
        record["data"]["actors"].pop()
        choices = available_choices(Frame.decode(record), context)
        self.assertEqual({(row["intent"], row["target_slot"], row["action_id"])
                          for row in choices.values()},
                         {("cast", 2, 106), ("cast", 3, 106)})
        context["controller_feedback"] = [{"kind": "area_incomplete", "tick": 400}]
        intents = {row["intent"] for row in available_choices(Frame.decode(record), context).values()}
        self.assertEqual(intents, {"wait", "approach"})
        record["data"]["actors"].pop()
        choices = available_choices(Frame.decode(record), context)
        self.assertIn(("cast", 2, 97), {(row["intent"], row["target_slot"], row["action_id"])
                                        for row in choices.values()})

    def test_defeat_order_objective_harms_only_the_next_listed_slot(self):
        record = sample_record(tick=500, active=False)
        record["data"]["actors"][0]["mp"] = 40
        second = copy.deepcopy(record["data"]["actors"][1])
        second.update(slot=3, position=[192, 0, 64])
        record["data"]["actors"].append(second)
        record["data"]["action_options"] = [
            {"id": 106, "name": "Explosion", "family": "warlock", "cost": 30,
             "restrictions": 0, "menu_order": 0},
            {"id": 97, "name": "Solid Shock", "family": "warlock", "cost": 5,
             "restrictions": 0, "menu_order": 1},
        ]
        context = {"defeat_order_objective": {"slots": [3, 2]}}
        rows = list(available_choices(Frame.decode(record), context).values())
        self.assertTrue(any(row["target_slot"] == 3 for row in rows))
        self.assertTrue(all(row["target_slot"] in (-1, 0, 3) and row["action_id"] != 106
                            for row in rows))
        record["data"]["actors"].pop()
        rows = list(available_choices(Frame.decode(record), context).values())
        self.assertTrue(any(row["target_slot"] == 2 for row in rows))

    def test_scene_return_hold_waits_for_expected_events_then_sheathes(self):
        hold = SceneReturnHold({"events": [0], "event_source": 2, "return_zone": 9, "return_room": 7})
        record = sample_record(tick=100)
        record["data"]["room"] = 7
        record["data"]["actors"][1]["position"] = [1024, 0, 1024]
        self.assertFalse(hold.holding(Frame.decode(record)))
        record["data"]["scene"]["event"].update(phase=4, source=1, id=0)
        hold.observe(Frame.decode(record))
        self.assertEqual(hold.matched, 0)
        record["data"]["scene"]["event"].update(phase=4, source=2, id=0)
        hold.observe(Frame.decode(record))
        record["data"]["scene"]["event"]["phase"] = 0
        frame = Frame.decode(record)
        hold.observe(frame)
        self.assertTrue(hold.holding(frame))
        self.assertEqual(hold.step(frame).buttons, frozenset({"Cross"}))
        record["tick"] = record["sequence"] = 110
        self.assertIsNone(hold.step(Frame.decode(record)))
        record["data"]["actors"][0]["weapon_drawn"] = 0
        record["tick"] = record["sequence"] = 200
        self.assertIsNone(hold.step(Frame.decode(record)))
        # A hostile in melee range (or a reaction aimed at Ashley) hands control back to combat.
        record["data"]["actors"][1]["position"] = [300, 0, 64]
        self.assertFalse(hold.holding(Frame.decode(record)))
        record["data"]["actors"][1]["position"] = [300, -600, 64]
        self.assertTrue(hold.holding(Frame.decode(record)))
        record["data"]["reaction_timing"].update(source_actor=2, target_actor=0)
        self.assertFalse(hold.holding(Frame.decode(record)))
        # Once attacked in the return room, a hostile drifting away does not restore the hold.
        record["data"]["reaction_timing"].update(source_actor=0, target_actor=2)
        self.assertFalse(hold.holding(Frame.decode(record)))
        defeated = Frame.decode(record)
        self.assertTrue(defeated.enemies)
        record["data"]["actors"][1]["hp"] = 0
        defeated = Frame.decode(record)
        self.assertFalse(defeated.enemies)
        self.assertTrue(hold.holding(defeated))
        record["data"]["room"] = 6
        self.assertFalse(hold.holding(Frame.decode(record)))
        ending = SceneReturnHold({"events": [0], "ending_to_title": True})
        ending.matched = 1
        self.assertFalse(ending.holding(frame))
        # Fixtures and the Lab decoder declare title-return goals through "terminal".
        for terminal in ("ending_to_title", "attract_to_title", "game_over_to_title"):
            declared = SceneReturnHold({"events": [0], "event_source": 2, "terminal": terminal})
            self.assertFalse(declared.enabled)

    def test_closing_a_window_the_game_already_destroyed_is_not_an_ownership_change(self):
        from vs_player.windows import WindowInput
        driver = WindowInput.__new__(WindowInput)
        driver.process_id = 42
        driver.window = 7
        driver.verify_process = lambda: None
        driver.user = mock.Mock()
        driver.user.IsWindow.return_value = False
        driver.close_window()
        self.assertIsNone(driver.window)
        driver.user.PostMessageW.assert_not_called()
        driver.window = 7
        driver.user.IsWindow.return_value = True
        driver.user.GetWindowThreadProcessId.side_effect = lambda window, owner: None
        with self.assertRaisesRegex(RuntimeError, "changed ownership"):
            driver.close_window()

    def test_stat_override_scenarios_require_the_explicit_assist_flag(self):
        from vs_player import runner
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scenario = root / "scenario.json"
            scenario.write_text(json.dumps({
                "ashley_profile": "all-abilities", "room_events": "production",
                "enemies": [], "ashley_stats": {"max_hp": 999},
            }), encoding="utf-8")
            store = mock.Mock()
            store.directory.return_value = root / "run"
            for allowed, error in ((False, "do not allow stat overrides"),
                                   (True, "")):
                options = PlayOptions(root / "VS.CombatLab.exe", root / "missing-data",
                                      scenario, allow_stat_overrides=allowed)
                with mock.patch.object(runner, "freeze_game", return_value={}):
                    if allowed:
                        with self.assertRaises(FileNotFoundError):
                            runner._play_game(store, "0" * 32, options, None, None, None, None)
                    else:
                        with self.assertRaisesRegex(ValueError, error):
                            runner._play_game(store, "0" * 32, options, None, None, None, None)

    def test_story_progress_scenarios_require_the_explicit_assist_flag(self):
        from vs_player import runner
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scenario = root / "scenario.json"
            scenario.write_text(json.dumps({
                "ashley_profile": "all-abilities", "room_events": "production",
                "enemies": [], "story_progress": 0,
            }), encoding="utf-8")
            store = mock.Mock()
            store.directory.return_value = root / "run"
            for allowed in (False, True):
                options = PlayOptions(root / "VS.CombatLab.exe", root / "missing-data",
                                      scenario, allow_story_progress=allowed)
                with mock.patch.object(runner, "freeze_game", return_value={}):
                    if allowed:
                        with self.assertRaises(FileNotFoundError):
                            runner._play_game(store, "0" * 32, options, None, None, None, None)
                    else:
                        with self.assertRaisesRegex(ValueError, "owner-approved assist"):
                            runner._play_game(store, "0" * 32, options, None, None, None, None)

    def test_area_spell_confirmation_retries_are_bounded(self):
        record = sample_record(tick=1, active=False)
        record["data"].update(control_mode=1, action_phase=0, action=0)
        record["data"]["action_options"] = [
            {"id": 105, "name": "Banish", "family": "warlock", "cost": 25,
             "restrictions": 0, "menu_order": 0}
        ]
        frame = Frame.decode(record)
        decision = Decision("cast", 2, -1, (1, 2, 3), (1, 2, 3),
                            "Use Banish on the observed hostile.", 1, 0, 105)
        controller = CombatController()
        controller.accept(decision, frame, frame)
        controller.transaction.selected = True
        record["data"].update(control_mode=2, action_phase=2, action=105,
                              area_center=[256, 0, 64])
        for attempt in range(controller.max_area_target_confirmations):
            record["tick"] = 10 + attempt * 8
            action = controller.step(Frame.decode(record))
            self.assertEqual(action.buttons, frozenset({"Circle"}))

        record["tick"] += 61
        with self.assertRaisesRegex(TimeoutError, "did not leave phase 2"):
            controller.step(Frame.decode(record))

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
        record["data"]["actors"][0].update(hp=165, max_hp=250)
        frame = Frame.decode(record)
        self.assertNotIn("item", {row["intent"] for row in available_choices(frame, {}).values()})
        choices = available_choices(frame, {"max_incoming_hp_damage": 106})
        self.assertEqual({row["intent"] for row in choices.values()},
                         {"item", "attack", "approach", "retreat"})

    def test_recovery_that_cannot_restore_the_two_hit_margin_is_not_offered(self):
        record = self.item_record()
        record["data"]["actors"][0].update(hp=166, max_hp=250)
        choices = available_choices(Frame.decode(record), {"max_incoming_hp_damage": 121})
        self.assertNotIn("item", {row["intent"] for row in choices.values()})
        self.assertIn("attack", {row["intent"] for row in choices.values()})

    def test_dullahan_threat_margin_keeps_healing_and_counterattack_selectable(self):
        record = self.item_record()
        record["data"]["actors"][0].update(hp=162, max_hp=250)
        record["data"]["actors"][1].update(hp=117, max_hp=186)
        choices = available_choices(Frame.decode(record), {"max_incoming_hp_damage": 88})
        self.assertIn("item:0:224:1:323", choices)
        self.assertIn("attack:2:-1:1", choices)
        self.assertIn("retreat:2:-1:1", choices)

    def test_caster_at_observed_two_hit_margin_makes_recovery_urgent(self):
        record = self.item_record()
        record["data"]["actors"][0].update(hp=158, max_hp=250)
        record["data"]["actors"][1]["spell_action_ids"] = [130]
        frame = Frame.decode(record)
        context = {"max_incoming_hp_damage": 92}
        self.assertTrue(urgent_recovery_needed(frame, context))
        choices = available_choices(frame, context)
        self.assertEqual({row["intent"] for row in choices.values()}, {"item"})
        self.assertIn("item:0:224:1:323", choices)

    def test_one_survivable_observed_hit_retains_urgent_recovery_priority(self):
        record = self.item_record()
        record["data"]["actors"][0].update(hp=155, max_hp=250)
        choices = available_choices(Frame.decode(record), {"max_incoming_hp_damage": 160})
        self.assertEqual({row["intent"] for row in choices.values()}, {"item"})
        record["data"]["actors"][0].update(hp=200, max_hp=250)
        choices = available_choices(Frame.decode(record), {"max_incoming_hp_damage": 300})
        self.assertNotIn("item:0:224:1:323", choices)
        self.assertIn("retreat:2:-1:1", choices)

    def test_urgent_recovery_is_detected_before_the_normal_planning_interval(self):
        record = self.item_record()
        record["data"]["actors"][0].update(hp=94, max_hp=250)
        frame = Frame.decode(record)
        self.assertTrue(urgent_recovery_needed(frame, {}))
        self.assertEqual({row["intent"] for row in available_choices(frame, {}).values()},
                         {"item"})
        record["data"]["actors"][0].update(hp=160, max_hp=250)
        self.assertFalse(urgent_recovery_needed(Frame.decode(record), {}))

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

    def test_pending_scene_tile_outranks_chasing_hostiles_unless_combat_gated(self):
        frame = self.frame()
        scene = {"zone": 1, "room": 6, "source": 2, "selector": 0, "tile": (3, 4), "activation_tick": None}
        def intents(context):
            return {row["intent"] for row in available_choices(frame, context).values()}

        pending = intents({"source_scene_navigation_objective": {**scene, "combat_clearance_required": False}})
        self.assertIn("navigate", pending)
        self.assertNotIn("attack", pending)
        gated = intents({"source_scene_navigation_objective": {**scene, "combat_clearance_required": True}})
        self.assertNotIn("navigate", gated)
        self.assertIn("attack", gated)
        activated = intents({"source_scene_navigation_objective": {
            **scene, "activation_tick": 900, "combat_clearance_required": False}})
        self.assertNotIn("navigate", activated)

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

    def test_finished_rejected_spell_transaction_does_not_cancel_later_attack_targeting(self):
        controller = CombatController()
        controller.transaction = types.SimpleNamespace(done=True, closing=True, selected=True)
        original = self.frame()
        controller.accept(self.decision(), original, original)
        self.assertIsNone(controller.transaction)
        frame = self.frame(
            10, control_mode=2, action_phase=1,
            target_page={"first": 0, "visible": 1, "selected": 0, "selection_step": 19},
            target_previews=[{"actor": 2, "limb": 0}],
        )
        self.assertEqual(controller.step(frame).buttons, frozenset({"Circle"}))

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

    def test_attack_reposition_uses_only_current_native_refusal_and_unmoved_position(self):
        controller = CombatController()
        original = self.frame()
        controller.accept(self.decision(), original, original)
        self.assertFalse(controller.attack_reposition_required(original))
        refused = self.frame(10, control_mode=2, action_phase=1,
                             target_page={"selection_step": 16})
        controller.step(refused)
        retry = self.frame(20)
        controller.accept(self.decision(20), retry, retry)
        self.assertTrue(controller.attack_reposition_required(retry))
        self.assertEqual(controller.step(retry).reason,
                         "move using current native positions and camera")
        for field in ("episode", "zone", "room"):
            record = sample_record(30, active=False)
            record["data"].update(control_mode=1, action_phase=0)
            if field == "episode":
                record["script"] = 1
            else:
                record["data"][field] += 1
            with self.subTest(field=field):
                self.assertFalse(controller.attack_reposition_required(Frame.decode(record)))
        for field in ("model", "spawn_index", "slot"):
            record = sample_record(30, active=False)
            record["data"].update(control_mode=1, action_phase=0)
            record["data"]["actors"][1][field] += 1
            with self.subTest(field=field):
                self.assertFalse(controller.attack_reposition_required(Frame.decode(record)))
        for distance in (15, 16):
            record = sample_record(30, active=False)
            record["data"].update(control_mode=1, action_phase=0)
            record["data"]["actors"][0]["position"][1] = -distance
            with self.subTest(distance=distance):
                self.assertEqual(controller.attack_reposition_required(Frame.decode(record)),
                                 distance < 16)
        controller.accept(self.decision(20, intent="approach"), retry, retry)
        self.assertFalse(controller.attack_reposition_required(retry))

    def test_repeated_native_refusals_mark_hostile_unreachable_for_choices(self):
        controller = CombatController()
        tick = 0
        for _ in range(CombatController.unreachable_rejections):
            start = self.frame(tick)
            controller.accept(self.decision(tick), start, start)
            refused = self.frame(tick + 5, control_mode=2, action_phase=1,
                                 target_page={"selection_step": 16})
            controller.step(refused)
            tick += 20
        current = self.frame(tick)
        verdicts = controller.unreachable_hostiles(current)
        self.assertEqual([row["slot"] for row in verdicts], [current.enemies[0].slot])
        intents = {(row["intent"], row["target_slot"])
                   for row in available_choices(current, {"unreachable_hostiles": verdicts}).values()}
        self.assertNotIn(("attack", current.enemies[0].slot), intents)
        self.assertNotIn(("approach", current.enemies[0].slot), intents)
        self.assertEqual(controller.unreachable_hostiles(
            self.frame(tick + CombatController.unreachable_window + 10)), [])

    def test_exhausted_terrain_route_marks_hostile_unreachable_without_ending_the_run(self):
        from vs_player.navigation import NO_OUTGOING_EDGE, TERRAIN_EXHAUSTED
        self.assertIn(NO_OUTGOING_EDGE, TERRAIN_EXHAUSTED)
        controller = CombatController()
        start = self.frame(0)
        controller.accept(self.decision(0), start, start)
        slot = start.enemies[0].slot
        self.assertFalse(controller.mark_terrain_unreachable(start, slot + 9))
        self.assertTrue(controller.mark_terrain_unreachable(start, slot))
        self.assertIsNone(controller.decision)
        self.assertEqual(controller.feedback[-1]["kind"], "terrain_unreachable")
        current = self.frame(30)
        verdicts = controller.unreachable_hostiles(current)
        self.assertEqual([row["slot"] for row in verdicts], [slot])
        intents = {(row["intent"], row["target_slot"])
                   for row in available_choices(current, {"unreachable_hostiles": verdicts}).values()}
        self.assertNotIn(("attack", slot), intents)
        self.assertNotIn(("approach", slot), intents)
        self.assertEqual(controller.unreachable_hostiles(
            self.frame(CombatController.unreachable_window + 10)), [])

    def test_unlisted_attack_target_also_requires_reposition(self):
        controller = CombatController()
        original = self.frame()
        controller.accept(self.decision(), original, original)
        refused = self.frame(10, control_mode=2, action_phase=1,
                             target_page={"selection_step": 19},
                             target_previews=[{"actor": 3, "limb": 0}])
        self.assertEqual(controller.step(refused).buttons, frozenset({"Cross"}))
        self.assertEqual(controller.feedback[-1]["kind"], "target_not_listed")
        retry = self.frame(20)
        controller.accept(self.decision(20), retry, retry)
        self.assertTrue(controller.attack_reposition_required(retry))

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

    def test_hp_damage_spell_choices_require_verified_effect_affordable_mp_and_no_failure(self):
        record = sample_record(active=False)
        record["data"]["actors"][0]["mp"] = 50
        record["data"]["action_options"] = [
            {"id": action, "family": "warlock", "cost": cost, "restrictions": 0}
            for action, cost in ((97, 25), (104, 22), (105, 25), (106, 36), (130, 38))
        ]

        choices = available_choices(Frame.decode(record), {})
        casts = {(row["target_slot"], row["action_id"]) for row in choices.values()
                 if row["intent"] == "cast"}
        self.assertEqual(casts, {(2, 97), (2, 105), (2, 106), (2, 130)})

        record["data"]["actors"][0]["mp"] = 35
        choices = available_choices(Frame.decode(record), {})
        casts = {(row["target_slot"], row["action_id"]) for row in choices.values()
                 if row["intent"] == "cast"}
        self.assertEqual(casts, {(2, 97), (2, 105)})

        record["data"]["actors"][0]["mp"] = 50
        choices = available_choices(Frame.decode(record), {
            "controller_feedback": [
                {"action_id": 97, "target": 2, "status": "no_effect"}
            ]
        })
        casts = {(row["target_slot"], row["action_id"]) for row in choices.values()
                 if row["intent"] == "cast"}
        self.assertEqual(casts, {(2, 105), (2, 106), (2, 130)})

    def test_banish_choice_respects_status_eligibility_and_mp(self):
        record = sample_record(active=False)
        record["data"]["actors"][0]["mp"] = 50
        record["data"]["action_options"] = [
            {"id": 105, "family": "warlock", "cost": 25, "restrictions": 0}
        ]
        choices = available_choices(Frame.decode(record), {})
        self.assertIn((2, 105), {(row["target_slot"], row["action_id"])
                                 for row in choices.values() if row["intent"] == "cast"})

        blocked = copy.deepcopy(record)
        blocked["data"]["actors"][1]["status_immunities"] = blocked["data"]["status_masks"][29]
        choices = available_choices(Frame.decode(blocked), {})
        self.assertNotIn((2, 105), {(row["target_slot"], row["action_id"])
                                   for row in choices.values() if row["intent"] == "cast"})

        record["data"]["actors"][0]["mp"] = 24
        choices = available_choices(Frame.decode(record), {})
        self.assertNotIn((2, 105), {(row["target_slot"], row["action_id"])
                                   for row in choices.values() if row["intent"] == "cast"})

        record["data"]["actors"][0]["mp"] = 50
        choices = available_choices(Frame.decode(record), {
            "controller_feedback": [
                {"action_id": 105, "target": 2, "status": "no_effect"}
            ]
        })
        self.assertNotIn((2, 105), {(row["target_slot"], row["action_id"])
                                   for row in choices.values() if row["intent"] == "cast"})

    def test_banish_is_prioritized_against_a_dangerous_caster_then_falls_back(self):
        record = sample_record(active=False)
        record["data"]["actors"][0]["mp"] = 50
        record["data"]["actors"][1]["spell_action_ids"] = [130]
        record["data"]["action_options"] = [
            {"id": 97, "family": "warlock", "cost": 25, "restrictions": 0},
            {"id": 105, "family": "warlock", "cost": 25, "restrictions": 0},
        ]

        choices = available_choices(Frame.decode(record), {})
        self.assertEqual(
            {(row["target_slot"], row["action_id"]) for row in choices.values()
             if row["intent"] == "cast"},
            {(2, 105)},
        )

        choices = available_choices(Frame.decode(record), {
            "controller_feedback": [
                {"action_id": 105, "target": 2, "status": "no_effect"}
            ]
        })
        self.assertEqual(
            {(row["target_slot"], row["action_id"]) for row in choices.values()
             if row["intent"] == "cast"},
            {(2, 97)},
        )

    def test_combat_choices_prioritize_damage_spell_caster_over_nearer_caster(self):
        record = sample_record(active=False)
        nearer = record["data"]["actors"][1]
        nearer.update(position=[128, 0, 64], spell_action_ids=[75])
        priority = copy.deepcopy(nearer)
        priority.update(slot=4, model=33, position=[640, 0, 64], spell_action_ids=[130])
        record["data"]["actors"].append(priority)
        record["data"]["action_options"] = [
            {"id": 75, "family": "sorcerer", "cost": 7, "restrictions": 0}
        ]

        choices = available_choices(Frame.decode(record), {})
        self.assertEqual({row["target_slot"] for row in choices.values()}, {4})

        record["data"]["action_options"] = []
        record["data"].update(control_mode=2, action_phase=1)
        record["data"]["target_previews"] = [
            {"actor": 2, "limb": 0, "position": [640, -100, 64]},
            {"actor": 4, "limb": 0, "position": [128, -100, 64]},
        ]
        record["data"]["target_page"].update(first=0, visible=2, selected=0, selection_step=19)
        choices = available_choices(Frame.decode(record), {})
        self.assertEqual({row["target_slot"] for row in choices.values()
                          if row["intent"] == "attack"}, {4})

    def test_zero_damage_forecast_does_not_remove_native_target_choices(self):
        record = sample_record(active=False)
        alternate = copy.deepcopy(record["data"]["actors"][1])
        alternate.update(slot=3, position=[256, 0, 64])
        record["data"]["actors"].append(alternate)
        record["data"].update(control_mode=2, action_phase=1)
        record["data"]["target_previews"] = [
            {"actor": 2, "limb": 0, "position": [128, -100, 64]},
            {"actor": 3, "limb": 0, "position": [256, -100, 64]},
        ]
        record["data"]["target_page"].update(first=0, visible=2, selected=0, selection_step=19)
        record["data"]["target_forecasts"] = [
            {"actor": 2, "limb": 0, "effect_id": 58, "amount": 0},
            {"actor": 3, "limb": 0, "effect_id": 58, "amount": 15},
        ]

        choices = available_choices(Frame.decode(record), {})
        attacks = [choice for choice in choices.values() if choice["intent"] == "attack"]
        self.assertEqual({choice["target_slot"] for choice in attacks}, {2, 3})

        record["data"]["target_previews"] = [
            {"actor": 2, "limb": 0, "position": [128, -100, 64]},
        ]
        record["data"]["target_forecasts"] = [
            {"actor": 2, "limb": 0, "effect_id": 58, "amount": 0},
        ]
        choices = available_choices(Frame.decode(record), {})
        attacks = [choice for choice in choices.values() if choice["intent"] == "attack"]
        self.assertEqual({choice["target_slot"] for choice in attacks}, {2})

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

    def test_defense_preparation_covers_physical_threat_even_when_spell_defense_is_mapped(self):
        record = sample_record(active=False, defense_actions=[40, 41, 42])
        record["data"]["actors"][1]["spell_action_ids"] = [130]
        record["data"]["action_options"] = [
            {"id": action, "name": str(action), "family": "defense", "cost": 2,
             "restrictions": 0, "menu_order": action-40}
            for action in (42, 43, 44, 45)
        ]

        choices = available_choices(Frame.decode(record), {})
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

    def test_levelled_warlock_spell_selects_rank_one_then_confirms(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": 130, "family": "warlock", "restrictions": 0, "menu_order": 15, "rank_count": 4}
        ]
        frame = Frame.decode(record)
        decision = Decision("cast", 2, -1, (1, 2, 3), (1, 2, 3), "meteor", 1, 1, 130, 1)
        transaction = MenuTransaction(decision, frame)
        transaction.selected = True
        record["data"]["control_mode"] = 5
        record["data"]["menu"].update(available=True, selected_menu=1, submenu_phase=7,
                                      list_phase=17, rows=[], spell_level=2,
                                      spell_level_count=4)
        self.assertEqual(transaction.step(Frame.decode(record)).button, "Left")
        record["data"]["menu"]["spell_level"] = 0
        self.assertEqual(transaction.step(Frame.decode(record)).button, "Circle")
        record["data"]["menu"]["spell_level"] = -1
        self.assertIsNone(transaction.step(Frame.decode(record)))

    def test_levelled_warlock_spell_raises_rank_to_requested_variant(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": 106, "family": "warlock", "restrictions": 0, "menu_order": 4, "rank": 0,
             "rank_count": 4},
            {"id": 109, "family": "warlock", "restrictions": 0, "menu_order": 4, "rank": 3,
             "rank_count": 4},
        ]
        frame = Frame.decode(record)
        decision = Decision("cast", 2, -1, (1, 2, 3), (1, 2, 3), "explosion", 1, 1, 109, 1)
        transaction = MenuTransaction(decision, frame)
        transaction.selected = True
        record["data"]["control_mode"] = 5
        record["data"]["menu"].update(available=True, selected_menu=1, submenu_phase=7,
                                      list_phase=17, rows=[], spell_level=1,
                                      spell_level_count=4)
        self.assertEqual(transaction.step(Frame.decode(record)).button, "Right")
        record["data"]["menu"]["spell_level"] = 3
        self.assertEqual(transaction.step(Frame.decode(record)).button, "Circle")

    def test_unlevelled_spell_ignores_persisting_rank_cursor(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": 98, "family": "warlock", "restrictions": 0, "menu_order": 2, "rank": 0,
             "rank_count": 1},
        ]
        frame = Frame.decode(record)
        decision = Decision("cast", 2, -1, (1, 2, 3), (1, 2, 3), "spell", 1, 1, 98, 1)
        transaction = MenuTransaction(decision, frame)
        transaction.selected = True
        record["data"]["control_mode"] = 5
        record["data"]["menu"].update(available=True, selected_menu=1, submenu_phase=7,
                                      list_phase=17, rows=[], spell_level=3,
                                      spell_level_count=4)
        action = transaction.step(Frame.decode(record))
        self.assertTrue(action is None or action.button not in {"Left", "Right"})

    def test_cast_transaction_finishes_when_room_event_takes_control(self):
        record = sample_record(active=False)
        record["data"]["action_options"] = [
            {"id": 109, "family": "warlock", "restrictions": 0, "menu_order": 4, "rank": 3},
        ]
        frame = Frame.decode(record)
        decision = Decision("cast", 2, -1, (1, 2, 3), (1, 2, 3), "explosion", 1, 1, 109, 1)
        transaction = MenuTransaction(decision, frame)
        transaction.opened = transaction.selected = transaction.cast_seen = True
        transaction.effect_seen = True
        record["data"]["control_mode"] = 3
        record["data"]["event_phase"] = 1
        self.assertIsNone(transaction.step(Frame.decode(record)))
        self.assertTrue(transaction.done)
        self.assertEqual(transaction.result["status"], "accepted")

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
            self.assertEqual(native_lab_script(options, Path("owned.json"),
                                               {"room_events": "production"})["scene"]["event_source"], 0)
            path.write_text(json.dumps(expectation | {"event_source": 1}), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "one original room-triggered event"):
                native_lab_script(options, Path("owned.json"), {"room_events": "production"})
            # A second event may follow natively (timer) or through an explicit follow tile.
            path.write_text(json.dumps(expectation | {"events": [1, 2]}), encoding="utf-8")
            native_lab_script(options, Path("owned.json"), {"room_events": "production"})
            path.write_text(json.dumps(expectation | {"events": [1, 2, 3]}), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "one original room-triggered event"):
                native_lab_script(options, Path("owned.json"), {"room_events": "production"})
            follow = PlayOptions(*basic, scene_expectation=path, scene_target_tile=(6, 12),
                                 scene_examine_quadrant=1, scene_follow_tile=(3, 13))
            path.write_text(json.dumps(expectation), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "or two when a follow tile"):
                native_lab_script(follow, Path("owned.json"), {"room_events": "production"})
            path.write_text(json.dumps(expectation | {"events": [1, 4]}), encoding="utf-8")
            native_lab_script(follow, Path("owned.json"), {"room_events": "production"})
            follow.validate_scene_expectation()
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
        arrival = {"executable": basic[0], "data": basic[1], "scenario": basic[2],
                   "scene_expectation": Path("scene.json"), "scene_target_tile": (8, 11)}
        PlayOptions(**arrival, door_route=("z038-r000-d000",)).validate_scene_expectation()
        with self.assertRaisesRegex(ValueError, "no single door case"):
            PlayOptions(**arrival, door_case="z038-r000-d000").validate_scene_expectation()
        # A route binds the first tile in its arrival room; the follow binds in that same room.
        PlayOptions(**arrival, door_route=("z038-r000-d000",),
                    scene_follow_tile=(3, 13)).validate_scene_expectation()
        with self.assertRaisesRegex(ValueError, "follow tile requires a first scene tile in the same room"):
            PlayOptions(**(arrival | {"scene_target_tile": None}),
                        scene_follow_tile=(3, 13)).validate_scene_expectation()
        with self.assertRaisesRegex(ValueError, "follow examine quadrant"):
            PlayOptions(**arrival, scene_follow_examine_quadrant=1).validate_scene_expectation()

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

    def test_setup_interaction_accepts_only_lone_cross_for_a_declared_scene(self):
        basic = (Path("VS.CombatLab.exe"), Path("data"), Path("fixture.json"))
        with self.assertRaisesRegex(ValueError, "scene expectation"):
            PlayOptions(*basic, frames=300, setup_presses="120:Cross",
                        setup_interaction=True).setup_control_tick()
        options = PlayOptions(*basic, frames=300, setup_presses="120:Up,122:Cross",
                              setup_interaction=True, scene_expectation=Path("goal.json"))
        self.assertEqual(options.setup_control_tick(), 126)
        for pulses in ("120:Cross+Up", "120:Circle", "120:Start", "120:Triangle"):
            with self.subTest(pulses=pulses):
                with self.assertRaises(ValueError):
                    PlayOptions(*basic, frames=300, setup_presses=pulses, setup_interaction=True,
                                scene_expectation=Path("goal.json")).setup_control_tick()

    def test_setup_interaction_rejects_room_entry_scene_goals(self):
        with tempfile.TemporaryDirectory() as directory:
            goal = Path(directory) / "goal.json"
            fixture = Path(directory) / "fixture.json"
            basic = (Path("VS.CombatLab.exe"), Path("data"), fixture)
            setup = {"room_events": "production"}
            for source, accepted in ((0, True), (2, True), (1, False)):
                with self.subTest(source=source):
                    goal.write_text(json.dumps({"events": [3], "event_source": source}),
                                    encoding="utf-8")
                    options = PlayOptions(*basic, frames=300, setup_presses="120:Cross",
                                          setup_interaction=True, scene_expectation=goal)
                    if accepted:
                        self.assertEqual(native_lab_script(options, fixture, setup)["press"],
                                         "120:Cross")
                    else:
                        with self.assertRaisesRegex(ValueError, "room-local or external"):
                            native_lab_script(options, fixture, setup)

    def test_shared_model_slots_are_bounded_and_budgeted(self):
        paths = PlayerPaths(Path("state"))
        self.assertEqual(server_environment(paths)["OLLAMA_NUM_PARALLEL"], "1")
        self.assertEqual(server_environment(paths, 11561, 4)["OLLAMA_NUM_PARALLEL"], "4")
        for parallel in (0, 5, True):
            with self.subTest(parallel=parallel):
                with self.assertRaises(ValueError):
                    server_environment(paths, 11561, parallel)
        self.assertEqual(model_budget(1), 4 * 1024**3)
        self.assertEqual(model_budget(4), 7 * 1024**3)

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

    def test_native_object_interaction_notice_is_dismissed_with_a_lone_circle(self):
        record = sample_record(tick=30, active=False)
        record["data"].update(control_mode=11, action_phase=0)
        frame = Frame.decode(record)
        action = CombatController().step(frame)
        self.assertEqual(action.buttons, frozenset({"Circle"}))
        self.assertTrue(input_lease_permitted(frame, frozenset({"Circle"})))
        self.assertFalse(input_lease_permitted(frame, frozenset({"Up"})))
        self.assertFalse(input_lease_permitted(frame, frozenset({"Circle", "Up"})))
        record["tick"] = 45
        self.assertIsNone(CombatController().step(Frame.decode(record)))
        # Settled clear-first combat withholds strategy, but the paused notice still needs its button.
        record["tick"] = 60
        held = CombatController().step(Frame.decode(record), allow_strategy=False)
        self.assertEqual(held.buttons, frozenset({"Circle"}))

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

    def test_native_text_choice_counts_as_a_visible_dialogue_wait(self):
        record = sample_record(active=False)
        record["data"]["control_mode"] = 3
        record["data"]["scene"]["event"].update(phase=4, choice_active=True)
        frame = Frame.decode(record)
        self.assertTrue(frame.dialogue_waiting)
        self.assertTrue(input_lease_permitted(frame, frozenset({"Circle"})))
        record["data"]["scene"]["event"]["choice_active"] = False
        self.assertFalse(Frame.decode(record).dialogue_waiting)

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

    def test_door_in_opening_range_is_faced_instead_of_navigated(self):
        # MAP269 z050-r008-d001: terrain had no outgoing edge 164 units from the door, inside the
        # Cross radius, while Ashley still faced a neighbouring tile.
        door = PhysicalDoor("z009-r006-d000")
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 1, "tile_z": 1, "layer": 0, "required_value": 0,
             "current_flag_value": 0}
        ]
        record["data"]["navigation"] = {"available": True, "facing_tile": [0, 1, 0]}
        door.observe(Frame.decode(record))
        self.assertTrue(door.opening_range(Frame.decode(record)))
        self.assertIsNone(door.navigation_request(Frame.decode(record)))
        record["tick"] = 10
        action = door.step(Frame.decode(record))
        self.assertIsNotNone(action)
        self.assertNotIn("Cross", action.buttons)
        self.assertIn("turn toward", action.reason)
        record["tick"] = 20
        record["data"]["navigation"]["facing_tile"] = [1, 1, 0]
        action = door.step(Frame.decode(record))
        self.assertEqual(action.buttons, frozenset({"Cross"}))

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

    def scene_handoff_route(self, scene_handoff: bool, loading: bool = True):
        route = PhysicalRoute(("z009-r006-d000",), scene_handoff=scene_handoff)
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 1, "tile_z": 0, "layer": 0, "required_value": 0,
             "current_flag_value": 0}
        ]
        route.observe(Frame.decode(record))
        route.step(Frame.decode(record))
        if loading:
            record["tick"] = 2
            record["data"]["ready"] = False
            route.observe(Frame.decode(record))
        record["tick"] = 3
        record["data"].update(ready=True, room=7, control_mode=3)
        record["data"]["scene"]["event"]["phase"] = 4
        route.observe(Frame.decode(record))
        return route

    def test_final_leg_accepts_an_original_event_taking_control_on_arrival(self):
        route = self.scene_handoff_route(True)
        self.assertEqual(route.accepted_tick, 3)
        self.assertEqual(route.legs[0].result()["scene_handoff_tick"], 3)

    def test_event_takeover_is_not_acceptance_without_a_scene_expectation(self):
        route = self.scene_handoff_route(False)
        self.assertIsNone(route.accepted_tick)
        self.assertIsNone(route.legs[0].result()["scene_handoff_tick"])

    def test_event_takeover_without_an_observed_room_load_is_not_arrival(self):
        route = self.scene_handoff_route(True, loading=False)
        self.assertIsNone(route.accepted_tick)

    def test_hostile_removed_by_an_original_event_does_not_hold_combat_open(self):
        goal = CombatGoal(clear_enemies_first=True)
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        goal.observe(Frame.decode(record))
        self.assertTrue(goal.combat_pending(Frame.decode(record), (9, 6)))
        record["tick"] = 50
        record["data"]["actors"] = [actor for actor in record["data"]["actors"]
                                    if actor["slot"] == 0]
        goal.observe(Frame.decode(record))
        self.assertTrue(goal.combat_pending(Frame.decode(record), (9, 6)))
        record["tick"] = 51
        record["data"].update(control_mode=3)
        record["data"]["scene"]["event"]["phase"] = 4
        goal.observe(Frame.decode(record))
        self.assertFalse(goal.combat_pending(Frame.decode(record), (9, 6)))
        self.assertEqual(set(goal.withdrawn.values()), {51})
        record["tick"] = 52
        record["data"]["actors"] = sample_record(active=False)["data"]["actors"]
        goal.observe(Frame.decode(record))
        self.assertTrue(goal.combat_pending(Frame.decode(record), (9, 6)))

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

    def chest_route_record(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["doors"] = [
            {"index": 0, "destination_zone": 9, "destination_room": 7,
             "tile_x": 1, "tile_z": 0, "layer": 0, "required_value": 0,
             "current_flag_value": 0}
        ]
        record["data"]["chests"] = [
            {"index": 0, "tile_x": 2, "tile_z": 2, "layer": 0, "facing": 0, "lock_level": 0,
             "opened_flag": 6, "opened": False,
             "misc": [{"slot": 2, "id": 472, "count": 1}]},
        ]
        return record

    def test_route_claims_the_source_chest_from_its_authored_facing_before_the_door(self):
        route = PhysicalRoute(("z009-r006-d000",), claim_source_chest=True)
        record = self.chest_route_record()
        route.observe(Frame.decode(record))
        chest = route.legs[0].chest
        self.assertEqual(chest.approach_tile(), (2, 3))
        self.assertIs(route.legs[0].navigation_target, chest)
        walk = route.step(Frame.decode(record))
        self.assertIn("chest approach tile", walk.reason)
        record["tick"] = 20
        record["data"]["actors"][0]["position"] = [2 * 128 + 64, 0, 3 * 128 + 64]
        turn = route.step(Frame.decode(record))
        self.assertIn("turn toward", turn.reason)
        record["tick"] = 40
        record["data"]["navigation"] = {"available": False, "facing_tile": [2, 2, 0]}
        self.assertEqual(route.step(Frame.decode(record)).buttons, frozenset({"Cross"}))
        record["tick"] = 60
        record["data"]["control_mode"] = 9
        route.observe(Frame.decode(record))
        self.assertIsNone(route.step(Frame.decode(record)))
        record["tick"] = 80
        record["data"]["control_mode"] = 1
        record["data"]["chests"][0]["opened"] = True
        route.observe(Frame.decode(record))
        self.assertEqual(chest.opened_tick, 80)
        self.assertTrue(chest.interaction_seen)
        self.assertIs(route.legs[0].navigation_target, route.legs[0])
        record["tick"] = 100
        self.assertIn("door", route.step(Frame.decode(record)).reason)
        self.assertEqual(route.result()["cases"][0]["chest"]["opened_tick"], 80)

    def test_unopened_chest_blocks_the_route_after_three_facing_presses(self):
        route = PhysicalRoute(("z009-r006-d000",), claim_source_chest=True)
        record = self.chest_route_record()
        record["data"]["actors"][0]["position"] = [2 * 128 + 64, 0, 3 * 128 + 64]
        record["data"]["navigation"] = {"available": False, "facing_tile": [2, 2, 0]}
        presses = 0
        for tick in range(1, 400, 10):
            record["tick"] = tick
            route.observe(Frame.decode(record))
            action = route.step(Frame.decode(record))
            presses += action is not None and "Cross" in action.buttons
        self.assertEqual(presses, 3)
        self.assertIn("three facing Cross", route.blocked_reason)

    def test_route_skips_locked_opened_or_empty_chests(self):
        for change in ({"lock_level": 1}, {"opened": True}, {"misc": []}):
            route = PhysicalRoute(("z009-r006-d000",), claim_source_chest=True)
            record = self.chest_route_record()
            record["data"]["chests"][0].update(change)
            route.observe(Frame.decode(record))
            self.assertIsNotNone(route.legs[0].chest.skipped)
            self.assertIs(route.legs[0].navigation_target, route.legs[0])
            self.assertIn("door", route.step(Frame.decode(record)).reason)

    def test_chest_contents_screen_is_native_loot(self):
        record = sample_record(active=False)
        record["data"].update(control_mode=9, action_phase=3)
        record["data"]["menu"].update(available=True, loading=False, selected_menu=12,
                                      loot_input_ready=True, loot_count=1)
        self.assertTrue(loot_menu_ready(Frame.decode(record)))
        record["data"]["action_phase"] = 2
        self.assertFalse(loot_menu_ready(Frame.decode(record)))

    def test_room_chest_claims_require_a_door_route(self):
        options = PlayOptions(Path("VS.CombatLab.exe"), Path("data"), Path("lab.json"),
                              claim_room_chests=True)
        with self.assertRaisesRegex(ValueError, "Room-chest claims"):
            options.validate_scene_expectation()

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


class LedgeJumpTests(unittest.TestCase):
    @staticmethod
    def planner(ledge=-512):
        from vs_player.crates import CratePlanner, RoomGrid
        # One column modelled on MAP020 x=6: a -128 platform (z0-1), floor (z2-3) and a ledge (z4-5).
        heights = [-128, -128, 0, 0, ledge, ledge]
        grid = {"width": 1, "depth": len(heights), "floor": [value & 0x7FFF for value in heights],
                "ceiling": [-1] * len(heights), "flags": [1] * len(heights)}
        return CratePlanner(RoomGrid.decode(grid), [], 128)

    def test_running_jump_reaches_a_ledge_384_above_the_platform(self):
        from vs_player.ledges import plan_ledge_jump
        jump = plan_ledge_jump(self.planner(), (), (0, 2), (0, 5))
        self.assertIsNotNone(jump)
        self.assertEqual((jump.runup, jump.takeoff, jump.landing, jump.direction),
                         ((0, 0), (0, 1), (0, 4), 2))
        self.assertEqual((jump.takeoff_height, jump.landing_height), (-128, -512))

    def test_ledges_beyond_jump_height_or_already_walkable_are_not_planned(self):
        from vs_player.ledges import plan_ledge_jump
        self.assertIsNone(plan_ledge_jump(self.planner(-640), (), (0, 2), (0, 5)))
        self.assertIsNone(plan_ledge_jump(self.planner(-256), (), (0, 2), (0, 5)))
        jump = plan_ledge_jump(self.planner(), (), (0, 2), (0, 5))
        self.assertIsNone(plan_ledge_jump(self.planner(), (), (0, 2), (0, 5),
                                          frozenset({(jump.runup, jump.direction)})))

    def test_run_stride_phase_must_reach_the_takeoff_window(self):
        from vs_player.ledges import DIAGONAL_STEP, LedgeJump, LedgeJumpAssist
        assist = LedgeJumpAssist((6, 12), None)
        # MAP020: run-up (6,6), takeoff (6,7) toward +z; the platform edge is z=1024.
        assist.jump = LedgeJump((6, 6), (6, 7), (6, 10), 2, -128, -512)
        self.assertEqual(assist._edge(), 1024)
        # Standing at 845 the strides take off at 1007 (too early) or 1025 (off the platform).
        self.assertFalse(assist.phase_ok(845))
        self.assertFalse(assist.in_press_window(1007))
        # One diagonal step shifts the phase so a stride lands on 1013, inside the grab window.
        self.assertTrue(assist.phase_ok(845 + DIAGONAL_STEP))
        self.assertTrue(assist.phase_ok(845 + 2 * DIAGONAL_STEP))
        self.assertTrue(assist.in_press_window(1012))
        self.assertTrue(assist.phase_ok(850))


class InputHandshakeTests(unittest.TestCase):
    @staticmethod
    def frame(tick, held=0, control=5):
        from types import SimpleNamespace
        return SimpleNamespace(tick=tick, held=held, control=control)

    def test_press_is_held_until_native_samples_it_then_released_before_the_next_edge(self):
        from vs_player.runner import InputHandshake
        down = 16384
        handshake = InputHandshake()
        handshake.pressed(self.frame(100), frozenset({"Down"}), 4)
        # The host batch has not pumped the key yet, so releasing now would lose the press.
        handshake.observe(self.frame(110))
        self.assertFalse(handshake.release_due(self.frame(110)))
        handshake.observe(self.frame(111, down))
        self.assertFalse(handshake.release_due(self.frame(113, down)))
        self.assertTrue(handshake.release_due(self.frame(115, down)))
        handshake.released(self.frame(115, down))
        # Until native reports the release, another press would merge into one held key.
        self.assertTrue(handshake.awaiting_release(self.frame(116, down)))
        handshake.observe(self.frame(118))
        self.assertFalse(handshake.awaiting_release(self.frame(118)))

    def test_field_movement_leases_may_run_together(self):
        from vs_player.runner import InputHandshake
        handshake = InputHandshake()
        handshake.pressed(self.frame(10, control=1), frozenset({"Up"}), 1)
        handshake.observe(self.frame(11, 4096, control=1))
        self.assertTrue(handshake.release_due(self.frame(12, 4096, control=1)))
        handshake.released(self.frame(12, 4096, control=1))
        self.assertFalse(handshake.awaiting_release(self.frame(12, 4096, control=1)))

    def test_running_jump_keeps_direction_held_while_square_releases(self):
        # MAP020 Lab replay: a one-frame Up gap after Up+Square halved the airborne distance.
        from vs_player.runner import InputHandshake
        up, square = 4096, 128
        handshake = InputHandshake()
        jump = frozenset({"Up", "Square"})
        handshake.pressed(self.frame(100, up, control=1), jump, 2)
        handshake.observe(self.frame(101, up | square, control=1))
        self.assertTrue(handshake.release_due(self.frame(103, up | square, control=1)))
        held = handshake.release_buttons(self.frame(103, up | square, control=1), jump, True)
        self.assertEqual(held, frozenset({"Up"}))
        self.assertTrue(handshake.awaiting_release(self.frame(103, up | square, control=1)))
        # Up stays held while the native pad still shows Square.
        self.assertEqual(handshake.release_buttons(self.frame(104, up | square, control=1), held, True), held)
        handshake.observe(self.frame(105, up, control=1))
        self.assertFalse(handshake.awaiting_release(self.frame(105, up, control=1)))
        # The handover window lets the next direction lease replace it without a gap.
        self.assertEqual(handshake.release_buttons(self.frame(106, up, control=1), held, True), held)
        self.assertEqual(handshake.release_buttons(self.frame(120, up, control=1), held, True), frozenset())
        self.assertEqual(handshake.sustained, frozenset())

    def test_menu_mixed_lease_still_releases_every_button(self):
        from vs_player.runner import InputHandshake
        handshake = InputHandshake()
        lease = frozenset({"Up", "Square"})
        handshake.pressed(self.frame(10), lease, 2)
        self.assertEqual(handshake.release_buttons(self.frame(13), lease, True), frozenset())
        self.assertEqual(handshake.sustained, frozenset())


class CratePuzzleTests(unittest.TestCase):
    LEDGE = (-384) & 0x7FFF

    def grid(self, alcove=False):
        # Row z=1 is a corridor whose east end is a ledge 384 units up; row z=0 is wall
        # except an optional one-tile alcove at x=1.
        width, depth = 5, 2
        floor = [-1] * width + [0, 0, 0, 0, self.LEDGE]
        flags = [0] * width + [1] * width
        if alcove:
            floor[1], flags[1] = 0, 1
        return {"width": width, "depth": depth, "floor": floor,
                "ceiling": [-1] * width * depth, "flags": flags}

    def planner(self, alcove=False):
        from vs_player.crates import CratePlanner, RoomGrid
        return CratePlanner(RoomGrid.decode(self.grid(alcove)), [], 128)

    def test_ledge_above_climb_limit_needs_a_pushed_crate_step(self):
        from vs_player.crates import Crate
        planner = self.planner()
        self.assertIsNone(planner.plan((0, 1), (), (4, 1)))
        plan = planner.plan((0, 1), (Crate(3, 2, 2, 1, 0),), (4, 1))
        self.assertEqual(plan, [{"action": "push", "stand": (1, 1), "direction": 3, "slot": 3}])

    def test_push_requires_a_far_tile_no_higher_than_the_crate_base(self):
        from vs_player.crates import Crate
        planner = self.planner()
        # Pushing east from (2,1) would put the crate onto the ledge tile, which is above its base.
        self.assertIsNone(planner.push_result((2, 1), 3, (Crate(3, 2, 3, 1, 0),)))
        self.assertIsNone(planner.push_result((2, 1), 0, (Crate(3, 2, 3, 1, 0),)))

    def test_wall_bound_liftable_crate_is_carried_and_placed_below_the_ledge(self):
        from vs_player.crates import Crate
        planner = self.planner(alcove=True)
        crate = Crate(4, 5, 1, 0, 0)
        self.assertIsNone(planner.push_result((1, 1), 0, (crate,)))
        plan = planner.plan((0, 1), (crate,), (4, 1))
        self.assertEqual(len(plan), 1)
        self.assertEqual(plan[0]["action"], "lift")
        self.assertEqual((plan[0]["stand"], plan[0]["direction"]), ((1, 1), 0))
        place = plan[0]["place_stand"]
        dx, dz = {0: (0, -1), 1: (-1, 0), 2: (0, 1), 3: (1, 0)}[plan[0]["place_direction"]]
        self.assertEqual((place[0] + dx, place[1] + dz), (3, 1))

    def frame(self, tick, tile=(0, 1), facing=(1, 1), crate_tile=(2, 1)):
        record = sample_record(tick, active=False)
        record["data"].update(control_mode=1, action_phase=0)
        record["data"]["actors"] = record["data"]["actors"][:1]
        record["data"]["actors"][0]["weapon_drawn"] = 0
        record["data"]["actors"][0]["position"] = [tile[0] * 128 + 64, 0, tile[1] * 128 + 64]
        record["data"]["navigation"] = {
            "available": True, "origin": [0, 0], "width": 1, "depth": 1, "spacing": 64,
            "collision_height": 128, "movement_mode": 0, "locomotion_state": 0,
            "route_phase": 0, "jumping": False, "facing_tile": [*facing, 0],
            "samples": [[0, -2032, 5]],
        }
        record["data"]["room_tiles"] = self.grid()
        record["data"]["gimmicks"] = [{
            "slot": 3, "object_id": 2, "interaction": 0, "falling": False,
            "tile_x": crate_tile[0], "tile_z": crate_tile[1],
            "position": [crate_tile[0] * 128 + 64, 0, crate_tile[1] * 128 + 64],
            "mode": 0, "motion_state": 0, "skipped": False, "visible": True,
            "parent": 255, "carrier": 0, "carried": False,
        }]
        return Frame.decode(record)

    def test_scene_goal_executes_the_crate_push_before_trigger_navigation(self):
        goal = SceneTileGoal((4, 1), 9, 6, 1)
        frame = self.frame(10)
        self.assertIsNone(goal.request(frame))
        self.assertTrue(goal.pushing(frame))
        self.assertEqual(goal.crates.plan[0]["stand"], (1, 1))
        walk = goal.step(self.frame(20))
        self.assertIsNotNone(walk)
        self.assertEqual(walk.reason, "execute model-selected observed-terrain waypoint")
        # On the stand tile already facing the crate: an ordinary Cross press starts the push.
        press = goal.step(self.frame(30, tile=(1, 1), facing=(2, 1)))
        press = press or goal.step(self.frame(40, tile=(1, 1), facing=(2, 1)))
        self.assertEqual(press.buttons, frozenset({"Cross"}))
        goal.step(self.frame(50, tile=(1, 1), facing=(2, 1), crate_tile=(3, 1)))
        self.assertEqual(goal.crates.stage, "settle")
        goal.step(self.frame(80, tile=(1, 1), facing=(2, 1), crate_tile=(3, 1)))
        self.assertFalse(goal.crates.active())
        self.assertEqual(goal.crates.actions_done, 1)
        # With the step in place the trigger is walkable, so ordinary navigation resumes.
        self.assertFalse(goal.crates.needed(self.frame(90, tile=(1, 1), crate_tile=(3, 1))))

    def test_scene_goal_treats_a_blocked_floor_trigger_cell_as_a_lock_door(self):
        frame = self.frame(10, tile=(1, 1), facing=(1, 0), crate_tile=(4, 1))
        # Passability-5 lock cell: native collision blocks it but it keeps a real floor.
        frame.raw["room_tiles"]["floor"][1] = 0
        goal = SceneTileGoal((1, 0), 9, 6, 1)
        goal.observe(frame)
        # Later frames omit the periodic room_tiles export; the observed grid must persist.
        frame = self.frame(10, tile=(1, 1), facing=(1, 0), crate_tile=(4, 1))
        del frame.raw["room_tiles"]
        self.assertTrue(goal.door_tile(frame))
        wall = SceneTileGoal((2, 0), 9, 6, 1)
        wall.observe(self.frame(10, tile=(1, 1), facing=(1, 0), crate_tile=(4, 1)))
        self.assertFalse(wall.door_tile(frame))
        # No route can reach a closed door cell, so the push starts from the adjacent stand.
        self.assertIsNone(goal.request(frame))
        self.assertEqual(goal.push_until, 250)
        press = goal.step(frame)
        self.assertEqual(press.buttons, frozenset({"Cross"}))
        self.assertEqual(goal.navigator.feedback[-1]["status"], "door_open_press")

    def test_scene_goal_examines_from_its_stand_tile_after_turning_to_the_quadrant(self):
        goal = SceneTileGoal((1, 1), 9, 6, 1, examine_quadrant=0)
        self.assertEqual(goal.identity, "scene:9:6:local:1:tile:1:1:examine:0")
        # Facing +X: on the stand tile the goal owns input and turns toward -Z before examining.
        frame = self.frame(10, tile=(1, 1), facing=(2, 1), crate_tile=(4, 1))
        self.assertIsNone(goal.request(frame))
        self.assertTrue(goal.pushing(frame))
        turn = goal.step(frame)
        self.assertEqual(turn.reason, "turn toward the original examine quadrant")
        self.assertNotIn("Cross", turn.buttons)
        press = goal.step(self.frame(20, tile=(1, 1), facing=(1, 0), crate_tile=(4, 1)))
        self.assertEqual(press.buttons, frozenset({"Cross"}))
        self.assertEqual(goal.result()["examine_presses"], [20])
        # Presses are spaced so native interaction playback can start before another Cross.
        self.assertIsNone(goal.step(self.frame(40, tile=(1, 1), facing=(1, 0), crate_tile=(4, 1))))
        with self.assertRaises(ValueError):
            SceneTileGoal((1, 1), 9, 6, 1, examine_quadrant=4)


if __name__ == "__main__":
    unittest.main()
