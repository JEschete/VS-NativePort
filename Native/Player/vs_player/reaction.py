from dataclasses import dataclass

from .observations import Frame
from .abilities import defense_value, offensive_value

SLOT_BUTTONS = {1: "Circle", 2: "Triangle", 3: "Square"}
SLOT_MASKS = {1: 32, 2: 16, 3: 128}


@dataclass(frozen=True)
class ReactionPress:
    slot: int
    button: str
    kind: str
    tick: int
    timer: int
    window: int
    margin: int
    action_id: int = 0
    estimated_effect_value: float = 0.0


class ReactionController:
    """Source-derived timing, independent of model inference latency."""

    def __init__(self) -> None:
        self.chain_priority = (1, 2, 3)
        self.defense_priority = (1, 2, 3)
        self.pending: ReactionPress | None = None
        self.last_signature: tuple[int, int, int] | None = None
        self.last_timer = 0
        self.issued = False
        self.opportunity_seen = False
        self.miss_recorded = False
        self.results: list[dict] = []
        self.coverage_gaps: list[dict] = []
        self.gap_recorded = False
        self.effect_probes: list[dict] = []
        self.effect_results: list[dict] = []
        self.pending_baseline: dict | None = None

    def observe_effects(self, frame: Frame) -> None:
        for probe in list(self.effect_probes):
            matching = (frame.reaction.action == probe["action_id"]
                        and frame.reaction.queue_index == probe["expected_queue"])
            if probe["seen"] and not matching:
                self.effect_results.append({
                    "status": "no_observed_effect", "kind": probe["kind"],
                    "action_id": probe["action_id"], "sent_tick": probe["tick"],
                    "observed_tick": frame.tick, "native_action_seen": True,
                })
                self.effect_probes.remove(probe)
                continue
            actor = next((actor for actor in frame.actors if actor.slot == probe["effect_target"]), None)
            if not matching and actor is not None:
                probe["before"] = getattr(actor, probe["resource"])
            probe["seen"] |= matching
            if matching:
                if actor is not None:
                    identity = (frame.episode, frame.zone, frame.room, actor.slot,
                                actor.model, actor.spawn_index)
                    if identity != probe["identity"]:
                        self.effect_results.append({
                            "status": "effect_target_changed", "kind": probe["kind"],
                            "action_id": probe["action_id"], "sent_tick": probe["tick"],
                            "observed_tick": frame.tick,
                        })
                        self.effect_probes.remove(probe)
                        continue
                    value = getattr(actor, probe["resource"])
                    if "status_mask" in probe:
                        mask = probe["status_mask"]
                        changed = ((probe["before"] & ~value) if probe["direction"] < 0
                                   else (~probe["before"] & value)) & mask
                        delta = changed.bit_count()
                    else:
                        delta = (value - probe["before"]) * probe["direction"]
                    if delta > 0:
                        self.effect_results.append({
                            "status": "effect_verified", "kind": probe["kind"],
                            "action_id": probe["action_id"], "sent_tick": probe["tick"],
                            "observed_tick": frame.tick, "target": actor.slot,
                            "resource": probe["resource"], "observed_amount": delta,
                        })
                        self.effect_probes.remove(probe)
                        continue
            if frame.tick - probe["tick"] > 300 or not frame.ready:
                self.effect_results.append({
                    "status": "no_observed_effect" if frame.ready else "effect_observation_interrupted",
                    "kind": probe["kind"], "action_id": probe["action_id"],
                    "sent_tick": probe["tick"], "observed_tick": frame.tick,
                    "native_action_seen": probe["seen"],
                })
                self.effect_probes.remove(probe)

    def priorities(self, chain: tuple[int, ...], defense: tuple[int, ...]) -> None:
        for value in (chain, defense):
            if sorted(value) != [1, 2, 3]:
                raise ValueError("Reaction priorities must each be a permutation of slots 1,2,3")
        self.chain_priority, self.defense_priority = chain, defense

    def observe(self, frame: Frame, allow_input: bool = True) -> ReactionPress | None:
        timing = frame.reaction
        if self.pending is not None:
            expected = 1 if self.pending.kind == "chain" else 2
            if timing.succeeded == expected and timing.previous_slot == self.pending.slot:
                self.results.append({
                    "status": "accepted", "kind": self.pending.kind,
                    "slot": self.pending.slot, "sent_tick": self.pending.tick,
                    "observed_tick": frame.tick,
                    "action_id": self.pending.action_id,
                    "estimated_effect_value": self.pending.estimated_effect_value,
                })
                if self.pending_baseline is not None:
                    self.effect_probes.append(self.pending_baseline)
                    self.pending_baseline = None
                self.pending = None
            elif timing.attempted and timing.succeeded == 0:
                self.results.append({
                    "status": "rejected", "kind": self.pending.kind,
                    "slot": self.pending.slot, "sent_tick": self.pending.tick,
                    "observed_tick": frame.tick,
                })
                self.pending = None
                self.pending_baseline = None
        self.observe_effects(frame)
        if not timing.active:
            if self.opportunity_seen and not self.issued and not self.miss_recorded:
                self.results.append({
                    "status": "missed" if frame.ready else "interrupted",
                    "kind": timing.kind, "observed_tick": frame.tick,
                    "reason": ("Eligible action ended before an input was delivered"
                               if frame.ready else "Observation stopped during an eligible action"),
                })
                self.miss_recorded = True
            self.last_signature = None
            return None
        signature = (timing.source_actor, timing.queue_index, timing.window)
        if signature != self.last_signature or timing.timer < self.last_timer:
            if self.pending is not None:
                self.results.append({
                    "status": "unconfirmed", "kind": self.pending.kind,
                    "slot": self.pending.slot, "sent_tick": self.pending.tick,
                    "observed_tick": frame.tick,
                })
                self.pending = None
                self.pending_baseline = None
            self.last_signature = signature
            self.issued = self.opportunity_seen = self.miss_recorded = False
            self.gap_recorded = False
        self.last_timer = timing.timer
        if timing.status != "evaluated" or timing.window == 0:
            return None
        if timing.tick_speed <= 0:
            raise ValueError("Active reaction timing has no positive native tick speed")
        priority = self.chain_priority if timing.kind == "chain" else self.defense_priority
        mapped = timing.chain_actions if timing.kind == "chain" else timing.defense_actions
        allowed = [
            slot for slot in priority
            if mapped[slot - 1] != 0 and timing.eligible[slot - 1]
            and slot != timing.previous_slot
            and not (SLOT_MASKS[slot] & timing.previous_buttons)
            and not (SLOT_MASKS[slot] & frame.held)
        ]
        values = {slot: (defense_value(frame, mapped[slot - 1])
                         if timing.kind == "defense"
                         else offensive_value(frame, mapped[slot - 1]))
                  for slot in allowed}
        allowed = [slot for slot in allowed if values[slot] > 0]
        if timing.kind == "defense":
            allowed.sort(key=lambda slot: values[slot], reverse=True)
        if (timing.kind == "defense" and timing.successful_now and not allowed
                and not self.gap_recorded and not timing.attempted and not self.issued
                and (timing.incoming_hp_damage > 0 or timing.incoming_statuses != 0)):
            self.coverage_gaps.append({
                "status": "no_effective_equipped_defense",
                "tick": frame.tick, "incoming_type": timing.incoming_type,
                "incoming_affinity": timing.incoming_affinity,
                "incoming_hp_damage": timing.incoming_hp_damage,
                "equipped": timing.defense_actions,
            })
            self.gap_recorded = True
        self.opportunity_seen |= bool(allowed)
        if (self.opportunity_seen and not self.issued and not self.miss_recorded
                and timing.timer > timing.window + timing.margin):
            self.results.append({
                "status": "missed", "kind": timing.kind, "observed_tick": frame.tick,
                "timer": timing.timer, "window": timing.window, "margin": timing.margin,
            })
            self.miss_recorded = True
        if (self.pending is not None or self.issued or timing.attempted
                or not allowed or not timing.successful_now or not allow_input):
            return None
        slot = allowed[0]
        self.pending = ReactionPress(
            slot, SLOT_BUTTONS[slot], timing.kind, frame.tick,
            timing.timer, timing.window, timing.margin,
            mapped[slot - 1], values[slot],
        )
        self.issued = True
        action_id = mapped[slot - 1]
        resource, direction = "hp", -1
        effect_target = timing.target_actor
        if timing.kind == "defense":
            effect_target = timing.source_actor if action_id in (42, 43) else 0
            direction = -1 if action_id in (42, 43) else 1
            resource = "mp" if action_id == 41 else "hp"
        elif action_id in (23, 25):
            effect_target, direction = 0, 1
            resource = "mp" if action_id == 25 else "hp"
        elif action_id in (24, 27):
            resource = "mp"
        actor = next((actor for actor in frame.actors if actor.slot == effect_target), None)
        parameters = {32: 13, 34: 15, 35: 12, 37: 14}
        status_mask = 0
        if action_id in parameters:
            resource, direction = "statuses", 1
            status_mask = frame.raw["status_masks"][parameters[action_id]]
        elif action_id == 40:
            resource, direction = "statuses", -1
            status_mask = frame.raw["status_masks"][13] | frame.raw["status_masks"][15]
        if actor is not None:
            self.pending_baseline = {
                "kind": timing.kind, "action_id": action_id, "tick": frame.tick,
                "effect_target": effect_target, "resource": resource,
                "direction": direction, "before": getattr(actor, resource), "seen": False,
                "expected_queue": timing.queue_index + 1,
                "identity": (frame.episode, frame.zone, frame.room, actor.slot,
                             actor.model, actor.spawn_index),
            }
            if status_mask:
                self.pending_baseline["status_mask"] = status_mask
        return self.pending

    def finish(self) -> None:
        if self.pending is not None:
            self.results.append({
                "status": "unconfirmed", "kind": self.pending.kind,
                "slot": self.pending.slot, "sent_tick": self.pending.tick,
            })
            self.pending = None
            self.pending_baseline = None
