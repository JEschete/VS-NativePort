import json
from dataclasses import dataclass

from .config import PROJECT
from .model import Decision
from .observations import Frame
from .abilities import item_recovery_bounds

SLOT_BUTTONS = {1: "Circle", 2: "Triangle", 3: "Square"}
MAGIC_FAMILIES = {"warlock": (0, 7), "shaman": (1, 9),
                  "sorcerer": (2, 11), "enchanter": (3, 13)}


@dataclass(frozen=True)
class MenuInput:
    button: str
    reason: str
    duration: int = 2


class MenuTransaction:
    """Observe menu progress and effects; never invoke menu/game functions directly."""

    def __init__(self, decision: Decision, frame: Frame) -> None:
        family = {"equip_chain": "chain", "equip_defense": "defense"}.get(decision.intent)
        if decision.intent == "item":
            options = [option for option in frame.raw["item_options"]
                       if option["id"] == decision.item_id and option["action_id"] == decision.action_id
                       and option["count"] > 0 and option["restrictions"] & 2 == 0]
        else:
            options = [option for option in frame.raw["action_options"]
                       if option["id"] == decision.action_id
                       and (family is None or option["family"] == family)]
        if not options:
            raise ValueError("Menu request is absent from native available action/item options")
        self.option = options[0]
        if decision.intent == "cast" and (
            self.option["family"] not in MAGIC_FAMILIES
            or self.option["restrictions"] & ~1 != 0
        ):
            raise ValueError("Requested spell is not currently permitted")
        self.decision = decision
        self.started = frame.tick
        self.opened = False
        self.cast_seen = False
        self.selected = False
        self.closing = False
        self.done = False
        self.result: dict | None = None
        self.target_before = next((actor for actor in frame.actors
                                   if actor.slot == decision.target_slot), None)
        self.player_before = frame.player
        self.last_target = self.target_before
        self.effect_seen = False
        self.effect_delta: dict = {}
        knowledge = json.loads(
            (PROJECT / "combat-knowledge.json").read_text(encoding="utf-8")
        )
        self.item_bounds = None
        self.item_count_before = 0
        if decision.intent == "item":
            self.item_bounds = item_recovery_bounds(self.option, knowledge["item_semantics"]["hp_recovery"])
            if self.item_bounds is None:
                raise ValueError("This consumable's effect is not supported for automatic HP recovery")
            self.semantics = {"role": "heal"}
        else:
            self.semantics = knowledge["known_action_semantics"].get(str(decision.action_id), {})

    def item_count(self, frame: Frame) -> int:
        return sum(item["count"] for item in frame.raw["items"] if item["id"] == self.decision.item_id)

    def _finish(self, frame: Frame, status: str, reason: str) -> None:
        self.done = True
        self.result = {
            "kind": self.decision.intent, "action_id": self.decision.action_id,
            "target": self.decision.target_slot, "equip_slot": self.decision.equip_slot,
            "status": status, "reason": reason, "tick": frame.tick,
            "native_action_seen": self.cast_seen, "effect": self.effect_delta,
            "item_id": self.decision.item_id,
        }

    def _row(self, frame: Frame, index: int, reason: str) -> MenuInput | None:
        menu = frame.raw["menu"]
        if menu["list_phase"] != 17 or not menu["rows"]:
            return None
        row = next((row for row in menu["rows"] if row["index"] == index), None)
        if row is None:
            raise ValueError("Requested menu row is absent from the current native list")
        if row["disabled"]:
            self.closing = True
            self.result = {"reason": "Native menu disabled this action"}
            return MenuInput("Triangle", "leave disabled native menu")
        if menu["selected_row"] != index:
            return MenuInput("Down" if menu["selected_row"] < index else "Up", reason)
        return MenuInput("Circle", reason)

    def observe(self, frame: Frame) -> None:
        if self.done:
            return
        if frame.tick - self.started > 2400:
            raise TimeoutError("Observed menu/action transaction exceeded its bounded lifetime")
        if self.decision.intent not in ("cast", "item"):
            mapped = (frame.reaction.chain_actions if self.decision.intent == "equip_chain"
                      else frame.reaction.defense_actions)
            if mapped[self.decision.equip_slot - 1] == self.decision.action_id:
                self.closing = True
            return
        if (frame.control == 2 and frame.source_actor == 0
                and frame.reaction.action == self.decision.action_id
                and frame.action_phase >= 4
                and (self.decision.intent != "item" or self.selected)):
            self.cast_seen = True
        target = next((actor for actor in frame.actors
                       if actor.slot == self.decision.target_slot), None)
        if not self.cast_seen or target is None or self.target_before is None:
            return
        before = self.target_before
        role = self.semantics.get("role")
        parameter = self.semantics.get("status_parameter")
        status_mask = frame.raw["status_masks"][parameter] if parameter is not None else 0
        cure_parameter = self.semantics.get("cures_parameter")
        cures = frame.raw["status_masks"][cure_parameter] if cure_parameter is not None else 0
        if role == "heal" and target.hp > before.hp:
            if self.decision.intent == "item":
                if self.item_bounds is None:
                    raise RuntimeError("Healing item lost its verified effect bounds")
                count = self.item_count(frame)
                self.effect_delta = {"hp_before": before.hp, "hp_after": target.hp,
                                     "item_count_before": self.item_count_before,
                                     "item_count_after": count}
                minimum = min(self.item_bounds[0], before.max_hp-before.hp)
                if self.item_count_before-count != 1 or target.hp-before.hp < minimum:
                    return
            self.effect_seen = True
            self.effect_delta |= {"hp_before": before.hp, "hp_after": target.hp}
        elif status_mask and target.statuses & status_mask and not before.statuses & status_mask:
            self.effect_seen = True
            self.effect_delta = {"status_mask": status_mask, "statuses_after": target.statuses}
        elif cures and before.statuses & cures and not target.statuses & cures:
            self.effect_seen = True
            self.effect_delta = {"cured_mask": cures}
        elif role is None and target.hp < before.hp:
            self.effect_seen = True
            self.effect_delta = {"hp_before": before.hp, "hp_after": target.hp}
        self.last_target = target

    def step(self, frame: Frame) -> MenuInput | None:
        self.observe(frame)
        if self.done or not frame.ready:
            return None
        menu = frame.raw["menu"]
        if frame.control == 1 and frame.event_phase == 0:
            if self.closing:
                if self.decision.intent in ("cast", "item"):
                    self._finish(frame, "blocked", self.result["reason"] if self.result else "Native targeting refused action")
                else:
                    self._finish(frame, "accepted", "Equipped mapping verified after normal menu return")
                return None
            if self.cast_seen:
                self._finish(
                    frame, "accepted" if self.effect_seen else "no_effect",
                    "Native action and effect verified" if self.effect_seen
                    else "Native cast completed without the requested observable effect",
                )
                return None
            if self.selected and self.opened:
                self._finish(frame, "blocked", "Selected action returned without committed execution")
                return None
            if self.decision.intent == "item":
                current = next((option for option in frame.raw["item_options"]
                                if option["id"] == self.decision.item_id
                                and option["action_id"] == self.decision.action_id), None)
                if current is None or current["restrictions"] & 2:
                    self._finish(frame, "blocked", "Consumable stock or native availability changed")
                    return None
                if current["restrictions"] & 1:
                    return None
                self.opened = True
                # GameLoop's shortcut bit0 is L2, not Select.
                return MenuInput("L2", "open production item shortcut", 8)
            if self.decision.intent == "cast":
                current = next((option for option in frame.raw["action_options"]
                                if option["id"] == self.decision.action_id), None)
                if current is None or current["restrictions"] & ~1:
                    self._finish(frame, "blocked", "Spell resources or status changed before opening the menu")
                    return None
                if current["restrictions"] & 1:
                    return None
            self.opened = True
            return MenuInput("Triangle", "open production preparation menu")
        if self.decision.intent == "item" and frame.control == 6:
            return MenuInput("L2+Up", "choose the native consumable shortcut", 4)
        if frame.control != 5:
            return None
        if not menu["available"] or menu["loading"]:
            return None
        if self.decision.intent == "item" and self.selected and not self.closing:
            return None
        if self.closing:
            if menu["selected_menu"] == 3 and menu["submenu_phase"] in (6, 7):
                return MenuInput("Cross", "leave ability assignment without toggling another slot")
            return MenuInput("Triangle", "return to gameplay after verified menu operation")
        selected_menu = menu["selected_menu"]
        wanted_menu = 4 if self.decision.intent == "item" else 1 if self.decision.intent == "cast" else 3
        if selected_menu == 0:
            desired = wanted_menu - 1
            row = next((row for row in menu["rows"] if row["index"] == desired), None)
            if row is None or row["disabled"]:
                self.closing = True
                self.result = {"reason": "Preparation category unavailable in native menu"}
                return MenuInput("Triangle", "leave unavailable menu")
            if menu["selected_row"] == desired:
                return MenuInput("Circle", "open required native submenu")
            return MenuInput("Down", "navigate native main menu")
        if selected_menu != wanted_menu:
            return MenuInput("Triangle", "back out of a different native submenu")
        phase = menu["submenu_phase"]
        if self.decision.intent == "item":
            if menu["shortcut"] != 5:
                return MenuInput("Triangle", "leave a different inventory screen")
            if self.selected:
                return None
            row = next((row for row in menu["rows"] if row["item_id"] == self.decision.item_id), None)
            if row is None:
                if menu["list_phase"] == 17:
                    self.closing = True
                    self.result = {"reason": "Owned healing item is absent from the native shortcut list"}
                    return MenuInput("Triangle", "leave unavailable consumable list")
                return None
            action = self._row(frame, row["index"], "select the owned native healing item")
            if action and action.button == "Circle":
                self.selected = True
                self.target_before = frame.player
                self.item_count_before = self.item_count(frame)
            return action
        if self.decision.intent == "cast":
            family = self.option["family"]
            class_row, list_phase = MAGIC_FAMILIES[family]
            if phase == 5:
                return self._row(frame, class_row, "select native spell class")
            if phase == list_phase:
                action = self._row(frame, self.option["menu_order"], "select learned native spell")
                if action and action.button == "Circle":
                    self.selected = True
                return action
            return None
        kind = 0 if self.decision.intent == "equip_chain" else 1
        if phase == 5:
            return self._row(frame, kind, "select chain or defense assignment list")
        if menu["assignment_phase"] == 17 and menu["assignment_type"] == 24 + kind:
            desired = self.option["menu_order"]
            selected = menu["assignment_selected"]
            if selected != desired:
                return MenuInput("Down" if selected < desired else "Up", "select available equipped ability")
            return MenuInput(SLOT_BUTTONS[self.decision.equip_slot], "assign through the native ability menu")
        return None
