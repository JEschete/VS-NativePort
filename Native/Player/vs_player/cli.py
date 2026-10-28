import argparse
import json
import sys
from pathlib import Path

from .config import PlayerPaths
from .processes import model_server, prepare_model, verify_installed_model, warm_model
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
    run = commands.add_parser("play", help="Control one managed game with authoritative observations")
    run.add_argument("--game-exe", type=Path, required=True)
    run.add_argument("--data", type=Path, required=True)
    fixture = run.add_mutually_exclusive_group(required=True)
    fixture.add_argument("--scenario", type=Path)
    fixture.add_argument("--save-file", type=Path, help="Copy an unchanged native save; use production Continue")
    run.add_argument("--frames", type=int, default=3600)
    run.add_argument("--require-chains", type=int, default=0)
    run.add_argument("--require-defenses", type=int, default=0)
    run.add_argument("--goal", default=PlayOptions.goal)
    run.add_argument("--door-case", default="", help="Physically traverse original zNNN-rNNN-dNNN record")
    args = parser.parse_args()
    paths = PlayerPaths(args.state_dir) if args.state_dir else PlayerPaths.default()
    if args.command == "bootstrap":
        weights = prepare_model(paths)
        print(f"Verified local model: {weights}")
    elif args.command == "doctor":
        verify_installed_model(paths)
        with model_server(paths) as client:
            print(json.dumps(warm_model(client), indent=2))
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
        ))
        print(json.dumps(result, indent=2))
        return 0 if result["passed"] else 2
    return 0


def entrypoint() -> int:
    try:
        return main()
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print(f"Player error: {error}", file=sys.stderr)
        return 1
