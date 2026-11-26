# Asset Pipeline: action hit values, behavior flags and volumes

[Asset pipeline index](../ASSET_PIPELINE.md)

### 4.7.1 Action hit values (#520)

This section explains what each `catalogs\battle\action-hits.csv` value makes the battle code do (§4.7). The
schema descriptions give the short form. The formulas come from the native battle code, which cites the decomp:
`Battle/ActionPreview.cpp`, `AttackPrerequisites.cpp`, `HitCalculations.cpp`, `HitModifiers.cpp`,
`StatFactors.cpp`, `DamageCalculations.cpp`, `DamagePolicies.cpp`, `BasicActionEffects.cpp`,
`DamageRecoveryEffects.cpp`, `StatusQueue.cpp`, `Items/Recovery.cpp` and `Field/Traps.cpp`. Integer division
rounds toward zero. "User" is the actor performing the action. "Target" is the actor or limb a hit is resolved
against.

**Order.** Each hit (first, then second) is resolved in turn. The `prerequisite` chooses one of these paths:

- `never-0`: the hit is skipped.
- `never-1`, `never-2`, `never-3` (direct): the effect is applied once to the first target. There is no check and
  no roll, so `hit_function` is unread.
- `always-2` (self): the hit is checked, rolled and applied against the user.
- Every other value, `always-0` and `always-1` included, is handled per target. A failed check skips the target;
  on the first hit, it also stores a miss for that target. Otherwise the hit is rolled. The effect applies when the
  roll lands, or always in a preview (the menu's damage and chance display).

### Prerequisites

| Value | Accepts |
|---|---|
| `always-0`, `always-1`, `unused-0` to `-2` | Every target. Exception: `always-0` on the second hit skips a target whose first hit was cancelled. |
| `undead`, `not-undead` | Only targets of that enemy class. |
| `can-apply-X` | Targets that can receive status X: the status rule exists, the target is not immune to it, and none of the rule's blocking statuses is active. |
| `can-apply-equipment-up`/`-down` | As `can-apply-X`, and the target also has armor, a shield or a blade. |
| `can-apply-*-attack-up` | As `can-apply-X`, and the target also has a blade. |
| `can-apply-*-defense-up` | As `can-apply-X`, and the target also has armor or a shield. |
| `can-apply-analyze` | Checks the analyze rule. Used by Analyze. |
| `can-apply-instant-defeat-undead` | Checks the instant-defeat rule; undead targets only. Used by Exorcism. |
| `can-apply-instant-defeat-living` | Checks the instant-defeat rule; living targets only. Used by Banish and Drain Mind. |

### Chance to hit

The roll is `rand(100) < chance`; a chance of 255 is a miss. The chance is worked out as follows:

1. Start from `hit_function`, below. A spell starts at 0 against a `spell-immune` target, and a target whose actor
   id is 128 always starts at 100.
2. A failed prerequisite makes it a miss.
3. Otherwise, double it against a `left-arm-impaired` target, then cap it at 100.
4. The player's rolls get a further +10, which the menu does not show.

| `hit_function` | Chance |
|---|---|
| `follows-previous-hit` | 100 if the action's most recent roll landed (for one target, its first hit); otherwise a miss. |
| `gem-modifier` | 100. +20 if the user's drawn weapon has the accuracy gem for physical or spell actions. −20 for each matching protective gem on the target's drawn shield (physical, spell, or one for the hit's effect). |
| `agility-difference` | 100 + A·(100 − user risk)/100 − (B + the hit limb's `agility_defense_bonus`)·(100 − target risk)/100, where A and B are the user's and target's agility: base + accessory + armor, plus weapon and shield while drawn. |
| `intelligence-difference` | As above with intelligence and without the 100; the target's side is also multiplied by its HP / max HP. |
| `agility-difference-2` | As `agility-difference`, with 130 − the action's cost in place of 100. |
| `risk` | (user risk + 150) × 100 / 256. |
| `chain-evasion` | (255 − the target limb's `chain_evasion`) × 100 / 255. |
| `none` | Refused if rolled; only a direct hit uses it. |

The three difference functions then change by a random −5 to +5 when executed and add the same gem terms as
`gem-modifier`. The result is floored at 0; a result of exactly 255 becomes 254.

### Amounts

| `stat_calculator` | Amount |
|---|---|
| `physical-damage-with-status-modifiers`, `-without-` | Damage formula below, using strength. `with` halves the amount while the user is `right-arm-impaired`. |
| `magic-damage` | Damage formula, using intelligence. |
| `hp-healed` | Damage formula, using intelligence, varying by −5 to +5 when executed. Refused for affinity `weapon`, and for `physical` while the user's weapon is drawn. |
| `item-recovery` | 5 × `stat_factor`, varying by −5 to +5 when executed, floored at 0. |
| `trap-panel-damage`, `heal-panel-hp-healed` | Target max HP / 10 × `stat_factor` + 10 × (trap level + 1), less the target's strength, armor and raised-shield defense, floored at 0, ±5 when executed. The trap level is a property of the room (from its MPD; no catalog authors it yet). Refused for affinity `weapon`. Trap damage never takes the last HP. |
| `recoverable-hp`, `recoverable-mp` | The target's max − current HP or MP. |
| `risk` | The target's risk. |
| `weapon-pp` | The user's weapon PP. |
| `shield-pp` | The user's shield PP. |
| `hp-lost-factor`, `mp-lost-factor` | The user's (max − current) HP or MP × `stat_factor` / 10. |
| `current-hp-factor` | The target's HP × `stat_factor` / 100. |
| `previous-hit-factor-0` to `-2` (identical) | The HP + MP damage the chained attack dealt its first target × `stat_factor` / 10. |
| `magic-damage-received-factor`, `magic-damage-received-of-type-factor` (identical) | The answered hit's HP + MP damage × `stat_factor` / 10, if it did damage and was a spell; otherwise 0. |
| `physical-damage-received-factor-0`, `-1` (identical) | The same, for a hit that was not a spell. |
| `damage-received-factor` | The same, for any hit that did damage. |
| `spell-cost-factor` | The answered spell's MP cost × `stat_factor` / 10; 0 if it was not a spell. |
| `last-value` | The amount the action's previous calculation produced. |
| `last-value-factor` | That amount × `stat_factor` / 10. |
| `none` | No amount (status effects). |

`weapon-pp`, `hp-lost-factor`, `mp-lost-factor`, `current-hp-factor` and the `previous-hit-factor` values add 1 for
each earlier ability in a chain. "Answered" means the attack that a defense ability is reacting to.

**Damage formula.**

1. Attack stat = the user's stat + accessory, plus the weapon's while drawn.
2. Rating = (weapon terms + the accessory's type, affinity and, for physical hits, enemy-class values) / 4 + 100.
   The weapon terms count only while it is drawn. They are its value for the hit's affinity, plus, for physical
   hits, its damage-type value and its value against the target's enemy class. The weapon's DP wear % is
   subtracted, and its PP share is added.
3. Attack = stat × rating / 100 × (100 + 10 × `stat_factor`) / 100.
4. Defense = [(target stat + accessory − 20) × limb rating + armor stat × armor rating + shield stat × shield
   rating] / (target risk / 2 + 100). Each rating is 100 plus that piece's type, affinity and, for physical hits,
   enemy-class values; the limb rating also adds the target's accessory values. Armor and shield lose a quarter of
   their DP wear %, and the shield gains a quarter of its PP share. The shield counts only while drawn.
5. Amount = max(0, (attack − defense) × gem%), where gem% is 100, +5 for the user's damage gem and −5 for the
   target's protective gem.
6. When executed:
   - Physical and magic damage add 1 + rand(5).
   - The basic attacks (actions 0-5) are critical with a chance of user risk / 10 + 10 percent, which doubles the
     amount.

### Effects

| `effect` | Does |
|---|---|
| `apply-X`, `cure-X` | Adds or removes status X. |
| `cure-paralysis-poison-and-numbness` | Removes paralysis, poison and numbness. |
| `dispel` | Removes every stat, element, quicken, silence, regeneration and magic-ward status. |
| `damage` | Takes the amount from HP, spreading it over the limbs. When executed, it also wears equipment DP and builds PP. |
| `drain-mp` | Takes the amount from MP, at most what the target has. |
| `drain-weapon-pp`, `drain-shield-pp` | Empty the target's weapon or shield PP. The shield is drained only when the answered hit did damage. |
| `recover-hp`, `-mp`, `-weapon-pp`, `-weapon-dp` | Add the amount. |
| `reduce-risk` | Lowers risk by the amount. |
| `recover-hp-on-X` | Heals the amount only when the answered hit was a spell, not a spell, of affinity X, or did damage. |
| `random-X-increase`, `-decrease` | Changes the stat by 1-4. |
| `instant-defeat` | Clears the statuses the instant-defeat rule lists and takes all remaining HP. |
| `armor-wear-from-damage` | Wears the hit limb's armor DP by twice the HP damage already queued. |
| `mark-success-0` to `-2` (identical) | Mark the user's action as succeeded (Eureka, Mark Trap, Fixate, Unlock). |
| `set-script-flag-32`, `-64` | Set that script flag to 1 when executed (32: Trap Clear; 64: unused). |
| `unused-0` to `-3` | Refused. |

`affinity` `weapon` takes the drawn blade's highest affinity, or `physical` when the weapon is sheathed, absent or
has no affinity above zero. `damage_type` `none` makes a physical hit use the weapon's damage type, or `blunt`
without a blade.

**Earlier names.** Four values were named after their upstream handlers until the names said what they do. The
schema lists the earlier spellings under `legacy_values`. Readers, mod patches included, still accept them, and
`VS.Extract authoring <dir>` rewrites them to the current names. No converter version changes, because the
values' codes are the same.

| Earlier name | Current name | Decomp handler |
|---|---|---|
| `can-apply-banish` | `can-apply-analyze` | `hitPrerequisiteCanApplyBanish` |
| `can-apply-exorcise` | `can-apply-instant-defeat-undead` | `hitPrerequisiteCanApplyExorcise` |
| `can-apply-drain` | `can-apply-instant-defeat-living` | `hitPrerequisiteCanApplyDrain` |
| `equipped-shield-dp` | `shield-pp` | `getEquippedShieldDp` |

### 4.7.2 Action behavior flags (#520)

This section defines each `behavior` flag of `catalogs\battle\actions.csv` (§4.7). Only the enemy AI reads the
first twelve (`Battle/Ai/PlanningPrimitives.cpp`, `DecisionPrimitives.cpp`, `PlanningWorkspace.cpp` and
`Battle/Targeting.cpp`): they change how an enemy chooses the action, not what the action does, and matter only
when an enemy has the action in one of its action slots. No enemy has a `trap` action or an environment-category
action in the original data, so those actions' AI flags have no effect. The battle code reads the last three for
every user.

The AI rates each pairing of one of its actions with a target. A pairing has a **score**, from the condition of
every actor after a simulated hit, and a **weight** from 0 to 128: the simulated hit chance scaled to 128, or 128
when nothing is simulated. The weight is the chance in 128 that the pairing enters the random pick among the
action's targets, and it breaks ties between equal scores. A weight of 0 rules the pairing out. An enemy's
**allies** and **opponents** come from its AI relations; an actor that is neither is never a target.

| Flag | What it does |
|---|---|
| `special-mode` | Special-action mode is a setting an enemy is spawned with (bit 12 of its spawn flags; no catalog column sets it yet). An enemy in that mode skips the hit simulation for the action. The pairing gets weight 40, and its score rises by one step, so it wins ties. Other enemies ignore the flag. |
| `rarely-chosen` | The AI discards the pairing when a roll from 0 to 127 is below 115, so it considers the action in about one evaluation in ten. |
| `fallback-eligible` | The AI remembers the first pairing for each action slot, up to 16, as a fallback. When a plan ends with no candidate and the enemy's spawn flags allow fallbacks (bit 13), the plan uses the remembered pairings at weight 128. |
| `shaped-area` | The AI estimates the area's reach in steps of 32 world units from `area_shape` and the tilt θ = `area_angle` × 11.25 degrees: `area_range_z` × \|cos θ\| for `ellipsoid`, `centered-cylinder` and `cone`, `area_range_y` × \|cos θ\| for `cylinder`, and `area_range_y` × \|sin θ\| for the other shapes. It keeps the reach only when it is shorter than the action's range, and does not use the action on a target nearer to the enemy than that reach. |
| `radial-area` | As `shaped-area`, but the reach is `area_range_y` steps of 32 world units, whatever the shape. |
| `targets-unconditionally` | A `beneficial` action may go to any ally, and the next three flags are ignored. Without this flag, the ally must match one of them. |
| `targets-status-restricted` | ... an ally with half movement, strength, intelligence or agility down, paralysis, poison or numbness, or silence when it has spells. |
| `targets-low-mp` | ... an ally that has spells and has MP below half its maximum or below the cost of its costliest spell. |
| `targets-low-hp` | ... an ally with HP below half its maximum. |
| `ignores-sight` | The AI may use the action on a target out of its sight. Without the flag, the target must be in sight. |
| `beneficial` | The AI uses the action only on allies, as the `targets-*` flags allow. |
| `harmful` | The AI uses the action only on opponents. Unless the target is a gimmick (an interactive room object such as a crate), the action also waits for the enemy's attack timer to run out, and carrying it out restarts the timer: from the room's AI action delay, or 50 frames while the enemy is allied with Ashley. |
| `menu-action` | The action is chosen from a menu: cancelling targeting returns to it, and the action is presented with its `effect_package` rather than a melee animation. |
| `hits-first-obstruction` | When the action is carried out, a line is traced from the user to the target. Terrain in the way stops the hit there, and it affects no one. Another actor in the way takes the hit instead. |
| `starts-unlocked` | The action is available when a new game begins. |

With neither `beneficial` nor `harmful`, the AI may use the action on allies and opponents alike. The action with
`legacy_id` 96 (the second Heal) swaps the two flags against an undead target, so the AI uses it to harm undead.

### 4.7.3 Action volumes (#520)

The `range_*` columns of `catalogs\battle\actions.csv` describe an action's **range volume** and the `area_*` columns
its **area volume** (`Battle/Targeting.cpp`, `Battle/SphereContainment.cpp`). A whole actor is a candidate when its
centre point (the bone its model tags 255) lies inside the volume; for a limb category, each body part is tested at
its own position. Extents are in steps of 32 world units, and x and z are measured from the centre line, so an x of
4 reaches 128 units to each side. Before it is tilted, a volume's height axis points straight up, its x (across)
axis runs from side to side and its z axis from front to back.

| Shape | Volume |
|---|---|
| `ellipsoid` | Centred on the origin, with half-axes x, y and z. |
| `centered-cylinder` | An elliptical cylinder centred on the origin. Its base has half-axes x and z, and it reaches y above and y below the origin. The doubled height is kept in a byte, so a y above 127 wraps. |
| `cylinder` | The same cylinder standing on the origin, y tall. |
| `cone` | A cone with its base (half-axes x and z) on the origin and its tip y above it. |
| `reversed-cone` | A cone with its tip on the origin, widening to its base y above it. |
| `rectangle` | A box standing on the origin, y tall, reaching x to each side and z ahead and behind. |
| `diamond` | As `rectangle`, but its base is a diamond with corners x to each side and z ahead and behind. |

`none` contains nothing. An angle of n tilts the height axis n × 11.25 degrees (n/32 of a turn) about the x axis,
so 16 turns the volume upside down. Where each volume goes depends on the action's category:

- **Chosen single target** (`direct-*`, `treasure-chest`): the range volume stands on the bone the user's model tags
  250 and turns with the user. A `range_angle` of n leans it from upright toward the user's back, so 8 lies flat
  behind and 24 flat ahead, as `analyze` and `unlock` do. The actors or body parts inside it are the choices. A
  `range_x` of 255 takes the weapon's extents and angle instead.
- **Ground area** (`ground-area*`): the action builds the same range volume and centres the area volume on the
  chosen point. The area volume does not turn: x runs along the map's x axis and z along its z axis, and an
  `area_angle` of 8 lays its height axis toward the map's +z. The actors or body parts inside it are the targets.
- **Source area** (`source-area*`): there is no separate area volume. The range volume starts at the user's root
  bone, turns toward the target and tilts so that its height axis points at it, so a `cylinder` is a beam `range_y`
  steps long. `range_angle` and the `area_*` columns are not used for targeting. Only enemies have these actions;
  when Ashley uses one, the volume stays level and points toward the map's +z.

Other categories do not use the volumes to find targets. Neither do chain and defense abilities, whose volumes are
`none`: a chain ability (the `direct-limb-or-object` abilities) hits the targets of the attack it follows, and a
defense ability (the `environment` abilities) answers the attack that hit the user, with the attacker as its only
target. The AI's `shaped-area` reach
(§4.7.2) reads `area_shape` and `area_angle` on its own.

### 4.7.4 Action presentation and limb damage

`camera_zoom` selects the combat camera transition: `follow` tracks the action midpoint, `zoom-in` moves toward
the near distance while increasing projection, `zoom-out` moves toward the far distance, and `hold` keeps the
current framing. A `zoom-in` action at a target distance of 449 or more falls back to `follow`.

`recovery_delay` is the action's base delay before its actor can act again. The actor's recovery bonus is added;
255 uses the equipped weapon's delay. Actions 22–53 add no delay. The timer advances by
`(base delay + recovery bonus) × 256 / 35` battle updates.

The visible `effect_package` is presentation, not the hit's `effect`. It is dispatched for the action before its
animation and effect track start; required actor assets and effect resources gate that start. The first and second
hit rows are applied separately during hit resolution, following §4.7.1's prerequisite and roll rules. There is no
fixed frame number in the catalog for when a hit lands.

The `category` also selects how damage is assigned to limbs:

| Distribution | Categories | Assignment |
|---|---|---|
| Even | `environment`, `direct-actor-including-user`, `ground-area-including-user`, `direct-actor`, `ground-area` | Equal integer shares across limbs with nonzero maximum HP; any remainder is not assigned. |
| Splash | `direct-limb-including-user`, `direct-limb`, `direct-limb-or-object` | Each limb receives its selected limb's splash percentage of the amount. |
| Selected limb | `ground-area-limb-including-user`, `ground-area-limb`, `source-area-limb-including-user`, `source-area-limb` | The selected limb receives the full amount. |
| None | `cloudstone`, `treasure-chest`, `trap`, `unused` | No limb damage is assigned. |

This is the damage-distribution policy after a hit amount is calculated; it does not change target selection or
the `effect` field.

Sources: `Battle/ActionPresentation.cpp`, `ActionCameraSelection.cpp`, `ActionCosts.cpp`, `ActionSequence.cpp`,
`ActionEffectDispatch.cpp` and `DamagePolicies.cpp`.
