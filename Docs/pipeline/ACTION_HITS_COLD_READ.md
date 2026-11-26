# Asset Pipeline: seeded battle-action cold reads (#520)

[Asset pipeline index](../ASSET_PIPELINE.md)

### 4.7.5 Seeded action-hit cold read

Python `random.Random("520:action-hits:2").sample` over `action-hits.csv` selected these five records. The
`action` field references the stable ID in `actions.csv`; the parent action's category defines targeting and limb
policy. Its legacy number is provenance, not authoring identity. Values are ordered as effect, prerequisite,
hit function, stat calculator, stat factor, damage type and affinity.

| Hit row | Parent action (category; legacy ID) | Values | Edit intent |
|---|---|---|---|
| `action.gravity` / `first` | Gravity (`source-area-limb`; 137) | `damage`; `always-1`; `agility-difference-2`; `magic-damage`; 0; `none`; `earth` | Tune this earth-affinity magic hit here; change target range separately on the action. |
| `action.raven-eye` / `second` | Raven Eye (`direct-limb`; 160) | `apply-poison`; `can-apply-poison`; `agility-difference`; `none`; 0; `none`; `weapon` | Change poison eligibility or application here; its first-hit damage is a separate row. |
| `action.hp-up` / `first` | HP-up (`direct-actor-including-user`; 251) | `random-max-hp-increase`; `always-1`; `gem-modifier`; `none`; 0; `none`; `physical` | This raises maximum HP; do not author it as HP recovery. |
| `action.blasphemous-howl` / `first` | Blasphemous Howl (`direct-limb`; 144) | `damage`; `always-1`; `agility-difference`; `physical-damage-without-status-modifiers`; 2; `piercing`; `dark` | Tune dark, piercing damage here; curse application is its separate second hit. |
| `action.int-up` / `first` | INT-up (`direct-actor-including-user`; 249) | `random-intelligence-increase`; `always-1`; `gem-modifier`; `none`; 0; `none`; `physical` | This raises intelligence; it is not a damage calculation. |

Hit semantics are documented in [ACTION_HITS](ACTION_HITS.md#471-action-hit-values-520); action ranges and
presentation are separate fields on the referenced action.

### 4.7.6 Seeded action-definition cold read

Python `random.Random("520:actions:2").sample` over `actions.csv` selected these five records. `category` references
the stable category ID in `action-categories.csv`; `legacy_id` remains import provenance. Values below include
cost, recovery delay, camera, effect package, range `(shape; x,y,z; angle)`, area (same order), and behavior flags.

| Action | Values | Edit intent |
|---|---|---|
| `action.acid-flow` (Acid Flow; legacy 138; spell) | `source-area-limb`; cost 35; delay 2; `zoom-out`; `asset.effect.e085.p`; range `cylinder; 1,24,1; 0`; area `none; 0,0,0; 0`; `ignores-sight|harmful|menu-action|starts-unlocked` | Keep its source-area limb category separate from its cylinder range and hit effect. |
| `action.acrid-ooze` (Acrid Ooze; legacy 180; ability) | `direct-limb`; cost 2; delay 1; `follow`; `asset.effect.e127.p`; range `ellipsoid; 9,10,9; 0`; area `none; 0,0,0; 0`; `special-mode|fallback-eligible|harmful|menu-action|hits-first-obstruction|starts-unlocked` | Adjust obstruction behavior via the named flag, not by changing the target category or effect package. |
| `action.enlighten` (Enlighten; legacy 61; spell) | `direct-actor-including-user`; cost 12; delay 0; `zoom-in`; `asset.effect.e008.p`; range `ellipsoid; 15,14,15; 0`; area `none; 0,0,0; 0`; `targets-unconditionally|ignores-sight|beneficial|menu-action` | Preserve self-target inclusion in the category; the effect package is presentation. |
| `action.herakles` (Herakles; legacy 60; spell) | `direct-actor-including-user`; cost 12; delay 0; `zoom-in`; `asset.effect.e007.p`; range `ellipsoid; 15,14,15; 0`; area `none; 0,0,0; 0`; `targets-unconditionally|ignores-sight|beneficial|menu-action` | Keep this distinct action and package even though its targeting and behavior match Enlighten. |
| `action.explosion-4` (Explosion {level}4; legacy 109; spell) | `ground-area-limb-including-user`; cost 60; delay 0; `follow`; `asset.effect.e056.p`; range `ellipsoid; 12,12,12; 0`; area `ellipsoid; 8,8,8; 0`; `shaped-area|ignores-sight|harmful|menu-action` | Edit the area volume separately from the outer range and category. |

### 4.7.7 Seeded action-category cold read

Python `random.Random("520:action-categories:2").sample` over `action-categories.csv` selected these five records.
Each category is a stable reference target for `actions.csv`; `display_name` is informational.

| Category (`display_name`) | Values (`selection`; `extent`; `includes_user`; `aim`; `target_kinds`) | Meaning and edit intent |
|---|---|---|
| `action-category.environment` (`environment`) | `automatic`; `single`; `no`; `actor`; `none` | No actor kinds are listed; keep this distinct from selectable actor-target categories. |
| `action-category.source-area-limb-including-user` (`source area limb including user`) | `automatic`; `area`; `yes`; `limb`; `leader|neutral|hostile` | Automatically selected source-area limb category includes the user; preserve that inclusion when editing recipients. |
| `action-category.ground-area-limb` (`ground area limb`) | `chosen`; `area`; `no`; `limb`; `leader|neutral|hostile` | Chosen ground-area limb category excludes the user; do not merge it with the source-area variant. |
| `action-category.direct-actor-including-user` (`direct actor including user`) | `chosen`; `single`; `yes`; `actor`; `leader|neutral|hostile` | Chosen single actor may include the user; this is also the category used by the sampled self-targeting spells. |
| `action-category.unused` (`unused`) | `automatic`; `single`; `no`; `actor`; `none` | Explicit unused category; retain as such rather than assigning it new gameplay meaning. |

Category fields and their consumers are described in [GAMEPLAY_CATALOGS](GAMEPLAY_CATALOGS.md#46-native-action-categories-499);
action behavior and volume semantics are in [ACTION_HITS](ACTION_HITS.md#472-action-behavior-flags-520).
