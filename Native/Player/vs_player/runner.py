import json
import hashlib
import math
import queue
import sqlite3
import subprocess
import sys
import threading
import time
from concurrent.futures import Future, ThreadPoolExecutor
from contextlib import contextmanager
from dataclasses import asdict, dataclass
from pathlib import Path

from .config import PROJECT, PlayerPaths, runtime_manifest
from .controller import (CombatController, InputAction, StaleDecision, input_lease_permitted,
                         strategy_planning_ready)
from .doors import PhysicalDoor, PhysicalRoute
from .model import Decision, ModelResponseError, NavigationDecision
from .navigation import SceneTileGoal, TerrainNavigator, field_navigation_ready
from .choices import reward_menu_ready, urgent_recovery_needed
from .observations import Frame
from .processes import (ProcessJob, freeze_game, game_environment, model_budget, model_server,
                        shared_model, verify_installed_model, warm_model)
from .store import Retention, RunStore, owned_tree_size
from .trace import ObservationFreshness, TraceReader, find_trace
from .windows import BUTTON_KEYS, WindowInput


@contextmanager
def planning_worker():
    worker = ThreadPoolExecutor(max_workers=1, thread_name_prefix="player-planner")
    try:
        yield worker
    finally:
        # A slow model request must not delay releasing keys or closing the game.
        worker.shutdown(wait=False, cancel_futures=True)


class AuditWriter:
    def __init__(self, paths: PlayerPaths, run_id: str) -> None:
        self.messages: queue.Queue[tuple[str, dict] | None] = queue.Queue(maxsize=4096)
        self.error: BaseException | None = None

        def write() -> None:
            store = RunStore(paths)
            try:
                while True:
                    message = self.messages.get()
                    if message is None:
                        return
                    store.event(run_id, *message)
            except (sqlite3.Error, OSError, ValueError, TypeError) as error:
                self.error = error
            finally:
                store.close()

        self.thread = threading.Thread(target=write, name="player-audit")
        self.thread.start()

    def check(self) -> None:
        if self.error is not None:
            raise RuntimeError("Player audit persistence failed") from self.error
        if not self.thread.is_alive():
            raise RuntimeError("Player audit writer stopped unexpectedly")

    def event(self, kind: str, payload: dict) -> None:
        self.check()
        try:
            self.messages.put_nowait((kind, payload))
        except queue.Full as error:
            raise RuntimeError("Player audit writer cannot keep up; no events will be dropped") from error

    def close(self) -> None:
        if self.thread.is_alive():
            self.messages.put(None, timeout=10)
            self.thread.join(timeout=10)
        if self.thread.is_alive():
            raise RuntimeError("Player audit writer did not finish")
        if self.error is not None:
            raise RuntimeError("Player audit persistence failed") from self.error


class PeriodicCleanup:
    def __init__(self, paths: PlayerPaths, retention: Retention) -> None:
        self.stop = threading.Event()
        self.error: BaseException | None = None

        def maintain() -> None:
            while not self.stop.wait(retention.interval_seconds):
                store = RunStore(paths)
                try:
                    report = store.cleanup(retention)
                    if report["over_budget_pinned_evidence"]:
                        raise RuntimeError("Pinned evidence exceeds the log budget; review it before more tests")
                except (sqlite3.Error, OSError, ValueError, RuntimeError) as error:
                    self.error = error
                    return
                finally:
                    store.close()

        self.thread = threading.Thread(target=maintain, name="player-retention")
        self.thread.start()

    def close(self) -> None:
        self.stop.set()
        self.thread.join(timeout=15)
        if self.thread.is_alive():
            raise RuntimeError("Owned log cleanup did not finish")


@dataclass(frozen=True)
class PlayOptions:
    executable: Path
    data: Path
    scenario: Path | None
    frames: int = 3600
    goal: str = "Defeat the hostile actors using normal controls and the equipped reactions."
    minimum_chains: int = 0
    minimum_defenses: int = 0
    maximum_trace_age: float = 0.1
    strict_reactions: bool = True
    model_port: int = 11551
    door_case: str = ""
    save_file: Path | None = None
    door_route: tuple[str, ...] = ()
    clear_enemies_first: bool = False
    setup_presses: str = ""
    new_game: bool = False
    simulation_speed: int = 1
    scene_expectation: Path | None = None
    scene_target_tile: tuple[int, int] | None = None
    scene_examine_quadrant: int | None = None
    # Type-8 triggers such as MAP203 local2 test several defeats in one evaluation (Triggers.c cases 7/16).
    simultaneous_defeat: tuple[int, ...] = ()
    # Type-8 rows such as MAP003 local1 need one hostile defeated while an earlier-listed slot lives.
    defeat_order: tuple[int, ...] = ()
    allow_stat_overrides: bool = False
    allow_story_progress: bool = False
    claim_room_chests: bool = False
    setup_interaction: bool = False
    shared_model: bool = False
    model_parallel: int = 1
    def validate_scene_expectation(self) -> None:
        if self.claim_room_chests and not self.door_route:
            raise ValueError("Room-chest claims require a physical door route from the source room")
        if self.scene_target_tile is not None and (
                self.scene_expectation is None or self.door_case
                or not isinstance(self.scene_target_tile, tuple)
                or len(self.scene_target_tile) != 2
                or any(type(value) is not int or not 0 <= value <= 255
                       for value in self.scene_target_tile)):
            raise ValueError("Scene-tile navigation requires an explicit local-scene goal, no single door case, "
                             "and a native tile")
        if self.scene_examine_quadrant is not None and (
                self.scene_target_tile is None or type(self.scene_examine_quadrant) is not int
                or not 0 <= self.scene_examine_quadrant <= 3):
            raise ValueError("A scene examine quadrant requires a scene stand tile and a native quadrant 0-3")
        if self.simultaneous_defeat and (
                self.scene_expectation is None or len(self.simultaneous_defeat) < 2
                or len(set(self.simultaneous_defeat)) != len(self.simultaneous_defeat)
                or any(type(slot) is not int or not 1 <= slot <= 31 for slot in self.simultaneous_defeat)):
            raise ValueError("A simultaneous defeat requires a local-scene goal and at least two distinct "
                             "hostile slots")
        if self.defeat_order and (
                self.scene_expectation is None or self.simultaneous_defeat
                or len(set(self.defeat_order)) != len(self.defeat_order)
                or any(type(slot) is not int or not 1 <= slot <= 31 for slot in self.defeat_order)):
            raise ValueError("A defeat order requires a local-scene goal, distinct hostile slots and no "
                             "simultaneous defeat")
        if self.scene_expectation is not None and (
                self.executable.name != "VS.CombatLab.exe" or self.scenario is None
                or self.new_game or self.save_file is not None):
            raise ValueError("Native scene completion requires an explicit Combat Lab source fixture")

    def validate_simulation_speed(self) -> None:
        if type(self.simulation_speed) is not int or self.simulation_speed not in (1, 2, 4):
            raise ValueError("Investigation speed must be1,2 or4")
        if self.simulation_speed == 1:
            return
        if (self.strict_reactions or self.minimum_chains or self.minimum_defenses
                or self.executable.name != "VS.CombatLab.exe" or self.scenario is None
                or self.new_game or self.save_file is not None
                or self.door_case or self.door_route):
            raise ValueError(
                "Accelerated play requires a Lab scene investigation without "
                "reaction certification, save/New Game or physical-door objectives"
            )

    def desktop_startup_presses(self) -> str:
        if self.executable.name != "VS.Desktop.exe" or self.scenario is not None:
            raise ValueError("Production New Game/Continue requires Desktop without a Lab scenario")
        if self.new_game:
            if self.save_file is not None or self.setup_presses:
                raise ValueError("Production New Game cannot use a save or source-fixture movement")
            return "1000:Start,1300:Circle"
        if self.save_file is None:
            raise ValueError("Desktop requires an explicit New Game or unchanged native save")
        return "1000:Start,1300:Circle,1450:Circle,1510:Up,1570:Circle"

    def setup_control_tick(self) -> int:
        if not self.setup_presses:
            return 0
        if (self.executable.name != "VS.CombatLab.exe" or self.scenario is None
                or self.save_file is not None):
            raise ValueError("Source setup movement requires a Combat Lab scenario")
        if self.door_case or self.door_route or self.clear_enemies_first:
            raise ValueError("Source setup movement cannot modify physical-door objectives")
        if self.setup_interaction and self.scene_expectation is None:
            raise ValueError("Setup interaction pulses require an original scene expectation")
        previous = -2
        for pulse in self.setup_presses.split(","):
            tick_text, separator, keys_text = pulse.partition(":")
            keys = keys_text.split("+")
            # A lone Cross is the ordinary field action that activates original type-4 triggers.
            interaction = self.setup_interaction and keys == ["Cross"]
            if (not separator or not tick_text.isascii() or not tick_text.isdecimal()
                    or not keys or len(set(keys)) != len(keys)
                    or not (interaction or set(keys) <= {"Up", "Down", "Left", "Right"})
                    or {"Up", "Down"} <= set(keys) or {"Left", "Right"} <= set(keys)):
                raise ValueError("Source setup pulses require decimal ticks and ordinary directions only")
            tick = int(tick_text)
            if tick < previous + 2:
                raise ValueError("Source setup pulses must be ordered and cannot overlap")
            if tick + 4 >= self.frames:
                raise ValueError("Source setup movement must finish before the frame budget")
            previous = tick
        return previous + 4


class CombatGoal:
    def __init__(self, door_case: str = "", door_route: tuple[str, ...] = (),
                 clear_enemies_first: bool = False, claim_room_chests: bool = False,
                 scene_handoff: bool = False) -> None:
        if door_case and door_route:
            raise ValueError("Choose a single physical door or a continuous route, not both")
        self.door = (PhysicalRoute(door_route, claim_room_chests, scene_handoff) if door_route else
                     PhysicalDoor(door_case) if door_case else None)
        self.seen: set[tuple[int, int, int, int, int]] = set()
        self.dead: set[tuple[int, int, int, int, int]] = set()
        # Living hostiles removed by an original event while it owned playback.
        self.withdrawn: dict[tuple[int, int, int, int, int], int] = {}
        self.quiet_start: int | None = None
        self.accepted_tick: int | None = None
        self.player_dead = False
        self.forced_skip = False
        self.encounters: set[tuple[int, int]] = set()
        self.max_incoming_hp_damage = 0
        self.clear_enemies_first = clear_enemies_first

    def combat_pending(self, frame: Frame, source_scope: tuple[int, int] | None = None) -> bool:
        if not self.clear_enemies_first:
            return False
        if source_scope is None:
            if self.door is None:
                return False
            leg = self.door.active_leg if isinstance(self.door, PhysicalRoute) else self.door
            source_scope = (leg.source_zone, leg.source_room)
        source = source_scope
        if (frame.zone, frame.room) == source and frame.enemies:
            return True
        return any(identity[1:3] == source and identity not in self.dead
                   and identity not in self.withdrawn
                   for identity in self.seen)

    def exit_ready(self, frame: Frame) -> bool:
        if self.door is None or self.accepted_tick is not None or self.combat_pending(frame):
            return False
        leg = self.door.active_leg if isinstance(self.door, PhysicalRoute) else self.door
        return (field_navigation_ready(frame) and not frame.enemies
                and not frame.dialogue_waiting and not reward_menu_ready(frame)
                and leg.contract is not None and not leg.waiting_for_gate
                and leg.arrival_tick is None and not leg.blocked_reason
                and (frame.zone, frame.room) == (leg.source_zone, leg.source_room))

    @staticmethod
    def identity(frame: Frame, actor) -> tuple[int, int, int, int, int]:
        return frame.episode, frame.zone, frame.room, actor.slot, actor.spawn_index

    def observe(self, frame: Frame) -> None:
        if self.door is not None:
            self.door.observe(frame)
            self.accepted_tick = self.door.accepted_tick
        if not frame.ready:
            return
        combat_observation = frame.event_phase == 0 and frame.control != 3
        if (combat_observation and frame.reaction.source_actor > 0
                and frame.reaction.target_actor == 0):
            self.max_incoming_hp_damage = max(
                self.max_incoming_hp_damage, frame.reaction.incoming_hp_damage,
            )
        self.player_dead |= frame.game_over
        scene = frame.raw["scene"]
        if scene["available"]:
            self.forced_skip |= scene["event"]["forced_skip"]
        if combat_observation:
            for actor in frame.enemies:
                self.seen.add(self.identity(frame, actor))
                if frame.player is not None:
                    self.encounters.add((actor.model, frame.player.weapon_category))
        for actor in frame.actors:
            identity = self.identity(frame, actor)
            if identity in self.seen and actor.hp == 0:
                self.dead.add(identity)
            elif actor.hp > 0:
                self.withdrawn.pop(identity, None)
        if frame.event_phase != 0 and scene["available"]:
            present = {self.identity(frame, actor) for actor in frame.actors}
            for identity in self.seen - self.dead - present:
                if identity[:3] == (frame.episode, frame.zone, frame.room):
                    self.withdrawn.setdefault(identity, frame.tick)
        settled = (
            self.seen and self.seen <= self.dead | set(self.withdrawn) and not frame.enemies
            and frame.player is not None and frame.player.hp > 0
            and frame.control == 1 and frame.raw["transition"] == 0
            and frame.event_phase == 0 and frame.raw["field_rendering_enabled"]
        )
        if self.door is not None:
            return
        if settled:
            if self.quiet_start is None:
                self.quiet_start = frame.tick
            if frame.tick - self.quiet_start >= 120 and self.accepted_tick is None:
                self.accepted_tick = frame.tick
        else:
            self.quiet_start = None
            if frame.enemies:
                self.accepted_tick = None


def native_lab_script(options: PlayOptions, scenario: Path, setup: dict) -> dict:
    options.validate_scene_expectation()
    script = {"name": "external-player", "scenario": str(scenario), "frames": options.frames}
    if options.setup_presses:
        script["press"] = options.setup_presses
    if options.scene_expectation is not None:
        if setup.get("room_events") != "production":
            raise ValueError("Native scene completion requires unchanged production-room events")
        expectation = json.loads(options.scene_expectation.read_text(encoding="utf-8"))
        if not isinstance(expectation, dict):
            raise ValueError("Native scene expectation must be a JSON object")
        script["scene"] = expectation
        if options.setup_interaction and expectation.get("event_source") not in (0, 2):
            raise ValueError("Setup interaction pulses require an original room-local or external event")
        if options.scene_target_tile is not None and (
                expectation.get("event_source") != 2
                or not isinstance(expectation.get("events"), list)
                or len(expectation["events"]) != 1):
            raise ValueError("Scene-tile navigation requires exactly one original room-local event")
    return script


def native_outcome(options: PlayOptions, reader: TraceReader,
                   returncode: int | None,
                   expected_scene: dict | None = None) -> tuple[bool, dict | None]:
    if options.executable.name == "VS.Desktop.exe":
        result = {
            "coverage": ("production_new_game_controller_observation" if options.new_game
                         else "production_continue_controller_observation"),
            "host_exit": reader.host_exit,
            "trace_finalized": reader.ended,
            "engine_errors": len(reader.engine_errors),
            "game_finished": bool(reader.host_exit and reader.host_exit.get("game_finished")),
        }
        complete = (returncode == 0 and reader.host_exit is not None
                    and reader.ended and not reader.engine_errors and not reader.pending)
        return complete, result
    checkpoint = reader.path.parent / "sequence-results.json"
    native = json.loads(checkpoint.read_text(encoding="utf-8"))
    scripts = native["scripts"]
    scene_complete = expected_scene is None
    expected_status = "completed"
    if options.scene_expectation is not None:
        expected_status = "passed"
        scene_complete = False
        if len(scripts) == 1 and expected_scene is not None:
            scene = scripts[0].get("scene", {})
            start, end = scene.get("control_tick"), scene.get("passed_tick")
            scene_complete = (
                scene.get("coverage") == "scene_playback_control"
                and scene.get("status") == "passed"
                and scene.get("expected_events") == expected_scene.get("events")
                and scene.get("matched_events") == len(expected_scene.get("events", []))
                and scene.get("event_source") == expected_scene.get("event_source", 0)
                and scene.get("return_zone") == expected_scene.get("return_zone")
                and scene.get("return_room") == expected_scene.get("return_room")
                and scene.get("preconditions") == expected_scene.get("preconditions")
                and type(start) is int and type(end) is int and end - start >= 120
            )
    complete = (
        returncode == 0 and len(scripts) == 1
        and scripts[0]["status"] == expected_status and scripts[0]["teardown_complete"]
        and scripts[0]["engine_errors"] == 0 and reader.ended and scene_complete
        and not reader.engine_errors and not reader.pending
    )
    return complete, scripts[0] if scripts else None


class SceneReturnHold:
    """Mirror Trace.cpp SceneTrial::Observe event matching. Once every expected event executed and
    the return room is reached, only native stable-control observation remains, so ordinary play
    (drawing the weapon at hostiles) must stop rather than keep resetting that interval."""

    def __init__(self, expectation: dict) -> None:
        self.events = list(expectation["events"])
        self.source = expectation.get("event_source", 0)
        self.room = (expectation.get("return_zone"), expectation.get("return_room"))
        self.enabled = not (expectation.get("ending_to_title") or expectation.get("attract_to_title"))
        self.matched = 0
        self.active = False
        self.last: int | None = None
        self.sheath_tick = -100
        self.hold_tick: int | None = None

    def observe(self, frame: Frame) -> None:
        scene = frame.raw.get("scene", {})
        if not scene.get("available"):
            return
        event = scene["event"]
        if event.get("phase") == 4 and event.get("source") == self.source:
            if not self.active or self.last != event["id"]:
                self.last = event["id"]
                if self.matched < len(self.events) and event["id"] == self.events[self.matched]:
                    self.matched += 1
            self.active = True
        else:
            self.active = False

    def holding(self, frame: Frame) -> bool:
        return (self.enabled and self.matched == len(self.events) and frame.ready
                and (frame.zone, frame.room) == self.room and frame.event_phase == 0
                and not self.engaged(frame))

    @staticmethod
    def engaged(frame: Frame) -> bool:
        """A hostile attacking Ashley keeps forcing battle control, so ordinary combat must end it
        before native can observe a stable field interval; distant hostiles are left alone."""
        player = frame.player
        if player is None:
            return False
        if frame.reaction.active and frame.reaction.target_actor == 0:
            return True
        x, y, z = player.position
        return any(math.hypot(enemy.position[0] - x, enemy.position[2] - z) <= 384
                   and abs(enemy.position[1] - y) <= 256 for enemy in frame.enemies)

    def step(self, frame: Frame) -> InputAction | None:
        if self.hold_tick is None:
            self.hold_tick = frame.tick
        if frame.player is not None and frame.player.weapon_drawn and frame.control in (1, 2):
            if frame.tick - self.sheath_tick > 30:
                self.sheath_tick = frame.tick
                return InputAction(frozenset({"Cross"}), 4,
                                   "sheathe normally while native observes restored scene control")
        return None


DIRECTION_BUTTONS = frozenset({"Up", "Down", "Left", "Right"})


class InputHandshake:
    """Release posted keys only after the native pad sampled them, and wait for the native release
    before the next edge. Batched host ticks otherwise merge a press with its release (the press is
    lost) or a release with the next press (no new edge), as MAP203 menu traces showed."""

    LOST_INPUT_TICKS = 120

    def __init__(self) -> None:
        self.mask = 0
        self.press_tick = 0
        self.duration = 0
        self.seen: int | None = None
        self.release_mask = 0
        self.release_tick = 0
        self.continuous = False

    @staticmethod
    def bits(buttons: frozenset[str]) -> int:
        return sum(BUTTON_KEYS[button][0] for button in buttons)

    def pressed(self, frame: Frame, buttons: frozenset[str], duration: int) -> None:
        self.mask, self.press_tick, self.duration, self.seen = self.bits(buttons), frame.tick, duration, None
        # Field movement needs no new press edge, so consecutive direction leases may run together.
        self.continuous = buttons <= DIRECTION_BUTTONS and frame.control == 1

    def observe(self, frame: Frame) -> None:
        if self.mask and self.seen is None and frame.held & self.mask == self.mask:
            self.seen = frame.tick
        if self.release_mask and not frame.held & self.release_mask:
            self.release_mask = 0

    def release_due(self, frame: Frame) -> bool:
        if not self.mask:
            return True
        if self.seen is None:
            return frame.tick - self.press_tick > self.LOST_INPUT_TICKS
        return frame.tick >= max(self.press_tick, self.seen) + self.duration

    def released(self, frame: Frame | None) -> None:
        if self.mask and not self.continuous:
            self.release_mask = self.mask
            self.release_tick = frame.tick if frame is not None else 0
        self.mask, self.seen = 0, None

    def awaiting_release(self, frame: Frame) -> bool:
        if self.release_mask and frame.tick - self.release_tick > self.LOST_INPUT_TICKS:
            self.release_mask = 0
        return bool(self.release_mask)


def _play_game(store: RunStore, run_id: str,
               options: PlayOptions, client, audit: AuditWriter,
               maintenance: PeriodicCleanup, retention: Retention) -> dict:
    run = store.directory(run_id)
    game = run / "game"
    hashes = freeze_game(options.executable, game)
    if options.new_game:
        setup = {"ashley_profile": "production-new-game"}
        scenario_bytes = json.dumps(setup, separators=(",", ":")).encode()
    elif options.scenario is None:
        if options.save_file is None:
            raise ValueError("A declared Lab scenario or an unchanged native save is required")
        scenario_bytes = options.save_file.resolve(strict=True).read_bytes()
        setup = {"ashley_profile": "continued-native-save"}
    else:
        scenario_bytes = options.scenario.resolve(strict=True).read_bytes()
        setup = json.loads(scenario_bytes)
    setup_control_tick = options.setup_control_tick()
    if options.setup_presses and setup.get("room_events") != "production":
        raise ValueError("Source setup movement requires unchanged production-room events")
    if setup.get("ashley_stats") and not options.allow_stat_overrides:
        raise ValueError("Managed acceptance runs do not allow stat overrides")
    if "story_progress" in setup and not options.allow_story_progress:
        raise ValueError("Declared story progress requires an explicit owner-approved assist")
    if setup.get("room_events") == "production" and setup.get("enemies"):
        raise ValueError("Production-room runs cannot replace original actors")
    command = [str(game / options.executable.name),
               "--player-observations",
               "--verification-log",
               "--data", str(options.data.resolve(strict=True)),
               "--speed", str(options.simulation_speed), "--save-dir", str(run / "private-saves")]
    scene_expectation = None
    if options.executable.name == "VS.CombatLab.exe" and options.scenario is not None:
        scenario = run / "scenario.json"
        scenario.write_bytes(scenario_bytes)
        sequence = run / "sequence.json"
        script = native_lab_script(options, scenario, setup)
        scene_expectation = script.get("scene")
        if scene_expectation is not None:
            (run / "scene-expectation.json").write_text(
                json.dumps(scene_expectation, indent=2) + "\n", encoding="utf-8",
            )
        sequence.write_text(json.dumps({
            "version": 1, "scripts": [script],
        }, indent=2) + "\n", encoding="utf-8")
        command.extend(["--sequence", str(sequence)])
    elif options.executable.name == "VS.Desktop.exe":
        presses = options.desktop_startup_presses()
        if options.save_file is not None:
            saves = run / "private-saves"
            saves.mkdir()
            (saves / "slot00.vssave").write_bytes(scenario_bytes)
        command.extend(["--frames", str(options.frames), "--press",
                        presses])
    else:
        raise ValueError("Combat Lab requires --scenario; Desktop requires --new-game or --save-file")
    audit.event("game.launch", {
        "command": command, "image_sha256": hashes,
        "scenario_sha256": hashlib.sha256(scenario_bytes).hexdigest(),
        "source_setup_presses": options.setup_presses,
        "native_scene_expectation": scene_expectation,
        "scene_target_tile": options.scene_target_tile,
        "scene_examine_quadrant": options.scene_examine_quadrant,
        "simultaneous_defeat": options.simultaneous_defeat,
        "defeat_order": options.defeat_order,
        "model_control_from_tick": setup_control_tick,
    })
    controller = CombatController()
    controller.input_ticks = options.simulation_speed
    controller.simultaneous_defeat = options.simultaneous_defeat
    goal = CombatGoal(options.door_case, options.door_route, options.clear_enemies_first,
                      options.claim_room_chests,
                      scene_handoff=options.scene_expectation is not None)
    # A native scene expectation is judged after the route reaches its room, so control stays open.
    continue_after_route = options.scene_expectation is not None and bool(options.door_route)
    # After a physical door route the trigger belongs to the arrival room, bound once the route is accepted.
    scene_tile_after_route = options.scene_target_tile is not None and bool(options.door_route)
    scene_navigation = (SceneTileGoal(options.scene_target_tile, setup["zone"], setup["room"],
                                     scene_expectation["events"][0], options.scene_examine_quadrant)
                        if options.scene_target_tile is not None and scene_expectation is not None
                        and not scene_tile_after_route else None)
    scene_hold = SceneReturnHold(scene_expectation) if scene_expectation is not None else None
    route_checkpoint_offset = 0
    latest: Frame | None = None
    reader: TraceReader | None = None
    driver: WindowInput | None = None
    release_tick = 0
    handshake = InputHandshake()
    held_deadline = 0.0
    future: Future[Decision] | None = None
    navigation_future: Future[NavigationDecision] | None = None
    navigation_owner: TerrainNavigator | SceneTileGoal | None = None
    pursuit = TerrainNavigator()
    pursuit_scope: tuple[int, ...] | None = None
    observed: Frame | None = None
    last_request = 0.0
    result_offset = 0
    effect_offset = 0
    ending_offset = 0
    invalid_responses = 0
    validation_feedback: str | None = None
    freshness = ObservationFreshness(options.maximum_trace_age)
    exit_handoff: tuple[int, str] | None = None
    inputs = 0
    max_age = 0.0
    started = time.monotonic()
    last_budget_check = started
    with (run / "game.stdout.log").open("wb") as stdout, (run / "game.stderr.log").open("wb") as stderr:
        process = subprocess.Popen(command, env=game_environment(), stdout=stdout, stderr=stderr)
        job = ProcessJob(process)
        try:
            driver = WindowInput(process.pid, game / options.executable.name)
            with planning_worker() as planner:
                while process.poll() is None:
                    audit.check()
                    if maintenance.error is not None:
                        raise RuntimeError("Periodic log cleanup failed") from maintenance.error
                    now = time.monotonic()
                    if now - started > max(120, options.frames / 30 + 60):
                        raise TimeoutError("Managed game exceeded its bounded wall-clock budget")
                    if driver.window is None and not driver.find_window():
                        time.sleep(0.01)
                        continue
                    if reader is None:
                        path = find_trace(game)
                        if path is None:
                            time.sleep(0.005)
                            continue
                        reader = TraceReader(path)
                        audit.event("trace.bound", {"path": str(path), "pid": process.pid})
                    frames = reader.read()
                    if reader.engine_errors:
                        raise RuntimeError("Native game recorded an engine error")
                    if not frames:
                        freshness.check_timeout(now)
                        if driver.held and now > held_deadline:
                            driver.set_buttons(frozenset())
                            raise TimeoutError("Input lease expired without a fresh game frame")
                        time.sleep(0.001)
                        continue
                    for observed_frame in frames:
                        handshake.observe(observed_frame)
                    for historical in frames[:-1]:
                        controller.reactions.observe(historical, allow_input=False)
                        if historical.tick <= options.frames:
                            goal.observe(historical)
                            if scene_navigation is not None:
                                scene_navigation.observe(historical)
                            if scene_hold is not None:
                                scene_hold.observe(historical)
                    latest = frames[-1]
                    age = reader.age(latest)
                    max_age = max(max_age, age)
                    if latest.raw["input"]["analog"]:
                        raise RuntimeError("Managed game still accepts a hardware gamepad")
                    was_stale = freshness.stale_since is not None
                    if not freshness.observe(age, now, latest.ready):
                        if driver.held:
                            handshake.released(latest)
                        driver.set_buttons(frozenset())
                        controller.reactions.observe(latest, allow_input=False)
                        if latest.tick <= options.frames:
                            goal.observe(latest)
                            if scene_navigation is not None:
                                scene_navigation.observe(latest)
                            if scene_hold is not None:
                                scene_hold.observe(latest)
                        if not was_stale:
                            audit.event("observation.stale", {
                                "tick": latest.tick, "age_ms": age * 1000,
                                "action": "release all input and wait for fresh state",
                            })
                        time.sleep(0.001)
                        continue
                    if was_stale:
                        audit.event("observation.recovered", {
                            "tick": latest.tick, "age_ms": age * 1000,
                        })
                    if latest.tick <= options.frames:
                        goal.observe(latest)
                        if scene_navigation is not None:
                            scene_navigation.observe(latest)
                        if scene_hold is not None:
                            scene_hold.observe(latest)
                    if (scene_tile_after_route and scene_navigation is None and scene_expectation is not None
                            and goal.accepted_tick is not None and latest.ready):
                        scene_navigation = SceneTileGoal(options.scene_target_tile, latest.zone, latest.room,
                                                         scene_expectation["events"][0],
                                                         options.scene_examine_quadrant)
                        audit.event("scene_navigation.bound", {
                            "tick": latest.tick, "zone": latest.zone, "room": latest.room,
                            "tile": options.scene_target_tile,
                        })
                        scene_navigation.observe(latest)
                    if isinstance(goal.door, PhysicalRoute):
                        for checkpoint in goal.door.checkpoints[route_checkpoint_offset:]:
                            audit.event("physical_door.accepted", checkpoint)
                        route_checkpoint_offset = len(goal.door.checkpoints)
                    if latest.game_over:
                        raise RuntimeError("The player was defeated")
                    if driver.held and (
                        handshake.release_due(latest)
                        or not input_lease_permitted(latest, driver.held)
                    ):
                        handshake.released(latest)
                        driver.set_buttons(frozenset())
                    input_busy = bool(driver.held) or handshake.awaiting_release(latest)
                    if future is not None and future.done():
                        decision = None
                        try:
                            decision = future.result()
                        except ModelResponseError as error:
                            audit.event("model.invalid_response", error.response)
                            invalid_responses += 1
                            validation_feedback = str(error)
                            future = None
                            if invalid_responses >= 3:
                                raise RuntimeError("Model failed three explicit validation/correction attempts") from error
                        if decision is not None:
                            invalid_responses = 0
                            validation_feedback = None
                            if observed is None:
                                raise RuntimeError("Planner response lost its observed-frame identity")
                            try:
                                controller.accept(decision, latest, observed)
                                audit.event("model.decision", asdict(decision))
                            except StaleDecision as error:
                                audit.event("model.stale_plan", {"reason": str(error), "tick": latest.tick})
                        future = None
                    if navigation_future is not None and navigation_future.done():
                        try:
                            navigation_decision = navigation_future.result()
                            if navigation_owner is None:
                                raise RuntimeError("Navigation response lost its observed route")
                            navigation_owner.accept(navigation_decision, latest)
                            audit.event("model.navigation", asdict(navigation_decision))
                            invalid_responses = 0
                        except StaleDecision as error:
                            audit.event("model.stale_navigation", {"reason": str(error), "tick": latest.tick})
                        except ModelResponseError as error:
                            audit.event("model.invalid_navigation", error.response)
                            invalid_responses += 1
                            if invalid_responses >= 3:
                                raise RuntimeError("Model failed three navigation validation attempts") from error
                        navigation_future = None
                    exit_ready = goal.exit_ready(latest)
                    scene_combat_pending = bool(
                        scene_navigation is not None
                        and goal.combat_pending(latest, (scene_navigation.zone, scene_navigation.room))
                    )
                    if exit_ready:
                        leg = goal.door.active_leg if isinstance(goal.door, PhysicalRoute) else goal.door
                        if leg is None:
                            raise RuntimeError("Door handoff lost its physical objective")
                        scope = (latest.episode, leg.identity)
                        if exit_handoff != scope:
                            audit.event("goal.post_combat_exit", {
                                "tick": latest.tick, "door": leg.identity,
                                "source_hostiles_observed_dead": len(goal.dead),
                                "contract": leg.contract,
                            })
                            exit_handoff = scope
                    if (scene_navigation is not None and future is None and navigation_future is None
                            and not driver.held and not controller.preparing
                            and not scene_combat_pending
                            and latest.tick >= setup_control_tick
                            and (not latest.enemies or (controller.decision is not None
                                                       and controller.decision.intent == "navigate"))):
                        context = scene_navigation.request(latest, avoid_hostiles=bool(latest.enemies))
                        if context is not None:
                            navigation_owner = scene_navigation
                            context["retrieved_navigation_evidence"] = store.navigation_experience(
                                latest.zone, latest.room, scene_navigation.identity,
                                scene_navigation.navigator.geometry_sha256,
                            )
                            audit.event("model.navigation_request", {
                                "tick": latest.tick, "scene_trigger": scene_navigation.result(),
                                "context": context,
                            })
                            if len(context["candidates"]) == 1:
                                # A single observed route offers no choice; skip inference latency that lets combat reset it.
                                navigation_future = Future()
                                navigation_future.set_result(NavigationDecision(
                                    next(iter(context["candidates"])), "only observed candidate route",
                                    latest.tick, 0.0,
                                ))
                            else:
                                navigation_future = planner.submit(
                                    client.navigate, latest, options.goal, context,
                                )
                    scene_pending = scene_navigation is not None and scene_navigation.pending(latest)
                    needs_plan = (controller.decision is None or now - last_request >= 2.0
                                  or urgent_recovery_needed(
                                      latest, {"max_incoming_hp_damage": goal.max_incoming_hp_damage}
                                  )
                                  or controller.needs_target_choice(latest)
                                  or ((exit_ready or (scene_pending and not scene_combat_pending))
                                      and controller.decision.intent != "navigate"))
                    player = latest.player
                    holding_scene = scene_hold is not None and scene_hold.holding(latest)
                    if (future is None and navigation_future is None
                            and not holding_scene
                            and strategy_planning_ready(latest) and player
                            and latest.tick >= setup_control_tick
                            and (latest.enemies or reward_menu_ready(latest) or exit_ready or scene_pending)
                            and not controller.preparing
                            and needs_plan and now - last_request >= 0.5
                            and (goal.accepted_tick is None or continue_after_route)):
                        observed = latest
                        position = player.position
                        nearest = min(latest.enemies, key=lambda actor: sum(
                            (actor.position[index] - position[index]) ** 2 for index in (0, 2)
                        )) if latest.enemies else None
                        experience = store.experience(nearest.model, player.weapon_category) if nearest else []
                        context = {
                            "last_decision": asdict(controller.decision) if controller.decision else None,
                            "target_choice_required": controller.needs_target_choice(latest),
                            "required_successes": {"chain": options.minimum_chains,
                                                   "defense": options.minimum_defenses},
                            "confirmed_successes": {
                                kind: sum(result["status"] == "accepted" and result["kind"] == kind
                                          for result in controller.reactions.results)
                                for kind in ("chain", "defense")
                            },
                            "recent_reactions": controller.reactions.results[-8:],
                            "verified_reaction_effects": controller.reactions.effect_results[-8:],
                            "defense_coverage_gaps": controller.reactions.coverage_gaps[-4:],
                            "controller_feedback": controller.feedback[-8:],
                            "validation_error_to_correct": validation_feedback,
                            "recent_terrain_feedback": pursuit.feedback[-6:],
                            "max_incoming_hp_damage": goal.max_incoming_hp_damage,
                            "unreachable_hostiles": controller.unreachable_hostiles(latest),
                        }
                        if options.simultaneous_defeat:
                            context["simultaneous_defeat_objective"] = {
                                "slots": list(options.simultaneous_defeat),
                                "claim": ("The original type-8 trigger needs these hostiles defeated in one "
                                          "native evaluation; one area spell must defeat all of them."),
                                "area_radii": dict(controller.area_radii),
                            }
                        if options.defeat_order:
                            context["defeat_order_objective"] = {
                                "slots": list(options.defeat_order),
                                "claim": ("Earlier-listed type-8 rows claim other defeat orders; these "
                                          "hostiles must fall one at a time in this order."),
                            }
                        if scene_navigation is not None:
                            context["source_scene_navigation_objective"] = {
                                **scene_navigation.result(),
                                "combat_clearance_required": scene_combat_pending,
                            }
                        if goal.door is not None:
                            leg = goal.door.active_leg if isinstance(goal.door, PhysicalRoute) else goal.door
                            if (leg.contract is not None and leg.arrival_tick is None
                                    and not leg.blocked_reason and not leg.waiting_for_gate
                                    and not goal.combat_pending(latest)
                                    and (latest.zone, latest.room) == (leg.source_zone, leg.source_room)):
                                target = leg.navigation_target
                                context["physical_navigation_objective"] = {
                                    "id": leg.identity, "contract": leg.contract,
                                    "claim": "Original controller traversal; living foes are not declared defeated",
                                    "phase": "post_combat_exit" if exit_ready else "physical_traversal",
                                    "next_action": "Navigate to and interact with this observed original door.",
                                }
                                if target is not leg:
                                    context["physical_navigation_objective"].update({
                                        "chest": target.result(),
                                        "next_action": ("Navigate to the observed original chest, open it and "
                                                        "claim its contents before using this door."),
                                    })
                            elif leg.waiting_for_gate:
                                context["native_gate_prerequisite"] = {
                                    "id": leg.identity, "contract": leg.contract,
                                    "instruction": (
                                        "Fight the original living hostiles and let native events finish. "
                                        "Onward navigation is unavailable until the observed flag qualifies; "
                                        "enemy death alone is not proof that this gate opened."
                                    ),
                                }
                            elif goal.combat_pending(latest):
                                context["combat_clearance_required"] = {
                                    "instruction": (
                                        "Defeat the original source-room hostiles first. "
                                        "Onward navigation remains unavailable until every observed "
                                        "source hostile has an actual zero-HP observation."
                                    ),
                                }
                        audit.event("model.request", {
                            "tick": latest.tick, "goal": options.goal,
                            "observation": latest.planning_data(), "context": context,
                            "verified_previous_outcomes": experience,
                        })
                        future = planner.submit(client.decide, latest, options.goal, experience, context)
                        last_request = now
                    if (future is None and navigation_future is None and not driver.held
                            and (not latest.enemies or (controller.decision is not None
                                                       and controller.decision.intent == "navigate"))
                            and not controller.preparing
                            and goal.door is not None and not goal.combat_pending(latest)
                            and goal.accepted_tick is None):
                        leg = goal.door.active_leg if isinstance(goal.door, PhysicalRoute) else goal.door
                        target = leg.navigation_target
                        navigation_context = target.navigation_request(latest, avoid_hostiles=bool(latest.enemies))
                        if navigation_context is not None:
                            navigation_owner = target.navigator
                            navigation_context["retrieved_navigation_evidence"] = store.navigation_experience(
                                latest.zone, latest.room, target.identity, target.navigator.geometry_sha256,
                            )
                            audit.event("model.navigation_request", {
                                "tick": latest.tick, "door": target.identity,
                                "context": navigation_context,
                            })
                            navigation_future = planner.submit(
                                client.navigate, latest, options.goal, navigation_context,
                            )
                    decision = controller.decision
                    pursuit_actor = next((actor for actor in latest.enemies
                                          if decision is not None and actor.slot == decision.target_slot), None)
                    pursuing = (decision is not None and decision.intent in ("attack", "approach")
                                and pursuit_actor is not None and not controller.preparing)
                    if pursuing and pursuit_actor is not None:
                        scope = (latest.episode, latest.zone, latest.room, pursuit_actor.slot,
                                 pursuit_actor.model, pursuit_actor.spawn_index)
                        if scope != pursuit_scope:
                            pursuit, pursuit_scope = TerrainNavigator(), scope
                    scene_movement = bool(
                        scene_navigation is not None and scene_navigation.ready(latest)
                        and (scene_navigation.navigator.path or scene_navigation.pushing(latest)
                             or (navigation_future is not None
                                 and navigation_owner is scene_navigation))
                    )
                    action = controller.step(
                        latest,
                        allow_strategy=(not input_busy
                                        and (goal.accepted_tick is None or continue_after_route)
                                        and latest.tick >= setup_control_tick
                                        and not (pursuing and pursuit.path and field_navigation_ready(latest))
                                        and not scene_movement),
                    )
                    ordinary_movement = (action is not None
                                         and action.reason == "move using current native positions and camera")
                    needs_geometry = (
                        pursuing and ordinary_movement and pursuit_actor is not None and action is not None
                        and latest.raw.get("navigation", {}).get("available")
                        and (controller.attack_reposition_required(latest)
                             or pursuit.ordinary_approach_stalled(latest, pursuit_actor, action.duration))
                    )
                    moving_to_hostile = (
                        pursuing and pursuit_actor is not None
                        and latest.raw.get("navigation", {}).get("available")
                        and field_navigation_ready(latest)
                        and not input_busy
                        and (goal.accepted_tick is None or continue_after_route)
                        and (pursuit.path or needs_geometry)
                    )
                    if moving_to_hostile and pursuit_actor is not None and not (action and action.reaction):
                        action = pursuit.step(latest)
                        just_reached = (pursuit.feedback and
                                        pursuit.feedback[-1]["status"] == "route_reached" and
                                        pursuit.feedback[-1]["tick"] == latest.tick)
                        if (not pursuit.path and future is None and navigation_future is None
                                and latest.control == 1 and latest.event_phase == 0 and not just_reached):
                            context = pursuit.request(
                                latest, (pursuit_actor.position[0], pursuit_actor.position[2]),
                                pursuit_actor.slot,
                            )
                            if context is not None:
                                navigation_owner = pursuit
                                key = f"actor:{pursuit_actor.model}:{pursuit_actor.spawn_index}"
                                context["retrieved_navigation_evidence"] = store.navigation_experience(
                                    latest.zone, latest.room, key, pursuit.geometry_sha256,
                                )
                                audit.event("model.navigation_request", {
                                    "tick": latest.tick, "target": pursuit_actor.slot,
                                    "context": context,
                                })
                                navigation_future = planner.submit(
                                    client.navigate, latest,
                                    f"Approach the observed hostile actor{pursuit_actor.slot} through "
                                    "real terrain until native attack targeting can select it.", context,
                                )
                    if (action is None and scene_navigation is not None and not input_busy
                            and not controller.preparing
                            and not scene_combat_pending
                            and (not latest.enemies or (controller.decision is not None
                                                       and controller.decision.intent == "navigate"))):
                        action = scene_navigation.step(latest)
                    if (action is None and goal.door is not None and not input_busy
                            and not controller.preparing
                            and (not latest.enemies or (controller.decision is not None
                                                       and controller.decision.intent == "navigate"))
                            and not goal.combat_pending(latest) and goal.accepted_tick is None):
                        action = goal.door.step(latest)
                    if goal.door is not None and (
                        goal.door.blocked_reason or
                        (isinstance(goal.door, PhysicalRoute) and goal.accepted_tick is not None
                         and not continue_after_route)
                    ):
                        audit.event("physical_route.finished", {
                            "tick": latest.tick, "blocked_reason": goal.door.blocked_reason,
                            "result": goal.door.result(),
                        })
                        driver.set_buttons(frozenset())
                        driver.close_window()
                        break
                    if latest.tick >= options.frames:
                        action = None
                    elif holding_scene and not (action and action.reaction):
                        if scene_hold.hold_tick is None:
                            audit.event("scene.return_hold", {
                                "tick": latest.tick, "zone": latest.zone, "room": latest.room,
                                "matched_events": scene_hold.matched,
                                "claim": ("Every expected event executed; ordinary play stops so native "
                                          "SceneTrial can observe restored control"),
                            })
                        action = scene_hold.step(latest)
                    for observation in pursuit.observations():
                        audit.event("navigation.outcome", {
                            "zone": latest.zone, "room": latest.room,
                            "door": f"actor:{pursuit_scope[-2]}:{pursuit_scope[-1]}" if pursuit_scope else "",
                            "geometry_sha256": pursuit.geometry_sha256,
                            "image_sha256": hashes, "observation": observation,
                            "claim": "Measured approach, not target eligibility or enemy defeat",
                        })
                    if goal.door is not None:
                        leg = goal.door.active_leg if isinstance(goal.door, PhysicalRoute) else goal.door
                        owners = [(leg.identity, leg.navigator)]
                        if leg.chest is not None:
                            owners.append((leg.chest.identity, leg.chest.navigator))
                        for identity, navigator in owners:
                            for observation in navigator.observations():
                                audit.event("navigation.outcome", {
                                    "zone": latest.zone, "room": latest.room, "door": identity,
                                    "geometry_sha256": navigator.geometry_sha256,
                                    "image_sha256": hashes, "observation": observation,
                                    "claim": "Measured local movement, not proof of whole-route passability",
                                })
                    if scene_navigation is not None:
                        for observation in scene_navigation.navigator.observations():
                            audit.event("navigation.outcome", {
                                "zone": latest.zone, "room": latest.room,
                                "door": scene_navigation.identity,
                                "geometry_sha256": scene_navigation.navigator.geometry_sha256,
                                "image_sha256": hashes, "observation": observation,
                                "claim": "Measured source-trigger approach; not native scene activation or completion",
                            })
                    if action is not None and (not input_busy or action.reaction):
                        driver.set_buttons(action.buttons)
                        handshake.pressed(latest, action.buttons, action.duration)
                        release_tick = latest.tick + action.duration
                        held_deadline = now + max(0.25, action.duration / 30)
                        inputs += 1
                        audit.event("controller.input", {
                            "tick": latest.tick, "buttons": sorted(action.buttons),
                            "release_tick": release_tick, "reason": action.reason,
                            "reaction": action.reaction, "observation_age_ms": age * 1000,
                        })
                    outcomes = controller.reactions.results[result_offset:]
                    for outcome in outcomes:
                        audit.event("reaction.result", outcome)
                    result_offset += len(outcomes)
                    for effect in controller.reactions.effect_results[effect_offset:]:
                        audit.event("reaction.effect", effect)
                    effect_offset = len(controller.reactions.effect_results)
                    for ending in controller.reactions.opportunity_endings[ending_offset:]:
                        audit.event("reaction.opportunity_ended", ending)
                    ending_offset = len(controller.reactions.opportunity_endings)
                    if options.strict_reactions and any(
                        result["status"] != "accepted" for result in outcomes
                    ):
                        raise RuntimeError("A reaction was missed, rejected or unconfirmed; evidence pinned")
                    if now - last_budget_check >= 15:
                        if owned_tree_size(run) > retention.run_bytes:
                            raise RuntimeError("Active run reached its log budget; nothing will be deleted")
                        last_budget_check = now
                process.wait(timeout=10)
                if reader is not None:
                    while True:
                        remaining = reader.read()
                        for frame in remaining:
                            controller.reactions.observe(frame, allow_input=False)
                            if frame.tick <= options.frames:
                                goal.observe(frame)
                                if scene_navigation is not None:
                                    scene_navigation.observe(frame)
                            latest = frame
                        if reader.at_eof:
                            break
        finally:
            if driver is not None and process.poll() is None:
                driver.set_buttons(frozenset())
                driver.close_window()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    audit.event("game.forced_shutdown", {"pid": process.pid})
            job.close()
            if reader is not None:
                reader.close()
    controller.reactions.finish()
    for outcome in controller.reactions.results[result_offset:]:
        audit.event("reaction.result", outcome)
    for effect in controller.reactions.effect_results[effect_offset:]:
        audit.event("reaction.effect", effect)
    for ending in controller.reactions.opportunity_endings[ending_offset:]:
        audit.event("reaction.opportunity_ended", ending)
    accepted = {kind: sum(item["status"] == "accepted" and item["kind"] == kind
                          for item in controller.reactions.results)
                for kind in ("chain", "defense")}
    failures = [item for item in controller.reactions.results if item["status"] != "accepted"]
    verified_effects = {
        kind: sum(item["status"] == "effect_verified" and item["kind"] == kind
                  for item in controller.reactions.effect_results)
        for kind in ("chain", "defense")
    }
    if reader is None or latest is None:
        raise RuntimeError("Game produced no supported player observations")
    if reader.pending or not reader.ended:
        raise RuntimeError("Native trace ended without complete final diagnostics")
    if reader.engine_errors:
        raise RuntimeError("Native game recorded an error during finalization")
    if options.save_file is not None:
        if options.save_file.read_bytes() != scenario_bytes:
            raise RuntimeError("The original save changed during the observation")
    complete, native_result = native_outcome(options, reader, process.returncode,
                                            scene_expectation)
    passed = (
        complete and goal.accepted_tick is not None and not goal.player_dead
        and not goal.forced_skip and not failures
        and not goal.combat_pending(latest)
        and accepted["chain"] >= options.minimum_chains
        and accepted["defense"] >= options.minimum_defenses
        and verified_effects["defense"] >= options.minimum_defenses
    )
    return {
        "passed": passed, "native_completed": complete,
        "goal_tick": goal.accepted_tick, "seen_hostiles": len(goal.seen),
        "zero_hp_hostiles": len(goal.dead), "player_dead": goal.player_dead,
        "scripted_withdrawals": [
            {"zone": key[1], "room": key[2], "slot": key[3], "spawn_index": key[4], "tick": tick}
            for key, tick in sorted(goal.withdrawn.items())
        ],
        "forced_skip": goal.forced_skip, "accepted_reactions": accepted,
        "verified_reaction_effects": verified_effects,
        "reaction_effect_outcomes": controller.reactions.effect_results,
        "reaction_failures": failures,
        "ended_reaction_opportunities": controller.reactions.opportunity_endings,
        "defense_coverage_gaps": controller.reactions.coverage_gaps,
        "reaction_coverage_demonstrated": (options.simulation_speed == 1 and all(accepted.values())
                                           and all(verified_effects.values()) and not failures),
        "simulation_speed": options.simulation_speed,
        "reaction_timing_certification_eligible": options.simulation_speed == 1,
        "inputs": inputs, "max_observation_age_ms": max_age * 1000,
        "native_result": native_result,
        "last_tick": latest.tick,
        "clear_enemies_first": options.clear_enemies_first,
        "physical_door": goal.door.result() if isinstance(goal.door, PhysicalDoor) else None,
        "physical_route": goal.door.result() if isinstance(goal.door, PhysicalRoute) else None,
        "preparation_outcomes": controller.feedback,
        "experience_contexts": [
            {"model": model, "weapon": weapon} for model, weapon in sorted(goal.encounters)
        ],
        "scenario_sha256": hashlib.sha256(scenario_bytes).hexdigest(),
        "ashley_profile": setup.get("ashley_profile"),
        "ashley_stats": setup.get("ashley_stats") or None,
        "story_progress": setup.get("story_progress"),
        "inherited_music": setup.get("inherited_music"),
        "source_setup_presses": options.setup_presses,
        "native_scene_expectation": scene_expectation,
        "scene_navigation": scene_navigation.result() if scene_navigation is not None else None,
        "model_control_from_tick": setup_control_tick,
    }


def play(paths: PlayerPaths, options: PlayOptions, retention: Retention = Retention()) -> dict:
    if options.frames <= 0 or options.maximum_trace_age <= 0:
        raise ValueError("Frame budget and freshness bounds must be positive")
    if options.minimum_chains < 0 or options.minimum_defenses < 0:
        raise ValueError("Required reaction counts cannot be negative")
    options.validate_simulation_speed()
    options.validate_scene_expectation()
    if options.new_game or options.executable.name == "VS.Desktop.exe":
        options.desktop_startup_presses()
    setup_control_tick = options.setup_control_tick()
    verify_installed_model(paths)
    store = RunStore(paths)
    cleanup = store.cleanup(retention)
    if cleanup["over_budget_pinned_evidence"]:
        store.close()
        raise RuntimeError("Pinned evidence exceeds the log budget; review it before more tests")
    run_id = store.create(options.goal, {
        "executable": str(options.executable.resolve()),
        "scenario": str(options.scenario.resolve()) if options.scenario else None,
        "save_file": str(options.save_file.resolve()) if options.save_file else None,
        "new_game": options.new_game,
        "frames": options.frames,
        "minimum_chains": options.minimum_chains,
        "minimum_defenses": options.minimum_defenses,
        "strict_reactions": options.strict_reactions,
        "simulation_speed": options.simulation_speed,
        "reaction_timing_certification_eligible": options.simulation_speed == 1,
        "door_case": options.door_case,
        "door_route": options.door_route,
        "clear_enemies_first": options.clear_enemies_first,
        "source_setup_presses": options.setup_presses,
        "scene_expectation": (str(options.scene_expectation.resolve())
                              if options.scene_expectation else None),
        "model_control_from_tick": setup_control_tick,
        "allow_stat_overrides": options.allow_stat_overrides,
        "allow_story_progress": options.allow_story_progress,
        "claim_room_chests": options.claim_room_chests,
        "setup_interaction": options.setup_interaction,
        "shared_model": options.shared_model,
        "model_port": options.model_port,
        "retention": asdict(retention),
        "model": runtime_manifest()["model"],
    })
    (store.directory(run_id) / "combat-knowledge.json").write_bytes(
        (PROJECT / "combat-knowledge.json").read_bytes()
    )
    audit = AuditWriter(paths, run_id)
    maintenance = PeriodicCleanup(paths, retention)
    summary = None
    result: dict | None = None
    status = "failed"
    try:
        server = (shared_model(options.model_port) if options.shared_model else
                  model_server(paths, options.model_port, store.directory(run_id) / "model"))
        with server as client:
            memory = warm_model(client, model_budget(options.model_parallel)
                                if options.shared_model else model_budget(1))
            memory["shared_server"] = options.shared_model
            audit.event("model.ready", memory)
            result = _play_game(store, run_id, options, client, audit, maintenance, retention)
            summary = result
        status = "passed" if summary["passed"] else "blocked"
        return summary | {"run_id": run_id, "directory": str(store.directory(run_id))}
    except KeyboardInterrupt:
        status = "cancelled"
        raise
    finally:
        try:
            maintenance.close()
        finally:
            try:
                audit.close()
            finally:
                error = sys.exception()
                if error is not None:
                    if status != "cancelled":
                        status = "failed"
                    summary = {"error": str(error), "type": type(error).__name__}
                if summary is None:
                    summary = {"error": "Run ended without a verified outcome"}
                try:
                    store.finish(run_id, status, summary)
                    if status == "passed":
                        if result is None:
                            raise RuntimeError("Passed run lost its verified result")
                        # Assisted runs must not teach the model unassisted matchups.
                        assisted = result.get("ashley_stats") or result.get("story_progress") is not None
                        contexts = [] if assisted else result["experience_contexts"]
                        for context in contexts:
                            store.remember(run_id, context["model"], context["weapon"], {
                                "run_id": run_id, "goal": options.goal,
                                "coverage": ("physical_controller_route" if options.door_route else
                                             "physical_controller_entry" if options.door_case else
                                             "combat_defeat_and_control"),
                                "seen_hostiles": result["seen_hostiles"],
                                "zero_hp_hostiles": result["zero_hp_hostiles"],
                                "scenario_sha256": result["scenario_sha256"],
                                "ashley_profile": result["ashley_profile"],
                                "simulation_speed": options.simulation_speed,
                                "reaction_timing_certification_eligible": options.simulation_speed == 1,
                                "accepted_reactions": result["accepted_reactions"],
                                "inputs": result["inputs"],
                                "goal_tick": result["goal_tick"],
                            })
                finally:
                    store.close()
