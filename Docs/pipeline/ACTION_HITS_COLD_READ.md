# Asset Pipeline: seeded action-hit cold read (#520)

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
