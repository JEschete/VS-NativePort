from dataclasses import dataclass
from typing import Any


def integer(value: object, field: str, minimum: int, maximum: int) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{field}: expected integer in [{minimum}, {maximum}]")
    return value


def boolean(value: object, field: str) -> bool:
    if type(value) is not bool:
        raise ValueError(f"{field}: expected boolean")
    return value


def triple(value: object, field: str, minimum: int, maximum: int) -> tuple[int, int, int]:
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError(f"{field}: expected three components")
    return (
        integer(value[0], field, minimum, maximum),
        integer(value[1], field, minimum, maximum),
        integer(value[2], field, minimum, maximum),
    )


@dataclass(frozen=True)
class Actor:
    slot: int
    model: int
    hp: int
    max_hp: int
    mp: int
    risk: int
    position: tuple[int, int, int]
    weapon_drawn: bool
    weapon_category: int
    range_components: tuple[int, int, int]
    range_shape: int
    range_angle: int
    native_extent: int
    hostile: bool
    relation: int
    spawn_index: int
    statuses: int = 0
    status_immunities: int = 0
    spell_action_ids: tuple[int, ...] = ()
    base_attack_type: int = 0
    shield_pp: int = 0
    max_mp: int = 0
    weapon_pp: int = 0
    actor_flags: int = 0

    @classmethod
    def decode(cls, data: dict[str, Any]) -> "Actor":
        reach = data["weapon_range"]
        return cls(
            integer(data["slot"], "actor.slot", 0, 15),
            integer(data["model"], "actor.model", -1, 65535),
            integer(data["hp"], "actor.hp", -32768, 32767),
            integer(data["max_hp"], "actor.max_hp", 0, 32767),
            integer(data["mp"], "actor.mp", -32768, 32767),
            integer(data["risk"], "actor.risk", 0, 65535),
            triple(data["position"], "actor.position", -(2**31), 2**31 - 1),
            bool(integer(data["weapon_drawn"], "actor.weapon_drawn", 0, 255)),
            integer(data["weapon_category"], "actor.weapon_category", 0, 255),
            triple(reach["components_world"], "actor.weapon_range", 0, 8160),
            integer(reach["shape"], "actor.range_shape", 0, 7),
            integer(reach["angle_steps"], "actor.range_angle", 0, 31),
            integer(reach["native_extent_world"], "actor.native_extent", 0, 8160),
            boolean(data["hostile_to_player"], "actor.hostile"),
            integer(data["relation_to_player"], "actor.relation", -1, 3),
            integer(data["spawn_index"], "actor.spawn_index", 0, 255),
            integer(data["statuses"], "actor.statuses", 0, 2**32-1),
            integer(data["status_immunities"], "actor.status_immunities", 0, 2**32-1),
            tuple(integer(action, "actor.spell_action", 0, 255)
                  for action in data["spell_action_ids"]),
            integer(data["base_attack_type"], "actor.base_attack_type", 0, 7),
            integer(data["shield_pp"], "actor.shield_pp", 0, 65535),
            integer(data["max_mp"], "actor.max_mp", 0, 32767),
            integer(data["weapon_pp"], "actor.weapon_pp", 0, 65535),
            integer(data["actor_flags"], "actor.flags", 0, 2**32-1),
        )


@dataclass(frozen=True)
class ReactionTiming:
    active: bool
    status: str
    source_actor: int
    target_actor: int
    action: int
    queue_index: int
    chain_step: int
    timer: int
    window: int
    tick_speed: int
    story_timing: bool
    previous_slot: int
    previous_buttons: int
    attempted: bool
    succeeded: int
    chain_actions: tuple[int, int, int]
    defense_actions: tuple[int, int, int]
    eligible: tuple[bool, bool, bool]
    incoming_type: int = 0
    incoming_affinity: int = -1
    incoming_cost: int = 0
    incoming_hp_damage: int = 0
    incoming_mp_damage: int = 0
    incoming_statuses: int = 0

    @classmethod
    def decode(cls, data: dict[str, Any]) -> "ReactionTiming":
        if not isinstance(data["eligibility_status"], str):
            raise ValueError("reaction eligibility status is missing")
        flags = data["eligible"]
        if not isinstance(flags, list) or len(flags) != 3:
            raise ValueError("reaction.eligible must contain three native decisions")
        return cls(
            boolean(data["active"], "reaction.active"),
            data["eligibility_status"],
            integer(data["source_actor"], "reaction.source", -1, 15),
            integer(data["target_actor"], "reaction.target", -1, 15),
            integer(data["action"], "reaction.action", -1, 65535),
            integer(data["queue_index"], "reaction.queue_index", 0, 2**32 - 1),
            integer(data["chain_step"], "reaction.chain_step", 0, 2**32 - 1),
            integer(data["timer"], "reaction.timer", 0, 65535),
            integer(data["window"], "reaction.window", 0, 65535),
            integer(data["tick_speed"], "reaction.tick_speed", 0, 65535),
            boolean(data["story_timing"], "reaction.story_timing"),
            integer(data["previous_slot"], "reaction.previous_slot", 0, 255),
            integer(data["previous_buttons"], "reaction.previous_buttons", 0, 255),
            boolean(data["attempted"], "reaction.attempted"),
            integer(data["succeeded"], "reaction.succeeded", 0, 255),
            triple(data["chain_actions"], "reaction.chain_actions", 0, 255),
            triple(data["defense_actions"], "reaction.defense_actions", 0, 255),
            (boolean(flags[0], "reaction.eligible"),
             boolean(flags[1], "reaction.eligible"),
             boolean(flags[2], "reaction.eligible")),
            integer(data["incoming_type"], "reaction.incoming_type", 0, 7),
            integer(data["incoming_affinity"], "reaction.incoming_affinity", -1, 255),
            integer(data["incoming_cost"], "reaction.incoming_cost", 0, 255),
            integer(data["incoming_hp_damage"], "reaction.incoming_hp_damage", 0, 2**31-1),
            integer(data["incoming_mp_damage"], "reaction.incoming_mp_damage", 0, 2**31-1),
            integer(data["incoming_statuses"], "reaction.incoming_statuses", 0, 2**32-1),
        )

    @property
    def kind(self) -> str:
        return "chain" if self.source_actor == 0 else "defense"

    @property
    def margin(self) -> int:
        # Battle_UpdateBattleAbilityTiming, original BATTLE/146C.c:7147.
        initial = 8 if self.story_timing else 10
        if self.source_actor != 0 and self.action >= 54:
            initial += 4
        return max(2, initial - self.queue_index // 3)

    @property
    def successful_now(self) -> bool:
        return self.window > 0 and self.window < self.timer <= self.window + self.margin


@dataclass(frozen=True)
class Frame:
    tick: int
    elapsed_us: int
    episode: int
    ready: bool
    zone: int
    room: int
    control: int
    action_phase: int
    source_actor: int
    game_over: bool
    camera_yaw: int | None
    event_phase: int | None
    dialogue_waiting: bool
    held: int
    actors: tuple[Actor, ...]
    reaction: ReactionTiming
    raw: dict[str, Any]

    @classmethod
    def decode(cls, record: dict[str, Any]) -> "Frame":
        if record["category"] != "frame":
            raise ValueError("Expected a frame record")
        data = record["data"]
        if data.get("player_schema") != 1:
            raise ValueError("Game lacks authoritative player telemetry schema 1; rebuild the host")
        if not isinstance(data["actors"], list) or len(data["actors"]) > 16:
            raise ValueError("Invalid native actor snapshot list")
        actors = tuple(Actor.decode(actor) for actor in data["actors"])
        if len({actor.slot for actor in actors}) != len(actors):
            raise ValueError("Duplicate actor slots in native telemetry")
        scene = data["scene"]
        available = boolean(scene["available"], "scene.available")
        camera = integer(scene["camera"]["yaw"], "camera.yaw", -(2**31), 2**31 - 1) if available else None
        event = scene["event"] if available else None
        phase = integer(event["phase"], "event.phase", 0, 255) if event else None
        waiting = False
        if event:
            waits = event["dialogue_waits"]
            if not isinstance(waits, list) or len(waits) != 8:
                raise ValueError("Expected the eight logical dialogue wait flags")
            waiting = any(boolean(value, "dialogue_wait") for value in waits)
        return cls(
            integer(record["tick"], "tick", 0, 2**64 - 1),
            integer(record["elapsed_us"], "elapsed_us", 0, 2**63 - 1),
            integer(record.get("script", 0), "script", 0, 2**32 - 1),
            boolean(data["ready"], "ready"),
            integer(data["zone"], "zone", 0, 65535),
            integer(data["room"], "room", 0, 65535),
            integer(data["control_mode"], "control", 0, 2**32 - 1),
            integer(data["action_phase"], "action_phase", 0, 2**32 - 1),
            integer(data["source_actor"], "source_actor", -1, 15),
            boolean(data["game_over"], "game_over"), camera, phase, waiting,
            integer(data["input"]["held"], "input.held", 0, 2**32 - 1),
            actors, ReactionTiming.decode(data["reaction_timing"]), data,
        )

    @property
    def player(self) -> Actor | None:
        return next((actor for actor in self.actors if actor.slot == 0), None)

    @property
    def enemies(self) -> tuple[Actor, ...]:
        incoming = self.reaction.source_actor if self.reaction.active and self.reaction.target_actor == 0 else -1
        return tuple(actor for actor in self.actors
                     if actor.slot != 0 and actor.hp > 0
                     and (actor.hostile or actor.slot == incoming))

    def planning_data(self) -> dict[str, Any]:
        def describe(actor: Actor) -> dict[str, Any]:
            return {
                "slot": actor.slot, "model": actor.model, "hp": actor.hp,
                "max_hp": actor.max_hp, "mp": actor.mp, "risk": actor.risk,
                "position_world": actor.position, "weapon_drawn": actor.weapon_drawn,
                "weapon_category": actor.weapon_category,
                "range_components_world": actor.range_components,
                "range_shape": actor.range_shape, "range_angle_steps": actor.range_angle,
                "native_range_extent_world": actor.native_extent,
                "hostile": actor.hostile, "relation": actor.relation,
                "statuses": actor.statuses, "status_immunities": actor.status_immunities,
                "spell_action_ids": actor.spell_action_ids,
                "base_attack_type": actor.base_attack_type,
                "shield_pp": actor.shield_pp, "max_mp": actor.max_mp,
                "weapon_pp": actor.weapon_pp,
            }
        return {
            "tick": self.tick, "zone": self.zone, "room": self.room,
            "control": self.control, "event_phase": self.event_phase,
            "player": describe(self.player) if self.player else None,
            "enemies": [describe(actor) for actor in self.enemies],
            "targets": self.raw["target_previews"][
                self.raw["target_page"]["first"]:
                self.raw["target_page"]["first"] + self.raw["target_page"]["visible"]
            ],
            "target_page": self.raw["target_page"],
            "chain_actions_by_slot": self.reaction.chain_actions,
            "defense_actions_by_slot": self.reaction.defense_actions,
            "active_reaction": {
                "active": self.reaction.active, "kind": self.reaction.kind,
                "action": self.reaction.action,
                "source_actor": self.reaction.source_actor,
                "target_actor": self.reaction.target_actor,
            },
            "learned_actions": self.raw["learned_actions"],
            "action_options_columns": ["id", "name", "family", "cost", "restrictions"],
            "action_options": [[option[key] for key in ("id", "name", "family", "cost", "restrictions")]
                               for option in self.raw["action_options"]],
            "menu": self.raw["menu"],
            "status_masks": self.raw["status_masks"],
        }
