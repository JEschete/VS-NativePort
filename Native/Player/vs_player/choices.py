import json

from .abilities import defensive_loadout_advice, item_recovery_bounds
from .config import PROJECT
from .observations import Frame


def event_menu_ready(frame: Frame) -> bool:
    menu = frame.raw["menu"]
    return (frame.ready and not frame.game_over and frame.control == 3
            and frame.event_phase == 4 and menu["available"] and not menu["loading"]
            and menu.get("event_owned") is True)


def loot_menu_ready(frame: Frame) -> bool:
    menu = frame.raw["menu"]
    return (((frame.control == 2 and frame.action_phase == 12)
             or (event_menu_ready(frame) and frame.action_phase == 4))
            and menu["available"] and not menu["loading"] and menu["selected_menu"] == 12
            and menu["loot_input_ready"] and menu["loot_count"] > 0)


def reward_menu_ready(frame: Frame) -> bool:
    menu = frame.raw["menu"]
    return (((frame.control == 2 and frame.action_phase == 14)
             or (event_menu_ready(frame) and frame.action_phase == 6)) and menu["available"]
            and not menu["loading"] and menu["list_phase"] == 17
            and any(not row["disabled"] for row in menu["rows"]))


def native_target_menu_ready(frame: Frame) -> bool:
    return (frame.ready and not frame.game_over and frame.control == 2
            and frame.source_actor == 0 and frame.action_phase in (1, 3)
            and frame.raw["target_page"]["selection_step"] == 19)


def available_choices(frame: Frame, context: dict) -> dict[str, dict]:
    choices: dict[str, dict] = {}
    player = frame.player
    if player is None:
        return {}
    knowledge = json.loads((PROJECT / "combat-knowledge.json").read_text(encoding="utf-8"))
    semantics = knowledge["known_action_semantics"]
    options = frame.raw["action_options"]
    feedback = context.get("controller_feedback", [])

    def add(intent: str, target: int = -1, action: int = -1, slot: int = 1,
            item: int = -1, limb: int = -1) -> None:
        key = f"{intent}:{target}:{action}:{slot}"
        if item >= 0:
            key += f":{item}"
        if limb >= 0:
            key += f":limb{limb}"
        choices[key] = {"intent": intent, "target_slot": target, "target_limb": limb,
                        "action_id": action, "equip_slot": slot, "item_id": item}

    def unsuccessful(action: int, target: int) -> bool:
        return any(row.get("action_id") == action and row.get("target") == target
                   and row.get("status") in ("blocked", "no_effect") for row in feedback)

    if reward_menu_ready(frame):
        for row in frame.raw["menu"]["rows"]:
            if not row["disabled"]:
                add("reward", action=row["index"])
        return choices

    learned_spells = [option for option in options
                      if option["family"] in ("shaman", "sorcerer", "enchanter", "warlock")
                      and option["restrictions"] == 0]
    danger_threshold = min(player.max_hp, context.get("max_incoming_hp_damage", 0) * 2)
    if player.hp * 5 < max(player.max_hp * 3, danger_threshold * 5):
        for option in learned_spells:
            if semantics.get(str(option["id"]), {}).get("role") == "heal" and not unsuccessful(option["id"], 0):
                add("cast", 0, option["id"])
        for option in frame.raw["item_options"]:
            bounds = item_recovery_bounds(option, knowledge["item_semantics"]["hp_recovery"])
            if (bounds is not None and bounds[0] > 0 and option["count"] > 0
                    and option["restrictions"] & 2 == 0
                    and not unsuccessful(option["action_id"], 0)):
                add("item", 0, option["action_id"], item=option["id"])
        if choices:
            return choices

    advice = defensive_loadout_advice(frame)
    recommended = advice.get("recommended_available_actions", [])
    defenses = frame.reaction.defense_actions
    if recommended and not any(action in recommended for action in defenses):
        for action in recommended:
            for slot in (1, 2, 3):
                add("equip_defense", 0, action, slot)
        return choices

    # A model cannot wait for an acknowledged milestone forever.
    required = context.get("required_successes", {}).get("defense", 0)
    confirmed = context.get("confirmed_successes", {}).get("defense", 0)
    if confirmed < required:
        add("wait")
        for enemy in frame.enemies:
            add("approach", enemy.slot)
        return choices

    chains = {option["id"] for option in options if option["family"] == "chain"}
    mapped = frame.reaction.chain_actions
    if 0 in mapped:
        for action in (22, 28, 23):
            if action in chains and action not in mapped:
                add("equip_chain", 0, action, mapped.index(0) + 1)
                return choices
    caster_targets = [enemy for enemy in frame.enemies if enemy.spell_action_ids]
    silence_mask = frame.raw["status_masks"][12]
    wants_silence = 35 in chains and any(
        not (enemy.statuses | enemy.status_immunities) & silence_mask
        for enemy in caster_targets
    )
    if wants_silence and 35 not in mapped:
        add("equip_chain", 0, 35, 3)
        return choices
    if (frame.enemies and all(enemy.mp == 0 for enemy in frame.enemies)
            and 24 in mapped and 28 in chains and 28 not in mapped):
        add("equip_chain", 0, 28, mapped.index(24) + 1)
        return choices

    for enemy in frame.enemies:
        if not enemy.spell_action_ids:
            continue
        for option in learned_spells:
            details = semantics.get(str(option["id"]), {})
            if option["id"] not in (55, 75) or unsuccessful(option["id"], enemy.slot):
                continue
            mask = frame.raw["status_masks"][details["status_parameter"]]
            if not (enemy.statuses | enemy.status_immunities) & mask:
                add("cast", enemy.slot, option["id"])

    for option in learned_spells:
        details = semantics.get(str(option["id"]), {})
        if details.get("role") != "buff" or unsuccessful(option["id"], 0):
            continue
        if option["id"] not in (60, 63, 76):
            continue
        if option["id"] == 76 and not any(enemy.spell_action_ids for enemy in frame.enemies):
            continue
        mask = frame.raw["status_masks"][details["status_parameter"]]
        if mask and not player.statuses & mask and player.mp >= option["cost"] + 5:
            add("cast", 0, option["id"])
    if choices:
        return choices

    if context.get("physical_navigation_objective") and all(
        context.get("confirmed_successes", {}).get(kind, 0) >= count
        for kind, count in context.get("required_successes", {}).items()
    ):
        add("navigate")
        return choices

    for enemy in frame.enemies:
        if native_target_menu_ready(frame):
            for target in frame.raw["target_previews"]:
                if target["actor"] == enemy.slot:
                    add("attack", enemy.slot, limb=target["limb"])
        else:
            add("attack", enemy.slot)
        add("approach", enemy.slot)
        add("retreat", enemy.slot)
    for option in learned_spells:
        details = semantics.get(str(option["id"]), {})
        role = details.get("role")
        if role in ("heal", "buff", "cure"):
            continue
        for enemy in frame.enemies:
            if not unsuccessful(option["id"], enemy.slot):
                add("cast", enemy.slot, option["id"])
    if not choices:
        add("wait")
    return choices
