"""Verify the actual first-lever handoff, not only sequence process completion."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def verify_script(directory: Path) -> dict:
    setup = json.loads((directory / "encounter.json").read_text(encoding="utf-8"))
    require(
        (setup["zone"], setup["room"], setup["room_events"]) == (9, 11, "production"),
        f"{directory}: wrong original-room setup",
    )
    require(
        setup["ashley_weapon"] == "crossbow"
        and setup["ashley_profile"] == "post-prologue"
        and setup["player"] == {"x": 6, "z": 10, "elevation": 0, "direction": 0}
        and setup["ashley_stats"] == {}
        and setup["enemies"] == []
        and "external_event" not in setup,
        f"{directory}: unexpected stats, placement override or direct event",
    )
    bat_hp = None
    bat_defeat = None
    lever = None
    exit_event = None
    map212 = None
    destination = None
    return_finished = None
    streak = 0
    longest = 0
    previous_tick = None
    allowed_buttons = 0x20 | 0x40 | 0x1000 | 0x4000 | 0x8000
    with (directory / "events.jsonl").open(encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            event = json.loads(line)
            data = event["data"]
            tick = event["tick"]
            context = f"{directory}:{line_number}"
            if event["category"] == "engine":
                require(data["level"] < 4, f"{context}: engine error: {data}")
                if "disc read: MAP/MAP212.MPD " in data["message"]:
                    map212 = tick
            if event["category"] != "frame":
                continue
            require(
                not data["input"]["held"] & ~allowed_buttons,
                f"{context}: input outside the recorded production controls",
            )
            scene = data["scene"].get("event", {})
            require(not scene.get("forced_skip", False), f"{context}: forced scene skip")
            actors = {actor["slot"]: actor for actor in data["actors"]}
            room = (data["zone"], data["room"])
            if room == (19, 0) and data["ready"] and destination is None:
                destination = tick
            active = (
                scene.get("source") == 2 and scene.get("phase", 0) != 0
                and room == (9, 11)
            )
            if active and scene["id"] == 1 and lever is None:
                lever = tick
            if active and scene["id"] == 4 and exit_event is None:
                actor = actors[0]
                require(
                    (actor["position"][0] // 128, actor["position"][2] // 128)
                    == (3, 13) and not actor["actor_flags"] & 0x104000,
                    f"{context}: authored exit was not reached naturally",
                )
                require(
                    actors[3]["hp"] == 76,
                    f"{context}: original wolf was not retained through the timed run",
                )
                exit_event = tick
            if room == (9, 11) and 4 in actors:
                hp = actors[4]["hp"]
                if bat_hp is None:
                    require(hp == 41, f"{context}: wrong original bat health")
                if bat_hp == 41 and hp == 0:
                    require(
                        data["source_actor"] == 0 and data["action"] == 1
                        and data["action_phase"] == 7,
                        f"{context}: bat defeat did not use the real attack path",
                    )
                    bat_defeat = tick
                bat_hp = hp
            returned = (
                room == (9, 12) and data["ready"] and data["control_mode"] == 1
                and data["action_phase"] == 0 and data["menu_state"] == 63
                and scene.get("phase") == 0 and 0 in actors and actors[0]["hp"] > 0
            )
            if returned and return_finished is None:
                return_finished = tick
            continuing = returned and previous_tick == tick - 1
            streak = streak + 1 if continuing else int(returned)
            longest = max(longest, streak)
            previous_tick = tick
    timings = (bat_defeat, lever, exit_event, destination, return_finished)
    require(
        all(value is not None for value in timings),
        f"{directory}: missing real attack, lever, exit, destination or return: {timings}",
    )
    require(
        all(
            first < second
            for first, second in zip(timings, timings[1:], strict=False)
        ),
        f"{directory}: incorrect authored handoff order: {timings}",
    )
    require(longest >= 120, f"{directory}: fewer than 120 consecutive normal return frames")
    return {
        "script": directory.name,
        "bat_defeat": bat_defeat,
        "lever_event": lever,
        "exit_event": exit_event,
        "map212_ready": destination,
        "physical_map212_read": map212,
        "normal_return": return_finished,
        "longest_normal_return": longest,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Combat Lab sequence run directory")
    parser.add_argument("--revision", required=True, help="Expected full source revision")
    args = parser.parse_args()
    manifest = json.loads((args.run / "manifest.json").read_text(encoding="utf-8"))
    require(
        manifest["source_status"] == "available"
        and manifest["revision"].strip() == args.revision,
        "Source revision does not match the requested evidence",
    )
    catalog = json.loads((args.run / "catalog.json").read_text(encoding="utf-8"))
    destination_rooms = [
        room for zone in catalog if zone["zone"] == 19
        for room in zone["rooms"] if room["index"] == 0
    ]
    require(
        len(destination_rooms) == 1
        and destination_rooms[0]["asset"] == "MAP/MAP212.MPD"
        and (destination_rooms[0]["width"], destination_rooms[0]["depth"]) == (32, 32),
        "Original destination identity or collision dimensions do not match MAP212",
    )
    results = json.loads((args.run / "sequence-results.json").read_text(encoding="utf-8"))
    require(results["status"] == "completed", "Sequence was not completed")
    require(len(results["scripts"]) == 2, "Expected both fresh original-room entries")
    reports = []
    for index, result in enumerate(results["scripts"], 1):
        require(
            result["status"] == "completed" and result["teardown_complete"]
            and result["engine_errors"] == 0,
            f"Script{index} was rejected, interrupted or failed teardown: {result}",
        )
        reports.append(verify_script(args.run / f"script-{index:04}"))
    print(json.dumps({
        "revision": args.revision, "worktree": manifest["worktree"],
        "run": args.run.name, "scripts": reports,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
