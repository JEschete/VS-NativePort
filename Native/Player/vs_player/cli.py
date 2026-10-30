import argparse
import json
import sys
import time
from pathlib import Path

from .config import PlayerPaths
from .processes import model_budget, model_server, prepare_model, verify_installed_model, warm_model
from .runner import PlayOptions, play
from .store import Retention, RunStore


def main() -> int:
    parser = argparse.ArgumentParser(description="Standalone native gameplay test player")
    parser.add_argument("--state-dir", type=Path)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("bootstrap", help="Install the checksum-pinned portable runtime and model")
    commands.add_parser("doctor", help="Verify local inference and conservative GPU use")
    commands.add_parser("cleanup", help="Prune only eligible owned logs; preserve pinned failures")
    pin = commands.add_parser("pin", help="Preserve a specific acceptance/failure run across cleanup")
    pin.add_argument("--run-id", required=True)
    serve = commands.add_parser("serve", help="Hold one warmed local model for up to four parallel games")
    serve.add_argument("--model-port", type=int, default=11551)
    serve.add_argument("--parallel", type=int, choices=(1, 2, 3, 4), default=4)
    run = commands.add_parser("play", help="Control one managed game with authoritative observations")
    run.add_argument("--game-exe", type=Path, required=True)
    run.add_argument("--data", type=Path, required=True)
    fixture = run.add_mutually_exclusive_group(required=True)
    fixture.add_argument("--scenario", type=Path)
    fixture.add_argument("--save-file", type=Path, help="Copy an unchanged native save; use production Continue")
    fixture.add_argument("--new-game", action="store_true",
                         help="Use Desktop New Game with private saves and the original full prologue")
    run.add_argument("--frames", type=int, default=3600)
    run.add_argument("--speed", type=int, choices=(1, 2, 4), default=1,
                     help="Simulation rate; accelerated Lab play requires --scene-investigation")
    run.add_argument("--scene-investigation", action="store_true",
                     help="Keep ordinary reaction misses as nonpasses; never certify accelerated timing")
    run.add_argument("--require-chains", type=int, default=0)
    run.add_argument("--require-defenses", type=int, default=0)
    run.add_argument("--goal", default=PlayOptions.goal)
    run.add_argument("--clear-enemies-first", action="store_true",
                     help="Require observed source-room enemy deaths before door or scene-tile navigation")
    run.add_argument("--setup-press", default="",
                     help="Direction-only native source-fixture pulses before model strategy (Lab only)")
    run.add_argument("--scene-expectation", type=Path,
                     help="Original Lab scene expectation JSON; native playback/control completes the case")
    run.add_argument("--scene-target-tile", nargs=2, type=int, metavar=("X", "Z"),
                     help="Use observed terrain to cross one original local-scene trigger tile")
    run.add_argument("--scene-examine-quadrant", type=int, choices=(0, 1, 2, 3),
                     help="Treat --scene-target-tile as a type-4 stand tile: face this native quadrant "
                          "(0 -Z, 1 -X, 2 +Z, 3 +X) and examine with ordinary Cross")
    run.add_argument("--scene-follow-tile", nargs=2, type=int, metavar=("X", "Z"),
                     help="After the first scene event returns control, cross this tile for the second event")
    run.add_argument("--scene-follow-examine-quadrant", type=int, choices=(0, 1, 2, 3),
                     help="Treat --scene-follow-tile as a type-4 stand tile facing this native quadrant")
    run.add_argument("--simultaneous-defeat", nargs="+", type=int, metavar="SLOT",
                     help="Original type-8 trigger needs these hostile slots defeated in one evaluation: "
                          "only cast observed area damage spells whose native preview contains all of them")
    run.add_argument("--defeat-order", nargs="+", type=int, metavar="SLOT",
                     help="Original type-8 rows claim other defeat orders: harm only the first living "
                          "listed hostile slot, one at a time, without area spells")
    run.add_argument("--allow-stat-overrides", action="store_true",
                     help="Accept declared Lab ashley_stats as a recorded owner-approved assist")
    run.add_argument("--allow-story-progress", action="store_true",
                     help="Accept a declared Lab story_progress (flag 0x82) as a recorded owner-approved assist")
    run.add_argument("--claim-room-chests", action="store_true",
                     help="Open and claim the source room's unlocked original chest before the door route")
    run.add_argument("--setup-interaction", action="store_true",
                     help="Also accept lone ordinary Cross setup pulses for an original local/external trigger")
    run.add_argument("--model-port", type=int, default=11551)
    run.add_argument("--run-log-mb", type=int, default=Retention.run_bytes // (1024 * 1024),
                     help="Per-run log budget; actor-dense rooms log more frame data per tick")
    run.add_argument("--shared-model", type=int, choices=(1, 2, 3, 4), metavar="PARALLEL",
                     help="Borrow a running `serve` model with this many slots instead of owning one")
    doors = run.add_mutually_exclusive_group()
    doors.add_argument("--door-case", default="", help="Physically traverse original zNNN-rNNN-dNNN record")
    doors.add_argument("--door-route", nargs="+", help="Traverse these original records in one continuous game")
    args = parser.parse_args()
    paths = PlayerPaths(args.state_dir) if args.state_dir else PlayerPaths.default()
    if args.command == "bootstrap":
        weights = prepare_model(paths)
        print(f"Verified local model: {weights}")
    elif args.command == "doctor":
        verify_installed_model(paths)
        with model_server(paths) as client:
            print(json.dumps(warm_model(client), indent=2))
    elif args.command == "serve":
        verify_installed_model(paths)
        with model_server(paths, args.model_port, parallel=args.parallel) as client:
            print(json.dumps(warm_model(client, model_budget(args.parallel)) | {
                "port": args.model_port, "parallel": args.parallel}), flush=True)
            try:
                while True:
                    time.sleep(240)
                    # Keep the shared weights resident between game runs.
                    warm_model(client, model_budget(args.parallel))
            except KeyboardInterrupt:
                pass
    elif args.command == "cleanup":
        store = RunStore(paths)
        try:
            report = store.cleanup(Retention())
            print(json.dumps(report, indent=2))
            return int(report["over_budget_pinned_evidence"])
        finally:
            store.close()
    elif args.command == "pin":
        store = RunStore(paths)
        try:
            store.pin(args.run_id)
            print(f"Pinned evidence: {args.run_id}")
        finally:
            store.close()
    elif args.command == "play":
        result = play(paths, PlayOptions(
            args.game_exe, args.data, args.scenario, args.frames, args.goal,
            args.require_chains, args.require_defenses, door_case=args.door_case,
            save_file=args.save_file,
            door_route=tuple(args.door_route or ()),
            clear_enemies_first=args.clear_enemies_first,
            setup_presses=args.setup_press,
            new_game=args.new_game,
            simulation_speed=args.speed,
            strict_reactions=not args.scene_investigation,
            scene_expectation=args.scene_expectation,
            scene_target_tile=(args.scene_target_tile[0], args.scene_target_tile[1])
            if args.scene_target_tile is not None else None,
            scene_examine_quadrant=args.scene_examine_quadrant,
            scene_follow_tile=(args.scene_follow_tile[0], args.scene_follow_tile[1])
            if args.scene_follow_tile is not None else None,
            scene_follow_examine_quadrant=args.scene_follow_examine_quadrant,
            simultaneous_defeat=tuple(args.simultaneous_defeat or ()),
            defeat_order=tuple(args.defeat_order or ()),
            allow_stat_overrides=args.allow_stat_overrides,
            allow_story_progress=args.allow_story_progress,
            claim_room_chests=args.claim_room_chests,
            setup_interaction=args.setup_interaction,
            model_port=args.model_port,
            shared_model=args.shared_model is not None,
            model_parallel=args.shared_model or 1,
        ), Retention(run_bytes=args.run_log_mb * 1024 * 1024))
        print(json.dumps(result, indent=2))
        return 0 if result["passed"] else 2
    return 0


def entrypoint() -> int:
    try:
        return main()
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print(f"Player error: {error}", file=sys.stderr)
        return 1
