import math
from dataclasses import dataclass

from .model import Decision
from .menus import MenuTransaction
from .observations import Actor, Frame
from .reaction import ReactionController
from .choices import loot_menu_ready, native_target_menu_ready, reward_menu_ready


@dataclass(frozen=True)
class InputAction:
    buttons: frozenset[str]
    duration: int
    reason: str
    reaction: bool = False


class StaleDecision(ValueError):
    pass


def input_lease_permitted(frame: Frame, buttons: frozenset[str]) -> bool:
    return frame.ready and (
        frame.control in (1, 2, 5, 6)
        or (frame.control == 3 and frame.dialogue_waiting
            and buttons == frozenset({"Circle"}))
        # GameLoop control 11 only polls Menu_IsSceneNotificationPending; a face button dismisses it.
        or (frame.control == 11 and buttons == frozenset({"Circle"}))
        or (loot_menu_ready(frame) and buttons == frozenset({"Square"}))
        or (reward_menu_ready(frame) and buttons in (
            frozenset({"Circle"}), frozenset({"Up"}), frozenset({"Down"})))
    )


def strategy_planning_ready(frame: Frame) -> bool:
    return (frame.ready and not frame.game_over
            and (reward_menu_ready(frame)
                 or (frame.event_phase == 0 and frame.control in (1, 2, 5, 6)
                     and not frame.dialogue_waiting)))


def direction_buttons(dx: int, dz: int, yaw: int) -> frozenset[str]:
    if dx == 0 and dz == 0:
        return frozenset()
    angle = (yaw % 4096) * math.tau / 4096
    down = dx * math.sin(angle) + dz * math.cos(angle)
    right = -dx * math.cos(angle) + dz * math.sin(angle)
    cutoff = math.hypot(dx, dz) * math.sin(math.pi / 8)
    buttons = set()
    if down > cutoff:
        buttons.add("Down")
    elif down < -cutoff:
        buttons.add("Up")
    if right > cutoff:
        buttons.add("Right")
    elif right < -cutoff:
        buttons.add("Left")
    return frozenset(buttons)


class CombatController:
    max_area_target_confirmations = 12
    max_area_steps = 48
    max_area_retries = 3
    unreachable_rejections = 3
    unreachable_window = 1800

    def __init__(self) -> None:
        self.reactions = ReactionController()
        self.decision: Decision | None = None
        self.identity: tuple[int, int, int, int, int] | None = None
        self.last_action_tick = -100
        # Native ticks per host frame (--speed); posted keys shorter than one host frame are lost.
        self.input_ticks = 1
        self.target_wait_started: int | None = None
        self.feedback: list[dict] = []
        self.transaction: MenuTransaction | None = None
        self.rejected_positions: dict[tuple[int, int, int, int, int], tuple[int, int, int]] = {}
        self.rejection_history: dict[tuple[int, int, int, int, int],
                                     list[tuple[int, tuple[int, int, int], tuple[int, int, int]]]] = {}
        # Hostiles whose observed terrain offered no supported route: (tick, hostile position).
        self.terrain_blocked: dict[tuple[int, int, int, int, int], tuple[int, tuple[int, int, int]]] = {}
        self.decision_scene: tuple[int, int, int] | None = None
        self.area_target_confirmations = 0
        self.area_target_confirmation_tick: int | None = None
        self.area_steer_reset()
        # Hostile slots an original type-8 trigger needs defeated by the same area action.
        self.simultaneous_defeat: tuple[int, ...] = ()
        # Native area volume radius observed per action while positioning.
        self.area_radii: dict[int, float] = {}

    def area_defeat_gaps(self, frame: Frame) -> list[int]:
        """Required living slots the native area preview misses or cannot defeat outright."""
        living = {actor.slot: actor for actor in frame.actors if actor.slot != 0 and actor.hp > 0}
        listed = {entry["actor"] for entry in frame.raw["target_previews"]}
        damage: dict[int, int] = {}
        for forecast in frame.raw.get("target_forecasts", []):
            if forecast.get("effect_id") == 58 and isinstance(forecast.get("amount"), int):
                damage[forecast["actor"]] = max(damage.get(forecast["actor"], 0), forecast["amount"])
        return [slot for slot in self.simultaneous_defeat if slot in living
                and (slot not in listed or damage.get(slot, 0) < living[slot].hp)]

    @property
    def preparing(self) -> bool:
        return self.transaction is not None and not self.transaction.done

    def needs_target_choice(self, frame: Frame) -> bool:
        decision = self.decision
        return (decision is not None and decision.intent == "attack"
                and decision.target_limb < 0 and native_target_menu_ready(frame)
                and len({target["limb"] for target in frame.raw["target_previews"]
                         if target["actor"] == decision.target_slot and target["limb"] >= 0}) > 1)

    @staticmethod
    def target_identity(frame: Frame, actor: Actor) -> tuple[int, int, int, int, int]:
        return frame.episode, frame.zone, frame.room, actor.model, actor.spawn_index

    def accept(self, decision: Decision, frame: Frame, observed: Frame) -> None:
        if not strategy_planning_ready(frame):
            raise StaleDecision("The original native scene owns strategic control")
        if frame.tick - decision.observation_tick > 600:
            raise StaleDecision("Model decision exceeded its semantic-plan lifetime")
        if (frame.episode, frame.zone, frame.room) != (
            observed.episode, observed.zone, observed.room
        ):
            raise StaleDecision("The observed scene changed during planning")
        if decision.intent == "reward" and (
            not reward_menu_ready(frame)
            or frame.raw["menu"]["rows"] != observed.raw["menu"]["rows"]
        ):
            raise StaleDecision("The earned reward menu changed during planning")
        actor = next((actor for actor in frame.enemies
                      if actor.slot == decision.target_slot), None)
        original = next((actor for actor in observed.enemies
                         if actor.slot == decision.target_slot), None)
        if decision.target_slot > 0 and (
            actor is None or original is None
            or self.target_identity(frame, actor) != self.target_identity(observed, original)
        ):
            raise StaleDecision("Model target changed during planning")
        if decision.intent == "cast" and not any(
            option["id"] == decision.action_id and option["restrictions"] & ~1 == 0
            for option in frame.raw["action_options"]
        ):
            raise StaleDecision("Native spell availability changed during model inference")
        if decision.intent == "item" and not any(
            option["id"] == decision.item_id and option["action_id"] == decision.action_id
            and option["count"] > 0 and option["restrictions"] & 2 == 0
            for option in frame.raw["item_options"]
        ):
            raise StaleDecision("Owned consumable availability changed during model inference")
        if (decision.intent == "attack" and decision.target_limb >= 0
                and native_target_menu_ready(observed)
                and (not native_target_menu_ready(frame) or not any(
                    target["actor"] == decision.target_slot
                    and target["limb"] == decision.target_limb
                    for target in frame.raw["target_previews"]))):
            raise StaleDecision("The observed native target limb changed during planning")
        transaction = None
        if decision.intent in ("cast", "item", "equip_chain", "equip_defense"):
            transaction = MenuTransaction(decision, frame)
        if self.preparing:
            raise StaleDecision("An observed menu transaction is still in progress")
        self.decision = decision
        self.decision_scene = (frame.episode, frame.zone, frame.room)
        self.identity = self.target_identity(frame, actor) if actor else None
        self.reactions.priorities(decision.chain_priority, decision.defense_priority)
        if transaction is not None:
            self.transaction = transaction
        self.area_target_confirmations = 0
        self.area_target_confirmation_tick = None
        self.area_steer_reset()

    def area_steer_reset(self) -> None:
        self.area_steps = 0
        self.area_stalls = 0
        self.area_best = math.inf
        self.area_yaw: int | None = None
        self.area_quantum: float | None = None
        self.area_last: tuple[tuple[int, int], tuple[int, int]] | None = None
        self.area_retries = 0
        self.area_force_step = False

    def area_goal_slots(self, frame: Frame, decision: Decision) -> set[int]:
        if decision.target_slot in self.simultaneous_defeat:
            # A required slot that is alive but not hostile at this instant is still required.
            living = {actor.slot for actor in frame.actors if actor.slot != 0 and actor.hp > 0}
            required = {slot for slot in self.simultaneous_defeat if slot in living}
            if required:
                return required
        return {decision.target_slot}

    @staticmethod
    def area_radius(frame: Frame) -> float | None:
        """Horizontal radius of the native area volume on the cursor, if observed.

        BATTLE.PRG D_800EB9B8 volumes[0] is the cast range around the caster; volumes[1]
        follows the positioning cursor as the action area."""
        center = frame.raw.get("area_center")
        for volume in frame.raw.get("area_volumes", [])[1:]:
            position = volume["position"]
            if (center is not None and abs(position[0] - center[0]) <= 2
                    and abs(position[2] - center[2]) <= 2 and volume["half_x"] > 0):
                return float(min(volume["half_x"], volume["half_z"]))
        return None

    def _area_give_up(self, frame: Frame, decision: Decision, reason: str,
                      status: str = "area_out_of_range", **details) -> InputAction:
        player = frame.player
        self.feedback.append({"kind": "area_target_unreachable", "status": status,
                              "tick": frame.tick, "action_id": decision.action_id,
                              "target": decision.target_slot,
                              "area_center": list(frame.raw["area_center"][:3]),
                              "player_position": player.position if player else None,
                              "steps": self.area_steps, "retries": self.area_retries,
                              **details})
        if self.transaction is not None and self.preparing:
            self.transaction.closing = True
            self.transaction.result = {"reason": "Native area sphere cannot cover the target"}
        else:
            self.decision = None
        return self._action(frame, ("Cross",), 2, reason)

    def _retry_area(self, frame: Frame, decision: Decision) -> InputAction | None:
        """Phase 3 omitted a goal slot; Cross returns to positioning for another lattice point."""
        if self.area_target_confirmations == 0 or decision.intent != "cast":
            return None
        if self.area_retries >= self.max_area_retries:
            return self._area_give_up(frame, decision, "leave area targeting that never covered the target")
        self.area_retries += 1
        self.area_force_step = True
        self.area_stalls = 0
        return self._action(frame, ("Cross",), 2, "return to area positioning after missed coverage")

    def _steer_area(self, frame: Frame, decision: Decision) -> InputAction | None:
        """Move the native positioning-mode sphere over the goal; phase 3 then confirms coverage."""
        goals = self.area_goal_slots(frame, decision)
        limbs = {slot: [(entry["position"][0], entry["position"][2])
                        for entry in frame.raw["target_previews"] if entry["actor"] == slot]
                 for slot in sorted(goals)}
        center = frame.raw["area_center"]
        cx, cz = center[0], center[2]
        if not all(limbs.values()):
            return self._area_give_up(frame, decision, "leave area positioning without an observed goal")
        # Native filtering keeps an actor when any listed limb lies in the area volume, so aim at
        # each actor's limb nearest the group's middle.
        mx = sum(x for parts in limbs.values() for x, _ in parts) / sum(map(len, limbs.values()))
        mz = sum(z for parts in limbs.values() for _, z in parts) / sum(map(len, limbs.values()))
        points = [min(parts, key=lambda p: math.hypot(p[0] - mx, p[1] - mz)) for parts in limbs.values()]
        tx = sum(p[0] for p in points) / len(points)
        tz = sum(p[1] for p in points) / len(points)
        distance = math.hypot(tx - cx, tz - cz)
        radius = self.area_radius(frame)
        if radius is not None:
            self.area_radii[decision.action_id] = radius
        if radius is not None and len(points) > 1:
            spread = max(math.hypot(px - tx, pz - tz) for px, pz in points)
            if spread > radius * 0.9:
                # No cursor position covers every required slot; wait for the hostiles to close in.
                return self._area_give_up(frame, decision, "leave an area too small for the required set",
                                          "area_spread", area_radius=radius, spread=round(spread))
        covered = (radius is not None
                   and all(math.hypot(px - cx, pz - cz) <= radius * 0.8 for px, pz in points))
        if self.area_last is not None:
            (px, pz), (right, down) = self.area_last
            mx, mz = cx - px, cz - pz
            moved = math.hypot(mx, mz)
            if self.area_yaw is None and moved >= 8:
                # direction_buttons rotates screen (-right, down) by -yaw onto world (x, z).
                # Learn only the first free move; later moves may slide along the range clamp.
                turn = math.atan2(down, -right) - math.atan2(mz, mx)
                self.area_yaw = round(turn * 4096 / math.tau) % 4096
                self.area_quantum = moved
            if distance + 16 < self.area_best:
                self.area_stalls = 0
            else:
                # The range volume clamps the sphere; sliding along its edge is not progress.
                self.area_stalls += 1
        self.area_best = min(self.area_best, distance)
        close = max(24.0, (self.area_quantum or 0.0) / 2)
        arrived = covered if radius is not None else distance <= close
        if arrived and not self.area_force_step:
            self.area_last = None
            return None
        if self.area_steps >= self.max_area_steps or self.area_stalls >= 6:
            reached = (all(math.hypot(px - cx, pz - cz) <= radius for px, pz in points)
                       if radius is not None else self.area_best <= max(close, 96.0))
            if reached and not self.area_force_step:
                # Clamped near the goal: let the native phase-3 list decide coverage.
                self.area_last = None
                return None
            return self._area_give_up(frame, decision, "leave area positioning that cannot cover the target")
        yaw = self.area_yaw if self.area_yaw is not None else (frame.camera_yaw or 0)
        buttons = direction_buttons(round(tx - cx), round(tz - cz), yaw)
        self.area_force_step = False
        if not buttons:
            self.area_last = None
            return None
        right = (1 if "Right" in buttons else 0) - (1 if "Left" in buttons else 0)
        down = (1 if "Down" in buttons else 0) - (1 if "Up" in buttons else 0)
        self.area_last = ((cx, cz), (right, down))
        self.area_steps += 1
        # A full hold moves the cursor about 120 units; tap when the area is smaller than that.
        fine = radius is not None and self.area_quantum is not None and distance < self.area_quantum
        return self._action(frame, buttons, 1 if fine else 4, "steer the native area sphere toward the target")

    def _action(self, frame: Frame, buttons: tuple[str, ...] | frozenset[str],
                duration: int, reason: str, reaction: bool = False) -> InputAction:
        self.last_action_tick = frame.tick
        return InputAction(frozenset(buttons), max(duration, self.input_ticks), reason, reaction)

    @property
    def input_gap(self) -> int:
        # Menus act on press edges, so each press needs a held and a released host frame.
        return max(6, 3 * self.input_ticks)

    def _reject_target(self, kind: str, decision: Decision, frame: Frame) -> None:
        player = frame.player
        if player is None:
            raise ValueError("Native target rejection lacks the observed player position")
        self.feedback.append({
            "kind": kind, "tick": frame.tick, "target": decision.target_slot, "intent": decision.intent,
            "target_identity": self.identity, "player_position": player.position,
        })
        if decision.intent == "attack" and self.identity is not None:
            self.rejected_positions[self.identity] = player.position
            target = next((actor for actor in frame.enemies
                           if actor.slot == decision.target_slot), None)
            if target is not None:
                self.rejection_history.setdefault(self.identity, []).append(
                    (frame.tick, player.position, target.position))

    def unreachable_hostiles(self, frame: Frame) -> list[dict]:
        """Hostiles native targeting repeatedly refused from distinct positions (walls, ledges)."""
        verdicts = []
        for actor in frame.enemies:
            identity = self.target_identity(frame, actor)
            history = [row for row in self.rejection_history.get(identity, [])
                       if frame.tick - row[0] <= self.unreachable_window
                       and math.dist(row[2], actor.position) <= 192]
            self.rejection_history[identity] = history
            blocked = self.terrain_blocked.get(identity)
            if blocked is not None and (frame.tick - blocked[0] > self.unreachable_window
                                        or math.dist(blocked[1], actor.position) > 192):
                del self.terrain_blocked[identity]
                blocked = None
            if len(history) >= self.unreachable_rejections:
                verdicts.append({
                    "slot": actor.slot, "native_target_rejections": len(history),
                    "claim": ("Native attack targeting refused this hostile from several nearby "
                              "positions; terrain such as a wall or ledge likely separates it."),
                })
            elif blocked is not None:
                verdicts.append({
                    "slot": actor.slot, "terrain_route_exhausted_tick": blocked[0],
                    "claim": ("Observed runtime terrain offers no further supported route toward this "
                              "hostile (high walls or ledges); approach it again only after it moves."),
                })
        return verdicts

    def mark_terrain_unreachable(self, frame: Frame, slot: int) -> bool:
        """Record that terrain navigation exhausted every supported route toward this hostile."""
        actor = next((actor for actor in frame.enemies if actor.slot == slot), None)
        if actor is None or frame.player is None:
            return False
        self.terrain_blocked[self.target_identity(frame, actor)] = (frame.tick, actor.position)
        self.feedback.append({"kind": "terrain_unreachable", "tick": frame.tick, "target": slot,
                              "player_position": frame.player.position, "target_position": actor.position})
        if self.decision is not None and self.decision.target_slot == slot and not self.preparing:
            self.decision = None
        return True

    def attack_reposition_required(self, frame: Frame) -> bool:
        decision = self.decision
        if (decision is None or decision.intent != "attack" or frame.player is None
                or (frame.episode, frame.zone, frame.room) != self.decision_scene):
            return False
        target = next((actor for actor in frame.enemies
                       if actor.slot == decision.target_slot), None)
        if target is None or self.target_identity(frame, target) != self.identity:
            return False
        rejected = self.rejected_positions.get(self.identity)
        return rejected is not None and sum(
            (frame.player.position[index] - rejected[index])**2 for index in (0, 1, 2)
        ) < 16**2

    def step(self, frame: Frame, allow_strategy: bool = True) -> InputAction | None:
        reaction = self.reactions.observe(frame)
        if reaction:
            return self._action(frame, (reaction.button,), 2,
                                f"{reaction.kind}:slot{reaction.slot}", True)
        if (frame.ready and frame.control == 11 and frame.event_phase == 0
                and frame.tick - self.last_action_tick >= self.input_gap):
            # Key/sigil unlock notices (Menu_ShowSceneNotification) pause the field until a button at
            # their arrow; dismissing one is not a strategy choice, so settled combat cannot block it.
            if frame.tick % 30 < 2:
                return self._action(frame, ("Circle",), 2, "dismiss the native door/lock notification")
            return None
        if not allow_strategy:
            return None
        if not frame.ready or frame.game_over or frame.player is None:
            return None
        if self.decision is not None and (frame.episode, frame.zone, frame.room) != self.decision_scene:
            if self.preparing:
                raise StaleDecision("Scene changed during an observed preparation transaction")
            self.feedback.append({"kind": "scene_changed", "tick": frame.tick})
            self.decision = None
        if frame.reaction.active:
            return None
        if frame.tick - self.last_action_tick < self.input_gap:
            return None
        if frame.dialogue_waiting:
            if frame.tick % 30 < 2:
                return self._action(frame, ("Circle",), 2, "visible dialogue confirmation")
            return None
        menu = frame.raw["menu"]
        if loot_menu_ready(frame):
            return self._action(frame, ("Square",), 2, "claim currently fitting native loot")
        if reward_menu_ready(frame):
            decision = self.decision
            if decision is None or decision.intent != "reward":
                return None
            menu = frame.raw["menu"]
            row = next((row for row in menu["rows"]
                        if row["index"] == decision.action_id and not row["disabled"]), None)
            if row is None:
                raise StaleDecision("The selected reward row is no longer available")
            if menu["selected_row"] != row["index"]:
                button = "Down" if menu["selected_row"] < row["index"] else "Up"
                return self._action(frame, (button,), 2, "select an enabled earned-reward row")
            self.decision = None
            return self._action(frame, ("Circle",), 2, "confirm the native earned ability reward")
        if self.preparing:
            if self.transaction is None:
                raise RuntimeError("Preparation state lost its transaction")
            instruction = self.transaction.step(frame)
            if self.transaction.done:
                if self.transaction.result is None:
                    raise RuntimeError("Menu transaction ended without an outcome")
                self.feedback.append(self.transaction.result)
                self.decision = None
                return None
            if instruction:
                return self._action(frame, tuple(instruction.button.split("+")),
                                    instruction.duration, instruction.reason)
        if frame.control == 5:
            if self.preparing:
                return None
            return self._action(frame, ("Cross",), 2, "return from menu through normal cancel")
        decision = self.decision
        if decision is None:
            return None
        target = next((actor for actor in frame.enemies
                       if actor.slot == decision.target_slot), None)
        if not self.preparing and decision.target_slot > 0 and (
            target is None or self.target_identity(frame, target) != self.identity
        ):
            self.feedback.append({"kind": "target_changed", "tick": frame.tick})
            self.decision = None
            return None
        if (frame.control == 2 and frame.source_actor == 0
                and frame.action_phase == 2 and decision.intent == "cast"):
            if self.transaction is None or not self.transaction.selected:
                raise StaleDecision("Native area targeting began without a selected spell transaction")
            if frame.raw.get("action") != decision.action_id:
                raise StaleDecision("Native area targeting changed to a different action")
            if self.transaction.closing:
                return self._action(frame, ("Cross",), 2, "leave rejected native area targeting")
            center = frame.raw.get("area_center")
            if (not isinstance(center, list) or len(center) < 3
                    or not all(isinstance(value, int) and not isinstance(value, bool)
                               for value in center[:3])):
                raise ValueError("Native area targeting lacks a valid observed center")
            if frame.raw.get("area_positioning"):
                steer = self._steer_area(frame, decision)
                if steer is not None:
                    return steer
            if self.area_target_confirmations >= self.max_area_target_confirmations:
                if (self.area_target_confirmation_tick is not None
                        and frame.tick - self.area_target_confirmation_tick > 60):
                    raise TimeoutError("Native area targeting did not leave phase 2 after confirmation")
                return None
            self.area_target_confirmations += 1
            self.area_target_confirmation_tick = frame.tick
            # BATTLE.PRG func_800C4794: Cross cancels the default sphere mode; Circle enters
            # positioning mode, then a second Circle confirms the area once it covers the target.
            return self._action(frame, ("Circle",), 2,
                                "advance or confirm native area targeting")
        if frame.control == 2 and frame.source_actor == 0 and frame.action_phase in (1, 3):
            if decision.intent == "wait":
                return None
            if decision.intent == "item" and self.transaction is not None and not self.transaction.selected:
                return self._action(frame, ("Cross",), 2,
                                    "cancel prior targeting before opening a healing-item menu")
            if decision.intent not in ("attack", "cast", "item"):
                return self._action(frame, ("Cross",), 2, "cancel targeting before changing strategy")
            page = frame.raw["target_page"]
            selection_step = page["selection_step"]
            if selection_step == 16:
                self._reject_target("target_not_selectable", decision, frame)
                if self.transaction and self.preparing:
                    self.transaction.closing = True
                    self.transaction.result = {"reason": "No native spell target"}
                else:
                    self.decision = None
                return self._action(frame, ("Cross",), 2, "cancel native no-target message")
            if selection_step != 19:
                return None
            if self.transaction is not None and self.transaction.closing:
                return self._action(frame, ("Cross",), 2, "leave rejected native targeting")
            if self.needs_target_choice(frame):
                return None
            previews = frame.raw["target_previews"]
            if (decision.intent == "cast" and decision.target_slot in self.simultaneous_defeat
                    and self.transaction is not None and self.preparing):
                gaps = self.area_defeat_gaps(frame)
                if gaps:
                    if self.area_retries < self.max_area_retries:
                        retry = self._retry_area(frame, decision)
                        if retry is not None:
                            return retry
                    # Defeating only part of the set fires the single-defeat trigger instead.
                    self.feedback.append({"kind": "area_incomplete", "status": "area_incomplete",
                                          "tick": frame.tick, "action_id": decision.action_id,
                                          "target": decision.target_slot, "missing": gaps})
                    self.transaction.closing = True
                    self.transaction.result = {"reason": "Native area preview cannot defeat every required slot"}
                    return self._action(frame, ("Cross",), 2, "cancel an area that misses a required defeat")
            desired = [
                index for index, entry in enumerate(previews)
                if entry["actor"] == decision.target_slot
                and (decision.target_limb < 0 or entry["limb"] == decision.target_limb)
            ]
            if not desired:
                retry = self._retry_area(frame, decision)
                if retry is not None:
                    return retry
                self._reject_target("target_not_listed", decision, frame)
                if self.transaction and self.preparing:
                    self.transaction.closing = True
                    self.transaction.result = {"reason": "Requested actor/limb is not a native spell target"}
                else:
                    self.decision = None
                return self._action(frame, ("Cross",), 2, "cancel unavailable target")
            first, visible = page["first"], page["visible"]
            if not any(first <= index < first + visible for index in desired):
                return self._action(frame, ("R1",), 2, "advance native target page")
            if page["selected"] in desired:
                return self._action(frame, ("Circle",), 2, "confirm native target")
            return self._action(frame, ("Down",), 2, "select observed target through native menu")
        if frame.control != 1 or frame.event_phase != 0:
            return None
        if self.preparing:
            return None
        if decision.intent in ("wait", "navigate"):
            return None
        if decision.intent == "interact":
            return self._action(frame, ("Cross",), 2, "normal field interaction")
        if target is None or frame.camera_yaw is None:
            raise ValueError("Movement/attack decision lacks current native position or camera facts")
        player = frame.player
        if decision.intent == "attack" and not player.weapon_drawn:
            return self._action(frame, ("Circle",), 2, "draw equipped weapon")
        dx = target.position[0] - player.position[0]
        dz = target.position[2] - player.position[2]
        if decision.intent == "retreat":
            dx, dz = -dx, -dz
        elif decision.intent == "attack":
            if player.native_extent <= 0:
                raise ValueError("Native weapon reach is unavailable; refusing a guessed range")
            if (not self.attack_reposition_required(frame)
                    and dx * dx + dz * dz <= player.native_extent * player.native_extent):
                return self._action(frame, ("Circle",), 2, "open native attack targeting")
        buttons = direction_buttons(dx, dz, frame.camera_yaw)
        if not buttons:
            return None
        return self._action(frame, buttons, 8, "move using current native positions and camera")
