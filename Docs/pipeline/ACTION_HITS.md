# Asset Pipeline: action hit values

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
- Every other value is handled per target. A failed check skips the target; on the first hit, it also stores a
  miss for that target. Otherwise the hit is rolled. The effect applies when the roll lands, or always in a
  preview (the menu's damage and chance display).

### Prerequisites

| Value | Accepts |
|---|---|
| `always-0`, `always-1`, `unused-0` to `-2` | Every target. Exception: `always-0` on the second hit skips a target whose first hit was cancelled. |
| `undead`, `not-undead` | Only targets of that enemy class. |
| `can-apply-X` | Targets that can receive status X: the status rule exists, the target is not immune to it, and none of the rule's blocking statuses is active. |
| `can-apply-equipment-up`/`-down` | As `can-apply-X`, and the target also has armor, a shield or a blade. |
| `can-apply-*-attack-up` | As `can-apply-X`, and the target also has a blade. |
| `can-apply-*-defense-up` | As `can-apply-X`, and the target also has armor or a shield. |
| `can-apply-banish` | Checks the analyze rule. Used by Analyze. |
| `can-apply-exorcise` | Checks the instant-defeat rule; undead targets only. |
| `can-apply-drain` | Checks the instant-defeat rule; living targets only. Used by Banish and Drain Mind. |

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
| `trap-panel-damage`, `heal-panel-hp-healed` | Target max HP / 10 × `stat_factor` + 10 × (trap level + 1), less the target's strength, armor and raised-shield defense, floored at 0, ±5 when executed. Refused for affinity `weapon`. Trap damage never takes the last HP. |
| `recoverable-hp`, `recoverable-mp` | The target's max − current HP or MP. |
| `risk` | The target's risk. |
| `weapon-pp` | The user's weapon PP. |
| `equipped-shield-dp` | Despite the name, the user's shield **PP**. |
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

**Names kept for compatibility:**

- `equipped-shield-dp` reads PP.
- `can-apply-banish` is Analyze's check, and Banish uses `can-apply-drain`, as Drain Mind does.

Renaming them needs a catalog version bump and aliases.
