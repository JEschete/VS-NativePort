"""Semantic applicability follows the production effect functions, not action labels."""

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .observations import Frame


def defense_value(frame: "Frame", action: int) -> float:
    player = frame.player
    if player is None:
        return 0
    timing = frame.reaction
    hp = timing.incoming_hp_damage
    amount = hp + timing.incoming_mp_damage
    spell = timing.incoming_type == 1
    missing_hp = max(0, player.max_hp - player.hp + hp)
    missing_mp = max(0, player.max_mp - player.mp)
    recovery_priority = 2 if player.hp * 2 < player.max_hp else 1
    if action == 40:
        masks = frame.raw["status_masks"]
        curable = masks[13] | masks[15]
        return 100 if (player.statuses | timing.incoming_statuses) & curable else 0
    if action == 41:
        return min(missing_mp, timing.incoming_cost // 2) * 0.5 if spell else 0
    if action in (42, 43):
        applies = spell if action == 42 else not spell
        return amount * 4 // 10 if applies and hp > 0 else 0
    if action in (44, 45):
        applies = spell if action == 44 else not spell
        return min(missing_hp, amount * 2 // 10) * recovery_priority if applies and hp > 0 else 0
    if 46 <= action <= 52:
        return (min(missing_hp, amount * 5 // 10) * recovery_priority
                if hp > 0 and timing.incoming_affinity == action - 46 else 0)
    if action == 53:
        return min(missing_hp, player.shield_pp) * recovery_priority if hp > 0 else 0
    return 0


def defensive_loadout_advice(frame: "Frame") -> dict:
    """Forecast applicability from observed native capabilities, not invented damage."""
    player = frame.player
    if player is None:
        return {}
    types = {enemy.base_attack_type for enemy in frame.enemies}
    spell_capable = any(enemy.spell_action_ids for enemy in frame.enemies)
    physical_expected = any(value != 1 for value in types)
    spell_expected = spell_capable or 1 in types
    available = {option["id"]: option for option in frame.raw["action_options"]
                 if option["family"] == "defense"}
    recommended = []
    if physical_expected:
        recommended.extend(action for action in (43, 45) if action in available)
    if spell_expected:
        recommended.extend(action for action in (42, 44) if action in available)
    masks = frame.raw["status_masks"]
    if player.statuses & (masks[13] | masks[15]) and 40 in available:
        recommended.insert(0, 40)
    if player.shield_pp > 0 and 53 in available:
        recommended.append(53)
    return {
        "physical_attack_expected": physical_expected,
        "spell_attack_possible": spell_expected,
        "recommended_available_actions": list(dict.fromkeys(recommended)),
        "reason": (
            "Ward cures only native paralysis/numbness. Siphon Soul needs an incoming spell "
            "and missing MP. Reflect/Absorb require matching spell/nonspell damage; "
            "affinity defenses additionally require their exact affinity. These forecasts "
            "are not hit-roll or damage guarantees."
        ),
    }


def offensive_value(frame: "Frame", action: int) -> float:
    player = frame.player
    target = next((actor for actor in frame.actors
                   if actor.slot == frame.reaction.target_actor), None)
    if player is None or target is None:
        return 0
    if action == 23 and player.hp >= player.max_hp:
        return 0
    if action == 25 and player.mp >= player.max_mp:
        return 0
    if action in (24, 27) and target.mp <= 0:
        return 0
    if action == 26 and player.hp >= player.max_hp:
        return 0
    if action == 27 and player.mp >= player.max_mp:
        return 0
    if action == 29 and player.hp * 2 < player.max_hp:
        return 0
    if action == 31 and player.weapon_pp <= 0:
        return 0
    parameters = {32: 13, 34: 15, 35: 12, 37: 14}
    if action in parameters:
        mask = frame.raw["status_masks"][parameters[action]]
        if mask == 0 or target.statuses & mask or target.status_immunities & mask:
            return 0
        return 2 if action == 35 and target.spell_action_ids else 1
    return 1
