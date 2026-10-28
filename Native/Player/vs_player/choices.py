import json

from .abilities import defensive_loadout_advice
from .config import PROJECT
from .observations import Frame


def available_choices(frame: Frame, context: dict) -> dict[str, dict]:
    choices: dict[str, dict] = {}
    player = frame.player
    if player is None:
        return {}
    knowledge = json.loads((PROJECT / "combat-knowledge.json").read_text(encoding="utf-8"))
    semantics = knowledge["known_action_semantics"]
    options = frame.raw["action_options"]
    feedback = context.get("controller_feedback", [])

    def add(intent: str, target: int = -1, action: int = -1, slot: int = 1) -> None:
        key = f"{intent}:{target}:{action}:{slot}"
        choices[key] = {"intent": intent, "target_slot": target, "target_limb": -1,
                        "action_id": action, "equip_slot": slot}

    def unsuccessful(action: int, target: int) -> bool:
        return any(row.get("action_id") == action and row.get("target") == target
                   and row.get("status") in ("blocked", "no_effect") for row in feedback)

    learned_spells = [option for option in options
                      if option["family"] in ("shaman", "sorcerer", "enchanter", "warlock")
                      and option["restrictions"] == 0]
    if player.hp * 5 < player.max_hp * 3:
        for option in learned_spells:
            if semantics.get(str(option["id"]), {}).get("role") == "heal" and not unsuccessful(option["id"], 0):
                add("cast", 0, option["id"])
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
    if choices:
        return choices

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

    for enemy in frame.enemies:
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
