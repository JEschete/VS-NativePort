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
    def __init__(self) -> None:
        self.reactions = ReactionController()
        self.decision: Decision | None = None
        self.identity: tuple[int, int, int, int, int] | None = None
        self.last_action_tick = -100
        self.target_wait_started: int | None = None
        self.feedback: list[dict] = []
        self.transaction: MenuTransaction | None = None
        self.rejected_positions: dict[tuple[int, int, int, int, int], tuple[int, int, int]] = {}
        self.decision_scene: tuple[int, int, int] | None = None

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

    def _action(self, frame: Frame, buttons: tuple[str, ...] | frozenset[str],
                duration: int, reason: str, reaction: bool = False) -> InputAction:
        self.last_action_tick = frame.tick
        return InputAction(frozenset(buttons), duration, reason, reaction)

    def _reject_target(self, kind: str, decision: Decision, frame: Frame) -> None:
        player = frame.player
        if player is None:
            raise ValueError("Native target rejection lacks the observed player position")
        self.feedback.append({
            "kind": kind, "tick": frame.tick, "target": decision.target_slot,
            "target_identity": self.identity, "player_position": player.position,
        })
        if decision.intent == "attack" and self.identity is not None:
            self.rejected_positions[self.identity] = player.position

    def step(self, frame: Frame, allow_strategy: bool = True) -> InputAction | None:
        reaction = self.reactions.observe(frame)
        if reaction:
            return self._action(frame, (reaction.button,), 2,
                                f"{reaction.kind}:slot{reaction.slot}", True)
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
        if frame.tick - self.last_action_tick < 6:
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
            if self.needs_target_choice(frame):
                return None
            previews = frame.raw["target_previews"]
            desired = [
                index for index, entry in enumerate(previews)
                if entry["actor"] == decision.target_slot
                and (decision.target_limb < 0 or entry["limb"] == decision.target_limb)
            ]
            if not desired:
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
            rejected = self.rejected_positions.get(self.identity) if self.identity is not None else None
            moved = rejected is None or sum(
                (player.position[index] - rejected[index])**2 for index in (0, 1, 2)
            ) >= 16**2
            if moved and dx * dx + dz * dz <= player.native_extent * player.native_extent:
                return self._action(frame, ("Circle",), 2, "open native attack targeting")
        buttons = direction_buttons(dx, dz, frame.camera_yaw)
        if not buttons:
            return None
        return self._action(frame, buttons, 8, "move using current native positions and camera")
