import json
import hashlib
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
from .controller import CombatController, StaleDecision
from .doors import PhysicalDoor
from .model import Decision, ModelResponseError
from .observations import Frame
from .processes import ProcessJob, freeze_game, game_environment, model_server, verify_installed_model, warm_model
from .store import Retention, RunStore, owned_tree_size
from .trace import TraceReader, find_trace
from .windows import WindowInput


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


class CombatGoal:
    def __init__(self, door_case: str = "") -> None:
        self.door = PhysicalDoor(door_case) if door_case else None
        self.seen: set[tuple[int, int, int, int, int]] = set()
        self.dead: set[tuple[int, int, int, int, int]] = set()
        self.quiet_start: int | None = None
        self.accepted_tick: int | None = None
        self.player_dead = False
        self.forced_skip = False
        self.encounters: set[tuple[int, int]] = set()

    @staticmethod
    def identity(frame: Frame, actor) -> tuple[int, int, int, int, int]:
        return frame.episode, frame.zone, frame.room, actor.slot, actor.spawn_index

    def observe(self, frame: Frame) -> None:
        if self.door is not None:
            self.door.observe(frame)
            self.accepted_tick = self.door.accepted_tick
        if not frame.ready:
            return
        self.player_dead |= frame.game_over
        scene = frame.raw["scene"]
        if scene["available"]:
            self.forced_skip |= scene["event"]["forced_skip"]
        for actor in frame.enemies:
            self.seen.add(self.identity(frame, actor))
            if frame.player is not None:
                self.encounters.add((actor.model, frame.player.weapon_category))
        for actor in frame.actors:
            identity = self.identity(frame, actor)
            if identity in self.seen and actor.hp == 0:
                self.dead.add(identity)
        settled = (
            self.seen and self.seen <= self.dead and not frame.enemies
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


def _play_game(store: RunStore, run_id: str,
               options: PlayOptions, client, audit: AuditWriter,
               maintenance: PeriodicCleanup, retention: Retention) -> dict:
    run = store.directory(run_id)
    game = run / "game"
    hashes = freeze_game(options.executable, game)
    if options.scenario is None:
        if options.save_file is None:
            raise ValueError("A declared Lab scenario or an unchanged native save is required")
        scenario_bytes = options.save_file.resolve(strict=True).read_bytes()
        setup = {"ashley_profile": "continued-native-save"}
    else:
        scenario_bytes = options.scenario.resolve(strict=True).read_bytes()
        setup = json.loads(scenario_bytes)
    if setup.get("ashley_stats"):
        raise ValueError("Managed acceptance runs do not allow stat overrides")
    if setup.get("room_events") == "production" and setup.get("enemies"):
        raise ValueError("Production-room runs cannot replace original actors")
    command = [str(game / options.executable.name),
               "--data", str(options.data.resolve(strict=True)),
               "--speed", "1", "--save-dir", str(run / "private-saves")]
    if options.executable.name == "VS.CombatLab.exe" and options.scenario is not None:
        scenario = run / "scenario.json"
        scenario.write_bytes(scenario_bytes)
        sequence = run / "sequence.json"
        sequence.write_text(json.dumps({
            "version": 1, "scripts": [{"name": "external-player",
                "scenario": str(scenario), "frames": options.frames}],
        }, indent=2) + "\n", encoding="utf-8")
        command.extend(["--sequence", str(sequence)])
    elif options.executable.name == "VS.Desktop.exe" and options.save_file is not None:
        saves = run / "private-saves"
        saves.mkdir()
        (saves / "slot00.vssave").write_bytes(scenario_bytes)
        command.extend(["--frames", str(options.frames), "--press",
                        "1000:Start,1300:Circle,1450:Circle,1510:Up,1570:Circle"])
    else:
        raise ValueError("Combat Lab requires --scenario; Desktop requires --save-file")
    audit.event("game.launch", {
        "command": command, "image_sha256": hashes,
        "scenario_sha256": hashlib.sha256(scenario_bytes).hexdigest(),
    })
    controller = CombatController()
    goal = CombatGoal(options.door_case)
    latest: Frame | None = None
    reader: TraceReader | None = None
    driver: WindowInput | None = None
    release_tick = 0
    held_deadline = 0.0
    future: Future[Decision] | None = None
    observed: Frame | None = None
    last_request = 0.0
    result_offset = 0
    effect_offset = 0
    invalid_responses = 0
    validation_feedback: str | None = None
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
                        if driver.held and now > held_deadline:
                            driver.set_buttons(frozenset())
                            raise TimeoutError("Input lease expired without a fresh game frame")
                        time.sleep(0.001)
                        continue
                    for historical in frames[:-1]:
                        controller.reactions.observe(historical, allow_input=False)
                        if historical.tick <= options.frames:
                            goal.observe(historical)
                    latest = frames[-1]
                    age = reader.age(latest)
                    max_age = max(max_age, age)
                    if latest.raw["input"]["analog"]:
                        raise RuntimeError("Managed game still accepts a hardware gamepad")
                    if latest.ready and age > options.maximum_trace_age:
                        driver.set_buttons(frozenset())
                        raise TimeoutError(f"Native observation is stale ({age * 1000:.1f} ms)")
                    if latest.tick <= options.frames:
                        goal.observe(latest)
                    if latest.game_over:
                        raise RuntimeError("The player was defeated")
                    if driver.held and (
                        latest.tick >= release_tick or not latest.ready
                        or latest.control not in (1, 2, 5)
                    ):
                        driver.set_buttons(frozenset())
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
                    needs_plan = controller.decision is None or now - last_request >= 2.0
                    player = latest.player
                    if (future is None and latest.ready and player and latest.enemies
                            and not controller.preparing
                            and needs_plan and now - last_request >= 0.5
                            and goal.accepted_tick is None):
                        observed = latest
                        position = player.position
                        nearest = min(latest.enemies, key=lambda actor: sum(
                            (actor.position[index] - position[index]) ** 2 for index in (0, 2)
                        ))
                        experience = store.experience(nearest.model, player.weapon_category)
                        context = {
                            "last_decision": asdict(controller.decision) if controller.decision else None,
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
                        }
                        audit.event("model.request", {
                            "tick": latest.tick, "goal": options.goal,
                            "observation": latest.planning_data(), "context": context,
                            "verified_previous_outcomes": experience,
                        })
                        future = planner.submit(client.decide, latest, options.goal, experience, context)
                        last_request = now
                    action = controller.step(
                        latest,
                        allow_strategy=not driver.held and goal.accepted_tick is None,
                    )
                    if (action is None and goal.door is not None and not driver.held
                            and not controller.preparing and not latest.enemies
                            and goal.accepted_tick is None):
                        action = goal.door.step(latest)
                    if latest.tick >= options.frames:
                        action = None
                    if action is not None and (not driver.held or action.reaction):
                        driver.set_buttons(action.buttons)
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
        native_result = {
            "coverage": "production_continue_controller_observation",
            "host_exit": reader.host_exit,
            "trace_finalized": reader.ended,
            "engine_errors": len(reader.engine_errors),
            "game_finished": bool(reader.host_exit and reader.host_exit.get("game_finished")),
        }
        complete = process.returncode == 0 and reader.host_exit is not None and reader.ended
    else:
        checkpoint = reader.path.parent / "sequence-results.json"
        native = json.loads(checkpoint.read_text(encoding="utf-8"))
        scripts = native["scripts"]
        complete = (
            process.returncode == 0 and len(scripts) == 1
            and scripts[0]["status"] == "completed" and scripts[0]["teardown_complete"]
            and scripts[0]["engine_errors"] == 0 and reader.ended
        )
        native_result = scripts[0] if scripts else None
    passed = (
        complete and goal.accepted_tick is not None and not goal.player_dead
        and not goal.forced_skip and not failures
        and accepted["chain"] >= options.minimum_chains
        and accepted["defense"] >= options.minimum_defenses
        and verified_effects["defense"] >= options.minimum_defenses
    )
    return {
        "passed": passed, "native_completed": complete,
        "goal_tick": goal.accepted_tick, "seen_hostiles": len(goal.seen),
        "zero_hp_hostiles": len(goal.dead), "player_dead": goal.player_dead,
        "forced_skip": goal.forced_skip, "accepted_reactions": accepted,
        "verified_reaction_effects": verified_effects,
        "reaction_effect_outcomes": controller.reactions.effect_results,
        "reaction_failures": failures,
        "defense_coverage_gaps": controller.reactions.coverage_gaps,
        "reaction_coverage_demonstrated": all(accepted.values()) and all(verified_effects.values()) and not failures,
        "inputs": inputs, "max_observation_age_ms": max_age * 1000,
        "native_result": native_result,
        "last_tick": latest.tick,
        "physical_door": goal.door.result() if goal.door else None,
        "preparation_outcomes": controller.feedback,
        "experience_contexts": [
            {"model": model, "weapon": weapon} for model, weapon in sorted(goal.encounters)
        ],
        "scenario_sha256": hashlib.sha256(scenario_bytes).hexdigest(),
        "ashley_profile": setup.get("ashley_profile"),
    }


def play(paths: PlayerPaths, options: PlayOptions, retention: Retention = Retention()) -> dict:
    if options.frames <= 0 or options.maximum_trace_age <= 0:
        raise ValueError("Frame budget and freshness bounds must be positive")
    if options.minimum_chains < 0 or options.minimum_defenses < 0:
        raise ValueError("Required reaction counts cannot be negative")
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
        "frames": options.frames,
        "minimum_chains": options.minimum_chains,
        "minimum_defenses": options.minimum_defenses,
        "strict_reactions": options.strict_reactions,
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
        with model_server(paths, options.model_port, store.directory(run_id) / "model") as client:
            memory = warm_model(client)
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
                        for context in result["experience_contexts"]:
                            store.remember(run_id, context["model"], context["weapon"], {
                                "run_id": run_id, "goal": options.goal,
                                "scenario_sha256": result["scenario_sha256"],
                                "ashley_profile": result["ashley_profile"],
                                "accepted_reactions": result["accepted_reactions"],
                                "inputs": result["inputs"],
                                "goal_tick": result["goal_tick"],
                            })
                finally:
                    store.close()
