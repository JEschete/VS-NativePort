import json
import math

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
             or (frame.control == 9 and frame.action_phase == 3)
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


# Horizontal clearance treated as inside an actor's collision when tracing a line of fire.
FIRE_LINE_CLEARANCE = 96


def fire_line_blocked(player, target, blockers) -> bool:
    """True when a blocker stands on the horizontal segment from the player to the target."""
    if player is None:
        return False
    px, pz = player.position[0], player.position[2]
    tx, tz = target.position[0] - px, target.position[2] - pz
    length = tx * tx + tz * tz
    if length == 0:
        return False
    for blocker in blockers:
        if blocker.slot == target.slot:
            continue
        bx, bz = blocker.position[0] - px, blocker.position[2] - pz
        along = (bx * tx + bz * tz) / length
        if not 0 < along < 1:
            continue
        cx, cz = bx - along * tx, bz - along * tz
        if cx * cx + cz * cz <= FIRE_LINE_CLEARANCE ** 2:
            return True
    return False


def urgent_recovery_needed(frame: Frame, context: dict) -> bool:
    player = frame.player
    if player is None:
        return False
    incoming = context.get("max_incoming_hp_damage", 0)
    return (player.hp * 5 < player.max_hp * 3
            or player.hp <= incoming <= player.max_hp
            or (incoming > 0 and incoming <= player.max_hp
                and player.hp <= incoming * 2
                and any(enemy.spell_action_ids for enemy in frame.enemies)))


def available_choices(frame: Frame, context: dict) -> dict[str, dict]:
    choices: dict[str, dict] = {}
    player = frame.player
    if player is None:
        return {}
    knowledge = json.loads((PROJECT / "combat-knowledge.json").read_text(encoding="utf-8"))
    semantics = knowledge["known_action_semantics"]
    damage_spell_ids = set(knowledge["damage_spells"]["action_ids"])
    options = frame.raw["action_options"]
    feedback = context.get("controller_feedback", [])
    # Native targeting refused these hostiles from several positions; melee would loop against terrain.
    unreachable = {row["slot"] for row in context.get("unreachable_hostiles", [])}
    # Native spell targeting listed no such hostile from here; other spells would be refused alike.
    refused_here = {row["target"] for row in feedback
                    if row.get("kind") == "target_not_listed" and row.get("intent") == "cast"
                    and row.get("target", 0) > 0
                    and row.get("player_position") is not None
                    and math.dist(row["player_position"], player.position) < 128}
    # Hostiles whose spell line of fire currently passes a protected hostile.
    shielded: set[int] = set()

    def add(intent: str, target: int = -1, action: int = -1, slot: int = 1,
            item: int = -1, limb: int = -1) -> None:
        if intent == "cast" and target in shielded:
            return
        key = f"{intent}:{target}:{action}:{slot}"
        if item >= 0:
            key += f":{item}"
        if limb >= 0:
            key += f":limb{limb}"
        choices[key] = {"intent": intent, "target_slot": target, "target_limb": limb,
                        "action_id": action, "equip_slot": slot, "item_id": item}

    def unsuccessful(action: int, target: int) -> bool:
        return target in refused_here or any(row.get("action_id") == action and row.get("target") == target
                   and (row.get("status") in ("blocked", "no_effect")
                        or (row.get("status") == "area_out_of_range"
                            and row.get("player_position") is not None
                            and math.dist(row["player_position"], player.position) < 128))
                   for row in feedback)

    def affordable_spell(option: dict) -> bool:
        cost = option.get("cost")
        return isinstance(cost, int) and 0 <= cost <= player.mp

    if reward_menu_ready(frame):
        for row in frame.raw["menu"]["rows"]:
            if not row["disabled"]:
                add("reward", action=row["index"])
        return choices

    ranked_spells = [option for option in options
                     if option["family"] in ("shaman", "sorcerer", "enchanter", "warlock")
                     and option["restrictions"] == 0]
    # Higher warlock ranks only serve the combined-defeat objective's larger native areas.
    learned_spells = [option for option in ranked_spells if option.get("rank", 0) == 0]
    combined = context.get("simultaneous_defeat_objective")
    living = {enemy.slot for enemy in frame.enemies}
    order = context.get("defeat_order_objective")
    first = next((slot for slot in order["slots"] if slot in living), None) if order else None
    if first is not None:
        # Earlier type-8 rows claim the other defeat orders, so only the next listed slot may be harmed.
        ordinary = available_choices(frame, {key: value for key, value in context.items()
                                             if key != "defeat_order_objective"})
        area_ids = set(knowledge["area_damage_spells"]["action_ids"])
        kept = {key: row for key, row in ordinary.items()
                if row["target_slot"] in (-1, 0, first) and row["action_id"] not in area_ids}
        if not any(row["target_slot"] == first for row in kept.values()):
            if native_target_menu_ready(frame):
                for target in frame.raw["target_previews"]:
                    if target["actor"] == first:
                        add("attack", first, limb=target["limb"])
            else:
                add("attack", first)
            add("approach", first)
        return kept | choices
    if combined:
        # A required slot that is alive but not currently hostile still must not fall alone.
        present = {actor.slot for actor in frame.actors if actor.slot != 0 and actor.hp > 0}
        required = set(combined["slots"])
    if combined and required <= present and (living - required or not required <= living):
        # Other hostiles fall first through ordinary single-target choices; their own defeat
        # triggers set the flags that keep earlier combined triggers from claiming this set.
        ordinary = available_choices(frame, {key: value for key, value in context.items()
                                             if key != "simultaneous_defeat_objective"}
                                     | {"protected_slots": sorted(required)})
        area_ids = set(knowledge["area_damage_spells"]["action_ids"])
        kept = {key: row for key, row in ordinary.items()
                if row["target_slot"] not in required and row["action_id"] not in area_ids}
        if not kept:
            add("wait")
            for slot in required & living:
                add("approach", slot)
        return kept or choices
    if combined and required <= living:
        # Any single defeat would satisfy the original single-defeat trigger first, so only area
        # spells may harm the set; failed previews wait for the hostiles to close in.
        area_ids = set(knowledge["area_damage_spells"]["action_ids"])
        settling = any(row.get("kind") == "area_incomplete" and frame.tick - row["tick"] < 180
                       for row in feedback)
        # Observed native area radii per spell; a spell whose area cannot yet cover every
        # required hostile at once waits for them to close in, while unmeasured spells may be tried.
        radii = {row["action_id"]: row["area_radius"] for row in feedback
                 if row.get("status") == "area_spread"} | {
                     int(action): radius for action, radius in combined.get("area_radii", {}).items()}
        spots = [(enemy.position[0], enemy.position[2]) for enemy in frame.enemies
                 if enemy.slot in combined["slots"]]
        mid = (sum(x for x, _ in spots) / len(spots), sum(z for _, z in spots) / len(spots))
        spread = max(math.dist(spot, mid) for spot in spots)
        if not settling:
            # Per spell, offer only the highest affordable rank whose area may still cover the set.
            best: dict[int, dict] = {}
            for option in ranked_spells:
                if (option["id"] in area_ids and affordable_spell(option)
                        and spread <= radii.get(option["id"], math.inf) * 0.9):
                    base = option["id"] - option.get("rank", 0)
                    if option.get("rank", 0) >= best.get(base, {}).get("rank", -1):
                        best[base] = option
            for option in best.values():
                for slot in combined["slots"]:
                    # A preview that never reached the set from here needs Ashley to move first.
                    if not unsuccessful(option["id"], slot):
                        add("cast", slot, option["id"])
        if not choices:
            add("wait")
            for slot in combined["slots"]:
                add("approach", slot)
        return choices
    # Slots a combined-defeat objective reserves for one area cast are never single targets.
    protected = set(context.get("protected_slots", ()))
    caster_targets = [enemy for enemy in frame.enemies
                      if enemy.spell_action_ids and enemy.slot not in protected]
    if caster_targets:
        dangerous_casters = [
            enemy for enemy in caster_targets
            if any(action in damage_spell_ids
                   or semantics.get(str(action), {}).get("role") == "damage"
                   for action in enemy.spell_action_ids)
        ]
        priority_casters = dangerous_casters or caster_targets
        player_position = player.position
        nearest_caster = min(
            priority_casters,
            key=lambda enemy: sum(
                (left - right) ** 2 for left, right in zip(enemy.position, player_position)
            ),
        )
        caster_targets = [nearest_caster]
    if native_target_menu_ready(frame):
        previews = frame.raw["target_previews"]
        preferred_slots = {enemy.slot for enemy in caster_targets}
        previewed_slots = {target["actor"] for target in previews}
        selected_slots = (preferred_slots & previewed_slots or previewed_slots) - protected
        target_enemies = [enemy for enemy in frame.enemies if enemy.slot in selected_slots]
    else:
        target_enemies = caster_targets or [enemy for enemy in frame.enemies
                                            if enemy.slot not in protected]
    # BATTLE.PRG func_80085B10 traces actor collision from the caster for resetsHitResults actions
    # and redirects the hit to the first actor in the line (MAP150 local5: a spell at slot 2 killed
    # protected slot 4 standing between). Protected slots must not shield a spell's single target.
    shielded.update(enemy.slot for enemy in frame.enemies
                    if fire_line_blocked(player, enemy, [other for other in frame.enemies
                                                         if other.slot in protected]))
    incoming = context.get("max_incoming_hp_damage", 0)
    danger_threshold = min(player.max_hp, incoming * 2)
    urgent_recovery = urgent_recovery_needed(frame, context)
    healing_choices: dict[str, dict] = {}
    if player.hp * 5 < max(player.max_hp * 3, danger_threshold * 5):
        if urgent_recovery:
            for option in learned_spells:
                if (semantics.get(str(option["id"]), {}).get("role") == "heal"
                        and not unsuccessful(option["id"], 0)):
                    add("cast", 0, option["id"])
        for option in frame.raw["item_options"]:
            bounds = item_recovery_bounds(option, knowledge["item_semantics"]["hp_recovery"])
            if (bounds is not None and bounds[0] > 0 and option["count"] > 0
                    and option["restrictions"] & 2 == 0
                    and not unsuccessful(option["action_id"], 0)):
                if (urgent_recovery
                        or (incoming <= player.max_hp
                            and player.hp + bounds[1] >= danger_threshold)):
                    add("item", 0, option["action_id"], item=option["id"])
        if choices:
            if urgent_recovery:
                return choices
            healing_choices = choices
            choices = {}

    advice = defensive_loadout_advice(frame)
    recommended = advice.get("recommended_available_actions", [])
    defenses = frame.reaction.defense_actions
    missing_defenses = [
        action for action in (40, 53)
        if action in recommended and action not in defenses
    ]
    for threat_expected, actions in (
        (advice.get("physical_attack_expected"), (43, 45)),
        (advice.get("spell_attack_possible"), (42, 44)),
    ):
        available = [action for action in actions if action in recommended]
        if threat_expected and available and not any(action in defenses for action in available):
            missing_defenses.extend(available)
    if missing_defenses:
        for action in missing_defenses:
            for slot in (1, 2, 3):
                add("equip_defense", 0, action, slot)
        return choices | healing_choices

    # A model cannot wait for an acknowledged milestone forever.
    required = context.get("required_successes", {}).get("defense", 0)
    confirmed = context.get("confirmed_successes", {}).get("defense", 0)
    if confirmed < required:
        add("wait")
        for enemy in target_enemies:
            if enemy.slot not in unreachable:
                add("approach", enemy.slot)
        return choices | healing_choices

    chains = {option["id"] for option in options if option["family"] == "chain"}
    mapped = frame.reaction.chain_actions
    if 0 in mapped:
        for action in (22, 28, 23):
            if action in chains and action not in mapped:
                add("equip_chain", 0, action, mapped.index(0) + 1)
                return choices | healing_choices
    banish_mask = frame.raw["status_masks"][29]
    dangerous_targets = [
        enemy for enemy in target_enemies
        if enemy.spell_action_ids and any(
            action in damage_spell_ids for action in enemy.spell_action_ids
        )
    ]
    for enemy in dangerous_targets:
        if (enemy.statuses | enemy.status_immunities) & banish_mask:
            continue
        for option in learned_spells:
            if (option["id"] == 105 and affordable_spell(option)
                    and not unsuccessful(option["id"], enemy.slot)):
                add("cast", enemy.slot, option["id"])
    if choices:
        return choices | healing_choices

    silence_mask = frame.raw["status_masks"][12]
    wants_silence = 35 in chains and any(
        not (enemy.statuses | enemy.status_immunities) & silence_mask
        for enemy in caster_targets
    )
    if wants_silence and 35 not in mapped:
        add("equip_chain", 0, 35, 3)
        return choices | healing_choices
    if (frame.enemies and all(enemy.mp == 0 for enemy in frame.enemies)
            and 24 in mapped and 28 in chains and 28 not in mapped):
        add("equip_chain", 0, 28, mapped.index(24) + 1)
        return choices | healing_choices

    for enemy in caster_targets:
        if not enemy.spell_action_ids:
            continue
        for option in learned_spells:
            details = semantics.get(str(option["id"]), {})
            if option["id"] not in (55, 75) or unsuccessful(option["id"], enemy.slot):
                continue
            mask = frame.raw["status_masks"][details["status_parameter"]]
            if not (enemy.statuses | enemy.status_immunities) & mask:
                add("cast", enemy.slot, option["id"])

    for enemy in target_enemies:
        for option in learned_spells:
            if (option["id"] not in damage_spell_ids or not affordable_spell(option)
                    or unsuccessful(option["id"], enemy.slot)):
                continue
            add("cast", enemy.slot, option["id"])

    for enemy in target_enemies:
        if (enemy.statuses | enemy.status_immunities) & banish_mask:
            continue
        for option in learned_spells:
            if (option["id"] == 105 and affordable_spell(option)
                    and not unsuccessful(option["id"], enemy.slot)):
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
        return choices | healing_choices

    choices.update(healing_choices)

    scene = context.get("source_scene_navigation_objective") or {}
    # A pending local scene that is not combat-gated must outrank chasing hostiles the player may never reach.
    scene_navigation = (bool(scene) and scene.get("activation_tick") is None
                        and not scene.get("combat_clearance_required"))
    all_unreachable = bool(frame.enemies) and all(enemy.slot in unreachable for enemy in frame.enemies)
    if (context.get("physical_navigation_objective") or scene_navigation) and (all_unreachable or all(
        context.get("confirmed_successes", {}).get(kind, 0) >= count
        for kind, count in context.get("required_successes", {}).items()
    )):
        add("navigate")
        return choices

    for enemy in target_enemies:
        if enemy.slot in unreachable:
            continue
        if native_target_menu_ready(frame):
            for target in frame.raw["target_previews"]:
                if target["actor"] == enemy.slot:
                    add("attack", enemy.slot, limb=target["limb"])
        else:
            add("attack", enemy.slot)
        add("approach", enemy.slot)
        add("retreat", enemy.slot)
    for slot in sorted(shielded - protected):
        if slot not in unreachable:
            # Moving changes the line of fire around the protected actor.
            add("approach", slot)
            add("retreat", slot)
    for option in learned_spells:
        details = semantics.get(str(option["id"]), {})
        role = details.get("role")
        if role in ("heal", "buff", "cure"):
            continue
        if role in ("damage", "banish") and not affordable_spell(option):
            continue
        if role == "banish":
            continue
        for enemy in frame.enemies:
            if enemy.slot in protected:
                continue
            if not unsuccessful(option["id"], enemy.slot):
                add("cast", enemy.slot, option["id"])
    if not choices:
        add("wait")
    return choices
